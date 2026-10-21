

global gdt_load

section .text

gdt_load:
    ; Argument: pointer to the GDT descriptor
    mov eax, [esp + 4]
    lgdt [eax]

    ; Reload data segment registers with selector 0x10
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Reload CS using a far jump
    jmp 0x08:.flush_cs

.flush_cs:
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
