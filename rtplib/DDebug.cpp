#include "DDebug.h"
#include "tsk_debug.h"
#include "tsk_string.h"
#include "tsk_memory.h"

enum cb_type {
	cb_info,
	cb_warn,
	cb_error,
	cb_fatal
};

int debug_xxx_cb(const void* arg, const char* fmt, enum cb_type type, va_list *app)
{
	int ret = -1;
	if (!arg) {
		return -1;
	}

	DDebugCallback *pDDebug = (DDebugCallback *)arg;

	if (pDDebug) {
		char* message = tsk_null;
		tsk_sprintf_2(&message, fmt, app);

		switch (type) {
		case cb_info:
		{
			ret = pDDebug->OnDebugInfo(message);
			break;
		}
		case cb_warn:
		{
			ret = pDDebug->OnDebugWarn(message);
			break;
		}
		case cb_error:
		{
			ret = pDDebug->OnDebugError(message);
			break;
		}
		case cb_fatal:
		{
			ret = pDDebug->OnDebugFatal(message);
			break;

		}
		}
		TSK_FREE(message);
	}

	return ret;
}

DDebugCallback::DDebugCallback()
{
	tsk_debug_set_arg_data(this);
	tsk_debug_set_info_cb(DDebugCallback::debug_info_cb);
	tsk_debug_set_warn_cb(DDebugCallback::debug_warn_cb);
	tsk_debug_set_error_cb(DDebugCallback::debug_error_cb);
	tsk_debug_set_fatal_cb(DDebugCallback::debug_fatal_cb);
}

int DDebugCallback::debug_info_cb(const void* arg, const char* fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = debug_xxx_cb(arg, fmt, cb_info, &ap);
	va_end(ap);

	return ret;
}

int DDebugCallback::debug_warn_cb(const void* arg, const char* fmt, ...) {
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = debug_xxx_cb(arg, fmt, cb_warn, &ap);
	va_end(ap);

	return ret;
}

int DDebugCallback::debug_error_cb(const void* arg, const char* fmt, ...) {
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = debug_xxx_cb(arg, fmt, cb_error, &ap);
	va_end(ap);

	return ret;
}

int DDebugCallback::debug_fatal_cb(const void* arg, const char* fmt, ...) {
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = debug_xxx_cb(arg, fmt, cb_fatal, &ap);
	va_end(ap);

	return ret;
}