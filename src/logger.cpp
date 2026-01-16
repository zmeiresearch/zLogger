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
//  Includes
//==============================================================================
#include <stdio.h>
#include <stdarg.h>

#include "logger.h"
#include "logger_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "log_sink_serial.h"

//==============================================================================
//  Defines
//==============================================================================
#define CMP_NAME            "Logger"
#define LOG_MAX_WAIT        (uint32_t)100   // max time to wait for synchronization
#define LOG_BUFFER_SIZE     4096
#define LOG_MAX_LINE_SIZE   (224)

#define COLOR_NONE          "\033[0m"       // default FG color
#define COLOR_TRACE         "\033[34m"      // Blue
#define COLOR_DEBUG         "\033[37m"      // White
#define COLOR_INFO          "\033[32m"      // Green
#define COLOR_WARN          "\033[33m"      // Yellow
#define COLOR_ERROR         "\033[31m"      // Red
#define COLOR_CRIT          "\033[91m"      // Bright red
#define COLOR_TEST          "\033[36m"      // Cyan
#define COLOR_DISABLED      "\033[90m"      // Dark grey

#define LOG_USE_COLOR 1

//==============================================================================
//  Local types
//==============================================================================


//==============================================================================
//  Local data
//==============================================================================
static eLogLevel            currentLevel = eLogInfo;
#if (configSUPPORT_STATIC_ALLOCATION == 1)
static uint8_t              logBufferStorage[LOG_BUFFER_SIZE] = { 0 };
static StaticStreamBuffer_t logBufferStruct;
#endif // configSUPPORT_STATIC_ALLOCATION
static uint8_t              tmpWriteBuf[LOG_MAX_LINE_SIZE] = { 0 };
static uint8_t              tmpReadBuf[LOG_MAX_LINE_SIZE] = { 0 };
static bool                 initialized = false;

static StreamBufferHandle_t logBuffer = NULL;

// Log sinks
static const LogSink sinks[] = {
    { "Serial", LogSinkSerialInit, LogSinkSerialGetWriteSize, LogSinkSerialWrite },
};

#if defined(LOG_USE_COLOR)
static const char * colorEscapeSequences[] = {
    COLOR_TRACE,
    COLOR_DEBUG,
    COLOR_INFO,
    COLOR_WARN,
    COLOR_ERROR,
    COLOR_CRIT,
    COLOR_TEST,
    COLOR_DISABLED};
#endif // LOG_USE_COLOR

//==============================================================================
//  Local functions
//==============================================================================

static size_t getSinksSmallestWriteSize()
{
    size_t writeSize = 0xffffffff;
    for (size_t i = 0; i < ARRAY_SIZE(sinks); i++)
    {
        size_t tmp = sinks[i].GetWriteSize();
        if (tmp < writeSize)
        {
            writeSize = tmp;
        }
    }
    return writeSize;
}

static size_t sinksWrite(const uint8_t * const buffer, const size_t toSend)
{
    size_t written = toSend;

    for (size_t i = 0; i < ARRAY_SIZE(sinks); i++)
    {
        size_t tmp = sinks[i].Write(buffer, toSend);
        if (tmp != written)
        {
            Log(eLogWarn, CMP_NAME, "Failure writing to sink %s: tried to write: %d, written: %d",
                    sinks[i].Name, toSend, tmp);
        }
    }
    return written;
}


static char getLevelChar(const eLogLevel level)
{
    const char chars[eLogLevelCount+1] = {
        'T',    // []race
        'D',    // []ebug
        'I',    // []nfo
        'W',    // []arning
        'E',    // []rror
        'C',    // []ritical
        'S',    // []est
        'Y',    // []disabled
        'X'     // Unknown
    };

    if (level < eLogLevelCount)
    {
        return chars[level];
    }
    else
    {
        Log(eLogWarn, CMP_NAME, "getLevelsChar: Unknown level: %d", level);
        return chars[eLogLevelCount];
    }
}

#if defined(LOG_USE_COLOR)
const char * getColor(eLogLevel level)
{
    return colorEscapeSequences[level];
}

const char * getDefaultColor(void)
{
    return colorEscapeSequences[0];
}
#endif // LOG_USE_COLOR

//==============================================================================
//  Exported functions
//==============================================================================
eStatus Log(const eLogLevel level, const char * const component, const char * const function, ...)
{
    eStatus retVal = eOK;

    if (!initialized)
    {
        retVal = eNOTINITIALIZED;
    }
    else if (LogPortInISR())
    {
        // not a good idea to call prinf and friends inside an ISR
        retVal = eUNSUPPORTED;
    }
    else if (level >= eLogLevelCount)
    {
        retVal = eINVALIDARG;
    }

    if (eOK == retVal)
    {
        if (level >= currentLevel)
        {
            int i;
            int writePtr = 0;

            if (LogPortLock(LOG_MAX_WAIT))
            {
                // first print the time
#if defined(LOG_USE_COLOR)
                i = snprintf((char *)&tmpWriteBuf[writePtr], LOG_MAX_LINE_SIZE,
                        "%s%s|%s|%c|%s|%s:", getColor(level), LogPortTimeGetString(), LogPortGetTime(), getLevelChar(level), component, function);
#else
                i = snprintf((char *)&tmpWriteBuf[writePtr], LOG_MAX_LINE_SIZE,
                        "%s|%s|%c|%s|%s:", LogPortTimeGetString(), LogPortGetTime(), getLevelChar(level), component, function);
#endif  // LOG_USE_COLOR

                if (i < 0)
                {
                    retVal = eFAILED;
                }
                else
                {
                    writePtr += i;
                    // if that went well, append the actual message as well
                    va_list args;
                    va_start(args, function);
                    const char * fmt = va_arg(args, char *);

                    int i = vsnprintf((char *)&tmpWriteBuf[writePtr], LOG_MAX_LINE_SIZE - writePtr, fmt, args);
                    va_end(args);

                    if (i < 0)
                    {
                        retVal = eFAILED;
                    }
                    else
                    {
                        writePtr += i;
                        // add a newline
#if defined(LOG_USE_COLOR)
                        i = snprintf((char *)&tmpWriteBuf[writePtr], LOG_MAX_LINE_SIZE - writePtr, "%s\r\n", getDefaultColor());
#else
                        i = snprintf((char *)&tmpWriteBuf[writePtr], LOG_MAX_LINE_SIZE - writePtr, "\r\n");
#endif  // LOG_USE_COLOR
                        if (i < 0)
                        {
                            retVal = eFAILED;
                        }
                        else
                        {
                            writePtr += i;
                        }
                    }
                }

                xStreamBufferSend(logBuffer, tmpWriteBuf, writePtr, 1);

                LogPortUnlock();
            }
            else
            {
                retVal = eBUSY;
            }
        }
    }

    return retVal;
}

eStatus LogSetLevel(const eLogLevel level)
{
    eStatus retVal = eINVALIDARG;

    if (level < eLogLevelCount)
    {
        currentLevel = level;
        retVal = eOK;
    }
    else
    {
        Log(eLogWarn, CMP_NAME, "Invalid log level: %d", level);
    }

    return retVal;
}

#define DUMP_BYTES_PER_LINE 16
eStatus LogDumpBuffer(const eLogLevel level, const char * const component, const char * const function, const uint8_t * const buffer, size_t buffer_size)
{
    eStatus retVal = eOK;

    char printBuf[DUMP_BYTES_PER_LINE * 3];  // 2 characters + separator/terminator
    size_t processed = 0;


    while ((eOK == retVal) && (processed < buffer_size))
    {
        size_t toPrint = buffer_size - processed;
        if (toPrint > DUMP_BYTES_PER_LINE)
        {
            toPrint = DUMP_BYTES_PER_LINE;
        }

        // print the first byte
        sprintf(&(printBuf[0]), "%02X", buffer[processed]);

        // Print the rest
        for (size_t i = 1; i < toPrint; i++)
        {
            // overwrite the null-termination of the previous sprintf
            sprintf(&(printBuf[((i - 1)*3) + 2]), " %02X", buffer[processed + i]);
        }

        retVal = Log(level, component, function, "%s", printBuf);
        processed += toPrint;
    }
    return retVal;
}

eStatus LogInit(void * params)
{
    eStatus retVal = eOK;
    bool oneSinkOk = false;

    (void)params;

    currentLevel = LOG_LEVEL_DEFAULT;   // default log level

    retVal = LogPortInit();

    if (eOK == retVal)
    {
        for (size_t i = 0; i < ARRAY_SIZE(sinks); i++)
        {
            retVal = sinks[i].Init();
            if (eOK == retVal)
            {
                oneSinkOk = true;
            }
            else
            {
                Log(eLogWarn, CMP_NAME, "LogInit: Error initializing %s sink", sinks[i].Name);
            }
        }

        retVal = (oneSinkOk) ? eOK : eFAILED;
    }

#if (configSUPPORT_STATIC_ALLOCATION == 1)
    logBuffer = xStreamBufferCreateStatic(LOG_BUFFER_SIZE, 1, logBufferStorage, &logBufferStruct);
#else
    logBuffer = xStreamBufferCreate(LOG_BUFFER_SIZE, 1);
#endif

    if (NULL != logBuffer)
    {
        initialized = true;
    }
    else
    {
        Log(eLogWarn, CMP_NAME, "LogInit: Error creating stream buffer!");
        retVal = eFAILED;
    }

    return retVal;
}

eStatus LogTask(void)
{
    size_t toSend = getSinksSmallestWriteSize();
    if (toSend > 0)
    {
        size_t toReceive = MIN(toSend, LOG_MAX_LINE_SIZE);
        size_t received = xStreamBufferReceive(logBuffer, tmpReadBuf, toReceive, portMAX_DELAY);
        sinksWrite(tmpReadBuf, received);
    }

    return eOK; // Always running
}
