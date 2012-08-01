#ifndef _BALLOT_CONTENTS_H
#define _BALLOT_CONTENTS_H 

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

/* this file contains the prototype accessors for ballot contents   */


/* This is as much of the ballot contents as we need to know (this
   electorate only) */
struct ballot_contents
{
	/* Total number of groups */
	unsigned int num_groups;

	/* Number of candidates in each group (hangs off end) */
	/* Indexed by group number - 1 (ie. zero-based) */
	unsigned int num_candidates[0];
};

/* Return the ballot contents */
extern struct ballot_contents *get_ballot_contents(void);

/* Set the ballot contents (called from authenticate) */
extern void set_ballot_contents(struct ballot_contents *bc);

#endif /* _BALLOT_CONTENTS_H  */
