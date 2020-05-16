#include "xgpu.h"
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

NS_GPU_BEGIN

int gpu_default_print(LogLevelEnum log_level, const char* format, va_list args);

static DebugLevelEnum _gpu_debug_level = DEBUG_LEVEL_0;

#define DEFAULT_MAX_NUM_ERRORS 20
#define ERROR_FUNCTION_STRING_MAX 128
#define ERROR_DETAILS_STRING_MAX 512
static ErrorObject* _gpu_error_code_queue = NULL;
static unsigned int _gpu_num_error_codes = 0;
static unsigned int _gpu_error_code_queue_size = DEFAULT_MAX_NUM_ERRORS;
static ErrorObject _gpu_error_code_result;

static int (*_gpu_print)(LogLevelEnum log_level, const char* format, va_list args) = &gpu_default_print;

int gpu_default_print(LogLevelEnum log_level, const char* format, va_list args)
{
	switch (log_level)
	{
#ifdef __ANDROID__
	case LOG_INFO:
		return __android_log_vprint((GetDebugLevel() >= DEBUG_LEVEL_3 ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO), "APPLICATION", format, args);
	case LOG_WARNING:
		return __android_log_vprint((GetDebugLevel() >= DEBUG_LEVEL_2 ? ANDROID_LOG_ERROR : ANDROID_LOG_WARN), "APPLICATION", format, args);
	case LOG_ERROR:
		return __android_log_vprint(ANDROID_LOG_ERROR, "APPLICATION", format, args);
#else
	case LOG_INFO:
		return vfprintf((GetDebugLevel() >= DEBUG_LEVEL_3 ? stderr : stdout), format, args);
	case LOG_WARNING:
		return vfprintf((GetDebugLevel() >= DEBUG_LEVEL_2 ? stderr : stdout), format, args);
	case LOG_ERROR:
		return vfprintf(stderr, format, args);
#endif
	default:
		return 0;
	}
}

void SetLogCallback(int(*callback)(LogLevelEnum log_level, const char* format, va_list args))
{
	if (callback == NULL)
		_gpu_print = &gpu_default_print;
	else
		_gpu_print = callback;
}

void LogInfo(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	_gpu_print(LOG_INFO, format, args);
	va_end(args);
}

void LogWarning(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	_gpu_print(LOG_WARNING, format, args);
	va_end(args);
}

void LogError(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	_gpu_print(LOG_ERROR, format, args);
	va_end(args);
}


void SetDebugLevel(DebugLevelEnum level)
{
	if (level > DEBUG_LEVEL_MAX)
		level = DEBUG_LEVEL_MAX;
	_gpu_debug_level = level;
}

DebugLevelEnum GetDebugLevel(void)
{
	return _gpu_debug_level;
}

void gpu_init_error_queue(void)
{
	if (_gpu_error_code_queue == NULL)
	{
		unsigned int i;
		_gpu_error_code_queue = (ErrorObject*)SDL_malloc(sizeof(ErrorObject)*_gpu_error_code_queue_size);

		for (i = 0; i < _gpu_error_code_queue_size; i++)
		{
			_gpu_error_code_queue[i].function = (char*)SDL_malloc(ERROR_FUNCTION_STRING_MAX + 1);
			_gpu_error_code_queue[i].error = ERROR_NONE;
			_gpu_error_code_queue[i].details = (char*)SDL_malloc(ERROR_DETAILS_STRING_MAX + 1);
		}
		_gpu_num_error_codes = 0;

		_gpu_error_code_result.function = (char*)SDL_malloc(ERROR_FUNCTION_STRING_MAX + 1);
		_gpu_error_code_result.error = ERROR_NONE;
		_gpu_error_code_result.details = (char*)SDL_malloc(ERROR_DETAILS_STRING_MAX + 1);
	}
}

void PushErrorCode(const char* function, ErrorEnum error, const char* details, ...)
{
	gpu_init_error_queue();

	if (GetDebugLevel() >= DEBUG_LEVEL_1)
	{
		// Print the message
		if (details != NULL)
		{
			char buf[ERROR_DETAILS_STRING_MAX];
			va_list lst;
			va_start(lst, details);
			vsnprintf(buf, ERROR_DETAILS_STRING_MAX, details, lst);
			va_end(lst);

			LogError("%s: %s - %s\n", (function == NULL ? "NULL" : function), GetErrorString(error), buf);
		}
		else
			LogError("%s: %s\n", (function == NULL ? "NULL" : function), GetErrorString(error));
	}

	if (_gpu_num_error_codes < _gpu_error_code_queue_size)
	{
		if (function == NULL)
			_gpu_error_code_queue[_gpu_num_error_codes].function[0] = '\0';
		else
		{
			strncpy(_gpu_error_code_queue[_gpu_num_error_codes].function, function, ERROR_FUNCTION_STRING_MAX);
			_gpu_error_code_queue[_gpu_num_error_codes].function[ERROR_FUNCTION_STRING_MAX] = '\0';
		}
		_gpu_error_code_queue[_gpu_num_error_codes].error = error;
		if (details == NULL)
			_gpu_error_code_queue[_gpu_num_error_codes].details[0] = '\0';
		else
		{
			va_list lst;
			va_start(lst, details);
			vsnprintf(_gpu_error_code_queue[_gpu_num_error_codes].details, ERROR_DETAILS_STRING_MAX, details, lst);
			va_end(lst);
		}
		_gpu_num_error_codes++;
	}
}

ErrorObject PopErrorCode(void)
{
	unsigned int i;
	ErrorObject result = { NULL, ERROR_NONE, NULL };

	gpu_init_error_queue();

	if (_gpu_num_error_codes <= 0)
		return result;

	// Pop the oldest
	strcpy(_gpu_error_code_result.function, _gpu_error_code_queue[0].function);
	_gpu_error_code_result.error = _gpu_error_code_queue[0].error;
	strcpy(_gpu_error_code_result.details, _gpu_error_code_queue[0].details);

	// We'll be returning that one
	result = _gpu_error_code_result;

	// Move the rest down
	_gpu_num_error_codes--;
	for (i = 0; i < _gpu_num_error_codes; i++)
	{
		strcpy(_gpu_error_code_queue[i].function, _gpu_error_code_queue[i + 1].function);
		_gpu_error_code_queue[i].error = _gpu_error_code_queue[i + 1].error;
		strcpy(_gpu_error_code_queue[i].details, _gpu_error_code_queue[i + 1].details);
	}
	return result;
}

const char* GetErrorString(ErrorEnum error)
{
	switch (error)
	{
	case ERROR_NONE:
		return "NO ERROR";
	case ERROR_BACKEND_ERROR:
		return "BACKEND ERROR";
	case ERROR_DATA_ERROR:
		return "DATA ERROR";
	case ERROR_USER_ERROR:
		return "USER ERROR";
	case ERROR_UNSUPPORTED_FUNCTION:
		return "UNSUPPORTED FUNCTION";
	case ERROR_NULL_ARGUMENT:
		return "NULL ARGUMENT";
	case ERROR_FILE_NOT_FOUND:
		return "FILE NOT FOUND";
	}
	return "UNKNOWN ERROR";
}

void gpu_free_error_queue(void)
{
	if (_gpu_num_error_codes > 0 && GetDebugLevel() >= DEBUG_LEVEL_1)
		LogError("Quit: %d uncleared error%s.\n", _gpu_num_error_codes, (_gpu_num_error_codes > 1 ? "s" : ""));
	 
	// Free the error queue
	for (unsigned int i = 0; i < _gpu_error_code_queue_size; i++)
	{
		SDL_free(_gpu_error_code_queue[i].function);
		_gpu_error_code_queue[i].function = NULL;
		SDL_free(_gpu_error_code_queue[i].details);
		_gpu_error_code_queue[i].details = NULL;
	}
	SDL_free(_gpu_error_code_queue);
	_gpu_error_code_queue = NULL;
	_gpu_num_error_codes = 0;

	SDL_free(_gpu_error_code_result.function);
	_gpu_error_code_result.function = NULL;
	SDL_free(_gpu_error_code_result.details);
	_gpu_error_code_result.details = NULL;
}

// Deletes all existing errors
void SetErrorQueueMax(unsigned int max)
{
	gpu_free_error_queue();

	// Reallocate with new size
	_gpu_error_code_queue_size = max;
	gpu_init_error_queue();
}

NS_GPU_END
