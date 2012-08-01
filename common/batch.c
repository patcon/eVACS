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

/* This file contains implementation of batch functionality*/
#include <string.h>
#include <stdlib.h>
#include <common/database.h>
#include <common/safe.h>
#include "batch.h"


/* batch number currently being operated on  */
static unsigned int current_batch;


unsigned int get_current_batch_number(void)
{
	return current_batch;
}

void set_current_batch_number(unsigned int batch_number)
{
	current_batch = batch_number;
}


/* DDS3.6: Get Entered Papers 
   from v2B */
struct batch *get_entered_batch(PGconn *conn, unsigned int batch_number)
     /*
       Return all preferences for the given batch number
     */

{
	PGresult *result;
	struct batch *head=NULL, *tmp=NULL;
	struct predefined_batch *batch;
	char *table_name;
	unsigned int num_papers,electorate_code;

	/* get electorate code */
	batch = resolve_batch_source(conn, batch_number);
	electorate_code = batch->electorate_code;

	/* get electorate name in order to access that Electorates
	   preference table */
	table_name = resolve_electorate_name(conn,electorate_code);

	num_papers = (unsigned int )SQL_singleton_int(conn,
				      "SELECT COUNT(*) "
				      "FROM paper "
				      "WHERE batch_number = %u;",
				      batch_number);

	/* build the batch structure */
	tmp = malloc(sizeof(*tmp) + (sizeof(tmp->papers[0]) * num_papers));

	tmp->b.batch_number = batch_number;

	result = SQL_query(conn,
			   "SELECT size, committed "
			   "FROM batch "
			   "WHERE number = %u;", batch_number);

	tmp->b.batch_size = atoi(PQgetvalue(result, 0, 0));

	if (*PQgetvalue(result,0,1) == 't')
		tmp->b.committed = true;
	else
		tmp->b.committed = false;

	tmp->b.num_papers = num_papers;
	
        /* papers and sub-structures inserted into tmp */
	if (num_papers > 0)
	        get_papers_for_batch(conn, tmp, table_name);
	
	tmp->next = NULL;
	head = tmp;

	
	PQclear(result);
	free(table_name);
        return head;
}
/* DDSv2C: Get Paper  */
struct paper *get_paper(unsigned int batch_number, unsigned int paper_index)

{
	PGconn *conn = connect_db_host(DATABASE_NAME, SERVER_ADDRESS);
	struct paper *ret;
	unsigned int paper_id, electorate_code;
	char *electorate_table_name;
	struct predefined_batch *batch;

	/* get electorate code */
	batch = resolve_batch_source(conn, batch_number);
	electorate_code = batch->electorate_code;

	/* get electorate name in order to access that Electorates
	   preference table */
	electorate_table_name = resolve_electorate_name(conn,electorate_code);

	
	/* find the (internal database) paper_id */
	paper_id = SQL_singleton_int(conn,
				     "SELECT id "
				     "FROM paper "
				     "WHERE batch_number = %u "
				     "AND index = %u;",
				   batch_number, paper_index);

	ret = malloc(sizeof(*ret));

	if (paper_id ==(unsigned int) -1) {
	  /* new paper  */
	    ret->p.index = paper_index;
	    ret->p.supervisor_tick = false;
	    ret->entries = NULL;
	} else {
	        ret = malloc(sizeof(*ret));
	        ret->entries = get_entries_for_paper(conn, paper_id,
						     electorate_table_name);
	}

	free(electorate_table_name);
	PQfinish(conn);
	
	return ret;
}

void get_papers_for_batch(PGconn *conn, struct batch *batch,
			  char *electorate_table_name)
/* load the batch structure pointed to by batch with paper data*/
{
        unsigned int i;
	PGresult *result;
	unsigned int num_rows,paper_id;

	result =  SQL_query(conn,
			    "SELECT id, "
			    "index, "
			    "supervisor_tick "
			    "FROM  paper "
			    "WHERE batch_number= %u "
			    "ORDER BY index;", batch->b.batch_number);
	
	num_rows = PQntuples(result);

	/*  build a return structure
	 */
	for (i=0;i<num_rows;i++) {
		paper_id = atoi(PQgetvalue(result, i, 0));
		batch->papers[i].p.index = atoi(PQgetvalue(result,i,1));
		batch->papers[i].p.supervisor_tick =  
			(*PQgetvalue(result, i, 2) == 't')?true:false;

		/* entries and sub-structures inserted into batch->papers */
		batch->papers[i].entries = get_entries_for_paper(conn,
					    paper_id,electorate_table_name);
	
	}
	PQclear(result);
	
}

static struct entry *new_entry(unsigned int num_prefs, const char *operid,
			       unsigned int pvn)
{
	struct entry *ret;

	ret = malloc(sizeof(*ret) + sizeof(ret->preferences[0]) * num_prefs);
	ret->e.num_preferences = num_prefs;
	strcpy(ret->e.operator_id, operid);
	ret->e.paper_version_num = pvn;
	return ret;
}

struct entry *get_entries_for_paper(PGconn *conn, unsigned int paper_id,
			   char *electorate_table_name)
/* load the paper structure pointed to by paper with entry data*/

{
	PGresult *result;
	struct entry *head = NULL, *tmp = NULL;
	unsigned int i, num_rows, entry_id;

	result =  SQL_query(conn,
			    "SELECT id,index,"
			    "operator_id,"
			    "num_preferences,"
			    "paper_version "
			    "FROM  entry "
			    "WHERE paper_id= %u "
			    "ORDER BY index DESC;",
			    paper_id);
	
	num_rows = PQntuples(result);
	
	/*  build a return structure
	 */
	for (i=0;i<num_rows;i++) {
		entry_id = atoi(PQgetvalue(result,i,0));
		tmp = new_entry(atoi(PQgetvalue(result,i,3)), /* num prefs */
				PQgetvalue(result,i,2), /* operator */
				atoi(PQgetvalue(result,i,4))); /* pvn */
		/* link the entries such that the last entry is at the head of the list */
		tmp->next = head;
		head = tmp;
	       
		get_prefs_for_entry(conn, entry_id, &head->preferences[0], 
				    electorate_table_name);
	}
	PQclear(result);

	return(head);
}

void get_prefs_for_entry(PGconn *conn, unsigned int entry_id,
			 struct preference preferences[],
			 char *electorate_table)
/* load the entry structure pointed to by entry with preference data*/

{
        unsigned int i;
	PGresult *result;
	unsigned int num_rows;

	result =  SQL_query(conn,
			    "SELECT preference_number, "
			    "party_index,"
			    "candidate_index "
			    "FROM %s "
			    "WHERE entry_id= %u "
			    "ORDER BY preference_number;",
			    electorate_table,entry_id);
	
	num_rows = PQntuples(result);
	

	/*  build a return structure
	 */
	for (i=0;i<num_rows;i++) {
		preferences[i].prefnum = atoi(PQgetvalue(result,i,0));
	        preferences[i].group_index = atoi(PQgetvalue(result,i,1));
		preferences[i].db_candidate_index = atoi(PQgetvalue(result,i,2));
	}
	PQclear(result);

}

void save_entry(struct entry *newentry, unsigned int batch_number, 
		unsigned int paper_index)
{
	PGconn *conn = connect_db_host(DATABASE_NAME, SERVER_ADDRESS);

	unsigned int pvn = newentry->e.paper_version_num;
	
	append_entry(conn, newentry, batch_number, paper_index, pvn); 
	
	PQfinish(conn);
}

void save_paper(struct paper *newpaper, unsigned int batch_number)
{
        PGconn *conn = connect_db_host(DATABASE_NAME, SERVER_ADDRESS);
	
	SQL_command(conn, 
		    "INSERT into paper(batch_number,index,supervisor_tick) "
		    "values(%u, %u, 'f');",
		    get_current_batch_number(), newpaper->p.index);

	PQfinish(conn);
}

void append_entry(PGconn *conn,struct entry *newentry,
		  unsigned int batch_number,unsigned int paper_index,
		  unsigned int paper_version)
     /*
       Insert the entry into the database.
     */
{	
	char *table_name,*oi;
        int electorate_code;
	int paper_id,entry_index=1,i;

	/* get electorate code */
	electorate_code = SQL_singleton_int(conn,
					    "SELECT electorate_code "
					    "FROM batch "
					    "WHERE number = %u;",
					    batch_number);
	if (electorate_code < 0)
		bailout("append_entry could not find batch number %u.\n",
			batch_number);

	/* get electorate name in order to access that Electorates
	   preference table */
	table_name = resolve_electorate_name(conn,electorate_code);

	/* Start the transaction */
	begin(conn);

	/* Check paper exists */
	paper_id = SQL_singleton_int(conn,
				     "SELECT id FROM paper WHERE index = %u "
				     "AND batch_number = %u;",
				     paper_index,batch_number);

	/* Insert new paper if necessary */
	if (paper_id < 0) {
	        SQL_command(conn,
			    "INSERT INTO "
			    "paper(batch_number,index) "
			    "VALUES(%u,%u);",batch_number,paper_index);
		entry_index = 1;
		paper_id = get_seq_currval(conn,"paper_id_seq");

	}
	else {
	        /* Get last entry index for this paper */
	        entry_index = SQL_singleton_int(conn,
						"SELECT MAX(index) FROM entry "
						"WHERE paper_id = %d;",
						paper_id);
		if (entry_index < 0)
		        entry_index = 1;   /* It must be the first one */
		else
		        entry_index++;

	}
	
	/* Insert new entry */
	SQL_command(conn,
		    "INSERT INTO entry(index,operator_id,"
		    "paper_id,num_preferences,paper_version) "
		    "VALUES(%u,'%s',%u,%u,%u);",
		    entry_index,oi=eq_malloc(newentry->e.operator_id),
		    paper_id,newentry->e.num_preferences,paper_version);
	free(oi);
	/* Insert the preferences */
	for (i=0;i<newentry->e.num_preferences;i++) {
		SQL_command(conn,
			    "INSERT INTO %s "
			    "VALUES(CURRVAL('entry_id_seq'),%u,%u,%u);",
			    table_name,
			    newentry->preferences[i].group_index,
			    newentry->preferences[i].db_candidate_index,
			    newentry->preferences[i].prefnum);
	}
	/* Complete the transaction */
	commit(conn);
}


/* DDS3.4: Resolve Batch Electorate 
   from v2B */
/* DDS3.6: Get Electorate Code from Predefined Batch Details
 from v2C*/
struct predefined_batch *resolve_batch_source(PGconn *conn, unsigned int batch_number) {
       /*
	 return the electorate code of the batch number
       */
       struct predefined_batch *batch;
       PGresult *result;
      
       result = SQL_query(conn, 
			  "SELECT electorate_code,polling_place_code "
			  "FROM batch WHERE number = %u;",
			  batch_number);

       if (PQresultStatus(result) != PGRES_TUPLES_OK) {
	       PQclear(result);
	       return NULL;
       }

       if (PQntuples(result) == 0) {
	       PQclear(result);
	       return NULL;
       }

       batch = malloc(sizeof(struct predefined_batch));
       batch->electorate_code = atoi(PQgetvalue(result,0,0));
       batch->polling_place_code = atoi(PQgetvalue(result,0,1));
       batch->batch_number = batch_number;

       /* make sure this struct knows its a singleton */
       batch->next = NULL;

       PQclear(result);

       return batch;
}



char *resolve_electorate_name(PGconn *conn, unsigned int electorate_code)

{
        return(SQL_singleton(conn,
			     "SELECT name FROM electorate WHERE code = %u;",
			     electorate_code));
}


int resolve_electorate_code(PGconn *conn,const char *electorate_name)
{
	return(SQL_singleton_int(conn,
				 "SELECT code FROM electorate "
				 "WHERE UPPER(name) = UPPER('%s');",
				 electorate_name));
}

char *resolve_polling_place_name(PGconn *conn, unsigned int polling_place_code)

{
        return(SQL_singleton(conn,
			     "SELECT name FROM polling_place WHERE code = %u;",
			     polling_place_code));
}

int resolve_polling_place_code(PGconn *conn,const char *polling_place_name)
{
	return(SQL_singleton_int(conn,
				 "SELECT code FROM polling_place "
				 "WHERE UPPER(name) = UPPER('%s');",
				 polling_place_name));
}

void free_batch(struct batch *batch)
{
	unsigned int i;

	/* For every paper, free linked list of entries */
	for (i = 0; i < batch->b.num_papers; i++) {
		while (batch->papers[i].entries) {
			struct entry *next;

			next = batch->papers[i].entries->next;
			free(batch->papers[i].entries);
			batch->papers[i].entries = next;
		}
	}

	free(batch);
}
