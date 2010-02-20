/*
 * Copyright (C) Daniel Elstner 2008 <daniel.kitta@gmail.com>
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
  BIT_0 = 1, /* 2400 Hz */
  BIT_1 = 2, /* 1200 Hz */
  BIT_T = 4  /*  600 Hz */
};

static snd_pcm_t*         audio       = 0;
static snd_output_t*      output      = 0;
static int16_t*           periodbuf   = 0;
static snd_pcm_uframes_t  periodsize  = 0;
static snd_pcm_uframes_t  periodpos   = 0;
static unsigned int       samplerate  = 48000;
static unsigned int       n_channels  = 1;
static unsigned int       phase       = 0;
static int                stdout_isterm;  /* log progress on standard output? */

static void
exit_usage(void)
{
  fprintf(stderr, "Usage: kcplay [-d DEVICE] [FILE]...\n");
  exit(optopt != 0);
}

static void
exit_error(const char* where)
{
  perror(where);
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
  snd_pcm_hw_params_t*  hwparams  = 0;
  snd_pcm_sw_params_t*  swparams  = 0;
  snd_pcm_uframes_t     bufsize   = 0;
  unsigned int          buftime   = 1000000;
  unsigned int          pertime   = 50000;
  int                   dir       = 0;
  int                   rc;

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

  if ((rc = snd_pcm_hw_params_get_buffer_size(hwparams, &bufsize)) < 0)
    exit_snd_error(rc, "buffer size");

  if ((rc = snd_pcm_hw_params_set_period_time_near(audio, hwparams, &pertime, &dir)) < 0)
    exit_snd_error(rc, "period time");

  if ((rc = snd_pcm_hw_params_get_period_size(hwparams, &periodsize, &dir)) < 0)
    exit_snd_error(rc, "period size");

  if ((rc = snd_pcm_hw_params(audio, hwparams)) < 0)
    exit_snd_error(rc, "applying hardware parameters");

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
#if 0
  if ((rc = snd_pcm_dump(audio, output)) < 0)
    exit_snd_error(rc, "dump setup");
#endif
}

static void
write_frame(int sample)
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

static void
write_bit(int t)
{
  for (; phase < t * samplerate; phase += 4800)
    write_frame(32000);
  phase -= t * samplerate;

  for (; phase < t * samplerate; phase += 4800)
    write_frame(-32000);
  phase -= t * samplerate;
}

static void
write_byte(unsigned int byte)
{
  for (int i = 0; i < 8; ++i)
  {
    write_bit(BIT_0 + (byte & 1));
    byte >>= 1;
  }
  write_bit(BIT_T);
}

static void
write_block(unsigned int blocknr, const uint8_t* data)
{
  for (int i = 0; i < 160; ++i)
    write_bit(BIT_1);

  write_bit(BIT_T);
  write_byte(blocknr);

  unsigned int checksum = 0;

  for (int i = 0; i < 128; ++i)
  {
    unsigned int b = data[i];
    checksum += b;
    write_byte(b);
  }
  write_byte(checksum & 0xFF);

  if (stdout_isterm)
  {
    printf("\r%.2X>", blocknr);
    fflush(stdout);
  }
}

static void
write_kcfile(const char* filename)
{
  FILE*         kcfile;
  unsigned int  load, end;
  int           nblocks;
  uint8_t       block[128];

  if (filename[0] == '-' && filename[1] == '\0')
    kcfile = stdin;
  else
    kcfile = fopen(filename, "rb");

  if (!kcfile || fread(&block, sizeof block, 1, kcfile) <= 0)
    exit_error(filename);

  unsigned int sig = block[0];

  if ((sig & 0xFB) == 0xD3 && block[1] == sig && block[2] == sig)
  {
    unsigned int length = block[11] | (unsigned)block[12] << 8;
    load = 0x0401;
    end  = 0x0401 + length;
    nblocks = (14 + 255 + length) / 128;
  }
  else
  {
    int nargs = block[16];
    load = block[17] | (unsigned)block[18] << 8;
    end  = block[19] | (unsigned)block[20] << 8;

    if (nargs < 2 || nargs > 10 || load >= end)
    {
      fprintf(stderr, "%s: invalid raw tape image header\n", filename);
      exit(1);
    }
    nblocks = (end - load + 255) / 128;
  }

  if (stdout_isterm)
  {
    wchar_t name[12];

    for (int i = 0; i < 11; ++i)
      name[i] = kc_to_wide_char(block[i]);
    name[11] = L'\0';

    printf("%ls %.4X %.4X\n", name, load, end);
  }

  /* The initial lead-in sound is played for about 8000 oscillations according
   * to the original documentation.  That length is useful for seeking on tape,
   * but otherwise not required.  800 oscillations are more than enough. */
  for (int i = 0; i < 640; ++i)
    write_bit(BIT_1);

  write_block(1, block);

  for (int i = 2; i <= nblocks; ++i)
  {
    if (fread(block, sizeof block, 1, kcfile) == 0)
    {
      fprintf(stderr, "%s: premature end of file\n", filename);
      exit(1);
    }
    write_block((i < nblocks) ? i & 0xFF : 0xFF, block);
  }

  if (stdout_isterm)
    putchar('\n');

  while (periodpos > 0)
    write_frame(0);

  if (kcfile != stdin && fclose(kcfile) != 0)
    exit_error(filename);
}

int
main(int argc, char** argv)
{
  const char* devname = "default";
  int         c, rc;

  while ((c = getopt(argc, argv, "d:?")) != -1)
    switch (c)
    {
      case 'd': devname = optarg; break;
      case '?': exit_usage();
      default:  abort();
    }

  if (optind >= argc)
    exit_usage();

  setlocale(LC_ALL, "");
  stdout_isterm = isatty(STDOUT_FILENO);

  init_audio(devname);
  periodbuf = malloc(periodsize * n_channels * sizeof(int16_t));

  for (int i = optind; i < argc; ++i)
    write_kcfile(argv[i]);

  free(periodbuf);

  if ((rc = snd_pcm_drain(audio)) < 0)
    exit_snd_error(rc, "drain");

  if ((rc = snd_pcm_close(audio)) < 0)
    exit_snd_error(rc, "close");

  return 0;
}
