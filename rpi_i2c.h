/*	I2C wrapper for Mike McCauley's bcm2835 library
	http://www.airspayce.com/mikem/bcm2835/
	
	Copyright (C) 2013 Andrey Chilikin https://github.com/achilikin

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __RPI_I2C_H__
#define __RPI_I2C_H__

#include <stdint.h>

#include <bcm2835.h>

#define I2C_SPEED_100K 100000
#define I2C_SPEED_400K 400000

static inline int rpi_i2c_open(uint8_t bus __attribute__((unused)))
{
	bcm2835_i2c_begin();
	return 0;
}

static inline int rpi_i2c_close(void)
{
	bcm2835_i2c_end();
	return 0;
}

static inline int rpi_i2c_config(uint32_t baudrate)
{
	bcm2835_i2c_set_baudrate(baudrate);
	return 0;
}

static inline int rpi_i2c_set_slave(uint8_t address)
{
	bcm2835_i2c_setSlaveAddress(address);
	return 0;
}

#define RPI2C_RS_READ 0x8000000

// for repeated start use 'start | RPI2C_RS_READ'
static inline int rpi_i2c_read_regs(uint32_t start, uint8_t *buf, uint32_t regs)
{
	int retval;
	uint8_t reg = start & 0xFF;
	
	if (start & RPI2C_RS_READ)
		retval = (int)bcm2835_i2c_read_register_rs((char *)&reg, (char *)buf, regs);
	else {
		retval = (int)bcm2835_i2c_write((const char *)&reg, 1);
		retval |= (int)bcm2835_i2c_read((char *)buf, regs);
	}

	if (retval)
		return -retval;

	return retval;
}

static inline int rpi_i2c_read(uint8_t *buf, uint32_t regs)
{
	int retval = (int)bcm2835_i2c_read((char *)buf, regs);

	if (retval)
		return -retval;

	return retval;
}

static inline int rpi_i2c_read_reg(uint32_t ureg)
{
	int retval;
	uint8_t val;

	retval = rpi_i2c_read_regs(ureg, &val, 1);

	if (retval)
		return -retval;

	retval = val;
	return retval & 0xFF;
}

static inline int rpi_i2c_write_reg(uint8_t reg, uint8_t data)
{
	uint8_t wr_buf[2];

	wr_buf[0] = reg;
	wr_buf[1] = data;

	int iret = (int)bcm2835_i2c_write((const char *)wr_buf, 2);

	if (iret)
		return -iret;
	
	return iret & 0xFF;
}

static inline int rpi_i2c_write(uint8_t *data, uint32_t len)
{
	int iret = 0;
	
	iret = (int)bcm2835_i2c_write((const char *)data, len);

	if (iret)
		return -iret;

	return iret & 0xFF;
}

#endif
