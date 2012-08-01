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

/* This CGI is invoked by the client to authenticate the barcode and
   map it to the electorate */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <pgsql/libpq-fe.h>
#include <common/rotation.h>
#include <common/barcode.h>
#include <common/barcode_hash.h>
#include <common/database.h>
#include <common/http.h>
#include <common/safe.h>
#include "voting_server.h"
#include "cgi.h"

struct barcode_hash_entry
{
	unsigned int ecode;
	unsigned int ppcode;
	bool used;
	char barcodehash[HASH_BITS + 1];
};

/* DDS3.2.4: Get Barcode Hash Table */
/* Returns electorate code, or NULL if not found */
static struct barcode_hash_entry *get_bhash_table(PGconn *conn,
						  const char *barcodehash)
{
	struct barcode_hash_entry *ret=NULL;
	PGresult *result=NULL;

	result = SQL_query(conn,
			   "SELECT electorate_code,polling_place_code, used "
			   "FROM barcode "
			   "WHERE hash = '%s';",barcodehash);
	
	if (PQntuples(result) >= 1) {
	  ret = malloc(sizeof(*ret));
	  ret->ecode = atoi(PQgetvalue(result,0,0));
	  ret->ppcode = atoi(PQgetvalue(result,0,1));
	  /* Booleans are either "t" or "f" */
	  ret->used = (PQgetvalue(result,0,2)[0] == 't');
	  strcpy(ret->barcodehash,barcodehash);
	}
	PQclear(result);

	return ret;
}

/* DDS????: Get Ballot Contents */
static struct http_vars *get_ballot_contents(PGconn *conn,unsigned int ecode,
					     struct http_vars *vars)
{
        PGresult *result;
	unsigned int num_groups,group;

	result = SQL_query(conn,
			   "SELECT party_index,count(index) FROM candidate "
			   "WHERE electorate_code = %u "
			   "GROUP BY party_index;",ecode);

	if ( (num_groups = PQntuples(result)) == 0 )
	  bailout("get_ballot_contents failed. "
		  "No groups found for this electorate.\n");

	vars = realloc(vars, sizeof(*vars) * (3 + 1 + num_groups + 1));

	vars[3].name = strdup("num_groups");
	vars[3].value = sprintf_malloc("%u", num_groups);

	for (group=0;group<num_groups;group++) {
	  vars[group+4].name = sprintf_malloc("group%s",PQgetvalue(
							 result,group,0));
	  vars[group+4].value = strdup(PQgetvalue(result,group,1));
	}
	vars[group+4].name = vars[group+4].value = NULL;

	PQclear(result);

	return vars;
}

/* Send the electorate and ballot contents */
static struct http_vars *create_response(PGconn *conn, struct electorate *elec)
{
	struct http_vars *vars;

	/* Start with the electorate information */
	vars = malloc(sizeof(*vars) * 3);
	vars[0].name = strdup("electorate_name");
	vars[0].value = strdup(elec->name);
	vars[1].name = strdup("electorate_code");
	vars[1].value = sprintf_malloc("%u", elec->code);
	vars[2].name = strdup("electorate_seats");
	vars[2].value = sprintf_malloc("%u", elec->num_seats);

	/* Get the ballot contents stuff. */
	return get_ballot_contents(conn, elec->code,vars);
}

/* DDS3.2.3: Authenticate */
int main(int argc, char *argv[])
{
	struct http_vars *vars;
	struct barcode_hash_entry *bcentry;
	struct barcode bc;
	char bchash[HASH_BITS+1];
	struct electorate *elecs, *i;
	PGconn *conn;
	int ppcode;

	/* Our own failure function */
	set_cgi_bailout();

	/* Can be called on slave as well as master */
	conn = connect_db_port("evacs", get_database_port());
	if (!conn) bailout("Could not open database connection\n");

	/* Copy barcode ascii code from POST arguments */
	vars = cgi_get_arguments();
	strncpy(bc.ascii, http_string(vars, "barcode"), sizeof(bc.ascii)-1);
	bc.ascii[sizeof(bc.ascii)-1] = '\0';
	http_free(vars);

	/* Extract data and checksum from ascii */
	if (!bar_decode_ascii(&bc))
		cgi_error_response(ERR_BARCODE_MISREAD);

	/* Hash the barcode to look up in the table */
	gen_hash(bchash, bc.data, sizeof(bc.data));

	bcentry = get_bhash_table(conn, bchash);
	if (!bcentry) {
		PQfinish(conn);
		fprintf(stderr, "Barcode `%s' not found\n", bc.ascii);
		cgi_error_response(ERR_BARCODE_AUTHENTICATION_FAILED);
	}

	/* DDS3.2.4: Check Unused */
	if (bcentry->used) {
		PQfinish(conn);
		fprintf(stderr, "Barcode `%s' already used\n", bc.ascii);
		cgi_error_response(ERR_BARCODE_USED);
	}

	ppcode = SQL_singleton_int(conn,"SELECT polling_place_code "
				   "FROM server_parameter;");
	if (ppcode < 0) {
		PQfinish(conn);
		cgi_error_response(ERR_SERVER_INTERNAL);
	}

	if (ppcode != bcentry->ppcode) {
		PQfinish(conn);
		cgi_error_response(ERR_BARCODE_PP_INCORRECT);
	}

	elecs = get_electorates(conn);
	for (i = elecs; i; i = i->next) {
		if (i->code == bcentry->ecode) {
			/* Found it! */
			vars = create_response(conn, i);
			free_electorates(elecs);
			PQfinish(conn);
			cgi_good_response(vars);
		}
	}

	/* Should never happen */
	free_electorates(elecs);
	PQfinish(conn);
	bailout("Barcode electorate %u not found\n", bcentry->ecode);
}

