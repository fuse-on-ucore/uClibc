/* Tests for UTMP functions.
   Copyright (C) 1998, 2001-2003 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Mark Kettenis <kettenis@phys.uva.nl>, 1998.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#ifdef UTMPX
# include <utmpx.h>
# define utmp utmpx
# define utmpname utmpxname
# define setutent setutxent
# define getutent getutxent
# define endutent endutxent
# define getutline getutxline
# define getutid getutxid
# define pututline pututxline
#else
# include <utmp.h>
#endif

#ifndef _HAVE_UT_TYPE
# define _HAVE_UT_TYPE 0
#endif
#ifndef _HAVE_UT_PID
# define _HAVE_UT_PID 0
#endif
#ifndef _HAVE_UT_ID
# define _HAVE_UT_ID 0
#endif
#ifndef _HAVE_UT_TV
# define _HAVE_UT_TV 0
#endif
#ifndef _HAVE_UT_HOST
# define _HAVE_UT_HOST 0
#endif

#if _HAVE_UT_TYPE || defined UTMPX

/* Prototype for our test function.  */
static int do_test (int argc, char *argv[]);

/* We have a preparation function.  */
static void do_prepare (int argc, char *argv[]);
#define PREPARE do_prepare

/* This defines the `main' function and some more.  */
#include "../test-skeleton.c"


/* These are for the temporary file we generate.  */
char *name;
int fd;

static void
do_prepare (int argc, char *argv[])
{
  size_t name_len;

  name_len = strlen (test_dir);
  name = malloc (name_len + sizeof ("/utmpXXXXXX"));
  mempcpy (mempcpy (name, test_dir, name_len),
	   "/utmpXXXXXX", sizeof ("/utmpXXXXXX"));
  add_temp_file (name);

  /* Open our test file.  */
  fd = mkstemp (name);
  if (fd == -1) {
    fprintf (stderr, "cannot open test file `%s': ", name);
    perror (NULL);
    exit (EXIT_FAILURE);
  }
}

struct utmp entry[] =
{
#if _HAVE_UT_TV || defined UTMPX
#define UT(a)  .ut_tv = { .tv_sec = (a)}
#else
#define UT(a)  .ut_time = (a)
#endif

  { .ut_type = BOOT_TIME, .ut_pid = 1, UT(1000) },
  { .ut_type = RUN_LVL, .ut_pid = 1, UT(2000) },
  { .ut_type = INIT_PROCESS, .ut_pid = 5, .ut_id = "si", UT(3000) },
  { .ut_type = LOGIN_PROCESS, .ut_pid = 23, .ut_line = "tty1", .ut_id = "1",
    .ut_user = "LOGIN", UT(4000) },
  { .ut_type = USER_PROCESS, .ut_pid = 24, .ut_line = "tty2", .ut_id = "2",
    .ut_user = "albert", UT(8000) },
  { .ut_type = USER_PROCESS, .ut_pid = 196, .ut_line = "ttyp0", .ut_id = "p0",
    .ut_user = "niels", UT(10000) },
  { .ut_type = DEAD_PROCESS, .ut_line = "ttyp1", .ut_id = "p1", UT(16000) },
  { .ut_type = EMPTY },
  { .ut_type = EMPTY }
};
int num_entries = sizeof entry / sizeof (struct utmp);

time_t entry_time = 20000;
pid_t entry_pid = 234;

static int
do_init (void)
{
  int n;

  setutent ();

  for (n = 0; n < num_entries; n++)
    {
      if (pututline (&entry[n]) == NULL)
	{
	  perror ("cannot write UTMP entry");
	  return 1;
	}
    }

  endutent ();

  return 0;
}


static int
do_check (void)
{
  struct utmp *ut;
  int n;

  setutent ();

  n = 0;
  while ((ut = getutent ()))
    {
      if (n < num_entries &&
	  memcmp (ut, &entry[n], sizeof (struct utmp)))
	{
	  fprintf (stderr, "UTMP entry does not match\n");
	  return 1;
	}

      n++;
    }

  if (n != num_entries)
    {
      fprintf (stderr, "number of UTMP entries is incorrect\n");
      return 1;
    }

  endutent ();

  return 0;
}

static int
simulate_login (const char *line, const char *user)
{
  int n;

  for (n = 0; n < num_entries; n++)
    {
      if (strcmp (line, entry[n].ut_line) == 0 ||
	  entry[n].ut_type == DEAD_PROCESS)
	{
	  if (entry[n].ut_pid == DEAD_PROCESS)
	    entry[n].ut_pid = (entry_pid += 27);
	  entry[n].ut_type = USER_PROCESS;
	  strncpy (entry[n].ut_user, user, sizeof (entry[n].ut_user));
#if _HAVE_UT_TV - 0 || defined UTMPX
	  entry[n].ut_tv.tv_sec = (entry_time += 1000);
#else
          entry[n].ut_time = (entry_time += 1000);
#endif
	  setutent ();

	  if (pututline (&entry[n]) == NULL)
	    {
	      perror ("cannot write UTMP entry");
	      return 1;
	    }

	  endutent ();

	  return 0;
	}
    }

  fprintf (stderr, "no entries available\n");
  return 1;
}

static int
simulate_logout (const char *line)
{
  int n;

  for (n = 0; n < num_entries; n++)
    {
      if (strcmp (line, entry[n].ut_line) == 0)
	{
	  entry[n].ut_type = DEAD_PROCESS;
	  strncpy (entry[n].ut_user, "", sizeof (entry[n].ut_user));
#if _HAVE_UT_TV - 0 || defined UTMPX
          entry[n].ut_tv.tv_sec = (entry_time += 1000);
#else
          entry[n].ut_time = (entry_time += 1000);
#endif
	  setutent ();

	  if (pututline (&entry[n]) == NULL)
	    {
	      perror ("cannot write UTMP entry");
	      return 1;
	    }

	  endutent ();

	  return 0;
	}
    }

  fprintf (stderr, "no entry found for `%s'\n", line);
  return 1;
}

static int
check_login (const char *line)
{
  struct utmp *up;
  struct utmp ut;
  int n;

  setutent ();

  strcpy (ut.ut_line, line);
  up = getutline (&ut);
  if (up == NULL)
    {
      fprintf (stderr, "cannot get entry for line `%s': ", line);
	  perror(NULL);
      return 1;
    }

  endutent ();

  for (n = 0; n < num_entries; n++)
    {
      if (strcmp (line, entry[n].ut_line) == 0)
	{
	  if (memcmp (up, &entry[n], sizeof (struct utmp)))
	    {
	      fprintf (stderr, "UTMP entry does not match\n");
	      return 1;
	    }

	  return 0;
	}
    }

  fprintf (stderr, "bogus entry for line `%s'\n", line);
  return 1;
}

static int
check_logout (const char *line)
{
  struct utmp ut;

  setutent ();

  strcpy (ut.ut_line, line);
  if (getutline (&ut) != NULL)
    {
      fprintf (stderr, "bogus login entry for `%s'\n", line);
      return 1;
    }

  endutent ();

  return 0;
}

static int
check_id (const char *id)
{
  struct utmp *up;
  struct utmp ut;
  int n;

  setutent ();

  ut.ut_type = USER_PROCESS;
  strcpy (ut.ut_id, id);
  up = getutid (&ut);
  if (up == NULL)
    {
      fprintf (stderr, "cannot get entry for ID `%s': ", id);
	  perror (NULL);
      return 1;
    }

  endutent ();

  for (n = 0; n < num_entries; n++)
    {
      if (strcmp (id, entry[n].ut_id) == 0)
	{
	  if (memcmp (up, &entry[n], sizeof (struct utmp)))
	    {
	      fprintf (stderr, "UTMP entry does not match\n");
	      return 1;
	    }

	  return 0;
	}
    }

  fprintf (stderr, "bogus entry for ID `%s'\n", id);
  return 1;
}

static int
check_type (int type)
{
  struct utmp *up;
  struct utmp ut;
  int n;

  setutent ();

  ut.ut_type = type;
  up = getutid (&ut);
  if (up == NULL)
    {
      fprintf (stderr, "cannot get entry for type `%d': ", type);
	  perror (NULL);
      return 1;
    }

  endutent ();

  for (n = 0; n < num_entries; n++)
    {
      if (type == entry[n].ut_type)
	{
	  if (memcmp (up, &entry[n], sizeof (struct utmp)))
	    {
	      fprintf (stderr, "UTMP entry does not match\n");
	      return 1;
	    }

	  return 0;
	}
    }

  fprintf (stderr, "bogus entry for type `%d'\n", type);
  return 1;
}

static int
do_test (int argc, char *argv[])
{
  int result = 0;

  utmpname (name);

  result |= do_init ();
  result |= do_check ();

  result |= simulate_login ("tty1", "erwin");
  result |= do_check ();

  result |= simulate_login ("ttyp1", "paul");
  result |= do_check ();

  result |= simulate_logout ("tty2");
  result |= do_check ();

  result |= simulate_logout ("ttyp0");
  result |= do_check ();

  result |= simulate_login ("ttyp2", "richard");
  result |= do_check ();

  result |= check_login ("tty1");
  result |= check_logout ("ttyp0");
  result |= check_id ("p1");
  result |= check_id ("2");
  result |= check_id ("si");
  result |= check_type (BOOT_TIME);
  result |= check_type (RUN_LVL);

  return result;
}

#else

/* No field 'ut_type' in struct utmp.  */
int
main ()
{
  return 0;
}

#endif
