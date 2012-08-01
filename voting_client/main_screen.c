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
#include <common/ballot_contents.h>
#include <common/voter_electorate.h>
#include <common/authenticate.h>
#include "voting_client.h"
#include "message.h"
#include "initiate_session.h"
#include "verify_barcode.h"
#include "draw_group_entry.h"
#include "main_screen.h"
#include "audio.h"

/* DDS3.2.8: Display Main Voting Screen */
/* DDS3.12: Display DEO Ballot Screen */
void dsp_mn_vt_scn(void)
{
	const struct electorate *electorate;
	struct ballot_contents *bc;
	unsigned int group_index;
	int cand;
	struct cursor cursor_position;
	struct image *ballot_h_image;

	electorate = get_voter_electorate();
	bc = get_ballot_contents();
	ballot_h_image = get_bh_image(get_language(), electorate);
	paste_image(0,0,ballot_h_image);

	for (group_index = 0; 
	     group_index < bc->num_groups; 
	     group_index++) {
		/* candidate -1 corresponds to group heading */
		for (cand = -1; 
		     cand < (int)bc->num_candidates[group_index]; 
		     cand++) {
			cursor_position.group_index = group_index;
			cursor_position.screen_candidate_index = cand;
			draw_group_entry(cursor_position,NO,false);
		}
		/* For the rest in the group, draw blank entry */
		for (; cand < electorate->num_seats; cand++) 
			draw_blank_entry(group_index, cand);
	}
	/* Fill in any "blank groups" */
	for (; group_index < groups_possible(); group_index++) {
		for (cand = -1; cand < (int)electorate->num_seats; cand++) 
			draw_blank_entry(group_index, cand);
	}
}
