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
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <signal.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "child_barcode.h"
#include "input.h"
#include "audio.h"
#include "image.h"

/* Keycodes which form reset keys */
static int reset_key[] = { 0x54 /*5*/, 0x51 /*9*/, 0x57 /*1*/ };

/* static means this starts at all false */
static bool reset_key_status[sizeof(reset_key)/sizeof(reset_key[0])];

/* The barcode-reading child */
static pid_t child;

/* The UNIX pipe they will send the barcode down */
static int pipe_from_child;

struct keymap
{
	unsigned int xkey;
	enum input_event evacs_key;
};

struct keymap keymap[] = {
	{0x4d, INPUT_START_AGAIN},
	{0x50, INPUT_UP},
	{0x53, INPUT_PREVIOUS},
	{0x58, INPUT_DOWN},
	{0x55, INPUT_NEXT},
	{0x5a, INPUT_SELECT},
	{0x16, INPUT_UNDO},
	{0x6c, INPUT_FINISH}
};

void do_reset(void)
{
	/* Terminate the child */
	kill(child, SIGTERM);

	/* Clean up the audio device */
	audio_shutdown();

	/* Close the display */
	close_display();

	/* Exit the program */
	exit(0);
}

static XEvent get_event(void)
{
	XEvent event;
	unsigned int i;

	XNextEvent(get_display(), &event);

	/* Do processing of reset events here, in common code. */
	if (event.type == KeyRelease) {
		/* If it's a reset key, mark it false */
		for (i = 0; i < sizeof(reset_key)/sizeof(reset_key[0]); i++) {
			if (reset_key[i] == event.xkey.keycode)
				reset_key_status[i] = false;
		}
	} else if (event.type == KeyPress) {
		/* If it's a reset key, mark it true */
		for (i = 0; i < sizeof(reset_key)/sizeof(reset_key[0]); i++) {
			if (reset_key[i] == event.xkey.keycode)
				reset_key_status[i] = true;
		}

		/* Are they all true? */
		for (i = 0; reset_key_status[i] == true; i++) {
			if (i == sizeof(reset_key)/sizeof(reset_key[0])-1)
				do_reset();
		}
	}

	return event;
}

/* Return the map for a give X keycode */
static enum input_event map_key(unsigned int keycode)
{
	unsigned int i;

	for (i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++) {
		if (keymap[i].xkey == keycode)
			return keymap[i].evacs_key;
	}
	/* An unused key was pressed */
	return INPUT_UNUSED;
}

/* DDS???: Read Barcode */
static enum input_event read_barcode(struct barcode *bc)
{
	int n;

	/* Child will have written nul-terminated barcode to the pipe */
	n = read(pipe_from_child, bc->ascii, sizeof(bc->ascii));
	assert(n > 0);

	/* Make sure it's terminated. */
	bc->ascii[sizeof(bc->ascii)-1] = '\0';

	return INPUT_BARCODE;
}

/* Waiting for either a keypress or a barcode (in which case,
   bc->ascii filled in). */
enum input_event get_keystroke_or_barcode(struct barcode *bc)
{
	XEvent event;

	/* Loop until we get a relevent keypress, or client message. */
	do {
		event = get_event();
		if (event.type == ClientMessage)
			return read_barcode(bc);
	} while (event.type != KeyPress);

	return map_key(event.xkey.keycode);
}

/* DDS???: Get Keystroke */
enum input_event get_keystroke(void)
{
	enum input_event event;

	/* We want to loop here reading the barcode, so they aren't
           buffered */
	do {
		struct barcode dummy;
		event = get_keystroke_or_barcode(&dummy);
	} while (event == INPUT_BARCODE);

	return event;
}

/* Wait for end of the world */
void wait_for_reset(void)
{
	/* get_event handles RESET internally */
	while (true) get_event();
}

bool initialize_input(void)
{
	/* We fork, and child reads the serial.  It sends us a barcode
	   through the pipe, and notifies us via a synthetic X event */
	int pipeend[2];
	struct pollfd pollset;
	char dummy;

	if (pipe(pipeend) != 0)
		return false;

	child = fork();
	/* Did fork fail? */
	if (child == (pid_t)-1) {
		close(pipeend[0]);
		close(pipeend[1]);
		return false;
	}

	/* Are we the child? */
	if (child == 0) {
		close(pipeend[0]);
		child_barcode(pipeend[1]);
	}

	close(pipeend[1]);
	pipe_from_child = pipeend[0];

	/* Give kiddy 5 seconds to acknowledge. */
	pollset.fd = pipe_from_child;
	pollset.events = POLL_IN;

	/* If we timeout, or the read fails, return false. */
	if (poll(&pollset, 1, 5000) != 1
	    || read(pipe_from_child, &dummy, 1) != 1) {
		close(pipe_from_child);
		return false;
	}

	return true;
}
