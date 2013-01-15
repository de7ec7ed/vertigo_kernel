// start of flat binary

#include <defines.h>
#include <start.h>
#include <armv7lib/int.h>

SECTION(start)

// this jump allows for the binary loader to just jump to the
// beginning of the buffer. This must be the first thing in the binary
ENTRY_POINT(default)
	// Jump to the entry point below
	b start_asm
	// Return to calling function
	//ldr r0, =CALLSIGN
	//mov pc, lr

VARIABLE(start_callsign) .word CALLSIGN

// this is the custom header
#include "hdr.S"

FUNCTION(start_asm)
	push {r1 - r12, lr}
	bl int_disable_fiq
	bl int_disable_irq
	ldr r1, =CALLSIGN
	push {r1}
	bl start_c
	pop {r1}
	push {r0}
	bl int_enable_irq
	bl int_enable_fiq
	pop {r0}
	pop {r1 - r12, pc}
