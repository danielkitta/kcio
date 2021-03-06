/*
 * Copyright (c) 2008  Daniel Elstner <daniel.kitta@gmail.com>
 *
 * KC-Term is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * KC-Term is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <build/config.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <curses.h>
#include <libkc/libkc.h>

#define BAUDRATE_KEYBOARD B1200
#define N_ELEMENTS(array) (sizeof(array) / sizeof((array)[0]))

static const unsigned char keypad2kc[] =
{
  [KEY_BREAK     - KEY_MIN] = 0x03, [KEY_DOWN      - KEY_MIN] = 0x0A,
  [KEY_UP        - KEY_MIN] = 0x0B, [KEY_LEFT      - KEY_MIN] = 0x08,
  [KEY_RIGHT     - KEY_MIN] = 0x09, [KEY_HOME      - KEY_MIN] = 0x10,
  [KEY_BACKSPACE - KEY_MIN] = 0x01, [KEY_F(1)      - KEY_MIN] = 0xF1,
  [KEY_F(2)      - KEY_MIN] = 0xF2, [KEY_F(3)      - KEY_MIN] = 0xF3,
  [KEY_F(4)      - KEY_MIN] = 0xF4, [KEY_F(5)      - KEY_MIN] = 0xF5,
  [KEY_F(6)      - KEY_MIN] = 0xF6, [KEY_F(7)      - KEY_MIN] = 0xF7,
  [KEY_F(8)      - KEY_MIN] = 0xF8, [KEY_F(9)      - KEY_MIN] = 0xF9,
  [KEY_F(10)     - KEY_MIN] = 0xFA, [KEY_F(11)     - KEY_MIN] = 0xFB,
  [KEY_F(12)     - KEY_MIN] = 0xFC, [KEY_DL        - KEY_MIN] = 0x02,
  [KEY_DC        - KEY_MIN] = 0x1F, [KEY_IC        - KEY_MIN] = 0x1A,
  [KEY_CLEAR     - KEY_MIN] = 0x0C, [KEY_NPAGE     - KEY_MIN] = 0x12,
  [KEY_PPAGE     - KEY_MIN] = 0x11, [KEY_ENTER     - KEY_MIN] = 0x0D,
  [KEY_PRINT     - KEY_MIN] = 0x0F, [KEY_BEG       - KEY_MIN] = 0x19,
  [KEY_END       - KEY_MIN] = 0x18, [KEY_SDC       - KEY_MIN] = 0x02,
  [KEY_SHOME     - KEY_MIN] = 0x0C, [KEY_SIC       - KEY_MIN] = 0x14,
  [KEY_SLEFT     - KEY_MIN] = 0x19, [KEY_SRIGHT    - KEY_MIN] = 0x18
};

static WINDOW* win_header;
static WINDOW* win_prompt;
static WINDOW* win_input;
static WINDOW* win_output;

static void
exit_usage(void)
{
  fputs("Usage: kcterm [-p PORT]\n", stderr);
  exit(1);
}

static void
destroy_screen(void)
{
  if (win_output)
  {
    delwin(win_output);
    win_output = 0;
  }
  if (win_input)
  {
    delwin(win_input);
    win_input = 0;
  }
  if (win_prompt)
  {
    delwin(win_prompt);
    win_prompt = 0;
  }
  if (win_header)
  {
    delwin(win_header);
    win_header = 0;
  }
  endwin();
}

static void
init_screen(void)
{
  initscr();
  nonl();
  raw();
  noecho();
  intrflush(stdscr, FALSE);
  wnoutrefresh(stdscr);

  if (!(win_header = newwin(2, 0, 0, 0)) ||
      !(win_prompt = newwin(1, 2, 3, 0)) ||
      !(win_input  = newwin(1, 0, 3, 2)) ||
      !(win_output = newwin(0, 0, 5, 0)))
  {
    destroy_screen();
    fputs("Failed to configure terminal screen\n", stderr);
    exit(1);
  }

  keypad(win_input, TRUE);
  idlok(win_output, TRUE);
  scrollok(win_output, TRUE);
}

static void
exit_error(const char* where, int err_no)
{
  destroy_screen();

  const char* what = strerror(err_no);

  fprintf(stderr, "%s: %s\n", where, (what) ? what : "unknown error");
  exit(1);
}

static void
send_bytes(int portfd, const unsigned char* buf, int n, WINDOW* win)
{
  ssize_t written = 0;

  while (written < n)
  {
    ssize_t rc = write(portfd, &buf[written], n - written);
    if (rc >= 0)
      written += rc;
    else if (errno != EINTR)
      exit_error("send bytes", errno);
  }

  int xmax, ymax;
  getmaxyx(win, ymax, xmax);
  --xmax;

  if (n > xmax)
  {
    buf += n - xmax;
    n = xmax;
  }

  int x, y;
  getyx(win, y, x);

  int ndel = x + n - xmax;
  if (ndel > 0)
  {
    wmove(win, y, 0);
    for (int i = 0; i < ndel; ++i)
      wdelch(win);
    wmove(win, y, x - ndel);
  }

  for (int i = 0; i < n; ++i)
  {
    cchar_t cc;
    wchar_t wc[CCHARW_MAX];
    attr_t  attrs;
    short   color_pair;

    wgetbkgrnd(win, &cc);
    getcchar(&cc, wc, &attrs, &color_pair, 0);

    wc[0] = kc_to_wide_char(buf[i]);

    setcchar(&cc, wc, attrs, color_pair, 0);
    wadd_wch(win, &cc);
  }

  while (tcdrain(portfd) < 0)
  {
    if (errno != EINTR)
      exit_error("send bytes", errno);
  }
}

static void
init_serial_port(int portfd, const char* portname)
{
  struct termios portattr;

  if (tcgetattr(portfd, &portattr) < 0)
    exit_error(portname, errno);

  portattr.c_iflag &= ~(BRKINT | ISTRIP | INLCR | ICRNL | IXON | IXOFF);
  portattr.c_iflag |= INPCK | IGNBRK | IGNPAR | PARMRK | IGNCR;

  portattr.c_oflag &= ~(OPOST | OCRNL | OFILL);

  portattr.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
  portattr.c_cflag |= CLOCAL | CREAD | CS8 | HUPCL | CRTSCTS; /* not in POSIX */

  portattr.c_lflag &= ~(ICANON | IEXTEN | ISIG | ECHO | NOFLSH | TOSTOP);

  portattr.c_cc[VINTR] = 3;
  portattr.c_cc[VMIN]  = 1;
  portattr.c_cc[VTIME] = 0;

  cfsetispeed(&portattr, BAUDRATE_KEYBOARD);
  cfsetospeed(&portattr, BAUDRATE_KEYBOARD);

  if (tcsetattr(portfd, TCSAFLUSH, &portattr) < 0
      || tcgetattr(portfd, &portattr) < 0)
    exit_error(portname, errno);

  if ((portattr.c_cflag & (CSIZE | CSTOPB | PARENB | CRTSCTS)) != (CS8 | CRTSCTS)
      || cfgetispeed(&portattr) != BAUDRATE_KEYBOARD
      || cfgetospeed(&portattr) != BAUDRATE_KEYBOARD)
  {
    destroy_screen();
    fprintf(stderr, "%s: serial port configuration not supported\n", portname);
    exit(1);
  }
}

static void
receive_kctext(int portfd, WINDOW* win)
{
  unsigned char buf[64];
  ssize_t       count;

  while ((count = read(portfd, buf, sizeof buf)) < 0)
    switch (errno)
    {
      case EINTR:
        continue;
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
      case EWOULDBLOCK:
#endif
      case EAGAIN:
        return;
      default:
        exit_error("receive KC text", errno);
    }

  cchar_t cc;
  wchar_t wc[CCHARW_MAX];
  attr_t  attrs;
  short   color_pair;

  wgetbkgrnd(win, &cc);
  getcchar(&cc, wc, &attrs, &color_pair, 0);

  for (ssize_t i = 0; i < count; ++i)
  {
    unsigned int byte = buf[i];

    if (byte != 0x00 && byte != 0x03 && byte != 0x0D)
    {
      wc[0] = (byte == 0x0A) ? L'\n' : kc_to_wide_char(byte);

      setcchar(&cc, wc, attrs, color_pair, 0);
      wadd_wch(win, &cc);
    }
  }
  wnoutrefresh(win);
}

/*
 * Handle character input from the terminal.  Returns non-zero
 * if CTRL-D was pressed, meaning the application should exit.
 */
static int
handle_key_input(int portfd, WINDOW* win)
{
  static const unsigned char kctab[] = { 0x1B, 0x30 }; // ESC 0
  wint_t        wc = L'\0';
  unsigned char kc;

  switch (wget_wch(win, &wc))
  {
    case OK:
      if (wc == L'\4') // C-d: quit
        return 1;
      if (wc == L'\t')
        send_bytes(portfd, kctab, sizeof kctab, win);
      else if ((kc = kc_from_wide_char(wc)))
        send_bytes(portfd, &kc, 1, win);
      else
        flash();
      break;

    case KEY_CODE_YES:
      if (wc >= KEY_MIN && wc < KEY_MIN + N_ELEMENTS(keypad2kc)
          && keypad2kc[wc - KEY_MIN])
        send_bytes(portfd, &keypad2kc[wc - KEY_MIN], 1, win);
      break;

    default:
      break;
  }

  return 0;
}

static void
input_loop(int portfd)
{
  if (portfd >= FD_SETSIZE)
  {
    destroy_screen();
    fputs("File descriptor too large\n", stderr);
    exit(1);
  }

  for (;;)
  {
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(portfd, &fds);

    int rc = select(portfd + 1, &fds, 0, 0, 0);
    if (rc <= 0)
    {
      if (rc < 0 && errno != EINTR)
        exit_error("select", errno);
      continue;
    }

    if (FD_ISSET(portfd, &fds))
      receive_kctext(portfd, win_output);

    if (FD_ISSET(STDIN_FILENO, &fds)
        && handle_key_input(portfd, win_input))
      break;

    wnoutrefresh(win_input);
    doupdate();
  }
}

int
main(int argc, char** argv)
{
  const char* portname = "/dev/ttyS0";
  int         c;

  while ((c = getopt(argc, argv, "p:?")) != -1)
    switch (c)
    {
      case 'p': portname = optarg; break;
      case '?': exit_usage();
      default:  abort();
    }

  if (optind < argc)
    exit_usage();

  setlocale(LC_ALL, "");
  init_screen();

  int portfd = open(portname, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (portfd < 0)
    exit_error(portname, errno);

  init_serial_port(portfd, portname);

  int flags = fcntl(portfd, F_GETFL, 0);
  if (flags < 0 || fcntl(portfd, F_SETFL, flags & ~O_NONBLOCK) < 0)
    exit_error(portname, errno);

  wprintw(win_header, "Using serial port %s.\nPress CTRL-D to quit.", portname);
  waddch(win_prompt, '>');
  wnoutrefresh(win_header);
  wnoutrefresh(win_prompt);
  wnoutrefresh(win_input);
  doupdate();

  input_loop(portfd);

  while (close(portfd) < 0)
    if (errno != EINTR)
      exit_error(portname, errno);

  destroy_screen();
  return 0;
}
