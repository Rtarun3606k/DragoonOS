

BITS 32


global loader
global page_directory
global page_tables
extern kmain

MAGIC_NUMBER equ 0x1BADB002
ALIGN_MODULES equ 0x00000001
CHECKSUM equ -(MAGIC_NUMBER + ALIGN_MODULES)

PAGE_SIZE equ 4096
PAGE_ENTRIES equ 1024
PAGE_TABLE_COUNT equ 4
KERNEL_PDE_INDEX equ 768
KERNEL_STACK_SIZE equ 4096
HIGHER_HALF_BASE equ 0xC0000000

section .multiboot
align 4
    dd MAGIC_NUMBER
    dd ALIGN_MODULES
    dd CHECKSUM

section .text.boot
loader:
    cli

    ; Validate the Multiboot magic number.
    cmp eax, 0x2BADB002
    jne .boot_error

    ; Preserve Multiboot values.
    mov [boot_magic], eax
    mov [boot_info], ebx

    ; Set up the bootstrap stack.
    mov esp, kernel_stack + KERNEL_STACK_SIZE
    xor ebp, ebp
    cld

    ; Clear the page directory.
    mov edi, page_directory
    xor eax, eax
    mov ecx, PAGE_ENTRIES
    rep stosd

    ; Clear all four page tables.
    mov edi, page_tables
    xor eax, eax
    mov ecx, PAGE_ENTRIES * PAGE_TABLE_COUNT
    rep stosd

    ; Fill four page tables: physical memory 0–16 MiB.
    mov edi, page_tables
    mov eax, 0x003
    mov ecx, PAGE_ENTRIES * PAGE_TABLE_COUNT

.fill_tables:
    mov [edi], eax
    add eax, PAGE_SIZE
    add edi, 4
    loop .fill_tables

    ; Temporarily identity-map the first 4 MiB.
    mov eax, page_tables
    or eax, 0x003
    mov [page_directory], eax

    ; Map physical 0–16 MiB at virtual 0xC0000000–0xC0FFFFFF.
    xor ecx, ecx

.map_tables:
    mov eax, ecx
    shl eax, 12
    add eax, page_tables
    or eax, 0x003

    mov edx, KERNEL_PDE_INDEX
    add edx, ecx
    mov [page_directory + edx * 4], eax

    inc ecx
    cmp ecx, PAGE_TABLE_COUNT
    jne .map_tables

    ; Load physical page-directory address.
    mov eax, page_directory
    mov cr3, eax

    ; Enable paging.
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; Jump to higher-half code.
    mov eax, higher_half_entry
    jmp eax

.boot_error:
    cli
    hlt
    jmp .boot_error

section .text
higher_half_entry:
    ; Switch to the higher-half stack.
    mov esp, kernel_stack + KERNEL_STACK_SIZE + HIGHER_HALF_BASE
    xor ebp, ebp

    ; Read saved boot values through higher-half aliases.
    mov eax, [boot_magic + HIGHER_HALF_BASE]
    mov ebx, [boot_info + HIGHER_HALF_BASE]

    ; Remove identity mapping after reaching higher-half code.
    mov dword [page_directory + HIGHER_HALF_BASE], 0

    ; Flush cached translations.
    mov ecx, page_directory
    mov cr3, ecx

    ; kmain(magic, multiboot_info_pointer).
    push ebx
    push eax
    call kmain
    add esp, 8

.hlt:
    cli
    hlt
    jmp .hlt

section .boot.bss
align 4096
page_directory:
    resd PAGE_ENTRIES

align 4096
page_tables:
    resd PAGE_ENTRIES * PAGE_TABLE_COUNT

align 16
kernel_stack:
    resb KERNEL_STACK_SIZE

align 4
boot_magic:
    resd 1

boot_info:
    resd 1

