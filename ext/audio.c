#include <ruby.h>
#include "extconf.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

Mix_Chunk *sounds[1024];
int sound_count = 0;

VALUE audio_play(VALUE self, VALUE channel_id, VALUE clip)
{
  Mix_Chunk *sound = sounds[NUM2INT(clip)];
  int channel = Mix_PlayChannel(NUM2INT(channel_id), sound, 0);
  return rb_int2inum(channel);
}

VALUE audio_load(VALUE self, VALUE file)
{
  Mix_Chunk *sound = NULL;
  sound = Mix_LoadWAV(StringValueCStr(file));
  if(sound == NULL)
  {
    fprintf(stderr, "Unable to load WAV file: %s\n", Mix_GetError());
  }

  sounds[0] = sound;
  sound_count++;

  return rb_int2inum(sound_count - 1);
}

VALUE audio_set_pos(VALUE self, VALUE channel_id, VALUE angle, VALUE distance)
{
  Mix_SetPosition(NUM2INT(channel_id), NUM2INT(angle), NUM2INT(distance));
  return Qnil;
}

VALUE audio_stop(VALUE self, VALUE channel_id)
{
  Mix_HaltChannel(NUM2INT(channel_id));
  return Qnil;
}

VALUE audio_pause(VALUE self, VALUE channel_id)
{
  Mix_Pause(NUM2INT(channel_id));
  return Qnil;
}

VALUE audio_resume(VALUE self, VALUE channel_id)
{
  Mix_Resume(NUM2INT(channel_id));
  return Qnil;
}

VALUE audio_set_volume(VALUE self, VALUE channel_id, VALUE volume)
{
  Mix_Volume(NUM2INT(channel_id), NUM2INT(volume));
  return Qnil;
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
  rb_define_singleton_method(mAudio, "load", audio_load, 1);
  rb_define_singleton_method(mAudio, "play", audio_play, 2);
  rb_define_singleton_method(mAudio, "set_pos", audio_set_pos, 3);
  rb_define_singleton_method(mAudio, "stop", audio_stop, 1);
  rb_define_singleton_method(mAudio, "pause", audio_pause, 1);
  rb_define_singleton_method(mAudio, "resume", audio_resume, 1);
  rb_define_singleton_method(mAudio, "set_volume", audio_set_volume, 2);
}
