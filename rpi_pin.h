/*	Raspberry Pi pin access wrapper
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

#ifndef __RPI_IRQ_H__
#define __RPI_IRQ_H__

#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RPI_REV1 1
#define RPI_REV2 2

enum PIN_DIRECTION { RPI_INPUT, RPI_OUTPUT };
enum PIN_EDGE_MODE { EDGE_NONE = 0, EDGE_RISING, EDGE_FALLING, EDGE_BOTH };

// revision 1 - old, 2 - new, including P5 pins
// must be called before any other rpi_pin_* functions if revision is 1
int rpi_pin_init(int pi_revision);

// export gpio pin, must be called before other get/set functions
int rpi_pin_export(uint8_t pin, enum PIN_DIRECTION dir);
int rpi_pin_set_dir(uint8_t pin, enum PIN_DIRECTION dir);
int rpi_pin_get(uint8_t pin); // read input value
int rpi_pin_set(uint8_t pin, uint8_t value); // set output value 0-1
int rpi_pin_unexport(uint8_t pin);

// functions for pin change of value edge detection support using poll()
// returns pin's file descriptor
int rpi_pin_fd(uint8_t pin);

// enables POLLPRI event on edge detection, pin must be in INPUT mode
int rpi_pin_poll_enable(uint8_t pin, enum PIN_EDGE_MODE mode);

// clears pending polls and returns current value
static inline int rpi_pin_poll_clear(int fd)
{
	char val;

	lseek(fd, 0, SEEK_SET);
	read(fd, &val, 1);

	return val - '0';
}

#define rpi_delay_ms(x) usleep((x)*1000u)

#ifdef __cplusplus
}
#endif

#endif
