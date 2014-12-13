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

#ifndef __SI4703_B16_H__
#define __SI4703_B16_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#if 0 // dummy bracket for VAssistX
}
#endif
#endif

#define _BM(bit) (1 << ((uint16_t)bit)) // convert bit number to bit mask

#define SI4703_ADDR 0x10
#define SI_RESET 23

// Define the register names
#define DEVICEID   0x00
#define CHIPID     0x01
#define POWERCFG   0x02
	#define DSMUTE      _BM(15)
	#define DMUTE       _BM(14)
	#define MONO        _BM(13)
	#define RDSM        _BM(11)
	#define SKMODE      _BM(10)
	#define SEEKUP      _BM(9)
	#define SEEK        _BM(8)
	#define PWR_DISABLE _BM(6)
	#define PWR_ENABLE  _BM(0)
	// seek direction
	#define SEEK_DOWN 0
	#define SEEK_UP   1

#define CHANNEL    0x03
	#define TUNE  _BM(15)
	#define CHAN  0x003FF

#define SYSCONF1   0x04
	#define RDSIEN _BM(15)
	#define STCIEN _BM(14)
	#define RDS    _BM(12)
	#define DE     _BM(11)
	#define AGCD   _BM(10)
	#define BLNDADJ 0x00C0
	#define GPIO3   0x0030
	#define GPIO2   0x000C
	#define GPIO1   0x0003

#define SYSCONF2   0x05
	#define SEEKTH   0xFF00
	#define BAND     0x00C0
	#define SPACING  0x0030
	#define VOLUME   0x000F
	#define BAND0    0x0000 // 87.5 - 107 MHz (Europe, USA)
	#define BAND1    0x0040 // 76-108 MHz (Japan wide band)
	#define BAND2    0x0080 // 76-90 MHz (Japan)
	#define SPACE50  0x0020 //  50 kHz spacing
	#define SPACE100 0x0010 // 100 kHz (Europe, Japan)
	#define SPACE200 0x0000 // 200 kHz (USA, Australia)

#define SYSCONF3   0x06
	#define SMUTER 0xC000
	#define SMUTEA 0x3000
	#define RDSPRF _BM(9)
	#define VOLEXT _BM(8)
	#define SKSNR  0x00F0
	#define SKCNT  0x000F

#define TEST1 0x07
	#define XOSCEN _BM(15)
	#define AHIZEN _BM(14)

#define TEST2    0x08
#define BOOTCONF 0x09

#define STATUSRSSI 0x0A
	#define RDSR   _BM(15)
	#define STC    _BM(14)
	#define SFBL   _BM(13)
	#define AFCRL  _BM(12)
	#define RDSS   _BM(11)
	#define BLERA  0x0600
	#define STEREO _BM(8)
	#define RSSI   0x00FF

#define READCHAN   0x0B
	#define BLERB  0xC000
	#define BLERC  0x3000
	#define BLERD  0x0C00
	#define RCHAN  0x03FF

#define RDSA       0x0C
#define RDSB       0x0D
#define RDSC       0x0E
#define RDSD       0x0F

int  si_read_regs(uint16_t *regs);
int  si_update(uint16_t *regs);
void si_dump(uint16_t *regs, const char *title, uint16_t span);
void si_power(uint16_t *regs, uint16_t mode);
void si_set_volume(uint16_t *regs, int volume);

int  si_get_freq(uint16_t *regs);
int  si_seek(uint16_t *regs, int dir);
void si_set_channel(uint16_t *regs, int chan);
void si_tune(uint16_t *regs, int freq);

int  si_set_state(uint16_t *regs, const char *name, uint16_t val);
void si_set_rdsprf(uint16_t *regs, int set);

extern int si_band[3][2];
extern int si_space[3];

#ifdef __cplusplus
}
#endif
#endif
