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
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <common/safe.h>
#include <common/database.h>
#include <common/batch.h>
#include <common/find_errors.h>
#include <common/current_paper_index.h>
#include <common/rotation.h>
#include <common/get_electorate_ballot_contents.h>
#include <voting_client/vote_in_progress.h>
#include <voting_client/get_rotation.h>
#include <data_entry/get_paper_version.h>
#include <data_entry/confirm_paper.h>
#include <data_entry/enter_paper.h>
#include <data_entry/batch_entry.h>
#include <voting_server/fetch_rotation.h>

#include "batch_edit.h"


static void print_report_heading(FILE *fp) {
   char *operator_id=get_operator_id();
   time_t systime = time(NULL);
   char  *asctime = malloc(strlen(ctime(&systime)+3));
   
   strcpy(asctime,ctime(&systime));
   fprintf(fp,"------------------------------------------------------------------------------\n");
   fprintf(fp,"USER: %s\t\t\t\t\t%s\n",operator_id, asctime );
   fprintf(fp,"------------------------------------------------------------------------------\n");
   
   free(asctime);
   free(operator_id);
}

void wait_for_reset(void)
{
	bailout("Failed to receive image from server. Is server down?\n");
}

static void printfile(const char *filename)
/*
  Send the file to the printer via a system command.
*/
{
	char *command;
	/* echo to screen */
	/*	command = sprintf_malloc("cat %s",filename);
	system(command);
	free(command);*/
	/* send to printer via lpr command */
	command = sprintf_malloc("lpr %s\n",filename);
  	system(command); 
	free(command);
}
/* count the number of entries in paper */
static unsigned int num_entries(struct paper_with_error *p)
{
	unsigned int i=0;
	struct entry_with_error *e;

	for (e=p->entries;e;e=e->next) i++;

	return i;
}

/* DDS3.36: Print Paper */
static void print_paper(PGconn *conn,struct paper_with_error *p,
			unsigned int e_code,FILE *fp)
/*
  Print the entries for a paper.
*/
{
	struct entry_with_error *e;
	char *candidate_name;
	unsigned int i;
	unsigned int entry_index=num_entries(p);;

	for (e=p->entries;e;e=e->next) {
		fprintf(fp,"%d\t%d\t%s\t%u\t%u\n",
		       p->p.index,entry_index--,e->e.operator_id,
		       e->error_code,e->e.paper_version_num);
		for (i=0;i < e->e.num_preferences;i++) {
			/* DDS3.36: Get Candidate Name */
			candidate_name = SQL_singleton(conn,
				               "SELECT name "
				               "FROM candidate "
				               "WHERE index = %u "
		        		       "AND party_index = %u "
		       			       "AND electorate_code = %u;",
					 e->preferences[i].db_candidate_index,
					 e->preferences[i].group_index,
					 e_code);
			assert(candidate_name);
			fprintf(fp,"\t\t\t\t\t%u\t%s\n",e->preferences[i].prefnum,
			       candidate_name);
			free(candidate_name);
		}
	}
	if (p->p.supervisor_tick) fprintf(fp,"PAPER APPROVED\n\n");
}

/* DDS3.8: Prompt for Batch Number */
static void batch_num_prompt(void)
/*
  Print a text prompt.
*/
{
	printf("Please enter batch number :");
}

/* DDS3.8: Get Batch Number from User */
static unsigned int get_batch_num(void)
/*
  Obtain a batch number from the user.
*/
{
	char *line;
	unsigned int b_num;

	batch_num_prompt();
	line = fgets_malloc(stdin);
	b_num = (unsigned int)atoi(line);
	free(line);
	return(b_num);
}

/* DDS3.8: Set Uncommitted Batch Number */
static bool set_uncom_batch_num(PGconn *conn,unsigned int *batch_number,
				unsigned int electorate_code)
/*
  Input a Batch Number and Batch Number OK = TRUE if it is valid
  and uncommitted.
*/
{
	unsigned int b_num,e_code=0,num_rows;
	PGresult *result;
	bool committed=false;

	b_num = get_batch_num();

	result = SQL_query(conn,
			   "SELECT electorate_code,committed "
			   "FROM batch "
			   "WHERE number = %u;",b_num);

	if ((num_rows = PQntuples(result)) > 0) {
		/* DDS3.8: Get Electorate Code from Predefined Batch Details */
		e_code = (unsigned int)atoi(PQgetvalue(result,0,0));
		committed = (*PQgetvalue(result,0,1) == 't') ? true : false;
	}

	PQclear(result);

	if (num_rows < 1) {
		printf("Unknown batch number.\n");
		return(false);
	}
	
	/* DDS3.8: Get Electorate Code from Batch Electorate */
	if (electorate_code != e_code) {
		printf("That batch number is not in this electorate.\n");
		return(false);
	}

	if (committed) {
		printf("This batch has already been committed.\n");
		return(false);
	}

	*batch_number = b_num;
	return(true);
}

/* DDS3.32: Set Batch Number */
static bool set_batch_num(PGconn *conn,unsigned int *batch_number,
			  unsigned int electorate_code)
/*
  Input a Batch Number. Return true and the batch number if the number is a
  valid batch number. Otherwise return false.
*/
{
	unsigned int b_num,e_code=0,num_rows;
	PGresult *result;

	b_num = get_batch_num();

	result = SQL_query(conn,
			   "SELECT electorate_code "
			   "FROM batch "
			   "WHERE number = %u;",b_num);

	if ((num_rows = PQntuples(result)) > 0)
		/* DDS3.8: Get Electorate Code from Predefined Batch Details */
		e_code = (unsigned int)atoi(PQgetvalue(result,0,0));

	PQclear(result);

	if (num_rows < 1) {
		printf("Unknown batch number.\n");
		return(false);
	}
	
	/* DDS3.8: Get Electorate Code from Batch Electorate */
	if (electorate_code != e_code) {
		printf("That batch number is not in this electorate.\n");
		return(false);
	}

	*batch_number = b_num;
	return(true);
}

/* DDS3.40: Print Errors in Batch */
void print_errors_in_batch(PGconn *conn,unsigned int batch_number)
/*
  Print all Papers in a Batch which are either informal or have Keystroke
  Errors.
*/
{
	bool committed_flag,print;
	const char *committed_text;
	char *electorate_name;
	char *polling_place_name;
	struct batch *batch;
	struct batch_with_error *bwe;
	unsigned int i,e_code;
	FILE *fp;
	char tmpfile[]="/tmp/errors_in_batch_XXXXXX";

	polling_place_name = SQL_singleton(conn,
				  "SELECT p.name "
				  "FROM batch b, polling_place p "
				  "WHERE b.number = %u "
				  "AND p.code = b.polling_place_code;",
					   batch_number);
	committed_flag = SQL_singleton_bool(conn,
					    "SELECT committed "
					    "FROM batch "
					    "WHERE number = %u;",batch_number);

	e_code = SQL_singleton_int(conn,
				   "SELECT electorate_code "
				   "FROM batch "
				   "WHERE number = %u;",batch_number);
	electorate_name = SQL_singleton(conn,
					"SELECT name "
					"FROM electorate "
					"WHERE code = %u;",e_code);

	committed_text = (committed_flag)?"COMMITTED":"";

	mkstemp(tmpfile);
	fp = fopen(tmpfile,"w");
	
	print_report_heading(fp);
	fprintf(fp,"Papers with Errors for %s from %s Batch: %u %s\n"
		"Paper\tEntry \n"
		"Index\tIndex\tUser\tError\tPVN\tPreferences\n"
		"----------------------------------------"
		"----------------------------------------\n",
		electorate_name,polling_place_name,batch_number,committed_text);
	free(electorate_name);
	free(polling_place_name);
		
	batch = get_entered_batch(conn,batch_number);
	bwe = find_errors_in_batch(batch);
	for (i=0;i<bwe->b.num_papers;i++) {
		print = true;
		switch (bwe->papers[i].error_code) {
		case ENTRY_ERR_CORRECT:
		case ENTRY_ERR_CORRECTED:
			print = false;
			break;
		case ENTRY_ERR_IGNORED:
		case ENTRY_ERR_INFORMAL:
		case ENTRY_ERR_MISSING_NUM:
		case ENTRY_ERR_DUPLICATED_NUM:
			if (bwe->papers[i].p.supervisor_tick)
				print = false;
			break;
		case ENTRY_ERR_UNENTERED:
		case ENTRY_ERR_KEYSTROKE:
			break;
		}
		
		if (print)
			print_paper(conn,&bwe->papers[i],e_code,fp);
	}
	free_batch_with_error(bwe);
	fclose(fp);
	printfile(tmpfile);
	printf("Errors in Batch %u printed.\n",batch->b.batch_number);
	free_batch(batch);
}

/* DDS3.38: Set Polling Place */
static bool set_polling_place(PGconn *conn,unsigned int *polling_place_code)
/*
  Allow operator to choose a polling place for reporting.
  Return true and the polling place code if polling place is valid.
  Otherwise return false.
*/
{
	int pp_code;
	char *pp_name;

	printf("Please enter the polling place name>");
	pp_name = fgets_malloc(stdin);

	/* DDS3.38: Resolve Polling Place Code */
	/* DDS3.38: Store Current Polling Place */
	pp_code = SQL_singleton_int(conn,
				    "SELECT code "
				    "FROM polling_place "
				    "WHERE UPPER(name) = UPPER('%s');"
				    ,pp_name);
	free(pp_name);
	if (pp_code == -1 ) {
		printf("Polling place %s unknown\n\n",pp_name);
		return(false);
	}
	
	*polling_place_code = pp_code;
	return(true);
}

/* DDS3.38: Print Batches for Polling Place */
void print_pp_batches(PGconn *conn, 
		      unsigned int electorate_code,
		      unsigned int polling_place_code)
/*
  Output to printer a report detailing batches originating from a polling place
*/
{
	unsigned int num_rows,i,j;
	int num_papers;
	char *pp_name, *elec_name;
	const char *committed;
	PGresult *result;
	struct batch *batch;
	struct batch_with_error *bwe;
	char tmpfile[]="/tmp/print_pp_batches_XXXXXX";
	FILE *fp;
	bool next_batch;

	pp_name = SQL_singleton(conn,
				"SELECT name "
				"FROM polling_place "
				"WHERE code = %u;",polling_place_code);
	
	elec_name = resolve_electorate_name(conn,electorate_code);
	mkstemp(tmpfile);
	fp = fopen(tmpfile,"w");

	print_report_heading(fp);
	fprintf(fp,"Batch report for Polling Place: %s (electorate %s)\n\n",
		pp_name, elec_name);

	result = SQL_query(conn,
			   "SELECT number,electorate_code,size,committed "
			   "FROM batch "
			   "WHERE polling_place_code = %u "
			   " AND electorate_code = %u "
			   "ORDER BY number;",polling_place_code,electorate_code);

	num_rows = PQntuples(result);

	for (i=0;i<num_rows;i++) {
		batch = get_entered_batch(conn,atoi(PQgetvalue(result,i,0)));
		bwe = find_errors_in_batch(batch);
		committed = (*PQgetvalue(result,i,3)=='t')?"COMMITTED":"";
		fprintf(fp,"Batch:%u size:%u\t%s\t\n",batch->b.batch_number,
			batch->b.batch_size, committed);

		/* set up loop */
		j = 1;
		do {
			num_papers = SQL_singleton_int(conn,
						       "SELECT COUNT(e.id) "
						       "FROM entry e, paper p "
						       "WHERE e.index = %u "
						       "AND e.paper_id = p.id "
						       "AND p.batch_number = %u;",
						       j,batch->b.batch_number);
			if (num_papers > 0)
				fprintf(fp,"\tNumber of papers with "
				       "entry index %u = %d\n",
					j,num_papers);
			j++;
		
		} while (num_papers >  0);
		
		next_batch = false;

		for (j=0;j<bwe->b.num_papers;j++) {
		        if  (bwe->papers[j].error_code == ENTRY_ERR_UNENTERED) {
			        fprintf(fp,"INCOMPLETE ENTRY\n");
				next_batch=true;
				break;
			}
		}		
		if (next_batch == true) continue;

		for (j=0;j<bwe->b.num_papers;j++) {
		        if  (bwe->papers[j].error_code == ENTRY_ERR_KEYSTROKE) {
			        fprintf(fp,"ENTRY ERROR\n");
				next_batch=true;
				break;
			}
		}		
		/* move on to next batch */
		if (next_batch == true) continue;
     
		/* look for papers needing approval */
		for (j=0;j < bwe->b.num_papers;j++) {
		        /* Increment appropriate counter */
		        if  (bwe->papers[j].error_code == ENTRY_ERR_IGNORED ||
			     bwe->papers[j].error_code == ENTRY_ERR_INFORMAL ||
			     bwe->papers[j].error_code == ENTRY_ERR_MISSING_NUM ||
			     bwe->papers[j].error_code == ENTRY_ERR_DUPLICATED_NUM) {
			        
			        next_batch = true;
				if (bwe->papers[j].p.supervisor_tick == false) 
					fprintf(fp,"APPROVAL NEEDED\n");
				else 
					fprintf(fp,"APPROVED\n");
				
				break;
			}
		}
		/* move on to next batch */
		if (next_batch == true) continue;	
		
		for (j=0;j<bwe->b.num_papers;j++) {
		        if  (bwe->papers[j].error_code == ENTRY_ERR_CORRECTED) {
			        fprintf(fp,"CORRECTED \n");
				next_batch=true;
				break;
			}
		}	
	        /* move on to next batch */
		if (next_batch == true) continue;	

		if (bwe->b.num_papers == 0) 
		        fprintf(fp,"NOT YET ENTERED\n"); 
		else
			fprintf(fp,"CORRECT\n");

		free_batch(batch);
		free_batch_with_error(bwe);
	}
	PQclear(result);
	fclose(fp);
	printfile(tmpfile);
	printf("Batches for %s printed.",pp_name);
	free(pp_name);
}

/* DDS3.34: Print Batch */
void print_batch(PGconn *conn,unsigned int batch_number)
/*
  Print the entire detail for every entry of every paper in the
  batch.
*/
{
	PGresult *result;
	const char *committed;
	struct batch *batch;
	struct batch_with_error *bwe;
	unsigned int i;
	char tmpfile[] = "/tmp/print_batch_XXXXXX";
	FILE *fp;

	/* DDS3.34: Resolve Polling Place Name */
	/* DDS3.34: Get Polling Place Code */
	/* DDS3.34: Get Polling Place name */
	result = SQL_query(conn,
			   "SELECT e.name,p.name,b.number,"
			          "b.committed,e.code "
			   "FROM batch b, electorate e,polling_place p "
			   "WHERE b.number = %u " 
			   "AND e.code = b.electorate_code "
			   "AND p.code = b.polling_place_code;",batch_number);

	committed = (*PQgetvalue(result,0,3) == 't') ? "COMMITTED" : "";

	mkstemp(tmpfile);
	fp = fopen(tmpfile,"w");
	print_report_heading(fp);
	
	fprintf(fp,"Complete Batch Dump for %s from %s Batch: %s %s\n"
		"Paper\tEntry\n"
		"Index\tIndex\tUser\tError\tPVN\tPreferences\n"
		"---------------------------------------"
		"---------------------------------------\n",
	       PQgetvalue(result,0,0),PQgetvalue(result,0,1),
	       PQgetvalue(result,0,2),committed);

	/* Batch is not committed */

	batch = get_entered_batch(conn,atoi(PQgetvalue(result,0,2)));
	bwe = find_errors_in_batch(batch);

	for (i=0;i<bwe->b.num_papers;i++)
		print_paper(conn,&bwe->papers[i],
			    atoi(PQgetvalue(result,0,4)),fp);

	free_batch_with_error(bwe);
	PQclear(result);
	fclose(fp);
	printfile(tmpfile);
	printf("Batch %u printed.\n",batch->b.batch_number);
	free_batch(batch);
}


/* DDS3.30: Print Batch Summary */
void print_batch_summary(PGconn *conn)
/*
  Output to printer an overall Tally of batches (by Polling Place) in
  each possible Batch state.
*/

{
	PGresult *result;
	unsigned int num_rows,i,j;
	unsigned int batches=0,committed=0,unentered=0,once_entered=0,
		keystroke=0,unapproved=0,uncommitted=0;
	char *electorate_name,*polling_place_name;
	struct batch *batch;
	struct batch_with_error *bwe;
	char tmpfile[]="/tmp/batch_summary_XXXXXX";
	bool next_batch;
	FILE *fp;
	

	/* DDS3.30: Get Batch Information */
	result = SQL_query(conn,
			   "SELECT e.name,p.name,b.number,"
			          "b.committed "
			   "FROM batch b, electorate e, polling_place p "
			   "WHERE b.polling_place_code = p.code "
			   "AND b.electorate_code = p.electorate_code "
			   "ORDER BY e.name,p.name,b.number;");
	
	num_rows = PQntuples(result);

	mkstemp(tmpfile);
	fp = fopen(tmpfile,"w");

	print_report_heading(fp);

	electorate_name = polling_place_name = (char *)"";
	/* for each batch */
	for (i=0;i<num_rows;i++) {
		/* Electorate name or Polling Place name change? */
		if (strcmp(electorate_name,PQgetvalue(result,i,0)) ||
		    strcmp(polling_place_name,PQgetvalue(result,i,1))) {
			/* print any accumulated counters */
			if ( batches > 0 )
				fprintf(fp,"\t\t%u\t%u\t%u\t%u\t%u\t%u\n",
				       committed,unentered,once_entered,
				       keystroke,unapproved,uncommitted);
			batches = committed = unentered = once_entered =
				keystroke = unapproved = uncommitted = 0;
			/* Electorate name change */
			if (strcmp(electorate_name,PQgetvalue(result,i,0))) {
				electorate_name = PQgetvalue(result,i,0);
				fprintf(fp,"\nBatch summary for %s:\n"
					"--------------------------------\n"
					"Polling Place\t\t\tSummary\n"
					"--------------------------------"
					"--------------------------------\n",
				       electorate_name);
				polling_place_name = (char *)"";
				fprintf(fp,"\t\tcommtd\tunentrd\t"
					"once\tkstroke\t"
					"unapprd\tuncommtd\n");
				fprintf(fp,"\t\t------\t-------\t"
					"----\t-------\t"
					"-------\t--------\n");

			}

			/* Polling Place name change */
			if (strcmp(polling_place_name,PQgetvalue(result,i,1))){
				polling_place_name = PQgetvalue(result,i,1);
				/* Print new Polling place heading  */
				fprintf(fp,"%s\n", polling_place_name);
				
			}
		}

		batches++;
		
		if (*PQgetvalue(result,i,3) == 't') { /* committed? */
			committed++;
			continue;
		}

		batch = get_entered_batch(conn,atoi(PQgetvalue(result,i,2)));
		
		if (batch->b.num_papers == 0 ) {
			unentered++;
			continue;
		}
		/* DDS3.30: Find Errors in Batch */
		bwe = find_errors_in_batch(batch);		
		next_batch = false;
		
		/* Have a look at each paper for an UNENTERED error code*/
		for (j=0;j < bwe->b.num_papers;j++) {
		        /* Increment appropriate counter */
		        if  (bwe->papers[j].error_code == ENTRY_ERR_UNENTERED) {
			        next_batch = true;
				once_entered++;
				break;
			}
			
		}
		/* move on to next batch */
		if (next_batch == true) continue;
		
		/* Have a look at each paper for a KEYSTROKE  error code*/
		for (j=0;j < bwe->b.num_papers;j++) {
		        /* Increment appropriate counter */
		        if  (bwe->papers[j].error_code == ENTRY_ERR_KEYSTROKE) {
			        next_batch = true;
				keystroke++;
				break;
			}
			
		}
		/* move on to next batch */
		if (next_batch == true) continue;

		/* Have a look at each paper for an  IGNORED,INFORMAL,
		   MISSING_NUM,DUPLICATED_NUM error code
		   which is NOT approved. i.e. does not have a supervisor tick
		*/
		for (j=0;j < bwe->b.num_papers;j++) {
		        if  ((bwe->papers[j].error_code == ENTRY_ERR_IGNORED ||
			     bwe->papers[j].error_code == ENTRY_ERR_INFORMAL ||
			     bwe->papers[j].error_code == ENTRY_ERR_MISSING_NUM ||
			     bwe->papers[j].error_code == ENTRY_ERR_DUPLICATED_NUM) &&
			     bwe->papers[j].p.supervisor_tick == false) {
			        next_batch = true;
				unapproved++;
				break;
			}
		}
		/* move on to next batch */
		if (next_batch == true) continue;
	

	/* NO errors found in papers? Batch status must be UNCOMMITTED*/
	if (j >= bwe->b.num_papers)
	        uncommitted++;

	free_batch(batch);
	free_batch_with_error(bwe);

	} /* for each batch */

	/* Write final counter values */
	if ( batches > 0 )
	        fprintf(fp,"\t\t%u\t%u\t%u\t%u\t%u\t%u\n",
			committed,unentered,once_entered,
			keystroke,unapproved,uncommitted);
	PQclear(result);
	fclose(fp);
	printfile(tmpfile);
	printf("Batch Summary Printed\n");
}

/* DDS3.28: Store Committed Batch Number */
static void store_comm_batch_num(PGconn *conn,unsigned int batch_number)
/*
  Adds a batch to the committed batch number store.
*/
{
	SQL_command(conn,
		    "UPDATE batch "
		    "SET committed = true "
		    "WHERE number = %u;",batch_number);
}


/* DDS3.28: Commit Votes */
static void commit_votes(PGconn *conn,unsigned int batch_number,
			   struct normalised_preference *np)
/*
  Saves the Normalised Preferences to the Confirmed Vote details store
  and adds the batch number to the confirmed votes store.
*/
{
	PGresult *result;
	unsigned int electorate_code,polling_place_code;
	unsigned int i;
	
	result = SQL_query(conn,"SELECT electorate_code,polling_place_code "
			        "FROM batch WHERE number = %u;",batch_number);
	
	electorate_code = atoi(PQgetvalue(result,0,0));
	polling_place_code = atoi(PQgetvalue(result,0,1));
	PQclear(result);

	/* Begin the transaction */
	begin(conn);

	/* Prevent race condition in SELECT - INSERT or UPDATE code */
	SQL_command(conn,"LOCK TABLE vote_summary IN EXCLUSIVE MODE;");
	SQL_command(conn,"LOCK TABLE preference_summary IN EXCLUSIVE MODE;");

	/* Build vote for each set of prefs in normalised preferences list */
	for(np=np; np; np=np->next) {
		/* Store vote in confirmed vote table */
		SQL_command(conn,
			    "INSERT INTO confirmed_vote "
			    "(electorate_code, polling_place_code) "
			    "VALUES(%u,%u);",
			    electorate_code,polling_place_code);
		/* And in the confirmed vote table summary if required */
		if ( SQL_singleton_int(conn,"SELECT COUNT(*) "
				       "FROM vote_summary "
				       "WHERE electorate_code = %d "
				       "AND polling_place_code = %d;",
				       electorate_code,polling_place_code)
		     == 0)
			SQL_command(conn,"INSERT INTO vote_summary "
				    "(electorate_code,polling_place_code,"
				    "entered_by,entered_at,informal_count) "
				    "VALUES (%d,%d,'EVACS batch','NOW',0);",
				    electorate_code,polling_place_code);

		/* If no preferences - increment number of informal votes */
		if (np->n.num_preferences == 0)
			SQL_command(conn,"UPDATE vote_summary "
				    "SET informal_count = informal_count + 1,"
				        "entered_by = 'EVACS batch',"
				        "entered_at = 'NOW' "
				    "WHERE electorate_code = %d "
				    "AND polling_place_code = %d;",
				    electorate_code,polling_place_code);

		/* Store preferences for vote in confirmed preference table */
		for (i=0;i < np->n.num_preferences;i++) {
			SQL_command(conn, "INSERT into "
				    "confirmed_preference "
				    "(group_index,db_candidate_index,prefnum) "
				    "values (%u,%u,%u) ",
				    np->preferences[i].group_index,
				    np->preferences[i].db_candidate_index,
				    np->preferences[i].prefnum);

			/* Summarise first preferences for election night */
			if ( np->preferences[i].prefnum == 1) {
				if (SQL_singleton_int(conn,"SELECT COUNT(*) "
					   "FROM preference_summary "
					   "WHERE electorate_code = %d "
				           "AND polling_place_code = %d "
				   	   "AND party_index = %d "
					   "AND candidate_index = %d;",
					 electorate_code,
					 polling_place_code,
					 np->preferences[i].group_index,
					 np->preferences[i].db_candidate_index)
				    == 0)
					SQL_command(conn,"INSERT INTO "
					      "preference_summary"
					      "(electorate_code,"
					      "polling_place_code,party_index,"
					      "candidate_index,phoned_primary,"
					      "evacs_primary,final_count) "
					      "VALUES(%d,%d,%d,%d,0,1,0);",
					electorate_code,
					polling_place_code,
					np->preferences[i].group_index,
					np->preferences[i].db_candidate_index);
				else
					SQL_command(conn,"UPDATE "
						"preference_summary "
						"SET evacs_primary = "
						    "evacs_primary + 1 "
						"WHERE electorate_code = %d "
						"AND polling_place_code = %d "
						"AND party_index = %d "
						"AND candidate_index = %d;",
					electorate_code,
					polling_place_code,
					np->preferences[i].group_index,
					np->preferences[i].db_candidate_index);
				SQL_command(conn,
					    "UPDATE vote_summary "
					    "SET entered_by = 'EVACS batch',"
					        "entered_at = 'NOW' "
					    "WHERE electorate_code = %d "
					    "AND polling_place_code = %d;",
				    electorate_code,polling_place_code);       
			}
		}
	}
	/* Store committed batch number */
	store_comm_batch_num(conn,batch_number);
	/* End the transaction */
	commit(conn);
	printf("Batch %u committed for counting.\n", batch_number);
}

/* DDS3.26: Normalise Preferences */
static void normalise_prefs(struct preference p_out[],
			    unsigned int *num_prefs_out,
			    struct preference p_in[],
			    unsigned int num_prefs_in)
/*
  Returns a list of prefernces which have Preference Indexes in ascending
  order starting from one with no missing numbers.
*/
{
	unsigned int prefnum=1,i,j=0,count;
	
	while (1) {
		/* Count number of time prefnum appears */
		count = 0;
		for (i=0;i<num_prefs_in;i++)
			if (p_in[i].prefnum == prefnum) {
				j = i;
				count++;
			}
		/* If only once - then copy across */
		if ( count == 1 ) {
			p_out[prefnum-1].prefnum = p_in[j].prefnum;
			p_out[prefnum-1].group_index = p_in[j].group_index;
			p_out[prefnum-1].db_candidate_index = 
				p_in[j].db_candidate_index;
			prefnum++;
		}
		else /* zero or more than one - exit loop */
			break;
	}

	*num_prefs_out = prefnum - 1;
}

/* DDS3.26: Normalise Batch */
static struct normalised_preference *normalise_batch(
	struct matched_preference *mp)
/*
  Returns a list of preferences which have Preference Indexes in ascending
  order starting from one with no missing numbers.
*/
{
	struct matched_preference *p;
	struct normalised_preference *np,*head=NULL;

	for (p=mp;p;p=p->next) {
		np = malloc(sizeof(*np) + 
			    sizeof(np->preferences[0])*p->m.num_preferences);
		normalise_prefs(&np->preferences[0],&np->n.num_preferences,
				&p->preferences[0],p->m.num_preferences);
		np->next = head;
		head = np;
	}
	return(head);
}

/* DDS3.26: Make Matched Preferences */
static struct matched_preference *make_match_prefs(PGconn *conn,
						   unsigned int batch_number)
/*
  Returns a list of corrected votes.
*/
{
	struct matched_preference *mp=NULL,*head=NULL;
	PGresult *result;
	unsigned int num_rows,num_prefs,i,entry_id,paper_index,batch_size;
	char *electorate_name;

	/* Get last entry for each paper in batch up to batch-size entries
	 ALL REMAINING ENTRIES ARE APPROVED IGNORED PAPERS */
	result = SQL_query(conn,
			   "SELECT e.id,e.num_preferences,p.index,el.name "
			   "FROM entry e, paper p, batch b,electorate el "
			   "WHERE p.batch_number = %u "
			   "AND e.paper_id = p.id "
			   "AND e.index = ( SELECT MAX(index) "
			                   "FROM entry "
			                   "WHERE paper_id = e.paper_id ) "
			   "AND b.number = p.batch_number "
			   "AND el.code = b.electorate_code "
			   "AND p.index < b.size;",
			   batch_number);

	num_rows = PQntuples(result);

	for (i=0;i<num_rows;i++) {
	  /* Dont commit ignored papers */
	  
		entry_id = atoi(PQgetvalue(result,i,0));
		num_prefs = atoi(PQgetvalue(result,i,1));
		paper_index = atoi(PQgetvalue(result,i,2));
		electorate_name = PQgetvalue(result,i,3);

		/* Allocate space for next matched preference entry */
		mp = malloc(sizeof(*mp) + 
			    sizeof(mp->preferences[0])*num_prefs);

		get_prefs_for_entry(conn,entry_id,&mp->preferences[0],
				    electorate_name);

		mp->m.paper_index = paper_index;
		mp->m.num_preferences = num_prefs;

		mp->next = head;
		head = mp;
	}
	return(head);
}


/* DDS3.26: Check Paper */
static bool check_paper(struct paper_with_error *pwe)
/*
  Returns true if the paper has: been entered twice, has no keystroke errors,
  and all ignored, informal, missing preference number and duplicate preference
  number papers have a supervisor tick. Otherwise false is returned.
*/

{
	switch (pwe->error_code) {
	case ENTRY_ERR_UNENTERED:
		printf("This batch has not been entered twice.\n");
		return false;

	case ENTRY_ERR_KEYSTROKE:
		printf("This batch has uncorrected keystroke errors.\n");
		return false;

	case ENTRY_ERR_IGNORED:
	case ENTRY_ERR_INFORMAL:
	case ENTRY_ERR_MISSING_NUM:
	case ENTRY_ERR_DUPLICATED_NUM:
	        if (pwe->p.supervisor_tick == false) {
			printf("This batch has papers requiring "
			       "Supervisor approval.\n");
		        return false;
		}
	case ENTRY_ERR_CORRECT:
	case ENTRY_ERR_CORRECTED:
		break;
	}
	return true;
}

/* DDS3.26: Extract Preferences */
static bool extract_preferences(PGconn *conn,
				struct batch_with_error *bwe,
				struct matched_preference **mp)
/*
  Returns Matched Preferences if all papers in the batch have: been entered
  twice, have no keystroke errors, and all ignored, informal, missing
  preference number and duplicate preference number papers have a 
  supervisor tick. Otherwise return false.
*/
{
	unsigned int i;
	bool edit_OK=false;

	for(i=0;i<bwe->b.num_papers;i++) {
		if ((edit_OK = check_paper(&bwe->papers[i])) == false)
			break;
	}

	if (edit_OK)
		*mp = make_match_prefs(conn,bwe->b.batch_number);
	else
		*mp = NULL;

	return(edit_OK);
}

/* DDS3.26: Commit Batch */
void commit_batch(PGconn *conn,unsigned int batch_number)
/*
  Commits a batch if all papers in the batch have been entered twice, have no
  keyboard errors, and all ignored, informal, missing preference number and
  duplicate number papers have a supervisor tick.
*/
{
	struct batch_with_error *bwe;
	struct batch *batch;
	struct matched_preference *mp;
	struct normalised_preference *np;

	batch = get_entered_batch(conn,batch_number);
	/* DDS3.30: Find Errors in Batch */
	bwe = find_errors_in_batch(batch);

	if (extract_preferences(conn,bwe,&mp)) {
		np = normalise_batch(mp);
		commit_votes(conn,batch_number,np);
	} else {
	  printf("That batch cannot be committed yet because it contains "
		 "unentered, uncorrected or unapproved papers.\n");
	}
	free_batch(batch);
	free_batch_with_error(bwe);
}

/* DDS3.2: ? */
static void batch_size_prompt(void)
/*
  Print a text prompt.
*/
{
	printf("Please enter the new batch size:");
}

/* DDS3.2: ? */
static unsigned int get_user_batch_size()
/*
  Obtain an batch size from the supervisor.
*/
{
	char *line;
	unsigned int b_size;

	batch_size_prompt();
	line = fgets_malloc(stdin);
	b_size = (unsigned int)atoi(line);
	free(line);
	return(b_size);
}

/* DDS3.24: Change Batch Size */
void chng_batch_size(PGconn *conn,unsigned int batch_number)
/*
  Allows a supervisor to change the size of a batch.
*/
{
	unsigned int batch_size;

	/* Get the new batch size */
	batch_size = get_user_batch_size();

	/* Update the batch size */
	SQL_command(conn,
		    "UPDATE batch "
		    "SET size = %u "
		    "WHERE number = %u;",batch_size,batch_number);
}

/* DDS3.20: Supervisor Duplicate */
static void duplicate(PGconn *conn,unsigned int from_entry_id,
		      unsigned int from_pvn,
		      int to_paper_id,unsigned int to_paper_index,
		      unsigned int batch_number,const char *operator_id,
		      const char *electorate_name)
/*
  Copy entry from one paper to another paper. Entry gets next highest entry
  index in target paper.
*/

{
	int entry_index;
	unsigned int num_prefs;

	num_prefs = SQL_singleton_int(conn,
				      "SELECT COUNT(*) "
				      "FROM %s "
				      "WHERE entry_id = %u;",
				      electorate_name,from_entry_id);

	/* Find OR create next paper */
	if (to_paper_id < 0) {
	        /* Lookup next paper ID */
	        to_paper_id = SQL_singleton_int(conn,
						"SELECT id "
						"FROM paper "
						"WHERE batch_number = %u "
						"AND index = %u;",
						batch_number,to_paper_index);
		/* Create it if it didn't exist */
		if (to_paper_id < 0) {
		        SQL_command(conn,
				    "INSERT INTO paper(batch_number,index) "
				    "VALUES(%u,%u);",
				    batch_number,to_paper_index);
			to_paper_id = get_seq_currval(conn,"paper_id_seq");
		}
	}

	/* Determine entry index for copied entry */
	/* DDS3.20: Next Entry Index */
	entry_index = SQL_singleton_int(conn,
					"SELECT MAX(index) "
					"FROM entry "
					"WHERE paper_id = %d;",
					to_paper_id) + 1;
	/* Set entry_index to 1 if no entries currently exist for this paper */
	if (entry_index < 0)
	      entry_index = 1;

	/* Copy the entry */
	/* DDS3.20: Get Copy of Entry */
	/* DDS3.20: Store Entry */
	SQL_command(conn,
		    "INSERT INTO entry(index,operator_id,"
		    "paper_id,paper_version,num_preferences) "
		    "VALUES(%d,'%s',%d,%u,%u);",
		    entry_index,operator_id,to_paper_id,
		    from_pvn,num_prefs);

	/* Copy the preferences for the entry */
	SQL_command(conn,
		    "INSERT INTO %s "
		    "SELECT CURRVAL('entry_id_seq'),party_index,"
		    "candidate_index,preference_number "
		    "FROM %s "
		    "WHERE entry_id = %u;",
		    electorate_name,electorate_name,from_entry_id);

	/* DDS3.2: Remove Supervisor Tick */
	SQL_command(conn,
		    "UPDATE paper "
		    "SET supervisor_tick = false "
		    "WHERE id = %d;",to_paper_id);
}

/* DDS3.22: Delete Papers in Batch Entry */
static void del_papers(PGconn *conn,unsigned int batch_number,
		       unsigned int electorate_code,
		       unsigned int paper_index,char *operator_id)
/*
  Delete entries from batch?
*/
{
	PGresult *result;
	unsigned int num_rows,i;
	char *electorate_name;

	result = SQL_query(conn,
			   "SELECT p.index,p.id,e.id,e.paper_version "
			   "FROM paper p,entry e "
			   "WHERE p.batch_number = %u "
			   "AND p.index > %u "
			   "AND e.paper_id = p.id "
			   "AND e.operator_id = '%s' "
			   "AND e.index = ( SELECT MAX(index) "
			                   "FROM entry "
			                   "WHERE paper_id = p.id "
			                   "AND operator_id = '%s' ) "
			   "ORDER BY p.index;",
			   batch_number,paper_index,operator_id,operator_id);
	num_rows = PQntuples(result);

	if (num_rows == 0) {
		printf("No action performed.\n"
		       "If entry to be removed is on last paper then "
		       "resize the batch.\n\n");
		return;
	}

	electorate_name = resolve_electorate_name(conn,electorate_code);
    
	/* Copy entries to next lowest paper in batch in succession */
	begin(conn);
	/* Don't wanna see other sessions updates while in transaction */
	SQL_command(conn,"SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;");
	for (i=0;i<num_rows;i++)
		duplicate(conn,
		          atoi(PQgetvalue(result,i,2)),
			  atoi(PQgetvalue(result,i,3)),
			  (i==0)?-1:atoi(PQgetvalue(result,i-1,1)),
			  atoi(PQgetvalue(result,i,0))-1,
			  batch_number,
			  operator_id,electorate_name);
	PQclear(result);
	commit(conn);
	free(electorate_name);
}

/* DDS3.10: Prompt for Paper Index */
static void prompt_for_pi(void)
/*
  Print a text prompt.
*/
{
	printf("Please enter the paper index:");
}

/* DDS3.10: Get Paper Index from User */
static unsigned int get_user_pi()
/*
  Obtain a Paper Index from the User.
*/
{
	char *line;
	unsigned int p_index;

	prompt_for_pi();
	line = fgets_malloc(stdin);
	p_index = (unsigned int)atoi(line);
	free(line);
	return(p_index);
}

/* DDS3.10: Set Paper Index */
static bool set_paper_index(PGconn *conn,
			    unsigned int batch_number,
			    unsigned int *paper_id,
			    unsigned int *paper_index)
/*
  Obtains a Paper Index from the user.
*/
{
	unsigned int p_index;
	unsigned int p_id;

	p_index = get_user_pi();

	/* DDS3.2: Is Paper Index in Entered Batch Details */
	if ((p_id = SQL_singleton_int(conn,
			  "SELECT id FROM paper "
			  "WHERE batch_number = %u "
			  "AND index = %u;",
			  batch_number,p_index)) == -1) {
		/* DDS3.2: Paper Index Warning */
		printf("Paper Index invalid.\n");
		return(false);
	}
	*paper_id = p_id;
	*paper_index = p_index;
	set_current_paper_index(p_index);
	return(true);
}
/* DDS3.16: Prompt for Entry Index */
static void entry_index_prompt(void)
/*
  Print a text prompt.
*/
{
	printf("Please enter the entry index:");
}

/* DDS3.16: Get Entry Index from User */
static unsigned int get_user_entry_index()
/*
  Obtain an Entry Index from the User.
*/
{
	char *line;
	unsigned int p_index;

	entry_index_prompt();
	line = fgets_malloc(stdin);
	p_index = (unsigned int)atoi(line);
	free(line);
	return(p_index);
}

/* DDS3.16: Set Entry Index */
static bool set_entry_index(PGconn *conn,unsigned int paper_id,
			    unsigned int *entry_index,char *operator_id)
/*
  Obtains a Entry Index from the user. If it's valid then also return the
  operator id for that entry.
*/
{
	unsigned int e_index;
	char *o_id;

	e_index = get_user_entry_index();

	/* DDS3.16: Is Entry Index in Entered Batch Details */
	if ((o_id = SQL_singleton(conn,
			  "SELECT operator_id FROM entry "
			  "WHERE paper_id = %u "
			  "AND index = %u;",
			  paper_id,e_index)) == NULL) {
		/* DDS3.16: Invalid Entry Index */
		printf("Entry Index invalid.\n");
		return(false);
	}
	/* Load dynamic parameter return values */
	*entry_index=e_index;
	strcpy(operator_id,o_id);
	free(o_id);
	return(true);
}

/* DDS3.22: Delete Duplicate Paper */
void del_dup_paper(PGconn *conn,unsigned int batch_number,
		   unsigned int electorate_code)
/*
  Obtain batch number, paper number, entry number from the user.
  Make an extra entry of all papers above a paper that was entered twice.
  Put the extra in the previous paper.
*/
{
	unsigned int paper_index;
	unsigned int entry_index;
	char operator_id[OPERATOR_ID_LEN+1];
	int paper_id;

	/* DDS3.2: Store Paper Index */
	if (!set_paper_index(conn,batch_number,&paper_id,&paper_index))
		return;

	if (!set_entry_index(conn,paper_id,
			    &entry_index,operator_id))
		return;

	del_papers(conn,batch_number,electorate_code,
		   paper_index,operator_id);	
}

/* DDS3.14: Insert Papers in Batch Entry */
static void insert_papers(PGconn *conn,unsigned int batch_number,
			  unsigned int electorate_code,
			  unsigned int paper_index,char *operator_id)
/*
  Insert papers into batch?
*/
{
	PGresult *result;
	unsigned int num_rows,i;
	char *electorate_name;

	result = SQL_query(conn,
			   "SELECT p.index,p.id,e.id,e.paper_version "
			   "FROM paper p,entry e "
			   "WHERE p.batch_number = %u "
			   "AND p.index >= %u "
			   "AND e.paper_id = p.id "
			   "AND e.operator_id = '%s' "
			   "AND e.index = ( SELECT MAX(index) "
			                   "FROM entry "
			                   "WHERE paper_id = p.id "
			                   "AND operator_id = '%s' )"
			   "ORDER BY p.index;",
			   batch_number,paper_index,operator_id,operator_id);
	num_rows = PQntuples(result);

	electorate_name = resolve_electorate_name(conn,electorate_code);

	/* Copy entries to next highest paper in batch in succession */
	begin(conn);
	/* Don't wanna see other sessions updates while in transaction */
	SQL_command(conn,"SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;");
	for (i=0;i<num_rows;i++)
		duplicate(conn,atoi(PQgetvalue(result,i,2)),
			  atoi(PQgetvalue(result,i,3)),
			  (i<num_rows-1)?atoi(PQgetvalue(result,i+1,1)):-1,
			  atoi(PQgetvalue(result,i,0))+1,
			  batch_number,
			  operator_id,electorate_name);
	PQclear(result);
	commit(conn);
	free(electorate_name);
}

/* DDS3.14: Insert Missing Paper */
void insert_miss_paper(PGconn *conn,unsigned int batch_number,
		       unsigned int electorate_code)
/*
  Insert a missing paper.
*/
{
	unsigned int paper_index;
	unsigned int entry_index;
	int paper_id;
	char operator_id[OPERATOR_ID_LEN+1];

	/* DDS3.2: Store Paper Index */
	if (!set_paper_index(conn,batch_number,&paper_id,&paper_index))
		return;

	if (!set_entry_index(conn,paper_id,
			    &entry_index,operator_id))
		return;

	insert_papers(conn,batch_number,electorate_code,
		      paper_index,operator_id);
}


/* DDS3.12: Get Last Preferences */
static struct entry *get_last_pref(PGconn *conn,unsigned int batch_number,
				   unsigned int electorate_code,
				   unsigned int paper_index)
/*
  Returns an entry containing the list of preferences from the highest
  entry index.
*/
{
	PGresult *result;
	struct entry *this_entry;
	char *electorate_name;

	/* Get the id of the highest entry index for this paper */
	result = SQL_query(conn,
			   "SELECT e.id,e.num_preferences "
			   "FROM entry e, paper p "
			   "WHERE p.batch_number = %u "
			   "AND p.index = %u "
			   "AND e.paper_id = p.id "
			   "AND e.index = ( SELECT MAX(index) "
			                   "FROM entry "
			                   "WHERE paper_id = e.paper_id );",
					batch_number,paper_index);

	/* Allocate space for this entry */
	this_entry = malloc(sizeof(*this_entry) + 
			    sizeof(this_entry->preferences[0]) *
			    atoi(PQgetvalue(result,0,1)));
	electorate_name = resolve_electorate_name(conn,electorate_code);
	/* And populate it with preferences */
	/* DDS3.2: Store Preferences */
	this_entry->e.num_preferences = atoi(PQgetvalue(result,0,1));
	get_prefs_for_entry(conn,atoi(PQgetvalue(result,0,0)),
			    &this_entry->preferences[0],
			    electorate_name);

	PQclear(result);
	free(electorate_name);
	return(this_entry);
}


/* DDS3.12: Store Preferences */
static void store_pref(struct entry *current_entry)
{
	unsigned int i;

	/* Clear the vote in progress */
	delete_prefs();

	/* Populate the vote in progress with the current preferences */
	for (i=0; i<current_entry->e.num_preferences; i++) {
		add_candidate_with_pref(current_entry->preferences[i]
					.group_index,
					current_entry->preferences[i]
					.db_candidate_index,
					current_entry->preferences[i].prefnum);
	}
}


/* DDS3.12: Populate Vote in Progress */
static void pop_votes(PGconn *conn,unsigned int batch_number,
		      unsigned int electorate_code,unsigned int paper_index)
/*
  Fill the vote in progress store with the last entry of the current batch
  and current index.
*/
{
	struct entry *current_entry;
       
	current_entry = get_last_pref(conn,batch_number,electorate_code,
				      paper_index);

	store_pref(current_entry);
}


/* DDS3.20: Remove Supervisor Tick */
static void rm_sup_tick(PGconn *conn, unsigned int paper_id)		
{
	SQL_command(conn,
		    "UPDATE paper "
		    "SET supervisor_tick = false "
		    "WHERE id = %u;",
		    paper_id);
}


/* DDS3.6: Edit Paper */
void edit_paper(PGconn *conn,unsigned int batch_number,
		unsigned int electorate_code)
/*
  Lets the supervisor edit a paper.
*/
{
	unsigned int paper_index;
	unsigned int paper_id;
	unsigned int pvn;
	bool cancelled;
	struct electorate *electorate;
	struct rotation *rotation;
	char *electorate_name;
	
	set_current_batch_number(batch_number);
	  
	/* DDS3.2: Store Paper Index */
	if (!set_paper_index(conn,batch_number,&paper_id,&paper_index))
		return;

	/* Populate vote in progress */
	pop_votes(conn,batch_number,electorate_code,paper_index);
	
	/* Need to set voter electorate and rotation first */
	electorate_name = resolve_electorate_name(conn,electorate_code);
	electorate = malloc(sizeof(struct electorate)
                                  + strlen(electorate_name)+1);
	strcpy(electorate->name,electorate_name);
	electorate->code = electorate_code;
	electorate->num_seats 
		= (unsigned int)SQL_singleton_int(conn,
						  "SELECT seat_count "
						  "FROM electorate "
						  "WHERE code = %u;",
						  electorate_code);
	store_electorate(electorate);

	pvn = get_paper_version();
	
   	rotation = fetch_rotation(conn, pvn, electorate->num_seats); 
	
	set_current_rotation(rotation); 

	cancelled = enter_paper();
	
	if (!cancelled) {
		confirm_paper(pvn);
		rm_sup_tick(conn, paper_id);
	}
	
}

/* DDS3.42: Has Supervisor Tick */
static bool has_sup_tick(PGconn *conn, unsigned int batch_number, 
		  unsigned int paper_index)
{
	bool tick;

	tick = SQL_singleton_bool(conn, 
				  "SELECT supervisor_tick "
				  "FROM paper "
				  "WHERE batch_number = %u "
				  "AND index = %u;",
				  batch_number, paper_index);
	return tick;
}

/* DDS3.42: Invalid Supervisor Tick */
static void sup_tick_warning(void)
{
	printf("The Specified paper hs already been approved.\n");
}


/* DDS3.42: Add Supervisor Tick */
static void add_sup_tick(PGconn *conn, unsigned int batch_number, 
		  unsigned int paper_index)
{
	SQL_command(conn,
		    "UPDATE paper "
		    "SET supervisor_tick = true "
		    "WHERE batch_number = %u "
		    "AND index = %u;",
		    batch_number, paper_index);

}


/* DDS3.42: Approve Paper */
void app_paper(PGconn *conn, unsigned int batch_number)
{
        struct paper *paper_to_approve;
	struct paper_with_error pwe;
	PGresult *result;
	unsigned int paper_id, paper_index, batch_size;

	if (!set_paper_index(conn, batch_number, &paper_id, &paper_index)) 
	        return;
	
	if (has_sup_tick(conn, batch_number, paper_index)) {
		sup_tick_warning();
	}
	else {   
	        result = SQL_query(conn,
			   "SELECT size FROM batch " 
			   "WHERE number = %u;", batch_number);
		batch_size = atoi(PQgetvalue(result, 0, 0));
	        paper_to_approve = get_paper(batch_number , paper_index);

		pwe = find_errors_in_paper(paper_to_approve,
					   paper_index, batch_size); 

		switch (pwe.error_code) {
		case ENTRY_ERR_CORRECT:
		case ENTRY_ERR_CORRECTED:
		  printf("That paper is correct. Approval not needed.\n");
			break;
		case ENTRY_ERR_IGNORED:
		case ENTRY_ERR_INFORMAL:
		case ENTRY_ERR_MISSING_NUM:
		case ENTRY_ERR_DUPLICATED_NUM:
		        add_sup_tick(conn, batch_number, paper_index);
		        printf("Paper Approved");
			break;
		case ENTRY_ERR_UNENTERED:
		        printf("That paper has not yet been entered twice\n");
			break;
		case ENTRY_ERR_KEYSTROKE:
		        printf("That paper has has errors. Please edit the paper to correct the errors\n");
			break;
		}
		
				
	}
}

/* DDS3.4: Execute Menu Selection */
static void exe_menu_selection(PGconn *conn,char command,
			       unsigned int electorate_code)
/*
  Execute a menu selection.
*/
{
	unsigned int batch_number;
	unsigned int polling_place_code;

	switch (command) {
	case 'A':
		if (set_uncom_batch_num(conn,&batch_number,electorate_code))
			edit_paper(conn,batch_number,electorate_code);
		break;
	case 'B':
		if (set_uncom_batch_num(conn,&batch_number,electorate_code))
			insert_miss_paper(conn,batch_number,electorate_code);
		break;
	case 'C':
		if (set_uncom_batch_num(conn,&batch_number,electorate_code))
			del_dup_paper(conn,batch_number,electorate_code);
		break;
	case 'D':
		if (set_uncom_batch_num(conn,&batch_number,electorate_code))
			chng_batch_size(conn,batch_number);
		break;
	case 'E':
		if (set_uncom_batch_num(conn,&batch_number,electorate_code)) 
			commit_batch(conn,batch_number);
		break;
	case 'F':
		print_batch_summary(conn);
		break;
	case 'G':
		if (set_polling_place(conn,&polling_place_code))
			print_pp_batches(conn,electorate_code,polling_place_code);
		break;
	case 'H':
		if (set_batch_num(conn,&batch_number,electorate_code))
			print_batch(conn,batch_number);
		break;
	case 'I':
		if (set_batch_num(conn,&batch_number,electorate_code))
			print_errors_in_batch(conn,batch_number);
		break;
	case 'J':
		if (set_batch_num(conn,&batch_number,electorate_code))
			app_paper(conn, batch_number); 
		break;
	default:
		printf("ERROR: Unrecognised command = %c\n",command);
		break;
	}
/*	check_for_memory_leak(); */
}

/* DDS3.2: Write Menu */
static void write_menu(PGconn *conn,unsigned int electorate_code)
/*
  Print to screen.
*/
{
	char *electorate_name;

	electorate_name = resolve_electorate_name(conn,electorate_code);

	printf("\n\n------------------------------------------------\n");
	printf("Electorate of %s\n",electorate_name);
	printf("------------------------------------------------\n\n");
	printf("Select one of the following activities by typing "
	       "the corresponding letter (A to J)\n");
	printf("A - Edit Paper\n");
	printf("B - Insert Missing Paper\n");
	printf("C - Delete Duplicated Paper\n");
	printf("D - Change Batch Size\n");
	printf("E - Commit Batch\n");
	printf("F - Print Batch Summary\n");
	printf("G - Print Batches for a Polling Place\n");
	printf("H - Print Batch\n");
	printf("I - Print Errors in Batch\n");
	printf("J - Approve Paper\n\n");
	printf("Enter Menu Selection:");

	free(electorate_name);
}

/* DDS3.2: Get Command */
static char get_BEntry_command(PGconn *conn,unsigned int electorate_code)
/*
  Obtain the command selection.
*/
{
	char command;
	char *dummy;

     	do {
		write_menu(conn,electorate_code);
		command = toupper(getchar());
		dummy = fgets_malloc(stdin); /* THIS IS CRAP */
		free(dummy); /* Throws away newline since getchar doesnt 
				work yet */
	} while (command < 'A' || command > 'J');
	
	return(command);
}

/* DDS3.2: Electorate Warning */
static void elect_warning(void)
/*
  Print a text warning.
*/
{
	printf("Not a valid Electorate.\n\n");
}

/* DDS3.2: Prompt for Electorate */
static void electorate_prompt(void)
/*
  Print a text prompt.
*/
{
	printf("Please enter Electorate:");
}

/* DDS3.2: Get Electorate Name from User */
static unsigned int get_user_elect(PGconn *conn)
/*
  Obtain a valid electorate from the user.
*/
{
	int e_code;
	char *electorate_name;

	do {
		electorate_prompt();
		electorate_name = fgets_malloc(stdin);
		if ((e_code = resolve_electorate_code(conn,electorate_name))
		    == -1) {
			elect_warning();
			free(electorate_name);
		}
	} while ( e_code == -1);

	return((unsigned int)e_code);
}

/* DDS3.2: Set Electorate */
static unsigned int set_electorate(PGconn *conn)
/*
  Set the electorate code in the Batch Electorate store
*/
{
	return(get_user_elect(conn));
}

/* DDS3.2: Batch Edit */
void batch_edit(void)
/*
  Obtain electorate, display the batch edit menu, input and execute a 
  menu selection
*/
{
      	char command;
	unsigned int electorate_code;
	PGconn *conn;

	conn = connect_db_host(DATABASE_NAME,SERVER_ADDRESS);
	if (!conn) bailout ("Can't connect to database '%s' at %s",
			    DATABASE_NAME,SERVER_ADDRESS);
	electorate_code = set_electorate(conn);

	set_ballot_contents(get_electorate_ballot_contents(conn, 
							   electorate_code));

	while (1) {
		command = get_BEntry_command(conn,electorate_code);
		exe_menu_selection(conn,command,electorate_code);
	}

	PQfinish(conn);
}

