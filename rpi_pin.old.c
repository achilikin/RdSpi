/*	Wrapper for Mike McCauley's bcm2835 library
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

#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>

#include "rpi_pin.h"

int rpi_tsc_init(void);
int rpi_tsc_close(void);

int rpi_init(void)
{
	if (!bcm2835_init())
		return -1;
	
	rpi_tsc_init();
	return 0;
}

int rpi_close(void)
{
	rpi_tsc_close();
	bcm2835_close();
	return 0;
}

static volatile uint32_t *bcm2835_tick;

// page 196 http://www.raspberrypi.org/wp-content/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
#define	ARM_TIMER_CONTROL    0x0408
#define ARM_TC_32BIT         0x0002 // enable 32bit counters
#define ARM_TC_IRQ_ENABLE    0x0020 // enable timer irq
#define ARM_TC_TIMER_ENABLE  0x0080 // enable timer
#define ARM_TC_TIMER_HALT    0x0010 // halt timer in debug mode
#define ARM_TC_FRC_ENABLE    0x0200 // enable free running counter

#define	ARM_TIMER_FRC        0x0420

#define BCM2835_TICK_BASE (BCM2835_PERI_BASE +  0x0000B000)

static uint32_t arm_timer_ctl;

int rpi_tsc_init(void)
{
	int mfd = -1;
	void *pmem = MAP_FAILED;
	// Open the master /dev/memory device
	if ((mfd = open("/dev/mem", O_RDWR | O_SYNC) ) < 0) {
		printf("rpi_tsc_init: Unable to open /dev/mem\n");
		return -1;
	}

	// GPIO TIMER:
	pmem = mmap(NULL, BCM2835_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, BCM2835_TICK_BASE);
	close(mfd);

	if (pmem == MAP_FAILED)
		return -1;

	bcm2835_tick = (volatile uint32_t *)pmem;
	arm_timer_ctl = *(bcm2835_tick + ARM_TIMER_CONTROL/4);
	*(bcm2835_tick + ARM_TIMER_CONTROL/4) = ARM_TC_FRC_ENABLE | ARM_TC_TIMER_HALT;

	return 0;
}

// Close this library and deallocate everything
int rpi_tsc_close(void)
{
	if (bcm2835_tick == NULL)
		return -1;

	// restore original control register
	*(bcm2835_tick + ARM_TIMER_CONTROL/4) = arm_timer_ctl;
	munmap((void *)bcm2835_tick, BCM2835_BLOCK_SIZE);

	return 0;
}    

// returns 32 bits of ARM free running counter (250MHz)
uint32_t rpi_rdtsc(void)
{
	uint32_t stamp;

	stamp = *(bcm2835_tick + ARM_TIMER_FRC/4);

	return stamp;
}

// returns lower 32 bits of system microsecond free running counter
uint32_t rpi_sysmks(void)
{
	uint32_t stamp;
	stamp = *(bcm2835_st + BCM2835_ST_CLO/4);
	return stamp;
}

// returns full 64 bits of system microsecond free running counter
uint64_t rpi_systsc(void)
{
	uint64_t stamp;
	stamp = *(bcm2835_st + BCM2835_ST_CHI/4);
	stamp <<= 32;
	stamp += *(bcm2835_st + BCM2835_ST_CLO/4);
	return stamp;
}

