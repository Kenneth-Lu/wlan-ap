//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_string.c
//
//  Description: Optimized string functionality and linux/windows compatibility
//
//     history:
//
//	   date		 who	  what
//	   ====		 ===	  ====
//     11/15/13  TH		  ported to ISIS
//
//       CONFIDENTIAL and PROPRIETARY MATERIALS
//
//	This source code is covered by the Webroot Software Development
//	Kit End User License Agreement. Please read the terms of this
//    license before altering or copying the source code.  If you
//	are not willing to be bound by those terms, you may not view or
//	use this source code.
//
//	   	  Export Restrictions
//
//	This source code is subject to the U.S. Export Administration
//	Regulations and other U.S. laws, and may not be exported or
//	re-exported to certain countries (currently Cuba, Iran, Libya,
//	North Korea, Sudan and Syria) or to persons or entities
//	prohibited from receiving U.S. exports (including those (a)
//	on the Bureau of Industry and Security Denied Parties List or
//	Entity List, (b) on the Office of Foreign Assets Control list
//	of Specially Designated Nationals and Blocked Persons, and (c)
//	involved with missile technology or nuclear, chemical or
//	biological weapons).
//
//	   Copyright(c) 2006 - 2014
//	         Webroot, Inc.
//       385 Interlocken Crescent
//      Broomfield, Colorado, USA 80021
//
//////////////////////////////////////////////////////////////////////////
#include "bc_string.h"
#include <ctype.h>

char* bc_cpystrn(char* dst, const char* src, size_t dst_size)
{
	char* d;
	char* end;
	if (dst_size == 0) {
		return (dst);
	}
	d = dst;
	end = dst + dst_size - 1;

	for (; d < end; ++d, ++src)
	{
		if (!(*d = *src))
			return d;
	}
	*d = '\0';  /* always null terminate */
	return d;
}

char* bc_strncpy(char* dst, const char* src, size_t dst_size)
{
	char* d;
	char* end;
	if (dst_size == 0) {
		return (dst);
	}
	d = dst;
	end = dst + dst_size;

	for (; d < end; ++d, ++src)
	{
		if (!(*d = *src))
			return d;
	}
	return d;
}

void bc_strtolower(char* s, size_t count)
{
	char* e;
	if (s)
	{
		e = s + count;
		for ( ; *s && s < e; ++s)
		{
			if (*s >= 'A' && *s <= 'Z')
				*s += 32;
		}
	}
}

void bc_strlower(char* s)
{
	while (*s)
	{
		*s = tolower(*s);
		++s;
	}
}

