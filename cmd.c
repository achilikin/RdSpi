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
#include <poll.h>
#include <termios.h>
#include <math.h>

#include "cmd.h"
#include "rds.h"
#include "pi2c.h"
#include "si4703.h"
#include "rpi_pin.h"

#define RSSI_LIMIT 35
#define DEFAULT_STATION 9770 // Local station with the good signal strength: CHOM Montreal

inline const char *is_on(uint16_t mask)
{
	if (mask) return "ON";
	return "OFF";
}

static void stdin_mode(int raw)
{
	struct termios term_attr;

	tcgetattr(STDIN_FILENO, &term_attr);
	if (raw)
		term_attr.c_lflag &= ~(ECHO | ICANON);
	else
		term_attr.c_lflag |= (ECHO | ICANON);
	term_attr.c_cc[VMIN] = raw ? 0 : 1;
	tcsetattr(STDIN_FILENO, TCSANOW, &term_attr);

	return;
}

static int stdin_getch(int ms_timeout)
{
	struct pollfd polls;
	polls.fd = STDIN_FILENO;
	polls.events = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI;
	polls.revents = 0;

	if (poll(&polls, 1, ms_timeout) < 0)
		return -1;

	int ch = 0;
	if (polls.revents)
		read(STDIN_FILENO, &ch, 1);

	return ch;
}

int cmd_arg(char *cmd, const char *str, char **arg)
{
	if (!cmd)
		return 0;
	while(*cmd && *cmd <= ' ') cmd++;
	char *end;
	size_t len = strlen(str);
	if (strncmp(cmd, str, len) == 0) {
		end = cmd + len;
		if (*end > ' ')
			return 0;
		if (arg) {
			for(; *end <= ' ' && *end != '\0'; end ++);
			*arg = end;
		}
		return 1;
	}
	return 0;
}

int cmd_is(char *str, const char *is)
{
	if (!str || !is)
		return 0;
	return cmd_arg(str, is, NULL);
}

int cmd_reset(char *arg __attribute__((unused)))
{
	uint16_t si_regs[16];
	si_read_regs(si_regs);

	rpi_pin_set_dir(SI_RESET, RPI_OUTPUT);
	rpi_pin_set(SI_RESET, 0);
	rpi_delay_ms(10);
	rpi_pin_set_dir(SI_RESET, RPI_INPUT);
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
	pi2c_write(PI2C_BUS, powerdown, 2);
	rpi_delay_ms(110);

	cmd_power(const_cast<char *>("up"));
	// tune to the local station with known signal strength
	si_tune(si_regs, DEFAULT_STATION);
	// si_tune() does not update registers
	si_update(si_regs);
	rpi_delay_ms(10);
	si_read_regs(si_regs);
	si_dump(si_regs, "\nTuned\n", 16);
	return 0;
}

int cmd_power(char *arg)
{
	uint16_t si_regs[16];
	si_read_regs(si_regs);

	if (arg) {
		if (cmd_is(arg, "up")) {
			si_power(si_regs, PWR_ENABLE);
			si_dump(si_regs, "\nPowerup:\n", 16);
			return 0;
		}
		if (cmd_is(arg, "down")) {
			si_power(si_regs, PWR_DISABLE);
			si_dump(si_regs, "\nPowerdown:\n", 16);
			return 0;
		}
	}

	si_dump(si_regs, "\nPower:\n", 16);
	return 0;
}

int cmd_dump(char *arg __attribute__((unused)))
{
	uint16_t si_regs[16];
	si_read_regs(si_regs);
	si_dump(si_regs, "Registers map:\n", 16);
	return 0;
}

static int get_ps_si(char *ps_name, uint16_t *regs, int timeout)
{ 
	int dt = 0;
	rds_gt00a_t rd;
	memset(&rd, 0, sizeof(rd));

	while(dt < timeout) {
		if (rd.valid == 0x0F)
			break;
		si_read_regs(regs);
		if (regs[STATUSRSSI] & RDSR) {
			// basic tuning and switching information
			if (RDS_GET_GT(regs[RDSB]) == RDS_GT_00A) {
				rds_parse_gt00a(&regs[RDSA], &rd);
			}
			rpi_delay_ms(40); // Wait for the RDS bit to clear
			dt += 40;
		}
		else {
			rpi_delay_ms(30);
			dt += 30;
		}
	}
	strcpy(ps_name, rd.ps);
	if (rd.valid == 0x0F)
		return 0;
	return -1;
}

int cmd_scan(char *arg) 
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
	printf("scanning, press any key to terminate...\n");

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

	int stop = 0;
	stdin_mode(1);

	while(stop == 0) {
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
				if ((pi = get_ps_si(ps_name, si_regs, 5000)) != -1)
					printf(" %04X '%s'", pi, ps_name);
			}
		}
		printf("\n");
		stop = stdin_getch(0);
	}
	
	stdin_mode(0);

	si_regs[POWERCFG] &= ~SKMODE; // restore wrap mode
	si_tune(si_regs, seek);

	printf("%d stations found\n", nstations);
	return 0;
}

int cmd_spectrum(char *arg)
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
				if ((pi = get_ps_si(ps_name, si_regs, 5000)) != -1)
					printf(" %04X '%s'", pi, ps_name);
			}
		}
		printf("\n");
	}
	return 0;
}

int cmd_seek(char *arg)
{
	int dir = SEEK_UP;
	uint16_t si_regs[16];

	if (cmd_is(arg, "up"))
		dir = SEEK_UP;
	if (cmd_is(arg, "down"))
		dir = SEEK_DOWN;

	si_read_regs(si_regs);
	int freq = si_seek(si_regs, dir);
	if (freq == 0) {
		printf("seek failed\n");
		return -1;
	}
	printf("tuned to %u\n", freq);
	return 0;
}

int cmd_tune(char *arg)
{
	int	freq = DEFAULT_STATION;
	uint16_t si_regs[16];

        // get target frequency as an int or a float
	if (arg) {
            if (index(arg,'.')>=0) { // something like 97.7
		freq = round(atof(arg) * 100);
                }
            else freq = atoi(arg);
	    printf("Tuning to %d.%02dMHz\n", freq/100, freq%100);
            }

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

int cmd_spacing(char *arg)
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

// VT100 ESC codes for monitor mode
static const char clr_all[] = { 27, '[', '2', 'J', '\0' };  // clear screen
static const char clr_eol[] = { 27, '[', '0', 'K', '\0' };  // clear to EOL
static const char go_top[]  = { 27, '[', '1', ';', '1', 'H','\0' }; // go top left
static const char cur_vis[] = { 27, '[', '?', '2', '5', 'h','\0' }; // show cursor
static const char cur_hid[] = { 27, '[', '?', '2', '5', 'l','\0' }; // hide cursor
static const char txt_nor[] = { 27, '[', '0', 'm', '\0' }; // normal text
static const char txt_rev[] = { 27, '[', '7', 'm', '\0' }; // reverse text

static void print_rds_hdr(rds_hdr_t *phdr)
{
	printf("%04X %04X %04X %04X ", phdr->rds[0], phdr->rds[1], phdr->rds[2], phdr->rds[3]);
	printf("| GT %02d%c PTY %2d TP %d | ", phdr->gt, 'A' + phdr->ver, phdr->pty, phdr->tp);
}

static void cmd_monitor_si(uint16_t *regs, uint16_t pr_mask, uint32_t timeout, int log)
{ 
	rds_gt00a_t rd0;
	rds_gt01a_t rd1;
	rds_gt02a_t rd2;
	rds_gt03a_t rd3;
	rds_gt04a_t rd4;
	rds_gt08a_t rd8;
	rds_gt10a_t rd10;
	rds_gt14a_t rd14;

	memset(&rd0, 0, sizeof(rd0));
	memset(&rd1, 0, sizeof(rd1));
	memset(&rd2, 0, sizeof(rd2));
	memset(&rd3, 0, sizeof(rd3));
	memset(&rd4, 0, sizeof(rd4));
	memset(&rd8, 0, sizeof(rd8));
	memset(&rd10, 0, sizeof(rd10));
	memset(&rd14, 0, sizeof(rd14));

	uint16_t gt_mask  = 0; // mask of groups detected
	uint16_t gta_mask = 0; // mask of A groups detected
	uint16_t gtb_mask = 0; // mask of B groups detected
	uint16_t rt_mask  = 0; // mask of radiotext segments processed
	uint16_t ps_mask  = 0;
	uint32_t endTime  = 0;

	int stop = 0;
	stdin_mode(1);
	if (!log)
		printf("%s%s%s", clr_all, go_top, cur_hid);
	printf("monitoring RDS, press any key to terminate...\n");

	while(stop == 0) {
		if (timeout && (rt_mask == 0xFFFF) && (ps_mask == 0x0F))
			break;
		si_read_regs(regs);
		if (regs[STATUSRSSI] & RDSR) {
			rds_hdr_t hdr;
			uint16_t gtv = RDS_GET_GT(regs[RDSB]);
			uint8_t  ver = (regs[RDSB] >> 11) & 0x01;
			uint8_t  gt  = (regs[RDSB] >> 12) & 0x0F;
			hdr.gt  = gt;
			hdr.ver = ver;
			hdr.tp  = (regs[RDSB] >> 10) & 0x01;
			hdr.pty = (regs[RDSB] >> 5) & 0x1F;
			memcpy(hdr.rds, &regs[RDSA], sizeof(hdr.rds));

			gt_mask |= _BM(gt);
			if (!ver)
				gta_mask |= _BM(gt);
			else
				gtb_mask |= _BM(gt);

			if (!log) {
				printf("%s%s", go_top, txt_rev);
				print_rds_hdr(&hdr);
				printf("monitoring RDS, press any key to terminate...%s%s\n", clr_eol, txt_nor);
			}

			// 0A: basic tuning and switching information
			if (gtv == RDS_GT_00A) {
				memcpy(&rd0.hdr, &hdr, sizeof(hdr));
				rds_parse_gt00a(&regs[RDSA], &rd0);
			}
			// 1A: Program Item Number and slow labeling codes
			if (gtv == RDS_GT_01A) {
				memcpy(&rd1.hdr, &hdr, sizeof(hdr));
				rds_parse_gt01a(&regs[RDSA], &rd1);
			}
			// 2A: Radiotext
			if (gtv == RDS_GT_02A) {
				memcpy(&rd2.hdr, &hdr, sizeof(hdr));
				rds_parse_gt02a(&regs[RDSA], &rd2);
			}
			// 3A: AID for ODA
			if (gtv == RDS_GT_03A) {
				memcpy(&rd3.hdr, &hdr, sizeof(hdr));
				rds_parse_gt03a(&regs[RDSA], &rd3);
			}
			// 4A: Clock-time and date
			if (gtv == RDS_GT_04A) {
				memcpy(&rd4.hdr, &hdr, sizeof(hdr));
				rds_parse_gt04a(&regs[RDSA], &rd4);
			}
			// 8A: Traffic Message Channel
			if (gtv == RDS_GT_08A) {
				memcpy(&rd8.hdr, &hdr, sizeof(hdr));
				rds_parse_gt08a(&regs[RDSA], &rd8);
			}
			// 10A: Program Type Name
			if (gtv == RDS_GT_10A) {
				memcpy(&rd10.hdr, &hdr, sizeof(hdr));
				rds_parse_gt10a(&regs[RDSA], &rd10);
			}
			// 14A: Enhanced Other Networks information
			if (gtv == RDS_GT_14A) {
				memcpy(&rd14.hdr, &hdr, sizeof(hdr));
				rds_parse_gt14a(&regs[RDSA], &rd14);
			}

			rpi_delay_ms(40); // Wait for the RDS bit to clear, from AN230
			endTime += 40;

			uint16_t mask = pr_mask & gta_mask;
			if (log)
				mask = pr_mask & _BM(gt);

			if (mask & _BM(0)) {
				ps_mask = rd0.valid;
				print_rds_hdr(&rd0.hdr);
				printf("TA %d MS %c DI %X Ci %d PS '%s' AF %d %d (%d): ", 
					rd0.ta, rd0.ms, rd0.di, rd0.ci, rd0.ps, regs[RDSC] &0xFF, regs[RDSC] >> 8, rd0.naf);
				for(int i = 0; rd0.af[i]; i++)
					printf("%d ", 8750 + rd0.af[i]*10);
				printf("%s\n", log ? "" : clr_eol);
			}

			if (mask & _BM(1)) {
				print_rds_hdr(&rd1.hdr);
				printf("RPC %d LA %d VC %d SLC %03X ", 
					rd1.rpc, rd1.la, rd1.vc, rd1.slc);
				if (rd1.pinc)
					printf(" %02d %02d:%02d", rd1.pinc >> 11, 
					(rd1.pinc >> 6) & 0x1F, rd1.pinc & 0x3F);
				printf("%s\n", log ? "" : clr_eol);
			}

			if (mask & _BM(2)) {
				rt_mask = rd2.valid;
				print_rds_hdr(&rd2.hdr);
				printf("AB %c Si %2d ", 'A' + rd2.ab, rd2.si);
				printf("RT '%s'", rd2.rt);
				printf("%s\n", log ? "" : clr_eol);
			}

			if (mask & _BM(3)) {
				print_rds_hdr(&rd3.hdr);
				printf("AGTC %d%c Msg %04X AID %04X VC %d ",
					rd3.agtc, rd3.ver + 'A', rd3.msg, rd3.aid, rd3.vc);
				if (rd3.vc == 0) {
					printf("LTN %d ", rd3.ltn);
					if (rd3.afi) printf("AFI ");
					if (rd3.m)	 printf("M ");
					if (rd3.i)	 printf("I ");
					if (rd3.n)	 printf("N ");
					if (rd3.r)	 printf("R ");
					if (rd3.u)	 printf("U ");
				}
				else {
					printf("SID %d ", rd3.sid);
					if (rd3.m)
						printf("G %d Ta %d Tw %d Td %d", rd3.g, rd3.ta, rd3.tw, rd3.td);
				}
				printf("%s\n", log ? "" : clr_eol);
			}

			if (mask & _BM(4)) {
				print_rds_hdr(&rd4.hdr);
				printf("%d/%02d/%02d %02d:%02d",
					rd4.year, rd4.month, rd4.day, rd4.hour, rd4.minute);
				if (rd4.tz_hour == 0 && rd4.tz_half == 0)
					printf(" UTC");
				else
					printf(" TZ%c%d.%d", rd4.ts_sign ? '-' : '+',
					rd4.tz_hour, rd4.tz_half);
				printf("%s\n", log ? "" : clr_eol);
			}

			if (mask & _BM(8)) {
				// check if 8A is Alert-C 
				print_rds_hdr(&rd8.hdr);
				if (rd3.agtc == 8 && rd3.ver == 0 && rd3.aid == 0xCD46) {
					printf("S%d G%d CI%d ", rd8.x4, rd8.x3, rd8.x2);
					if (rd8.x3)
						printf("D%d DIR%d Ext %d Eve %d Loc %04X",
						rd8.d, rd8.dir, rd8.ext, rd8.eve, rd8.loc);
					else
						printf("Y %04X Loc %04X", rd8.y, rd8.loc);
				}
				else
					printf("X4 %d VC %d", rd8.x4, rd8.vc);
				printf("%s\n", log ? "" : clr_eol);
			}

			if (mask & _BM(10)) {
				print_rds_hdr(&rd10.hdr);
				printf("AB %c Ci %d PTYN '%s'", 'A' + rd10.ab, rd10.ci, rd10.ps);
				printf("%s\n", log ? "" : clr_eol);
			}

			if (mask & _BM(14)) {
				print_rds_hdr(&rd14.hdr);
				printf("TP %d VC %2d ", rd14.tp_on, rd14.variant);
				printf("I %04X ", rd14.info);
				printf("PI %04X ", rd14.pi_on);
				printf("PS '%s' ", rd14.ps);
				if (rd14.avc & _BM(13))
					printf("PTY %2d TA %d ", rd14.pty >> 11, rd14.pty & 0x01);
				if (rd14.avc & _BM(14))
					printf("PIN %04X", rd14.pin);
				printf("%s\n", log ? "" : clr_eol);
			}
		}
		else {
			rpi_delay_ms(30);
			endTime += 30;
		}

		if (timeout && (endTime >= timeout))
			break;
		stop = stdin_getch(0);
	}

	stdin_mode(0);

	int freq = si_get_freq(regs);
	printf("\nScanned %d.%02d ", freq/100, freq%100);
	if (rd0.valid == 0x0F)
		printf("'%s' ", rd0.ps);
	printf("for %d ms\n", endTime);
	if (rd2.valid)
		printf("Radiotext: '%s'\n", rd2.rt);
	if (!gt_mask)
		printf("no RDS detected\n");
	else {
		printf("Active groups %04X:\n", gt_mask);
		for(int i = 0; i < 16; i++) {
			if (gta_mask & (1 << i))
				printf("    %02dA %s\n", i, rds_gt_name(i, 0));
			if (gtb_mask & (1 << i))
				printf("    %02dB %s \n", i, rds_gt_name(i, 1));
		}
		printf("\n");
	}
	if (!log)
		printf("%s", cur_vis);
}

int cmd_monitor(char *arg)
{
	int log = 0;
	uint16_t si_regs[16];
	uint16_t gtmask = 0xFFFF;
	uint32_t timeout = 15000;

	si_read_regs(si_regs);

	if (cmd_is(arg, "on")) {
		si_set_rdsprf(si_regs, 1);
		si_update(si_regs);
		si_read_regs(si_regs);
		printf("RDSPRF set to %s\n", is_on(si_regs[SYSCONF3] & RDSPRF));
		si_dump(si_regs, "\nRegister map\n", 16);
		return 0;
	}
	if (cmd_is(arg, "off")) {
		si_set_rdsprf(si_regs, 0);
		si_update(si_regs);
		si_read_regs(si_regs);
		printf("RDSPRF set to %s\n", is_on(si_regs[SYSCONF3] & RDSPRF));
		si_dump(si_regs, "\nRegister map\n", 16);
		return 0;
	}
	if (cmd_is(arg, "verbose")) {
		si_read_regs(si_regs);
		si_regs[POWERCFG] ^= RDSM;
		si_update(si_regs);
		si_read_regs(si_regs);
		printf("RDSM set to %s\n", is_on(si_regs[POWERCFG] & RDSM));
		si_dump(si_regs, "\nRegister map\n", 16);
		return 0;
	}

	char *val;
	if (cmd_arg(arg, "gt", &val)) {
		gtmask = 0;
		while(*val && isdigit(*val)) {
			uint16_t gt = strtoul(val, &val, 10);
			gtmask |= (1 << gt) & 0xFFFF;
			while(*val && (*val == ',' || *val <= ' '))
				val++;
		}
		arg = val;
	}

	if (cmd_arg(arg, "time", &val)) {
		timeout = strtoul(val, &val, 10)*1000; // argument - timeout in seconds
		arg = val;
	}

	if (cmd_is(arg, "log"))
		log = 1;

	cmd_monitor_si(si_regs, gtmask, timeout, log);
	return 0;
}

int cmd_volume(char *arg)
{
	uint8_t volume;
	uint16_t si_regs[16];
	si_read_regs(si_regs);

	volume = si_regs[SYSCONF2] & VOLUME;
	if (arg) {
		volume = atoi(arg);

                // volume over 15 is achieved by un-disabling the volume-boost
		if (volume>15) {
		    si_set_state(si_regs, "VOLEXT", 0);
		    volume -= 15;
		    }
		else si_set_state(si_regs, "VOLEXT", 1);

		si_set_volume(si_regs, volume);
		return 0;
	}

	printf("volume %d\n", volume);
	return 0;
}

int cmd_set(char *arg)
{
	uint16_t val;
	const char *pval;
	char buf[32];

        // no register specified
	if (arg == NULL)
		return -1;

	pval = arg;
	if (pval == NULL) return -1; // no value specified

        // get name of register in buffer and also
        // get integer value of argument in val
	int i;
	for(i = 0; i < 31; i++) {
		if (*pval == '=')
			break;
		buf[i] = toupper(*pval++);
	}
	buf[i] = '\0';
	if (*pval != '=') return -1;
	val = (uint16_t)atoi(++pval);

	uint16_t regs[16];
	si_read_regs(regs);

        // modify the specified register
	if (si_set_state(regs, buf, val) != -1) {
		si_update(regs);
		si_dump(regs, arg, 16);
		return 0;
	}
	return -1;
}
