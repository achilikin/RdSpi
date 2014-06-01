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

typedef int (*cmd_handler)(const char *arg);

typedef struct cmd {
	const char *name;
	cmd_handler cmd;
} cmd_t;

int cmd_dump(const char *arg);
int cmd_reset(const char *arg);
int cmd_power(const char *arg);
int cmd_scan(const char *arg);
int cmd_spectrum(const char *arg);
int cmd_spacing(const char *arg);
int cmd_seek(const char *arg);
int cmd_tune(const char *arg);
int cmd_rds(const char *arg);
int cmd_volume(const char *arg);
int cmd_set(const char *arg);

inline int str_is(const char *str, const char *is)
{
	return strcmp(str, is) == 0;
}

#ifdef __cplusplus
}
#endif


#endif
