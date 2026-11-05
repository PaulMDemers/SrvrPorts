/* x86-64 SysV, one raw JSCell pointer in the selected location only.
 * Caller owns the reference in complemented form. No calls in the window.
 * Audit the final target object and caller disassembly before using results. */
.text
.globl jsc_gc_roots_park
.type jsc_gc_roots_park,@function
jsc_gc_roots_park:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    subq $16,%rsp
    movq $0,(%rsp)
    movq $0,8(%rsp)
    fnstcw (%rsp)                  /* x87 control word is callee-saved */
    movq %rdi,%rbx                  /* encoded cell */
    movq %rsi,%r15                  /* 0 control, 1 r12, 2 xmm15, 3 red zone */
    movq %rdx,%r13                  /* ready address, non-cell stack storage */
    movq %rcx,%r14                  /* release address */
    movq $0,-8(%rsp)
    movq $0,-16(%rsp)
    movq $0,-24(%rsp)
    movq $0,-32(%rsp)
    movq $0,-40(%rsp)
    movq $0,-48(%rsp)
    movq $0,-56(%rsp)
    movq $0,-64(%rsp)
    movq $0,-72(%rsp)
    movq $0,-80(%rsp)
    movq $0,-88(%rsp)
    movq $0,-96(%rsp)
    movq $0,-104(%rsp)
    movq $0,-112(%rsp)
    movq $0,-120(%rsp)
    movq $0,-128(%rsp)
    pxor %xmm0,%xmm0
    pxor %xmm1,%xmm1
    pxor %xmm2,%xmm2
    pxor %xmm3,%xmm3
    pxor %xmm4,%xmm4
    pxor %xmm5,%xmm5
    pxor %xmm6,%xmm6
    pxor %xmm7,%xmm7
    pxor %xmm8,%xmm8
    pxor %xmm9,%xmm9
    pxor %xmm10,%xmm10
    pxor %xmm11,%xmm11
    pxor %xmm12,%xmm12
    pxor %xmm13,%xmm13
    pxor %xmm14,%xmm14
    pxor %xmm15,%xmm15
    /* Overwrite x87/MMX physical slots, then mark the x87 stack empty. */
    fninit
    fldz
    fldz
    fldz
    fldz
    fldz
    fldz
    fldz
    fldz
    fninit
    xorl %r12d,%r12d
    testq %r15,%r15
    jz .Lpark_clear
    notq %rbx
    cmpq $1,%r15
    jne .Lpark_xmm
    movq %rbx,%r12
    jmp .Lpark_clear
.Lpark_xmm:
    cmpq $2,%r15
    jne .Lpark_red
    movq %rbx,%xmm15
    jmp .Lpark_clear
.Lpark_red:
    cmpq $3,%r15
    jne .Lpark_invalid
    movq %rbx,-128(%rsp)
.Lpark_clear:
    xorl %ebx,%ebx
    xorl %eax,%eax
    xorl %ecx,%ecx
    xorl %edx,%edx
    xorl %esi,%esi
    xorl %edi,%edi
    xorl %r8d,%r8d
    xorl %r9d,%r9d
    xorl %r10d,%r10d
    xorl %r11d,%r11d
    movl $1,(%r13)
.Lpark_wait:
    cmpl $0,(%r14)
    je .Lpark_wait
    xorl %r12d,%r12d
    pxor %xmm15,%xmm15
    movq $0,-128(%rsp)
    fldcw (%rsp)
    addq $16,%rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret
.Lpark_invalid:
    ud2
.size jsc_gc_roots_park,.-jsc_gc_roots_park
.section .note.GNU-stack,"",@progbits
