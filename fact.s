.text
.balign 16
.globl fact
fact:
	endbr64
	pushq %rbp
	movq %rsp, %rbp
	subq $8, %rsp
	pushq %rbx
	cmpl $1, %edi
	jle .Lbb2
	movl %edi, %ebx
	subl $1, %edi
	callq fact
	movl %ebx, %edi
	imull %edi, %eax
	jmp .Lbb3
.Lbb2:
	movl $1, %eax
.Lbb3:
	popq %rbx
	leave
	ret
.type fact, @function
.size fact, .-fact
/* end function fact */

.text
.balign 16
.globl main
main:
	endbr64
	pushq %rbp
	movq %rsp, %rbp
	movl $5, %edi
	callq fact
	cmpl $120, %eax
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
