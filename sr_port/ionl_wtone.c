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

#include "mdef.h"
#include "io.h"

void ionl_wtone(int v)
{
	mstr temp;
	char p;

	p = (char)v;
	temp.len = 1;
	temp.addr = &p;
	ionl_write(&temp);
	return;
}
