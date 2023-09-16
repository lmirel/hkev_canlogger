
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/if.h>

#include <stropts.h>
#include <poll.h>

#include <stdarg.h>
#include <stdio.h>

#define DEBUG 1
void dbg_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, "\n#D:");
    vfprintf(stdout, fmt, args);
    va_end(args);
}
#define TRACE(x) do { if (DEBUG) dbg_printf x; } while (0)

void out_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, "\n#I:");
    vfprintf(stdout, fmt, args);
    va_end(args);
}
#define PRINT(x) do { out_printf x; } while (0)

void err_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "\n#E:");
    vfprintf(stderr, fmt, args);
    va_end(args);
}
#define ERROR(x) do { err_printf x; } while (0)

#define debug_print(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)
#define debug_error(fmt, ...) \
	do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); if (DEBUG) fflush (stderr);} while (0)
#define log_print(fmt, ...) \
	do { dbg_printf(); fflush (stdout);} while (0)
#define log_error(fmt, ...) \
	do { fprintf(stderr, fmt, __VA_ARGS__); fflush (stderr);} while (0)


int open_canfile(char *fn)
{
  int fd = open(fn, O_RDONLY);
	if(-1 == fd)
	{
		ERROR(("Open Failed"));
		return -1;
	}
	PRINT(("file opened at %d", fd));
	return fd;
}

#include <limits.h>
#include <termios.h>
int open_canserial(char *_dashport, int spd)
{
	char tbuf[PATH_MAX];
	struct termios options;
	int _dashportfd = -1;
	//open using realpath() symlink to actual /dev/ttyUSBx
	if (realpath (_dashport, tbuf))
		_dashportfd = open (tbuf, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (_dashportfd == -1)
	{
		ERROR (("serial device not found on %s", tbuf));
		//_dashport = NULL;
		return _dashportfd;
	}
	//set options
	tcgetattr (_dashportfd, &options);
	//
	cfsetispeed (&options, spd);
	cfsetospeed (&options, spd);
	cfmakeraw (&options);
	//
	if (tcsetattr (_dashportfd, TCSANOW, &options) < 0)
	{
		ERROR (("cannot set options for serial device %s", tbuf));
		close (_dashportfd);
		_dashportfd = -1;
		//_dashport = NULL;
	}
	char lb[3];
	while(read(_dashportfd, lb, 1) == 1) 
		printf("\npipe cleaning serial port");
	PRINT(("connected to serial device '%s', on %d", tbuf, _dashportfd));
	return _dashportfd;
}

static void usage(char *app)
{
  printf("%s %s\n", app, "0.1");
  printf ("\n");
}

char *opt_infn = NULL; //input file name
char *opt_insd = NULL; //input serial device /dev/ttyUSBx
char *opt_oucan = "vcan0";//

#include <getopt.h>
int env_init (int argc, char *argv[])
{
  /*
   * init params and stuff
   */
  //
  int c;

  struct option long_options[] = {
    /* These options don't set a flag. We distinguish them by their indices. */
    { "help",   no_argument,       0, '?' },
    { "file",   required_argument, 0, 'f' },
    { "serial", required_argument, 0, 's' },
    { "can",    required_argument, 0, 'c' },
    { 0, 0, 0, 0 }
  };

  while (1)
  {
    /* getopt_long stores the option index here. */
    int option_index = 0;

    c = getopt_long (argc, argv, "s:f:c:?", long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c) {
    case '?':
      usage (argv[0]);
      exit (0);
      break;
    case 'f': //use dash to forward data
      opt_infn = optarg;
      break;
    case 's': //use dash to forward data
      opt_insd = optarg;
      break;
    case 'c': //use dash to forward data
      opt_oucan = optarg;
      break;
    default:
      printf("unrecognized option: %c\n", c);
    }
  }

  //configuration summary 
  printf ("\n# ##");
  printf ("\n#virtual can sender");
  printf ("\n#running configuration:");
  printf ("\n#   output can: %s (-c)", opt_oucan?opt_oucan:"n/a");
  printf ("\n#   input file: %s (-f)", opt_infn?opt_infn:"n/a");
  printf ("\n# input serial: %s (-s)", opt_insd?opt_insd:"n/a");
  printf ("\n# ##");
  //
	if (!opt_infn && !opt_insd)
	{
		ERROR(("please provide input device or file; aborting.\n"));
		exit(-2);
	}
	//
  return 1;
}

//int usleep(useconds_t usec);
int process_can_frame(char *msg, int s)
{
	struct can_frame frame;
	int nbytes;
	static long lts = 0;
	//can frame msg format
	//<[27]  7D4   [08]  03 22 01 01 AA AA AA AA
	//current timestamp
	long cts = (int)strtol(msg + 2, NULL, 10);
	//sleep only for file handling
	if (1 && (opt_infn && lts > 0))
	{
		//printf("#sleep for %ldms (cts %ld)\n", (cts - lts), cts);
		usleep((cts - lts) * 1000);
	}
	lts = cts;
	//find end of timestamp
  char *cp = strchr(msg, ']');
  int canid = (int)strtol(cp + 2, NULL, 16);
	//find start of frame length
	cp = strchr(cp + 2, '[');
	int fl = (int)strtol(cp + 1, NULL, 16);
	frame.can_id  = canid;
	frame.can_dlc = fl;
	//find end of frame len
	cp = strchr(cp + 1, ']') + 2;
	for (int i = 0; i< fl; i++)
		frame.data[i] = (int)strtol(cp + i * 3, NULL, 16);
	if (1)
	{
		PRINT(("CAN: %s  %0X   [%02X] ", opt_oucan, canid, fl));
		for (int i = 0; i< fl; i++)
			printf(" %02X", frame.data[i]);
	}
	nbytes = write(s, &frame, sizeof(struct can_frame));
	if (nbytes < 1)
		ERROR(("wrote %dB vs %dB", nbytes, sizeof(struct can_frame)));
	return 1;
}

int main(int argc, char *argv[])
{
	int s;
	struct sockaddr_can addr;
	struct ifreq ifr;
	int infd = -1;
	const char *ifname = opt_oucan;

  env_init (argc, argv);

	if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) == -1) {
		perror("Error while opening socket");
		return -1;
	}

	strcpy(ifr.ifr_name, ifname);
	ioctl(s, SIOCGIFINDEX, &ifr);

	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

	printf("%s at index %d\n", ifname, ifr.ifr_ifindex);

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("Error in socket bind");
		return -2;
	}
	infd = opt_infn?open_canfile(opt_infn):open_canserial(opt_insd, B115200);
	struct pollfd fds[2];
	int timeout_msecs = 5000;
	int ret;
	int i;
	char msg[120];
	int msgr = 0;
	int _go_on = 1;
	while (_go_on && infd != -1)
	{
		/* poll */
		memset ((void*)fds, 0, sizeof(fds));
		fds[0].fd = infd;
		fds[0].events = POLLIN;
		ret = poll(fds, 1, timeout_msecs);
		TRACE(("poll ret %d, evt 0x%X vs 0x%X", ret, fds[0].revents, fds[0].events));
		if (ret > 0)
		{
			/* An event on one of the fds has occurred. */
			for (i = 0; i < 1; i++)
			{
				if (fds[i].revents & POLLIN)
				{
					/* Data may be read from device number i. */
					while (msgr < 120)
					{
						if (read(fds[i].fd, msg+msgr, 1) == 1)
						{
							//TRACE(("read char: %02X", msg[msgr]));
							msgr++;
							if ((msg[msgr-1] == '\r' || msg[msgr-1] == '\n'))
							{
								msg[msgr-1] = 0;
								msgr = 0;
								TRACE(("read line: <%s>", msg));
								//only process CAN messages starting with '<'out or '>'in
								if (*msg == '<' || *msg == '>')
								{
									process_can_frame(msg, s);
								}
							}
						}
						else
						{
							if (opt_infn)
							{
								//file reading failure means EOF
								TRACE(("finished processing data"));
								_go_on = 0;
								break;
							}
							else
								break;
						}

					}
					TRACE(("read %dB", msgr));
				}
				if (fds[i].revents != POLLIN) 
				{
					printf("\n#D:fd=%d; events 0x%X: %s%s%s\n", fds[i].fd, fds[i].revents,
						(fds[i].revents & POLLIN)  ? "POLLIN "  : "",
						(fds[i].revents & POLLHUP) ? "POLLHUP " : "",
						(fds[i].revents & POLLERR) ? "POLLERR " : "");
					_go_on = 0;
					break;
				}
			} //for all
		} //if poll event
		if (ret == 0)
		{
			TRACE(("TIMEOUT"));
			_go_on = 0;
		}
		if (ret < 0)
		{
			TRACE(("finished processing data"));
			_go_on = 0;
		}
	} //while 1
	TRACE(("cleaning up.."));
	close(infd);
	TRACE(("done.\n"));
	return 0;
}
