/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MATCHC_INCLUDED
#define MATCHC_INCLUDED

/* Character oriented match call */
unsigned char *matchc(int del_len, unsigned char *del_str, int src_len, unsigned char *src_str, int *res);
/* Byte oriented match call */
unsigned char *matchb(int del_len, unsigned char *del_str, int src_len, unsigned char *src_str, int *res);

#endif /* MATCHC_INCLUDED */
