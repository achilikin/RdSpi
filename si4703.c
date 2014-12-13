/*	Si4703 based RDS scanner
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
Si4702/03 FM Radio Receiver ICs:
	http://www.silabs.com/products/audio/fmreceivers/pages/si470203.aspx

Si4703S-C19 Data Sheet:
	https://www.sparkfun.com/datasheets/BreakoutBoards/Si4702-03-C19-1.pdf

Si4703S-B16 Data Sheet:
	Not on SiLabs site anymore, google for it

Si4700/01/02/03 Firmware Change List, AN281:
	Not on SiLabs site anymore, google for it

Using RDS/RBDS with the Si4701/03:
	http://www.silabs.com/Support%20Documents/TechnicalDocs/AN243.pdf

Si4700/01/02/03 Programming Guide:
	http://www.silabs.com/Support%20Documents/TechnicalDocs/AN230.pdf

RBDS Standard:
	ftp://ftp.rds.org.uk/pub/acrobat/rbds1998.pdf
*/
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "rds.h"
#include "pi2c.h"
#include "si4703.h"
#include "rpi_pin.h"

struct si4703_state {
	const char *name;
	uint8_t  reg;
	uint8_t  offset;
	uint16_t mask;
}si_state[] = {
	{ "DSMUTE",  POWERCFG, 15, DSMUTE },
	{ "DMUTE",   POWERCFG, 14, DMUTE },
	{ "MONO",    POWERCFG, 13, MONO },
	{ "RDSM",    POWERCFG, 11, RDSM },
	{ "SKMODE",  POWERCFG, 10, SKMODE },
	{ "SEEKUP",  POWERCFG,  9, SEEKUP },
	{ "SEEK",    POWERCFG,  8, SEEK },
	{ "DISABLE", POWERCFG,  6, PWR_DISABLE },
	{ "ENABLE",  POWERCFG,  0, PWR_ENABLE }, // do not write 0 to this register

	{ "TUNE",    CHANNEL,  15, TUNE },
	{ "CHAN",    CHANNEL,   0, CHAN },

	{ "RDSIEN",  SYSCONF1, 15, RDSIEN },
	{ "SRCIEN",  SYSCONF1, 14, STCIEN },
	{ "RDS",     SYSCONF1, 12, RDS },
	{ "DE",      SYSCONF1, 11, DE },
	{ "AGCD",    SYSCONF1, 10, AGCD },
	{ "BLNDADJ", SYSCONF1,  7, BLNDADJ },
	{ "GPIO3",   SYSCONF1,  4, GPIO3 },
	{ "GPIO2",   SYSCONF1,  2, GPIO2 },
	{ "GPIO1",   SYSCONF1,  0, GPIO1 },

	{ "SEEKTH",  SYSCONF2,  8, SEEKTH },
	{ "BAND",    SYSCONF2,  6, BAND },
	{ "SPACE",   SYSCONF2,  4, SPACING },
	{ "VOLUME",  SYSCONF2,  0, VOLUME },

	{ "SMUTER",  SYSCONF3,  14, SMUTER },
	{ "SMUTEA",  SYSCONF3,  12, SMUTEA },
	{ "RDSPRF",  SYSCONF3,   9, RDSPRF },
	{ "VOLEXT",  SYSCONF3,   8, VOLEXT },
	{ "SKSNR",   SYSCONF3,   4, SKSNR },
	{ "SKCNT",   SYSCONF3,   0, SKCNT },
	
	{ "XOSCEN",  TEST1,     15, XOSCEN },
	{ "AHIZEN",  TEST1,     14, AHIZEN },

	{ "RDSR",  STATUSRSSI,  15, RDSR },
	{ "STC",   STATUSRSSI,  14, STC },
	{ "SFBL",  STATUSRSSI,  13, SFBL },
	{ "AFCRL", STATUSRSSI,  12, AFCRL },
	{ "RDSS",  STATUSRSSI,  11, RDSS },
	{ "BLERA", STATUSRSSI,   9, BLERA },
	{ "ST",    STATUSRSSI,   8, STEREO },
	{ "RSSI",  STATUSRSSI,   0, RSSI },

	{ "BLERB",    READCHAN, 14, BLERB },
	{ "BLERC",    READCHAN, 12, BLERC },
	{ "BLERD",    READCHAN, 10, BLERD },
	{ "READCHAN", READCHAN,  0, RCHAN },
};

int si_band[3][2] = { {8750, 10800}, {7600, 10800}, {7600, 9000}};
int si_space[3] = { 20, 10, 5 };

inline int cmd_is(const char *str, const char *is)
{
	return strcmp(str, is) == 0;
}

int si_set_state(uint16_t *regs, const char *name, uint16_t val)
{
	uint16_t current;
	int nstates = sizeof(si_state)/sizeof(si_state[0]);
	for(int i = 0; i < nstates; i++) {
		if (cmd_is(si_state[i].name, name)) {
			current = regs[si_state[i].reg] & si_state[i].mask;
			current >>= si_state[i].offset;
			val <<= si_state[i].offset;
			val &= si_state[i].mask;
			regs[si_state[i].reg] &= ~si_state[i].mask;
			regs[si_state[i].reg] |= val;
			return current;
		}
	}
	return -1;
}

int si_read_regs(uint16_t *regs)
{
	uint8_t buf[32];

	if (pi2c_read(PI2C_BUS, buf, 32) < 0)
		return -1;

	// Si4703 sends back registers as 10, 11, 12, 13, 14, 15, 0, ...
	// so we need to shuffle our buffer a bit
	for(int i = 0, x = 10; x != 9; x++, i += 2) {
		x &= 0x0F;
		regs[x] = buf[i] << 8;
		regs[x] |= buf[i+1];
	}
	return 0;
}

static inline int bit_set(uint16_t reg, uint16_t bit)
{
	int ret = 0;
	if (reg & bit)
		ret = 1;
	return ret;
}

static int _get_freq(uint16_t *regs, uint16_t reg)
{
	int band = (regs[SYSCONF2] >> 6) & 0x03;
	int space = (regs[SYSCONF2] >> 4) & 0x03;
	int nchan = reg & 0x03FF;

	int freq = nchan*si_space[space] + si_band[band][0];
	return freq;
}

static const char *si_parse_reg(uint16_t *regs, uint8_t reg)
{
	static char buf[256];
	uint16_t bits = regs[reg];

	if (reg == CHIPID) {
		sprintf(buf, ": REV %X DEV %s FIRMWARE %d", (bits >> 10) & 0x3F,
			((bits >> 6) & 0x0F) == 9 ? "Si4703" : "Si4702", bits & 0x3F);
		return buf;
	}
	if (reg == POWERCFG) {
		sprintf(buf, ": DSMUTE %d DMUTE %d MONO %d RDSM %d SKMODE %d SEEKUP %d SEEK %d DISABLE %d ENABLE %d", 
			bit_set(bits, DSMUTE), bit_set(bits, DMUTE), bit_set(bits, MONO), bit_set(bits, RDSM),
			bit_set(bits, SKMODE), bit_set(bits, SEEKUP), bit_set(bits, SEEK),
			bit_set(bits, PWR_DISABLE), bit_set(bits, PWR_ENABLE));
		return buf;
	}
	if (reg == CHANNEL) {
		int freq =  _get_freq(regs, regs[CHANNEL]);
		sprintf(buf, ": TUNE %d CHAN %d (%d.%02dMHz)", bit_set(bits, TUNE), bits & 0x3FF, freq/100, freq%100);
		return buf;
	}
	if (reg == SYSCONF1) {
		sprintf(buf, ": RDSIEN %d STCIEN %d RDS %d DE %d AGCD %d BLNDADJ %d GPIO3 %d GPIO2 %d GPIO %d",
			bit_set(bits, RDSIEN), bit_set(bits, STCIEN), bit_set(bits, RDS),
			bit_set(bits, DE), bit_set(bits, AGCD), (bits >> 6) & 0x03,
			(bits >> 4) & 0x03, (bits >> 2) & 0x03, bits & 0x3);
		return buf;
	}
	if (reg == SYSCONF2) {
		sprintf(buf, ": SEEKTH %d BAND %d SPACE %d VOLUME %d",
			(bits >> 8) & 0xFF,	(bits >> 6) & 0x03, (bits >> 4) & 0x03, bits & 0x0F);
		return buf;
	}
	if (reg == SYSCONF3) {
		sprintf(buf, ": SMUTER %d SMUTEA %d RDSPRF %d VOLEXT %d SKSNR %d SKCNT %d", (bits >> 14) & 0x03, 
			(bits >> 12) & 0x03, bit_set(bits, RDSPRF), bit_set(bits, VOLEXT), (bits >> 4) & 0x0F, bits & 0x0F);
		return buf;
	}
	if (reg == TEST1) {
		sprintf(buf, ": XOSCEN %d AHIZEN %d", bit_set(bits, XOSCEN), bit_set(bits, AHIZEN));
		return buf;
	}
	if (reg == STATUSRSSI) {
		sprintf(buf, ": RDSR %d STC %d SF/BL %d AFCRL %d RDSS %d BLERA %d ST %d RSSI %d",
			bit_set(bits, RDSR), bit_set(bits, STC), bit_set(bits, SFBL), bit_set(bits, AFCRL),
			bit_set(bits, RDSS), (bits >> 9) & 0x03, (bits >> 8) & 0x01, bits & 0xFF);
		return buf;
	}
	if (reg == READCHAN) {
		int freq = _get_freq(regs, regs[READCHAN]);
		sprintf(buf, ": BLERB %d BLERC %d BLERD %d READCHAN %d (%d.%02dMHz)",
			(bits >> 14) & 0x03, (bits >> 12) & 0x03, (bits >> 10) & 0x03, bits & 0x3FF, freq/100, freq%100);
		return buf;
	}
	return "";
}

void si_dump(uint16_t *regs, const char *title, uint16_t span)
{
	if (title)
		printf("%s", title);
	uint8_t start = (span >> 8) & 0xFF;
	uint8_t num = span & 0xFF;
	if (start > 15)
		return;
	if ((start + num) > 16)
		return;
	for(uint8_t i = 0; i < num; i++)
		printf("%X %04X%s\n", i, regs[start + i], si_parse_reg(regs, start + i));
}

int si_update(uint16_t *regs)
{
	int i = 0, ret = 0;
	uint8_t buf[32];
	
	// write automatically begins at register 0x02
	for(int reg = 2 ; reg < 8 ; reg++) {
		buf[i++] = regs[reg] >> 8;
		buf[i++] = regs[reg] & 0x00FF;
	}

	ret = pi2c_write(PI2C_BUS, buf, i);
	return ret;
}

void si_set_channel(uint16_t *regs, int chan)
{
	int band = (regs[SYSCONF2] >> 6) & 0x03;
	int space = (regs[SYSCONF2] >> 4) & 0x03;
	int nchan = (si_band[band][1] - si_band[band][0])/si_space[space];

	if (chan > nchan) chan = nchan;

	regs[CHANNEL] &= 0xFC00;
	regs[CHANNEL] |= chan;
	regs[CHANNEL] |= TUNE;
	si_update(regs);

	rpi_delay_ms(10);

	// poll to see if STC is set
	int i = 0;
	while(i++ < 100) {
		si_read_regs(regs);
		if (regs[STATUSRSSI] & STC) break;
		rpi_delay_ms(10);
	}

	regs[CHANNEL] &= ~TUNE;
	si_update(regs);

	// wait for the si4703 to clear the STC as well
	i = 0;
	while(i++ < 100) {
		si_read_regs(regs);
		if (!(regs[STATUSRSSI] & STC)) break;
		rpi_delay_ms(10);
	}
}

// freq: 9500 for 95.00 MHz
void si_tune(uint16_t *regs, int freq)
{
	si_read_regs(regs);

	int band = (regs[SYSCONF2] >> 6) & 0x03;
	int space = (regs[SYSCONF2] >> 4) & 0x03;

	if (freq < si_band[band][0]) freq = si_band[band][0];
	if (freq > si_band[band][1]) freq = si_band[band][1];

	int nchan = (freq - si_band[band][0])/si_space[space];

	si_set_channel(regs, nchan);
}

void si_set_rdsprf(uint16_t *regs, int set)
{
	if (set) {
		regs[SYSCONF1] |= RDS;
		regs[SYSCONF3] |= RDSPRF;
	}
	else {
		regs[SYSCONF1] &= ~RDS;
		regs[SYSCONF3] &= ~RDSPRF;
	}
}

void si_set_volume(uint16_t *regs, int volume)
{
	if (volume < 0) volume = 0;
	if (volume > 15) volume = 15;

	if (volume)
		regs[POWERCFG] |= DSMUTE | DMUTE; // unmute
	else
		regs[POWERCFG] &= ~(DSMUTE | DMUTE); // mute

	regs[SYSCONF2] &= ~VOLUME; // clear volume bits
	regs[SYSCONF2] |= volume;  // set new volume
	si_update(regs);
}

int si_get_freq(uint16_t *regs)
{
	return _get_freq(regs, regs[READCHAN]);
}

int si_seek(uint16_t *regs, int dir)
{
	si_read_regs(regs);

	int channel = regs[READCHAN] & RCHAN; // current channel
	if(dir == SEEK_DOWN)
		regs[POWERCFG] &= ~SEEKUP; //Seek down is the default upon reset
	else
		regs[POWERCFG] |= SEEKUP; //Set the bit to seek up

	regs[POWERCFG] |= SEEK; //Start seek
	si_update(regs); //Seeking will now start

	//Poll to see if STC is set
	int i = 0;
	while(i++ < 500) {
		si_read_regs(regs);
		if((regs[STATUSRSSI] & STC) != 0) break; //Tuning complete!
		rpi_delay_ms(10);
	}

	si_read_regs(regs);
	int valueSFBL = regs[STATUSRSSI] & SFBL; //Store the value of SFBL
	regs[POWERCFG] &= ~SEEK; //Clear the seek bit after seek has completed
	si_update(regs);

	//Wait for the si4703 to clear the STC as well
	i = 0;
	while(i++ < 500) {
		si_read_regs(regs);
		if( (regs[STATUSRSSI] & STC) == 0) break; //Tuning complete!
		rpi_delay_ms(10);
	}

	if (channel == (regs[READCHAN] & RCHAN))
		return 0;
	if (valueSFBL) //The bit was set indicating we hit a band limit or failed to find a station
		return 0;
	return si_get_freq(regs);
}

void si_power(uint16_t *regs, uint16_t mode)
{
	if (mode == PWR_ENABLE) {
		// set only ENABLE bit, leaving device muted
		regs[POWERCFG] = PWR_ENABLE;
		// by default we allow wrap by not setting SKMODE
		// regs[POWERCFG] |= SKMODE; 
		regs[SYSCONF1] |= RDS; // enable RDS

		// set mono/stereo blend adjust to default 0 or 31-49 RSSI
		regs[SYSCONF1] &= ~BLNDADJ;
		// set different BLDADJ if needed
		// x00=31-49, x40=37-55, x80=19-37,xC0=25-43 RSSI
		//	regs[SYSCONF1] |= 0x0080;
		// enable RDS High-Performance Mode
		regs[SYSCONF3] |= RDSPRF;
		regs[SYSCONF1] |= DE; // set 50us De-Emphasis for Europe, skip for USA
		// select general Europe/USA 87.5 - 108 MHz band
		regs[SYSCONF2] = BAND0 | SPACE100; // 100kHz channel spacing for Europe

		// apply recommended seek settings for "most stations"
		regs[SYSCONF2] &= ~SEEKTH;
		regs[SYSCONF2] |= 0x0C00; // SEEKTH 12
		regs[SYSCONF3] &= 0xFF00;
		regs[SYSCONF3] |= 0x004F; // SKSNR 4, SKCNT 15
	}
	else {
		// power down condition
		regs[POWERCFG] = PWR_DISABLE | PWR_ENABLE;
	}

	si_update(regs);
	// recommended powerup time
	rpi_delay_ms(110);
}
