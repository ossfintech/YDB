/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CACHE_H
#define CACHE_H

typedef struct {
	mstr		str;
	uint4		code;
} icode_str;	/* For indirect code source. */

typedef struct cache_ent
{
	mstr		obj;
	icode_str	src;
	int		refcnt;			/* Number of indirect source code pointing to same cache entry */
	int		zb_refcnt;		/* Number of zbreak action entry pointing to same cache entry */
} cache_entry;

/* Following is the indirect routine header build as part of an indirect code object */
typedef struct ihead_struct
{
	cache_entry	*indce;
	int4		vartab_off;
	int4		vartab_len;
	int4		temp_mvals;
	int4		temp_size;
	int4		fixup_vals_off;	/* literal mval table offset */
	int4		fixup_vals_num;	/* literal mval table's mval count */
} ihdtyp;

#define ICACHE_TABLE_INIT_SIZE 	64	/* Use 1K memory initially */
#define ICACHE_SIZE 		ROUND_UP2(sizeof(cache_entry), NATIVE_WSIZE)

/* We allow cache_table to grow till we hit 10K memory or 200 entries. If more memory is needed, we do compaction.
 * Note : We need to do some experiment to find out good numbers for this.
 *        May be these numbers should be automatically adjustible during run time
 */
#define MAX_CACHE_MEMSIZE	10240
#define MAX_CACHE_ENTRIES	200

void indir_lits(ihdtyp *ihead);
void cache_init(void);
mstr *cache_get(icode_str *indir_src);
void cache_put(icode_str *src, mstr *object);
void cache_table_rebuild(void);
void cache_stats(void);

#endif
