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
#include <stdbool.h>
#include <assert.h>
#include "cursor.h"

/* Create a mapping for the rotation, collapsed to this number of
   candidates */
static void produce_collapsed_map(const struct rotation *rot,
				  unsigned int map[],
				  unsigned int num_candidates)
{
	unsigned int i,j=0;

	/* get the number of seats from the robson rotation */

	/* run through each robson rotation position
	   adding the value to the map if there is a candidate
	   at that position  
	*/
	for (i = 0; i < rot->size; i++) {
	  /* is there a candidate for this position?   */
	        if (rot->rotations[i] < num_candidates) {
		  /* yes, add it to the map  */
	                map[j]=rot->rotations[i];
	                j++;
	        }
	}
	
}

unsigned int translate_dbci_to_sci(unsigned int num_candidates,
				   unsigned int db_candidate_index,
				   const struct rotation *rot)
{
	unsigned int map[MAX_ELECTORATE_SEATS];
	unsigned int i;

	assert(num_candidates <= rot->size);
	assert(db_candidate_index < rot->size);
	assert(db_candidate_index < num_candidates);

	produce_collapsed_map(rot, map, num_candidates);

	/* Look for the screen candidate index which maps onto this
	   db_candidate_index */
	for (i = 0; map[i] != db_candidate_index; i++)
		assert(i < num_candidates);

	return i;
}

/* DDS3.2.12: Translate SCI to DBCI */
unsigned int translate_sci_to_dbci(unsigned int num_candidates,
				   unsigned int screen_candidate_index,
				   const struct rotation *rot)
{
	unsigned int map[MAX_ELECTORATE_SEATS];
	unsigned int i;

	assert(screen_candidate_index < num_candidates);

	produce_collapsed_map(rot, map, num_candidates);

	/* Look for the candidate index which maps onto this
	   screen_candidate_index */
	for (i = 0; i != map[screen_candidate_index]; i++)
		assert(i < num_candidates);

	return i;
}
