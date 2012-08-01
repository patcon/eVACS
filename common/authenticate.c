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
#include <assert.h>
#include <stdlib.h>
#include "authenticate.h"
#include "voting_errors.h"
#include "http.h"
#include "ballot_contents.h"

/* Convert the ballot contents from the http variables */
static enum error unpack_ballot_contents(const struct http_vars *vars)
{
	const char *val;
	int i;
	struct ballot_contents *bc;

	/* Server response must contain num_groups, and groupXX entries */
	val = http_string(vars, "num_groups");
	if (!val || (i = atoi(val)) <= 0)
		return ERR_SERVER_RESPONSE_BAD;

	/* Now we know the number of groups, we can allocate memory */
	bc = malloc(sizeof(*bc) + i*sizeof(unsigned int));
	if (!bc)
		return ERR_INTERNAL;

	bc->num_groups = i;

	/* Extract number of candidates for each group */
	for (i = 0; i < bc->num_groups; i++) {
		char name[sizeof("group") + INT_CHARS];

		sprintf(name, "group%u", (unsigned int)i);
		val = http_string(vars, name);
		if (!val)
			return ERR_SERVER_RESPONSE_BAD;
		bc->num_candidates[i] = atoi(val);
		assert(bc->num_candidates[i] > 0);
	}
	/* ballot contents must be set only once from each authenticate 
	   invocation  */
	assert(!get_ballot_contents());
	set_ballot_contents(bc);
	return ERR_OK;
}

/* DDS3.2.4: Authenticate */
/* Authenticate barcode: returns electorate (to be freed by caller),
   or calls display_error() itself */
enum error authenticate(const struct barcode *bc, struct electorate **elecp)
{
	struct http_vars *reply;
	enum error ret;
	const char *val;
	const struct http_vars request[]
		= { { (char *)"barcode", (char *)bc->ascii }, { NULL, NULL } };

	reply = http_exchange(get_server(), get_port(),
			      AUTHENTICATE_CGI, request);
	/* Some error occurred? */
	if (http_error(reply) != ERR_OK)
		return http_error(reply);

	/* We should have an electorate name, electorate code, and
	   number of seats. */
	val = http_string(reply, "electorate_name");
	if (!val)
		return ERR_SERVER_RESPONSE_BAD;

	/* Now we have the name, we can allocate space for the electorate */
	*elecp = malloc(sizeof(struct electorate) + strlen(val)+1);
	if (!*elecp) return ERR_INTERNAL;
	strcpy((*elecp)->name, val);

	val = http_string(reply, "electorate_code");
	if (!val)
		return ERR_SERVER_RESPONSE_BAD;
	(*elecp)->code = atoi(val);

	val = http_string(reply, "electorate_seats");
	if (!val)
		return ERR_SERVER_RESPONSE_BAD;
	(*elecp)->num_seats = atoi(val);

	/* We also set the ballot contents from these variables */
	ret = unpack_ballot_contents(reply);
	http_free(reply);
	return ret;
}


