/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"

LITDEF	err_msg gdeerrors[] = {
	{ "BLKSIZ512", "Block size !AD rounds to !AD", 4 },
	{ "EXECOM", "Executing command file !AD", 2 },
	{ "FILENOTFND", "File !AD not found", 2 },
	{ "GDCREATE", "Creating Global Directory file !/	!AD", 2 },
	{ "GDECHECK", "Internal GDE consistency check", 0 },
	{ "GDUNKNFMT", "!AD !/	is not formatted as a Global Directory", 2 },
	{ "GDUPDATE", "Updating Global Directory file !/	!AD", 2 },
	{ "GDUSEDEFS", "Using defaults for Global Directory !/	!AD", 2 },
	{ "ILLCHAR", "!AD is not a legal character in this context", 2 },
	{ "INPINTEG", "Input integrity error -- aborting load", 0 },
	{ "KEYTOOBIG", "But record size !AD can only support key size !AD", 4 },
	{ "KEYSIZIS", "Key size is !AD", 2 },
	{ "KEYWRDAMB", "!AD is ambiguous for !AD", 4 },
	{ "KEYWRDBAD", "!AD is not a valid !AD in this context", 4 },
	{ "LOADGD", "Loading Global Directory file !/	!AD", 2 },
	{ "LOGOFF", "No longer logging to file !AD", 2 },
	{ "LOGON", "Logging to file !AD", 2 },
	{ "LVSTARALON", "The * name cannot be deleted or renamed", 0 },
	{ "MAPBAD", "!AD !AD for !AD !AD does not exist", 8 },
	{ "MAPDUP", "!AD !AD and !AD both map to !AD !AD", 10 },
	{ "NAMENDBAD", "Subscripted name !AD must end with right parenthesis", 2 },
	{ "NOACTION", "Not updating Global Directory !AD", 2 },
	{ "RPAREN", "List must end with right parenthesis or continue with comma", 0 },
	{ "NOEXIT", "Cannot exit because of verification failure", 0 },
	{ "NOLOG", "Logging is currently disabled!/ Log file is !AD.", 2 },
	{ "NOVALUE", "Qualifier !AD does not take a value", 2 },
	{ "NONEGATE", "Qualifier !AD cannot be negated", 2 },
	{ "OBJDUP", "!AD !AD already exists", 4 },
	{ "OBJNOTADD", "Not adding !AD !AD", 4 },
	{ "OBJNOTCHG", "Not changing !AD !AD", 4 },
	{ "OBJNOTFND", "!AD !AD does not exist", 4 },
	{ "OBJREQD", "!AD required", 2 },
	{ "PREFIXBAD", "!AD - !AD !AD must start with an alphabetic character", 6 },
	{ "QUALBAD", "!AD is not a valid qualifier", 2 },
	{ "QUALDUP", "!AD qualifier appears more than once in the list", 2 },
	{ "QUALREQD", "!AD required", 2 },
	{ "RECTOOBIG", "Block size !AD and !AD reserved bytes limit record size to !AD", 6 },
	{ "RECSIZIS", "Record size is !AD", 2 },
	{ "REGIS", "in region !AD", 2 },
	{ "SEGIS", "in !AD segment !AD", 4 },
	{ "VALTOOBIG", "!AD is larger than the maximum of !AD for a !AD", 6 },
	{ "VALTOOLONG", "!AD exceeds the maximum length of !AD for a !AD", 6 },
	{ "VALTOOSMALL", "!AD is less than the minimum of !AD for a !AD", 6 },
	{ "VALUEBAD", "!AD is not a valid !AD", 4 },
	{ "VALUEREQD", "Qualifier !AD requires a value", 2 },
	{ "VERIFY", "Verification !AD", 2 },
	{ "BUFSIZIS", "Journal Buffer size is !AD", 2 },
	{ "BUFTOOSMALL", "But block size !AD requires buffer size !AD", 4 },
	{ "MMNOBEFORIMG", "MM segments do not support before image jounaling", 0 },
	{ "NOJNL", "!AD segments do not support journaling", 2 },
	{ "GDREADERR", "Error reading Global Directory: !AD", 2 },
	{ "GDNOTSET", "Global Directory not changed because the current GD cannot be written", 0 },
	{ "INVGBLDIR", "Invalid Global Directory spec: !AD.!/Continuing with !AD", 4 },
	{ "WRITEERROR", "Cannot exit because of write failure.  Reason for failure: !AD", 2 },
	{ "NONASCII", "!AD is illegal for a !AD as it contains non-ASCII characters", 4 },
	{ "GDECRYPTNOMM", "!AD segment has encryption turned on. Cannot support MM access method.", 2 },
	{ "JNLALLOCGROW", "Increased Journal ALLOCATION from [!AD blocks] to [!AD blocks] to match AUTOSWITCHLIMIT for !AD !AD", 8 },
	{ "KEYFORBLK", "But block size !AD and reserved bytes !AD limit key size to !AD", 6 },
	{ "STRMISSQUOTE", "Missing double-quote at end of string specification !AD", 2 },
	{ "GBLNAMEIS", "in gblname !AD", 2 },
	{ "NAMSUBSEMPTY", "Subscript #!UL is empty in name specification", 1 },
	{ "NAMSUBSBAD", "Subscript #!UL with value !AD in name specification is an invalid number or string", 3 },
	{ "NAMNUMSUBSOFLOW", "Subscript #!UL with value !AD in name specification has a numeric overflow", 3 },
	{ "NAMNUMSUBNOTEXACT", "Subscript #!UL with value !AD in name specification is not an exact GT.M number", 3 },
	{ "MISSINGDELIM", "Delimiter !AD expected before !AD !AD", 6 },
	{ "NAMRANGELASTSUB", "Ranges in name specification !AD are allowed only in the last subscript", 2 },
	{ "NAMSTARSUBSMIX", "Name specification !AD cannot contain * and subscripts at the same time", 2 },
	{ "NAMLPARENNOTBEG", "Subscripted Name specification !AD needs to have a left parenthesis at the beginning of subscripts", 2 },
	{ "NAMRPARENNOTEND", "Subscripted Name specification !AD cannot have anything following the right parenthesis at the end of subscripts", 2 },
	{ "NAMONECOLON", "Subscripted Name specification !AD must have at most one colon (range) specification", 2 },
	{ "NAMRPARENMISSING", "Subscripted Name specification !AD is missing one or more right parentheses at the end of subscripts", 2 },
	{ "NAMGVSUBSMAX", "Subscripted Name specification !AD has more than the maximum # of subscripts (!UL)", 3 },
	{ "NAMNOTSTRSUBS", "Subscript #!UL with value !AD in name specification is not a properly formatted string subscript", 3 },
	{ "NAMSTRSUBSFUN", "Subscript #!UL with value !AD in name specification uses function other than $C/$CHAR/$ZCH/$ZCHAR", 3 },
	{ "NAMSTRSUBSLPAREN", "Subscript #!UL with value !AD in name specification does not have left parenthesis following $ specification", 3 },
	{ "NAMSTRSUBSCHINT", "Subscript #!UL with value !AD in name specification does not have a positive integer inside $C/$CHAR/$ZCH/$ZCHAR", 3 },
	{ "NAMSTRSUBSCHARG", "Subscript #!UL with value !AD in name specification specifies a $C/$ZCH with number !UL that is invalid in the current $zchset", 4 },
	{ "GBLNAMCOLLUNDEF", "Error opening shared library of collation sequence #!UL for GBLNAME !AD", 3 },
	{ "NAMRANGEORDER", "Range in name specification !AD specifies out-of-order subscripts using collation sequence #!UL", 3 },
	{ "NAMRANGEOVERLAP", "Range in name specifications !AD and !AD overlap using collation sequence #!UL", 5 },
	{ "NAMGVSUBOFLOW", "Subscripted name !AD...!AD is too long to represent in the database using collation value #!UL", 5 },
	{ "GBLNAMCOLLRANGE", "Collation sequence #!UL is out of range (0 thru 255)", 1 },
	{ "STDNULLCOLLREQ", "Region !AD needs Standard Null Collation enabled because global !AD spans through it", 4 },
	{ "GBLNAMCOLLVER", "Global directory indicates GBLNAME !AD has collation sequence #!UL with a version #!UL but shared library reports different version #!UL", 5 },
	{ "GDEASYNCIONOMM", "!AD segment has ASYNCIO turned on. Cannot support MM access method.", 2 },
	{ "NOPERCENTY", "^%Y* is a reserved global name in YottaDB", 0 },
};



LITDEF	int gdeerrors_undocarr[] = {
};


GBLDEF	err_ctl gdeerrors_ctl = {
	248,
	"GDE",
	&gdeerrors[0],
	86,
	&gdeerrors_undocarr[0],
	0
};

