#ifndef BATCH_EDIT_H
#define BATCH_EDIT_H
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


extern void batch_edit(void);
extern void edit_paper(PGconn *conn,unsigned int batch_number,
		       unsigned int electorate_code);
extern void insert_miss_paper(PGconn *conn,unsigned int batch_number,
			      unsigned int electorate_code);
extern void del_dup_paper(PGconn *conn,unsigned int batch_number,
			  unsigned int electorate_code);
extern void chng_batch_size(PGconn *conn,unsigned int batch_number);
extern void commit_batch(PGconn *conn,unsigned int batch_number);
extern void print_batch_summary(PGconn *conn);
extern void print_pp_batches(PGconn *conn, unsigned int elect_code, unsigned int pp_code);
extern void print_batch(PGconn *conn,unsigned int batch_number);
extern void print_errors_in_batch(PGconn *conn,unsigned int batch_number);
extern void app_paper(PGconn *conn, unsigned int batch_number); 
#endif /* BATCH_EDIT_H */
