/*
 * Copyright (c) Daniel Elstner 2008-2010 <daniel.kitta@gmail.com>
 *
 * KCPlay is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * KCPlay is distributed in the hope that it will be useful, but
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

enum
{
  SYNC_CYCLES = 160,
  PRE_CYCLES  = 1200 - SYNC_CYCLES
};

static snd_pcm_t*        audio      = 0;
static snd_output_t*     output     = 0;
static int16_t*          periodbuf  = 0;
static snd_pcm_uframes_t periodsize = 0;
static snd_pcm_uframes_t periodpos  = 0;
static unsigned int      samplerate = 48000;
static unsigned int      n_channels = 1;
static unsigned int      phase      = 0;
static unsigned int      basefreq   = 600;
static int               amplitude  = 23170; // 1 / sqrt(2)
static unsigned int      ratescale;
static int               stdout_isterm; // log progress on standard output?

static void
exit_usage(void)
{
  fputs("Usage: kcplay [-a VOLUME] [-d DEVICE] [-f FREQUENCY]"
        " [-r RATE] [-t FORMAT] [-v] FILE...\n", stderr);
  exit(optopt != 0);
}

static void
exit_file_error(FILE* stream, const char* filename)
{
  if (ferror(stream))
    perror(filename);
  else
    fprintf(stderr, "%s: premature end of file\n", filename);

  exit(1);
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

  if ((rc = snd_pcm_open(&audio, devname, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
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

  if ((rc = snd_pcm_sw_params_set_start_threshold(audio, swparams,
                                                  bufsize / periodsize * periodsize)) < 0)
    exit_snd_error(rc, "start threshold");

  if ((rc = snd_pcm_sw_params(audio, swparams)) < 0)
    exit_snd_error(rc, "applying software parameters");

  snd_pcm_sw_params_free(swparams);

  if ((rc = snd_pcm_prepare(audio)) < 0)
    exit_snd_error(rc, "preparing device");
}

static void
play_frame(int sample)
{
  for (unsigned int i = 0; i < n_channels; ++i)
    periodbuf[n_channels * periodpos + i] = sample;

  if (++periodpos == periodsize)
  {
    snd_pcm_uframes_t written = 0;

    while (written < periodsize)
    {
      snd_pcm_sframes_t rc = snd_pcm_writei(audio, &periodbuf[n_channels * written],
                                            periodsize - written);
      if (rc < 0)
        rc = snd_pcm_recover(audio, rc, 0);
      if (rc >= 0)
        written += rc;
      else if (rc != -EINTR && rc != -EAGAIN)
        exit_snd_error(rc, "writing sample data");
    }
    periodpos = 0;
  }
}

/* Approximate cos(pi * x / samplerate) in fixed point arithmetic using Taylor
 * expansion.  The argument must be within the range [0, samplerate / 2], or
 * otherwise the result is undefined.
 */
static int
approx_cosine(unsigned int x, unsigned int amp)
{
  enum
  {
    PREC = 15,
    ONE  = 1 << PREC
  };
  unsigned int phi  = (x * ratescale) >> (30 - PREC);
  unsigned int phi2 = (phi * phi) >> PREC;

  // Compute the eigth-degree Taylor polynomial for the cosine function:
  //              x^2   x^4   x^6   x^8
  // cos(x) = 1 - --- + --- - --- + ---
  //               2!    4!    6!    8!
  unsigned int icos = 2 * ONE - phi2 * (ONE - phi2 * (ONE - phi2 * (ONE - phi2
                      / 56) / (30 * ONE)) / (12 * ONE)) / ONE;
  return (amp * icos) >> (PREC + 1);
}

static void
play_bit(int t)
{
  unsigned int srate = samplerate;
  unsigned int phi   = phase;
  unsigned int step  = 8 * basefreq;
  int          amp   = amplitude;

  for (; phi < (srate << t) / 2; phi += step)
    play_frame(approx_cosine(phi >> t, amp));

  for (; phi < (srate << t); phi += step)
    play_frame(-approx_cosine(srate - (phi >> t), amp));

  phi -= srate << t;

  for (; phi < (srate << t) / 2; phi += step)
    play_frame(-approx_cosine(phi >> t, amp));

  for (; phi < (srate << t); phi += step)
    play_frame(approx_cosine(srate - (phi >> t), amp));

  phase = phi - (srate << t);
}

static void
play_ramp(int slope)
{
  unsigned int srate = samplerate;
  unsigned int phi   = phase;
  unsigned int step  = 8 * basefreq;
  int          amp   = amplitude;

  for (; phi < srate; phi += step)
    play_frame((amp + 1 - slope * approx_cosine(phi / 2, amp)) >> 1);

  for (; phi < 2 * srate; phi += step)
    play_frame((amp + 1 + slope * approx_cosine(srate - phi / 2, amp)) >> 1);

  phase = phi - 2 * srate;
}

static void
play_byte(unsigned int byte)
{
  for (int i = 0; i < 8; ++i)
  {
    play_bit(byte & BIT_1);
    byte >>= 1;
  }
  play_bit(BIT_T);
}

static void
play_block(unsigned int blocknr, const uint8_t* data)
{
  for (int i = 0; i < SYNC_CYCLES; ++i)
    play_bit(BIT_1);

  play_bit(BIT_T);
  play_byte(blocknr);

  unsigned int checksum = 0;

  for (int i = 0; i < 128; ++i)
  {
    unsigned int b = data[i];
    checksum += b;
    play_byte(b);
  }
  play_byte(checksum & 0xFF);

  if (stdout_isterm)
  {
    printf("\r%.2X>", blocknr);
    fflush(stdout);
  }
}

static void
play_kcfile(const char* filename, KCFileFormat format)
{
  FILE*        kcfile;
  unsigned int length  = UINT_MAX;
  unsigned int load    = UINT_MAX;
  unsigned int end     = UINT_MAX;
  unsigned int start   = UINT_MAX;
  int          blocknr = 1;
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
    kcfile = stdin;
  else
    if (!(kcfile = fopen(filename, "rb")))
      kc_exit_error(filename);

  switch (KC_BASE_FORMAT(format))
  {
    case KC_FORMAT_TAP:
    {
      if (fread(block, KC_TAP_MAGIC_LEN + 1, 1, kcfile) == 0)
        exit_file_error(kcfile, filename);

      if (memcmp(block, KC_TAP_MAGIC, KC_TAP_MAGIC_LEN) != 0)
      {
        fputs("TAP file ID not found\n", stderr);
        exit(1);
      }
      blocknr = block[KC_TAP_MAGIC_LEN];

      if (fread(block, sizeof block, 1, kcfile) == 0)
        exit_file_error(kcfile, filename);
      break;
    }
    case KC_FORMAT_KCC:
    {
      if (fread(block, sizeof block, 1, kcfile) == 0)
        exit_file_error(kcfile, filename);

      load = block[17] | (unsigned)block[18] << 8;
      end  = block[19] | (unsigned)block[20] << 8;

      int nargs = block[16];

      if (nargs >= 3)
        start = block[21] | (unsigned)block[22] << 8;

      if (nargs < 2 || nargs > 10 || load >= end)
      {
        fprintf(stderr, "%s: Invalid KCC start block\n", filename);
        exit(1);
      }
      nblocks = (128 + 127 + end - load) / 128;
      break;
    }
    case KC_FORMAT_SSS:
    {
      kc_filename_to_tape(format, filename, block);

      if (fread(&block[11], 2, 1, kcfile) == 0)
        exit_file_error(kcfile, filename);

      length  = block[11] | (unsigned)block[12] << 8;
      nblocks = (14 + 127 + length) / 128;

      if (fread(&block[13], MIN(length + 1, sizeof block - 13), 1, kcfile) == 0)
        exit_file_error(kcfile, filename);

      if (length < sizeof block - 14)
        memset(&block[14 + length], 0, sizeof block - 14 - length);
      break;
    }
    default:
      abort();
  }

  if (stdout_isterm)
  {
    for (int i = 0; i < 11; ++i)
      name[i] = kc_to_wide_char(block[i]);
    name[11] = L'\0';

    printf("%ls", name);

    if (load != UINT_MAX)
    {
      printf(" %.4X %.4X", load, end);

      if (start != UINT_MAX)
        printf(" %.4X", start);
    }
    putchar('\n');
  }

  play_ramp(1);

  // The initial lead-in sound is played for about 8000 oscillations according
  // to the original documentation.  That length is useful for seeking on tape,
  // but otherwise not required.  About one second (at 1200 Hz) is more than
  // enough.
  for (int i = 0; i < PRE_CYCLES; ++i)
    play_bit(BIT_1);

  play_block(blocknr, block);

  for (int i = 2; i <= nblocks; ++i)
  {
    if (KC_BASE_FORMAT(format) == KC_FORMAT_TAP)
    {
      blocknr = getc(kcfile);

      if (blocknr == EOF)
      {
        if (ferror(kcfile))
          kc_exit_error(filename);

        break; // end of TAP file
      }
    }
    else if (KC_BASE_FORMAT(format) == KC_FORMAT_KCC && i == nblocks)
      blocknr = 0xFF;
    else
      blocknr = i & 0xFF;

    size_t readsize = sizeof block;

    if (KC_BASE_FORMAT(format) == KC_FORMAT_SSS && i == nblocks)
    {
      readsize = length + sizeof block + 14 - nblocks * sizeof block;
      memset(&block[readsize], 0, sizeof block - readsize);
    }
    if (fread(block, readsize, 1, kcfile) == 0)
      exit_file_error(kcfile, filename);

    if (KC_BASE_FORMAT(format) == KC_FORMAT_TAP && blocknr == 1)
    {
      if (stdout_isterm)
      {
        for (int i = 0; i < 11; ++i)
          name[i] = kc_to_wide_char(block[i]);
        name[11] = L'\0';

        printf("\n%ls\n", name);
      }
      for (int i = 0; i < PRE_CYCLES; ++i)
        play_bit(BIT_1);
    }
    play_block(blocknr, block);
  }

  // For BASIC tape images (SSS), send the last block twice: first with the
  // normally incremented block number, and a second time with block number
  // FF.  This is how the original hardware behaves -- probably to maintain
  // compatibility with other home computers of the time.
  if (KC_BASE_FORMAT(format) == KC_FORMAT_SSS)
    play_block(0xFF, block);

  play_ramp(-1);

  if (stdout_isterm)
    putchar('\n');

  while (periodpos > 0)
    play_frame(0);

  if (kcfile != stdin && fclose(kcfile) != 0)
    kc_exit_error(filename);
}

int
main(int argc, char** argv)
{
  const char*  devname = "default";
  int          verbose = 0;
  KCFileFormat format  = KC_FORMAT_ANY;
  int          c, rc;

  while ((c = getopt(argc, argv, "a:d:f:r:t:v?")) != -1)
    switch (c)
    {
      case 'a': amplitude  = kc_parse_arg_num(optarg, 0.0, 1.0, INT16_MAX); break;
      case 'd': devname    = optarg; break;
      case 'f': basefreq   = kc_parse_arg_num(optarg, 1.0, 1 << 20, 1.0); break;
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

  init_audio(devname);

  if (verbose && (rc = snd_pcm_dump(audio, output)) < 0)
    exit_snd_error(rc, "dump setup");

  if (8 * basefreq > samplerate)
  {
    fprintf(stderr, "Base frequency of %u Hz is out of range at %u samples per second\n",
            basefreq, samplerate);
    exit(1);
  }
  ratescale = 3373259426u / samplerate; // 2^30 * pi / samplerate
  periodbuf = malloc(periodsize * n_channels * sizeof(int16_t));

  for (int i = optind; i < argc; ++i)
    play_kcfile(argv[i], format);

  free(periodbuf);

  if ((rc = snd_pcm_drain(audio)) < 0)
    exit_snd_error(rc, "drain");

  if ((rc = snd_pcm_close(audio)) < 0)
    exit_snd_error(rc, "close");

  return 0;
}
