/*  BSD License
    Copyright (c) 2015 Andrey Chilikin https://github.com/achilikin
    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright notice,
	this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
	BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef CONSOLE_IO_H
#define CONSOLE_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#if 0 // to trick VisualAssist
}
#endif
#endif

struct console_io_s;

#define CLI_EOK      0 // success
#define CLI_EARG    -1 // invalid argument
#define CLI_ENOTSUP -2 // command not supported
#define CLI_ENODEV  -3 // device communication error

typedef int (cgetch_t)(struct console_io_s *cio);
typedef int (cputch_t)(struct console_io_s *cio, int ch);
typedef int (cputs_t)(struct console_io_s *cio, const char *str);

// command line processing, returns CLI_E*
typedef int (cproc_t)(struct console_io_s *cio, void *ptr);

struct console_io_s
{
	int  ifd; // input file descriptor
	int  ofd; // output file descriptor
	char prompt;
	void *data;

	cgetch_t *getch;
	cputch_t *putch;
	cputs_t  *puts;
	cproc_t  *interact;
	cproc_t  *proc;
};

typedef struct console_io_s console_io_t;

#define CMD_LEN   0x7F
#define CLI_DEBUG 0x8000

typedef struct cmd_line_s
{
	uint16_t flags;
	uint16_t cursor;
	uint16_t esc;
	uint16_t idx;
	char cmd[CMD_LEN + 1];
	char hist[CMD_LEN + 1];
} cmd_line_t;

// non-character flag
#define KEY_EXTRA   0x8000

#define ARROW_UP    0x8001
#define ARROW_DOWN  0x8002
#define ARROW_RIGHT 0x8003
#define ARROW_LEFT  0x8004

#define KEY_HOME    0x8005
#define KEY_INS     0x8006
#define KEY_DEL     0x8007
#define KEY_END     0x8008
#define KEY_PGUP    0x8009
#define KEY_PGDN    0x800A

#define KEY_F1      0x8011
#define KEY_F2      0x8012
#define KEY_F3      0x8013
#define KEY_F4      0x8014
#define KEY_F5      0x8015
#define KEY_F6      0x8017
#define KEY_F7      0x8018
#define KEY_F8      0x8019
#define KEY_F9      0x8020
#define KEY_F10     0x8021
#define KEY_F11     0x8023
#define KEY_F12     0x8024

// Terminal ESC codes
static const char clrs[]  = { 27, '[', '2', 'J', '\0' }; // clear screen
static const char cdwn[]  = { 27, '[', 'J', '\0' };      // clear down
static const char csol[]  = { 27, '[', '1', 'K', '\0' }; // clear to Start OL
static const char ceol[]  = { 27, '[', '0', 'K', '\0' }; // clear to EOL
static const char scur[]  = { 27, '[', 's', '\0' };      // save cursor
static const char rcur[]  = { 27, '[', 'u', '\0' };      // restore cursor
static const char gotop[] = { 27, '[', '1', ';', '1', 'H','\0' }; // top left
static const char civis[] = { 27, '[', '?', '2', '5', 'l','\0' }; // invisible cursor

enum stdio_mode_t { STDIO_MODE_CANON = 0, STDIO_MODE_RAW };

void stdio_mode(enum stdio_mode_t mode);
void stdio_init(console_io_t *cli, cproc_t *cli_handler);

#ifdef __cplusplus
}
#endif

#endif
