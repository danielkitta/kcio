/*
 * Copyright (c) 2008  Daniel Elstner <daniel.kitta@gmail.com>
 *
 * KC-Send is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * KC-Send is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <build/config.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <libkc/libkc.h>

#define BAUDRATE_NORMAL B1200
#define BAUDRATE_BOOST  B2400

/* Address of CAOS 4.2 V24DUP initialization table */
#define V24DUPINIT 0xA809u

static const unsigned char v24boost[] =
{
  0x54, V24DUPINIT & 0xFF, V24DUPINIT >> 8, 0x09, 0x00,
  0x47, 0x17, 0x18, 0x04, 0x44, 0x03, 0xE1, 0x05, 0x6A  /* 2400 Baud duplex */
};
static const unsigned char v24normal[] =
{
  0x54, V24DUPINIT & 0xFF, V24DUPINIT >> 8, 0x09, 0x00,
  0x47, 0x2E, 0x18, 0x04, 0x44, 0x03, 0xE1, 0x05, 0x6A  /* 1200 Baud duplex */
};
static const unsigned char v24escape[] = { 0x1B };

static int            stdout_isterm;  /* log progress on standard output? */
static int            portfd;         /* serial port file descriptor */
static struct termios portattr;       /* serial port "terminal" settings */

static void
exit_usage(void)
{
  fprintf(stderr, "Usage: kcsend [-p PORT] [FILE]...\n");
  exit(1);
}

static void
exit_error(const char* where)
{
  perror(where);
  exit(1);
}

static void
change_baudrate(speed_t rate)
{
  cfsetispeed(&portattr, rate);
  cfsetospeed(&portattr, rate);

  if (tcsetattr(portfd, TCSADRAIN, &portattr) < 0)
    exit_error("change baudrate");
}

static void
send_sequence(const unsigned char* data, ssize_t length)
{
  ssize_t written = 0;

  while (written < length)
  {
    ssize_t rc = write(portfd, &data[written], length - written);
    if (rc >= 0)
      written += rc;
    else if (errno != EINTR)
      exit_error("send sequence");
  }

  while (tcdrain(portfd) < 0)
  {
    if (errno != EINTR)
      exit_error("send sequence");
  }
}

static unsigned int
send_kcfile(const char* filename)
{
  FILE*         kcfile;
  unsigned char block[128];

  if (filename[0] == '-' && filename[1] == '\0')
    kcfile = stdin;
  else
    kcfile = fopen(filename, "rb");

  if (!kcfile || fread(&block, sizeof block, 1, kcfile) <= 0)
    exit_error(filename);

  int nargs = block[16];
  unsigned int load = block[17] | (unsigned)block[18] << 8;
  unsigned int end  = block[19] | (unsigned)block[20] << 8;

  if (nargs < 2 || nargs > 10 || load >= end)
  {
    fprintf(stderr, "%s: invalid raw tape image header\n", filename);
    exit(1);
  }
  unsigned int start = 0xFFFF;
  if (nargs >= 3)
    start = block[21] | (unsigned)block[22] << 8;

  unsigned int length = end - load;

  send_sequence(v24escape, sizeof v24escape);
  change_baudrate(BAUDRATE_BOOST);

  if (stdout_isterm)
  {
    wchar_t name[12];

    for (int i = 0; i < 11; ++i)
      name[i] = kc_to_wide_char(block[i]);
    name[11] = L'\0';

    printf("%ls %.4X %.4X\n01>", name, load, end);
    fflush(stdout);
  }

  const unsigned char prolog[] = { 0x54, load & 0xFF, load >> 8, length & 0xFF, length >> 8 };
  send_sequence(prolog, sizeof prolog);

  unsigned int offset = 0;

  while (offset < length)
  {
    size_t nread = length - offset;
    if (nread > sizeof block)
      nread = sizeof block;
    nread = fread(block, 1, nread, kcfile);

    if (nread == 0)
    {
      fprintf(stderr, "%s: premature end of file\n", filename);
      exit(1);
    }
    offset += nread;
    send_sequence(block, nread);

    if (stdout_isterm)
    {
      printf("\r%.2X>", (offset / 128 + 1) & 0xFF);
      fflush(stdout);
    }
  }
  change_baudrate(BAUDRATE_NORMAL);

  if (kcfile != stdin && fclose(kcfile) != 0)
    exit_error(filename);

  if (stdout_isterm)
    puts("\rFF>");

  return start;
}

static void
init_serial_port(const char* portname)
{
  if (tcgetattr(portfd, &portattr) < 0)
    exit_error(portname);

  portattr.c_iflag &= ~(IGNBRK | INPCK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF);
  portattr.c_iflag |= BRKINT;

  portattr.c_oflag &= ~(OPOST | OCRNL | OFILL);

  portattr.c_cflag &= ~(CLOCAL | CSIZE | CSTOPB | PARENB);
  portattr.c_cflag |= CREAD | CS8 | HUPCL | CRTSCTS; /* not in POSIX */

  portattr.c_lflag &= ~(ICANON | IEXTEN | ISIG | ECHO | NOFLSH | TOSTOP);

  portattr.c_cc[VINTR] = 3;
  portattr.c_cc[VMIN]  = 1;
  portattr.c_cc[VTIME] = 0;

  cfsetispeed(&portattr, BAUDRATE_NORMAL);
  cfsetospeed(&portattr, BAUDRATE_NORMAL);

  if (tcsetattr(portfd, TCSAFLUSH, &portattr) < 0 ||
      tcgetattr(portfd, &portattr) < 0)
    exit_error(portname);

  if ((portattr.c_cflag & (CSIZE | CSTOPB | PARENB | CRTSCTS)) != (CS8 | CRTSCTS)
      || cfgetispeed(&portattr) != BAUDRATE_NORMAL
      || cfgetospeed(&portattr) != BAUDRATE_NORMAL)
  {
    fprintf(stderr, "%s: serial port configuration not supported\n", portname);
    exit(1);
  }
}

int
main(int argc, char** argv)
{
  const char*  portname = "/dev/ttyS0";
  int          c;

  while ((c = getopt(argc, argv, "p:?")) != -1)
    switch (c)
    {
      case 'p': portname = optarg; break;
      case '?': exit_usage();
      default:  abort();
    }

  if (optind >= argc)
    exit_usage();

  setlocale(LC_ALL, "");
  stdout_isterm = isatty(STDOUT_FILENO);

  portfd = open(portname, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (portfd < 0)
    exit_error(portname);

  init_serial_port(portname);

  int flags = fcntl(portfd, F_GETFL, 0);
  if (flags < 0 || fcntl(portfd, F_SETFL, flags & ~O_NONBLOCK) < 0)
    exit_error(portname);

  if (stdout_isterm)
    printf("Using serial port %s\n", portname);

  send_sequence(v24escape, sizeof v24escape);
  send_sequence(v24boost,  sizeof v24boost);

  unsigned int autostart = 0xFFFF;

  for (int i = optind; i < argc; ++i)
    autostart = send_kcfile(argv[i]);

  send_sequence(v24escape, sizeof v24escape);
  change_baudrate(BAUDRATE_BOOST);
  send_sequence(v24normal, sizeof v24normal);
  change_baudrate(BAUDRATE_NORMAL);

  if (autostart < 0xFFFF)
  {
    const unsigned char v24exec[] = { 0x55, autostart & 0xFF, autostart >> 8 };

    send_sequence(v24escape, sizeof v24escape);
    send_sequence(v24exec,   sizeof v24exec);
  }

  if (close(portfd) < 0)
    exit_error(portname);

  return 0;
}
