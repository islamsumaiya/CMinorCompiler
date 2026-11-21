.text
.balign 16
.globl add
add:
	endbr64
	movl %edi, %eax
	addl %esi, %eax
	ret
.type add, @function
.size add, .-add
/* end function add */

.text
.balign 16
.globl main
main:
	endbr64
	pushq %rbp
	movq %rsp, %rbp
	movl $32, %esi
	movl $10, %edi
	callq add
	cmpl $42, %eax
	jz .Lbb4
	movl $1, %eax
	jmp .Lbb5
.Lbb4:
	movl $0, %eax
.Lbb5:
	leave
	ret
.type main, @function
.size main, .-main
/* end function main */

.section .note.GNU-stack,"",@progbits
