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

#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <malloc.h>
#include <termios.h>

#include "cli.h"
#include "cmd.h"

void stdio_mode(enum stdio_mode_t mode)
{
	struct termios term_attr;

	tcgetattr(STDIN_FILENO, &term_attr);
	if (mode == STDIO_MODE_RAW)
		term_attr.c_lflag &= ~(ECHO | ICANON);
	else
		term_attr.c_lflag |= (ECHO | ICANON);
	term_attr.c_cc[VMIN] = (mode == STDIO_MODE_RAW) ? 0 : 1;
	tcsetattr(STDIN_FILENO, TCSANOW, &term_attr);

	return;
}

static int stdin_getch(int fd)
{
	struct pollfd polls;
	polls.fd = fd;
	polls.events = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI;
	polls.revents = 0;

	if (poll(&polls, 1, 0) < 0)
		return -1;

	int ch = 0;
	if (polls.revents)
		read(fd, &ch, 1);

	return ch;
}

// Escape sequence states
#define ESC_CHAR    0
#define ESC_BRACKET 1
#define ESC_BRCHAR  2
#define ESC_TILDA   3
#define ESC_FUNC    4
#define ESC_CRLF    5

static int stdio_getch(console_io_t *cli)
{
	cmd_line_t *cl = (cmd_line_t *)cli->data;

	int ch = stdin_getch(cli->ifd);
	if (ch <= 0)
		return ch;

	if (cl->flags & CLI_DEBUG) {
		dprintf(cli->ofd, "['");
		if (ch < ' ')
			dprintf(cli->ofd, "%X", ch);
		else
			dprintf(cli->ofd, "%c", ch);
		dprintf(cli->ofd, "':%u]", ch);
	}

	if (ch == 127)
		return '\b';

	// ESC sequence state machine
	if (ch == 27) {
		cl->esc = ESC_BRACKET;
		return 0;
	}
	if (cl->esc == ESC_BRACKET) {
		if ((ch == '[')  || (ch == 'O')) {
			cl->esc = ESC_BRCHAR;
			return 0;
		}
	}
	if (cl->esc == ESC_BRCHAR) {
		if (ch >= 'A' && ch <= 'D') {
			ch = KEY_EXTRA | (ch - 'A' + 1);
			cl->esc = ESC_CHAR;
			goto fret;
		}
		if (ch == 'F') {
			ch = KEY_END;
			cl->esc = ESC_CHAR;
			goto fret;
		}
		if (ch == 'H') {
			ch = KEY_HOME;
			cl->esc = ESC_CHAR;
			goto fret;
		}

		if ((ch >= '1') && (ch <= '6')) {
			cl->esc = ESC_TILDA;
			cl->idx = ch - '0';
			return 0;
		}
		goto fret;
	}
	if (cl->esc == ESC_TILDA) {
		if (ch == '~') {
			ch = KEY_EXTRA | (cl->idx + 4);
			cl->esc = ESC_CHAR;
			goto fret;
		}
		if ((ch >= '0') && (ch <= '9')) {
			cl->idx <<= 4;
			cl->idx |= ch - '0';
			cl->esc = ESC_FUNC;
			return 0;
		}
		cl->esc = ESC_CHAR;
		return 0;
	}

	if (cl->esc == ESC_FUNC) {
		if (ch == '~') {
			cl->esc = ESC_CHAR;
			ch = cl->idx | KEY_EXTRA;
			goto fret;
		}
		cl->esc = ESC_CHAR;
		return 0;
	}

	// convert CR to LF 
	if (ch == '\r') {
		cl->esc = ESC_CRLF;
		return '\n';
	}
	// do not return LF if it is part of CR+LF combination
	if (ch == '\n') {
		if (cl->esc == ESC_CRLF) {
			cl->esc = ESC_CHAR;
			return 0;
		}
	}
	cl->esc = ESC_CHAR;
fret:
	if ((cl->flags & CLI_DEBUG) && (ch & KEY_EXTRA))
		dprintf(cli->ofd, "<%04X>", ch);

	return ch;
}

static int stdio_putch(console_io_t *cli, int ch)
{
	write(cli->ofd, &ch, 1);
	return 0;
}

static int stdio_puts(console_io_t *cli, const char *str)
{
	dprintf(cli->ofd, "%s", str);
	return 0;
}

static int stdio_interact(console_io_t *cli, void *ptr)
{
	uint16_t ch;

	if ((ch = cli->getch(cli)) <= 0)
		return ch;

	cmd_line_t *cl = (cmd_line_t *)cli->data;

	if (ch & KEY_EXTRA) {
		if (ch == ARROW_UP && (cl->cursor == 0)) {
			// execute last successful command
			for(cl->cursor = 0; ; cl->cursor++) {
				cl->cmd[cl->cursor] = cl->hist[cl->cursor];
				if (cl->cmd[cl->cursor] == '\0')
					break;
			}
			cli->puts(cli, cl->cmd);
		}
		return 1;
	}

	if (ch == '\n') {
		cli->putch(cli, ch);
		if (*cl->cmd) {
			int8_t ret = cli->proc(cli, ptr);
			memcpy(cl->hist, cl->cmd, CMD_LEN);
			if (ret == CLI_EARG)
				cli->puts(cli, "Invalid argument\n");
			else if (ret == CLI_ENOTSUP)
				cli->puts(cli, "Unknown command\n");
			else if (ret == CLI_ENODEV)
				cli->puts(cli, "Device error\n");
		}
		for(uint8_t i = 0; i < cl->cursor; i++)
			cl->cmd[i] = '\0';
		cl->cursor = 0;
		if (!stop && cli->prompt) {
			cli->putch(cli, cli->prompt);
			cli->putch(cli, ' ');
		}
		return 1;
	}

	// backspace processing
	if (ch == '\b') {
		if (cl->cursor) {
			cl->cursor--;
			cl->cmd[cl->cursor] = '\0';
			cli->putch(cli,'\b');
			cli->putch(cli, ' ');
			cli->putch(cli, '\b');
		}
	}

	// skip control or damaged bytes
	if (ch < ' ')
		return 0;

	// echo
	cli->putch(cli, ch);

	cl->cmd[cl->cursor++] = (uint8_t)ch;
	cl->cursor &= CMD_LEN;
	// clean up in case of overflow (command too long)
	if (!cl->cursor) {
		for(unsigned i = 0; i <= CMD_LEN; i++)
			cl->cmd[i] = '\0';
	}

	return 1;
}

void stdio_init(console_io_t *cli, cproc_t *cli_handler)
{
	cli->data = malloc(sizeof(cmd_line_t));
	memset(cli->data, 0, sizeof(cmd_line_t));
	cli->ifd = STDIN_FILENO;
	if (!cli->ofd)
		cli->ofd = STDOUT_FILENO;
	cli->getch = stdio_getch;
	cli->putch = stdio_putch;
	cli->puts  = stdio_puts;
	cli->interact = stdio_interact;
	cli->proc  = cli_handler;

	if (cli->prompt) {
		stdio_putch(cli, cli->prompt);
		stdio_putch(cli, ' ');
	}
}
