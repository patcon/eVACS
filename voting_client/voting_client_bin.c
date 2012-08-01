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
#include "voting_client.h"
#include "message.h"
#include "input.h"
#include "audio.h"
#include "initiate_session.h"
#include "accumulate_preferences.h"

/* DDS3.1: Do Voting */
int main(int argc, char *argv[])
{
	/* If we have an argument, monitor is upside down */
	initialise_display((argc > 1));
	audio_init();
	if (!initialize_input())
		display_error(ERR_INTERNAL);
	init_session();
	accumulate_preferences();
	display_ack_screen();
	do_reset();
}
