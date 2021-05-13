//////////////////////////////////////////////////////////////////////////
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
#include "bc_tld.h"
#include "tld_names.h"
#include <stdlib.h>
#include <string.h>


static int str_compare(const void* key, const void* elem)
{
	const char* e = *((const char* const*) elem);
	const char* k = (const char*) key;
	return strcmp(k, e);
}

static int is_tld(const char* domain)
{
	return bsearch(domain, tld_names, tld_names_count, sizeof(char*), str_compare) != 0;
}

static int is_exclude(const char* domain)
{
	return bsearch(domain, tld_ex_names, tld_ex_names_count, sizeof(char*), str_compare) != 0;
}

static int is_wild(const char* domain)
{
	return bsearch(domain, tld_wc_names, tld_wc_names_count, sizeof(char*), str_compare) != 0;
}

// Return tld + 1 component of host name
const char* bc_tld_plus_one(const char* host)
{
	const char* dot = strchr(host, '.');
	if (!dot)
		return 0;
	const char* tld1 = host;
	const char* next = 0;
	++dot;
	while (dot && *dot)
	{
		if (is_exclude(tld1) || is_tld(dot))
			return tld1;
		next = dot;
		dot = strchr(dot, '.');
		if (dot)
		{
			++dot;
			if (is_wild(dot))
				return tld1;
		}
		tld1 = next;
	}	
	return 0;
}
	
// Return true if host is a top level domain
int bc_is_tld(const char* host)
{
	if (is_exclude(host))
		return 0;
	if (is_tld(host))
		return 1;
	const char* p = strchr(host, '.');
	if (p && is_wild(++p))
		return 1;
	return 0;
}



