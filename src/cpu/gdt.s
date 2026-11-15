

global gdt_load
global tss_flush
global enter_usermode

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


tss_flush: ; task state segment
    ; Entry 5 in the GDT is at byte offset 5 * 8 = 40 (0x28)
    mov ax, 0x28
    ltr ax
    ret



enter_usermode:
    cli                     ; Disable interrupts while building the iret frame

    mov eax, [esp + 4]      ; Argument 1: entry_eip
    mov ebx, [esp + 8]      ; Argument 2: user_esp

    ; Load Ring 3 data segment selector (0x20 | RPL 3 = 0x23)
    mov cx, 0x23
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx

    ; Build the Ring 3 interrupt-return stack frame
    push 0x23               ; SS  (User Data Selector)
    push ebx                ; ESP (User Stack Pointer)
    pushfd                  ; EFLAGS
    pop edx
    or edx, 0x200           ; Set IF (bit 9) so PIT/Keyboard IRQs keep firing in Ring 3
    push edx                ; EFLAGS (with interrupts enabled)
    push 0x1B               ; CS  (User Code Selector: 0x18 | RPL 3)
    push eax                ; EIP (User Entry Point)

    iret                    ; Pop EIP, CS, EFLAGS, ESP, SS -> drops to Ring 3


section .note.GNU-stack noalloc noexec nowrite progbits
