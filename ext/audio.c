#include <ruby.h>
#include "extconf.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

VALUE audio_clip_play(VALUE self)
{
  Mix_Chunk *sound = NULL;
  sound = Mix_LoadWAV("boom.wav");
  if(sound == NULL)
  {
    fprintf(stderr, "Unable to load WAV file: %s\n", Mix_GetError());
  }

  int channel;
  channel = Mix_PlayChannel(-1, sound, 0);

  return self;
}

VALUE audio_clip_initialize(VALUE self)
{
  return self;
}

void Init_audio()
{
  if (SDL_Init(SDL_INIT_AUDIO) != 0) { exit(1); }

  int audio_rate = 22050;
  Uint16 audio_format = AUDIO_S16SYS;
  int audio_channels = 2;
  int audio_buffers = 4096;

  if(Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) != 0) { exit(1); }

  VALUE mAudio = rb_define_module("Audio");
  VALUE cClip = rb_define_class_under(mAudio, "Clip", rb_cObject);
  rb_define_method(cClip, "initialize", audio_clip_initialize, 0);
  rb_define_method(cClip, "play", audio_clip_play, 0);
}
