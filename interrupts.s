global interrupt_common

extern interrupt_handler

section .text

interrupt_common:
    pusha

    ; Pass a pointer to the saved registers to C.
    push esp
    call interrupt_handler
    add esp, 4

    popa

    ; Remove interrupt number and error code.
    add esp, 8

    iret

section .note.GNU-stack noalloc noexec nowrite progbits
