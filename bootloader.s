.code64
.global efi_main
.extern Cefi_main

.section .text
efi_main:
    mov %rsp, %rax
    and $-16, %rsp
    push %rax
    leaq Cefi_main(%rip), %rax
    jmp *%rax

