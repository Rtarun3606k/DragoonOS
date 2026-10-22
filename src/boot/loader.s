global loader
extern kmain

MAGIC_NUMBER equ 0x1BADB002
ALIGN_MODULES equ 0x00000001
CHECKSUM equ -(MAGIC_NUMBER + ALIGN_MODULES)

KERNEL_STACK_SIZE equ 4096

section .text
    align 4
    dd MAGIC_NUMBER
    dd ALIGN_MODULES
    dd CHECKSUM

loader:
    cmp eax, 0x2BADB002
    jne .boot_error
    mov esp, kernel_stack + KERNEL_STACK_SIZE
    xor ebp, ebp

    ; Preserve GRUB's Multiboot information pointer.
    push ebx
    push eax
    call kmain
    add esp,8

.hlt:
    cli
    hlt
    jmp .hlt

section .bss
    align 16

.boot_error:
    cli
    hlt
    jmp .boot_error

kernel_stack:
    resb KERNEL_STACK_SIZE
