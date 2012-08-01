/* This file is (C) copyright 2001 Software Improvements, Pty Ltd.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include <stdlib.h>
#include <common/barcode_hash.h>
#include "voting_server.h"
#include "save_and_verify.h"

/* DDS3.2.22: Primary Store */
static enum error primary_store_start(PGconn *conn,
				      const struct preference_set *vote,
				      const struct barcode *bc,
				      const struct electorate *elec)
{
	char hash[HASH_BITS + 1];
	int i,pp_code;
	unsigned int num_rows;

	gen_hash(hash, bc->data, sizeof(bc->data));
	/* Get polling_place_code */
	pp_code = (unsigned int)SQL_singleton_int(conn,
						  "SELECT polling_place_code "
						  "FROM server_parameter;");
	if (pp_code < 0)
		bailout("Primary store failed. Polling Place code not "
			"found in server_parameter table.");

	/* begin transaction */
	begin(conn);

	/* Mark barcode as used */
	num_rows = SQL_command(conn,
			       "UPDATE barcode "
			       "SET used = true "
			       "WHERE hash = '%s' "
			       "AND used = false;",hash);

	/* Check barcode exists and has not been used */
	if (num_rows != 1 )
		return(ERR_COMMIT_FAILED);

	/* Store the vote */
	num_rows = SQL_command(conn,
			       "INSERT INTO confirmed_vote"
			       "(electorate_code,polling_place_code) "
			       "VALUES(%u,%u);",
			       elec->code,pp_code);

	/* Sanity check  */
	if ( num_rows != 1 )
		return(ERR_COMMIT_FAILED);

	/* INSERT the preferences for the vote */
	for (i=0;i<vote->num_preferences;i++) {
		num_rows = 
			SQL_command(conn,
				    "INSERT INTO confirmed_preference"
				    "(group_index,db_candidate_index,prefnum) "
				    "VALUES(%u,%u,%u);",
				    vote->candidates[i].group_index,
				    vote->candidates[i].db_candidate_index,
				    vote->candidates[i].prefnum);
		/* Sanity check  */
		if ( num_rows != 1 )
			return(ERR_COMMIT_FAILED);
	}
	
	/* Error if not EXACTLY one row updated */
	return ((num_rows==1)?ERR_OK:ERR_COMMIT_FAILED);
}

static enum error primary_store_commit(PGconn *conn)
{
	commit(conn);
	return ERR_OK;
}

static void primary_store_abort(PGconn *conn)
{
	rollback(conn);
}

/* DDS3.2.22: Secondary Store */
static enum error secondary_store(const struct http_vars *vars)
{
	struct http_vars *retvars;
	enum error ret;

	if (!am_i_master())
		/* I am the slave, so this is the secondary store */
		return ERR_OK;

	/* Send to slave for secondary store */
	retvars = http_exchange(SLAVE_SERVER_ADDRESS, SLAVE_SERVER_PORT,
				"/cgi-bin/commit_vote", vars);
	if (!retvars)
		ret = ERR_SERVER_UNREACHABLE;
	else {
		ret = http_error(retvars);
		http_free(retvars);
	}

	return ret;
}

/* DDS3.2.22: Save And Verify Vote */
enum error save_and_verify(PGconn *conn,
			   const struct preference_set *vote,
			   const struct barcode *bc,
			   const struct electorate *elec,
			   const struct http_vars *vars)
{
	enum error err;

	err = primary_store_start(conn, vote, bc, elec);
	if (err != ERR_OK) return err;

	err = secondary_store(vars);
	if (err != ERR_OK)
		primary_store_abort(conn);
	else
		err = primary_store_commit(conn);
	return err;
}
