global outb
global inb
global outw ; shutdown

outb:
  mov al,[esp + 8]
  mov dx,[esp + 4]
  out dx,al
  ret


inb:
  mov dx,[esp+4]
  xor eax,eax
  in al,dx
  ret

;shutdown
outw:
    mov dx, [esp + 4]
    mov ax, [esp + 8]
    out dx, ax
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
