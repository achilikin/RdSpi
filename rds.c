/*  Some of RBDS standard functions
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
#include <ctype.h>
#include <string.h>

#include "rds.h"

#ifndef _BM
#define _BM(bit) (1 << ((uint16_t)bit)) // convert bit number to bit mask
#endif

#define _BM(bit) (1 << ((uint16_t)bit)) // convert bit number to bit mask
const char *rds_gt_00  = "Basic tuning and switching information";
const char *rds_gt_01  = "Program-item number and slow labeling codes";
const char *rds_gt_02  = "Radiotext";
const char *rds_gt_03A = "Applications Identification for Open Data";
const char *rds_gt_03B = "Open data application";
const char *rds_gt_04A = "Clock-time and date";
const char *rds_gt_04B = "Open data application";
const char *rds_gt_05  = "Transparent data channels or ODA";
const char *rds_gt_06  = "In house applications or ODA";
const char *rds_gt_07A = "Radio paging or ODA";
const char *rds_gt_07B = "Open data application";
const char *rds_gt_08  = "Traffic Message Channel or ODA";
const char *rds_gt_09  = "Emergency warning systems or ODA";
const char *rds_gt_10A = "Program Type Name";
const char *rds_gt_10B = "Open data application";
const char *rds_gt_11  = "Open data application";
const char *rds_gt_12  = "Open data application";
const char *rds_gt_13A = "Enhanced Radio paging or ODA";
const char *rds_gt_13B = "Open data application";
const char *rds_gt_14  = "Enhanced Other Networks information";
const char *rds_gt_15A = "Fast basic tuning and switching information (phased out)";
const char *rds_gt_15B = "Fast tuning and switching information";

static const char *rds_gt_names[] = {
	rds_gt_00,  rds_gt_00,
	rds_gt_01,  rds_gt_01,
	rds_gt_02,  rds_gt_02,
	rds_gt_03A, rds_gt_03B,
	rds_gt_04A, rds_gt_04B,
	rds_gt_05,  rds_gt_05,
	rds_gt_06,  rds_gt_06,
	rds_gt_07A, rds_gt_07B,
	rds_gt_08,  rds_gt_08,
	rds_gt_09,  rds_gt_09,
	rds_gt_10A, rds_gt_10B,
	rds_gt_11,  rds_gt_11,
	rds_gt_12,  rds_gt_12,
	rds_gt_13A, rds_gt_13B,
	rds_gt_14,  rds_gt_14,
	rds_gt_15A, rds_gt_15B
};

const char *rds_gt_name(uint8_t group, uint8_t version)
{
	int idx = (group & 0x0F)*2 + (version & 0x01);
	return rds_gt_names[idx];
}

static char *rds_swap16(void *ptr)
{
	char *p8 = (char *)ptr;
	char tmp = p8[0];
	p8[0] = p8[1];
	p8[1] = tmp;

	return (char *)ptr;
}

static int rds_add_af(rds_gt00a_t *dst, uint8_t *paf)
{
	if (paf[1] == 250) // skip LF/MF for now
		return 0;

	for(int n = 0; n < 2; n++) {
		if (paf[n] >= 225 && paf[n] <= 249) { // number of af
			dst->naf = paf[n] - 224;
			memset(dst->af, 0, sizeof(dst->af));
			continue;
		}
		if (!paf[n] || paf[n] > 204)
			continue;
		for(int i = 0; i < 25; i++) {
			if (dst->af[i] == 0)
				dst->af[i] = paf[n];
			if (dst->af[i] == paf[n])
				break;
		}
	}
	return 0;
}

int rds_parse_gt00a(uint16_t *prds, rds_gt00a_t *pgt)
{
	uint8_t ci = (prds[RDS_B] & 0x03);
	pgt->ta = !!(prds[RDS_B] & RDS_TA); // Traffic Announcement
	pgt->ms = (prds[RDS_B] & RDS_MS) ? 'M' : 'S'; // Music/Speech

	// Decoder control bit
	if (prds[RDS_B] & RDS_DI)
		pgt->di |= _BM(3-ci);
	else
		pgt->di &= ~_BM(3-ci);

	pgt->ci = ci;
	pgt->valid |= 1 << ci;

	if (!(prds[RDS_B] & RDS_VER)) {
		uint8_t *paf = (uint8_t *)rds_swap16(&prds[RDS_C]);
		rds_add_af(pgt, paf);
	}

	ci <<= 1;
	if (pgt->ps[0] == '\0') {
		memset(pgt->ps, ' ', 8);
		pgt->ps[8] = '\0';
	}

	char *pchar = rds_swap16(&prds[RDS_D]);
	for(int i = 0; i < 2; i++) {
		if (isalnum(pchar[i]) || pchar[i] == ' ')
			pgt->ps[ci+i] = pchar[i];
	}

	return 0;
}

int rds_parse_gt01a(uint16_t *prds, rds_gt01a_t *pgt)
{
	pgt->rpc  = prds[RDS_B] & 0x1F;
	pgt->la   = !!(prds[RDS_C] & 0x8000);
	pgt->vc   = (prds[RDS_C] >> 12) & 0x07;
	pgt->slc  = prds[RDS_C]& 0x0FFF;
	pgt->pinc = prds[RDS_D]& 0x0FFF;
	return 0;
}

int rds_parse_gt02a(uint16_t *prds, rds_gt02a_t *pgt)
{
	uint8_t ab = !!(prds[RDS_B] & RDS_AB);
	uint8_t si = (prds[RDS_B] & 0x0F);
	
	if (pgt->rt[0] == '\0' || pgt->ab != ab) {
		memset(pgt->rt, ' ', 64);
		pgt->rt[64] = '\0';
	}
	
	pgt->ab = ab;
	pgt->si = si;
	pgt->valid |= 1 << si;
	si *= 4;

	char *pchar = rds_swap16(&prds[RDS_C]);
	rds_swap16(&prds[RDS_D]);

	for (int i = 0; i < 4; i++) {
		uint8_t ch = pchar[i];
		if (isalnum(ch) || ch == ' ') {
			if (ch == '\r') {
				pgt->valid = 0xFFFF;
				ch = '^'; 
			}
			pgt->rt[si + i] = ch;
		}
	}

	return 0;
}

int rds_parse_gt03a(uint16_t *prds, rds_gt03a_t *pgt)
{
	pgt->agtc = (prds[RDS_B] >> 1) & 0x0F;
	pgt->ver  = prds[RDS_B] & 0x01;
	uint16_t msg = prds[RDS_C];
	pgt->msg = msg;
	pgt->aid = prds[RDS_D];

	pgt->vc = msg >> 14;
	if (pgt->vc == 0) {
		pgt->ltn = (msg >> 6) & 0x3F;
		pgt->afi = !!(msg & 0x20);
		pgt->m   = !!(msg & 0x10);
		pgt->i   = !!(msg & 0x08);
		pgt->n   = !!(msg & 0x04);
		pgt->r   = !!(msg & 0x02);
		pgt->u   = !!(msg & 0x01);
	}
	else {
		pgt->g   = (msg >> 12) & 0x03;
		pgt->sid = (msg >> 6) & 0x3F;
		pgt->ta  = (msg >> 4) & 0x03;
		pgt->tw  = (msg >> 2) & 0x03;
		pgt->td  = (msg >> 0) & 0x03;
	}
	return 0;
}

int rds_parse_gt04a(uint16_t *prds, rds_gt04a_t *pgt)
{
	uint8_t hour = (prds[RDS_D] >> 12) & 0x0F;
	hour |= (prds[RDS_C] & 0x01) << 4;
	if (hour > 23)
		return -1;
	uint8_t minute = (prds[RDS_D] >> 6) & 0x3F;
	if (minute > 59)
		return -1;
	int8_t tz = prds[RDS_D] & 0x0F;

	uint32_t date = (prds[RDS_C] >> 1);
	date |= (prds[RDS_B] & 0x03) << 15;
	int y = (int)(((double)date - 15078.2)/365.25);
	int m = (int)((((double)date - 14956.1) - (int)(y*365.25))/30.6001);
	int d = date - 14956 - (int)(y*365.25) - (int)(m*30.60001);
	int k = (m == 14 || m == 15) ? 1 : 0;
	y += k;
	m = m - 1 - k*12;

	pgt->hour    = hour;
	pgt->minute  = minute;
	pgt->tz_hour = tz >> 1;
	pgt->tz_half = tz &0x01;
	pgt->ts_sign = prds[RDS_D] & _BM(5);
	pgt->day = d;
	pgt->month = m;
	pgt->year  = y + 1900;

	return 0;
}

int rds_parse_gt08a(uint16_t *prds, rds_gt08a_t *pgt)
{
	if (pgt->spn[0] == '\0') {
		memset(pgt->spn, ' ', 8);
		pgt->spn[8] = '\0';
	}

	pgt->x4 = !!(prds[RDS_B] & 0x10);
	pgt->vc = (prds[RDS_B] & 0x0F);

	pgt->x3 = !!(prds[RDS_B] & 0x08);
	pgt->x2 = !!(prds[RDS_B] & 0x04);

	pgt->y = prds[RDS_C];
	pgt->d = !!(prds[RDS_C] & 0x8000);
	pgt->dir = !!(prds[RDS_C] & 0x4000);
	pgt->ext = (prds[RDS_C] >> 11) & 0x07;
	pgt->eve = prds[RDS_C] & 0x7FF;

	pgt->loc = prds[RDS_D];

	return 0;
}

int rds_parse_gt10a(uint16_t *prds, rds_gt10a_t *pgt)
{
	uint8_t ab = !!(prds[RDS_B] & RDS_AB);
	uint8_t ci = (prds[RDS_B] & 0x01);

	if (pgt->ps[0] == '\0' || pgt->ab != ab) {
		memset(pgt->ps, ' ', 8);
		pgt->ps[8] = '\0';
	}
	pgt->ci = ci;
	pgt->ab = ab;

	ci *= 4;
	char *pchar = rds_swap16(&prds[RDS_C]);
	rds_swap16(&prds[RDS_D]);
	for(int i = 0; i < 4; i++) {
		if (isalnum(pchar[i]) || pchar[i] == ' ')
		pgt->ps[ci+i] = pchar[i];
	}
	return 0;
}

int rds_parse_gt14a(uint16_t *prds, rds_gt14a_t *pgt)
{
	char *pchar = rds_swap16(&prds[RDS_C]);

	if (pgt->pi_on != prds[RDS_D]) {
		rds_hdr_t hdr;
		memcpy(&hdr, &pgt->hdr, sizeof(hdr));
		memset(pgt, 0, sizeof(rds_gt14a_t));
		memset(pgt->ps, ' ', 8);
		memcpy(&pgt->hdr, &hdr, sizeof(hdr));
	}

	pgt->tp_on   = !!(prds[RDS_B] & 0x10);
	pgt->variant = prds[RDS_B] & 0x0F;
	pgt->info    = prds[RDS_C];
	pgt->pi_on   = prds[RDS_D];

	pgt->avc |= 1 << pgt->variant;

	if (pgt->variant < 4) {
		for(int i = 0; i <2; i++) {
			if (isalnum(pchar[i]) || pchar[i] == ' ')
			pgt->ps[pgt->variant*2+i] = pchar[i];
		}
		return 0;
	}
	if (pgt->variant == 4) {
		pgt->af[0] = pchar[1];
		pgt->af[1] = pchar[0];
		return 0;
	}
	if (pgt->variant < 10) {
		pgt->mf[pgt->variant - 5] = prds[RDS_C];
		return 0;
	}
	if (pgt->variant == 12) {
		pgt->li = prds[RDS_C];
		return 0;
	}
	if (pgt->variant == 13) {
		pgt->pty = prds[RDS_C];
		return 0;
	}
	if (pgt->variant == 14) {
		pgt->pin = prds[RDS_C];
		return 0;
	}
	return 0;
}
