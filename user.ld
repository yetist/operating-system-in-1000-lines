ENTRY(start)

_R = 4; /* 可读 */
_W = 2; /* 可写 */
_X = 1; /* 可执行 */

PHDRS
{
    text_phdr PT_LOAD FLAGS(_R|_X);
    data_phdr PT_LOAD FLAGS(_R|_W);
}

SECTIONS {
    . = 0x1000000;

    .text :{
        KEEP(*(.text.start));
        *(.text .text.*);
    } : text_phdr

    .rodata : ALIGN(4) {
        *(.rodata .rodata.*);
    } : text_phdr

    .data : ALIGN(4) {
        *(.data .data.*);
    } : data_phdr

    .bss : ALIGN(4) {
        *(.bss .bss.* .sbss .sbss.*);

        /*
         * RISC-V ABI requires 16-byte alignment for the stack.
         *
         * https://github.com/nuta/operating-system-in-1000-lines/pull/23
         */
        . = ALIGN(16);
        . += 64 * 1024; /* 64KB stack space */
        __stack_top = .;

       ASSERT(. < 0x1800000, "too large executable");
    } : data_phdr
}
