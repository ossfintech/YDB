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
#include "mdef.h"
#include "cmidef.h"
#include "gtm_socket.h"
#include "gtm_fcntl.h"
#ifdef __sparc
#define BSD_COMP
#endif
#include <sys/ioctl.h>
#include <netinet/in.h>
#ifndef __MVS__
#include <netinet/tcp.h>
#endif
#include <errno.h>
#include "eintr_wrappers.h"

#ifndef GTCM_MIN_TCP_SNDBUFLEN
#define GTCM_MIN_TCP_SNDBUFLEN	(64 * 1024)	/* accommodate the longest message possible with 2-byte message length */
#endif
#ifndef GTCM_MIN_TCP_RCVBUFLEN
#define GTCM_MIN_TCP_RCVBUFLEN	(64 * 1024)	/* accommodate the longest message possible with 2-byte message length */
#endif

GBLREF	uint4	process_id;

cmi_status_t cmj_setupfd(int fd)
{
	int rval, buflen, snd_buflen, rcv_buflen;
	long lpid = (long)process_id;
	int on = 1;
	GTM_SOCKLEN_TYPE optlen;

	FCNTL3(fd, F_SETFL, O_NONBLOCK, rval);
	if (-1 == rval)
		return (cmi_status_t)errno;
	FCNTL3(fd, F_SETOWN, lpid, rval);
	if (-1 == rval)
		return (cmi_status_t)errno;
#ifdef F_SETSIG
	FCNTL3(fd, F_SETSIG, (long)SIGIO, rval);
	if (-1 == rval)
		return (cmi_status_t)errno;
#endif
	rval = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
			(const void *)&on, sizeof(on));
	if (rval < 0)
		return (cmi_status_t)errno;
#ifdef TCP_NODELAY
	/* z/OS only does setsockop on SOL and IP */
	rval = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
			(const void *)&on, sizeof(on));
	if (rval < 0)
		return (cmi_status_t)errno;
#endif
#ifndef GTCM_KEEP_DEFAULT_BUFLEN /* if you want to test with system allocated default buflen, define this and rebuild */
	optlen = sizeof(buflen);
	if (-1 == (rval = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&snd_buflen, &optlen)) ||
	    -1 == (rval = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&rcv_buflen, &optlen)))
		return (cmi_status_t)errno;
	CMI_DPRINT(("Current buffer sizes for fd %d are send : %d, recv : %d\n", fd, snd_buflen, rcv_buflen));
	if (GTCM_MIN_TCP_SNDBUFLEN > snd_buflen)
	{
		buflen = GTCM_MIN_TCP_SNDBUFLEN; /* increase send buflen to avoid breaking large messages in chunks */
		if (-1 == (rval = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&buflen, optlen)))
			return (cmi_status_t)errno;
		CMI_DPRINT(("Send buffer size for fd %d now %d\n", fd, buflen));
	}
	if (GTCM_MIN_TCP_RCVBUFLEN > rcv_buflen)
	{
		buflen = GTCM_MIN_TCP_RCVBUFLEN; /* increase recv buflen to avoid having to assemble smaller chunks */
		if (-1 == (rval = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&buflen, optlen)))
			return (cmi_status_t)errno;
		CMI_DPRINT(("Recv buffer size for fd %d now %d\n", fd, buflen));
	}
#endif
	return SS_NORMAL;
}