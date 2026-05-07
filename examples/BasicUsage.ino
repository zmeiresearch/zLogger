/*==============================================================================
   zLogger Example - Basic Usage

   This example demonstrates how to use the zLogger library with basic
   logging functionality and optional custom time strings.
  ============================================================================*/

#include <Arduino.h>
#include <logger.h>

//==============================================================================
// Configuration
//==============================================================================
#define CMP_NAME "BasicExample"

// Uncomment to enable custom time string
// #define USE_CUSTOM_TIME

// Uncomment to demonstrate custom sink registration
// #define USE_CUSTOM_SINK

//==============================================================================
// Optional: Override LogPortTimeGetString for custom time formatting
//==============================================================================
#ifdef USE_CUSTOM_TIME
const char * LogPortTimeGetString() {
    static char timeBuffer[32];
    unsigned long seconds = millis() / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;

    snprintf(timeBuffer, sizeof(timeBuffer), "%02lu:%02lu:%02lu",
             hours % 24, minutes % 60, seconds % 60);
    return timeBuffer;
}
#endif

#ifdef USE_CUSTOM_SINK
static eStatus ExampleSinkInit(void * context) {
    (void)context;
    return eOK;
}

static size_t ExampleSinkGetWriteSize(void * context) {
    (void)context;
    return 64;
}

static size_t ExampleSinkWrite(void * context, const uint8_t * buffer, size_t toSend) {
    (void)context;
    (void)buffer;
    return toSend;
}

static const LogSink ExampleSink = {
    "Example",
    ExampleSinkInit,
    ExampleSinkGetWriteSize,
    ExampleSinkWrite,
    NULL
};

static const LogSink ExampleSinks[] = {
    ExampleSink,
};

static LogInitParams ExampleInitParams = {
    ExampleSinks,
    sizeof(ExampleSinks) / sizeof(ExampleSinks[0])
};
#endif

//==============================================================================
// FreeRTOS Logger Task
//==============================================================================
void loggerTask(void *pvParameters) {
    (void)pvParameters;
    while(1) {
        LogTask();  // Process log buffer and write to sinks
    }
}

//==============================================================================
// Demo Task - Generates log messages at different levels
//==============================================================================
void demoTask(void *pvParameters) {
    (void)pvParameters;
    int counter = 0;

    while(1) {
        LOG(eLogTrace, "Trace message %d", counter);
        LOG(eLogDebug, "Debug message %d", counter);
        LOG(eLogInfo, "Info message %d", counter);
        LOG(eLogWarn, "Warning message %d", counter);
        LOG(eLogError, "Error message %d", counter);

        // Demonstrate buffer dump
        if (counter % 5 == 0) {
            uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0xAA, 0xBB, 0xCC};
            LOG_DUMP_BUFFER(eLogDebug, data, sizeof(data));
        }

        counter++;
        vTaskDelay(pdMS_TO_TICKS(5000));  // Log every 5 seconds
    }
}

//==============================================================================
// Setup
//==============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialize the logger
#ifdef USE_CUSTOM_SINK
    eStatus status = LogInit(&ExampleInitParams);
#else
    eStatus status = LogInit(NULL);
#endif
    if (status != eOK) {
        Serial.println("Failed to initialize logger!");
        return;
    }

    // Set log level (optional, defaults to eLogInfo)
    LogSetLevel(eLogTrace);  // Show all log levels

    LOG(eLogInfo, "Logger initialized successfully");
    LOG(eLogInfo, "Starting demo tasks");

    // Create logger task
    xTaskCreate(
        loggerTask,
        "Logger",
        2048,
        NULL,
        1,          // Priority
        NULL
    );

    // Create demo task
    xTaskCreate(
        demoTask,
        "Demo",
        2048,
        NULL,
        2,          // Higher priority than logger
        NULL
    );

    LOG(eLogInfo, "Tasks created, entering main loop");
}

//==============================================================================
// Loop - Not used with FreeRTOS tasks
//==============================================================================
void loop() {
    // With FreeRTOS, tasks handle everything
    vTaskDelay(portMAX_DELAY);
}
