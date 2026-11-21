.text
.balign 16
.globl main
main:
	endbr64
	movl $0, %eax
	movl %eax, %esi
	movl $0, %edx
	movl $0, %ecx
	movl $1, %eax
.p2align 4
.Lbb2:
	cmpl $5, %eax
	jg .Lbb5
	cmpl $3, %eax
	setz %r8b
	movzbl %r8b, %r8d
	movl %eax, %edi
	addl %ecx, %edi
	cmpl $0, %r8d
	cmovz %edi, %ecx
	addl $1, %eax
	jmp .Lbb2
.Lbb5:
	cmpl $0, %edx
	jnz .Lbb11
	cmpl $12, %ecx
	jnz .Lbb10
	cmpl $0, %esi
	jnz .Lbb9
	movl $0, %eax
	jmp .Lbb12
.Lbb9:
	movl $1, %eax
	jmp .Lbb12
.Lbb10:
	movl $1, %eax
	jmp .Lbb12
.Lbb11:
	movl $1, %eax
.Lbb12:
	ret
.type main, @function
.size main, .-main
/* end function main */

.section .note.GNU-stack,"",@progbits
