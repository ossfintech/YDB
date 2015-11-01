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

/* gvcst_bmp_mark_free.c
	This marks all the blocks in kill set list to be marked free.
	Note ks must be already sorted
*/
#include "mdef.h"

#include "gtm_string.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "memcoherency.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_end.h"
#include "t_retry.h"
#include "t_begin.h"
#include "t_write_map.h"
#include "mm_read.h"
#include "add_inter.h"
#include "gvcst_bmp_mark_free.h"

GBLREF	char			*update_array, *update_array_ptr;
GBLREF	cw_set_element		cw_set[];
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	unsigned char		rdfail_detail;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	boolean_t		mu_reorg_process;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	inctn_detail_t		inctn_detail;			/* holds detail to fill in to inctn jnl record */
GBLREF	short			dollar_tlevel;

trans_num gvcst_bmp_mark_free(kill_set *ks)
{
	block_id	bit_map, next_bm;
	blk_ident	*blk, *blk_top, *nextblk;
	trans_num	ctn, start_db_fmt_tn;
	unsigned int	len;
	int4		blk_prev_version;
	srch_hist	alt_hist;
	trans_num	ret_tn = 0;
	boolean_t	visit_blks;
	srch_blk_status	bmphist;
	cache_rec_ptr_t	cr;

	error_def(ERR_GVKILLFAIL);

	assert(inctn_bmp_mark_free_gtm == inctn_opcode || inctn_bmp_mark_free_mu_reorg == inctn_opcode);
	/* Note down the desired_db_format_tn before you start relying on cs_data->fully_upgraded.
	 * If the db is fully_upgraded, take the optimal path that does not need to read each block being freed.
	 * But in order to detect concurrent desired_db_format changes, note down the tn (when the last format change occurred)
	 * 	before the fully_upgraded check	and after having noted down the database current_tn.
	 * If they are the same, then we are guaranteed no concurrent desired_db_format change occurred.
	 * If they are not, then fall through to the non-optimal path where each to-be-killed block has to be visited.
	 * The reason we need to visit every block in case desired_db_format changes is to take care of the case where
	 *	MUPIP REORG DOWNGRADE concurrently changes a block that we are about to free.
	 */
	start_db_fmt_tn = cs_data->desired_db_format_tn;
	visit_blks = (!cs_data->fully_upgraded);	/* Local evaluation */
	assert(!visit_blks || (visit_blks && dba_bg == cs_addrs->hdr->acc_meth)); /* must have blks_to_upgrd == 0 for non-BG */
	assert(0 == dollar_tlevel); 			/* Should NOT be in TP now */
	blk = &ks->blk[0];
	blk_top = &ks->blk[ks->used];
	if (!visit_blks)
	{	/* database has been completely upgraded. free all blocks in one bitmap as part of one transaction */
		inctn_detail.blknum = 0; /* to indicate no adjustment to "blks_to_upgrd" necessary */
		for ( ; blk < blk_top;  blk = nextblk)
		{
			if (0 != blk->flag)
			{
				nextblk = blk + 1;
				continue;
			}
			assert(0 < blk->block);
			assert((int4)blk->block < cs_addrs->ti->total_blks);
			bit_map = ROUND_DOWN2((int)blk->block, BLKS_PER_LMAP);
			next_bm = bit_map + BLKS_PER_LMAP;
			/* Scan for the next local bitmap */
			for (nextblk = blk;
				(0 == nextblk->flag) && (nextblk < blk_top) && ((block_id)nextblk->block < next_bm);
				++nextblk)
				;
			update_array_ptr = update_array;
			len = (char *)nextblk - (char *)blk;
			memcpy(update_array_ptr, blk, len);
			update_array_ptr += len;
			alt_hist.h[0].blk_num = 0;			/* need for calls to T_END for bitmaps */
			/* the following assumes sizeof(blk_ident) == sizeof(int) */
			assert(sizeof(blk_ident) == sizeof(int));
			*(int *)update_array_ptr = 0;
			t_begin(ERR_GVKILLFAIL, TRUE);
			for (;;)
			{
				ctn = cs_addrs->ti->curr_tn;
				/* Need a read fence before reading fields from cs_data as we are reading outside
				 * of crit and relying on this value to detect desired db format state change.
				 */
				SHM_READ_MEMORY_BARRIER;
				if (start_db_fmt_tn != cs_data->desired_db_format_tn)
				{	/* Concurrent db format change has occurred. Need to visit every block to be killed
					 * to determine its block format. Fall through to the non-optimal path below
					 */
					ret_tn = 0;
					break;
				}
				bmphist.blk_num = bit_map;
				if (NULL == (bmphist.buffaddr = t_qread(bmphist.blk_num, (sm_int_ptr_t)&bmphist.cycle,
									&bmphist.cr)))
				{
					t_retry(rdfail_detail);
					continue;
				}
				t_write_map(&bmphist, (uchar_ptr_t)update_array, ctn);
				if ((trans_num)0 == (ret_tn = t_end(&alt_hist, NULL)))
					continue;
				break;
			}
			if (0 == ret_tn) /* db format change occurred. Fall through to below for loop to visit each block */
				break;
		}
	}	/* for all blocks in the kill_set */
	for ( ; blk < blk_top; blk++)
	{	/* Database has NOT been completely upgraded. Have to read every block that is going to be freed
		 * and determine whether it has been upgraded or not. Every block will be freed as part of one
		 * separate update to the bitmap. This will cause as many transactions as the blocks are being freed.
		 * But this overhead will be present only as long as the database is not completely upgraded.
		 * The reason why every block is updated separately is in order to accurately maintain the "blks_to_upgrd"
		 * counter in the database file-header when the block-freeup phase (2nd phase) of the M-kill proceeds
		 * concurrently with a MUPIP REORG UPGRADE/DOWNGRADE. If the bitmap is not updated for every block freeup
		 * then MUPIP REORG UPGRADE/DOWNGRADE should also upgrade/downgrade all blocks in one bitmap as part of
		 * one transaction (only then will we avoid double-decrement of "blks_to_upgrd" counter by the M-kill as
		 * well as the MUPIP REORG UPGRADE/DOWNGRADE). That is a non-trivial task as potentially 512 blocks need
		 * to be modified as part of one non-TP transaction which is unnecessarily making it heavyweight. Compared
		 * to that, incurring a per-block bitmap update overhead in the M-kill is considered acceptable since this
		 * will be the case only as long as we are in compatibility mode which should be hopefully not for long.
		 */
		if (0 != blk->flag)
			continue;
		assert(0 < blk->block);
		assert((int4)blk->block < cs_addrs->ti->total_blks);
		assert(!IS_BITMAP_BLK(blk->block));
		bit_map = ROUND_DOWN2((int)blk->block, BLKS_PER_LMAP);
		assert(dba_bg == cs_addrs->hdr->acc_meth);
		/* We need to check each block we are deleting to see if it is in the format of a previous version.
		 * If it is, then "csd->blks_to_upgrd" needs to be correspondingly adjusted.
		 */
		alt_hist.h[0].level = 0;	/* Initialize for loop below */
		alt_hist.h[1].blk_num = 0;
		update_array_ptr = update_array;
		*((blk_ident *)update_array_ptr) = *blk;
		update_array_ptr += sizeof(blk_ident);
		/* the following assumes sizeof(blk_ident) == sizeof(int) */
		assert(sizeof(blk_ident) == sizeof(int));
		*(int *)update_array_ptr = 0;
		t_begin(ERR_GVKILLFAIL, TRUE);
		for (;;)
		{
			ctn = cs_addrs->ti->curr_tn;
			alt_hist.h[0].tn      = ctn;
			alt_hist.h[0].blk_num = blk->block;
			if (NULL == (alt_hist.h[0].buffaddr = t_qread(alt_hist.h[0].blk_num,
								      (sm_int_ptr_t)&alt_hist.h[0].cycle,
								      &alt_hist.h[0].cr)))
			{
				t_retry(rdfail_detail);
				continue;
			}
			cr = alt_hist.h[0].cr;
			assert((GDSV5 == cr->ondsk_blkver) || (GDSV4 == cr->ondsk_blkver));
			if (GDSVCURR != cr->ondsk_blkver)
				inctn_detail.blknum = blk->block;
			else
				inctn_detail.blknum = 0; /* to indicate no adjustment to "blks_to_upgrd" necessary */
			bmphist.blk_num = bit_map;
			if (NULL == (bmphist.buffaddr = t_qread(bmphist.blk_num, (sm_int_ptr_t)&bmphist.cycle,
								&bmphist.cr)))
			{
				t_retry(rdfail_detail);
				continue;
			}
			t_write_map(&bmphist, (uchar_ptr_t)update_array, ctn);
			if ((trans_num)0 == (ret_tn = t_end(&alt_hist, NULL)))
				continue;
			break;
		}
	}	/* for all blocks in the kill_set */
	return ret_tn;
}