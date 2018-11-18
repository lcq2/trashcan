OUTPUT_ARCH( "riscv" )

ENTRY( reset_vector )

SECTIONS
{
    . = 0x1000;
    _ftext = .;
    PROVIDE( eprol = . );

    .text :
    {
        *(.text.init)
    }

    .text :
    {
        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
    }

    .rodata :
    {
        *(.rdata)
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
    }

    PROVIDE( etext = . );
    _etext = .;

    . = ALIGN(0x1000);

    _fdata = .;

    .data :
    {
        *(.data)
        *(.data.*)
        *(.srodata*)
        *(.gnu.linkonce.d.*)
        *(.comment)
    }

    . = ALIGN(4);

    PROVIDE( edata = . );
    _edata = .;

    . = .;
    _fbss = .;

    . = .;

    . = .;
    _bss_start = .;
    .bss :
    {
        *(.bss)
        *(.bss.*)
        *(.sbss*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
    }

    _end = .;
}