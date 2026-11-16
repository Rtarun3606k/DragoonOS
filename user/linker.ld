ENTRY(_start)

SECTIONS {
    . = 0x08048000;

    .text : { *(.text*) }

    . = ALIGN(4);
    .rodata : { *(.rodata*) }

    . = ALIGN(16);
    .data : {
        _sdata = .;
        _sidata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    }

    . = ALIGN(16);
    .bss : {
        _sbss = .;
        *(COMMON)
        *(.bss*)
        . = ALIGN(4);
        _ebss = .;

        /* TinyGo's conservative GC never grows the heap on baremetal,
           so reserve it inside the loaded image. */
        . = . + 0x00100000;
        _heap_end = .;
    }

    _globals_start = _sdata;
    _globals_end   = _ebss;
    _heap_start    = _ebss;
    _stack_top     = 0xC0000000;  /* DragoonOS user stack top */
}
