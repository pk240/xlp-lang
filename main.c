# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <ctype.h>
# include "map-master/src/map.h"
# include "sds-master/sds.h"

/* macros to create new variant */
# define NERROR(x) (struct variant){.type=ERROR,.data.f=x}
# define NF(x) (struct variant){.type=FLOAT,.data.f=x}
# define NI(x) (struct variant){.type=INTEGER,.data.i=x}
# define NS(x) (struct variant){.type=STRING,.data.s=x}

/* defaults */
# define DEF_MAIN_STACK "main_stack"
# define DEF_MAIN_BH "main_bh" /* branch history */
# define DEF_MAIN_EXE "main_exe" /* command stack */
# define DEF_GLOBAL_JH "global_jh" /* jump history (switch/yield), for coroutines */

struct variant {
	enum { ERROR,FLOAT,INTEGER,STRING } type;
	union {
		float f;
		int i;
		sds s;
	} data;
};

struct stack {
	int size;
	int top;
	int pos;
	struct variant * stack;
	struct variant related; /* related stack, for "use" command */
	struct variant bh; /* jump history of this stack, for "use" command */
};

map_t(struct stack) m;

char escape(char c) {
	switch (c) {
		case 'n':c='\n'; break;
		case 't':c='\t'; break;
	}
	return c;
}

struct stack * get_stack(struct variant name) {
	static int init_size=2;
	struct stack * named_stack=map_get(& m,name.data.s);
	if (! named_stack) {
		/* create default stack */
		struct stack new=(struct stack){
			.size=init_size,
			.top=0,
			.pos=0,
			.stack=calloc(init_size,sizeof(struct variant)),
			.related=NS(DEF_MAIN_STACK),
			.bh=NS(DEF_MAIN_BH),
		};
		map_set(& m,name.data.s,new);
		named_stack=map_get(& m,name.data.s);
	}
	return named_stack;
}

int * get_top(struct variant name) {
	struct stack * s=get_stack(name);
	return & s->top;
}

struct variant peek_top(struct variant name) {
	struct stack * s=get_stack(name);
	if (s->top==0) return NERROR(0);
	return s->stack[s->top-1];
}

struct variant pop(struct variant name) {
	struct stack * s=get_stack(name);
	if (s->top==0) return NERROR(0);
	s->top--;
	return s->stack[s->top];
}

void undo(struct variant name) {
	struct stack * s=get_stack(name);
	s->top++;
}

void push(struct variant name, struct variant val) {
	struct stack * s=get_stack(name);
	//struct variant * v;
	//v=& s->stack[s->top];
	//if (v->type==STRING) sdsfree(v->data.s);
	s->stack[s->top++]=val;
	if (s->top>=s->size) {
		int oldsize=s->size;
		s->size*=10;
		s->stack=realloc(s->stack,s->size*sizeof(struct variant));
		memset(s->stack+oldsize,0,(s->size-oldsize)*sizeof(struct variant));
	}
}

int find_label(struct variant name, struct variant val) {
	struct stack * s=get_stack(name);
	for (int i=1;i<s->top;i++)
		/* +1 because labels start with " */
		if (!strncmp(s->stack[i].data.s,"label",5)&&!strcmp(s->stack[i-1].data.s+1,val.data.s)) return i;
	return 0;
}

/* tokenize file and load tokens onto executable stack */
void load_file(struct variant cmd, char * fname) {
	FILE * f=fopen(fname,"r");
	char buf[1024];
	char c,prevc=' ';
	int i=0,in_comment=0,in_string=0,escaped=0,in_at=0;
	while ((c=fgetc(f))!=EOF) {
		if (i==1024) i=1023; /* avoid overflow, TODO: maybe realloc buf */
		if (c=='\n') { in_comment=0; c=' '; }
		if (in_comment) continue;
		if (escaped) { escaped=0; buf[i++]=escape(c); continue; }
		if (c=='\\') { escaped=1; continue; }
		if (in_string&&c!='"') { buf[i++]=c; continue; }
		if (c=='\t') { c=' '; }
		if (c=='#') { in_comment=1; continue; }
		if (c=='@'&&i==0) { in_at=1; continue; }
		if (c=='"') {
			if (in_string) c=' ';
			in_string=!in_string;
		}
		if (c==' ') {
			if (prevc==' ') continue;
			if (i==0) continue;
			buf[i]=0;
			if (in_at) { cmd=NS(sdsnew(buf)); in_at=0; i=0; continue; }
			push(cmd,NS(sdsnew(buf)));
			i=0;
		} else {
			buf[i++]=c;
		}
		prevc=c;
	}
}

# define LEN(s) (sizeof(s)/sizeof(s[0]))
# define C(s) } else if (!strncmp(cur,s,LEN(s))) {

int exec(struct variant global) {
	static int debug;
	struct variant cmd=peek_top(global);
	struct stack * exe=get_stack(cmd);
	if (exe->pos==exe->top) {
		exe->pos=0;
		/* TODO: should this also reset bh and related? */
		return 0;
	}
	struct variant cs=exe->related;
	struct variant bh=exe->bh;
	sds cur=exe->stack[exe->pos].data.s;
	if (debug) printf("%2d/%2d) %s\n",exe->pos,exe->top,cur);
	if (cur[0]=='"') {
		push(cs,NS(sdsnew(cur+1)));
	} else if (cur[0]=='.'||isdigit(cur[0])) {
		if (strchr(cur,'.')) push(cs,NF(atof(cur)));
		else push(cs,NI(atoi(cur)));
	/* the following is a list of commands in macros */
	C("debug")	debug=!debug;
	/* stack interaction */
	C("save")	struct variant to_name=pop(cs);
			struct variant to_val=pop(cs);
			struct stack * s=get_stack(to_name);
			s->stack[0]=to_val;
	C("load")	struct variant from_name=pop(cs);
			struct stack * s=get_stack(from_name);
			push(cs,s->stack[0]);
	C("push")	struct variant to_stack=pop(cs);
			struct variant to_val=pop(cs);
			push(to_stack,to_val);
	C("pop")	struct variant from_stack=pop(cs);
			struct variant from_val=pop(from_stack);
			push(cs,from_val);
	C("poke")	int pos=pop(cs).data.i;
			struct variant to_name=pop(cs);
			struct variant to_val=pop(cs);
			struct stack * s=get_stack(to_name);
			int top=s->top;
			if (pos<0) pos=top+pos;
			s->stack[pos]=to_val;
	C("peek")	struct variant from_name=pop(cs);
			int pos=pop(cs).data.i;
			struct stack * s=get_stack(from_name);
			int top=s->top;
			if (pos<0) pos=top+pos;
			push(cs,s->stack[pos]);
	/* special */
	C("undo")	undo(cs);
	/* branching */
	C("label")	pop(cs);
	C("goto")	struct variant to_name=pop(cs);
			if (strlen(to_name.data.s)) {
				exe->pos=find_label(cmd,to_name);
			}
	C("call")	struct variant to_name=pop(cs);
			if (strlen(to_name.data.s)) {
				push(bh,NI(exe->pos));
				exe->pos=find_label(cmd,to_name);
			}
	C("cmp")	struct variant branch_false=pop(cs);
			struct variant branch_true=pop(cs);
			struct variant branch_condition=pop(cs);
			if (branch_condition.data.i) push(cs,branch_true); else push(cs,branch_false);
	C("return")	exe->pos=pop(bh).data.i;
	C("exit")	exit(pop(cs).data.i);
	C("size")	int top=* get_top(cs);
			push(cs,NI(top));
	/* stack manipulation */
	C("printstack")	struct variant * v;
			struct stack * s=get_stack(cs);
			int top=s->top-1;
			printf("----[STACK]----\n");
			for (int j=top;j>=0;j--) {
				v=s->stack+j;
				if (v->type==ERROR) printf("%2d) [ERROR]\n",j);
				else if (v->type==FLOAT) printf("%2d) [%5f]\n",j,v->data.f);
				else if (v->type==INTEGER) printf("%2d) [%5d]\n",j,v->data.i);
				else if (v->type==STRING) printf("%2d) [%5s]\n",j,v->data.s);
			}
			printf("---------------\n");
	C("print")	struct variant v=pop(cs);
			if (v.type==FLOAT) printf("%f\n",v.data.f);
			else if (v.type==INTEGER) printf("%d\n",v.data.i);
			else if (v.type==STRING) printf("%s",v.data.s);
	C("reverse")	struct variant prev_top=pop(cs);
			struct variant prev_sec=pop(cs);
			push(cs,prev_top);
			push(cs,prev_sec);
	C("duplicate")	struct variant to_dup=pop(cs);
			push(cs,to_dup);
			if (to_dup.type==STRING) to_dup=NS(sdsdup(to_dup.data.s));
			push(cs,to_dup);
	C("clear")	int * top=get_top(cs);
			* top=0;
	/* math & logic */
	C("or")		struct variant b=pop(cs),a=pop(cs);
			push(cs,NI(a.data.i||b.data.i));
	C("and")	struct variant b=pop(cs),a=pop(cs);
			push(cs,NI(a.data.i&&b.data.i));
	C("is_error")	struct stack * s=get_stack(cs);
			push(cs,NI(s->stack[s->top-1].type==ERROR));
	C("=")		struct variant b=pop(cs),a=pop(cs);
			push(cs,NI(a.data.i==b.data.i));
	C("<")		struct variant b=pop(cs),a=pop(cs);
			push(cs,NI(a.data.i<b.data.i));
	C(">")		struct variant b=pop(cs),a=pop(cs);
			push(cs,NI(a.data.i>b.data.i));
	C("+")		struct variant b=pop(cs),a=pop(cs);
			if (b.type==STRING&&a.type==STRING)
				/*cannot use sdscat because of imminent forced free*/
				push(cs,NS(sdscatprintf(sdsempty(), "%s%s",a.data.s,b.data.s)));
			else
				push(cs,NI(a.data.i+b.data.i));
	C("*")		struct variant b=pop(cs),a=pop(cs);
			push(cs,NI(a.data.i*b.data.i));
	C("/")		struct variant b=pop(cs),a=pop(cs);
			push(cs,NI(a.data.i/b.data.i));
	C("%")		struct variant b=pop(cs),a=pop(cs);
			push(cs,NI(a.data.i%b.data.i));
	/* co-routines */
	C("getstack")	push(cs,cs);
	C("getexe")	push(cs,cmd);
	C("use")	struct variant newrel=pop(cs),new_bh=pop(cs);
			exe->related=newrel;
			exe->bh=new_bh;
	C("yield")	struct variant val=pop(cs);
			pop(global);
			push(get_stack(peek_top(global))->related,val);
	C("switch")	struct variant name=pop(cs);
			if (strlen(name.data.s))
				push(global,name);
	C("destroy")	//destroy a stack and free strings
	} else {
		printf("unrecognized: %s\n",cur);
	}
	exe->pos++;
	return 1;
}

int main(int argc, char ** argv) {
	if (argc<2) { printf("Needs files.\n"); exit(1); }

	map_init(& m);
	struct variant cmd=NS(DEF_MAIN_EXE);
	for (int i=1;i<argc;i++) load_file(cmd,argv[i]);

	struct variant global=NS(DEF_GLOBAL_JH);
	push(global,cmd);

	do {
		while (exec(global));
		pop(global);
	} while (peek_top(global).type!=ERROR);
}
