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
#ifndef __JNLSP_H__
#define __JNLSP_H__

/* Start jnlsp.h - platform-specific journaling definitions.  */

#ifndef sys_nerr
#include <errno.h>
#endif

typedef int64_t			jnl_proc_time;

/* in disk blocks but jnl file addresses are kept by byte so limited by uint4 for now */
#ifndef OFF_T_LONG
#define JNL_ALLOC_MAX		4194304  /* 2GB */
#else
#define JNL_ALLOC_MAX		8388608  /* 4GB */
#endif

typedef	int			fd_type;
typedef unix_file_info		fi_type;

#define NOJNL			-1
#define LENGTH_OF_TIME		11
#define SOME_TIME(X)		(X != 0)
#define JNL_S_TIME(Y,X)		Y->val.X.process_vector.jpv_time
#define JNL_S_TIME_PINI(Y,X,Z)	Y->val.X.process_vector[Z].jpv_time
#define JNL_M_TIME(X)		mur_options.X
#define MID_TIME(X)		X
#define EXTTIME(T)		extract_len = exttime(*T, ref_time, extract_len)
#define EXTTIMEVMS(T)
#define EXTINTVMS(I)
#define EXTTXTVMS(T,L)

#define	JNL_SHORT_TIME(X)	(time((time_t *)&X))
#define	JNL_WHOLE_TIME(X)	{	\
	time_t temp_t; 			\
	time(&temp_t); 			\
	X = temp_t;			\
	}
#define CMP_JNL_PROC_TIME(t1, t2) (t1 - t2)
#define JNL_FILE_SWITCHED(reg) 	(!is_gdid_gdid_identical((gd_id_ptr_t)&(&FILE_INFO(reg)->s_addrs)->hdr->jnl_file.u,		\
									(gd_id_ptr_t)&(&FILE_INFO(reg)->s_addrs)->jnl->fileid))
#define JNL_GDID_PTR(sa)	((gd_id_ptr_t)(&(sa->hdr->jnl_file.u)))

uint4 jnl_file_open(gd_region *reg, bool init, int4 dummy);

#endif
