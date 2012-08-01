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

/* Database interface routines for counting. */
#include <stdlib.h>
#include <string.h>
#include <common/database.h>
/*#include <common/safe.h>*/
#include "ballot_iterators.h"
#include "candidate_iterators.h"
#include "fetch.h"

/* Case-insensitive search for electorate: NULL if not found. */
struct electorate *fetch_electorate(PGconn *conn, const char *ename)
{
	struct electorate *elec;
	PGresult *result;

	result = SQL_query(conn,
			   "SELECT code, seat_count FROM electorate "
			   "WHERE name = '%s';", ename);
	if (PQntuples(result) != 1) {
		PQclear(result);
		return NULL;
	}

	elec = malloc(sizeof(*elec) + strlen(ename)+1);
	elec->code = atoi(PQgetvalue(result, 0, 0));
	elec->num_seats = atoi(PQgetvalue(result, 0, 1));
	strcpy(elec->name, ename);
	elec->next = NULL;

	return elec;
}

/* Given the non-NULL electorate, fill in all the groups, return number. */
unsigned int fetch_groups(PGconn *conn, 
			  const struct electorate *elec,
			  struct group *groups)
{
	PGresult *result;
	unsigned int i;

	result = SQL_query(conn,
			   "SELECT name, abbreviation, index FROM party "
			   "WHERE electorate_code = %u "
			   "ORDER by index;", elec->code);
	for (i = 0; i < PQntuples(result); i++) {
		groups[i].name = strdup(PQgetvalue(result, i, 0));
		groups[i].abbrev = strdup(PQgetvalue(result, i, 1));
		groups[i].group_index = atoi(PQgetvalue(result, i, 2));
	}
	PQclear(result);
	return i;
}
/* Find a group by index */
static struct group *find_group(struct group *groups, unsigned int index)
{
	while (groups->group_index != index)
		groups++;
	return groups;
}

/* Given the group information, return the candidate list */
struct cand_list *fetch_candidates(PGconn *conn, 
				   const struct electorate *elec,
				   struct group *groups)
{
	struct cand_list *list = NULL;
	unsigned int i;
	PGresult *result;

	/* By returning them in order, we help the scrutiny sheet generation */
	result = SQL_query(conn,
			   "SELECT name, index, party_index FROM candidate "
			   "WHERE electorate_code = %u "
			   "ORDER BY party_index DESC, name DESC;", elec->code);
	for (i = 0; i < PQntuples(result); i++) {
		list = new_cand_list(malloc(sizeof(struct candidate)), list);
		list->cand->name = strdup(PQgetvalue(result, i, 0));
		list->cand->db_candidate_index
			= atoi(PQgetvalue(result, i, 1));
		list->cand->group = find_group(groups,
					       atoi(PQgetvalue(result, i, 2)));
		list->cand->count_when_quota_reached = 0;
		/* We are PREpending to list, so count is backwards */
		list->cand->scrutiny_pos = PQntuples(result) - i - 1;
		/* All piles empty, all totals 0 */
		memset(list->cand->c, 0, sizeof(list->cand->c));
		/* surplus distributed flag: init false */
		list->cand->surplus_distributed=false;
	}
	PQclear(result);
	return list;
}

/* Load a single vote */
static struct ballot *load_vote(PGconn *conn, const char *vote_id)
{
	struct ballot *ballot;
	PGresult *result;
	unsigned int i;

	result = SQL_query(conn, "SELECT prefnum,db_candidate_index,group_index "
			   "FROM confirmed_preference "
			   "WHERE vote_id = %s;", vote_id);
	ballot = malloc(sizeof(*ballot)
			+ sizeof(ballot->prefs[0])*PQntuples(result));
	ballot->num_preferences = PQntuples(result);
	ballot->count_transferred = 0;
	/* They many not be in order */
	for (i = 0; i < ballot->num_preferences; i++) {
		ballot->prefs[atoi(PQgetvalue(result, i, 0))-1]
			.group_index = atoi(PQgetvalue(result, i, 2));
		ballot->prefs[atoi(PQgetvalue(result, i, 0))-1]
			.db_candidate_index = atoi(PQgetvalue(result, i, 1));
	}

	PQclear(result);
	return ballot;
}
/* Get all the ballots for this electorate */
struct ballot_list *fetch_ballots(PGconn *conn, const struct electorate *elec)
{
	struct ballot_list *list = NULL;
	PGresult *result;
	unsigned int i,num_votes,five_percent;

	/* It would be better to do a join here */
	result = SQL_query(conn,
			   "SELECT id FROM confirmed_vote "
			   "WHERE electorate_code = %u;", elec->code);
	num_votes =  PQntuples(result);
	five_percent = (num_votes / 20);
	fprintf(stderr,"0|                     |100%"
		"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
	for (i = 0; i < PQntuples(result); i++) {
	        list = new_ballot_list(load_vote(conn,
						 PQgetvalue(result, i, 0)),
				       list);
		if ((i % five_percent) == 0) fprintf(stderr, "#");
	}
	fprintf(stderr, "\n");					  
	PQclear(result);
	return list;
}
