#include <config.h>
#include <defines.h>
#include <types.h>

#include <dbglib/gen.h>
#include <fxplib/gen.h>
#include <stdlib/check.h>
#include <stdlib/stdarg.h>
#include <stdlib/string.h>

#include <kernel/log.h>
#include <kernel/mas.h>
#include <kernel/vec.h>

DBG_DEFINE_VARIABLE(log_dbg, DBG_LEVEL_2);

result_t log_init(void) {

	log_state_t *state;

	DBG_LOG_FUNCTION(log_dbg, DBG_LEVEL_3);

	state = malloc(sizeof(log_state_t));

	CHECK_NOT_NULL(state, "unable to allocate memory for the state", state, log_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	memset(state, 0, sizeof(log_state_t));

	CHECK_SUCCESS(call_register_handler(LOG_CALL_IDENTIFIER, gen_add_base(&log_call_handler), state), "unable to add call handler", FAILURE, log_dbg, DBG_LEVEL_2)
		return FAILURE;
	CHECK_END

	return SUCCESS;
}

result_t log_fini(void) {

	// TODO: stuff here

	return FAILURE;
}

result_t log_call_handler(call_handler_t *handler, void *data, gen_general_purpose_registers_t *registers) {

	u32_t identifier;

	DBG_LOG_FUNCTION(log_dbg, DBG_LEVEL_3);

	identifier = (u32_t)(registers->r2);

	DBG_LOG_STATEMENT("identifier", identifier, log_dbg, DBG_LEVEL_3);

	CHECK_NOT_NULL(handler->data, "handler->data is null", handler->data, log_dbg, DBG_LEVEL_2)
		registers->r0 = FAILURE;
		return SUCCESS;
	CHECK_END

	data = handler->data;

	if(identifier == LOG_FUNCTION_INIT) {
		CHECK_SUCCESS(log_call_init_handler((log_state_t *)data), "call init function handler failure", identifier, log_dbg, DBG_LEVEL_2)
			registers->r0 = FAILURE;
			return SUCCESS;
		CHECK_END
	}
	else if(identifier == LOG_FUNCTION_BUFFER_SIZE) {
		CHECK_SUCCESS(log_call_buffer_size_handler((log_state_t *)data, registers), "buffer size function handler failure", identifier, log_dbg, DBG_LEVEL_2)
			registers->r0 = FAILURE;
			return SUCCESS;
		CHECK_END
	}
	else if(identifier == LOG_FUNCTION_BUFFER_VALUE) {
		CHECK_SUCCESS(log_call_buffer_value_handler((log_state_t *)data, registers), "buffer value function handler failure", identifier, log_dbg, DBG_LEVEL_2)
			registers->r0 = FAILURE;
			return SUCCESS;
		CHECK_END
	}
	else if(identifier == LOG_FUNCTION_FINI) {
		CHECK_SUCCESS(log_call_fini_handler((log_state_t *)data), "call fini function handler failure", identifier, log_dbg, DBG_LEVEL_2)
			registers->r0 = FAILURE;
			return SUCCESS;
		CHECK_END
	}
	else {
		DBG_LOG_STATEMENT("unhandled log function", identifier, log_dbg, DBG_LEVEL_3);
		registers->r0 = FAILURE;
		return SUCCESS;
	}

	registers->r0 = SUCCESS;
	return SUCCESS;
}

result_t log_call_init_handler(log_state_t *state) {

	DBG_LOG_FUNCTION(log_dbg, DBG_LEVEL_3);

	if(state->buffer != NULL) {
		free(state->buffer);
	}

	memset(state, 0, sizeof(log_state_t));

	mem_get_buffer_size(&(state->size));

	state->buffer = malloc(state->size);

	mem_get_buffer(state->buffer);

	return SUCCESS;
}

result_t log_call_buffer_size_handler(log_state_t *state, gen_general_purpose_registers_t *registers) {

	DBG_LOG_FUNCTION(log_dbg, DBG_LEVEL_3);

	registers->r1 = (u32_t)(state->size);

	return SUCCESS;
}

result_t log_call_buffer_value_handler(log_state_t *state, gen_general_purpose_registers_t *registers) {

	DBG_LOG_FUNCTION(log_dbg, DBG_LEVEL_3);

	if(state->index >= state->size) {
		return FAILURE;
	}

	if((state->size - state->index) < sizeof(u32_t)) {
		memcpy(&(registers->r1), &((state->buffer)[state->index]), (state->size - state->index));
		state->index += (state->size - state->index);
	}
	else {
		memcpy(&(registers->r1), &((state->buffer)[state->index]), sizeof(u32_t));
		state->index += sizeof(u32_t);
	}

	return SUCCESS;
}

result_t log_call_fini_handler(log_state_t *state) {

	DBG_LOG_FUNCTION(log_dbg, DBG_LEVEL_3);

	if(state->buffer != NULL) {
		free(state->buffer);
	}

	memset(state, 0, sizeof(log_state_t));

	mem_clear();

	return SUCCESS;
}

result_t log_putc(u8_t c) {

	#ifdef __SERIAL_DEBUG__
	if(ser_putc(c) == FAILURE) {
		return FAILURE;
	}
	#endif //__SERIAL_DEBUG__

	#ifdef __MEMORY_DEBUG__
	if(mem_putc(c) == FAILURE) {
		return FAILURE;
	}
	#endif //__MEMORY_DEBUG__

	return SUCCESS;
}

result_t log_write(u8_t *buffer, size_t size) {

	if(buffer == NULL) {
		return FAILURE;
	}

	#ifdef __SERIAL_DEBUG__
	if(ser_write(buffer, size) == FAILURE) {
		return FAILURE;
	}
	#endif //__SERIAL_DEBUG__

	#ifdef __MEMORY_DEBUG__
	if(mem_write(buffer, size) == FAILURE) {
		return FAILURE;
	}
	#endif //__MEMORY_DEBUG__

	return SUCCESS;
}

result_t log_vprintf(const char *fmt, va_list args) {

	size_t value;
	u8_t number[8];

	while(*fmt) {

		if(*fmt == '%') {

			fmt++;

			if(*fmt == 's') {

				value = va_arg(args, size_t);

				if(log_write((u8_t *)value, strlen((const char *)value)) != SUCCESS) {
					return FAILURE;
				}

				fmt++;
			}
			else if(*fmt == 'c') {

				value = va_arg(args, u32_t);

				if(log_putc((u8_t)value) != SUCCESS) {
					return FAILURE;
				}

				fmt++;
			}
			else if(*fmt == 'o') {

				value = va_arg(args, u32_t);

				memset(number, 0, sizeof(number));

				itoa(value, (char *)number, 8);

				if(log_write(number, strlen((const char *)number)) != SUCCESS) {
					return FAILURE;
				}

				fmt++;
			}
			else if(*fmt == 'x') {

				value = va_arg(args, u32_t);

				memset(number, 0, sizeof(number));

				itoa(value, (char *)number, 16);

				if(log_write(number, strlen((const char *)number)) != SUCCESS) {
					return FAILURE;
				}

				fmt++;
			}
			else if(*fmt == 'd') {

				value = va_arg(args, u32_t);

				memset(number, 0, sizeof(number));

				itoa(value, (char *)number, 10);

				if(log_write(number, strlen((const char *)number)) != SUCCESS) {
					return FAILURE;
				}

				fmt++;
			}
			else if(*fmt == 'p') {

				value = va_arg(args, size_t);

				memset(number, 0, sizeof(number));

				ltoa(value, (char *)number, 16);

				if(log_write(number, strlen((const char *)number)) != SUCCESS) {
					return FAILURE;
				}

				fmt++;
			}
		}

		if(*fmt == '\n') {

			if(log_putc('\r') != SUCCESS) {
				return FAILURE;
			}
		}

		if(log_putc(*fmt) != SUCCESS) {
			return FAILURE;
		}

		fmt++;
	}

	return SUCCESS;
}

result_t log_printf(const char *fmt, ...) {

	va_list args;
	result_t result;

	va_start(args, fmt);

	result = log_vprintf(fmt, args);

	va_end(args);

	return result;
}

result_t log_get_debug_level(size_t *level) {

	DBG_LOG_FUNCTION(log_dbg, DBG_LEVEL_3);

	DBG_GET_VARIABLE(log_dbg, *level);

	return SUCCESS;
}

result_t log_set_debug_level(size_t level) {

	DBG_LOG_FUNCTION(log_dbg, DBG_LEVEL_3);

	DBG_SET_VARIABLE(log_dbg, level);

	return SUCCESS;
}
