/* This file is (C) copyright 2001 Software Improvements, Pty Ltd.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pgsql/libpq-fe.h>
#include <common/database.h>
#include <common/authenticate.h>
#include <common/rotation.h>
#include <common/safe.h>
#include "voting_server.h"
#include "reconstruct.h"
#include "cgi.h"
#include "save_and_verify.h"

/* This does the authenticate step as well (we need the ballot details
   anyway) */
static struct electorate *find_electorate(const struct barcode *barcode)
{
	enum error err;
	struct electorate *elec;

	err = authenticate(barcode, &elec);
	if (err != ERR_OK)
		cgi_error_response(err);

	return elec;
}

static struct rotation decode_rotation(const struct http_vars *vars,
				       unsigned int num_seats)
{
	struct rotation rot;
	unsigned int i;

	rot.size = num_seats;
	for (i = 0; i < rot.size; i++) {
		char varname[strlen("rotation")
			    + sizeof(STRINGIZE(MAX_ELECTORATE_SEATS))];
		const char *val;

		sprintf(varname, "rotation%u", i);
		val = http_string(vars, varname);
		rot.rotations[i] = atoi(val);
	}

	/* Do sanity checks on input: must be all numbers up to rot.size */
	for (i = 0; i < rot.size; i++) {
		unsigned int j;

		if (rot.rotations[i] >= rot.size)
			bailout("Bad rotation #%u: %u\n", i, rot.rotations[i]);

		for (j = 0; j < rot.size; j++) {
			if (j != i && rot.rotations[j] == rot.rotations[i])
				bailout("Rotations %u & %u == %u\n",
					j, i, rot.rotations[i]);
		}
	}
	return rot;
}

static const char *get_number(unsigned int *num, const char *str)
{
	char *endp;
	unsigned long l;

	/* No number?  Return NULL */
	if (!num)
		return NULL;

	errno = 0;
	l = strtoul(str, &endp, 10);
	/* No characters consumed? */
	if (endp == str)
		return NULL;

	/* Overflow? */
	if (l == ULONG_MAX && errno == ERANGE) return NULL;
	if (l > UINT_MAX) return NULL;

	/* Store return value */
	*num = l;

	/* Swallow one comma if there is one */
	if (*endp == ',') return endp + 1;
	else return endp;
}

/* Convert an ascii vote string to a set of preferences */
static struct preference_set unwrap_vote(const char *vote)
{
	const char *votep;
	struct preference_set prefs;
	unsigned int i;

	for (i = 0, votep = vote; *votep != '\0'; i++) {
		prefs.candidates[i].prefnum = i+1;
		votep = get_number(&prefs.candidates[i].group_index, votep);
		votep = get_number(&prefs.candidates[i].db_candidate_index,
				   votep);
		if (!votep || i == PREFNUM_MAX-1)
			bailout("Malformed vote string `%s'\n", vote);
	}

	prefs.num_preferences = i;
	return prefs;
}

/* This commits a vote.  It operates in two modes: master and
   slave. */
/* DDS3.2.26: Commit Vote */
int main(int argc, char *argv[])
{
	struct http_vars *vars;
	const char *keystrokes;
	struct preference_set prefs;
	struct rotation rot;
	struct electorate *elec;
	struct barcode bc;
	enum error err;
	PGconn *conn;

	/* Tell the other functions to use our bailout code */
	set_cgi_bailout();

	conn = connect_db_port("evacs", get_database_port());

	/* Don't free this: we keep pointers into it */
	vars = cgi_get_arguments();

	/* Unwrap CGI variables */
	strncpy(bc.ascii, http_string(vars, "barcode"), sizeof(bc.ascii)-1);
	bc.ascii[sizeof(bc.ascii)-1] = '\0';
	if (!bar_decode_ascii(&bc))
		cgi_error_response(ERR_BARCODE_MISREAD);

	elec = find_electorate(&bc);
	keystrokes = http_string(vars, "keystrokes");
	prefs = unwrap_vote(http_string(vars, "vote"));
	rot = decode_rotation(vars, elec->num_seats);

	/* Compare vote they gave with reconstructed voter keystrokes */
	if (!reconstruct_and_compare(&rot, keystrokes, &prefs)) {
		fprintf(stderr,"%s: Reconstructed keystrokes do not match\n",
			am_i_master() ? "master" : "slave");
		cgi_error_response(ERR_RECONSTRUCTION_FAILED);
	}

	/* Do the actual verification and commit */
	err = save_and_verify(conn, &prefs, &bc, elec, vars);

	/* Cleanup */
	http_free(vars);
	PQfinish(conn);

	/* This will be an OK response if err = ERR_OK */
	cgi_error_response(err);
}
