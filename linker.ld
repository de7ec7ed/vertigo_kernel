OUTPUT_FORMAT(elf32-littlearm)
ENTRY(default)

SECTIONS
{
	. = 0x00000000;
	.text : {
	    start = .;
	    *start.S.o(.start*)
	}
	. = ALIGN(4);
	.text : {
		text = .;
		*(.text*);
	}
	. = ALIGN(4);
	.text : {
		rodata = .;
		*(.rodata*);
	}
	. = ALIGN(4);
	.text :	{
		data = .;
		*(.data*)
	}
	. = ALIGN(4);
	.text : {
		bss = .;
		*(.bss*);
	}
	. = ALIGN(4);
	.text : {
		*end.S.o(.end*)
	}
	/DISCARD/ : {
		*(.ARM.attributes);
		*(.comment);
		*(.LC0);
	}
		
}
