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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "rpi_pin.h"
#include "rpi_i2c.h"
#include "cmd.h"
#include "si4703.h"

#define RSSI_LIMIT 35
#define DEFAULT_STATION 9500 // Local station with the good signal strength

inline const char *is_on(uint16_t mask)
{
	if (mask) return "ON";
	return "OFF";
}

int cmd_reset(const char *arg __attribute__((unused)))
{
	uint16_t si_regs[16];
	si_read_regs(si_regs);

	rpi_set_pin_mode(SI_RESET, RPI_GPIO_OUT_LOW);
	rpi_delay_ms(10);
	rpi_set_pin_mode(SI_RESET, RPI_GPIO_IN);
	rpi_delay_ms(1);

	if (si_read_regs(si_regs) != 0) {
		printf("Unable to read Si4703!\n");
		return -1;
	}
	si_dump(si_regs, "Reset Map:\n", 16);

	// enable the oscillator
	si_regs[TEST1] |= XOSCEN;
	si_update(si_regs);
	rpi_delay_ms(500); // recommended delay
	si_read_regs(si_regs);
	si_dump(si_regs, "\nOscillator enabled:\n", 16);
	// the only way to reliable start the device is to powerdown and powerup
	// just powering up does not work for me after cold start
	uint8_t powerdown[2] = { 0, PWR_DISABLE | PWR_ENABLE };
	rpi_i2c_write(powerdown, 2);
	rpi_delay_ms(110);

	cmd_power("up");
	// tune to the local station with known signal strength
	si_tune(si_regs, DEFAULT_STATION);
	// si_tune() does not update registers
	si_update(si_regs);
	rpi_delay_ms(10);
	si_read_regs(si_regs);
	si_dump(si_regs, "\nTuned\n", 16);
	return 0;
}

int cmd_power(const char *arg)
{
	uint16_t si_regs[16];
	si_read_regs(si_regs);

	if (arg) {
		if (str_is(arg, "up")) {
			si_power(si_regs, PWR_ENABLE);
			si_dump(si_regs, "\nPowerup:\n", 16);
			return 0;
		}
		if (str_is(arg, "down")) {
			si_power(si_regs, PWR_DISABLE);
			si_dump(si_regs, "\nPowerdown:\n", 16);
			return 0;
		}
	}

	si_dump(si_regs, "\nPower:\n", 16);
	return 0;
}

int cmd_dump(const char *arg __attribute__((unused)))
{
	uint16_t si_regs[16];
	si_read_regs(si_regs);
	si_dump(si_regs, "Registers map:\n", 16);
	return 0;
}

int cmd_scan(const char *arg) 
{
	uint8_t mode = 0;
	int nstations = 0;
	int freq, seek = 0;
	uint16_t si_regs[16];

	si_read_regs(si_regs);
	si_regs[POWERCFG] |= SKMODE; // stop seeking at the upper or lower band limit

	// mode as recommended in AN230, Table 23. Summary of Seek Settings
	if (arg) {
		mode = atoi(arg);
		if (mode > 5) mode = 5;
	}

	si_set_channel(si_regs, 0);
	printf("scanning...\n");

	if (mode > 0) {
		si_regs[SYSCONF2] &= 0x00FF;
		si_regs[SYSCONF3] &= 0xFF00;
		uint16_t conf2 = 0;
		uint16_t conf3 = 0;

		if (mode == 1) {
			conf2 = 0x1900;
			conf3 = 0x0000;
		}
		if (mode == 2) {
			conf2 = 0x1900;
			conf3 = 0x0048;
		}
		if (mode == 3) {
			conf2 = 0x0C00;
			conf3 = 0x0048;
		}
		if (mode == 4) {
			conf2 = 0x0C00;
			conf3 = 0x007F;
		}
		if (mode == 5) {
			conf2 = 0x0000;
			conf3 = 0x004F;
		}
		si_regs[SYSCONF2] |= conf2;
		si_regs[SYSCONF3] |= conf3;

		si_update(si_regs);
	}

	while(1) {
		freq = si_seek(si_regs, SEEK_UP);
		if (freq == 0)
			break;
		seek = freq;
		nstations++;
		uint8_t rssi = si_regs[STATUSRSSI]	& RSSI;
		printf("%5d ", freq);
		for(int si = 0; si < rssi; si++)
			printf("-");
		printf(" %d", rssi);
		fflush(stdout);

		int dt = 0;
		if (rssi > RSSI_LIMIT) {
			while(dt < 10000) {
				si_read_regs(si_regs);
				if (si_regs[STATUSRSSI] & STEREO)
					break;
				rpi_delay_ms(10);
				dt += 10;
			}
		}
		uint16_t st = si_regs[STATUSRSSI] & STEREO;

		if (st) {
			printf(" ST");
			fflush(stdout);
			if (rssi > RSSI_LIMIT) {
				int pi;
				char ps_name[16];
				if ((pi = si_rds_get_ps(ps_name, si_regs, 5000)) != -1)
					printf(" %04X '%s'", pi, ps_name);
			}
		}
		printf("\n");
	}
	si_regs[POWERCFG] &= ~SKMODE; // restore wrap mode
	si_tune(si_regs, seek);

	printf("%d stations found\n", nstations);
	return 0;
}

int cmd_spectrum(const char *arg)
{
	uint16_t si_regs[16];
	uint8_t rssi_limit = RSSI_LIMIT;

	si_read_regs(si_regs);
	int band = (si_regs[SYSCONF2] >> 6) & 0x03;
	int space = (si_regs[SYSCONF2] >> 4) & 0x03;
	int nchan = (si_band[band][1] - si_band[band][0]) / si_space[space];

	if (arg)
		rssi_limit = (uint8_t)atoi(arg);

	for (int i = 0; i <= nchan; i++) {
		si_regs[CHANNEL] &= ~CHAN;
		si_regs[CHANNEL] |= i;
		si_regs[CHANNEL] |= TUNE;
		si_update(si_regs);
		rpi_delay_ms(10);

		while(1) {
			si_read_regs(si_regs);
			if (si_regs[STATUSRSSI] & STC) break;
			rpi_delay_ms(10);
		}	
		si_regs[CHANNEL] &= ~TUNE;
		si_update(si_regs);
		while(i) {
			si_read_regs(si_regs);
			if(!(si_regs[STATUSRSSI] & STC)) break;
			rpi_delay_ms(10);
		}

		uint8_t rssi = si_regs[STATUSRSSI]	& 0xFF;
		printf("%5d ", si_band[band][0] + i*si_space[space]);
		for(int si = 0; si < rssi; si++)
			printf("-");
		printf(" %d", rssi);
		fflush(stdout);

		int dt = 0;
		if (rssi > rssi_limit) {
			while(dt < 3000) {
				si_read_regs(si_regs);
				if (si_regs[STATUSRSSI] & STEREO)
					break;
				rpi_delay_ms(10);
				dt += 10;
			}
		}
		uint16_t st = si_regs[STATUSRSSI] & STEREO;

		if (st) {
			printf(" ST");
			fflush(stdout);
			if (rssi > rssi_limit) {
				int pi;
				char ps_name[16];
				if ((pi = si_rds_get_ps(ps_name, si_regs, 5000)) != -1)
					printf(" %04X '%s'", pi, ps_name);
			}
		}
		printf("\n");
	}
	return 0;
}

int cmd_seek(const char *arg)
{
	int dir = SEEK_UP;
	uint16_t si_regs[16];

	if (str_is(arg, "up"))
		dir = SEEK_UP;
	if (str_is(arg, "down"))
		dir = SEEK_DOWN;

	si_read_regs(si_regs);

	if (si_seek(si_regs, dir) == 0) {
		printf("seek failed\n");
		return -1;
	}

	return 0;
}

int cmd_tune(const char *arg)
{
	int	freq = DEFAULT_STATION;
	uint16_t si_regs[16];

	if (arg)
		freq = atoi(arg);

	si_read_regs(si_regs);
	si_tune(si_regs, freq);
	si_read_regs(si_regs);
	freq = si_get_freq(si_regs);
	if (freq) {
		printf("Tuned to %d.%02dMHz\n", freq/100, freq%100);
		si_dump(si_regs, "Register map:\n", 16);
		return 0;
	}
	return -1;
}

int cmd_spacing(const char *arg)
{
	uint16_t spacing = 0;
	uint16_t si_regs[16];
	si_read_regs(si_regs);

	if (arg) { // do we have an extra parameter?
		spacing = atoi(arg);
		if (spacing == 200) spacing = 0;
		if (spacing == 100) spacing = 1;
		if (spacing == 50)  spacing = 2;
		if (spacing > 2) {
			printf("Invalid spacing, use 200, 100, 50 kHz\n");
			return -1;
		}
		si_regs[SYSCONF2] &= ~SPACING;
		si_regs[SYSCONF2] |= spacing << 4;
		si_update(si_regs);
		return 0;
	}

	spacing = (si_regs[SYSCONF2] & SPACING) >> 4;
	printf("spacing %d\n", si_space[spacing]);
	return 0;
}

int cmd_rds(const char *arg)
{
	uint16_t si_regs[16];
	uint16_t gtmask = 0xFFFF;

	si_read_regs(si_regs);

	if (arg) { // do we have an extra parameter?
		if (str_is(arg, "on")) {
			si_set_rdsprf(si_regs, 1);
			si_update(si_regs);
			si_read_regs(si_regs);
			printf("RDSPRF set to %s\n", is_on(si_regs[SYSCONF3] & RDSPRF));
			si_dump(si_regs, "\nRegister map\n", 16);
			return 0;
		}
		if (str_is(arg, "off")) {
			si_set_rdsprf(si_regs, 0);
			si_update(si_regs);
			si_read_regs(si_regs);
			printf("RDSPRF set to %s\n", is_on(si_regs[SYSCONF3] & RDSPRF));
			si_dump(si_regs, "\nRegister map\n", 16);
			return 0;
		}
		if (str_is(arg, "verbose")) {
			si_read_regs(si_regs);
			si_regs[POWERCFG] ^= RDSM;
			si_update(si_regs);
			si_read_regs(si_regs);
			printf("RDSM set to %s\n", is_on(si_regs[POWERCFG] & RDSM));
			si_dump(si_regs, "\nRegister map\n", 16);
			return 0;
		}
		gtmask = (1 << atoi(arg)) & 0xFFFF;
	}

	si_rds_dump(si_regs, gtmask, 15000);
	return 0;
}

int cmd_volume(const char *arg)
{
	uint8_t volume;
	uint16_t si_regs[16];
	si_read_regs(si_regs);

	volume = si_regs[SYSCONF2] & VOLUME;
	if (arg) {
		volume = atoi(arg);
		si_set_volume(si_regs, volume);
		return 0;
	}

	printf("volume %d\n", volume);
	return 0;
}

int cmd_set(const char *arg)
{
	uint16_t val;
	const char *pval;
	char buf[32];

	if (arg == NULL)
		return -1;

	pval = arg;
	if (pval == NULL)
		return -1;
	int i;
	for(i = 0; i < 31; i++) {
		if (*pval == '=')
			break;
		buf[i] = toupper(*pval++);
	}
	buf[i] = '\0';
	if (*pval != '=')
		return -1;

	val = (uint16_t)atoi(++pval);

	uint16_t regs[16];
	si_read_regs(regs);
	if (si_set_state(regs, buf, val) != -1) {
		si_update(regs);
		si_dump(regs, arg, 16);
		return 0;
	}
	return -1;
}
