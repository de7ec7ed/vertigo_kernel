#ifndef __END_H__
#define __END_H__

#include <types.h>

#ifdef __C__

extern u32_t end_callsign;

#endif //__C__

#ifdef __ASSEMBLY__

.extern end_callsign

#endif //__ASSEMBLY__

#endif //__END_H__
