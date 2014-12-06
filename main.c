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
	{ "reset", cmd_reset },
	{ "power", cmd_power },
	{ "dump", cmd_dump },
	{ "spacing", cmd_spacing },
	{ "scan", cmd_scan },
	{ "spectrum", cmd_spectrum },
	{ "seek", cmd_seek },
	{ "tune", cmd_tune },
	{ "volume", cmd_volume },
	{ "rds", cmd_rds },
	{ "set", cmd_set },
};

static const uint32_t ncmd = sizeof(commands)/sizeof(commands[0]);

int main(int argc, char **argv)
{
	uint16_t si_regs[16];
	memset(si_regs, 0, sizeof(si_regs));

	if (argc == 1) {
		printf("Supported commands:\n");
		for(uint32_t i = 0; i < ncmd; i++) {
			printf("    %s\n", commands[i].name);
		}
		return 0;
	}

	rpi_pin_init(RPI_REV2);
	pi2c_open(PI2C_BUS);
	pi2c_select(PI2C_BUS, SI4703_ADDR);

	const char *arg = NULL;
	if (argc > 2)
		arg = argv[2];

	for(uint32_t i = 0; i < ncmd; i++) {
		if (str_is(argv[1], commands[i].name))
			return commands[i].cmd(arg);
	}

	pi2c_close(PI2C_BUS);
	return 0;
}
