#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "config.h"

#define LOG_LEVEL_NONE   0
#define LOG_LEVEL_ERROR  1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_INFO   3
#define LOG_LEVEL_DEBUG  4

#ifndef CURRENT_LOG_LEVEL
#define CURRENT_LOG_LEVEL LOG_LEVEL_INFO
#endif

#define LOG_ERROR(msg) do { if (CURRENT_LOG_LEVEL >= LOG_LEVEL_ERROR) Serial.println(msg); } while (0)
#define LOG_WARN(msg)  do { if (CURRENT_LOG_LEVEL >= LOG_LEVEL_WARN)  Serial.println(msg); } while (0)
#define LOG_INFO(msg)  do { if (CURRENT_LOG_LEVEL >= LOG_LEVEL_INFO)  Serial.println(msg); } while (0)
#define LOG_DEBUG(msg) do { if (CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG) Serial.println(msg); } while (0)

#define LOG_ERRORF(label, value) do { if (CURRENT_LOG_LEVEL >= LOG_LEVEL_ERROR) { Serial.print(label); Serial.println(value); } } while (0)
#define LOG_WARNF(label, value)  do { if (CURRENT_LOG_LEVEL >= LOG_LEVEL_WARN)  { Serial.print(label); Serial.println(value); } } while (0)
#define LOG_INFOF(label, value)  do { if (CURRENT_LOG_LEVEL >= LOG_LEVEL_INFO)  { Serial.print(label); Serial.println(value); } } while (0)
#define LOG_DEBUGF(label, value) do { if (CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG) { Serial.print(label); Serial.println(value); } } while (0)

#endif