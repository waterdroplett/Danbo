ENTRY(_start)
SECTIONS {
    . = 1M;
    kernel_start = .;
    .text : { *(.multiboot) *(.text) }
    .rodata : { *(.rodata*) }
    .data : { *(.data) }
    .bss : { *(COMMON) *(.bss) }
    . = ALIGN(4K);
    kernel_end = .;
}