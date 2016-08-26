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

#include "cmd.h"
#include "cli.h"
#include "rds.h"
#include "pi2c.h"
#include "si4703.h"
#include "rpi_pin.h"

#define RSSI_LIMIT 35
#define DEFAULT_STATION 9500 // Local station with the good signal strength

inline const char *is_on(uint16_t mask)
{
	if (mask) return "ON";
	return "OFF";
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

int cmd_reset(int fd, UNUSED(char *arg))
{
	uint16_t si_regs[16];
	si_read_regs(si_regs);

	rpi_pin_set_dir(SI_RESET, RPI_OUTPUT);
	rpi_pin_set(SI_RESET, 0);
	rpi_delay_ms(10);
	rpi_pin_set_dir(SI_RESET, RPI_INPUT);
	rpi_delay_ms(1);

	if (si_read_regs(si_regs) != 0) {
		dprintf(fd, "Unable to read Si4703!\n");
		return CLI_ENODEV;
	}
	si_dump(fd, si_regs, "Reset Map:\n", 16);

	// enable the oscillator
	si_regs[TEST1] |= XOSCEN;
	si_update(si_regs);
	rpi_delay_ms(500); // recommended delay
	si_read_regs(si_regs);
	si_dump(fd, si_regs, "\nOscillator enabled:\n", 16);
	// the only way to reliable start the device is to powerdown and powerup
	// just powering up does not work for me after cold start
	uint8_t powerdown[2] = { 0, PWR_DISABLE | PWR_ENABLE };
	pi2c_write(PI2C_BUS, powerdown, 2);
	rpi_delay_ms(110);

	cmd_power(fd, const_cast<char *>("up"));
	// tune to the local station with known signal strength
	si_tune(si_regs, DEFAULT_STATION);
	// si_tune() does not update registers
	si_update(si_regs);
	rpi_delay_ms(10);
	si_read_regs(si_regs);
	si_dump(fd, si_regs, "\nTuned\n", 16);
	return 0;
}

int cmd_power(int fd, char *arg)
{
	uint16_t si_regs[16];
	if (si_read_regs(si_regs) != 0)
		return CLI_ENODEV;

	if (arg && *arg) {
		if (cmd_is(arg, "up")) {
			si_power(si_regs, PWR_ENABLE);
			si_dump(fd, si_regs, "\nPowerup:\n", 16);
			return 0;
		}
		if (cmd_is(arg, "down")) {
			si_power(si_regs, PWR_DISABLE);
			si_dump(fd, si_regs, "\nPowerdown:\n", 16);
			return 0;
		}
	}

	si_dump(fd, si_regs, "\nPower:\n", 16);
	return 0;
}

int cmd_dump(int fd, char *arg __attribute__((unused)))
{
	uint16_t si_regs[16];
	if (si_read_regs(si_regs) == 0) {
		si_dump(fd, si_regs, "Registers map:\n", 16);
		return 0;
	}
	return CLI_ENODEV;
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
				memcpy(rd.hdr.rds, &regs[RDSA], sizeof(rd.hdr.rds));
				rds_parse_gt00a(rd.hdr.rds, &rd);
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
		return rd.hdr.rds[0];
	return -1;
}

int cmd_scan(int fd, char *arg)
{
	uint8_t mode = 0;
	int nstations = 0;
	int freq, seek = 0;
	uint16_t si_regs[16];

	if (si_read_regs(si_regs) != 0)
		return CLI_ENODEV;

	si_regs[POWERCFG] |= SKMODE; // stop seeking at the upper or lower band limit

	// mode as recommended in AN230, Table 23. Summary of Seek Settings
	if (arg && *arg) {
		mode = atoi(arg);
		if (mode > 5) mode = 5;
	}

	si_set_channel(si_regs, 0);
	dprintf(fd, "scanning, press any key to terminate...\n");

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
	while(!is_stop(&stop)) {
		freq = si_seek(si_regs, SEEK_UP);
		if (freq == 0)
			break;
		seek = freq;
		nstations++;
		uint8_t rssi = si_regs[STATUSRSSI]	& RSSI;
		dprintf(fd, "%5d ", freq);
		for(int si = 0; si < rssi; si++)
			dprintf(fd, "-");
		dprintf(fd, " %d", rssi);

		int dt = 0;
		if (rssi > RSSI_LIMIT) {
			while(dt < 10000) {
				si_read_regs(si_regs);
				if (si_regs[STATUSRSSI] & STEREO) break;
				if (is_stop(&stop)) break;
				rpi_delay_ms(10);
				dt += 10;
			}
		}
		uint16_t st = si_regs[STATUSRSSI] & STEREO;

		if (st) {
			dprintf(fd, " ST");
			if (rssi > RSSI_LIMIT) {
				int pi;
				char ps_name[16];
				if ((pi = get_ps_si(ps_name, si_regs, 5000)) != -1)
					dprintf(fd, " %04X '%s'", pi, ps_name);
			}
		}
		dprintf(fd, "\n");
	}
	
	si_regs[POWERCFG] &= ~SKMODE; // restore wrap mode
	si_tune(si_regs, seek);

	dprintf(fd, "%d stations found\n", nstations);
	return 0;
}

int cmd_spectrum(int fd, char *arg)
{
	uint16_t si_regs[16];
	uint8_t rssi_limit = RSSI_LIMIT;

	if (si_read_regs(si_regs) != 0)
		return CLI_ENODEV;
	int band = (si_regs[SYSCONF2] >> 6) & 0x03;
	int space = (si_regs[SYSCONF2] >> 4) & 0x03;
	int nchan = (si_band[band][1] - si_band[band][0]) / si_space[space];

	if (arg && *arg)
		rssi_limit = (uint8_t)atoi(arg);

	dprintf(fd, "scanning, press any key to terminate...\n");

	int stop = 0;
	for (int i = 0; i <= nchan && !stop; i++, is_stop(&stop)) {
		si_regs[CHANNEL] &= ~CHAN;
		si_regs[CHANNEL] |= i;
		si_regs[CHANNEL] |= TUNE;
		si_update(si_regs);
		rpi_delay_ms(10);

		while(1) {
			si_read_regs(si_regs);
			if (si_regs[STATUSRSSI] & STC) break;
			if (is_stop(&stop)) break;
			rpi_delay_ms(10);
		}	
		si_regs[CHANNEL] &= ~TUNE;
		si_update(si_regs);
		while(i) {
			si_read_regs(si_regs);
			if(!(si_regs[STATUSRSSI] & STC)) break;
			if (is_stop(&stop)) break;
			rpi_delay_ms(10);
		}

		uint8_t rssi = si_regs[STATUSRSSI]	& 0xFF;
		dprintf(fd, "%5d ", si_band[band][0] + i*si_space[space]);
		for(int si = 0; si < rssi; si++)
			dprintf(fd, "-");
		dprintf(fd, " %d", rssi);

		int dt = 0;
		if (rssi > rssi_limit) {
			while(dt < 3000) {
				si_read_regs(si_regs);
				if (si_regs[STATUSRSSI] & STEREO) break;
				if (is_stop(&stop)) break;
				rpi_delay_ms(10);
				dt += 10;
			}
		}
		uint16_t st = si_regs[STATUSRSSI] & STEREO;

		if (st) {
			dprintf(fd, " ST");
			if (rssi > rssi_limit) {
				int pi;
				char ps_name[16];
				if ((pi = get_ps_si(ps_name, si_regs, 5000)) != -1)
					dprintf(fd, " %04X '%s'", pi, ps_name);
			}
		}
		dprintf(fd, "\n");
	}
	return 0;
}

int cmd_seek(int fd, char *arg)
{
	int dir = SEEK_UP;
	uint16_t si_regs[16];

	if (cmd_is(arg, "up"))
		dir = SEEK_UP;
	else if (cmd_is(arg, "down"))
		dir = SEEK_DOWN;
	else {
		dprintf(fd, "wrong seeking direction\n");
		return -1;
	}

	dprintf(fd, "seeking %s\n", arg);

	if (si_read_regs(si_regs) != 0)
		return CLI_ENODEV;
	int freq = si_seek(si_regs, dir);
	if (freq == 0) {
		dprintf(fd, "seek failed\n");
		return -1;
	}
	dprintf(fd, "tuned to %u\n", freq);
	return 0;
}

int cmd_tune(int fd, char *arg)
{
	unsigned freq = DEFAULT_STATION;
	uint16_t si_regs[16];

	if (arg && *arg) {
		freq = strtol(arg, &arg, 10);
		if (*arg == '.') {
			unsigned decimal = strtol(arg + 1, NULL, 10);
			if (decimal > 100)
				decimal = 0;
			freq = freq * 100 + decimal;
		}
	}

	if (si_read_regs(si_regs) != 0)
		return CLI_ENODEV;
	si_tune(si_regs, freq);
	si_read_regs(si_regs);
	freq = si_get_freq(si_regs);
	if (freq) {
		dprintf(fd, "Tuned to %d.%02dMHz\n", freq/100, freq%100);
		si_dump(fd, si_regs, "Register map:\n", 16);
		return 0;
	}
	return -1;
}

int cmd_spacing(int fd, char *arg)
{
	uint16_t spacing = 0;
	uint16_t si_regs[16];
	if (si_read_regs(si_regs) != 0)
		return CLI_ENODEV;

	if (arg && *arg) { // do we have an extra parameter?
		spacing = atoi(arg);
		if (spacing == 200) spacing = 0;
		if (spacing == 100) spacing = 1;
		if (spacing == 50)  spacing = 2;
		if (spacing > 2) {
			dprintf(fd, "Invalid spacing, use 200, 100, 50 kHz\n");
			return -1;
		}
		si_regs[SYSCONF2] &= ~SPACING;
		si_regs[SYSCONF2] |= spacing << 4;
		si_update(si_regs);
		return 0;
	}

	spacing = (si_regs[SYSCONF2] & SPACING) >> 4;
	dprintf(fd, "spacing %d\n", si_space[spacing]);
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

static void print_rds_hdr(int fd, rds_hdr_t *phdr)
{
	dprintf(fd, "%04X %04X %04X %04X ", phdr->rds[0], phdr->rds[1], phdr->rds[2], phdr->rds[3]);
	dprintf(fd, "| GT %02d%c PTY %2d TP %d | ", phdr->gt, 'A' + phdr->ver, phdr->pty, phdr->tp);
}

/* default printer for 'rds log' command */
static void print_rds(int fd, rds_hdr_t *phdr, int log)
{
	print_rds_hdr(fd, phdr);
	dprintf(fd, "%02X %04X %04X", phdr->rds[1] & 0x1F, phdr->rds[2], phdr->rds[3]);
	dprintf(fd, "%s\n", log ? "" : clr_eol);
}

static void cmd_monitor_si(int fd, uint16_t *regs, uint16_t pr_mask, uint32_t timeout, int log)
{ 
	rds_gt00a_t rd0;
	rds_gt01a_t rd1;
	rds_gt02a_t rd2;
	rds_gt03a_t rd3;
	rds_gt04a_t rd4;
	rds_gt05a_t rd5;
	rds_gt08a_t rd8;
	rds_gt10a_t rd10;
	rds_gt14a_t rd14;
	rds_hdr_t   rds[16];

	memset(&rd0, 0, sizeof(rd0));
	memset(&rd1, 0, sizeof(rd1));
	memset(&rd2, 0, sizeof(rd2));
	memset(&rd3, 0, sizeof(rd3));
	memset(&rd5, 0, sizeof(rd5));
	memset(&rd8, 0, sizeof(rd8));
	memset(&rd10, 0, sizeof(rd10));
	memset(&rd14, 0, sizeof(rd14));

	uint16_t gt_mask  = 0; // mask of groups detected
	uint16_t gta_mask = 0; // mask of A groups detected
	uint16_t gtb_mask = 0; // mask of B groups detected
	uint16_t rt_mask  = 0; // mask of radiotext segments processed
	uint16_t ps_mask  = 0;
	uint32_t endTime  = 0;

	if (!log) {
		dprintf(fd, "%s%s%s", clr_all, go_top, cur_hid);
		dprintf(fd, "monitoring RDS, press any key to terminate...%s%s\n", clr_eol, txt_nor);
	}

	while(!is_stop(NULL)) {
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
			memcpy(&rds[gt], &hdr, sizeof(hdr));

			gt_mask |= _BM(gt);
			if (!ver)
				gta_mask |= _BM(gt);
			else
				gtb_mask |= _BM(gt);

			if (!log) {
				dprintf(fd, "%s%s", go_top, txt_rev);
				print_rds_hdr(fd, &hdr);
				dprintf(fd, "monitoring RDS, press any key to terminate...%s%s\n", clr_eol, txt_nor);
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
			// 5A: Transparent data channels or ODA
			if (gtv == RDS_GT_05A) {
				memcpy(&rd5.hdr, &hdr, sizeof(hdr));
				rds_parse_gt05a(&regs[RDSA], &rd5);
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
				print_rds_hdr(fd, &rd0.hdr);
				dprintf(fd, "TA %d MS %c DI %X Ci %d PS '%s' AF %d %d (%d): ", 
					rd0.ta, rd0.ms, rd0.di, rd0.ci, rd0.ps, regs[RDSC] &0xFF, regs[RDSC] >> 8, rd0.naf);
				for(int i = 0; rd0.af[i]; i++)
					dprintf(fd, "%d ", 8750 + rd0.af[i]*10);
				dprintf(fd, "%s\n", log ? "" : clr_eol);
			}

			if (mask & _BM(1)) {
				print_rds_hdr(fd, &rd1.hdr);
				dprintf(fd, "RPC %d LA %d VC %d SLC %03X ", 
					rd1.rpc, rd1.la, rd1.vc, rd1.slc);
				if (rd1.pinc)
					dprintf(fd, " %02d %02d:%02d", rd1.pinc >> 11, 
					(rd1.pinc >> 6) & 0x1F, rd1.pinc & 0x3F);
				dprintf(fd, "%s\n", log ? "" : clr_eol);
			}

			if (mask & _BM(2)) {
				rt_mask = rd2.valid;
				print_rds_hdr(fd, &rd2.hdr);
				dprintf(fd, "AB %c Si %2d ", 'A' + rd2.ab, rd2.si);
				dprintf(fd, "RT '%s'", rd2.rt);
				dprintf(fd, "%s\n", log ? "" : clr_eol);
			}

			if (mask & _BM(3)) {
				print_rds_hdr(fd, &rd3.hdr);
				dprintf(fd, "AGTC %d%c Msg %04X AID %04X VC %d ",
					rd3.agtc, rd3.ver + 'A', rd3.msg, rd3.aid, rd3.vc);
				if (rd3.vc == 0) {
					dprintf(fd, "LTN %d ", rd3.ltn);
					if (rd3.afi) dprintf(fd, "AFI ");
					if (rd3.m)	 dprintf(fd, "M ");
					if (rd3.i)	 dprintf(fd, "I ");
					if (rd3.n)	 dprintf(fd, "N ");
					if (rd3.r)	 dprintf(fd, "R ");
					if (rd3.u)	 dprintf(fd, "U ");
				}
				else {
					dprintf(fd, "SID %d ", rd3.sid);
					if (rd3.m)
						dprintf(fd, "G %d Ta %d Tw %d Td %d", rd3.g, rd3.ta, rd3.tw, rd3.td);
				}
				dprintf(fd, "%s\n", log ? "" : clr_eol);
			}

			if (mask & _BM(4)) {
				print_rds_hdr(fd, &rd4.hdr);
				dprintf(fd, "%d/%02d/%02d %02d:%02d",
					rd4.year, rd4.month, rd4.day, rd4.hour, rd4.minute);
				if (rd4.tz_hour == 0 && rd4.tz_half == 0)
					dprintf(fd, " UTC");
				else
					dprintf(fd, " TZ%c%d.%d", rd4.ts_sign ? '-' : '+',
					rd4.tz_hour, rd4.tz_half);
				dprintf(fd, "%s\n", log ? "" : clr_eol);
			}

			if (mask & _BM(5)) {
				print_rds_hdr(fd, &rd5.hdr);
				for (uint8_t i = 0; i < 32; i++) {
					if (rd5.channel & (1u << i))
						dprintf(fd, "TDS[%u] %04X %04X ",
							i, rd5.tds[i][0], rd5.tds[i][1]);
				}
				dprintf(fd, "%s\n", log ? "" : clr_eol);
			}

			if (mask & _BM(6))
				print_rds(fd, &rds[6], log);

			if (mask & _BM(7))
				print_rds(fd, &rds[7], log);
			
			if (mask & _BM(8)) {
				// check if 8A is Alert-C 
				print_rds_hdr(fd, &rd8.hdr);
				if (rd3.agtc == 8 && rd3.ver == 0 && rd3.aid == 0xCD46) {
					dprintf(fd, "S%d G%d CI%d ", rd8.x4, rd8.x3, rd8.x2);
					if (rd8.x3)
						dprintf(fd, "D%d DIR%d Ext %d Eve %d Loc %04X",
						rd8.d, rd8.dir, rd8.ext, rd8.eve, rd8.loc);
					else
						dprintf(fd, "Y %04X Loc %04X", rd8.y, rd8.loc);
				}
				else
					dprintf(fd, "X4 %d VC %d", rd8.x4, rd8.vc);
				dprintf(fd, "%s\n", log ? "" : clr_eol);
			}

			if (mask & _BM(9))
				print_rds(fd, &rds[9], log);

			if (mask & _BM(10)) {
				print_rds_hdr(fd, &rd10.hdr);
				dprintf(fd, "AB %c Ci %d PTYN '%s'", 'A' + rd10.ab, rd10.ci, rd10.ps);
				dprintf(fd, "%s\n", log ? "" : clr_eol);
			}
			
			if (mask & _BM(11))
				print_rds(fd, &rds[11], log);
			
			if (mask & _BM(12))
				print_rds(fd, &rds[12], log);

			if (mask & _BM(13))
				print_rds(fd, &rds[13], log);

			if (mask & _BM(14)) {
				print_rds_hdr(fd, &rd14.hdr);
				dprintf(fd, "TP %d VC %2d ", rd14.tp_on, rd14.variant);
				dprintf(fd, "I %04X ", rd14.info);
				dprintf(fd, "PI %04X ", rd14.pi_on);
				dprintf(fd, "PS '%s' ", rd14.ps);
				if (rd14.avc & _BM(13))
					dprintf(fd, "PTY %2d TA %d ", rd14.pty >> 11, rd14.pty & 0x01);
				if (rd14.avc & _BM(14))
					dprintf(fd, "PIN %04X", rd14.pin);
				dprintf(fd, "%s\n", log ? "" : clr_eol);
			}

			if (mask & _BM(15))
				print_rds(fd, &rds[15], log);
		}
		else {
			rpi_delay_ms(30);
			endTime += 30;
		}

		if (timeout && (endTime >= timeout))
			break;
	}

	int freq = si_get_freq(regs);
	dprintf(fd, "\nScanned %d.%02d ", freq/100, freq%100);
	if (rd0.valid == 0x0F)
		dprintf(fd, "'%s' ", rd0.ps);
	dprintf(fd, "for %d ms\n", endTime);
	if (rd2.valid)
		dprintf(fd, "Radiotext: '%s'\n", rd2.rt);
	if (!gt_mask)
		dprintf(fd, "no RDS detected\n");
	else {
		dprintf(fd, "Active groups %04X:\n", gt_mask);
		for(int i = 0; i < 16; i++) {
			if (gta_mask & (1 << i))
				dprintf(fd, "    %02dA %s\n", i, rds_gt_name(i, 0));
			if (gtb_mask & (1 << i))
				dprintf(fd, "    %02dB %s \n", i, rds_gt_name(i, 1));
		}
		dprintf(fd, "\n");
	}
	if (!log)
		dprintf(fd, "%s", cur_vis);
}

int cmd_monitor(int fd, char *arg)
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
		dprintf(fd, "RDSPRF set to %s\n", is_on(si_regs[SYSCONF3] & RDSPRF));
		si_dump(fd, si_regs, "\nRegister map\n", 16);
		return 0;
	}
	if (cmd_is(arg, "off")) {
		si_set_rdsprf(si_regs, 0);
		si_update(si_regs);
		si_read_regs(si_regs);
		dprintf(fd, "RDSPRF set to %s\n", is_on(si_regs[SYSCONF3] & RDSPRF));
		si_dump(fd, si_regs, "\nRegister map\n", 16);
		return 0;
	}
	if (cmd_is(arg, "verbose")) {
		si_read_regs(si_regs);
		si_regs[POWERCFG] ^= RDSM;
		si_update(si_regs);
		si_read_regs(si_regs);
		dprintf(fd, "RDSM set to %s\n", is_on(si_regs[POWERCFG] & RDSM));
		si_dump(fd, si_regs, "\nRegister map\n", 16);
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

	cmd_monitor_si(fd, si_regs, gtmask, timeout, log);
	return 0;
}

int cmd_volume(int fd, char *arg)
{
	uint8_t volume;
	uint16_t si_regs[16];
	si_read_regs(si_regs);

	volume = si_regs[SYSCONF2] & VOLUME;
	if (arg && *arg) {
		volume = atoi(arg);
		si_set_volume(si_regs, volume);
		return 0;
	}

	dprintf(fd, "volume %d\n", volume);
	return 0;
}

int cmd_set(int fd, char *arg)
{
	uint16_t val;
	const char *pval;
	char buf[32];

	if (arg == NULL || *arg == '\0')
		return -1;
	pval = arg;
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
		si_dump(fd, regs, arg, 16);
		return 0;
	}
	return -1;
}
