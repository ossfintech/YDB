/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"
#include "cli.h"
#include "cli_parse.h"
#include "error.h"
#include "cli_disallow.h"

#define	NO_STRING	"NO"

GBLDEF char	 	*parm_ary[MAX_PARMS];		/* Parameter strings buffers */
GBLDEF unsigned int	parms_cnt;			/* Parameters count */
GBLDEF void 		(*func)(void);			/* Function to be dispatched
							   to for this command */

GBLREF char 		cli_err_str[];	/* Parse Error message buffer */
static CLI_ENTRY 	*gpqual_root;			/* pointer to root of
							   subordinate qualifier table */
static CLI_ENTRY 	*gpcmd_qual;			/* pointer command qualifier table */
static CLI_ENTRY 	*gpcmd_verb;			/* pointer to verb table */
static CLI_ENTRY 	*gpcmd_qual_val;		/* pointer to qualifier table */
static CLI_PARM 	*gpcmd_parm_vals;		/* pointer to parameters for command */

GBLREF char 		cli_token_buf[];
GBLREF CLI_ENTRY 	cmd_ary[];

GBLREF IN_PARMS *cli_lex_in_ptr;

/*
 * -----------------------------------------------
 * Clear all parameter values and flags of command
 * parameter table for a given command.
 *
 * Arguments:
 *	cmd_parms	- address of parameter array
 *	follow		- whether to clear extra value tables or not
 *	  TRUE		- follow down
 *	  FALSE		- do not follow down
 *
 * Return:
 *	none
 * -----------------------------------------------
 */
void	clear_parm_vals(CLI_ENTRY *cmd_parms, boolean_t follow) 		/* pointer to option's parameter table */
{
	CLI_ENTRY	*root_param, *find_cmd_param();
	int		need_copy;

	need_copy = (gpcmd_qual != cmd_parms);

	while (strlen(cmd_parms->name) > 0)
	{
		if (cmd_parms->pval_str)
			free(cmd_parms->pval_str);
		/* if root table exists, copy over any qualifier values to the new parameter table */
		if ((FALSE != follow) && need_copy &&
			(root_param = find_cmd_param(cmd_parms->name, gpcmd_qual, FALSE)))
		{
			cmd_parms->pval_str = root_param->pval_str;
			cmd_parms->negated = root_param->negated;
			cmd_parms->present = root_param->present;
			root_param->pval_str = 0;
		} else
		{
			cmd_parms->negated = 0;
			cmd_parms->present = 0;
			cmd_parms->pval_str = 0;
			/* dfault_str also implements default values.
			   0: it is not present by default,
			   DEFA_PRESENT: it is present by default, no value,
			   str pointer: it is present by default, with the value pointed to
			*/
			if (cmd_parms->dfault_str)
			{
				cmd_parms->present = CLI_DEFAULT;
				if (CLI_PRESENT != (int)cmd_parms->dfault_str)
					cmd_parms->pval_str = cmd_parms->dfault_str;
			}
			if ((FALSE != follow) && cmd_parms->qual_vals)
				clear_parm_vals(cmd_parms->qual_vals, FALSE);
		}
		cmd_parms++;
	}
}

/*
 * ---------------------------------------------------------
 * Find entry in the qualifier table
 *
 * Arguments:
 *	str	- parameter string
 *	pparm	- pointer to parameter table for this command
 *
 * Return:
 *	if found, index into command table array,
 *	else -1
 * ---------------------------------------------------------
 */
int 	find_entry(char *str, CLI_ENTRY *pparm)
{
	int 		match_ind, res;
	boolean_t 	len_match;
	int 		ind;
	char 		*sp;

	ind = 0;
	match_ind = -1;
	len_match = FALSE;
	cli_strupper(str);

	while (0 < strlen(sp = (pparm + ind)->name))
	{
		/* As the table is parsed as long as the string in question is lexically smaller
		   than the entry in the table, we go on checking the next entry.
		   If a match is found, the lengths of the two strings (the table entry and the
		   string in question) are compared.
		  When the next entry is checked,
		   if it is not a match, the previous entry is the correct match;
		   if it is a match again (a longer entry), if the first match was a length-match
		   as well, the first entry is the match returned. otherwise an error is returned
		   since we cannot make a decision (does SE match SET or SEP). */

		if (0 == (res = strncmp(sp, str, strlen(str))))
		{
			if (-1 != match_ind)
			{
				if (FALSE == len_match)
					return(-1);
				break;
			} else
			{
				if (strlen(str) == strlen(sp))
					len_match = TRUE;
				match_ind = ind;
			}
		} else
		{
			if (0 < res)
				break;
		}
		ind++;
	}
	if (-1 != match_ind && gpqual_root && 0 == strncmp(gpqual_root->name, str, strlen(str)))
		return(-1);
	return(match_ind);
}

/*
 * -----------------------------------------------
 * Find command in command table
 *
 * Arguments:
 *	str	- command string
 *
 * Return:
 *	if found, index into command table array,
 *	else -1
 * -----------------------------------------------
 */
int 	find_verb(char *str)
{
	return(find_entry(str, cmd_ary));
}


/*
 * ---------------------------------------------------------
 * Find command parameter in command parameter table
 *
 * Arguments:
 *	str	- parameter string
 *	pparm	- pointer to parameter table for this command
 *
 * Return:
 *	if found, pointer to parameter structure
 *	else 0
 * ---------------------------------------------------------
 */
CLI_ENTRY *find_cmd_param(char *str, CLI_ENTRY *pparm, int follow)
{
	CLI_ENTRY	*pparm_tmp;
	int 	ind, ind_match;
	char 	*sp;

	ind_match = -1;
	if (NULL == pparm)
		return NULL;
	if (0 <= (ind = find_entry(str, pparm)))
		return pparm + ind;

	if (FALSE == follow)
		return NULL;
	/*if to follow, go through the qual_vals, and check those tables */
	for(ind =0; 0 < strlen((pparm + ind)->name); ind++)
	{
		pparm_tmp = (pparm + ind)->qual_vals;
		if (pparm_tmp)
			ind_match = find_entry(str, pparm_tmp);
		if (-1 != ind_match)
			return pparm_tmp + ind_match;
	}
	return NULL;
}

/*
 * ---------------------------------------------------------
 * Parse one option.
 * Read tokens from the input.
 * Check if it is a valid qualifier or parameter.
 * If it is a parameter, get it, and save it in the
 * global parameter array.
 * If it is a qualifier, get its value and save it in a value table,
 * corresponding to this option.
 *
 * Arguments:
 *	pcmd_parms	- pointer to command parameter table
 *	eof		- pointer to end of file flag
 *
 * Return:
 *	1 - option parsed OK
 *	-1 - failure to parse option
 *	0 - no more tokens, in which case
 *		the eof flag is set on end of file.
 * ---------------------------------------------------------
 */
int 	parse_arg(CLI_ENTRY *pcmd_parms, int *eof)
{
	CLI_ENTRY 	*pparm;
	char 		*opt_str, *val_str;
	int 		neg_flg, parm_len;

	/* -----------------------------------------
	 * get qualifier marker, or parameter token
	 * -----------------------------------------
	 */
	if (VAL_LIST == gpcmd_verb->val_type && parms_cnt == gpcmd_verb->max_parms)
		return(0);
	if (!cli_look_next_token(eof))
		return(0);
	/* -------------------------------------------------------------------
	 * here cli_token_buf is set by the previous cli_look_next_token(eof)
	 * call itself since it in turn calls cli_gettoken()
	 * -------------------------------------------------------------------
	 */
	if (!cli_is_qualif(cli_token_buf) && !cli_is_assign(cli_token_buf))
	{
		/* ----------------------------------------------------
		 * If token is not a qualifier, it must be a parameter
		 * ----------------------------------------------------
		 */
		/* ------------------------------------------------------------
		 * no need to check for eof on cli_get_string_token(eof) since
		 * already checked that on the previous cli_look_next_token.
		 * now you have to skip initial white spaces before reading
		 * the string since cli_get_string_token considers a space
		 * as a blank token. hence the need for the skip_white_space()
		 * call.
		 * ------------------------------------------------------------
		 */
		skip_white_space();
		cli_get_string_token(eof);
		if (parms_cnt >= gpcmd_verb->max_parms)
		{
			SNPRINTF(cli_err_str, MAX_STRLEN, "Too many parameters ");
			return(-1);
		}
		if (parm_ary[parms_cnt] && ((char *)-1 != parm_ary[parms_cnt]))
			free(parm_ary[parms_cnt]);
		parm_len = strlen(cli_token_buf) + 1;
		parm_ary[parms_cnt] = malloc(parm_len);
		memcpy(parm_ary[parms_cnt++], cli_token_buf, parm_len);
		return(1);
	}
	/* ---------------------------------------------------------------------
	 * cli_gettoken(eof) need not be checked for return value since earlier
	 * itself we have checked for return value in cli_look_next_token(eof)
	 * ---------------------------------------------------------------------
	 */
	cli_gettoken(eof);
	opt_str = cli_token_buf;
	if (!pcmd_parms)
	{
		SNPRINTF(cli_err_str, MAX_STRLEN, "No qualifiers allowed for this command");
		return(-1);
	}
	/* ------------------------------------------
	 * Qualifiers must start with qualifier token
	 * ------------------------------------------
	 */
	if (!cli_is_qualif(cli_token_buf))
	{
		SNPRINTF(cli_err_str, MAX_STRLEN, "Qualifier expected instead of : %s ", opt_str);
		return(-1);
	}
	/* -------------------------
	 * Get the qualifier string
	 * -------------------------
	 */
	if (!cli_look_next_token(eof) || 0 == cli_gettoken(eof))
	{
		SNPRINTF(cli_err_str, MAX_STRLEN, "Qualifier string missing %s ", opt_str);
		return(-1);
	}
	/* ---------------------------------------
	 * Fold the qualifier string to upper case
	 * ---------------------------------------
	 */
	cli_strupper(opt_str);
	/* -------------------------
	 * See if option is negated and update
	 * -------------------------
	 */
	if (-1 == (neg_flg = cli_check_negated(&opt_str, pcmd_parms, &pparm)))
		return(-1);
	/* --------------------------------------------------
	 * If there is another level, update global pointers
	 * --------------------------------------------------
	 */
	if (pparm->parms)
	{
		func = pparm->func;
		gpqual_root = pparm;
		clear_parm_vals(pparm->parms, TRUE);
		gpcmd_qual = pparm->parms;
	}
	/* -------------------------------------------------------------
	 * If value is disallowed for this qualifier, and an assignment
	 * token is encounter, report error, values not allowed for
	 * negated qualifiers
	 * -------------------------------------------------------------
	 */
	if (neg_flg || VAL_DISALLOWED == pparm->required)
	{
		if (cli_look_next_token(eof) && cli_is_assign(cli_token_buf))
		{
			SNPRINTF(cli_err_str, MAX_STRLEN,
			  "Assignment is not allowed for this option : %s",
			  pparm->name);
			return(-1);
		}
	} else
	{
		/* --------------------------------------------------
		 * Get Value either optional, or required.
		 * In either case, there must be an assignment token
		 * --------------------------------------------------
		 */
		if (!cli_look_next_token(eof) || !cli_is_assign(cli_token_buf))
		{
	    		if (VAL_REQ == pparm->required)
			{
				SNPRINTF(cli_err_str, MAX_STRLEN, "Option : %s needs value", pparm->name);
				return(-1);
			} else
			{
				if (pparm->present)
				{
					/* The option was specified before, so clean up that one,
					 * the last one overrides
					 */
					if (pparm->pval_str)
						free(pparm->pval_str);
					pparm->negated = 0;
					if (pparm->qual_vals)
						clear_parm_vals(pparm->qual_vals, FALSE);

				}
				pparm->negated = neg_flg;
				pparm->present = 1;
				/* -------------------------------
				 * Allocate memory and save value
				 * -------------------------------
				 */
				if (pparm->parm_values)
				{
					pparm->pval_str = malloc(strlen(pparm->parm_values->prompt) + 1);
					strcpy(pparm->pval_str, pparm->parm_values->prompt);
					if (!cli_get_sub_quals(pparm))
						return(-1);
				}
				return(1);
			}
		}
 		cli_gettoken(eof);
		/* ---------------------------------
		 * Get the assignment token + value
		 * ---------------------------------
		 */
		if (!cli_is_assign(cli_token_buf))
		{
			SNPRINTF(cli_err_str, MAX_STRLEN,
			  "Assignment missing after option : %s",
			  pparm->name);
			return(-1);
		}
		/* --------------------------------------------------------
		 * get the value token, "=" is NOT a token terminator here
		 * --------------------------------------------------------
		 */
		if (!cli_look_next_string_token(eof) || 0 == cli_get_string_token(eof))
		{
			SNPRINTF(cli_err_str, MAX_STRLEN,
			  "Unrecognized option : %s, value expected but not found",
			  pparm->name);
			cli_lex_in_ptr->tp = 0;
			return(-1);
		}
		val_str = cli_token_buf;
		if (!cli_numeric_check(pparm, val_str))
		{
			cli_lex_in_ptr->tp = 0;
			return(-1);
		}
		if (pparm->present)
		{
			/* The option was specified before, so clean up that one,
			 * the last one overrides
			 */
			if (pparm->pval_str)
				free(pparm->pval_str);
			if (pparm->qual_vals)
				clear_parm_vals(pparm->qual_vals, FALSE);
		}
		/* -------------------------------
		 * Allocate memory and save value
		 * -------------------------------
		 */
		pparm->pval_str = malloc(strlen(cli_token_buf) + 1);
		strcpy(pparm->pval_str, cli_token_buf);

		if (!cli_get_sub_quals(pparm))
			return(-1);
	}
	if (pparm->present)
		pparm->negated = 0;
	pparm->negated = neg_flg;
	pparm->present = 1;

	return(1);
}

/*
 * -----------------------------------------------------
 * Check if the value is numeric if it is supposed to be
 *
 * Return:
 *	TRUE	- It is numeric or val_type is not
 *	          numeric anyway
 *	FALSE	- Is not numeric
 * -----------------------------------------------------
 */
boolean_t cli_numeric_check(CLI_ENTRY *pparm, char *val_str)
{
	boolean_t retval = TRUE;
	if (VAL_NUM == pparm->val_type)
	{
		if (pparm->hex_num)
		{
			if (!cli_is_hex(val_str))
			{
				SNPRINTF(cli_err_str, MAX_STRLEN,
				  "Unrecognized value: %s, HEX number expected",
				  val_str);
				retval = FALSE;
			}
		} else if (!cli_is_dcm(val_str))
		{
			SNPRINTF(cli_err_str, MAX_STRLEN,
			  "Unrecognized value: %s, Decimal number expected",
			  val_str);
			retval = FALSE;
		}
	}
	return(retval);
}

/*---------------------------
 * Check if option is negated
 *---------------------------
 */
int cli_check_negated(char **opt_str_ptr, CLI_ENTRY *pcmd_parm_ptr, CLI_ENTRY **pparm_ptr)
{
	int 		neg_flg;
	CLI_ENTRY	*pcmd_parms;
	char		*opt_str_tmp;

	pcmd_parms = pcmd_parm_ptr;
	opt_str_tmp = *opt_str_ptr;
	if (0 == MEMCMP_LIT(*opt_str_ptr, NO_STRING))
	{
		*opt_str_ptr += sizeof(NO_STRING) - 1;
		neg_flg = 1;
	} else
		neg_flg = 0;
	/* --------------------------------------------
	 * search qualifier table for qualifier string
	 * --------------------------------------------
	 */
	if (0 == (*pparm_ptr = find_cmd_param(*opt_str_ptr, pcmd_parms, FALSE)))
	{
		/* Check that the qualifier does not have the NO prefix */
		if (0 == (*pparm_ptr = find_cmd_param(opt_str_tmp, pcmd_parms, FALSE)))
		{
			SNPRINTF(cli_err_str, MAX_STRLEN, "Unrecognized option : %s", *opt_str_ptr);
			cli_lex_in_ptr->tp = 0;
			return(-1);
		} else
		{
			/* It was a valid qualifier with the prefix NO */
			*opt_str_ptr = opt_str_tmp;
			neg_flg = 0;
		}
	}
	/* ----------------------------------------------------
	 * if option is negated and it is not negatable, error
	 * ----------------------------------------------------
	 */
	if (!(*pparm_ptr)->negatable && neg_flg)
	{
		SNPRINTF(cli_err_str, MAX_STRLEN, "Option %s may not be negated", *opt_str_ptr);
		return(-1);
	}
	return neg_flg;
}

/*
 * --------------------------------------------------
 *
 * Process the qualifier to see if any extra parameters
 * are possible in the <...>
 * Return:
 *	TRUE	- OK
 *	FALSE	- Error
 * --------------------------------------------------
 */
boolean_t cli_get_sub_quals(CLI_ENTRY *pparm)
{
	CLI_ENTRY 	*pparm_qual, *pparm1;
	char		local_str[MAX_LINE], tmp_str[MAX_LINE], *tmp_str_ptr;
	char 		*ptr_next_val, *ptr_next_comma, *ptr_equal;
	int		len_str, neg_flg, ptr_equal_len;
	boolean_t	val_flg, has_a_qual;

	has_a_qual = FALSE;
	pparm_qual = pparm->qual_vals;
	if (!pparm_qual)
		return TRUE;
	gpcmd_qual_val = pparm; /*for future reference */
	if ((VAL_STR == pparm->val_type) || (VAL_LIST == pparm->val_type))
	{
		strncpy(local_str, pparm->pval_str, sizeof(local_str) - 1);
		ptr_next_val = local_str;
		while (NULL != ptr_next_val)
		{
			len_str=strlen(ptr_next_val);
			strncpy(tmp_str, ptr_next_val, len_str);
			cli_strupper(tmp_str);
			tmp_str[len_str] = 0;
			tmp_str_ptr = tmp_str;
			ptr_next_comma = strchr(tmp_str_ptr, ',');
			if (NULL == ptr_next_comma)
				ptr_next_comma = tmp_str_ptr + len_str;
			else
				*ptr_next_comma = 0;
			ptr_equal = strchr(tmp_str_ptr, '=');
			if (ptr_equal && (ptr_equal < ptr_next_comma) )
				*ptr_equal = 0;
			else
				ptr_equal = NULL;
			/* -------------------------
			 * See if option is negated
			 * -------------------------
			 */
			if (-1 == (neg_flg = cli_check_negated( &tmp_str_ptr, pparm_qual, &pparm1)))
				return FALSE;
			if ( 1 == neg_flg)
				len_str -=  strlen(NO_STRING);

			if ((ptr_equal) && (ptr_equal + 1 < ptr_next_comma))
				val_flg = TRUE;
			else
				val_flg = FALSE;
			/* -------------------------------------------------------------
			 * If value is disallowed for this qualifier, and an assignment
			 * is encountered, report error, values not allowed for
			 * negated qualifiers
			 * -------------------------------------------------------------
			 */
			if (neg_flg || VAL_DISALLOWED == pparm1->required)
			{
				if (val_flg)
				{
					SNPRINTF(cli_err_str, MAX_STRLEN,
					  "Assignment is not allowed for this option : %s",
					  pparm1->name);
					cli_lex_in_ptr->tp = 0;
					return FALSE;
				}
			} else
			{
				if ((!val_flg) && VAL_REQ == pparm1->required)
				{
					if (ptr_equal)
						SNPRINTF(cli_err_str, MAX_STRLEN,
						  "Unrecognized option : %s, value expected but not found",
						  pparm1->name);
					else
						SNPRINTF(cli_err_str, MAX_STRLEN, "Option : %s needs value", tmp_str_ptr);
					cli_lex_in_ptr->tp = 0;
					return FALSE;
				}
				if (!cli_numeric_check(pparm1, ptr_equal + 1))
				{
					cli_lex_in_ptr->tp = 0;
					return FALSE;
				}
				if (pparm1->present)
				{
					/* The option was specified before, so clean up that one,
					 * the last one overrides
					 */
					if (pparm1->pval_str)
						free(pparm1->pval_str);
				}
				if ((!val_flg) && (VAL_NOT_REQ == pparm1->required) && pparm1->parm_values)
				{
					pparm1->pval_str = malloc(strlen(pparm1->parm_values->prompt) + 1);
					strcpy(pparm1->pval_str, pparm1->parm_values->prompt);
				}
				if (val_flg)
				{
					ptr_equal_len = strlen(ptr_equal + 1);
					pparm1->pval_str = malloc(ptr_equal_len + 1);
					strncpy(pparm1->pval_str, ptr_next_val + (ptr_equal - tmp_str_ptr) + 1, ptr_equal_len);
					pparm1->pval_str[ptr_equal_len] = 0;
				}

			}
			if (pparm1->present)
				pparm->negated = 0;
			pparm1->negated = neg_flg;
			pparm1->present = CLI_PRESENT;
			has_a_qual = TRUE;
			if ((tmp_str_ptr + len_str) != ptr_next_comma)
			{
				ptr_next_val = strchr(ptr_next_val, ',')+ 1;
				if (!*ptr_next_val)
				{
					SNPRINTF(cli_err_str, MAX_STRLEN, "Option expected", tmp_str_ptr);
					cli_lex_in_ptr->tp = 0;
					return FALSE;

				}
			} else
				ptr_next_val = NULL;
		}
	}
	/* do one last parse on the qualifier table, if there is any other qualifier present,
	 * the DEFA_PRESENT one is not necessary, otherwise leave it as it is.
	 */
	if (has_a_qual)
	{
		pparm1 = pparm_qual;
		while (0 != *pparm1->name)
		{
			if (CLI_DEFAULT == pparm1->present)
				pparm1->present = CLI_ABSENT;
			pparm1++;
		}
	}

	return TRUE;
}

/*
 * -----------------------------------------------------------
 * Parse one command.
 * Get tokens from the input stream.
 * See if the first token is a command name, as it should be,
 * and if it is, check if optional arguments that follow,
 * are legal, and if they are, get their values and
 * save them in a value table, corresponding to this
 * option.
 * If any of these conditions are not met, parse error occures.
 *
 * Return:
 *	0 - command parsed OK
 *	EOF - end of file
 *	<> - failure to parse command
 * -----------------------------------------------------------
 */
int parse_cmd(void)
{
	int 	res, cmd_ind;
	char 	*cmd_str;
	int 	opt_cnt;
	int 	eof, cmd_err;
	error_def(ERR_CLIERR);

	opt_cnt = 0;
	gpqual_root = 0;
	func = 0;
	cmd_err = 0;
	parms_cnt = 0;			/* Parameters count */
	*cli_err_str = 0;

	cmd_str = cli_token_buf;
	/* ------------------------
	 * If no more tokens exist
	 * ------------------------
	 */
	if (0 == cli_gettoken(&eof))
	{
		if (eof)
			return(EOF);
		return(0);
	}
	/* ------------------------------
	 * Find command in command table
	 * ------------------------------
	 */
	if (-1 != (cmd_ind = find_verb(cmd_str)))
	{
		gpcmd_qual = cmd_ary[cmd_ind].parms;
		gpcmd_parm_vals = cmd_ary[cmd_ind].parm_values;
		gpcmd_verb = &cmd_ary[cmd_ind];
		if (gpcmd_qual)
			clear_parm_vals(gpcmd_qual, TRUE);
		func = cmd_ary[cmd_ind].func;
		/* ----------------------
		 * Parse command options
		 * ----------------------
		 */
		do
		{
			res = parse_arg(gpcmd_qual, &eof);
			if (1 == res)
			{
				opt_cnt++;
			}
		} while (1 == res);
	} else
	{
		SNPRINTF(cli_err_str, MAX_STRLEN, "Unrecognized command: %s", cmd_str);
		cli_lex_in_ptr->tp = 0;
		res = -1;
	}
	if (1 > opt_cnt && -1 != res
	    && VAL_REQ == cmd_ary[cmd_ind].required)
	{
		SNPRINTF(cli_err_str, MAX_STRLEN, "Command argument expected, but not found");
		res = -1;
	}
	/*------------------------------------------------------
	 * Check that the disallow conditions are met (to allow)
	 *------------------------------------------------------
	 */
	if ((-1 != res) && ((FALSE == check_disallow(gpcmd_verb)) || (FALSE == check_disallow(gpcmd_qual_val))))
		res = -1;
	/* -------------------------------------
	 * If parse error, display error string
	 * -------------------------------------
	 */
	if (-1 == res)
		func = 0;
	else
		return(0);
	/* -------------------------
	 * If gettoken returned EOF
	 * -------------------------
	 */
	if (eof)
		return(EOF);
	else
		return(ERR_CLIERR);
}

/*
 * ------------------------------------------------------------
 * See if command parameter is present on the command line,
 * and if it is, return the pointer to its table entry.
 *
 * Arguments:
 *	parm_str	- parameter string to search for
 *
 * Return:
 *	0 - not present
 *	pointer to parameter entry
 * ------------------------------------------------------------
 */
CLI_ENTRY *get_parm_entry(char *parm_str)
{
	CLI_ENTRY	*pparm;
	bool		root_match;
	char		local_str[MAX_LINE], *tmp_ptr;
	error_def(ERR_MUPCLIERR);

	strncpy(local_str, parm_str, sizeof(local_str) - 1);
	root_match = (gpqual_root && !strncmp(gpqual_root->name, local_str, strlen(local_str)));

	/* ---------------------------------------
	 * search qualifier table for this option
	 * ---------------------------------------
	 */

	if (!gpcmd_qual)
		return(NULL);
	if (NULL == strchr(local_str,'.'))
		pparm = find_cmd_param(local_str, gpcmd_qual, TRUE);
	else
	{
		tmp_ptr= strchr(local_str,'.');
		/* there should be at least 1 character after the . */
		assert (tmp_ptr + 1 < local_str + strlen(local_str));
		*tmp_ptr = 0;
		pparm = find_cmd_param(local_str, gpcmd_qual, FALSE);
		if (pparm)
			pparm = find_cmd_param(tmp_ptr+1, pparm->qual_vals, FALSE);

	}
	if (root_match && !pparm)
		return(gpqual_root);
	else if (pparm)
		return(pparm);
	else
		return(NULL);
}

/*
 * ------------------------------------------------------------
 * See if command parameter is present on the command line
 *
 * Arguments:
 *	entry	- parameter string to search for
 *
 * Return:
 *	0 - not present (i.e. CLI_ABSENT)
 *	<> 0 - present  (either CLI_PRESENT or CLI_NEGATED)
 * ------------------------------------------------------------
 */
int cli_present(char *entry)
{
	CLI_ENTRY	*pparm;
	char		local_str[MAX_LINE];

	strncpy(local_str, entry, sizeof(local_str) - 1);

	cli_strupper(local_str);
	if (pparm = get_parm_entry(local_str))
	{
		if (pparm->negated)
			return(CLI_NEGATED);
		if ((CLI_PRESENT == pparm->present) || (CLI_DEFAULT == pparm->present))
			return(CLI_PRESENT);
	}

	return(CLI_ABSENT);
}

/*
 * ------------------------------------------------------------
 * Get the command parameter value
 *
 * Arguments:
 *	entry		- parameter string to search for
 *	val_buf		- if parameter is present, it is copied to
 *			  this buffer.
 *
 * Return:
 *	0 - not present
 *	<> 0 - ok
 * ------------------------------------------------------------
 */
bool cli_get_value(char *entry, char val_buf[])
{
	CLI_ENTRY	*pparm;
	char		local_str[MAX_LINE];

	strncpy(local_str, entry, sizeof(local_str) - 1);
	cli_strupper(local_str);
	if (NULL == (pparm = get_parm_entry(local_str)))
		return(FALSE);
	if (!pparm->present || NULL == pparm->pval_str)
		return(FALSE);
	else
		strcpy(val_buf, pparm->pval_str);
	return(TRUE);
}

/*
 * --------------------------------------------------
 * See if the qualifier given by 'entry' is negated
 * on the command line
 *
 * Return:
 *	TRUE	- Negated
 *	FALSE	- otherwise
 * --------------------------------------------------
 */
boolean_t cli_negated(char *entry) 		/* entity */
{
	CLI_ENTRY	*pparm;
	char		local_str[MAX_LINE];

	strncpy(local_str, entry, sizeof(local_str) - 1);
	cli_strupper(local_str);
	if (pparm = get_parm_entry(local_str))
		return(pparm->negated);
	return(FALSE);
}


bool cli_get_parm(char *entry, char val_buf[])
{
	char		*sp;
	int		ind;
	int		match_ind, res;
	char		local_str[MAX_LINE];
	int		eof;
	char		*gets_res;
	int		parm_len;

	ind = 0;
	assert(0 != gpcmd_parm_vals);
	strncpy(local_str, entry, sizeof(local_str) - 1);
	cli_strupper(local_str);
	match_ind = -1;

	while (0 < strlen(sp = (gpcmd_parm_vals + ind)->name))
	{
		if (0 == (res = strncmp(sp, local_str, strlen(local_str))))
		{
			if (-1 != match_ind)
				return(FALSE);
			else
				match_ind = ind;
		} else
		{
			if (0 < res)
				break;
		}
		ind++;
	}
	if (-1 != match_ind)
	{
		/* ---------------------------
		 * If no value, prompt for it
		 * ---------------------------
		 */

		if (NULL == parm_ary[match_ind])
		{
			PRINTF("%s", (gpcmd_parm_vals + match_ind)->prompt);
			FGETS(local_str, MAX_LINE, stdin, gets_res);
			if (gets_res)
			{
				parm_len = strlen(gets_res);
				/* chop off newline */
				if (local_str[parm_len - 1] == '\n')
				{
					local_str[parm_len - 1] = '\0';
					--parm_len;
				}
				parm_ary[match_ind] = malloc(parm_len + 1);
				if (parm_len)
					memcpy(parm_ary[match_ind], &local_str[0], parm_len);
				*(parm_ary[match_ind] + parm_len + 1) = '\0';
			} else
				return FALSE;
		} else if ((char *)-1 == parm_ary[match_ind])
			return(FALSE);
		memcpy(val_buf, parm_ary[match_ind], strlen(parm_ary[match_ind]) + 1);
		if (!cli_look_next_token(&eof) || (0 == cli_gettoken(&eof)))
			parm_ary[match_ind] = (char *)-1;
		else
		{
			parm_len = strlen(cli_token_buf) + 1;
			if (MAX_LINE < parm_len)
			{
				PRINTF("Parameter string too long\n");
				return(FALSE);
			}
			if (parm_ary[match_ind])
				free(parm_ary[match_ind]);
			parm_ary[match_ind] = malloc(parm_len);
			memcpy(parm_ary[match_ind], cli_token_buf, parm_len);
		}
	} else
	{
		/* -----------------
		 * check qualifiers
		 * -----------------
		 */
		if (!cli_get_value(local_str, val_buf))
			return(FALSE);
	}
	return(TRUE);
}
