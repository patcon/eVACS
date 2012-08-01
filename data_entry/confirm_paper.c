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
#include <pwd.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdlib.h>
#include <common/safe.h>  
#include <common/batch.h>
#include <common/find_errors.h>
#include <common/current_paper_index.h>
#include <voting_client/vote_in_progress.h>

#include "confirm_paper.h"
static unsigned int number_of_entries;

/* DDS???: Get Operator ID */
char *get_operator_id(void)
{
	char *name;

	struct passwd *passwd;
	
	passwd = getpwuid(getuid());
	
	name = malloc(strlen(passwd->pw_name) * sizeof(char) + 1);
	
	strcpy(name, passwd->pw_name);

	name[8] = '\0';
	
	return name;
}

/* DDS3.22: Save Entry */
static bool save_deo_vote(unsigned int batch_number, unsigned int paper_index, 
		   struct entry *newentry)
{
	unsigned int i;
	const struct preference_set *prefs;

	prefs = get_vote_in_progress();

	/* Insert preferences from vote in progress */
	for (i=0; i<prefs->num_preferences; i++) {
		newentry->preferences[i].prefnum 
			= prefs->candidates[i].prefnum;
		newentry->preferences[i].group_index
			= prefs->candidates[i].group_index;
		newentry->preferences[i]
			.db_candidate_index 
			= prefs->candidates[i].db_candidate_index;
		newentry->e.num_preferences++;
	}
	/* Save the new entry in the entered batch details store */
	save_entry(newentry, batch_number, paper_index); 
	
	return true;
}

/* DDS3.22: Confirm Paper */
void confirm_paper(unsigned int pvn)
{
	struct paper *paper;
	struct entry *newentry, *i,*lasti;
	unsigned int batch_number, paper_index;
	char *operator_id;
	const struct preference_set *prefs;
	bool equal, save_ok, confirm_save = true;

	operator_id = get_operator_id();
	
	batch_number = get_current_batch_number(); 
	paper_index = get_current_paper_index();
	paper = get_paper(batch_number, paper_index);
	
	/* Count the number of entries already in paper */
	number_of_entries = 0;

	for (i = paper->entries; i; i = i->next) {
		    number_of_entries++;
		    lasti=i;
	}

	/* If more than one entry, need to check if existing entries are 
	   correct or contain uncorrectable errors, i.e. two previous entries 
	   match.
	   But if operator is a SUPER, don't perform this check*/
	
	if ((strncmp(operator_id,"SUPER",5) !=0) && (number_of_entries > 1)) {
		for (i = paper->entries; i; i = i->next) {
			if ( i != lasti ) {
				equal = compare_entries(lasti, i);
				if (equal) {
					/* Discard new entry */
					confirm_save = false;
					break;
				}
			}
		}
	}
	/* Save new entry */
	if (confirm_save) {
		prefs = get_vote_in_progress();
		newentry = malloc(sizeof(struct entry) 
					+ strlen(operator_id) + 1
					+sizeof(struct preference) 
					* prefs->num_preferences);
		newentry->e.paper_version_num = pvn;
		newentry->e.num_preferences = 0;
		strcpy(newentry->e.operator_id, operator_id);
		save_ok =  save_deo_vote(batch_number, paper_index, newentry);
		if (!save_ok) {
			bailout("Could not save paper - "
				"Internal Error\n");
		}
	}
	update_current_paper_index();

}
