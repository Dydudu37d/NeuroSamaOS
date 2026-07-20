OUTPUT_FORMAT(elf64-x86-64)
ENTRY(efi_main)

SECTIONS
{
    . = 0x10000;

    .KernelPool (NOLOAD) : 
    {
        *(.KernelPool)
    }

    .text : {
        __text_start = .;
        *(.text)
        *(.text.*)
        __text_end = .;
    }

    .rodata : {
        __rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        __rodata_end = .;
    }

    .data : {
        __data_start = .;
        *(.data)
        *(.data.*)
        __data_end = .;
    }

    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.eh_frame)
        *(.debug*)
        *(.comment)
        *(.note.gnu.property)
    }
}

__image_size = __bss_end - __text_start;
__image_end = __bss_end;