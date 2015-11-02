/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc.*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_limits.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_ctype.h"
#include "gtm_inet.h"
#include <errno.h>
#ifdef VMS
#include <descrip.h> /* Required for gtmrecv.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmrecv.h"
#include "cli.h"
#include "gtm_stdio.h"
#include "util.h"
#include "repl_log.h"

GBLREF	gtmrecv_options_t	gtmrecv_options;

int gtmrecv_get_opt(void)
{

	boolean_t	log, log_interval_specified;
	unsigned short 	log_file_len, filter_cmd_len;
	boolean_t	buffsize_status;
	boolean_t	filter;
	int		status;
	unsigned short	statslog_val_len;
	char		statslog_val[4]; /* "ON" or "OFF" */
	uint4		n_readers, n_helpers;

	gtmrecv_options.start = (CLI_PRESENT == cli_present("START"));
	gtmrecv_options.shut_down = (CLI_PRESENT == cli_present("SHUTDOWN"));
	gtmrecv_options.checkhealth = (CLI_PRESENT == cli_present("CHECKHEALTH"));
	gtmrecv_options.statslog = (CLI_PRESENT == cli_present("STATSLOG"));
	gtmrecv_options.showbacklog = (CLI_PRESENT == cli_present("SHOWBACKLOG"));
	gtmrecv_options.changelog = (CLI_PRESENT == cli_present("CHANGELOG"));
	gtmrecv_options.updateonly = (CLI_PRESENT == cli_present("UPDATEONLY"));
	gtmrecv_options.updateresync = (CLI_PRESENT == cli_present("UPDATERESYNC"));
	gtmrecv_options.helpers = (CLI_PRESENT == cli_present("HELPERS"));

	gtmrecv_options.listen_port = 0; /* invalid port; indicates listenport not specified */
	if (gtmrecv_options.start && CLI_PRESENT == cli_present("LISTENPORT"))
	{
		if (!cli_get_int("LISTENPORT", &gtmrecv_options.listen_port))
		{
			util_out_print("Error parsing LISTENPORT qualifier", TRUE);
			return (-1);
		}
		if (buffsize_status = (CLI_PRESENT == cli_present("BUFFSIZE")))
		{
			if (!cli_get_int("BUFFSIZE", &gtmrecv_options.buffsize))
			{
				util_out_print("Error parsing BUFFSIZE qualifier", TRUE);
				return (-1);
			}
			if (MIN_RECVPOOL_SIZE > gtmrecv_options.buffsize)
				gtmrecv_options.buffsize = MIN_RECVPOOL_SIZE;
		} else
			gtmrecv_options.buffsize = DEFAULT_RECVPOOL_SIZE;
		/* Round up or down buffsize */

		if (filter = (CLI_PRESENT == cli_present("FILTER")))
		{
			filter_cmd_len = MAX_FILTER_CMD_LEN;
	    		if (!cli_get_str("FILTER", gtmrecv_options.filter_cmd, &filter_cmd_len))
			{
				util_out_print("Error parsing FILTER qualifier", TRUE);
				return (-1);
			}
		} else
			gtmrecv_options.filter_cmd[0] = '\0';

		gtmrecv_options.stopsourcefilter = (CLI_PRESENT == cli_present("STOPSOURCEFILTER"));
	}

	if ((gtmrecv_options.start && 0 != gtmrecv_options.listen_port) || gtmrecv_options.statslog || gtmrecv_options.changelog)
	{
		log = (CLI_PRESENT == cli_present("LOG"));
		log_interval_specified = (CLI_PRESENT == cli_present("LOG_INTERVAL"));
		if (log)
		{
			log_file_len = MAX_FN_LEN + 1;
			if (!cli_get_str("LOG", gtmrecv_options.log_file, &log_file_len))
			{
				util_out_print("Error parsing LOG qualifier", TRUE);
				return (-1);
			}
		} else
			gtmrecv_options.log_file[0] = '\0';
		gtmrecv_options.rcvr_log_interval = gtmrecv_options.upd_log_interval = 0;
		if (log_interval_specified
		    && 0 == cli_parse_two_numbers("LOG_INTERVAL", GTMRECV_LOGINTERVAL_DELIM, &gtmrecv_options.rcvr_log_interval,
							&gtmrecv_options.upd_log_interval))
			return (-1);
		if (gtmrecv_options.start)
		{
			if (0 == gtmrecv_options.rcvr_log_interval)
				gtmrecv_options.rcvr_log_interval = LOGTRNUM_INTERVAL;
			if (0 == gtmrecv_options.upd_log_interval)
				gtmrecv_options.upd_log_interval = LOGTRNUM_INTERVAL;
		} /* For changelog, interval == 0 implies don't change log interval already established */
		  /* We ignore interval specification for statslog, Vinaya 2005/02/07 */
	}

	if (gtmrecv_options.shut_down)
	{
		if (CLI_PRESENT == (status = cli_present("TIMEOUT")))
		{
			if (!cli_get_int("TIMEOUT", &gtmrecv_options.shutdown_time))
			{
				util_out_print("Error parsing TIMEOUT qualifier", TRUE);
				return (-1);
			}
			if (DEFAULT_SHUTDOWN_TIMEOUT < gtmrecv_options.shutdown_time || 0 > gtmrecv_options.shutdown_time)
			{
				gtmrecv_options.shutdown_time = DEFAULT_SHUTDOWN_TIMEOUT;
				util_out_print("shutdown TIMEOUT changed to !UL", TRUE, gtmrecv_options.shutdown_time);
			}
		} else if (CLI_NEGATED == status)
			gtmrecv_options.shutdown_time = -1;
		else /* TIMEOUT not specified */
			gtmrecv_options.shutdown_time = DEFAULT_SHUTDOWN_TIMEOUT;
	}

	if (gtmrecv_options.statslog)
	{
		statslog_val_len = 4; /* max(strlen("ON"), strlen("OFF")) + 1 */
		if (!cli_get_str("STATSLOG", statslog_val, &statslog_val_len))
		{
			util_out_print("Error parsing STATSLOG qualifier", TRUE);
			return (-1);
		}
#ifdef UNIX
		cli_strupper(statslog_val);
#endif
		if (0 == strcmp(statslog_val, "ON"))
			gtmrecv_options.statslog = TRUE;
		else if (0 == strcmp(statslog_val, "OFF"))
			gtmrecv_options.statslog = FALSE;
		else
		{
			util_out_print("Invalid value for STATSLOG qualifier, should be either ON or OFF", TRUE);
			return (-1);
		}
	}
	gtmrecv_options.n_readers = gtmrecv_options.n_writers = 0;
	if (gtmrecv_options.helpers && gtmrecv_options.start)
	{ /* parse the helpers qualifier to find out how many readers and writes have to be started */
		if (0 == (status = cli_parse_two_numbers("HELPERS", UPD_HELPERS_DELIM, &n_helpers, &n_readers)))
			return (-1);
		if (!(status & CLI_2NUM_FIRST_SPECIFIED))
			n_helpers = DEFAULT_UPD_HELPERS;
		if (MIN_UPD_HELPERS > n_helpers || MAX_UPD_HELPERS < n_helpers)
		{
			util_out_print("Invalid number of helpers; must be in the range [!UL,!UL]", TRUE, MIN_UPD_HELPERS,
					MAX_UPD_HELPERS);
			return (-1);
		}
		if (!(status & CLI_2NUM_SECOND_SPECIFIED))
			n_readers = (int)(n_helpers * ((float)DEFAULT_UPD_HELP_READERS)/DEFAULT_UPD_HELPERS); /* may round down */
		if (n_readers > n_helpers)
		{
			n_readers = n_helpers;
			util_out_print("Number of readers exceeds number of helpers, reducing number of readers to number of "
					"helpers", TRUE);
		}
		gtmrecv_options.n_readers = n_readers;
		gtmrecv_options.n_writers = n_helpers - n_readers;
	}
	return (0);
}
