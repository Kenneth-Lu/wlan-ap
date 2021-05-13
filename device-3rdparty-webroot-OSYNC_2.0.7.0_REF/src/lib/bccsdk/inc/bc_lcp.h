//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_lcp.h
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
#ifndef bc_lcp_h
#define bc_lcp_h
#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define LCP_POINTS_MAX 20

/*! bc_lcp_context data structure */
struct bc_lcp_context
{
	const char* url;
	const char* points[LCP_POINTS_MAX];
	const char* domainEnd;
	int   pointCount;
	int   nextLevel;
};

struct bc_init_params;

/*! initialize lcp */
int bc_lcp_init(const struct bc_init_params* ip);
/*! shutdown lcp */
void bc_lcp_shutdown(void);

/*! bc_parse_lcp */
/*!
	Initialize LCP (least common path) structure for taversing URL components
	Note: This code assumes that the URL has already been extracted and only
	contains the host name and path components. The lcp context holds pointers
	to elements in the url, so the url cannot be modified or deleted while
	the lcp context is being used.
	Returns the number of elements found, < 1 on error or nothing found.
*/
int bc_parse_lcp(struct bc_lcp_context* ctx, const char* url);

/*! bc_next_lcp */
/*!
	Copies the next smaller part of the url to dest.
*/
int bc_next_lcp(struct bc_lcp_context* ctx, char* dest, size_t dest_size);


#ifdef __cplusplus
}
#endif

#endif /* bc_lcp_h */


