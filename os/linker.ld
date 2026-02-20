OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(efi_main)

SECTIONS
{
    . = 0;

    .text : ALIGN(4K) {
        *(.text*)
    }

    .rodata : ALIGN(4K) {
        *(.rodata*)
    }

    .sdata : ALIGN(4K) {
        *(.sdata*)
    }

    .data : ALIGN(4K) {
        *(.data*)
    }

    .dynamic : ALIGN(4K) {
        *(.dynamic)
    }

    .dynsym : ALIGN(4K) {
        *(.dynsym)
    }

    .rela : ALIGN(4K) {
        *(.rela*)
    }

    .rel : ALIGN(4K) {
        *(.rel*)
    }

    .bss : ALIGN(4K) {
        *(COMMON)
        *(.bss*)
    }

    .reloc : ALIGN(4K) {
        *(.reloc)
    }
}
