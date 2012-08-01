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

#include <common/voter_electorate.h>
#include "draw_group_entry.h"
#include "message.h"
#include "verify_barcode.h"
#include "image.h"
#include "get_img_at_cursor.h"
#include "initiate_session.h"
#include "voting_client.h"
#include "audio.h"

struct coordinates 
{
	unsigned int x;
	unsigned int y;
};

/* DDS3.2.12: Get Group Width */
static unsigned int get_group_width(const struct electorate *voter_electorate)
{
	struct image *image;

	image = get_group_image(voter_electorate->code,1);
       
	return image_width(image);
}

static unsigned int get_group_h_height(const struct electorate 
				       *voter_electorate)
{
	struct image *image;

	image = get_group_image(voter_electorate->code,1);

	return image_height(image);
}


/* DDS3.2.12: Get Preference Image Size */
static unsigned int get_preference_image_size(const struct electorate 
					      *voter_electorate)
{
	struct image *image;
	
	image = get_preference_image(voter_electorate->code, 0);
	
	return image_height(image);
}


/* DDS3.2.12: Get Ballot Heading Height */
static unsigned int get_ballot_h_height(const struct electorate 
					*voter_electorate)
{
	struct image *ballot_h_image;
	
	ballot_h_image = get_bh_image(get_language(), voter_electorate);

	return image_height(ballot_h_image);
}


/* Calculate how many groups fit across the screen */
static unsigned int get_groups_across(const struct electorate *voter_elec)
{
	unsigned int group_width;

	group_width = get_group_width(voter_elec);

	/* We want to round UP, not DOWN. */
	/* NOOOO, we want to round down!!! or else we get a partial group */
	return (get_screen_width()) / group_width; 
}

/* How tall is a group? */
static unsigned int get_group_height(const struct electorate *voter_electorate)
{
	return get_group_h_height(voter_electorate)
		+ (voter_electorate->num_seats
		   * get_preference_image_size(voter_electorate));
}

/* DDS3.2.12: Calculate Cursor Coordinates */
static struct coordinates calc_curs_coord(unsigned int group_index, 
					   int candidate_index)
{
	struct coordinates coordinates;
	const struct electorate *voter_electorate;
	unsigned int group_height, num_columns;
	
	voter_electorate = get_voter_electorate();
	group_height = get_group_height(voter_electorate);
	num_columns = get_groups_across(voter_electorate);

	/* First calculate coordinates of group */
	coordinates.x = (group_index % num_columns)
		* get_group_width(voter_electorate);
	coordinates.y = get_ballot_h_height(voter_electorate)
		+ (group_index / num_columns) * group_height;

	/* Now calculate coordinates of this candidate */
	coordinates.y += (candidate_index + 1)
		* get_preference_image_size(voter_electorate);

	return coordinates;
}

/* DDS3.2.12: Get Cursor Coordinates */
static struct coordinates get_cursor_coordinates(struct cursor cursor)
{
	struct coordinates coordinates;
	
	coordinates = calc_curs_coord(cursor.group_index, 
				      cursor.screen_candidate_index);
	
	return coordinates;
}


/* DDS3.2.12: Draw Group Entry */
void draw_group_entry(struct cursor cursor, enum HlFlag highlight,
		      bool interrupt)
{
	struct image_set set;
	struct image *image, *highlighted;
	struct coordinates coordinates;
	unsigned int pref_size;
	struct audio_set audio = { NULL, { NULL, NULL, NULL } };

	/* We play audio only if highlighted */
	if (highlight)
		audio = get_audio_at_cursor(&cursor);

	set = get_img_at_cursor(&cursor);
	
	coordinates = get_cursor_coordinates(cursor);

	/* set contains either a group heading image or a candidate 
	   and pref_box image */
	if (set.group) {
		image = set.group;
		if (highlight) {
			highlighted = highlight_image(image);
			paste_image(coordinates.x, coordinates.y, highlighted);
			play_audio_loop(interrupt, audio.group_audio);
		}
		else {
			paste_image(coordinates.x, coordinates.y, image);
		}
	}
	else {
		/* set contains candidate and pref_box images */
		image = set.candidate;
		pref_size = image_width(set.prefnumber);
		paste_image(coordinates.x, coordinates.y, set.prefnumber);
		if (highlight) {
			highlighted = highlight_image(image);
			paste_image(coordinates.x + pref_size, coordinates.y, 
				    highlighted);

			/* Audio for this candidate */
			play_multiaudio_loop(interrupt, 3,
					     audio.candidate_audio);
		} else {
			paste_image(coordinates.x + pref_size, coordinates.y, 
				    image);
		}
				   
	}
}

/* Draw a blank groun entry */
void draw_blank_entry(unsigned int group_index,
		      int screen_index)
{
	struct image *blank;
	struct cursor cursor;
	struct coordinates coords;

	/* Create a dummy cursor for this position */
	cursor.group_index = group_index;
	cursor.screen_candidate_index = screen_index;
	/* Convert to screen coordinates */
	coords = get_cursor_coordinates(cursor);

	/* Get the blank image for this electorate, and paste it */
	blank = get_blank_image(get_voter_electorate()->code);
	paste_image(coords.x, coords.y, blank);
}

/* How many groups *could* fit on the screen? */
unsigned int groups_possible(void)
{
	unsigned int num_columns, column_height, num_rows;
	const struct electorate *voter_electorate;

	voter_electorate = get_voter_electorate();
	num_columns = get_groups_across(voter_electorate);
	column_height = get_group_height(voter_electorate);

	/* We want to round UP, not down */
	num_rows = (get_screen_height() - get_ballot_h_height(voter_electorate)
		    + column_height-1) / column_height;

	return num_rows * num_columns;
}
