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
#define BAUDRATE_BOOST  B19200

static const unsigned char v24boostcode[] = /* hex dump of v24boost.asm */
{
  0x54, 0x00, 0xB7, 0x76, 0x00, 0x01, 0x03, 0x07,
  0xCD, 0x47, 0xB7, 0xCD, 0x58, 0xB7, 0x5F, 0xCD,
  0x58, 0xB7, 0x57, 0xCD, 0x58, 0xB7, 0x6F, 0xCD,
  0x58, 0xB7, 0x67, 0x18, 0x13, 0xCD, 0x58, 0xB7,
  0x12, 0x13, 0x80, 0x47, 0x0D, 0x20, 0xF6, 0xDB,
  0x0B, 0xE6, 0x04, 0x28, 0xFA, 0x78, 0xD3, 0x09,
  0x01, 0x80, 0x00, 0xAF, 0xED, 0x42, 0x30, 0xE5,
  0x09, 0x4D, 0x6F, 0xB9, 0x38, 0xDF, 0x3E, 0x01,
  0xF3, 0xD3, 0x0B, 0xDB, 0x0B, 0xFB, 0x0F, 0x30,
  0xF5, 0x01, 0x2E, 0x09, 0x21, 0x6D, 0xB7, 0x3E,
  0x47, 0xF3, 0xD3, 0x0D, 0x79, 0xD3, 0x0D, 0x0E,
  0x0B, 0xED, 0xB3, 0xFB, 0xC9, 0xDB, 0x0B, 0x0F,
  0x3F, 0x3E, 0x05, 0xF3, 0xD3, 0x0B, 0x3E, 0xD4,
  0x1F, 0xD3, 0x0B, 0xFB, 0x07, 0x38, 0xEE, 0xDB,
  0x09, 0xC9, 0x18, 0x04, 0x44, 0x03, 0xE1, 0x05,
  0xEA, 0x11, 0x18
};
static const unsigned char v24boostrun[] = { 0x55, 0x00, 0xB7 }; /* 'U' B700 */
static const unsigned char v24escape[]   = { 0x1B };
static const unsigned char v24mcload[]   = { 0x54 }; /* 'T' */

static int            stdout_isterm;  /* log progress on standard output? */
static int            portfd;         /* serial port file descriptor */
static int            boostmode;      /* enable 19200 Baud transfer? */
static struct termios portattr;       /* serial port "terminal" settings */

static void
exit_usage(void)
{
  fputs("Usage: kcsend [-p PORT] [-o OFFSET] [-l] [-n] [FILE]...\n", stderr);
  exit(1);
}

static void
change_baudrate(speed_t rate)
{
  cfsetispeed(&portattr, rate);
  cfsetospeed(&portattr, rate);

  if (tcsetattr(portfd, TCSADRAIN, &portattr) < 0)
    kc_exit_error("change baudrate");
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
      kc_exit_error("send sequence");
  }

  while (tcdrain(portfd) < 0)
  {
    if (errno != EINTR)
      kc_exit_error("send sequence");
  }
}

static unsigned int
receive_byte(void)
{
  unsigned char byte;
  ssize_t       count;

  while ((count = read(portfd, &byte, 1)) < 0)
  {
    if (errno != EINTR)
      kc_exit_error("receive byte");
  }
  if (count == 0)
  {
    fputs("receive byte: connection broken\n", stderr);
    exit(1);
  }
  return byte;
}

static unsigned int
send_kcfile(const char* filename, unsigned int loadoffset)
{
  FILE*         kcfile;
  unsigned char block[128];

  if (filename[0] == '-' && filename[1] == '\0')
    kcfile = stdin;
  else
    kcfile = fopen(filename, "rb");

  if (!kcfile || fread(&block, sizeof block, 1, kcfile) <= 0)
    kc_exit_error(filename);

  int nargs = block[16];
  unsigned int load = block[17] | (unsigned)block[18] << 8;
  unsigned int end  = block[19] | (unsigned)block[20] << 8;

  if (nargs < 2 || nargs > 10 || load >= end)
  {
    fprintf(stderr, "%s: invalid raw tape image header\n", filename);
    exit(1);
  }
  send_sequence(v24escape, sizeof v24escape);

  if (boostmode)
  {
    send_sequence(v24boostrun, sizeof v24boostrun);
    change_baudrate(BAUDRATE_BOOST);
  }
  else
    send_sequence(v24mcload, sizeof v24mcload); /* just the 'T' command */

  unsigned int length = end - load;

  load = (load + loadoffset) & 0xFFFFu;
  end  = (end  + loadoffset) & 0xFFFFu;

  unsigned int start = 0xFFFF;

  if (nargs >= 3)
    start = block[21] | (unsigned)block[22] << 8;

  if (nargs == 3)
    start = (start + loadoffset) & 0xFFFFu;

  if (stdout_isterm)
  {
    wchar_t name[12];

    for (int i = 0; i < 11; ++i)
      name[i] = kc_to_wide_char(block[i]);
    name[11] = L'\0';

    printf("%ls %.4X %.4X", name, load, end);

    if (nargs >= 3)
      printf(" %.4X", start);

    fputs("\n01>", stdout);
    fflush(stdout);
  }

  const unsigned char prolog[] = { load & 0xFF, load >> 8, length & 0xFF, length >> 8 };
  send_sequence(prolog, sizeof prolog);

  unsigned int offset = 0;

  while (offset < length)
  {
    size_t blocksize = length - offset;
    if (blocksize > sizeof block)
      blocksize = sizeof block;
    size_t nread = fread(block, 1, blocksize, kcfile);

    if (nread == 0)
      break;
    offset += nread;
    send_sequence(block, nread);

    const char* indicator = ">";

    if (boostmode)
    {
      unsigned int checksum = 0;

      for (size_t i = 0; i < nread; ++i)
        checksum += block[i];

      if (receive_byte() != (checksum & 0xFF))
        indicator = "*\n"; /* checksum error */
    }
    if (stdout_isterm)
    {
      if (nread != blocksize)
        indicator = "*\n"; /* end of file error */

      printf("\r%.2X%s", (offset / 128 + 1) & 0xFF, indicator);
      fflush(stdout);
    }

    if (nread == 0)
      break;
  }

  if (kcfile != stdin && fclose(kcfile) != 0)
    kc_exit_error(filename);

  if (offset == length && stdout_isterm)
    puts("\rFF>");

  if (boostmode)
    change_baudrate(BAUDRATE_NORMAL);

  if (offset < length)
  {
    fprintf(stderr, "\r%s: premature end of file\n", filename);
    exit(1);
  }
  return start;
}

static void
init_serial_port(const char* portname)
{
  if (tcgetattr(portfd, &portattr) < 0)
    kc_exit_error(portname);

  portattr.c_iflag &= ~(BRKINT | IGNCR | ISTRIP | INLCR | ICRNL
                        | IXON | IXOFF | PARMRK);
  portattr.c_iflag |= INPCK | IGNBRK | IGNPAR;

  portattr.c_oflag &= ~(OPOST | OCRNL | OFILL);

  portattr.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
  portattr.c_cflag |= CREAD | CS8 | HUPCL | CLOCAL | CRTSCTS; /* not in POSIX */

  portattr.c_lflag &= ~(ICANON | IEXTEN | ISIG | ECHO | TOSTOP);

  portattr.c_cc[VMIN]  = 1;
  portattr.c_cc[VTIME] = 0;

  cfsetispeed(&portattr, BAUDRATE_NORMAL);
  cfsetospeed(&portattr, BAUDRATE_NORMAL);

  if (tcsetattr(portfd, TCSAFLUSH, &portattr) < 0 ||
      tcgetattr(portfd, &portattr) < 0)
    kc_exit_error(portname);

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
  const char*  portname   = "/dev/ttyS0";
  unsigned int loadoffset = 0;
  int          autostart  = 1;
  int          c;

  boostmode = 1;
  stdout_isterm = isatty(STDOUT_FILENO);

  setlocale(LC_ALL, "");

  while ((c = getopt(argc, argv, "p:o:ln?")) != -1)
    switch (c)
    {
      case 'p': portname   = optarg; break;
      case 'l': boostmode  = 0; break;
      case 'n': autostart  = 0; break;
      case 'o': loadoffset = (kc_parse_arg_int(optarg, -0xFFFF, 0xFFFF) + 0x10000) & 0xFFFFu; break;
      case '?': exit_usage();
      default:  abort();
    }

  if (optind >= argc)
    exit_usage();

  portfd = open(portname, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (portfd < 0)
    kc_exit_error(portname);

  init_serial_port(portname);

  int flags = fcntl(portfd, F_GETFL, 0);
  if (flags < 0 || fcntl(portfd, F_SETFL, flags & ~O_NONBLOCK) < 0)
    kc_exit_error(portname);

  if (stdout_isterm)
    printf("Using serial port %s\n", portname);

  if (boostmode)
  {
    send_sequence(v24escape, sizeof v24escape);
    send_sequence(v24boostcode, sizeof v24boostcode);
  }
  unsigned int start = 0xFFFF;

  for (int i = optind; i < argc; ++i)
    start = send_kcfile(argv[i], loadoffset);

  if (autostart && start < 0xFFFF)
  {
    const unsigned char v24exec[] = { 0x55, start & 0xFF, start >> 8 };

    send_sequence(v24escape, sizeof v24escape);
    send_sequence(v24exec,   sizeof v24exec);
  }
  if (close(portfd) < 0)
    kc_exit_error(portname);

  return 0;
}
