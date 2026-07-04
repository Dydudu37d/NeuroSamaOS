.global machine_check_handler
machine_check_handler:
    cli
    leaq mc_saved_old_rsp(%rip), %rax
    movq %rsp, (%rax)
    leaq mc_emergency_stack(%rip), %rax
    addq $4096, %rax
    movq %rax, %rsp
    movq %rax, %rbp
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq $0x17A, %rcx
    rdmsr
    shlq $32, %rdx
    orq  %rdx, %rax
    movq %rax, %r15
    movq $0x3F8, %rdx
    movb $'M', %al
    outb %al, %dx
    movb $'C', %al
    outb %al, %dx
    movb $':', %al
    outb %al, %dx
    movb $' ', %al
    outb %al, %dx
    movq %r15, %rdi
    call print_hex_asm
    movb $' ', %al
    outb %al, %dx
    testq $4, %r15
    jz 1f
    leaq msg_mcip(%rip), %rsi
    call print_string_asm
    jmp 2f
    1:
    leaq msg_mce(%rip), %rsi
    call print_string_asm
    2:
    movq $0x17B, %rcx
    rdmsr
    andq $0xFF, %rax
    movq %rax, %r14
    leaq msg_bank_count(%rip), %rsi
    call print_string_asm
    movq %r14, %rdi
    call print_dec_asm
    call print_newline_asm
    xorq %r13, %r13
    3:
    cmpq %r14, %r13
    jge 9f
    movq %r13, %rax
    shlq $2, %rax
    addq $0x401, %rax
    movq %rax, %rcx
    rdmsr
    shlq $32, %rdx
    orq  %rdx, %rax
    movq %rax, %r12
    testq 0x8000000000000000, %rax
    jz 8f
    leaq msg_bank(%rip), %rsi
    call print_string_asm
    movq %r13, %rdi
    call print_dec_asm
    movb $'=', %al
    outb %al, %dx
    movb $'0', %al
    outb %al, %dx
    movb $'x', %al
    outb %al, %dx
    movq %r12, %rdi
    call print_hex_asm
    testq 0x2000000000000000, %r12
    jz 4f
    leaq msg_uc(%rip), %rsi
    call print_string_asm
    jmp 5f
    4:
    leaq msg_corrected(%rip), %rsi
    call print_string_asm
    5:
    testq 0x0400000000000000, %r12
    jz 6f
    leaq msg_pcc(%rip), %rsi
    call print_string_asm
    6:
    movq %r13, %rax
    shlq $2, %rax
    addq $0x402, %rax
    movq %rax, %rcx
    rdmsr
    shlq $32, %rdx
    orq  %rdx, %rax
    testq %rax, %rax
    jz 7f
    leaq msg_addr(%rip), %rsi
    call print_string_asm
    movq %rax, %rdi
    call print_hex_asm
    7:
    movq %r13, %rax
    shlq $2, %rax
    addq $0x403, %rax
    movq %rax, %rcx
    rdmsr
    shlq $32, %rdx
    orq  %rdx, %rax
    testq %rax, %rax
    jz 8f
    leaq msg_misc(%rip), %rsi
    call print_string_asm
    movq %rax, %rdi
    call print_hex_asm
    8:
    call print_newline_asm
    incq %r13
    jmp 3b
    9:
    movq $0x17A, %rcx
    xorq %rax, %rax
    xorq %rdx, %rdx
    wrmsr
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax
    movq mc_saved_old_rsp(%rip), %rsp
    iretq
    10:
    leaq msg_halt(%rip), %rsi
    call print_string_asm
    cli
    11: hlt
    jmp 11b
    print_string_asm:
    pushq %rax
    pushq %rdx
    movq $0x3F8, %rdx
    1: movb (%rsi), %al
    testb %al, %al
    jz 2f
    outb %al, %dx
    incq %rsi
    jmp 1b
    2: popq %rdx
    popq %rax
    ret
    print_hex_asm:
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    movq $0x3F8, %rdx
    movq $60, %rcx
    leaq hex_table(%rip), %rsi
    1: movq %rdi, %rax
    shrq %cl, %rax
    andq $0xF, %rax
    movb (%rsi, %rax), %al
    outb %al, %dx
    subq $4, %rcx
    jge 1b
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax
    ret
    print_dec_asm:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    movq $0x3F8, %rdx
    movq $10, %rcx
    xorq %rbx, %rbx
    1: xorq %rdx, %rdx
    divq %rcx
    pushq %rdx
    incq %rbx
    testq %rax, %rax
    jnz 1b
    movq $0x3F8, %rdx
    2: popq %rax
    addb $'0', %al
    outb %al, %dx
    decq %rbx
    jnz 2b
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    ret
    print_newline_asm:
    pushq %rax
    pushq %rdx
    movq $0x3F8, %rdx
    movb $'\r', %al
    outb %al, %dx
    movb $'\n', %al
    outb %al, %dx
    popq %rdx
    popq %rax
    ret
    hex_table: .ascii "0123456789ABCDEF"
    msg_mcip: .asciz "MCIP "
    msg_mce: .asciz "MCE "
    msg_bank_count: .asciz "Banks:"
    msg_bank: .asciz "Bank"
    msg_uc: .asciz " UNCORRECTED"
    msg_corrected: .asciz " CORRECTED"
    msg_pcc: .asciz " PCC"
    msg_addr: .asciz " ADDR:0x"
    msg_misc: .asciz " MISC:0x"
    msg_halt: .asciz "\r\nHALTING\r\n"
