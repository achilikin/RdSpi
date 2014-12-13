/*	CLI commands hadlers for Si4703 based RDS scanner
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

#ifndef __SI4703_CMD_H__
#define __SI4703_CMD_H__

#include <string.h>

#ifdef __cplusplus
extern "C" {
#if 0 // dummy bracket for VAssistX
}
#endif
#endif

typedef int (*cmd_handler)(char *arg);

typedef struct cmd {
	const char *name;
	const char *help;
	cmd_handler cmd;
} cmd_t;

int cmd_dump(char *arg);
int cmd_reset(char *arg);
int cmd_power(char *arg);
int cmd_scan(char *arg);
int cmd_spectrum(char *arg);
int cmd_spacing(char *arg);
int cmd_seek(char *arg);
int cmd_tune(char *arg);
int cmd_monitor(char *arg);
int cmd_volume(char *arg);
int cmd_set(char *arg);

int cmd_arg(char *cmd, const char *str, char **arg);
int cmd_is(char *str, const char *is);

#ifdef __cplusplus
}
#endif
#endif
