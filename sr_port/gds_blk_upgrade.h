/****************************************************************
 *								*
 *	Copyright 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GDS_BLK_UPGRADE_INCLUDED
#define GDS_BLK_UPGRADE_INCLUDED

#define UPGRADE_IF_NEEDED	0	/* default */
#define UPGRADE_NEVER		1
#define UPGRADE_ALWAYS		2

int4 gds_blk_upgrade(sm_uc_ptr_t gds_blk_src, sm_uc_ptr_t gds_blk_trg, int4 bsiz, enum db_ver *ondsk_blkver);

GBLREF	uint4		gtm_blkupgrade_flag;	/* control whether dynamic upgrade is attempted or not */
GBLREF	boolean_t	dse_running;

/* See if block needs to be converted to current version. Assume buffer is at least short aligned */
#define GDS_BLK_UPGRADE_IF_NEEDED(blknum, srcbuffptr, trgbuffptr, curcsd, ondskblkver, upgrdstatus)				\
{																\
	/* In order to detect if a block needs to be upgraded or not, we do the following series of tests.			\
	 * If DSE, the variable "gtm_blkupgrade_flag" controls whether upgrade is attempted or not.				\
	 *	If it is UPGRADE_NEVER, we never attempt upgrade.								\
	 *	Likewise, if it is UPGRADE_ALWAYS, we unconditionally upgrade.							\
	 *	If it is UPGRADE_IF_NEEDED, then the following checks are done.							\
	 * 1) If the file-header has "fully_upgraded" set to TRUE, we know for sure no block needs to be upgraded.		\
	 *    This check is performed in this macro itself as it is a quick check and avoids a function call.			\
	 * 2) Else, the block might or might not need an upgrade. To decide, we do some more checks.				\
	 *    V5 onwards, the first 2 bytes of the block header is the version indicator. It is 1 in V5.			\
	 *    In V4, the first 2 bytes of the block header was the block-size which is guaranteed to be				\
	 *    at least sizeof(v15_blk_hdr) (the size of the V4 blk_hdr structure) which is 8 bytes in Unix			\
	 *    and 7 bytes in VMS. We use the first 2 bytes in the block header as a first level check.				\
	 *    If they are >= sizeof(v15_blk_hdr), we decide it is a V4 format block and try to upgrade.				\
	 *    This check is performed in this macro itself as it is a quick check and avoids a function call.			\
	 * 3) It is quite possible that we might conclude the format incorrectly based on just the above checks.		\
	 *	This can be due to any one of the following.									\
	 *	 => A V4 format block might have a corrupt block-header where the first 2 bytes are exactly 1.			\
	 *	 => A V5 format block might have a corrupt block-header where the first 2 bytes are not 1.			\
	 *	 => We might be reading an unused block (marked free/recycled in the bitmap) from disk.				\
	 *	    This is possible due to a variety of reasons including concurrency issues while				\
	 *		traversing the B-tree that cause us to end up in a restartable situation.				\
	 *	    If we read such a block, then the block contents (including the header) is not valid.			\
	 *	    In VMS it contains uninitialized data. In Unix it contains 0s.						\
	 *	In all the above cases, we have no definitive way of determining exactly which format the block is.		\
	 *	For the case where we incorrectly conclude it is V5 format, we dont do much.					\
	 *	But for the other case, we do better by checking if the V4 block has enough room to accommodate			\
	 *		the extra space needed for the upgrade.									\
	 *	If yes, we go ahead with the upgrade.										\
	 *	If not, we check if the V4 block size is less than the database block size.					\
	 *		If yes, then we believe it is a valid V4 block that is just too big to be upgraded.			\
	 *			We therefore issue a DYNUPGRDFAIL error.							\
	 *		If no, this is a corrupt block and we decide not to upgrade.						\
	 *	Something to consider for the future is to invoke block certification on this block.				\
	 *    This check is involved and hence is done in the function "gds_blk_upgrade" (invoked from this macro).		\
	 * This is not a theoretically foolproof solution but for all practical purposes should be good enough.			\
	 * Note that for a database that has been completely upgraded to V5 format, we do not have any inconclusiveness.	\
	 *															\
	 * Note the clearing of srcbuffptr is done as a flag that gds_blk_upgrd was run (used by dsk_read).			\
	 */															\
	if (!dse_running || (UPGRADE_IF_NEEDED == gtm_blkupgrade_flag))								\
	{															\
		if ((curcsd)->fully_upgraded || (sizeof(v15_blk_hdr) > ((v15_blk_hdr_ptr_t)(srcbuffptr))->bsiz))		\
		{														\
			upgrdstatus = SS_NORMAL;										\
			if (NULL != (ondskblkver))										\
				*(ondskblkver) = GDSV5;										\
		} else														\
		{														\
			upgrdstatus = gds_blk_upgrade((sm_uc_ptr_t)(srcbuffptr), (sm_uc_ptr_t)(trgbuffptr),			\
						      (curcsd)->blk_size, (ondskblkver));					\
                        if (srcbuffptr != trgbuffptr)										\
                                srcbuffptr = NULL;										\
		}														\
	} else if (UPGRADE_NEVER == gtm_blkupgrade_flag)									\
	{															\
		upgrdstatus = SS_NORMAL;											\
		if (NULL != (ondskblkver))											\
			*(ondskblkver) = GDSV5;											\
	} else if (UPGRADE_ALWAYS == gtm_blkupgrade_flag)									\
	{															\
		upgrdstatus = gds_blk_upgrade((sm_uc_ptr_t)(srcbuffptr), (sm_uc_ptr_t)(trgbuffptr),				\
					      (curcsd)->blk_size, (ondskblkver));						\
		if (NULL != (ondskblkver))											\
			*(ondskblkver) = GDSV4;											\
                if (srcbuffptr != trgbuffptr)											\
                        srcbuffptr = NULL;											\
	}															\
}

#endif
