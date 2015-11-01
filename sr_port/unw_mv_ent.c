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

#include "hashdef.h"
#include "lv_val.h"
#include "mv_stent.h"
#include "sbs_blk.h"

GBLREF symval		*curr_symval;
GBLREF sbs_blk		*sbs_blk_hdr;
GBLREF boolean_t	dollar_truth;
GBLREF lv_val		*active_lv;
GBLREF mval		dollar_etrap;
GBLREF mval		dollar_ztrap;

mval	*unw_mv_ent(mv_stent *mv_st_ent)
{
	lv_blk		*lp, *lp1;
	symval		*symval_ptr;
	mval		*ret_value;
	ht_entry	*hte;

	active_lv = (lv_val *)0; /* if we get here, subscript set was successful,
				  * clear active_lv to avoid later cleanup problems */

	switch (mv_st_ent->mv_st_type)
	{
	case MVST_MSAV:
		*mv_st_ent->mv_st_cont.mvs_msav.addr = mv_st_ent->mv_st_cont.mvs_msav.v;
		if (mv_st_ent->mv_st_cont.mvs_msav.addr == &dollar_etrap)
		{
			dollar_ztrap.str.len = 0;
		}
		if (mv_st_ent->mv_st_cont.mvs_msav.addr == &dollar_ztrap)
		{
			dollar_etrap.str.len = 0;
		}
		return 0;
	case MVST_MVAL:
	case MVST_IARR:
		return 0;
	case MVST_STAB:
		if (mv_st_ent->mv_st_cont.mvs_stab)
		{
			assert(mv_st_ent->mv_st_cont.mvs_stab == curr_symval);
			symval_ptr = curr_symval;
			curr_symval = symval_ptr->last_tab;
			free(symval_ptr->first_block.lv_base);
			for (lp = symval_ptr->first_block.next ; lp ; lp = lp1)
			{
				free(lp->lv_base);
				lp1 = lp->next;
				free(lp);
			}
			if (symval_ptr->sbs_que.fl == (sbs_blk *)symval_ptr)
				assert(symval_ptr->sbs_que.fl == symval_ptr->sbs_que.bl);
			else
			{
				symval_ptr->sbs_que.bl->sbs_que.fl = sbs_blk_hdr;
				sbs_blk_hdr = symval_ptr->sbs_que.fl;
			}
			free(symval_ptr->h_symtab.base);
			free(symval_ptr);
		}
		return 0;
	case MVST_NTAB:
		hte = ht_get(&curr_symval->h_symtab, (mname *)mv_st_ent->mv_st_cont.mvs_ntab.nam_addr);
		assert(hte);
		hte->ptr = (char *)mv_st_ent->mv_st_cont.mvs_ntab.save_value;
		if (mv_st_ent->mv_st_cont.mvs_ntab.lst_addr)
			*mv_st_ent->mv_st_cont.mvs_ntab.lst_addr = (struct lv_val_struct *)hte->ptr;
		return 0;
	case MVST_PARM:
		if (mv_st_ent->mv_st_cont.mvs_parm.mvs_parmlist)
			free(mv_st_ent->mv_st_cont.mvs_parm.mvs_parmlist);
		ret_value = mv_st_ent->mv_st_cont.mvs_parm.ret_value;
		if (ret_value)
			dollar_truth = (boolean_t)mv_st_ent->mv_st_cont.mvs_parm.save_truth;
		return ret_value;
	case MVST_PVAL:
		if (mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.nam_addr)
		{
			hte = ht_get(&curr_symval->h_symtab, (mname *)mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.nam_addr);
			assert(hte);
			hte->ptr = (char *)mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.save_value;
			if (mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.lst_addr)
				*mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.lst_addr = (struct lv_val_struct *)hte->ptr;
			if (mv_st_ent->mv_st_cont.mvs_pval.mvs_val->ptrs.val_ent.children)
				lv_killarray(mv_st_ent->mv_st_cont.mvs_pval.mvs_val->ptrs.val_ent.children);
		}
		mv_st_ent->mv_st_cont.mvs_pval.mvs_val->ptrs.free_ent.next_free = curr_symval->lv_flist;
		curr_symval->lv_flist = mv_st_ent->mv_st_cont.mvs_pval.mvs_val;
		return 0;
	case MVST_NVAL:
		hte = ht_get(&curr_symval->h_symtab, (mname *)&mv_st_ent->mv_st_cont.mvs_nval.name);
		assert(hte);
		hte->ptr = (char *)mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab.save_value;
		if (mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab.lst_addr)
			*mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab.lst_addr = (struct lv_val_struct *)hte->ptr;
		if (mv_st_ent->mv_st_cont.mvs_nval.mvs_val->ptrs.val_ent.children)
			lv_killarray(mv_st_ent->mv_st_cont.mvs_nval.mvs_val->ptrs.val_ent.children);
		mv_st_ent->mv_st_cont.mvs_nval.mvs_val->ptrs.free_ent.next_free = curr_symval->lv_flist;
		curr_symval->lv_flist = mv_st_ent->mv_st_cont.mvs_nval.mvs_val;
		return 0;
	case MVST_STCK:
		*(mv_st_ent->mv_st_cont.mvs_stck.mvs_stck_addr) =
			(unsigned char *)mv_st_ent->mv_st_cont.mvs_stck.mvs_stck_val;
		return 0;
	case MVST_TVAL:
		dollar_truth = (boolean_t)mv_st_ent->mv_st_cont.mvs_tval;
		return 0;
	case MVST_TPHOLD:
		return 0;	/* just a place holder for TP */
	default:
		GTMASSERT;
		return 0;
	}
}
