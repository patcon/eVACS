#ifndef _DRAW_GROUP_ENTRY_H
#define _DRAW_GROUP_ENTRY_H
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
#include <common/cursor.h>
struct electorate;

enum HlFlag
{
	NO,
	YES
};

/* Draw Group Entry: if highlighted, audio will be played.  If
   interrupt is true, the current audio will be interrupted. */
extern void draw_group_entry(struct cursor cursor,
			     enum HlFlag highlight,
			     bool interrupt);

/* Draw a blank group entry */
extern void draw_blank_entry(unsigned int group_index, int screen_index);

/* How many groups *could* fit on the screen? */
extern unsigned int groups_possible(void);

#endif /*_DRAW_GROUP_ENTRY_H*/
