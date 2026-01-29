#include <ruby.h>
#include "extconf.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

Mix_Chunk *sounds[1024];
int sound_count = 0;
int audio_initialized = 0;

VALUE audio_play(VALUE self, VALUE channel_id, VALUE clip)
{
  if (!audio_initialized) {
    rb_raise(rb_eRuntimeError, "Audio not initialized");
    return Qnil;
  }

  int clip_id = NUM2INT(clip);
  if (clip_id < 0 || clip_id >= sound_count) {
    rb_raise(rb_eArgError, "Invalid clip id: %d", clip_id);
    return Qnil;
  }

  Mix_Chunk *sound = sounds[clip_id];
  if (sound == NULL) {
    rb_raise(rb_eRuntimeError, "Sound at clip %d is NULL (failed to load?)", clip_id);
    return Qnil;
  }

  int channel = Mix_PlayChannel(NUM2INT(channel_id), sound, 0);
  return rb_int2inum(channel);
}

VALUE audio_load(VALUE self, VALUE file)
{
  if (!audio_initialized) {
    rb_raise(rb_eRuntimeError, "Audio not initialized");
    return Qnil;
  }

  if (sound_count >= 1024) {
    rb_raise(rb_eRuntimeError, "Maximum number of sounds (1024) reached");
    return Qnil;
  }

  Mix_Chunk *sound = NULL;
  sound = Mix_LoadWAV(StringValueCStr(file));
  if (sound == NULL) {
    rb_raise(rb_eRuntimeError, "Unable to load WAV file: %s", Mix_GetError());
    return Qnil;
  }

  int clip_id = sound_count;
  sounds[clip_id] = sound;
  sound_count++;

  return rb_int2inum(clip_id);
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
  VALUE mAudio = rb_define_module("Audio");
  rb_define_singleton_method(mAudio, "load", audio_load, 1);
  rb_define_singleton_method(mAudio, "play", audio_play, 2);
  rb_define_singleton_method(mAudio, "set_pos", audio_set_pos, 3);
  rb_define_singleton_method(mAudio, "stop", audio_stop, 1);
  rb_define_singleton_method(mAudio, "pause", audio_pause, 1);
  rb_define_singleton_method(mAudio, "resume", audio_resume, 1);
  rb_define_singleton_method(mAudio, "set_volume", audio_set_volume, 2);

  if (SDL_Init(SDL_INIT_AUDIO) != 0) {
    rb_raise(rb_eRuntimeError, "Failed to initialize SDL audio: %s", SDL_GetError());
    return;
  }

  int audio_rate = 22050;
  Uint16 audio_format = AUDIO_S16SYS;
  int audio_channels = 2;
  int audio_buffers = 4096;

  if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) != 0) {
    rb_raise(rb_eRuntimeError, "Failed to open audio: %s", Mix_GetError());
    return;
  }

  audio_initialized = 1;
}
