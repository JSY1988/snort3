//--------------------------------------------------------------------------
// Copyright (C) 2014-2020 Cisco and/or its affiliates. All rights reserved.
// Copyright (C) 2013-2013 Sourcefire, Inc.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "messages.h"

#include <syslog.h>

#include <cassert>
#include <cstdarg>

#include "main/snort_config.h"
#include "parser/parser.h"
#include "time/packet_time.h"
#include "utils/util_cstring.h"

using namespace snort;

static int already_fatal = 0;

static unsigned parse_errors = 0;
static unsigned parse_warnings = 0;
static unsigned reload_errors = 0;

static std::string reload_errors_description;

void reset_parse_errors()
{
    parse_errors = 0;
    reload_errors = 0;
}

unsigned get_parse_errors()
{
    unsigned tmp = parse_errors;
    parse_errors = 0;
    return tmp;
}

unsigned get_parse_warnings()
{
    unsigned tmp = parse_warnings;
    parse_warnings = 0;
    return tmp;
}

void reset_reload_errors()
{
    reload_errors = 0;
    reload_errors_description.clear();
}

unsigned get_reload_errors()
{
    return reload_errors;
}

std::string& get_reload_errors_description()
{
    return reload_errors_description;
}

static void log_message(FILE* file, const char* type, const char* msg)
{
    const char* file_name;
    unsigned file_line;
    get_parse_location(file_name, file_line);

    if ( file_line )
        LogMessage(file, "%s: %s:%d %s\n", type, file_name, file_line, msg);

    else if ( file_name )
        LogMessage(file, "%s: %s: %s\n", type, file_name, msg);

    else
        LogMessage(file, "%s: %s\n", type, msg);
}

namespace snort
{
void ParseWarning(WarningGroup wg, const char* format, ...)
{
    if ( !(SnortConfig::get_conf()->warning_flags & (1 << wg)) )
        return;

    char buf[STD_BUF+1];
    va_list ap;

    va_start(ap, format);
    vsnprintf(buf, STD_BUF, format, ap);
    va_end(ap);

    buf[STD_BUF] = '\0';
    log_message(stderr, "WARNING", buf);

    parse_warnings++;
}

void ParseError(const char* format, ...)
{
    char buf[STD_BUF+1];
    va_list ap;

    va_start(ap, format);
    vsnprintf(buf, STD_BUF, format, ap);
    va_end(ap);

    buf[STD_BUF] = '\0';
    log_message(stderr, "ERROR", buf);

    parse_errors++;
}

void ReloadError(const char* format, ...)
{
    char buf[STD_BUF+1];
    va_list ap;

    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    buf[sizeof(buf)-1] = '\0';
    log_message(stderr, "ERROR", buf);

    if ( reload_errors_description.empty() )
        reload_errors_description = buf;
    else
    {
        reload_errors_description += ",";
        reload_errors_description += buf;
    }

    reload_errors++;
}

[[noreturn]] void ParseAbort(const char* format, ...)
{
    char buf[STD_BUF+1];
    va_list ap;

    va_start(ap, format);
    vsnprintf(buf, STD_BUF, format, ap);
    va_end(ap);

    buf[STD_BUF] = '\0';

    const char* file_name;
    unsigned file_line;
    get_parse_location(file_name, file_line);

    if ( file_line )
        FatalError("%s:%u %s\n", file_name, file_line, buf);
    else
        FatalError("%s\n", buf);
}

static void WriteLogMessage(FILE* fh, bool prefer_fh, const char* format, va_list& ap)
{
    if ( SnortConfig::get_conf() && !prefer_fh )
    {
        if ( SnortConfig::log_syslog() )
        {
            char buf[STD_BUF+1];
            vsnprintf(buf, STD_BUF, format, ap);
            buf[STD_BUF] = '\0';
            syslog(LOG_DAEMON | LOG_NOTICE, "%s", buf);
            return;
        }
    }
    vfprintf(fh, format, ap);
}

/*
 * Function: LogMessage(const char *, ...)
 *
 * Purpose: Print a message to stdout or with logfacility.
 *
 * Arguments: format => the formatted error string to print out
 *            ... => format commands/fillers
 *
 * Returns: void function
 */
void LogMessage(const char* format,...)
{
    if ( SnortConfig::get_conf() and SnortConfig::log_quiet() )
        return;

    va_list ap;
    va_start(ap, format);

    WriteLogMessage(stdout, false, format, ap);

    va_end(ap);
}

void LogMessage(FILE* fh, const char* format,...)
{
    if ( fh == stdout and SnortConfig::get_conf() and SnortConfig::log_quiet() )
        return;

    va_list ap;
    va_start(ap, format);

    WriteLogMessage(fh, (fh != stdout && fh != stderr), format, ap);

    va_end(ap);
}

/*
 * Function: WarningMessage(const char *, ...)
 *
 * Purpose: Print a message to stderr or with logfacility.
 *
 * Arguments: format => the formatted error string to print out
 *            ... => format commands/fillers
 *
 * Returns: void function
 */
void WarningMessage(const char* format,...)
{
    va_list ap;

    va_start(ap, format);

    if ( SnortConfig::get_conf() and SnortConfig::log_syslog() )
    {
        char buf[STD_BUF+1];
        vsnprintf(buf, STD_BUF, format, ap);
        buf[STD_BUF] = '\0';
        syslog(LOG_DAEMON | LOG_WARNING, "%s", buf);
    }
    else
    {
        vfprintf(stderr, format, ap);
    }

    va_end(ap);
}

/*
 * Function: ErrorMessage(const char *, ...)
 *
 * Purpose: Print a message to stderr.
 *
 * Arguments: format => the formatted error string to print out
 *            ... => format commands/fillers
 *
 * Returns: void function
 */
void ErrorMessage(const char* format,...)
{
    va_list ap;

    va_start(ap, format);

    if ( SnortConfig::get_conf() and SnortConfig::log_syslog() )
    {
        char buf[STD_BUF+1];
        vsnprintf(buf, STD_BUF, format, ap);
        buf[STD_BUF] = '\0';
        syslog(LOG_CONS | LOG_DAEMON | LOG_ERR, "%s", buf);
    }
    else
    {
        vfprintf(stderr, format, ap);
    }
    va_end(ap);
}

/*
 * Function: FatalError(const char *, ...)
 *
 * Purpose: When a fatal error occurs, this function prints the error message
 *          and cleanly shuts down the program
 *
 * Arguments: format => the formatted error string to print out
 *            ... => format commands/fillers
 *
 * Returns: void function
 */
[[noreturn]] void FatalError(const char* format,...)
{
    char buf[STD_BUF+1];
    va_list ap;

    // -----------------------------
    // bail now if we are reentering
    if ( already_fatal )
        exit(1);
    else
        already_fatal = 1;
    // -----------------------------

    va_start(ap, format);
    vsnprintf(buf, STD_BUF, format, ap);
    va_end(ap);

    buf[STD_BUF] = '\0';

    if ( SnortConfig::get_conf() and SnortConfig::log_syslog() )
    {
        syslog(LOG_CONS | LOG_DAEMON | LOG_ERR, "FATAL ERROR: %s", buf);
    }
    else
    {
        fprintf(stderr, "FATAL: %s", buf);
        fprintf(stderr,"Fatal Error, Quitting..\n");
    }

#if 0
    // FIXIT-M need to stop analyzers / workers
    // and they should handle the DAQ break / abort
    if ( SnortIsInitializing() )
    {
        DAQ_Abort();
        exit(1);
    }
    else
#endif
    {
        // FIXIT-M this makes no sense from main thread
        exit(EXIT_FAILURE);
    }
}

NORETURN_ASSERT void log_safec_error(const char* msg, void*, int e)
{
    static THREAD_LOCAL unsigned safec_errors = 0;

    if ( ++safec_errors < 1000 )
        ErrorMessage("SafeC error %i: %s\n", e, msg);

    assert(false);
}

bool ConfigLogger::log_flag(const char* caption, bool flag)
{
    LogMessage("%25.25s: %s\n", caption, flag ? "enabled" : "disabled");
    return flag;
}

void ConfigLogger::log_limit(const char* caption, int val, int unlim, int disable)
{
    if ( val == disable )
        LogMessage("%25.25s: %d (disabled)\n", caption, val);
    else if ( val == unlim )
        LogMessage("%25.25s: %d (unlimited)\n", caption, val);
    else
        LogMessage("%25.25s: %d\n", caption, val);
}

void ConfigLogger::log_limit(const char* caption, int val, int unlim)
{
    if ( val == unlim )
        LogMessage("%25.25s: %d (unlimited)\n", caption, val);
    else
        LogMessage("%25.25s: %d\n", caption, val);
}

void ConfigLogger::log_limit(const char* caption, int64_t val, int64_t unlim)
{
    if ( val == unlim )
        LogMessage("%25.25s: %" PRId64 " (unlimited)\n", caption, val);
    else
        LogMessage("%25.25s: %" PRId64 "\n", caption, val);
}

void ConfigLogger::log_value(const char* caption, int32_t n)
{
    LogMessage("%25.25s: %" PRId32 "\n", caption, n);
}

void ConfigLogger::log_value(const char* caption, uint32_t n)
{
    LogMessage("%25.25s: %" PRIu32 "\n", caption, n);
}

void ConfigLogger::log_value(const char* caption, int64_t n)
{
    LogMessage("%25.25s: %" PRId64 "\n", caption, n);
}

void ConfigLogger::log_value(const char* caption, uint64_t n)
{
    LogMessage("%25.25s: %" PRIu64 "\n", caption, n);
}

void ConfigLogger::log_value(const char* caption, const char* str)
{
    if ( str and str[0] )
        LogMessage("%25.25s: %s\n", caption, str);
}

void ConfigLogger::log_list(const char* caption, const char* list)
{
    std::string res;
    std::stringstream ss;
    const std::string offset(26,' ');

    size_t len = 0;
    std::string val;

    ss << list;
    while (ss >> val)
    {
        if ( len + val.length() > max_line_len )
        {
            res += '\n' + offset;
            len = 0;
        }
        res += ' ' + val;
        len += val.length() + 1;
    }

    LogMessage("%25.25s:%s\n", caption, res.c_str());
}
} //namespace snort

