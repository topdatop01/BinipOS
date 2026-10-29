OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    /* Размещаем ядро в верхнем полупространстве (стандарт для Limine x86_64) */
    . = 0xffffffff80000000;

    .text : {
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    /* Секция для запросов загрузчику */
    .limine_requests : {
        KEEP(*(.limine_requests))
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        *(.bss .bss.*)
    }
}

