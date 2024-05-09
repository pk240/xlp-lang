# xlp-lang
an RPN programming language
# Usage
./ikslop <file.xlp>
# XLP format
Each word in the xlp file is put on an execution stack.
Default execution stack is "main_exe".
When the file is finished reading, execution will begin from the first placed item on the execution stack to last.
# Syntax
Values can be integer, float or string (there is also type error, when an empty stack is popped from). They are placed on the calculation stack.
Commands pop from the calculation stack and push the result back on the calculation stack.
Comments start with #. Strings are enclosed by "STRING". Some escape sequences are supported. There is a shortcut to put commands from an XLP file on a particular execution stack by using @stack-name.
# Commands
arguments are denoted as (n) where n of 1 is top of stack after command. Arguments are popped: 1 2 + will not leave either 1 or 2 on the stack.

 - debug	-	toggles printing current command.
 - save	-	changes top of stack with name (1) to (2)
 - load		-	read top of stack with name (1) and pushes it to main calculation stack
 - push	-	pushes value (2) to stack with name (1)
 - pop		-	pops from stack (1) to main stack
 - poke	-	write to stack (1) at position (2) the value (3)
 - peek	-	read from stack (1) at position (2) and pushes it to main
 - undo	-	increases main stack's top of stack
 - label	-	create label named (1)
 - goto	-	changes execution stack's position to the label (1)
 - call		-	like goto with (1), but also keep track of position so it can be returned to
 - cmp		-	if (3) is true, push (2), else push (1) to main stack
 - return	-	return to after previous call
 - exit		-	exit program with status code (1)
 - size		-	push to main stack the size of the main stack (before size was called)
 - printstack	-	print entire main stack
 - print	-	print value (1)
 - reverse - pushes (1) and then (2) reversing the order of the top 2 of stack
 - duplicate - pushes (1) twice
 - clear	-	sets main calculation stack's position to 0
 - is_error	-	reads top of stack and pushes whether it is of type error.
 - or		-	or's (1) and (2) and pushes the result
 - and, =, <, >, =, +, -, *, /, %	-	like or with (1) and (2)
 - getstack	-	pushes the name of the calculation stack
 - getexe		-	pushes the name of the execution stack
 - use			-	the calculation stack becomes (1) and the branch history stack becomes (2)
 - yield		-	returns to the previous execution stack, pushing to its calculation stack value (1). The position in the current execution stack or the names of its calculation and branch history stacks are not reset.
 - switch		-	changes the current execution stack to the one named (1)

