#ifndef __KERNEL_LOG_H__
#define __KERNEL_LOG_H__

#include <types.h>

#include <armv7lib/gen.h>

#include <kernel/call.h>

#include <stdlib/stdarg.h>

#define LOG_CALL_IDENTIFIER 0x00000000

#define LOG_FUNCTION_INIT         0
#define LOG_FUNCTION_BUFFER_SIZE  1
#define LOG_FUNCTION_BUFFER_VALUE 2
#define LOG_FUNCTION_FINI         3

#ifdef __C__

#define printf(a, ...) log_printf(gen_add_base(a), ##__VA_ARGS__)

typedef struct log_state log_state_t;

struct log_state {
	u8_t *buffer;
	u32_t size;
	size_t index;
};

extern result_t log_init(void);
extern result_t log_fini(void);

extern result_t log_call_handler(call_handler_t *handler, void *data, gen_general_purpose_registers_t *registers);

extern result_t log_call_init_handler(log_state_t *state);
extern result_t log_call_buffer_size_handler(log_state_t *state, gen_general_purpose_registers_t *registers);
extern result_t log_call_buffer_value_handler(log_state_t *state, gen_general_purpose_registers_t *registers);
extern result_t log_call_fini_handler(log_state_t *state);

extern result_t log_putc(u8_t c);
extern result_t log_write(u8_t *buffer, size_t size);
extern result_t log_printf(const char *fmt, ...);
extern result_t log_vprintf(const char *fmt, va_list args);

extern result_t log_get_debug_level(size_t *level);
extern result_t log_set_debug_level(size_t level);

#endif //__C__

#endif //__KERNEL_LOG_H__
