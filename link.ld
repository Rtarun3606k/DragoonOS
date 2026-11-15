


ENTRY(loader)

KERNEL_BASE = 0xC0000000;
PHYS_BASE   = 0x00100000;

SECTIONS
{
    /* Low-address bootstrap: GRUB enters here before paging. */
    . = PHYS_BASE;

    .boot ALIGN(0x1000) :
    {
        KEEP(*(.multiboot))
        *(.text.boot)
    }

    .boot.bss ALIGN(0x1000) (NOLOAD) :
    {
        *(.boot.bss)
    }

    /* Physical address immediately following the bootstrap. */
    __kernel_phys_start =
        ALIGN(LOADADDR(.boot.bss) + SIZEOF(.boot.bss), 0x1000);

    /* Link the main kernel at its higher-half virtual address. */
    . = KERNEL_BASE + __kernel_phys_start;

    .text ALIGN(0x1000) : AT(ADDR(.text) - KERNEL_BASE)
    {
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(0x1000) : AT(ADDR(.rodata) - KERNEL_BASE)
    {
        *(.rodata*)
        *(.eh_frame*)
    }

    .data ALIGN(0x1000) : AT(ADDR(.data) - KERNEL_BASE)
    {
        *(.data*)
        *(.got*)
    }

    .bss ALIGN(0x1000) (NOLOAD) : AT(ADDR(.bss) - KERNEL_BASE)
    {
        *(COMMON)
        *(.bss*)
    }
  __kernel_phys_end = . - KERNEL_BASE;

    /DISCARD/ :
    {
        *(.note.GNU-stack)
    }
}

