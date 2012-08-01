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
#include <common/evacs.h>
#include <common/safe.h>

#include "database.h"

/* DDS 3.6: Find Errors in all Papers */
extern paper_with_errors *find_errors_in_electorate_papers(
						unsigned int electorate_code)
{
        PGconn *conn;
	PGresult *result;
	struct preference *prefs=NULL;
	struct paper_with_errors *papers=NULL;
	
	conn = PQsetdb(NULL, NULL, NULL, NULL, DATABASE_NAME);
	if (PQstatus(conn) != CONNECTION_OK) bailout(
			 "FATAL: Couldn't access DB: '%s'\n",DATABASE_NAME); 

	prefs=get_electorate_prefs(conn, electorate_code);

	PQfinish(conn);

	papers = find_errors_in_papers(prefs);
	
	return papers;
}

extern paper_with_errors *find_errors_in_batch_papers(
						unsigned int batch_number)
{
        PGconn *conn;
	PGresult *result;
	struct preference *prefs=NULL;
	struct paper_with_errors *papers=NULL;
	
	conn = connect_db(DATABASE_NAME);
	if (conn == NULL) bailout(
			 "FATAL: Couldn't access DB: '%s'\n",DATABASE_NAME); 

	prefs=get_batch_prefs(conn, batch_number);

	PQfinish(conn);

	papers = find_errors_in_papers(prefs);

	return papers;
}


static struct paper_with_errors find_errors_in_papers(struct preference *prefs)
{
        PGconn *conn;
	PGresult *result;
	struct preference *next=prefs, *tmpprf=prefs;
	struct paper_with_errors *papers=NULL,*tmppwe=NULL;
	struct entry_with_error *entry,*tmpewe=NULL;  
	unsigned int current_batch, current_paper, current_entry;
	
	/* build a paper_with_errors from preference list */
	while (next) {
	  current_batch = next->batch;
	  current_paper = next->paper;
	  current_entry = next->entry;
	  
	  if ( (tmppwe = lsearch_paper(papers, current_batch, current_paper))
	       == NULL) {
	    /* insert new paper */
	        tmppwe = malloc(sizeof(*papers));
	        tmppwe->batch = next->batch;
		tmppwe->paper = next->paper;
		tmppwe->entries = NULL;
		tmppwe->next = NULL;
	  }
	  
	  if ( (tmpewe = lsearch_entry(tmppwe->entries, current_entry)
		      == NULL) ) {
	    /* insert new entry into paper */
		        tmpewe = malloc(sizeof(*entry));
		        tmpewe->entry_index = current_entry;
		        tmpewe->operator = next->operator_id;
		}
	  /* insert new preference into entry */
		tmpewe->preference = pref_copy(next);
		tmpewe->preference->next = NULL;
		     
		    
		
		     
		     
	        tmppwe->entries = entry;
	        tmppwe->entry->
		
		
	
}










