//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_url.h
//
//  Description: URL parsing
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
#ifndef bc_url_h
#define bc_url_h
#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! bc_extract_url */
/*!
	Parse URL into host and path components suitable for a bc query
	Removes everything but the host and path parts, strips off "www." and
	returns the combined string.
	in = http://user:password@www.foo.com:80/path?query=blah
	out = foo.com/path
*/
int bc_extract_url(char* dest, const char* url, size_t n);
int bc_extract_host(char* dest, const char* url, size_t n);


#ifdef __cplusplus
}
#endif

#endif /* bc_url_h */


