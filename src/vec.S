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

#include <defines.h>

#include <armv7lib/gen.h>

#include <kernel/vec.h>

VEC_ASM_HANDLER rst, VEC_RESET_VECTOR
VEC_ASM_HANDLER und, VEC_UNDEFINED_INSTRUCTION_VECTOR
VEC_ASM_HANDLER svc, VEC_SUPERVISOR_CALL_VECTOR
VEC_ASM_HANDLER pabt, VEC_PREFETCH_ABORT_VECTOR
VEC_ASM_HANDLER dabt, VEC_DATA_ABORT_VECTOR
VEC_ASM_HANDLER ntsd, VEC_NOT_USED_VECTOR
VEC_ASM_HANDLER irq, VEC_INTERRUPT_VECTOR
VEC_ASM_HANDLER fiq, VEC_FAST_INTERRUPT_VECTOR
