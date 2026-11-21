.text
.balign 16
.globl bump
bump:
	endbr64
	ret
.type bump, @function
.size bump, .-bump
/* end function bump */

.text
.balign 16
.globl calc
calc:
	endbr64
	movl %edi, %eax
	addl %esi, %eax
	imull $2, %eax, %eax
	ret
.type calc, @function
.size calc, .-calc
/* end function calc */

.text
.balign 16
.globl main
main:
	endbr64
	pushq %rbp
	movq %rsp, %rbp
	movl $7, %edi
	callq bump
	movl $11, %esi
	movl $10, %edi
	callq calc
	cmpl $42, %eax
	jnz .Lbb6
	movl $0, %eax
	jmp .Lbb7
.Lbb6:
	movl $1, %eax
.Lbb7:
	leave
	ret
.type main, @function
.size main, .-main
/* end function main */

.section .note.GNU-stack,"",@progbits
