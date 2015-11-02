/****************************************************************
 *								*
 *	Copyright 2003, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#if defined(UNIX)
#include "gtm_unistd.h"
#include "eintr_wrappers.h"
#elif defined(VMS)
#include <rms.h>
#include <iodef.h>
#include <psldef.h>
#include <ssdef.h>
#include <efndef.h>
#include "iosb_disk.h"
#endif

#include "gdsroot.h"
#include "gtm_rename.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "iosp.h"
#include "jnl_typedef.h"
#include "gtmio.h"
#include "gtmmsg.h"
#include "wcs_flu.h"	/* for wcs_flu() prototype */

GBLREF	jnl_ctl_list		*mur_jctl;
GBLREF	reg_ctl_list		*mur_ctl;
GBLREF	int			mur_regno;
GBLREF	mur_gbls_t		murgbl;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	mur_opt_struct		mur_options;
GBLREF	mur_rab_t		mur_rab;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;

LITREF	int			jrt_update[];

uint4 mur_process_intrpt_recov()
{
	jnl_ctl_list		*jctl, *last_jctl;
	reg_ctl_list		*rctl, *rctl_top;
	int			rename_fn_len, save_name_len;
	char			prev_jnl_fn[MAX_FN_LEN + 1], rename_fn[MAX_FN_LEN + 1], save_name[MAX_FN_LEN + 1];
	jnl_create_info		jnl_info;
	uint4			status, status2;
	uint4			max_autoswitchlimit, max_jnl_alq, max_jnl_deq;
	sgmnt_data_ptr_t	csd;
#if defined(VMS)
	io_status_block_disk	iosb;
#endif
	boolean_t		jfh_changed, first_time;

	error_def(ERR_PREMATEOF); /* for DO_FILE_WRITE */
	error_def(ERR_JNLCREATERR);
	error_def(ERR_JNLWRERR);
	error_def(ERR_JNLFSYNCERR);
	error_def(ERR_TEXT);

	for (mur_regno = 0, rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++, mur_regno++)
	{
		gv_cur_region = rctl->gd;	/* mur_blocks_free, mur_master_map and wcs_flu require this to be set */
		cs_addrs = rctl->csa;		/* mur_master_map requires this is set */
		csd = cs_data = rctl->csd;	/* mur_blocks_free, mur_master_map require this to be set */
		assert(rctl->csd == rctl->csa->hdr);
		mur_jctl = jctl = rctl->jctl_turn_around;
		assert(rctl->csd == rctl->csa->hdr);
		max_jnl_alq = max_jnl_deq = max_autoswitchlimit = 0;
		for (last_jctl = NULL ; (NULL != jctl); last_jctl = jctl, jctl = jctl->next_gen)
		{
			if (max_autoswitchlimit < jctl->jfh->autoswitchlimit)
			{	/* Note that max_jnl_alq, max_jnl_deq are not the maximum journal allocation/extensions across
				 * generations, but rather the allocation/extension corresponding to the maximum autoswitchlimit.
				 */
				max_autoswitchlimit = jctl->jfh->autoswitchlimit;
				max_jnl_alq         = jctl->jfh->jnl_alq;
				max_jnl_deq         = jctl->jfh->jnl_deq;
			}
			/* Until now, "rctl->blks_to_upgrd_adjust" holds the number of V4 format newly created bitmap blocks
			 * seen in INCTN records in backward processing. It is possible that backward processing might have
			 * missed out on seeing those INCTN records which are part of virtually-truncated or completely-rolled-bak
			 * journal files. The journal file-header has a separate field "prev_recov_blks_to_upgrd_adjust" which
			 * maintains exactly this count. Therefore adjust the rctl counter accordingly.
			 */
			assert(!jctl->jfh->prev_recov_blks_to_upgrd_adjust || !jctl->jfh->recover_interrupted);
			assert(!jctl->jfh->prev_recov_blks_to_upgrd_adjust || jctl->jfh->prev_recov_end_of_data);
			rctl->blks_to_upgrd_adjust += jctl->jfh->prev_recov_blks_to_upgrd_adjust;
		}
		if (max_autoswitchlimit > last_jctl->jfh->autoswitchlimit)
		{
			csd->jnl_alq         = max_jnl_alq;
			csd->jnl_deq         = max_jnl_deq;
			csd->autoswitchlimit = max_autoswitchlimit;
		} else
		{
			assert(csd->jnl_alq         == last_jctl->jfh->jnl_alq);
			assert(csd->jnl_deq         == last_jctl->jfh->jnl_deq);
			assert(csd->autoswitchlimit == last_jctl->jfh->autoswitchlimit);
		}
		/* now that rctl->blks_to_upgrd_adjust is completely computed, use that to increment filehdr blks_to_upgrd.
		 * note that blks_to_upgrd should be adjusted before calling mur_master_map() as otherwise an assert
		 * in that routine might fail in case the counter is incorrect and a V4 format bitmap block is read in.
		 */
		csd->blks_to_upgrd += rctl->blks_to_upgrd_adjust;
		if (csd->blks_to_upgrd)
			csd->fully_upgraded = FALSE;
		mur_master_map();
		jctl = mur_jctl;
		csd->trans_hist.early_tn = csd->trans_hist.header_open_tn = jctl->turn_around_tn;
		csd->trans_hist.curr_tn = csd->trans_hist.early_tn;	/* INCREMENT_CURR_TN macro not used but noted in comment
									 * to identify all places that set curr_tn */
		/* MUPIP REORG UPGRADE/DOWNGRADE stores its partially processed state in the database file header.
		 * It is difficult for recovery to restore those fields to a correct partial value.
		 * Hence reset the related fields as if the desired_db_format got set just then at the EPOCH record
		 * 	and that there was no more processing that happened.
		 * This might potentially mean some duplicate processing for MUPIP REORG UPGRADE/DOWNGRADE after the recovery.
		 * But that will only be the case as long as the database is in compatibility (mixed) mode (hopefully not long).
		 */
		if (csd->desired_db_format_tn > jctl->turn_around_tn)
			csd->desired_db_format_tn = jctl->turn_around_tn;
		if (csd->reorg_db_fmt_start_tn > jctl->turn_around_tn)
			csd->reorg_db_fmt_start_tn = jctl->turn_around_tn;
		if (csd->tn_upgrd_blks_0 > jctl->turn_around_tn)
			csd->tn_upgrd_blks_0 = (trans_num)-1;
		csd->reorg_upgrd_dwngrd_restart_block = 0;
		csd->trans_hist.free_blocks = mur_blocks_free();
		if (dba_bg == csd->acc_meth)
			/* This is taken from bt_refresh() */
			((th_rec *)((uchar_ptr_t)cs_addrs->th_base + cs_addrs->th_base->tnque.fl))->tn = jctl->turn_around_tn - 1;
		wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_FSYNC_DB);
	}
	for (mur_regno = 0, rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++, mur_regno++)
	{
		if (!rctl->jfh_recov_interrupted)
			jctl = rctl->jctl_turn_around;
		else
		{
			DEBUG_ONLY(
				for (jctl = rctl->jctl_turn_around; NULL != jctl->next_gen; jctl = jctl->next_gen)
					;
				/* check that latest gener file name does not match db header */
				assert((rctl->csd->jnl_file_len != jctl->jnl_fn_len)
					|| (0 != memcmp(rctl->csd->jnl_file_name, jctl->jnl_fn, jctl->jnl_fn_len)));
			)
			jctl = rctl->jctl_alt_head;
		}
		assert(NULL != jctl);
		for ( ; NULL != jctl->next_gen; jctl = jctl->next_gen)
			;
		assert(rctl->csd->jnl_file_len == jctl->jnl_fn_len); 			       /* latest gener file name */
		assert(0 == memcmp(rctl->csd->jnl_file_name, jctl->jnl_fn, jctl->jnl_fn_len)); /* should match db header */
		if (SS_NORMAL != (status = prepare_unique_name((char *)jctl->jnl_fn, jctl->jnl_fn_len, "", "",
								rename_fn, &rename_fn_len, &status2)))
			return status;
		jctl->jnl_fn_len = rename_fn_len;  /* change the name in memory to the proposed name */
		memcpy(jctl->jnl_fn, rename_fn, rename_fn_len + 1);
		/* Rename hasn't happened yet at the filesystem level. In case current recover command is interrupted,
		 * we need to update jfh->next_jnl_file_name before mur_forward(). Update jfh->next_jnl_file_name for
		 * all journal files from which PBLK records were applied. Create new journal files for forward play.
		 */
		assert(NULL != rctl->jctl_turn_around);
		jctl = rctl->jctl_turn_around; /* points to journal file which has current recover's turn around point */
		assert(0 != jctl->turn_around_offset);
		jctl->jfh->turn_around_offset = jctl->turn_around_offset;	/* save progress in file header for 	*/
		jctl->jfh->turn_around_time = jctl->turn_around_time;		/* possible re-issue of recover 	*/
		jfh_changed = TRUE;
		DEBUG_ONLY(first_time = TRUE;)
		for ( ; NULL != jctl; jctl = jctl->next_gen)
		{	/* setup the next_jnl links. note that in the case of interrupted recovery, next_jnl links
			 * would have been already set starting from the turn-around point journal file of the
			 * interrupted recovery but the new recovery MIGHT have taken us to a still previous
			 * generation journal file that needs its next_jnl link set. this is why we do the next_jnl
			 * link setup even in the case of interrupted recovery although in most cases it is unnecessary.
			 */
			if (NULL != jctl->next_gen)
			{
				jctl->jfh->next_jnl_file_name_length = jctl->next_gen->jnl_fn_len;
				memcpy(jctl->jfh->next_jnl_file_name, jctl->next_gen->jnl_fn, jctl->next_gen->jnl_fn_len);
				jfh_changed = TRUE;
			} else
				assert(0 == jctl->jfh->next_jnl_file_name_length); /* null link from latest generation */
			if (jctl->jfh->turn_around_offset && (jctl != rctl->jctl_turn_around))
			{	/* it is possible that the current recovery has a turn-around-point much before the
				 * previously interrupted recovery. if it happens to be a previous generation journal
				 * file then we have to reset the original turn-around-point to be zero in the journal
				 * file header in order to ensure if this recovery gets interrupted we do interrupted
				 * recovery processing until the new turn-around-point instead of stopping incorrectly
				 * at the original turn-around-point itself.
				 */
				assert(!jctl->turn_around_offset);
				assert(rctl->recov_interrupted);	/* rctl->jfh_recov_interrupted can fail */
				assert(first_time);	/* there can't be two journal files after the current
							 * turn-around-point with a non-zero turn-around-point */
				DEBUG_ONLY(first_time = FALSE;)
				jctl->jfh->turn_around_offset = 0;
				jctl->jfh->turn_around_time = 0;
				jfh_changed = TRUE;
			}
			if (jfh_changed)
			{
				DO_FILE_WRITE(jctl->channel, 0, jctl->jfh, JNL_HDR_LEN, jctl->status, jctl->status2);
				if (SS_NORMAL != jctl->status)
				{
					if (SS_NORMAL == jctl->status2)
						gtm_putmsg(VARLSTCNT(5) ERR_JNLWRERR, 2, jctl->jnl_fn_len,
							jctl->jnl_fn, jctl->status);
					else
						gtm_putmsg(VARLSTCNT1(6) ERR_JNLWRERR, 2, jctl->jnl_fn_len,
							jctl->jnl_fn, jctl->status, PUT_SYS_ERRNO(jctl->status2));
					return jctl->status;
				}
				UNIX_ONLY(
				GTM_FSYNC(jctl->channel, jctl->status);
				if (-1 == jctl->status)
				{
					jctl->status2 = errno;
					assert(FALSE);
					gtm_putmsg(VARLSTCNT(9) ERR_JNLFSYNCERR, 2,
						jctl->jnl_fn_len, jctl->jnl_fn,
						ERR_TEXT, 2, RTS_ERROR_TEXT("Error with fsync"), jctl->status2);
					return ERR_JNLFSYNCERR;
				}
				)
			}
			jfh_changed = FALSE;
		}
		memset(&jnl_info, 0, sizeof(jnl_info));
		jnl_info.status = jnl_info.status2 = SS_NORMAL;
		jnl_info.prev_jnl = &prev_jnl_fn[0];
		set_jnl_info(rctl->gd, &jnl_info);
		jnl_info.prev_jnl_len = rctl->jctl_turn_around->jnl_fn_len;
		memcpy(jnl_info.prev_jnl, rctl->jctl_turn_around->jnl_fn, rctl->jctl_turn_around->jnl_fn_len);
		jnl_info.prev_jnl[jnl_info.prev_jnl_len] = 0;
		jnl_info.jnl_len = rctl->csd->jnl_file_len;
		memcpy(jnl_info.jnl, rctl->csd->jnl_file_name, jnl_info.jnl_len);
		jnl_info.jnl[jnl_info.jnl_len] = 0;
		assert(!mur_options.rollback || jgbl.mur_rollback);
		jnl_info.reg_seqno = rctl->jctl_turn_around->turn_around_seqno;
		jgbl.gbl_jrec_time = rctl->jctl_turn_around->turn_around_time;	/* time needed for cre_jnl_file_common() */
		if (EXIT_NRM != cre_jnl_file_common(&jnl_info, rename_fn, rename_fn_len))
		{
			gtm_putmsg(VARLSTCNT(4) ERR_JNLCREATERR, 2, jnl_info.jnl_len, jnl_info.jnl);
			return jnl_info.status;
		}
		if (NULL != rctl->jctl_alt_head) /* remove the journal files created by last interrupted recover process */
		{
			mur_rem_jctls(rctl);
			rctl->jctl_alt_head = NULL;
		}
		/* From this point on, journal records are written into the newly created journal file. However, we still read
		 * from old journal files.
		 */
	}
	jgbl.gbl_jrec_time = 0;	/* set to a safe value */
	return SS_NORMAL;
}
