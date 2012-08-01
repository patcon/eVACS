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
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <common/evacs.h>
#include <common/database.h>
#include <common/safe.h>

#include "createtables.h"

/* Create a database */
void create_database(PGconn *conn, const char *name)
{
	SQL_command(conn,"CREATE DATABASE %s;",name);
}

/* Drop a database */
void drop_database(PGconn *conn, const char *name)
{
	SQL_command_nobail(conn,"DROP DATABASE %s;",name);
}

void create_barcode_table(PGconn *conn)
     /*
       Create table to hold barcodes on polling place server.
       The additional field used is added to flag used barcodes.
     */

{
        drop_table(conn,"barcode");

        create_table(conn, "barcode",
		     "hash bit("STRINGIZE(HASH_BITS)") NOT NULL PRIMARY KEY,"
		     "polling_place_code INTEGER NOT NULL "
		     "REFERENCES polling_place(code),"
		     "electorate_code INTEGER NOT NULL "
		     "REFERENCES electorate(code),"
		     "used BOOL NOT NULL DEFAULT FALSE");
	create_index(conn,"barcode","electorate_code",
		     "barcode_ecode_idx");
	create_index(conn,"barcode","polling_place_code",
		     "barcode_ppcode_idx");
}

void create_confirmed_vote_table(PGconn *conn)
     /*
       Create the confirmed vote table on the polling place server.

       NOTE: Use of SERIAL replaced with explicit sequence creation
       and references in DEFAULT and CHECK constraints. This frees
       us from the reliance on Postgres creating the implicit
       sequence of a 'known' name.
       The CHECK constraint further inforces the rule that this
       table can only ever be INSERTed into.
     */
{
        drop_table(conn,"confirmed_vote");
	drop_sequence(conn,"confirmed_vote_id_seq");
	
	create_sequence(conn,"confirmed_vote_id_seq");
	create_table(conn,"confirmed_vote",
		     /* "id SERIAL PRIMARY KEY," */
		     "id INTEGER NOT NULL PRIMARY KEY "
		     "DEFAULT NEXTVAL('confirmed_vote_id_seq'),"
		     "electorate_code INTEGER NOT NULL "
		     "REFERENCES electorate(code),"
		     "polling_place_code INTEGER NOT NULL");
	create_index(conn,"confirmed_vote","electorate_code",
		     "vote_electorate_idx");
}

void create_confirmed_preference_table(PGconn *conn)
     /*
       Create the confirmed preference table on the polling place server.

       NOTE: vote_id is DEFAULTed to the current value of the 
       confirmed_vote_id_seq sequence and enforced with a CHECK
       constraint. This allowed the voting server to simply insert into
       confirmed_vote once then insert each prefence into this table
       for each preference without explicitly referring to the index or
       vote_id columns.
       The CHECK constraint further inforces the rule that this
       table can only ever be INSERTed into.
     */
{
        drop_table(conn,"confirmed_preference");
	
	create_table(conn,"confirmed_preference",
		     "vote_id INTEGER NOT NULL "
		     "REFERENCES confirmed_vote(id) "
		     "DEFAULT CURRVAL('confirmed_vote_id_seq'),"
		     "group_index INTEGER NOT NULL,"
		     "db_candidate_index INTEGER NOT NULL,"
		     "prefnum INTEGER NOT NULL "
		     "CHECK(prefnum BETWEEN 1 AND 99),"
		     "PRIMARY KEY (vote_id,prefnum)");
}

void create_robson_rotation_table(PGconn *conn,unsigned int number_of_seats)
     /*
       Create a robson_rotation table in the database
       for the number of seats indicated.
     */
{
        char *table_name,*def;

	table_name = sprintf_malloc("robson_rotation_%u",number_of_seats);
        drop_table(conn,table_name);

	/* NOTE: cannot do CHECK constraints on arrays */
	
	def = sprintf_malloc("rotation_num INTEGER NOT NULL PRIMARY KEY,"
			     "rotation INTEGER[%u] NOT NULL",
			     number_of_seats);
	create_table(conn,table_name,def);

	free(def);
	free(table_name);
}

void create_electorate_table(PGconn *conn)
{
        drop_table(conn,"electorate");
	create_table(conn,"electorate",
		     "code INTEGER PRIMARY KEY,"
		     "name TEXT NOT NULL UNIQUE,"
		     "seat_count INTEGER NOT NULL,"
		     "number_of_electors INTEGER,"
		     "colour TEXT");
}

void create_party_table(PGconn *conn)
{
        drop_table(conn,"party");
	create_table(conn,"party",
		     "electorate_code INTEGER NOT NULL "
		     "REFERENCES electorate(code),"
		     "index INTEGER NOT NULL,"
		     "name TEXT NOT NULL,"
		     "abbreviation TEXT,"
		     "PRIMARY KEY(electorate_code,index)");
	create_index(conn,"party","electorate_code","party_ecode_idx");
}

void create_candidate_table(PGconn *conn)
{
	drop_table(conn,"candidate");
	create_table(conn,"candidate",
		     "electorate_code INTEGER NOT NULL,"
		     "party_index INTEGER NOT NULL,"
		     "index INTEGER NOT NULL,"
		     "name TEXT NOT NULL,"
		     "sitting_member BOOL DEFAULT false,"
		     "FOREIGN KEY (electorate_code,party_index) "
		     "REFERENCES party(electorate_code,index),"
		     "PRIMARY KEY(electorate_code,party_index,index)");
	create_index(conn,"candidate","electorate_code,party_index",
		     "candidate_ep_idx");
}

void create_ballot_content_tables(PGconn *conn)
     /*
       Create electorate_detail, group and candidate tables.

       party used instead of group due to Postgres naming
       conventions.

       NOTE: Ignore failure of DROP TABLE commands.
     */
{
        create_electorate_table(conn);
	create_party_table(conn);
	create_candidate_table(conn);
}

void create_server_parameter_table(PGconn *conn)
/*
  Create table on polling place server to hold server related inforation,
  such as polling_place_code.
  This table will only ever contain one row, this is enforced by a check
  constraint.
*/
{
	drop_table(conn,"server_parameter");
	create_table(conn,"server_parameter",
		     "id INTEGER NOT NULL PRIMARY KEY "
		     "DEFAULT 1 CHECK (id=1),"
		     "polling_place_code INTEGER NOT NULL");
}

void create_batch_table(PGconn *conn)
     /* 
       Create batch table.

       NOTE: Ignore failure of DROP TABLE commands.
     */
{
        drop_table(conn,"batch");
	create_table(conn,"batch",
		     "number INTEGER NOT NULL PRIMARY KEY,"
		     "polling_place_code INTEGER NOT NULL "
		     "REFERENCES polling_place(code),"
		     "electorate_code INTEGER NOT NULL "
		     "REFERENCES electorate(code),"
		     "size INTEGER,"
		     "num_papers INTEGER NOT NULL DEFAULT 0,"
		     "committed BOOL NOT NULL DEFAULT false");
	create_index(conn,"batch","polling_place_code","batch_pp_code_idx");
	create_index(conn,"batch","electorate_code","batch_ecode_idx");
}

void create_paper_table(PGconn *conn)
/*
  Creating this table also creates a sequence called paper_id_seq
  to maintain the serial id column.

  NOTE: Use of SERIAL replaced with explicit sequence creation
  and referenced in DEFAULT and CHECK constraint. This frees
  us from the reliance on Postgres creating the implicit
  sequence of a 'known' name.

  Great idea, but it doesn't work!!! removing check and reverting id

*/
{
        drop_table(conn,"paper");
        drop_sequence(conn,"paper_id_seq");

	/*create_sequence(conn,"paper_id_seq");*/
	create_table(conn,"paper", 
		     "id SERIAL PRIMARY KEY, "
		     "batch_number INTEGER NOT NULL "
		     "REFERENCES batch(number),"
		     "index INTEGER NOT NULL,"
		     "supervisor_tick BOOL NOT NULL DEFAULT FALSE");
	create_index(conn,"paper","batch_number","paper_batch_idx");
	create_index(conn,"paper","batch_number,index","paper_batchnum_idx");
}

void create_entry_table(PGconn *conn)
/*
  Creating this table also creates a sequence called entry_id_seq
  to maintain the serial id column.

  NOTE: Use of SERIAL replaced with explicit sequence creation
  and references in DEFAULT and CHECK constraints. This frees
  us from the reliance on Postgres creating the implicit
  sequence of a 'known' name.

  Great idea, but it doesn't work!!! removing check and reverting id
*/
{
        drop_table(conn,"entry");
	drop_sequence(conn,"entry_id_seq");

	/*create_sequence(conn,"entry_id_seq");*/ 
	create_table(conn,"entry",
		     "id SERIAL PRIMARY KEY, "
		     "paper_id INTEGER NOT NULL "
		     "REFERENCES paper(id),"
		     "index INTEGER NOT NULL,"
		     "operator_id TEXT NOT NULL,"
		     "num_preferences INTEGER NOT NULL,"
		     "paper_version INTEGER NOT NULL");
	create_index(conn,"entry","paper_id",
		     "entry_paperid_idx");
}

void create_polling_place_table(PGconn *conn)
     /*
       Create polling place table.

       NOTE: Ignore failure of DROP TABLE commands.
     */
{
        drop_table(conn,"polling_place");
	create_table(conn,"polling_place",
		     "code INTEGER NOT NULL PRIMARY KEY,"
		     "name TEXT NOT NULL UNIQUE,"
		     "loaded BOOL NOT NULL DEFAULT false,"
		     "electorate_code INTEGER REFERENCES electorate(code)");
}

void create_electorate_preference_tables(PGconn *conn)
     /*
       Create electorate tables for each electorate.
       The name of each table is the name of the electorate within
       the 'electorate' table.
       'party' used instead of 'group' due to Postgres naming
       conventions.

       NOTE: The tables 'entry' and 'candidate' must already exist.
       ALSO NOTE: Ignore failure of DROP TABLE commands.
     */
{
       PGresult *result;
       unsigned int i,num_rows;
       char *name,*index_name;

       const char *def = "entry_id INTEGER NOT NULL,"
	                 "party_index INTEGER NOT NULL,"
	                 "candidate_index INTEGER NOT NULL,"
	                 "preference_number INTEGER NOT NULL";
        
       result = SQL_query(conn,"SELECT name from electorate"); 

       num_rows = PQntuples(result);

       for (i=0;i<num_rows;i++) {
	       name = PQgetvalue(result,i,0);
	       /* drop and create table */
	       drop_table(conn,name);
	       create_table(conn,name,def);
	       create_index(conn,name,"entry_id",
			    index_name=sprintf_malloc("%s_entryid_idx",name));
/*	       free(index_name);*/
       }
       PQclear(result);
}

PGconn *clean_database(const char *name)
{
	PGconn *conn;
	PGresult *result;
	char db_command[sizeof("DROP DATABASE %s;") + strlen(name)];

	sprintf(db_command, "DROP DATABASE %s;", name);
	/* Open database connection, to create test db. */
	conn = PQsetdb(NULL, NULL, NULL, NULL, "template1");

	/* Sometimes, old users seem to hold database for a while... */
	while (PQresultStatus(result = PQexec(conn, db_command))
	       != PGRES_COMMAND_OK
	       && strstr(PQresultErrorMessage(result), "being accessed")) {
		sleep(1);
		PQclear(result);
	}
	if (PQresultStatus(result) != PGRES_COMMAND_OK
	    && strstr(PQresultErrorMessage(result), "does not exist") == 0)
		bailout("Dropping database %s failed: %s (%s)\n",
			name, PQresStatus(PQresultStatus(result)),
			PQresultErrorMessage(result));
	PQclear(result);

	SQL_command(conn, "CREATE DATABASE %s;", name);
	PQfinish(conn);
	return connect_db(name);
}

void create_last_result_table(PGconn *conn)
     /*
       Create last result table.

       NOTE: Ignore failure of DROP TABLE commands.
     */
{
        drop_table(conn,"last_result");
	create_table(conn,"last_result",
		     "electorate_code INTEGER NOT NULL,"
		     "polling_place_code INTEGER NOT NULL,"
		     "abbreviation TEXT,"
		     "count INTEGER NOT NULL,"
		     "PRIMARY KEY (electorate_code,polling_place_code,"
		     "abbreviation)");
}

/*
  Election Night System tables
*/

/*
  Summary tables for EVACS and phoned-in 1st pref. votes on Election Night
  and EVAVS votes from then on.
*/

void create_preference_summary_table(PGconn *conn)
     /*
       Create preference_summary table.

       NOTE: Ignore failure of DROP TABLE commands.
     */
{
        drop_table(conn,"preference_summary");
	create_table(conn,"preference_summary",
		     "electorate_code INTEGER NOT NULL "
		     "REFERENCES electorate(code),"
		     "polling_place_code INTEGER NOT NULL "
		     "REFERENCES polling_place(code),"
		     "party_index INTEGER NOT NULL,"
		     "candidate_index INTEGER NOT NULL,"
		     "phoned_primary INTEGER NOT NULL,"
		     "evacs_primary INTEGER NOT NULL,"
		     "final_count INTEGER NOT NULL,"
		     "FOREIGN KEY(electorate_code,party_index,candidate_index)"
		     "REFERENCES candidate(electorate_code,party_index,index),"
		     "PRIMARY KEY (electorate_code,polling_place_code,"
		     "party_index,candidate_index)");
	
}

void create_vote_summary_table(PGconn *conn)
     /*
       Create vote_summary table.

       NOTE: Ignore failure of DROP TABLE commands.
     */
{
        drop_table(conn,"vote_summary");
	create_table(conn,"vote_summary",
		     "electorate_code INTEGER NOT NULL "
		     "REFERENCES electorate(code),"
		     "polling_place_code INTEGER NOT NULL "
		     "REFERENCES polling_place(code),"
		     "entered_by TEXT NOT NULL,"
		     "entered_at TIMESTAMP NOT NULL,"
		     "informal_count INTEGER NOT NULL");
}

void create_scrutiny_tables(PGconn *conn)
     /*
       Create scrutiny tables. (Used by preference_results display in ENS)

       NOTE: Ignore failure of DROP TABLE commands.
     */
{
        drop_table(conn,"scrutiny");
	create_table(conn,"scrutiny",
		     "electorate_code INTEGER NOT NULL PRIMARY KEY "
		     "REFERENCES electorate(code),"
		     "exhausted_votes INTEGER NOT NULL,"
		     "loss_by_fraction INTEGER NOT NULL");

        drop_table(conn,"scrutiny_pref");
	create_table(conn,"scrutiny_pref",
	           "electorate_code INTEGER NOT NULL,"
	           "party_index INTEGER NOT NULL,"
		   "candidate_index INTEGER NOT NULL,"
		   "after_preferences INTEGER NOT NULL,"
		   "FOREIGN KEY(electorate_code,party_index,candidate_index) "
		   "REFERENCES candidate(electorate_code,party_index,index),"
		   "PRIMARY KEY(electorate_code,party_index,candidate_index)");
}

