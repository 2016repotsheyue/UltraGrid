/*
 * FILE:    debug.c
 * PROGRAM: RAT
 * AUTHORS: Isidor Kouvelas
 *          Colin Perkins
 *          Mark Handley
 *          Orion Hodson
 *          Jerry Isdale
 *
 * $Revision: 1.1 $
 * $Date: 2007/11/08 09:48:59 $
 *
 * Copyright (c) 1995-2000 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Computer Science
 *      Department at University College London
 * 4. Neither the name of the University nor of the Department may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "config_unix.h"
#include "config_win32.h"
#include "compat/platform_time.h"
#include "debug.h"

#include "host.h"

#include <mutex>
#ifdef WIN32
#include <wincon.h>
#endif

static void _dprintf(const char *format, ...)
{
        if (log_level < LOG_LEVEL_DEBUG) {
                return;
        }

#ifdef WIN32
        char msg[65535];
        va_list ap;

        va_start(ap, format);
        _vsnprintf(msg, 65535, format, ap);
        va_end(ap);
        OutputDebugString(msg);
#else
        va_list ap;

        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);
#endif                          /* WIN32 */
}

void log_msg(int level, const char *format, ...)
{
        static std::mutex log_lock;
        va_list ap;
        const char *color_nix = NULL;

        if (log_level < level) {
                return;
        }

#if 0 // WIN32
        if (log_level == LOG_LEVEL_DEBUG) {
                char msg[65535];
                va_list ap;

                va_start(ap, format);
                _vsnprintf(msg, 65535, format, ap);
                va_end(ap);
                OutputDebugString(msg);
                return;
        }
#endif                          /* WIN32 */

#ifdef WIN32
        HANDLE con;
        bool no_color = true;
        CONSOLE_SCREEN_BUFFER_INFO con_info;
        uint32_t color_win = 0u;

        switch (level) {
        case LOG_LEVEL_FATAL:   color_win = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
        case LOG_LEVEL_ERROR:   color_win = FOREGROUND_RED; break;
        case LOG_LEVEL_WARNING: color_win = FOREGROUND_RED | FOREGROUND_GREEN; break;
        case LOG_LEVEL_NOTICE:  color_win = FOREGROUND_GREEN; break;
        }

#else
        switch (level) {
        case LOG_LEVEL_FATAL:   color_nix = "\033[1;31m"; break;
        case LOG_LEVEL_ERROR:   color_nix = "\033[0;31m"; break;
        case LOG_LEVEL_WARNING: color_nix = "\033[0;33m"; break;
        case LOG_LEVEL_NOTICE:  color_nix = "\033[0;32m"; break;
        }
#endif

        // format new format string
        char *format_new = (char *) alloca(strlen(format) + 7 /* col start */ + 4 /* col end */ +
                        (3 + 20 /* 64b int dec */ + 1 /* dot */ + 3 /* ms */) /* time */ + 1);
        format_new[0] = '\0';
        if (color_nix_term && color_nix) {
                strcat(format_new, color_nix);
        }
        if (log_level >= LOG_LEVEL_VERBOSE) {
                unsigned long long time_ms = time_since_epoch_in_ms();
                sprintf(format_new + strlen(format_new), "[%llu.%03llu] ", time_ms / 1000,
                                time_ms % 1000);
        }
        strcat(format_new, format);
        if (color_nix_term && color_nix) {
                strcat(format_new, "\033[0m");
        }

        // get number of required bytes
        va_start(ap, format);
        int size = vsnprintf(NULL, 0, format_new, ap);
        va_end(ap);

        // format the string
        char *buffer = (char *) alloca(size + 1);
        va_start(ap, format);
        if (vsprintf(buffer, format_new, ap) != size) {
                va_end(ap);
                return;
        }
        va_end(ap);

        // print it
        log_lock.lock();
#ifdef WIN32
        if (color_win != 0u) {
                con = GetStdHandle(STD_ERROR_HANDLE);
                no_color = con == INVALID_HANDLE_VALUE || getenv("NO_COLOR");
                if (!no_color) {
                        GetConsoleScreenBufferInfo(con, &con_info);
                        SetConsoleTextAttribute(con, ((con_info.wAttributes) & 0xF0) | color_win);
                }
        }
#endif
        int written = 0;
        do {
                ssize_t ret = write(STDERR_FILENO, buffer + written, size - written);
                if (ret <= 0) {
                        break;
                } else {
                        written += ret;
                }
        } while (written < size);
#ifdef WIN32
        if (!no_color) {
                SetConsoleTextAttribute(con, con_info.wAttributes);
        }
#endif
        log_lock.unlock();
}

/**
 * debug_dump:
 * @lp: pointer to memory region.
 * @len: length of memory region in bytes.
 *
 * Writes a dump of a memory region to stdout.  The dump contains a
 * hexadecimal and an ascii representation of the memory region.
 *
 **/
void debug_dump(void *lp, int len)
{
        char *p;
        int i, j, start;
        char Buff[81];
        char stuffBuff[10];
        char tmpBuf[10];

        _dprintf("Dump of %ld=%lx bytes\n", len, len);
        start = 0L;
        while (start < len) {
                /* start line with pointer position key */
                p = (char *)lp + start;
                sprintf(Buff, "%p: ", p);

                /* display each character as hex value */
                for (i = start, j = 0; j < 16; p++, i++, j++) {
                        if (i < len) {
                                sprintf(tmpBuf, "%X", ((int)(*p) & 0xFF));

                                if (strlen((char *)tmpBuf) < 2) {
                                        stuffBuff[0] = '0';
                                        stuffBuff[1] = tmpBuf[0];
                                        stuffBuff[2] = ' ';
                                        stuffBuff[3] = '\0';
                                } else {
                                        stuffBuff[0] = tmpBuf[0];
                                        stuffBuff[1] = tmpBuf[1];
                                        stuffBuff[2] = ' ';
                                        stuffBuff[3] = '\0';
                                }
                                strcat(Buff, stuffBuff);
                        } else
                                strcat(Buff, " ");
                        if (j == 7)     /* space between groups of 8 */
                                strcat(Buff, " ");
                }

                /* fill out incomplete lines */
                for (; j < 16; j++) {
                        strcat(Buff, "   ");
                        if (j == 7)
                                strcat(Buff, " ");
                }
                strcat(Buff, "  ");

                /* display each character as character value */
                for (i = start, j = 0, p = (char *)lp + start;
                     (i < len && j < 16); p++, i++, j++) {
                        if (((*p) >= ' ') && ((*p) <= '~'))     /* test displayable */
                                sprintf(tmpBuf, "%c", *p);
                        else
                                sprintf(tmpBuf, "%c", '.');
                        strcat(Buff, tmpBuf);
                        if (j == 7)     /* space between groups of 8 */
                                strcat(Buff, " ");
                }
                _dprintf("%s\n", Buff);
                start = i;      /* next line starting byte */
        }
}
