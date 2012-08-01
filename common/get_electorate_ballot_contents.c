/* This file is (C) copyright 2001 Software Improvements, Pty Ltd */

/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#include <stdlib.h>
#include <assert.h>
#include "evacs.h"
#include "database.h"
#include "ballot_contents.h"
#include "get_electorate_ballot_contents.h"

struct ballot_contents *get_electorate_ballot_contents(PGconn *conn,
						       unsigned int electorate_code)
/* fill a ballot_contents structure with database values */
{
	struct ballot_contents *ret;
	PGresult *result;
	unsigned int num_rows,i;

	/* retrieve candidate count for each group from database */

	result = SQL_query(conn,
			   "SELECT p.index, count(c.name) "
			   "FROM party p, candidate c "
                           "WHERE p.electorate_code = %u "
			   "AND c.electorate_code = %u "
			   "AND c.party_index = p.index "
			   "GROUP BY p.index;"
			   ,electorate_code,electorate_code);
	
	num_rows = PQntuples(result);
	if (num_rows == 0) bailout("No candidates returned from DB");
	
	ret = malloc(sizeof(*ret) + sizeof(ret->num_candidates[0]) *(num_rows+1));  
	/* fill in the data structure */
	ret->num_groups = num_rows;
	
	for (i=0;i<num_rows;i++) {
		ret->num_candidates[i] = atoi(PQgetvalue(result, i, 1));
	}
	PQclear(result);
	return ret;
}
