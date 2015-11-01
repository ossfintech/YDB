/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "interlock.h"
#include "lockconst.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "copy.h"
#include "performcaslatchcheck.h"
#include "wcs_backoff.h"
#include "add_inter.h"

GBLREF	int4		process_id;
GBLREF	gd_region	*gv_cur_region;
GBLREF	volatile int4	fast_lock_count;		/* Used in wcs_stale */
GBLREF	int		num_additional_processors;

int4	add_inter(int val, sm_int_ptr_t addr, sm_global_latch_ptr_t latch)
{
	int4	ret, tries, spins, maxspin;
	error_def(ERR_DBCCERR);
	error_def(ERR_ERRCALL);

	++fast_lock_count;
	maxspin = num_additional_processors ? LOCK_SPINS : 1;
	for (tries = 0;  LOCK_TRIES > tries; tries++)
	{
		for (spins = maxspin; 0 < spins; spins--)
		{
			if (GET_SWAPLOCK(latch))
			{
				*addr += val;
				ret = *addr;
				RELEASE_SWAPLOCK(latch);
				--fast_lock_count;
				assert(0 <= fast_lock_count);
				return ret;
			}
		}
		if (0 != tries)
			wcs_backoff(tries);
		performCASLatchCheck(latch, tries);
	}
	--fast_lock_count;
	DUMP_LOCKHIST();
	rts_error(VARLSTCNT(9) ERR_DBCCERR, 2, LEN_AND_LIT("*unknown*"), ERR_ERRCALL, 3, CALLFROM);
}


