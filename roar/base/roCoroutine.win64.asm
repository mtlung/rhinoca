COMMENT [[May change to use intrinic, see http://v8.googlecode.com/svn/branches/bleeding_edge/src/x64/codegen-x64.cc]]

.code
COMMENT [[Source: http://github.com/JuliaLang/julia/blob/master/src/support/_setjmp.win64.S]]
roSetJmp proc
	mov rdx,QWORD PTR [rsp]
	mov QWORD PTR [rcx],0
	mov QWORD PTR [rcx+8],rbx
	mov QWORD PTR [rcx+16],rsp
	mov QWORD PTR [rcx+24],rbp
	mov QWORD PTR [rcx+32],rsi
	mov QWORD PTR [rcx+40],rdi
	mov QWORD PTR [rcx+48],r12
	mov QWORD PTR [rcx+56],r13
	mov QWORD PTR [rcx+64],r14
	mov QWORD PTR [rcx+72],r15
	mov QWORD PTR [rcx+80],rdx
	mov QWORD PTR [rcx+88],0
	movaps XMMWORD PTR [rcx+96],xmm6
	movaps XMMWORD PTR [rcx+112],xmm7
	movaps XMMWORD PTR [rcx+128],xmm8
	movaps XMMWORD PTR [rcx+144],xmm9
	movaps XMMWORD PTR [rcx+160],xmm10
	movaps XMMWORD PTR [rcx+176],xmm11
	movaps XMMWORD PTR [rcx+192],xmm12
	movaps XMMWORD PTR [rcx+208],xmm13
	movaps XMMWORD PTR [rcx+224],xmm14
	movaps XMMWORD PTR [rcx+240],xmm15
	xor rax,rax
	ret
roSetJmp endp

COMMENT [[Source: http://github.com/JuliaLang/julia/blob/master/src/support/_longjmp.win64.S]]
roLongJmp proc
	mov rbx,QWORD PTR [rcx+8]
	mov rsp,QWORD PTR [rcx+16]
	mov rbp,QWORD PTR [rcx+24]
	mov rsi,QWORD PTR [rcx+32]
	mov rdi,QWORD PTR [rcx+40]
	mov r12,QWORD PTR [rcx+48]
	mov r13,QWORD PTR [rcx+56]
	mov r14,QWORD PTR [rcx+64]
	mov r15,QWORD PTR [rcx+72]
	mov r8, QWORD PTR [rcx+80]
	movaps xmm6,XMMWORD PTR [rcx+96]
	movaps xmm7,XMMWORD PTR [rcx+112]
	movaps xmm8,XMMWORD PTR [rcx+128]
	movaps xmm9,XMMWORD PTR [rcx+144]
	movaps xmm10,XMMWORD PTR [rcx+160]
	movaps xmm11,XMMWORD PTR [rcx+176]
	movaps xmm12,XMMWORD PTR [rcx+192]
	movaps xmm13,XMMWORD PTR [rcx+208]
	movaps xmm14,XMMWORD PTR [rcx+224]
	movaps xmm15,XMMWORD PTR [rcx+240]
	mov eax,edx
	test eax,eax
	jne a
	inc eax
a: mov QWORD PTR [rsp],r8
	ret
roLongJmp endp

COMMENT [[Not yet used, suppose to enable CORO_ASM]]
_coro_transfer proc
	movaps xmm6, XMMWORD PTR [rsp]
	movaps xmm7, XMMWORD PTR [rsp+16]
	movaps xmm8, XMMWORD PTR [rsp+32]
	movaps xmm9, XMMWORD PTR [rsp+48]
	movaps xmm10, XMMWORD PTR [rsp+64]
	movaps xmm11, XMMWORD PTR [rsp+80]
	movaps xmm12, XMMWORD PTR [rsp+96]
	movaps xmm13, XMMWORD PTR [rsp+112]
	movaps xmm14, XMMWORD PTR [rsp+128]
	movaps xmm15, XMMWORD PTR [rsp+144]
	push rsi
	push rdi
	push rbp
	push rbx
	push r12
	push r13
	push r14
	push r15
_coro_transfer endp

end
