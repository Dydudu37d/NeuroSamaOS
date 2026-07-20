ENTRY(KernelMain)
SECTIONS
{
    . = 0x10000;
    .KernelPool (NOLOAD) : 
    {
        *(.KernelPool)
    }
    .text : { *(.text) }
    .data : { *(.data) }
    .bss : { 
        *(.bss)

    }
}
