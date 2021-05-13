//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_alloc.c
//
//  Description: Malloc wrapper
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
#include "bc_alloc.h"

//#define BCA_CUSTOM_ALLOC
#ifdef BCA_CUSTOM_ALLOC
void* bc_malloc(size_t size)
{
    /* custom code here */
    return 0;
}

void bc_free(void* ptr)
{
    /* custom code here */
}

void* bc_calloc(size_t nelem, size_t size)
{
    /* custom code here */
    return 0;
}

#else
#include <stdlib.h>

void* bc_malloc(size_t size)
{
    return malloc(size);
}

void bc_free(void* ptr)
{
    free(ptr);
}

void* bc_calloc(size_t nelem, size_t size)
{
    return calloc(nelem, size);
}

#endif
