/*	Si4703 based RDS scanner
	Copyright (c) 2014 Andrey Chilikin (https://github.com/achilikin)
    
	This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "pi2c.h"
#include "rpi_pin.h"
#include "si4703.h"

cmd_t commands[] = {
	{ "-q", "quiet (no output) flag: effects all commands", 0L },
	{ "reset", "reset radio module", cmd_reset },
	{ "power", "power up|down", cmd_power },
	{ "dump", "dump registers map", cmd_dump },
	{ "spacing", "spacing 50|100|200 kHz", cmd_spacing },
	{ "scan", "scan [mode 1-5]", cmd_scan },
	{ "spectrum", "spectrum [rssi limit]", cmd_spectrum },
	{ "seek", "seek up|down", cmd_seek },
	{ "tune", "tune [freq]", cmd_tune },
	{ "volume", "volume [0-15]", cmd_volume },
	{ "rds", "rds [silently|on|off|verbose] gt [0,...,15] [time sec] [log]", cmd_monitor },
	{ "set", "set register value", cmd_set },
};

static const uint32_t ncmd = sizeof(commands)/sizeof(commands[0]);

int main(int argc, char **argv)
{
	char argbuf[BUFSIZ];
	uint16_t si_regs[16];
	memset(si_regs, 0, sizeof(si_regs));

	if (argc == 1) {
		printf("Supported commands:\n");
		for(uint32_t i = 0; i < ncmd; i++) {
			printf("    %s: %s\n", commands[i].name, commands[i].help);
		}
		printf("Channel scan spacing of 50, 100 or 200 kHz is possible with the spacing command\n");
                printf("Band selection for Japan, Japan wideband, or Europe/U.S./Asia is set with BAND[1:0]\n");
                printf("Seek tuning searches for a channel with RSSI greater than or equal to the value in SEEKTH (set SEEKTH=255)\n");
                printf("  Optional SNR and/or impulse noise detector criteria may be used to qualify valid stations (set SKSNR=0..15)\n");
		printf("RDS command details:\n");
		printf("  on|off: RDSPRF is a si4703 mode to enhance RDS message processing at the expense of FM scanning\n");
		printf("  verbose: si4703 increases visibility to RDS block-error levels and synchronization status\n");
		printf("  silently: don't print running data to stdout\n");
		return 0;
	}

	rpi_pin_init(RPI_REV2);
	pi2c_open(PI2C_BUS);
	pi2c_select(PI2C_BUS, SI4703_ADDR);

	argbuf[0] = '\0';
	char *arg = NULL;
        if (strcmp(argv[1],"-q")==0) {
            // special handling for this flag.  Kill stdout.
            stdout = fopen("/dev/null","w");
            argc--; argv++;
            }
	if (argc > 2) {
		for(int i = 2; i < argc; i++) {
			if (i > 2)
				strcat(argbuf, " ");
			strcat(argbuf, argv[i]);
		}
		arg = argbuf;
	}

	for(uint32_t i = 0; i < ncmd; i++) {
		if (cmd_is(argv[1], commands[i].name)) {
			if (commands[i].cmd) return commands[i].cmd(arg);
                }
	}

	pi2c_close(PI2C_BUS);
	return 0;
}
