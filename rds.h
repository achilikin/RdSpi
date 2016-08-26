/*  Some of RBDS standard defines
    ftp://ftp.rds.org.uk/pub/acrobat/rbds1998.pdf

    Copyright (c) 2014 Andrey Chilikin (https://github.com/achilikin)
    
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

#ifndef __RDS_CONSTANTS_H__
#define __RDS_CONSTANTS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#if 0 // dummy bracket for VAssistX
}
#endif
#endif

// RBDS Standard:
// ftp://ftp.rds.org.uk/pub/acrobat/rbds1998.pdf

// RBDS Standard, Annex D
// Program Identification: Country Code, Area Coverage, Reference Number
#define RDS_PI(CC,AC,RN) ((((uint16_t)RN) & 0xFF) | \
	((((uint16_t)AC) & 0x0F) << 8) | \
	((((uint16_t)CC) & 0x0F) << 12))

// Some European Broadcasting Area Country Codes
// It is better not to use CC of your country
// to avoid collision with any local radio stations
#define RDS_GERMANY 0x0D
#define RDS_IRELAND 0x02
#define RDS_RUSSIA  0x07
#define RDS_UK      0x0C

// Coverage-Area Codes
#define CAC_LOCAL    0 // Local, single transmitter
#define CAC_INTER    1 // International
#define CAC_NATIONAL 2 // National
#define CAC_SUPRA    3 // Supra-Regional
#define CAC_REGION   4 // Regional + Region code 0-11

#define RDS_GT(GT,VER) ((uint16_t)(((((uint8_t)GT)&0xF)<<1) | (((uint8_t)VER)&0x1)) << 11)
#define RDS_TP 0x4000 // Traffic Program ID, do not use with MMR-70
#define RDS_PTY(PTY) ((((uint16_t)PTY) & 0x1F) << 5)
#define RDS_GET_GT(reg) ((uint16_t)(reg) & 0xF800)

#define RDS_VER 0x0800 // A/B version
#define RDS_TA  0x0010 // Traffic Announcement
#define RDS_MS  0x0008 // Music/Speech (group 0A/0B)
#define RDS_DI  0x0004 // Decoder-Identification control code (group 0A/0B)
#define RDS_AB  0x0010 // Text A/B reset flag

#define RDS_PSIM 0x0003 // Program Service name index mask
#define RDS_RTIM 0x000F // Radiotext index mask
#define RDS_PTYM (0x1F<<5)

// Some of RBDS groups parsed so far
#define RDS_GT_00A RDS_GT(0,0)
#define RDS_GT_01A RDS_GT(1,0)
#define RDS_GT_02A RDS_GT(2,0)
#define RDS_GT_03A RDS_GT(3,0)
#define RDS_GT_04A RDS_GT(4,0)
#define RDS_GT_05A RDS_GT(5,0)
#define RDS_GT_08A RDS_GT(8,0)
#define RDS_GT_10A RDS_GT(10,0)
#define RDS_GT_14A RDS_GT(14,0)

const char *rds_gt_name(uint8_t group, uint8_t version);

// Some of PTY codes, for full list see RBDS Standard, Annex F
// DO NOT USE 30 or 31!!!
#define PTY_NONE    0
#define PTY_NEWS    1
#define PTY_INFORM  2
#define PTY_SPORT   3
#define PTY_TALK    4
#define PTY_ROCK    5
#define PTY_COUNTRY 10
#define PTY_JAZZ    14
#define PTY_CLASSIC 15
#define PTY_WEATHER 29

#define RDS_A 0
#define RDS_B 1
#define RDS_C 2
#define RDS_D 3

typedef struct rds_hdr_s
{
	uint16_t rds[4];
	uint8_t gt;  // group type
	uint8_t ver; // version
	uint8_t tp;  // Traffic Program
	uint8_t pty;
} rds_hdr_t;

typedef struct rds_gt00a_s
{
	rds_hdr_t hdr;
	uint8_t valid;
	uint8_t ta;
	uint8_t ms;
	uint8_t di;
	uint8_t ci;
	char    ps[9];
	uint8_t naf;
	uint8_t af[25];
} rds_gt00a_t;

typedef struct rds_gt01a_s
{
	rds_hdr_t hdr;
	uint8_t  rpc;
	uint8_t  la;
	uint8_t  vc;
	uint16_t slc;
	uint16_t pinc;
} rds_gt01a_t;

typedef struct rds_gt02a_s
{
	rds_hdr_t hdr;
	uint16_t valid; // valid segments
	uint8_t  ab;
	uint8_t  si;
	char     rt[65];
} rds_gt02a_t;

typedef struct rds_gt03a_s
{
	rds_hdr_t hdr;
	uint8_t  agtc; // Application Group Type Code
	uint8_t  ver;  // 0 - A, 1 - B
	uint16_t msg;
	uint16_t aid;  // Application ID

	uint8_t  vc;
	uint8_t  ltn;
	uint8_t  afi, m, i, n, r, u;
	uint8_t  g, sid, ta, tw, td;
} rds_gt03a_t;

typedef struct rds_gt04a_s
{
	rds_hdr_t hdr;
	uint8_t  hour;
	uint8_t  minute;
	uint8_t  tz_hour;
	uint8_t  tz_half;
	uint8_t  ts_sign;
	uint8_t  day;
	uint8_t  month;
	uint8_t  week;
	uint16_t year;
} rds_gt04a_t;

typedef struct rds_gt05a_s
{
	rds_hdr_t hdr;
	uint32_t  channel;
	uint16_t  tds[32][2];
} rds_gt05a_t;

typedef struct rds_gt08a_s
{
	rds_hdr_t hdr;
	uint8_t  x4;
	uint8_t  vc;
	uint8_t  x3;
	uint8_t  x2;
	char     spn[9]; // service provider name
	uint16_t y;
	uint8_t  d;
	uint8_t  dir;
	uint8_t  ext;
	uint16_t eve;
	uint16_t loc;
} rds_gt08a_t;

typedef struct rds_gt10a_s
{
	rds_hdr_t hdr;
	uint8_t ab;
	uint8_t ci;
	char    ps[9];
} rds_gt10a_t;

typedef struct rds_gt14a_s
{
	rds_hdr_t hdr;
	uint8_t  tp_on;
	uint8_t  variant;
	uint16_t info;
	uint16_t pi_on;

	uint16_t avc;
	char     ps[9];
	uint8_t  af[2];
	uint16_t mf[5];
	uint16_t li;
	uint16_t pty;
	uint16_t pin;

} rds_gt14a_t;

int rds_parse_gt00a(uint16_t *prds, rds_gt00a_t *pgt);
int rds_parse_gt01a(uint16_t *prds, rds_gt01a_t *pgt);
int rds_parse_gt02a(uint16_t *prds, rds_gt02a_t *pgt);
int rds_parse_gt03a(uint16_t *prds, rds_gt03a_t *pgt);
int rds_parse_gt04a(uint16_t *prds, rds_gt04a_t *pgt);
int rds_parse_gt05a(uint16_t *prds, rds_gt05a_t *pgt);
int rds_parse_gt08a(uint16_t *prds, rds_gt08a_t *pgt);
int rds_parse_gt10a(uint16_t *prds, rds_gt10a_t *pgt);
int rds_parse_gt14a(uint16_t *prds, rds_gt14a_t *pgt);

#ifdef __cplusplus
}
#endif
#endif
