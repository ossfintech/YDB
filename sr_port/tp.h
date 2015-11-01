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

#ifndef __TP_H__
#define __TP_H__

#include <sys/types.h>

/* HEADER-FILE-DEPENDENCIES : hashtab.h, buddy_list.h */

#define JNL_LIST_INIT_ALLOC		16		/* initial allocation for si->jnl_list */
#define	CW_SET_LIST_INIT_ALLOC		64		/* initial allocation for si->cw_set_list */
#define	TLVL_CW_SET_LIST_INIT_ALLOC     64		/* initial allocation for si->tlvl_cw_set_list */
#define	NEW_BUFF_LIST_INIT_ALLOC       	 8		/* initial allocation for si->new_buff_list */
#define	RECOMPUTE_LIST_INIT_ALLOC	 8		/* initial allocation for si->recompute_list */
#define	BLKS_IN_USE_INIT_ELEMS		64		/* initial no. of elements in hash table si->blks_in_use */
#define	TLVL_INFO_LIST_INIT_ALLOC	 4		/* initial allocation for si->tlvl_info_head */
#define	GBL_TLVL_INFO_LIST_INIT_ALLOC	 8		/* initial allocation for global_tlvl_info_head (one per region) */

#define	INIT_CUR_TP_HIST_SIZE		64 		/* initial value of si->cur_tp_hist_size */
#define TP_MAX_MM_TRANSIZE		64*1024
#define JNL_FORMAT_BUFF_INIT_ALLOC	16*1024

#define TP_BATCH_ID		"BATCH"
#define TP_BATCH_LEN		sizeof(TP_BATCH_ID)
#define TP_BATCH_SHRT		2	/* permit abbreviation to two characters */
#define TP_DEADLOCK_FACTOR	5	/* multiplied by dollar_trestart to produce an argument for wcs_backoff */
#define MAX_VISIBLE_TRESTART	4	/* Per Bhaskar on 10/20/98: dollar_trestart is not allowed to visibly exceed 4
					 * because of errors this causes in legacy Profile versions (any < 6.1)        */

/* structure to hold transaction level specific info per segment.
 * Aids in incremental rollback, to identify the state of various elements at the BEGINNING of a new transaction level. */

typedef struct tlevel_info_struct
{
	que_ent		free_que;
	struct tlevel_info_struct
			*next_tlevel_info;
	kill_set	*tlvl_kill_set;		/* these two fields hold the kill_set state before this t_level started */
	int4		tlvl_kill_used;
	jnl_format_buffer
			*tlvl_jfb_info;		/* state of the tp_jnl_format_buff_info list before this t_level started */
	uint4		tlvl_cumul_jrec_len,
			tlvl_cumul_index;	/* tlvl_cumul_index is maintained only #ifdef DEBUG (similar to cumul_index) */
	srch_blk_status	*tlvl_tp_hist_info;	/* state of the tp_hist array (tail) before this t_level started */
	short		t_level;
} tlevel_info;

/* structure to hold the global (across all segments) dollar_tlevel specific information.
 * To identify the state of various elements at the BEGINNING of a given transaction level.
 */

typedef struct global_tlvl_info_struct
{
	struct global_tlvl_info_struct
			*next_global_tlvl_info;
	sgmnt_addrs	*global_tlvl_fence_info;
	short		t_level;
} global_tlvl_info;

/* A note on the buddy lists used in sgm_info structure,
 *	cw_set_list		->	uses get_new_element() and free_last_n_elements()
 *	tlvl_cw_set_list	-> 	uses get_new_free_element() and free_element()
 *	jnl_list		->	uses get_new_element() and free_last_n_elements()
 *	tlvl_info_list		->	uses get_new_element() and free_last_n_elements()
 *	gbl_tlvl_info_list	->	uses get_new_element() and free_last_n_elements()
 *	new_buff_list		->	uses get_new_free_element() and free_element()
 *	recompute_list		->	uses get_new_element()
 */

/* A small comment about the tp_hist_size and cur_tp_hist_size members of the sgm_info structure.
 * Currently, we allow a maximum of 64k (TP_MAX_MM_TRANSIZE) blocks in the read-set of a TP transaction and n_bts/2 blocks
 * in the write-set. Each block in the read-set has a corresponding srch_blk_status structure used in final validation in tp_tend.
 * The current code requires the read-set to be a contiguous array of srch_blk_status structures for performance considerations.
 * A process may need anywhere from 1 to 64K elements of this array depending on the TP transaction although the average need
 * 	is most likely to be less than a hundred. But mallocing 64K srch_blk_status structures at one stretch (in order to allow
 * 	the maximum) is too costly in terms of virtual memory (approx. 3 MB memory per region per process).
 * Therefore, we malloc only cur_tp_hist_size structures initially. As the need for more structures arises, we malloc an array
 *	double the size of the original	and reset pointers appropriately.
 * cur_tp_hist_size always represents the current allocation while tp_hist_size represents the maximum possible allocation.
 */
typedef struct sgm_info_struct
{
	struct sgm_info_struct
			*next_sgm_info;
	srch_blk_status	*first_tp_hist,
			*last_tp_hist;
	hashtab		*blks_in_use;
	gd_region	*gv_cur_region;
	trans_num	start_tn,
			start_csh_tn;		/* not maintained but preserved in case needed in future */
	cw_set_element	*first_cw_set,
			*last_cw_set,
			*first_cw_bitmap;
	buddy_list	*cw_set_list;		/* list(buddy) of cw_set_elements for this region */
	buddy_list	*tlvl_cw_set_list;	/* list(buddy) of horizontal cw_set_elements for this region */
						/* first link in each of these horizontal lists is maintained in
						 * cw_set_list buddy list */
	buddy_list	*new_buff_list;		/* to hold the new_buff for the cw_set elements */
	buddy_list	*recompute_list;	/* to hold the list of to-be-recomputed keys and values */
	buddy_list	*tlvl_info_list;	/* to hold the list of tlvl_info structures */
	cache_rec_ptr_ptr_t
			cr_array;
	kill_set	*kill_set_head,
			*kill_set_tail;
	tlevel_info	*tlvl_info_head;
	jnl_format_buffer	*jnl_head,
				**jnl_tail;
	buddy_list	*format_buff_list;
	buddy_list	*jnl_list;
	int		cw_set_depth,
			cr_array_index,
			num_of_blks,
			tp_hist_size,
			cur_tp_hist_size,
			total_jnl_record_size,
			cr_array_size;
	boolean_t	fresh_start;
	short		crash_count;
	bool		backup_block_saved;
	boolean_t	kip_incremented;
} sgm_info;

typedef struct
{
#ifdef	BIGENDIAN
	unsigned	flag	 : 1;
	unsigned	cw_index : 15;
	unsigned	next_off : 16;
#else
	unsigned	next_off : 16;
	unsigned	cw_index : 15;
	unsigned	flag	 : 1;
#endif
} off_chain;

/* The following struct is built into a separate list for each transaction
   because it is not thrown away if a transaction restarts. The list keeps
   growing so we can lock down all the necessary regions in the correct order
   in case one attempt doesn't get very far while later attempts get further.
   Items will be put on the list sorted in unique_id order so that they will always
   be grab-crit'd in the same order thus avoiding deadlocks. */

/* The structure backup_region_list defined in mupipbckup.h needs to have its first four fields
   identical to the first three fields in this structure */
typedef struct tp_region_struct
{
	struct	tp_region_struct *fPtr;			/* Next in list */
	gd_region	*reg;				/* Region pointer */
	gd_id		file_id; 			/* both for VMS and UNIX */
} tp_region;

typedef struct ua_list_struct
{
	struct ua_list_struct
			*next_ua;
	char		*update_array;
	int		update_array_size;
} ua_list;

typedef struct new_buff_buddy_list_struct
{
	que_ent		free_que;
	unsigned char	new_buff[1];
}new_buff_buddy_list;

#define TP_TRACE_HIST(X, Y) 										\
{													\
	GBLREF	int4		tprestart_syslog_delta;							\
	GBLREF	block_id	t_fail_hist_blk[CDB_MAX_TRIES];						\
	GBLREF	gv_namehead	*tp_fail_hist[CDB_MAX_TRIES];						\
													\
	if (tprestart_syslog_delta)									\
	{												\
		t_fail_hist_blk[t_tries] = ((block_id)X); 						\
		tp_fail_hist[t_tries] = (gv_namehead *)(((int)X & ~(-BLKS_PER_LMAP)) ? Y : NULL);	\
	}												\
}

#define TP_TRACE_HIST_MOD(X, Y, n, csd, histtn, bttn, level)						\
{													\
	GBLREF	int4		tprestart_syslog_delta;							\
	GBLREF	int4		tp_fail_n;								\
	GBLREF	int4		tp_fail_level;								\
	GBLREF	block_id	t_fail_hist_blk[CDB_MAX_TRIES];						\
	GBLREF	gv_namehead	*tp_fail_hist[CDB_MAX_TRIES];						\
	GBLREF	int4		tp_fail_histtn[CDB_MAX_TRIES], tp_fail_bttn[CDB_MAX_TRIES];		\
													\
	if (tprestart_syslog_delta)									\
	{												\
		t_fail_hist_blk[t_tries] = ((block_id)X);						\
		tp_fail_hist[t_tries] = (gv_namehead *)(((int)X & ~(-BLKS_PER_LMAP)) ? Y : NULL); 	\
		(csd)->tp_cdb_sc_blkmod[(n)]++;								\
		tp_fail_n = (n);									\
		tp_fail_level = (level);								\
		tp_fail_histtn[t_tries] = (histtn);							\
		tp_fail_bttn[t_tries] = (bttn);								\
	}												\
}

GBLREF	short	dollar_trestart;

#define	ASSERT_IS_WITHIN_TP_HIST_ARRAY_BOUNDS(first_tp_srch_status, sgm_info_ptr)	\
{											\
	assert(NULL == (first_tp_srch_status) 						\
		|| ((first_tp_srch_status) >= (sgm_info_ptr)->first_tp_hist		\
			&& (first_tp_srch_status) < (sgm_info_ptr)->last_tp_hist));	\
}

#define	TP_RETRY_ACCOUNTING(csd, status)						\
{											\
	if (dollar_trestart < ARRAYSIZE((csd)->n_tp_retries) - 1)			\
		(csd)->n_tp_retries_conflicts[dollar_trestart]++;			\
	else										\
		(csd)->n_tp_retries_conflicts[ARRAYSIZE(csd->n_tp_retries) - 1]++;	\
}

#define PREV_OFF_INVALID -1

#define TOTAL_TPJNL_REC_SIZE(total_jnl_rec_size, si, csa)							\
{														\
	total_jnl_rec_size = si->total_jnl_record_size; 							\
	if (jbp->before_images)											\
		total_jnl_rec_size += (si->cw_set_depth * csa->pblk_align_jrecsize);				\
	/* Since we have already taken into account an align record per journal record and since the size of	\
	 * an align record will be < (size of the journal record written + fixed-size of align record)		\
	 * we can be sure we won't need more than twice the computed space.					\
	 */													\
	total_jnl_rec_size *= 2;										\
}

/* This macro gives a pessimistic estimate on the total journal record size needed.
 * The side effect is that we would end up with a journal file extension when it was actually not needed.
 * But that should happen only if we are nearing 4G and hence should not be a concern.
 */
#define TOTAL_NONTPJNL_REC_SIZE(total_jnl_rec_size, non_tp_jfb_ptr, csa, cw_set_depth)				\
{														\
	total_jnl_rec_size = non_tp_jfb_ptr->record_size;							\
	if (total_jnl_rec_size)											\
		total_jnl_rec_size += csa->min_total_nontpjnl_rec_size;						\
	if (jbp->before_images)											\
		total_jnl_rec_size += (cw_set_depth * csa->pblk_align_jrecsize);				\
	/* The following two computations are either rare or are needed in dse where performance		\
	 * 	is not a concern. Hence these are recomputed everytime instead of storing in a variable.	\
	 */													\
	if (dse_running || write_after_image)											\
		total_jnl_rec_size += (JREC_PREFIX_SIZE + jnl_fixed_size[JRT_AIMG] + JREC_SUFFIX_SIZE		\
					+ csa->hdr->blk_size							\
					+ JREC_PREFIX_SIZE + jnl_fixed_size[JRT_ALIGN] + JREC_SUFFIX_SIZE);	\
	if (mu_reorg_process || 0 == cw_depth || inctn_gvcstput_extra_blk_split == inctn_opcode)		\
		total_jnl_rec_size += (JREC_PREFIX_SIZE + jnl_fixed_size[JRT_INCTN] + JREC_SUFFIX_SIZE) 	\
					+ (JREC_PREFIX_SIZE + jnl_fixed_size[JRT_ALIGN] + JREC_SUFFIX_SIZE);	\
	/* Since we have already taken into account an align record per journal record and since the size of	\
	 * an align record will be < (size of the journal record written + fixed-size of align record)		\
	 * we can be sure we won't need more than twice the computed space.					\
	 */													\
	total_jnl_rec_size *= 2;										\
}

#define INVALIDATE_CLUE(cse) 					\
{								\
	off_chain	macro_chain;				\
								\
	assert(cse->blk_target);				\
	cse->blk_target->clue.end = 0;				\
	macro_chain = *(off_chain *)&cse->blk_target->root;	\
	if (macro_chain.flag)					\
		cse->blk_target->root = 0;			\
}

/* freeup killset starting from the link 'ks' */

#define FREE_KILL_SET(si, ks)					\
{								\
	kill_set	*macro_next_ks;				\
	for (; ks; ks = macro_next_ks)				\
	{							\
		macro_next_ks = ks->next_kill_set;		\
		free(ks);					\
	}							\
}

/* freeup jnl_format_buff_info_list starting from the link 'jfb' */

#define FREE_JFB_INFO(si, jfb)							\
{										\
	int	macro_cnt, format_buf_cnt;					\
	for (macro_cnt = 0, format_buf_cnt = 0; jfb; jfb = jfb->next)		\
	{									\
		format_buf_cnt += jfb->record_size;				\
		macro_cnt++;							\
	}									\
	free_last_n_elements(si->format_buff_list, format_buf_cnt); 		\
	free_last_n_elements(si->jnl_list, macro_cnt); 				\
}

/* freeup gbl_tlvl_info_list starting from the link 'gti' */

#define FREE_GBL_TLVL_INFO(gti)							\
{										\
	int	macro_cnt;							\
	for (macro_cnt = 0; gti; gti = gti->next_global_tlvl_info)		\
		macro_cnt++;							\
	if (macro_cnt)								\
		free_last_n_elements(global_tlvl_info_list, macro_cnt);		\
}

#ifdef VMS
/* The error below has special handling in a few condition handlers because it not so much signals an error
   as it does drive the necessary mechanisms to invoke a restart. Consequently this error can be
   overridden by a "real" error. For VMS, the extra parameters are specified to provide "placeholders" on
   the stack in the signal array if a real error needs to be overlayed in place of this one (see example
   code in mdb_condition_handler). The number of extra parameters need to be 2 more than the largest
   number of parameters for an rts_error in tp_restart().
*/
#define INVOKE_RESTART	rts_error(VARLSTCNT(6) ERR_TPRETRY, 4, 0, 0, 0, 0, 0, 0, 0, 0);
#else
#define INVOKE_RESTART	rts_error(VARLSTCNT(1) ERR_TPRETRY);
#endif


void tp_get_cw(cw_set_element *cs, int depth, cw_set_element **cs1);
bool tp_tend(bool crit_only);
void tp_clean_up(boolean_t rollback_flag);
void tp_cw_list(cw_set_element **cs);
void tp_get_cw(cw_set_element *cs, int depth, cw_set_element **cs1);
void tp_incr_clean_up(short newlevel);
void tp_incr_rollback(short newlevel);
void tp_set_sgm(void);
tp_region *insert_region(gd_region *reg, tp_region **reg_list, tp_region **reg_free_list, int4 size);
void tp_start_timer_dummy(int4 timeout_seconds);
void tp_clear_timeout_dummy(void);
void tp_timeout_action_dummy(void);

#endif
