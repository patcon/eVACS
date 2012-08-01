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
#include <common/database.h>
#include <common/evacs.h>
#include <common/batch.h>
#include "handle_few_votes.h"
#include "check_votes.h"


/* Get votes from database */
static struct vote_list *get_votes(unsigned int *num_rows,
				   unsigned int *polling_place_code,
				   unsigned int csc_code)
{
	PGconn *conn=connect_db(LOAD1DB_NAME);
	PGresult *result;
	unsigned int i;
	struct vote_list *votes;

	/* Get the polling place code for this loaded data */
	*polling_place_code = SQL_singleton_int(conn,
						"SELECT polling_place_code "
						"FROM confirmed_vote "
						"WHERE id = "
						" (SELECT MIN(id) "
						  "FROM confirmed_vote);");
	/*
	  Update polling place of all votes when the number of votes that
	   that electorate total less than 20.
	*/

	/* DDS3.8: Save Polling Place with Few Votes */
	/* DDS3.8: Get Polling Place with Few Votes */
	begin(conn);
	i = SQL_command(conn,
		    "UPDATE confirmed_vote "
		    "SET polling_place_code = %u "
		    "WHERE electorate_code "
		    "IN (SELECT electorate_code "
		         "FROM confirmed_vote cv "
		         "WHERE 20 > (SELECT COUNT(*) "
		                     "FROM confirmed_vote "
		                     "WHERE electorate_code = "
 		                           "cv.electorate_code));",
		    csc_code);

	/* Load our 'view' of the database - with the updated rows */
	result = SQL_query(conn,
			   "SELECT id,polling_place_code, "
			   "electorate_code "
			   "FROM confirmed_vote;");

	*num_rows = PQntuples(result);

	votes = malloc(sizeof(*votes) + 
		       sizeof(struct vote) * *num_rows);

	for (i=0; i<*num_rows; i++) {
		votes->vote[i].polling_place_code 
			= atoi(PQgetvalue(result, i, 1));
		votes->vote[i].electorate_code 
			= atoi(PQgetvalue(result,i,2));
		votes->vote[i].preferences 
			= get_prefs_for_vote(conn,
					     atoi(PQgetvalue(result,i,0)));
	}
	PQclear(result);
	/* Don't permanently effect the load database! */
	rollback(conn);
	PQfinish(conn);

	return votes;
}

/* DDS3.8: Copy Vote */
static void copy_vote(PGconn *conn, struct vote vote)
{
	unsigned int i;
	
	SQL_command(conn,
		    "INSERT INTO confirmed_vote"
		    "(electorate_code,polling_place_code) "
		    "VALUES(%u,%u);",
		    vote.electorate_code,vote.polling_place_code);
	/* And in the confirmed vote table summary if required */
	if ( SQL_singleton_int(conn,"SELECT COUNT(*) "
			       "FROM vote_summary "
			       "WHERE electorate_code = %d "
			       "AND polling_place_code = %d;",
			       vote.electorate_code,vote.polling_place_code)
	     == 0)
		SQL_command(conn,"INSERT INTO vote_summary "
			    "(electorate_code,polling_place_code,"
			    "entered_by,entered_at,informal_count) "
			    "VALUES (%d,%d,'EVACS','NOW',0);",
			    vote.electorate_code,vote.polling_place_code);

	/* If no preferences - increment number of informal votes */
	if (vote.preferences.num_preferences == 0)
		SQL_command(conn,"UPDATE vote_summary "
			    "SET informal_count = informal_count + 1,"
			        "entered_by = 'EVACS',"
			        "entered_at = 'NOW' "
			    "WHERE electorate_code = %d "
			    "AND polling_place_code = %d;",
			    vote.electorate_code,vote.polling_place_code);
	for (i=0;i<vote.preferences.num_preferences;i++) {
		SQL_command(conn,
			    "INSERT INTO confirmed_preference"
			    "(group_index,db_candidate_index,prefnum) "
			    "VALUES(%u,%u,%u);",
			    vote.preferences.candidates[i].group_index,
			    vote.preferences.candidates[i].db_candidate_index,
			    vote.preferences.candidates[i].prefnum);

		/* Summarise first preferences for election night */
		if ( vote.preferences.candidates[i].prefnum == 1) {
			if (SQL_singleton_int(conn,"SELECT COUNT(*) "
					   "FROM preference_summary "
					   "WHERE electorate_code = %d "
				           "AND polling_place_code = %d "
				   	   "AND party_index = %d "
				           "AND candidate_index = %d;",
			     vote.electorate_code,
			     vote.polling_place_code,
			     vote.preferences.candidates[i].group_index,
			     vote.preferences.candidates[i].db_candidate_index)
			    == 0)
				SQL_command(conn,"INSERT INTO "
					    "preference_summary"
					    "(electorate_code,"
					    "polling_place_code,party_index,"
					    "candidate_index,phoned_primary,"
					    "evacs_primary,final_count) "
					    "VALUES(%d,%d,%d,%d,0,1,0);",
			    vote.electorate_code,
			    vote.polling_place_code,
			    vote.preferences.candidates[i].group_index,
			    vote.preferences.candidates[i].db_candidate_index);
			else
				SQL_command(conn,"UPDATE "
					    "preference_summary "
					    "SET evacs_primary = "
					        "evacs_primary + 1 "
					    "WHERE electorate_code = %d "
					    "AND polling_place_code = %d "
					    "AND party_index = %d "
					    "AND candidate_index = %d;",
			    vote.electorate_code,
			    vote.polling_place_code,
			    vote.preferences.candidates[i].group_index,
			    vote.preferences.candidates[i].db_candidate_index);
			SQL_command(conn,"UPDATE vote_summary "
				    "SET entered_by = 'EVACS',"
				        "entered_at = 'NOW' "
				    "WHERE electorate_code = %d "
				    "AND polling_place_code = %d;",
				    vote.electorate_code,
				    vote.polling_place_code);
			}
	}
}

/* DDS3.8: Handle Few Votes */
void handle_few_votes(void)
{
	struct vote_list *votes;
	unsigned int num_votes, i,polling_place_code;
	int num_rows,csc_code;

	PGconn *conn=connect_db(DATABASE_NAME);
	
	/* Get polling place to use when less than 20 votes in electorate */
	csc_code = resolve_polling_place_code(conn, "Central Scrutiny");

	if ( csc_code < 0 )
		/* The fallback CSC code is .. */
		csc_code = 400;
		/* bailout("Could not find code for Central Scrutiny "
		   "polling place"); */

	/* Get votes structure from the load database */
	votes = get_votes(&num_votes,&polling_place_code,
			  (unsigned int)csc_code);

	/* Start the transaction */
	begin(conn);

	/* Prevent race condition in SELECT - INSERT or UPDATE code */
	/* This won't block readers - only other writers */
	SQL_command(conn,"LOCK TABLE vote_summary IN EXCLUSIVE MODE;");
	SQL_command(conn,"LOCK TABLE preference_summary IN EXCLUSIVE MODE;");


	/* Insert votes into counting database */
	for (i=0; i<num_votes; i++)
		copy_vote(conn, votes->vote[i]);
	free(votes);

	/* Mark polling place as loaded */
	num_rows = SQL_command(conn,"UPDATE polling_place "
			       "SET loaded = true "
			       "WHERE code = %u "
			       "AND loaded = false;",
			       polling_place_code);
	/* Sanity check - check that only one row was updated */
	if ( num_rows != 1 ) {
		rollback(conn);
		PQfinish(conn);
		if (num_rows == 0)
			bailout("This polling place has already been "
				"loaded!\n");
		else
			bailout("More than one entry exists for polling "
				"place code %u.\n",polling_place_code);
	}

	/* End the transaction */
	commit(conn);
       	PQfinish(conn);
	printf("Number of votes loaded = %u\n",num_votes);
}





