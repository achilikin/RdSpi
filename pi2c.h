/*	Basic I2C wrapper for Raspberry Pi.
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

/**
	Basic I2C wrapper for Raspberry Pi.

	Make sure that RPi i2c driver is enabled, check following files:
	/etc/modprobe.d/raspi-blacklist.conf
		#blacklist i2c-bcm2708
	/etc/modules
		i2c-dev
	/etc/modprobe.d/i2c.conf
		options i2c_bcm2708 baudrate=400000
*/

#ifndef __PI2C_H__
#define __PI2C_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PI2C_BUS0 0 /*< P5 header I2C bus */
#define PI2C_BUS1 1 /*< P1 header I2C bus */
#define PI2C_BUS  PI2C_BUS1 // default bus

int pi2c_open(uint8_t bus);  /*< open I2C bus  */
int pi2c_close(uint8_t bus); /*< close I2C bus */
int pi2c_select(uint8_t bus, uint8_t slave); /*< select I2C slave */
int pi2c_read(uint8_t bus, uint8_t *data, uint32_t len);
int pi2c_write(uint8_t bus, const uint8_t *data, uint32_t len);
#ifdef __cplusplus
}
#endif

#endif
