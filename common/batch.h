#ifndef _BATCH_H
#define _BATCH_H
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

/* This file contains definitions for batches */

#include <stdbool.h>
#include <pgsql/libpq-fe.h>
#include "evacs.h"

#define OPERATOR_ID_LEN 8


struct predefined_batch 
/* structure defining the relationship between batch numbers and their
   corresponding electorate and polling place */
{
	/* Non-NULL if this is part of a list. */
	struct predefined_batch *next;
        
        unsigned int batch_number;
        unsigned int electorate_code;
        unsigned int polling_place_code;
};

struct matched_preference_info
{
	unsigned int paper_index;
	unsigned int num_preferences;
};

struct matched_preference
/* matched preferences list */
{
	struct matched_preference *next;
	struct matched_preference_info m;
	struct preference preferences[0];
};

struct normalised_preference_info
{
	unsigned int num_preferences;
};

struct normalised_preference
{
	struct normalised_preference *next;
	struct normalised_preference_info n;
	struct preference preferences[0];
};

struct entry_info
/* data common to preferences in an entry */
{
	unsigned int paper_version_num;
	char operator_id[OPERATOR_ID_LEN + 1];
	unsigned int num_preferences;
};	



struct entry
{
	/* Non-NULL if this is part of a list. */
	struct entry *next;

	/* Actual information about this entry */
	struct entry_info e;

	/* Actual preferences hang off end */
	struct preference preferences[0];
};

struct paper_info
/* data common to entries in a paper */
{
	unsigned int index;
	bool supervisor_tick;
};

struct paper
{
	/* Actual information about this paper */
	struct paper_info p;

	/* Pointer to linked list of entries */
	struct entry *entries;
};

struct batch_info
/* data common to papers in a batch */
{
	unsigned int batch_number;
	unsigned int batch_size;
	unsigned int num_papers;
	bool committed;
};

struct batch
{
	/* Non-NULL if this is part of a list. */
	struct batch *next;

	struct batch_info b;

	/* Papers array hangs off end */
	struct paper papers[0];
};

/* batch prototypes */

extern unsigned int get_current_batch_number(void);
extern void set_current_batch_number(unsigned int batch_number);

extern struct paper *get_paper(unsigned int batch_number,
			       unsigned int paper_index);
extern void save_entry(struct entry *newentry, unsigned int batch_number, 
		       unsigned int paper_index);
extern void save_paper(struct paper *newpaper, unsigned int batch_number);
extern struct batch *get_entered_electorate_batches(PGconn *conn,
				    unsigned int electorate_code);
extern struct batch *get_entered_batch(PGconn *conn,
				       unsigned int batch_number);
void get_papers_for_batch(PGconn *conn, struct batch *b_ptr,
			  char *electorate_table_name);
struct entry *get_entries_for_paper(PGconn *conn, unsigned int paper_id,
			   char *electorate_table_name);
void get_prefs_for_entry(PGconn *conn, unsigned int entry_id,
			 struct preference preferernces[],
			 char *electorate_table);
extern void append_entry(PGconn *conn, struct entry *newentry,
			 unsigned int batch_number,unsigned int paper_index,
			 unsigned int paper_version);

extern void free_prefs(struct preference *);
extern struct predefined_batch *resolve_batch_source(PGconn *conn,
						   unsigned int batch_number);
extern char *resolve_electorate_name(PGconn *conn, unsigned int electorate_code);
extern int resolve_electorate_code(PGconn *conn, const char *electorate_name);
extern char *resolve_polling_place_name(PGconn *conn,
					unsigned int polling_place_code);
extern int resolve_polling_place_code(PGconn *conn,
				      const char *polling_place_name);
extern void free_batch(struct batch *batch);
#endif /* _BATCH_H */










