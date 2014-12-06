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

#ifndef __RPI_PIN_H__
#define __RPI_PIN_H__

#ifdef __cplusplus
extern "C" {
#if 0 // dummy bracket for VAssistX
}
#endif
#endif

#include <bcm2835.h>

#define RPI_GPIO_04 RPI_V2_GPIO_P1_07 // Version 2, Pin P1-07
#define RPI_GPIO_17 RPI_V2_GPIO_P1_11 // Version 2, Pin P1-11
#define RPI_GPIO_18 RPI_V2_GPIO_P1_12 // Version 2, Pin P1-12
#define RPI_GPIO_22 RPI_V2_GPIO_P1_15 // Version 2, Pin P1-15
#define RPI_GPIO_23 RPI_V2_GPIO_P1_16 // Version 2, Pin P1-16
#define RPI_GPIO_24	RPI_V2_GPIO_P1_18 // Version 2, Pin P1-18
#define RPI_GPIO_25 RPI_V2_GPIO_P1_22 // Version 2, Pin P1-22
#define RPI_GPIO_27 RPI_V2_GPIO_P1_13 // Version 2, Pin P1-13
// RPi Version 2, new plug P5
#define RPI_GPIO_28 RPI_V2_GPIO_P5_03 // Version 2, Pin P5-03
#define RPI_GPIO_29 RPI_V2_GPIO_P5_04 // Version 2, Pin P5-04
#define RPI_GPIO_30 RPI_V2_GPIO_P5_05 // Version 2, Pin P5-05
#define RPI_GPIO_31	RPI_V2_GPIO_P5_06 // Version 2, Pin P5-06

#define RPI_GPIO_PWM RPI_V2_GPIO_P1_12 // Version 2, Pin P1-12

#define RPI_VALID_GPIO (RPI_GPIO_04 | RPI_GPIO_17 | RPI_GPIO_18 | RPI_GPIO_22 | \
						RPI_GPIO_23 | RPI_GPIO_24 | RPI_GPIO_25 | RPI_GPIO_27 | \
						RPI_GPIO_28 | RPI_GPIO_29 | RPI_GPIO_30 | RPI_GPIO_31)

#define RPI_GPIO_OUT 0x00

#define RPI_GPIO_OUT_HIGH  0x01 // Set Output to High
#define RPI_GPIO_OUT_LOW   0x00 // Set Output to Low

#define RPI_GPIO_IN  0x80
// IN mode
#define RPI_GPIO_PUD_UP   0x01 // Enable Pull Up control
#define RPI_GPIO_PUD_DOWN 0x02 // Enable Pull Down control
#define RPI_GPIO_PUD_OFF  0x03  // Off ? disable pull-up/down

static inline void rpi_delay_ms(uint32_t ms)
{
	bcm2835_delay(ms);
}

static inline void rpi_delay_mcs(uint64_t us)
{
	bcm2835_delayMicroseconds(us);
}

static inline void rpi_set_pin_pud(uint8_t pin, uint8_t pud)
{
	pud &= RPI_GPIO_PUD_OFF;
	if (pud == RPI_GPIO_PUD_UP)
		bcm2835_gpio_set_pud(pin, BCM2835_GPIO_PUD_UP);
	else if (pud == RPI_GPIO_PUD_DOWN)
		bcm2835_gpio_set_pud(pin, BCM2835_GPIO_PUD_DOWN);
	else
		bcm2835_gpio_set_pud(pin, BCM2835_GPIO_PUD_OFF);
}

static inline int rpi_set_pin_mode(uint8_t pin, uint8_t mode)
{
	if (mode & RPI_GPIO_IN) {
		bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_INPT);
		rpi_set_pin_pud(pin, mode);
		return 0;
	}

	bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_OUTP);
	(mode & RPI_GPIO_OUT_HIGH) ? bcm2835_gpio_set(pin) : bcm2835_gpio_clr(pin);
	return 0;
}

static inline void rpi_pin_set(uint8_t pin)
{
	bcm2835_gpio_set(pin);
}

static inline void rpi_pin_clr(uint8_t pin)
{
	bcm2835_gpio_clr(pin);
}

static inline void rpi_pin_write(uint8_t pin, uint8_t on_off)
{
	bcm2835_gpio_write(pin, on_off);
}

static inline void rpi_pin_set_mask(uint32_t mask)
{
	mask &= RPI_VALID_GPIO;
	bcm2835_gpio_set_multi(mask);
}

static inline void rpi_pin_clr_mask(uint32_t mask)
{
	mask &= RPI_VALID_GPIO;
	bcm2835_gpio_clr_multi(mask);
}

static inline void rpi_pin_write_mask(uint32_t mask, uint8_t on_off)
{
	mask &= RPI_VALID_GPIO;
	bcm2835_gpio_write_multi(mask, on_off);
}

static inline int rpi_pin_get(uint8_t pin)
{
	return bcm2835_gpio_lev(pin);
}

int rpi_init(void);
int rpi_close(void);

// some extra functions
uint32_t rpi_rdtsc(void);  // ARM based 250MHz free running counter
uint32_t rpi_sysmks(void); // lower 32 bits of system 1MHz free running counter
uint64_t rpi_systsc(void); // system 1MHz free running counter

#ifdef __cplusplus
}
#endif

#endif
