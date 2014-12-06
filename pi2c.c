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

/*
	Basic I2C wrapper for Raspberry Pi.

	Make sure that RPi i2c driver is enabled, check following files:
	/etc/modprobe.d/raspi-blacklist.conf
		#blacklist i2c-bcm2708
	/etc/modules
		i2c-dev
	/etc/modprobe.d/i2c.conf
		options i2c_bcm2708 baudrate=400000
*/
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "pi2c.h"

/* i2c bus file descriptors */
static int i2c_bus[2] = { -1, -1 };

/* open I2C bus if not opened yet and store file descriptor */
int pi2c_open(uint8_t bus)
{
	char bus_name[64];

	if (bus > PI2C_BUS1)
		return -1;

	// already opened?
	if (i2c_bus[bus] >= 0)
		return 0;

	// open i2c bus and store file descriptor
	sprintf(bus_name, "/dev/i2c-%u", bus);

	if ((i2c_bus[bus] = open(bus_name, O_RDWR)) < 0)
		return -1;

	return 0;
}

/* close I2C bus file descriptor */
int pi2c_close(uint8_t bus)
{
	if (bus > PI2C_BUS1)
		return -1;

	if (i2c_bus[bus] >= 0)
		close(i2c_bus[bus]);
	i2c_bus[bus] = -1;

	return 0;
}

/* select I2C device for pi2c_write() calls */
int pi2c_select(uint8_t bus, uint8_t slave)
{
	if ((bus > PI2C_BUS1) || (i2c_bus[bus] < 0))
		return -1;

	return ioctl(i2c_bus[bus], I2C_SLAVE, slave);
}

/* write to I2C device selected by pi2c_select() */
int pi2c_write(uint8_t bus, const uint8_t *data, uint32_t len)
{
	if ((bus > PI2C_BUS1) || (i2c_bus[bus] < 0))
		return -1;

	if (write(i2c_bus[bus], data, len) != (ssize_t)len)
		return -1;

	return 0;
}

/* read I2C device selected by pi2c_select() */
int pi2c_read(uint8_t bus, uint8_t *data, uint32_t len)
{
	if ((bus > PI2C_BUS1) || (i2c_bus[bus] < 0))
		return -1;

	if (read(i2c_bus[bus], data, len) != (ssize_t)len)
		return -1;

	return 0;
}
