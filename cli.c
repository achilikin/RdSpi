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
#include <unistd.h>

#include "cli.h"
#include "cmd.h"

static const char *shelp =
	"general commands\n"
	"  exit|quit|q\n"
	"  cli debug on|off\n"
	"  cls: clear screen\n"
	"si4703 commands\n"
	"";

static int cli_proc(console_io_t *cli, char *arg, void *ptr);
static int cls_proc(console_io_t *cli, char *arg, void *ptr);
static int exit_proc(console_io_t *cli, char *arg, void *ptr);
static int help_proc(console_io_t *cli, char *arg, void *ptr);

static int reset_proc(console_io_t *cli, char *arg, void *ptr);
static int power_proc(console_io_t *cli, char *arg, void *ptr);
static int dump_proc(console_io_t *cli, char *arg, void *ptr);
static int spacing_proc(console_io_t *cli, char *arg, void *ptr);
static int scan_proc(console_io_t *cli, char *arg, void *ptr);
static int spectrum_proc(console_io_t *cli, char *arg, void *ptr);
static int tune_proc(console_io_t *cli, char *arg, void *ptr);
static int seek_proc(console_io_t *cli, char *arg, void *ptr);
static int volume_proc(console_io_t *cli, char *arg, void *ptr);
static int set_proc(console_io_t *cli, char *arg, void *ptr);
static int rds_proc(console_io_t *cli, char *arg, void *ptr);

typedef int (cmd_proc)(console_io_t *cli, char *arg, void *ptr);
static struct command_s {
	const char *cmd;
	cmd_proc   *proc;
} clis[] = {
	{ "cli", cli_proc },
	{ "cls", cls_proc },
	{ "help", help_proc },
	{ "exit", exit_proc },
	{ "quit", exit_proc },
	{ "q", exit_proc },
	{ "reset", reset_proc },
	{ "power", power_proc },
	{ "dump", dump_proc },
	{ "spacing", spacing_proc },
	{ "scan", scan_proc },
	{ "spectrum", spectrum_proc },
	{ "tune", tune_proc },
	{ "seek", seek_proc },
	{ "volume", volume_proc },
	{ "set", set_proc },
	{ "rds", rds_proc },
	{ NULL, NULL }
};
extern cmd_t commands[];

int stdio_cli_handler(console_io_t *cli, void *ptr)
{
	char *arg;
	cmd_line_t *cl = (cmd_line_t *)cli->data;

	for(unsigned i = 0; clis[i].proc != NULL; i++) {
		if (cmd_arg(cl->cmd, clis[i].cmd, &arg))
			return clis[i].proc(cli, arg, ptr);
	}

	return CLI_ENOTSUP;
}

int help_proc(console_io_t *cli, UNUSED(char *arg), UNUSED(void *ptr))
{
	cli->puts(cli, shelp);
	for(uint32_t i = 0; commands[i].name != NULL; i++) {
		dprintf(cli->ofd, "  %s: %s\n", commands[i].name, commands[i].help);
	}
	return CLI_EOK;
}

int exit_proc(UNUSED(console_io_t *cli), UNUSED(char *arg), UNUSED(void *ptr))
{
	stop = 1;
	return CLI_EOK;
}

int cls_proc(console_io_t *cli, UNUSED(char *arg), UNUSED(void *ptr))
{
	cli->puts(cli, clrs);
	return CLI_EOK;
}

int cli_proc(console_io_t *cli, char *arg, UNUSED(void *ptr))
{
	cmd_line_t *cl = (cmd_line_t *)cli->data;

	if (cmd_arg(arg, "debug", &arg)) {
		if (cmd_is(arg, "on"))
			cl->flags |= CLI_DEBUG;
		else if (cmd_is(arg, "off"))
			cl->flags &= ~CLI_DEBUG;
		else
			return CLI_EARG;
	}
	return CLI_EOK;
}

int reset_proc(console_io_t *cli, char *arg, UNUSED(void *ptr))
{
	return cmd_reset(cli->ofd, arg);
}

int power_proc(console_io_t *cli, char *arg, UNUSED(void *ptr))
{
	return cmd_power(cli->ofd, arg);
}

int dump_proc(console_io_t *cli, char *arg, UNUSED(void *ptr))
{
	return cmd_dump(cli->ofd, arg);
}

int spacing_proc(console_io_t *cli, char *arg, UNUSED(void *ptr))
{
	return cmd_spacing(cli->ofd, arg);
}

int scan_proc(console_io_t *cli, char *arg, UNUSED(void *ptr))
{
	return cmd_scan(cli->ofd, arg);
}

int spectrum_proc(console_io_t *cli, char *arg, UNUSED(void *ptr))
{
	return cmd_spectrum(cli->ofd, arg);
}

int tune_proc(console_io_t *cli, char *arg, UNUSED(void *ptr))
{
	return cmd_tune(cli->ofd, arg);
}

int seek_proc(console_io_t *cli, char *arg, UNUSED(void *ptr))
{
	return cmd_seek(cli->ofd, arg);
}

int volume_proc(console_io_t *cli, char *arg, UNUSED(void *ptr))
{
	return cmd_volume(cli->ofd, arg);
}

int set_proc(console_io_t *cli, char *arg, UNUSED(void *ptr))
{
	return cmd_set(cli->ofd, arg);
}

int rds_proc(console_io_t *cli, char *arg, UNUSED(void *ptr))
{
	return cmd_monitor(cli->ofd, arg);
}
