ENTRY(boot)

_R = 4; /* 可读 */
_W = 2; /* 可写 */
_X = 1; /* 可执行 */

PHDRS
{
    text_phdr PT_LOAD FLAGS(_R|_X);
    data_phdr PT_LOAD FLAGS(_R|_W);
}

SECTIONS {
    . = 0x9000000080200000;
    __kernel_base = .;

    .text :{
        KEEP(*(.text.boot));
        *(.text .text.*);
    } : text_phdr

    .rodata : ALIGN(4) {
        *(.rodata .rodata.*);
    } : text_phdr

    .data : ALIGN(4) {
        *(.data .data.*);
    } : data_phdr

    .bss : ALIGN(4) {
        __bss = .;
        *(.bss .bss.* .sbss .sbss.*);
        __bss_end = .;
    } : data_phdr

    . = ALIGN(4);
    . += 128 * 1024; /* 128KB */
    __stack_top = .;

    . = ALIGN(4096);
    __free_ram = .;
    . += 64 * 1024 * 1024; /* 64MB */
    __free_ram_end = .;
}
