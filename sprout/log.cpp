/**
 * @file log.cpp
 *
 * Copyright (C) 2013  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by post at
 * Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 */


#include "log.h"
#include <stdarg.h>
#include <stdio.h>

#define LL_ERROR_STRING "Error"
#define LL_WARNING_STRING "Warning"
#define LL_STATUS_STRING "Status"
#define LL_INFO_STRING "Info"
#define LL_VERBOSE_STRING "Verbose"
#define LL_DEBUG_STRING "Debug"

#define MAX_LOGLINE 8192

namespace Log
{
  static Logger *logger = new Logger();
  static int loggingLevel = 4;
};

void
Log::setLoggingLevel(int level)
{
  Log::loggingLevel = level;
}

void
Log::setLogger(Logger *log)
{
  Log::logger = log;
  if (Log::logger != NULL)
  {
    Log::logger->set_flags(Logger::FLUSH_ON_WRITE|Logger::ADD_TIMESTAMPS);
  }
}

void
Log::write(int level, const char *module, const char *fmt, ...)
{
  if (!Log::logger) {
    return;
  }

  if (level > Log::loggingLevel) {
    return;
  }

  va_list args;
  char logline[MAX_LOGLINE];
  char* logLevel = NULL;

  switch (level) {
    case 0: logLevel = LL_ERROR_STRING; break;
    case 1: logLevel = LL_WARNING_STRING; break;
    case 2: logLevel = LL_STATUS_STRING; break;
    case 3: logLevel = LL_INFO_STRING; break;
    case 4: logLevel = LL_VERBOSE_STRING; break;
    default: logLevel = LL_DEBUG_STRING; break;
  }

  int written = snprintf(logline, MAX_LOGLINE - 2, "%s %s: ", logLevel, module);

  va_start(args, fmt);
  written += vsnprintf(logline + written, MAX_LOGLINE - written - 2, fmt, args);
  va_end(args);

  // Add a new line and null termination.
  logline[written] = '\n';
  logline[written+1] = '\0';

  Log::logger->write(logline);
}

// LCOV_EXCL_START Only used in exceptional signal handlers - not hit in UT

void
Log::backtrace(const char *fmt, ...)
{
  if (!Log::logger) {
    return;
  }

  va_list args;
  char logline[MAX_LOGLINE];
  va_start(args, fmt);
  int written = vsnprintf(logline, MAX_LOGLINE, fmt, args);
  va_end(args);

  // Add a new line and null termination.
  logline[written] = '\n';
  logline[written+1] = '\0';

  Log::logger->backtrace(logline);
}

// LCOV_EXCL_STOP
