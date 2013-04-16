/* This file is part of VERTIGO.
 *
 * (C) Copyright 2013, Siege Technologies <http://www.siegetechnologies.com>
 * (C) Copyright 2013, Kirk Swidowski <http://www.swidowski.com>
 *
 * VERTIGO is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VERTIGO is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VERTIGO. If not, see <http://www.gnu.org/licenses/>.
 *
 * Written by Kirk Swidowski <kirk@swidowski.com>
 */

// start of flat binary

#include <defines.h>
#include <kernel/start.h>
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
