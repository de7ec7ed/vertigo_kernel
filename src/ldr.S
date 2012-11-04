#include <defines.h>

FUNCTION(ldr_call_add_module)
	push {r0 - r1}
	ldr r0, =CALLSIGN
	pop {r1 - r2}
	.word CALLSIGN
	mov pc, lr

FUNCTION(ldr_call_remove_module)
	push {r0}
	ldr r0, =CALLSIGN
	pop {r1}
	.word CALLSIGN
	mov pc, lr
