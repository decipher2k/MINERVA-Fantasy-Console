/* kernel.ld - Linker script for Fantasy Pi bare-metal kernel */

ENTRY(_start)

SECTIONS
{
    . = 0x80000;

    .text : {
        KEEP(*(.text.boot))
        *(.text .text.* .gnu.linkonce.t*)
    }

    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r*)
    }

    .data : {
        *(.data .data.* .gnu.linkonce.d*)
    }

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* Embedded cartridge ROM section */
    .rom : {
        _binary_cartridge_bin_start = .;
        KEEP(*(.rom))
        KEEP(*(.rom.*))
        _binary_cartridge_bin_end = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.gnu*)
        *(.note*)
        *(.eh_frame*)
    }
}

__bss_size = (__bss_end - __bss_start) >> 3;
