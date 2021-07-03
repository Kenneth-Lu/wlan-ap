//////////////////////////////////////////////////////////////////////////
//
//  Filename: bc_string.h
//
//  Description: Optimized string functionality
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
#ifndef bc_string_h
#define bc_string_h
#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! cpystrn */
/*!
	Use this when you don't want a buffer overflow.
	Differences from strncpy 
	1. always null terminates
	2. does not pad destination with nulls
	3. returns pointer to NUL terminator of dst to allow truncation check
	\param dst Destination buffer
	\param src Source string
	\param dst_size The size of the destination buffer
*/
char* bc_cpystrn(char* dst, const char* src, size_t dst_size);

/*! strncpy - optimized strncpy function. */
/*!
	Use this when you need to write an exact number of characters
	Behaves like strncpy with the following differences
	1. does not zero fill the buffer
	2. return value is where the copy ended
	\param dst The destination buffer
	\param src The source string
	\param dst_size The size of the destination buffer
*/
char* bc_strncpy(char* dst, const char* src, size_t dst_size);

/*! Convert a string to lower case. */
/*!
	This function only changes upper case ASCII values.
	\param s The string to convert
	\param count The number of characters in the string to convert
*/
void bc_strtolower(char* s, size_t count);

/*! Convert string to lower case */
void bc_strlower(char* s);

#ifdef __cplusplus
}
#endif

#endif /* bc_string_h */


