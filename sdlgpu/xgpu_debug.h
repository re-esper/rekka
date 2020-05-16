#ifndef _XGPU_DEBUG_H
#define _XGPU_DEBUG_H

#include "xgpu_define.h"

NS_GPU_BEGIN

typedef enum {
	ERROR_NONE = 0,
	ERROR_BACKEND_ERROR = 1,
	ERROR_DATA_ERROR = 2,
	ERROR_USER_ERROR = 3,
	ERROR_UNSUPPORTED_FUNCTION = 4,
	ERROR_NULL_ARGUMENT = 5,
	ERROR_FILE_NOT_FOUND = 6
} ErrorEnum;

typedef struct ErrorObject
{
	char* function;
	ErrorEnum error;
	char* details;
} ErrorObject;

typedef enum {
	DEBUG_LEVEL_0 = 0,
	DEBUG_LEVEL_1 = 1,
	DEBUG_LEVEL_2 = 2,
	DEBUG_LEVEL_3 = 3,
	DEBUG_LEVEL_MAX = 3
} DebugLevelEnum;

typedef enum {
	LOG_INFO = 0,
	LOG_WARNING,
	LOG_ERROR
} LogLevelEnum;


// Debugging, logging, and error handling

#define Log LogInfo

/* Sets the global debug level.
* DEBUG_LEVEL_0: Normal
* DEBUG_LEVEL_1: Prints messages when errors are pushed via PushErrorCode()
* DEBUG_LEVEL_2: Elevates warning logs to error priority
* DEBUG_LEVEL_3: Elevates info logs to error priority
*/
void SetDebugLevel(DebugLevelEnum level);

/* Returns the current global debug level. */
DebugLevelEnum GetDebugLevel(void);

/* Prints an informational log message. */
void LogInfo(const char* format, ...);

/* Prints a warning log message. */
void LogWarning(const char* format, ...);

/* Prints an error log message. */
void LogError(const char* format, ...);

/* Sets a custom callback for handling logging.  Use stdio's vsnprintf() to process the va_list into a string.  Passing NULL as the callback will reset to the default internal logging. */
void SetLogCallback(int(*callback)(LogLevelEnum log_level, const char* format, va_list args));

/* Pushes a new error code into the error queue.  If the queue is full, the queue is not modified.
* \param function The name of the function that pushed the error
* \param error The error code to push on the error queue
* \param details Additional information string, can be NULL.
*/
void PushErrorCode(const char* function, ErrorEnum error, const char* details, ...);

/* Pops an error object from the error queue and returns it.  If the error queue is empty, it returns an error object with NULL function, ERROR_NONE error, and NULL details. */
ErrorObject PopErrorCode(void);

/* Gets the string representation of an error code. */
const char* GetErrorString(ErrorEnum error);

/* Changes the maximum number of error objects that SDL_gpu will store.  This deletes all currently stored errors. */
void SetErrorQueueMax(unsigned int max);

NS_GPU_END

#endif
