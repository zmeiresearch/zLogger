/*==============================================================================
   zLogger - Flexible logging library for ESP32

   Copyright 2020-2026 Ivan Vasilev, Zmei Research Ltd.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
  ============================================================================*/

//==============================================================================
//  Multi-include & c++ guards
//==============================================================================
#ifndef INC_LOGGER_H
#define INC_LOGGER_H

#ifdef __cplusplus
extern "C"
{
#endif

//==============================================================================
//  Includes
//==============================================================================
#include <globals.h>


//==============================================================================
//  Defines
//==============================================================================
#if !defined(LOG_LEVEL_DEFAULT)
#if defined(DEBUG)
#define LOG_LEVEL_DEFAULT           eLogInfo
#else // !DEBUG
#define LOG_LEVEL_DEFAULT           eLogInfo
#endif // DEBUG
#endif // LOG_LEVEL_DEFAULT

#define LOG(level, ...)             Log((level), CMP_NAME, __func__, __VA_ARGS__)
#define LOG_DUMP_BUFFER(level, ...) LogDumpBuffer((level), CMP_NAME, __func__, __VA_ARGS__)

//==============================================================================
//  Exported types
//==============================================================================
typedef enum _eLogLevel
{
    eLogTrace,
    eLogDebug,
    eLogInfo,
    eLogWarn,
    eLogError,
    eLogCrit,
    eLogTest,
    eLogDisabled,
    eLogLevelCount,
} eLogLevel;

// Function pointers to different log sinks. Would've been cleaner with an
// interface, but I want to keep it as C as possible
typedef eStatus (*LogSinkInitFn)(void);
typedef size_t  (*LogSinkGetWriteSizeFn)(void);
typedef size_t  (*LogSinkWriteFn)(const uint8_t * const buffer, const size_t toSend);

typedef struct _LogSink
{
    const char*             Name;
    LogSinkInitFn           Init;
    LogSinkGetWriteSizeFn   GetWriteSize;
    LogSinkWriteFn          Write;
} LogSink;

//==============================================================================
//  Exported data
//==============================================================================

//==============================================================================
//  Exported functions
//==============================================================================
eStatus Log(const eLogLevel level, const char * const component, const char * const function, ...);
eStatus LogSetLevel(const eLogLevel level);
eStatus LogDumpBuffer(const eLogLevel level, const char * const component, const char * const function, const uint8_t * const buffer, size_t buffer_size);

//==============================================================================
//  Module generic interface
//==============================================================================
eStatus LogInit(void * params);
eStatus LogTask(void);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // INC_LOGGER_H
