//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_util.c
//
//  Description: Utility functions
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
//		  Export Restrictions
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
//		 Webroot, Inc.
//       385 Interlocken Crescent
//      Broomfield, Colorado, USA 80021
//
//////////////////////////////////////////////////////////////////////////
#include "bc_util.h"
#include "bccsdk.h"
#include <stdio.h>

void bc_print_url_data(const struct bc_url_data* data) {
    printf("CC: %d,%d REP: %d A1: %d\n",
	   data->cc[0].category, data->cc[0].confidence, data->reputation,
	   (data->flags & BCA_FLAG_ONE_CAT) ? 1 : 0);
}

void bc_print_url_request(const struct bc_url_request* req) {
    printf("URL: %s LCP: %s ", req->url, req->lcp);
    bc_print_url_data(&req->data);
}

void bc_print_md5_data(const uint64_t key[2], const struct bc_url_data* data) {
    printf("MD5: %016llx%016llx ",
	   (unsigned long long)key[0], (unsigned long long)key[1]);
    bc_print_url_data(data);
}

void bc_print_md5_match(const char* url, const uint64_t key[2],
			const struct bc_url_data* data) {
    printf("URL: %s ", url);
    bc_print_md5_data(key, data);
}
