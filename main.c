/*	Si4703 based RDS scanner
	Copyright (c) 2015 Andrey Chilikin (https://github.com/achilikin)
    
	This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "cmd.h"
#include "cli.h"
#include "pi2c.h"
#include "rpi_pin.h"
#include "si4703.h"

cmd_t commands[] = {
	{ "reset", "reset radio module", cmd_reset },
	{ "power", "power up|down", cmd_power },
	{ "dump", "dump registers map", cmd_dump },
	{ "spacing", "spacing 50|100|200 kHz", cmd_spacing },
	{ "scan", "scan [mode 1-5]", cmd_scan },
	{ "spectrum", "spectrum [rssi limit]", cmd_spectrum },
	{ "seek", "seek up|down", cmd_seek },
	{ "tune", "tune [freq]", cmd_tune },
	{ "volume", "volume [0-30]", cmd_volume },
	{ "rds", "rds [on|off|verbose] gt [0,...,15] [time sec (0 - no timeout)] [log]", cmd_monitor },
	{ "set", "set register value", cmd_set },
	{ NULL, NULL, NULL }
};

int stop = 0;
console_io_t cli;
int stdio_cli_handler(console_io_t *cli, void *ptr);

int is_stop(int *pstop)
{
	int stop = cli.getch(&cli);
	if (pstop) {
		if (*pstop)
			stop = *pstop;
		else
			*pstop = stop;
	}
	return stop;
}

int main(int argc, char **argv)
{
	char *arg = NULL;
	char argbuf[BUFSIZ];
	int  cmd_mode = 0, verbose = 1;
	stdio_mode(STDIO_MODE_CANON);

	if (argc == 2) {
		if (cmd_is(argv[1], "cmd")) {
			cmd_mode = 1;
			cli.prompt = '>';
		}
		if (cmd_is(argv[1], "help"))
			argc = 1;
	}

	if (argc == 1) {
		printf("Supported commands:\n");
		printf("    cmd: run in interactive command mode\n");
		for(uint32_t i = 0; commands[i].name != NULL; i++) {
			printf("    %s: %s\n", commands[i].name, commands[i].help);
		}
		return 0;
	}

	if (!cmd_mode && argc > 2 && cmd_is(argv[argc-1], "--silent")) {
		cli.prompt = '\0';
		verbose = 0;
		argc--;
	}

	if (!verbose)
		cli.ofd = open("/dev/null", O_WRONLY);
	stdio_init(&cli, stdio_cli_handler);
	stdio_mode(STDIO_MODE_RAW);

	rpi_pin_init(RPI_REV2);
	pi2c_open(PI2C_BUS);
	pi2c_select(PI2C_BUS, SI4703_ADDR);

	if (cmd_mode) {
		while(!stop)
			cli.interact(&cli, NULL);
		goto restore;
	}

	argbuf[0] = '\0';
	if (argc > 2) {
		for(int i = 2; i < argc; i++) {
			if (i > 2)
				strcat(argbuf, " ");
			strcat(argbuf, argv[i]);
		}
		arg = argbuf;
	}

	for(uint32_t i = 0; commands[i].name != NULL; i++) {
		if (cmd_is(argv[1], commands[i].name)) {
			commands[i].cmd(cli.ofd, arg);
			break;
		}
	}

restore:
	pi2c_close(PI2C_BUS);
	stdio_mode(STDIO_MODE_CANON);

	return 0;
}
