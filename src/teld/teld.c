/*! 
 * @file Telescope deamon.
 * $Id$
 * @author petr
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <mcheck.h>
#include <math.h>
#include <stdarg.h>
#include <pthread.h>
#include <getopt.h>

#include <argz.h>

#include "lx200.h"
#include "../utils/hms.h"
#include "../utils/devdem.h"
#include "../status.h"

#define SERVERD_PORT    	5557	// default serverd port
#define SERVERD_HOST		"localhost"	// default serverd hostname

#define DEVICE_PORT		5556	// default camera TCP/IP port
#define DEVICE_NAME 		"teld"	// default camera name

#define TEL_PORT		"/dev/ttyS0"	// default telescope port

// macro for telescope calls, will write error
#define tel_call(call)   if ((ret = call) < 0) \
{\
	 devser_write_command_end (DEVDEM_E_HW, "Telescope error: %s", strerror(errno));\
	 return ret;\
}

struct radec
{
  double ra;
  double dec;
};

#define RADEC 	((struct radec *) arg)
void
client_move_cancel (void *arg)
{
  // TODO find directions to stop slew, try if it works
  tel_stop_slew_any ();
  devdem_status_mask (0, TEL_MASK_MOVING, TEL_STILL, "move canceled");
}

void *
start_move (void *arg)
{
  int ret;
  devdem_status_mask (0, TEL_MASK_MOVING, TEL_MOVING,
		      "moving of telescope started");
  if ((ret = tel_move_to (RADEC->ra, RADEC->dec)) < 0)
    {
      devdem_status_mask (0, TEL_MASK_MOVING, TEL_STILL, "with error");
    }
  else
    {
      devdem_status_mask (0, TEL_MASK_MOVING, TEL_STILL, "move finished");
    }
  return NULL;
}

void *
start_park (void *arg)
{
  int ret;
  devdem_status_mask (0, TEL_MASK_MOVING, TEL_MOVING,
		      "parking telescope started");
  if ((ret = tel_park ()) < 0)
    {
      devdem_status_mask (0, TEL_MASK_MOVING, TEL_STILL, "with error");
    }
  else
    {
      devdem_status_mask (0, TEL_MASK_MOVING, TEL_STILL, "parked");
    }
  return NULL;
}

#undef RADEC

/*! 
 * Handle teld command.
 *
 * @param command	command to perform
 * 
 * @return -2 on exit, -1 and set errno on HW failure, 0 when command
 * was successfully performed
 */
int
teld_handle_command (char *command)
{
  int ret;
  double dval;

  if (strcmp (command, "ready") == 0)
    {
      tel_call (tel_is_ready ());
    }
  else if (strcmp (command, "set") == 0)
    {
      double ra, dec;
      if (devser_param_test_length (2))
	return -1;
      if (devser_param_next_hmsdec (&ra))
	return -1;
      if (devser_param_next_hmsdec (&dec))
	return -1;
      if (devdem_priority_block_start ())
	return -1;
      tel_call (tel_set_to (ra, dec));
      devdem_priority_block_end ();
    }
  else if (strcmp (command, "move") == 0)
    {
      struct radec coord;
      if (devser_param_test_length (2))
	return -1;
      if (devser_param_next_hmsdec (&(coord.ra)))
	return -1;
      if (devser_param_next_hmsdec (&(coord.dec)))
	return -1;
      if (devdem_priority_block_start ())
	return -1;
      devser_thread_create (start_move, (void *) &coord, sizeof coord, NULL,
			    client_move_cancel);
      devdem_priority_block_end ();
      return 0;
    }
  else if (strcmp (command, "ra") == 0)
    {
      tel_call (tel_read_ra (&dval));
      devser_dprintf ("ra %f", dval);
    }
  else if (strcmp (command, "dec") == 0)
    {
      tel_call (tel_read_dec (&dval));
      devser_dprintf ("dec %f", dval);
    }
  else if (strcmp (command, "park") == 0)
    {
      if (devdem_priority_block_start ())
	return -1;
      devser_thread_create (start_park, NULL, 0, NULL, client_move_cancel);
      devdem_priority_block_end ();
    }
// extended functions
  else if (strcmp (command, "lon") == 0)
    {
      tel_call (tel_read_longtitude (&dval));
      devser_dprintf ("dec %f", dval);
    }
  else if (strcmp (command, "lat") == 0)
    {
      tel_call (tel_read_latitude (&dval));
      devser_dprintf ("dec %f", dval);
    }
  else if (strcmp (command, "lst") == 0)
    {
      tel_call (tel_read_siderealtime (&dval));
      devser_dprintf ("dec %f", dval);
    }
  else if (strcmp (command, "loct") == 0)
    {
      tel_call (tel_read_localtime (&dval));
      devser_dprintf ("dec %f", dval);
    }
  else if (strcmp (command, "exit") == 0)
    {
      return -2;
    }
  else if (strcmp (command, "help") == 0)
    {
      devser_dprintf ("ready - is telescope ready to observe?");
      devser_dprintf ("set - set telescope coordinates");
      devser_dprintf ("move - move telescope");
      devser_dprintf ("ra - telescope right ascenation");
      devser_dprintf ("dec - telescope declination");
      devser_dprintf ("park - park telescope");
      devser_dprintf ("lon - telescope longtitude");
      devser_dprintf ("lat - telescope latitude");
      devser_dprintf ("lst - telescope local sidereal time");
      devser_dprintf ("loct - telescope local time");
      devser_dprintf ("exit - exit from main loop");
      devser_dprintf ("help - print, what you are reading just now");
      ret = errno = 0;
    }
  else
    {
      devser_write_command_end (DEVDEM_E_COMMAND, "Unknow command: '%s'",
				command);
      return -1;
    }
  return ret;
}

void
exit_handler ()
{
  if (!devser_child_pid)
    tel_cleanup ();
  syslog (LOG_INFO, "Exit");
}

int
main (int argc, char **argv)
{
  char *stats[] = { "telescope" };

  char *tel_port = TEL_PORT;

  char *serverd_host = SERVERD_HOST;
  uint16_t serverd_port = SERVERD_PORT;

  char *device_name = DEVICE_NAME;
  uint16_t device_port = DEVICE_PORT;

  char *hostname = NULL;
  int c;

#ifdef DEBUG
  mtrace ();
#endif

  /* get attrs */
  while (1)
    {
      static struct option long_option[] = {
	{"tel_port", 1, 0, 'l'},
	{"port", 1, 0, 'p'},
	{"serverd_host", 1, 0, 's'},
	{"serverd_port", 1, 0, 'q'},
	{"device_name", 1, 0, 'd'},
	{"help", 0, 0, 0},
	{0, 0, 0, 0}
      };
      c = getopt_long (argc, argv, "l:p:s:q:h", long_option, NULL);

      if (c == -1)
	break;

      switch (c)
	{
	case 'l':
	  tel_port = optarg;
	  break;
	case 'p':
	  device_port = atoi (optarg);
	  if (device_port < 1 || device_port == UINT_MAX)
	    {
	      printf ("invalid device port option: %s\n", optarg);
	      exit (EXIT_FAILURE);
	    }
	  break;
	case 's':
	  serverd_host = optarg;
	  break;
	case 'q':
	  serverd_port = atoi (optarg);
	  if (serverd_port < 1 || serverd_port == UINT_MAX)
	    {
	      printf ("invalid serverd port option: %s\n", optarg);
	      exit (EXIT_FAILURE);
	    }
	  break;
	case 'd':
	  device_name = optarg;
	  break;
	case 0:
	  printf
	    ("Options:\n\tserverd_port|p <port_num>\t\tport of the serverd");
	  exit (EXIT_SUCCESS);
	case '?':
	  break;
	default:
	  printf ("?? getopt returned unknow character %o ??\n", c);
	}
    }

  if (optind != argc - 1)
    {
      printf ("hostname wasn't specified\n");
      exit (EXIT_FAILURE);
    }

  hostname = argv[argc - 1];

  // open syslog
  openlog (NULL, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

  if (devdem_init (stats, 1))
    {
      syslog (LOG_ERR, "devdem_init: %m");
      exit (EXIT_FAILURE);
    }

  if (tel_connect (tel_port) < 0)
    {
      syslog (LOG_ERR, "tel_connect: %s", strerror (errno));
      exit (EXIT_FAILURE);
    }

  if (devdem_register
      (serverd_host, serverd_port, device_name, DEVICE_TYPE_MOUNT, hostname,
       device_port) < 0)
    {
      perror ("devser_register");
      exit (EXIT_FAILURE);
    }

  atexit (exit_handler);

  return devdem_run (device_port, teld_handle_command);
}
