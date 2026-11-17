BITS 32

global task_switch

section .text

; void task_switch(uint32_t *old_esp, uint32_t new_esp)
;
; Saves the callee-saved registers and the kernel stack pointer of the
; current task, then restores the incoming task's context.  For a brand new
; task the saved context returns into task_bootstrap().
task_switch:
    push ebp
    push ebx
    push esi
    push edi

    mov eax, [esp + 20]     ; old_esp
    mov edx, [esp + 24]     ; new_esp
    mov [eax], esp
    mov esp, edx

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
