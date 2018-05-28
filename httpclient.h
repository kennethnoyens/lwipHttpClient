/*
	HTTP CLIENT FOR RAW LWIP
	(c) 2008-2009 Noyens Kenneth
	PUBLIC VERSION V0.2 16/05/2009
 
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU LESSER GENERAL PUBLIC LICENSE Version 2.1 as published by
	the Free Software Foundation.
 
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.
 
	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the
	Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __HTTPCLIENT_H
#define __HTTPCLIENT_H

#include "utils/lwiplib.h"

#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "ethernet.h"


// You can replace this enum for saving MEMORY (replace with define's)
typedef enum
{
	OK,
	OUT_MEM,
	TIMEOUT,
	NOT_FOUND,
	GEN_ERROR
} hc_errormsg;

struct hc_state {
  u8_t Num;
  char *Page;
  char *PostVars;
  char *RecvData;
  u16_t Len;
  u8_t ConnectionTimeout;
  void (* ReturnPage)(u8_t num, hc_errormsg, char *data, u16_t len);
};

// Public function
int hc_open(struct ip_addr, char *, char *, void (*)(u8_t, hc_errormsg, char *, u16_t));

#endif //  __HTTPCLIENT_H
