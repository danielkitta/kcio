/*
 * Copyright (c) 2008-2010  Daniel Elstner <daniel.kitta@gmail.com>
 *
 * KC-Rec is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * KC-Rec is distributed in the hope that it will be useful, but
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <libkc/libkc.h>

enum BitLength
{
  BIT_0 = 0, // 2400 Hz
  BIT_1 = 1, // 1200 Hz
  BIT_T = 2  //  600 Hz
};

static snd_pcm_t*        audio      = 0;
static snd_output_t*     output     = 0;
static int16_t*          periodbuf;
static snd_pcm_uframes_t periodsize = 0;
static snd_pcm_uframes_t periodpos;
static unsigned int      samplerate = 48000;
static unsigned int      channel    = 0;
static unsigned int      n_channels;
static unsigned int      sync_period;
static int               stdout_isterm; // log progress on standard output?

static void
exit_usage(void)
{
  fputs("Usage: kcrec [-c CHANNEL] [-d DEVICE] [-r RATE] [-t FORMAT] [-v] FILE...\n", stderr);
  exit(optopt != 0);
}

static void
exit_snd_error(int rc, const char* what)
{ 
  fprintf(stderr, "ALSA error (%s): %s\n", what, snd_strerror(rc));
  exit(1);
}

static void
init_audio(const char* devname)
{
  snd_pcm_hw_params_t* hwparams = 0;
  snd_pcm_sw_params_t* swparams = 0;
  snd_pcm_uframes_t    bufsize  = 0;
  unsigned int         buftime  = 1000000;
  unsigned int         pertime  = 50000;
  int                  dir      = 0;
  int                  rc;

  if ((rc = snd_output_stdio_attach(&output, stderr, 0)) < 0)
    exit_snd_error(rc, "log output");

  if ((rc = snd_pcm_open(&audio, devname, SND_PCM_STREAM_CAPTURE, 0)) < 0)
    exit_snd_error(rc, "opening device");

  if ((rc = snd_pcm_hw_params_malloc(&hwparams)) < 0)
    exit_snd_error(rc, "hardware parameters");

  if ((rc = snd_pcm_hw_params_any(audio, hwparams)) < 0)
    exit_snd_error(rc, "hardware parameters");

  if ((rc = snd_pcm_hw_params_set_rate_resample(audio, hwparams, 0)) < 0)
    exit_snd_error(rc, "hardware parameters");

  if ((rc = snd_pcm_hw_params_set_access(audio, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    exit_snd_error(rc, "access type");

  if ((rc = snd_pcm_hw_params_set_format(audio, hwparams, SND_PCM_FORMAT_S16)) < 0)
    exit_snd_error(rc, "sample format");

  if ((rc = snd_pcm_hw_params_set_channels_near(audio, hwparams, &n_channels)) < 0)
    exit_snd_error(rc, "number of channels");

  if ((rc = snd_pcm_hw_params_set_rate_near(audio, hwparams, &samplerate, &dir)) < 0)
    exit_snd_error(rc, "sample rate");

  if ((rc = snd_pcm_hw_params_set_buffer_time_near(audio, hwparams, &buftime, &dir)) < 0)
    exit_snd_error(rc, "buffer time");

  if ((rc = snd_pcm_hw_params_set_period_time_near(audio, hwparams, &pertime, &dir)) < 0)
    exit_snd_error(rc, "period time");

  if ((rc = snd_pcm_hw_params(audio, hwparams)) < 0)
    exit_snd_error(rc, "applying hardware parameters");

  if ((rc = snd_pcm_hw_params_get_buffer_size(hwparams, &bufsize)) < 0)
    exit_snd_error(rc, "buffer size");

  if ((rc = snd_pcm_hw_params_get_period_size(hwparams, &periodsize, &dir)) < 0)
    exit_snd_error(rc, "period size");

  snd_pcm_hw_params_free(hwparams);

  if ((rc = snd_pcm_sw_params_malloc(&swparams)) < 0)
    exit_snd_error(rc, "software parameters");

  if ((rc = snd_pcm_sw_params_current(audio, swparams)) < 0)
    exit_snd_error(rc, "software parameters");

  if ((rc = snd_pcm_sw_params_set_stop_threshold(audio, swparams,
                                                 bufsize / periodsize * periodsize)) < 0)
    exit_snd_error(rc, "stop threshold");

  if ((rc = snd_pcm_sw_params(audio, swparams)) < 0)
    exit_snd_error(rc, "applying software parameters");

  snd_pcm_sw_params_free(swparams);

  if ((rc = snd_pcm_prepare(audio)) < 0)
    exit_snd_error(rc, "preparing device");
}

static int
read_frame(void)
{
  if (periodpos >= periodsize)
  {
    snd_pcm_uframes_t nread = 0;

    while (nread < periodsize)
    {
      snd_pcm_sframes_t rc = snd_pcm_readi(audio, &periodbuf[n_channels * nread],
                                           periodsize - nread);
      if (rc < 0)
        rc = snd_pcm_recover(audio, rc, 0);
      if (rc >= 0)
        nread += rc;
      else if (rc != -EINTR && rc != -EAGAIN)
        exit_snd_error(rc, "reading sample data");
    }
    periodpos = 0;
  }
  return periodbuf[n_channels * (periodpos++) + channel];
}

static int
peek_last_frame(void)
{
  return periodbuf[n_channels * (periodpos - 1) + channel];
}

static unsigned int
wait_for_edge(void)
{
  unsigned int countdown = samplerate / 128 + 1;

  int32_t left;
  int32_t right = peek_last_frame();

  for (unsigned int i = 0; i < countdown; ++i)
  {
    left  = right;
    right = read_frame();

    // Note: assumes two's complement
    if ((left ^ right) < 0)
    {
      static unsigned int last_offset = 0;

      unsigned int offset = (((left + right) ^ right) < 0) ? 1 : 0;
      unsigned int delta  = 2 * i + 2 + offset - last_offset;

      last_offset = offset;
      return delta;
    }
  }
  return 2 * countdown; // countdown expired
}

static unsigned int
sync_block(void)
{
  enum { LEADIN_THRESHOLD = 2 * 24 };

  unsigned long min_period = samplerate / 8192;
  unsigned long max_period = samplerate / 256;

  unsigned long timer = 4ul * samplerate; // about 2 seconds
  unsigned long sum;
  unsigned long count;
  unsigned long period;

  for (count = 0, sum = 0;; ++count, sum += period)
  {
    period = wait_for_edge();

    // Virtual band-pass filter
    if (period > min_period && period < max_period)
    {
      unsigned long ex = count * period; // extrapolation

      // Period within +/-25% of the average?
      if (4 * ex >= 3 * sum && 4 * ex <= 5 * sum)
        continue;

      // Minimum duration passed and period within +/-33% of 2 * average?
      if (count > LEADIN_THRESHOLD && 3 * ex > 4 * sum && 3 * ex < 8 * sum)
      {
        period = wait_for_edge();
        unsigned long average = (sum + count / 2) / count;

        // Second half period within +/-25% of 2 * average?
        if (2 * period > 3 * average && 2 * period < 5 * average)
          return average;
      }
    }
    if (timer <= sum)
      return 0;

    timer -= sum;
    sum   = 0;
    count = 0;
  }
}

static unsigned int
record_bit(void)
{
  unsigned int norm  = sync_period;
  unsigned int first = wait_for_edge();

  if (3 * first > norm && 3 * first < 8 * norm)
  {
    unsigned int second = wait_for_edge();

    // The last oscillation at the end of every block is missing its second
    // half period: KC bug!  Thus, let overlong periods pass.
    if (3 * second > norm)
    {
      if (4 * second < 3 * norm) // below norm/2 +50%?
      {
        if (first < norm)
          return BIT_0;
      }
      else if (3 * second > 4 * norm) // above 2*norm - 33%?
      {
        if (first > norm)
          return BIT_T;
      }
      else
      {
        if (3 * first > 2 * norm && 3 * first < 5 * norm) // within norm -33%/+66%?
          return BIT_1;
      }
    }
  }
  fputs("Analog signal decoding error\n", stderr);
  exit(1);
}

static unsigned int
record_byte(void)
{
  unsigned int byte = 0;

  for (int i = 0; i < 8; ++i)
  {
    unsigned int bit = record_bit();

    if (bit > BIT_1)
    {
      fputs("Analog signal synchronization error\n", stderr);
      exit(1);
    }
    byte = (byte >> 1) | (bit << 7);
  }

  if (record_bit() != BIT_T)
  {
    fputs("Analog signal synchronization loss\n", stderr);
    exit(1);
  }
  return byte;
}

static int
record_block(unsigned char* data)
{
  sync_period = sync_block();

  if (sync_period == 0)
    return -1; // timeout

  int blocknr = record_byte();
  unsigned int checksum = 0;

  for (int i = 0; i < 128; ++i)
  {
    unsigned int byte = record_byte();
    data[i] = byte;
    checksum += byte;
  }
  if ((checksum & 0xFF) != record_byte())
  {
    fputs("Block checksum error\n", stderr);
    exit(1);
  }
  return blocknr;
}

static void
record_kcfile(const char* filename, KCFileFormat format)
{
  FILE*        kcfile;
  unsigned int length;
  int          blocknr;
  int          nblocks = INT_MAX / 128;
  wchar_t      name[12];
  uint8_t      block[128];

  if (format == KC_FORMAT_ANY)
  {
    format = kc_format_from_filename(filename);

    if (format == KC_FORMAT_ANY)
      format = KC_FORMAT_TAP;
  }

  if (filename[0] == '-' && filename[1] == '\0')
    kcfile = stdout;
  else
    if (!(kcfile = fopen(filename, "wb")))
      kc_exit_error(filename);

  if (KC_BASE_FORMAT(format) == KC_FORMAT_TAP)
  {
    // For TAP files, record blocks in whatever order they come in.
    do
      blocknr = record_block(block);
    while (blocknr < 0);

    if (stdout_isterm)
    {
      printf("\r%.2X>", blocknr);
      fflush(stdout);
    }
    if (fputs(KC_TAP_MAGIC, kcfile) < 0
        || putc(blocknr, kcfile) == EOF
        || fwrite(block, sizeof block, 1, kcfile) == 0)
      kc_exit_error(filename);
  }
  else
  {
    while ((blocknr = record_block(block)) != 1)
      if (blocknr >= 0 && stdout_isterm)
      {
        printf("%.2X*\n", blocknr);
        fflush(stdout);
      }

    if (stdout_isterm)
    {
      for (int i = 0; i < 11; ++i)
        name[i] = kc_to_wide_char(block[i]);
      name[11] = L'\0';

      printf("%ls", name);
    }

    if (KC_BASE_FORMAT(format) == KC_FORMAT_SSS)
    {
      unsigned int sig = block[0];

      if (((sig & 0xFB) == 0xD3 || (sig & 0xFE) == 0xD4) && block[1] == sig && block[2] == sig)
      {
        length  = block[11] | (unsigned)block[12] << 8;
        nblocks = (14 + 127 + length) / 128;
      }
      else
      {
        fputs("\nInvalid KC-BASIC start block\n", stderr);
        exit(1);
      }

      if (fwrite(&block[11], MIN(sizeof block - 11, 3 + length), 1, kcfile) == 0)
        kc_exit_error(filename);
    }
    else // KC_BASE_FORMAT(format) == KC_FORMAT_KCC
    {
      unsigned int load = block[17] | (unsigned)block[18] << 8;
      unsigned int end  = block[19] | (unsigned)block[20] << 8;

      if (stdout_isterm)
        printf(" %.4X %.4X", load, end);

      int nargs = block[16];

      if (nargs >= 3)
      {
        unsigned int start = block[21] | (unsigned)block[22] << 8;

        if (stdout_isterm)
          printf(" %.4X", start);
      }
      if (nargs < 2 || nargs > 10 || load >= end)
      {
        fputs("\nInvalid KCC start block\n", stderr);
        exit(1);
      }
      nblocks = (128 + 127 + end - load) / 128;

      if (fwrite(block, sizeof block, 1, kcfile) == 0)
        kc_exit_error(filename);
    }

    if (stdout_isterm)
      putchar('\n');
  }

  for (int i = 2; i <= nblocks; ++i)
  {
    blocknr = record_block(block);

    if (blocknr < 0)
      break;

    if (KC_BASE_FORMAT(format) != KC_FORMAT_TAP)
    {
      int expected = (KC_BASE_FORMAT(format) == KC_FORMAT_SSS || i < nblocks) ? i & 0xFF : 0xFF;

      if (blocknr != expected)
      {
        if (stdout_isterm)
          printf("\r%.2X*\n", blocknr);

        fputs("Block sequence error\n", stderr);
        exit(1);
      }
    }
    if (stdout_isterm)
    {
      printf("\r%.2X>", blocknr);
      fflush(stdout);
    }

    if (KC_BASE_FORMAT(format) == KC_FORMAT_TAP && putc(blocknr, kcfile) == EOF)
      kc_exit_error(filename);

    size_t writesize = sizeof block;

    if (KC_BASE_FORMAT(format) == KC_FORMAT_SSS && blocknr == nblocks)
      writesize = sizeof block + 14 + length - nblocks * sizeof block;

    if (fwrite(block, writesize, 1, kcfile) == 0)
      kc_exit_error(filename);
  }

  if (stdout_isterm)
    putchar('\n');

  if (kcfile != stdout && fclose(kcfile) != 0)
    kc_exit_error(filename);

  if (blocknr < 0 && KC_BASE_FORMAT(format) != KC_FORMAT_TAP)
  {
    fputs("Block sequence timeout\n", stderr);
    exit(1);
  }
}

int
main(int argc, char** argv)
{
  const char*  devname = "default";
  int          verbose = 0;
  KCFileFormat format  = KC_FORMAT_ANY;
  int          c, rc;

  while ((c = getopt(argc, argv, "c:d:r:t:v?")) != -1)
    switch (c)
    {
      case 'c': channel    = kc_parse_arg_int(optarg, 1, 256) - 1; break;
      case 'd': devname    = optarg; break;
      case 'r': samplerate = kc_parse_arg_num(optarg, 1.0, 1 << 24, 1.0); break;
      case 't': format     = kc_parse_arg_format(optarg); break;
      case 'v': verbose    = 1; break;
      case '?': exit_usage();
      default:  abort();
    }

  if (optind >= argc)
    exit_usage();

  setlocale(LC_ALL, "");
  stdout_isterm = isatty(STDOUT_FILENO);

  n_channels = channel + 1;
  init_audio(devname);

  if (channel >= n_channels)
  {
    fprintf(stderr, "Channel number %u out of range for stream with %u channels\n",
            channel + 1, n_channels);
    exit(1);
  }

  if (verbose && (rc = snd_pcm_dump(audio, output)) < 0)
    exit_snd_error(rc, "dump setup");

  periodbuf = calloc(periodsize * n_channels, sizeof(int16_t));
  periodpos = periodsize;

  for (int i = optind; i < argc; ++i)
    record_kcfile(argv[i], format);

  free(periodbuf);

  if ((rc = snd_pcm_drop(audio)) < 0)
    exit_snd_error(rc, "drop");

  if ((rc = snd_pcm_close(audio)) < 0)
    exit_snd_error(rc, "close");

  return 0;
}
