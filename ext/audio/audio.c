#include <ruby.h>
#include "extconf.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdlib.h>
#include <stdio.h>

Mix_Chunk *sounds[1024];
int sound_count = 0;
int audio_initialized = 0;

VALUE audio_cleanup(VALUE self)
{
  (void)self;
  fprintf(stderr, "[native_audio] cleanup: start (initialized=%d, sound_count=%d)\n", audio_initialized, sound_count);
  fflush(stderr);

  if (!audio_initialized) {
    fprintf(stderr, "[native_audio] cleanup: skipped (not initialized)\n");
    fflush(stderr);
    return Qnil;
  }

  for (int i = 0; i < sound_count; i++) {
    if (sounds[i] != NULL) {
      fprintf(stderr, "[native_audio] cleanup: freeing sound %d\n", i);
      fflush(stderr);
      Mix_FreeChunk(sounds[i]);
      sounds[i] = NULL;
    }
  }
  sound_count = 0;

  fprintf(stderr, "[native_audio] cleanup: calling Mix_CloseAudio\n");
  fflush(stderr);
  Mix_CloseAudio();

  fprintf(stderr, "[native_audio] cleanup: calling Mix_Quit\n");
  fflush(stderr);
  Mix_Quit();

  fprintf(stderr, "[native_audio] cleanup: calling SDL_Quit\n");
  fflush(stderr);
  SDL_Quit();

  fprintf(stderr, "[native_audio] cleanup: done\n");
  fflush(stderr);
  audio_initialized = 0;
  return Qnil;
}

VALUE audio_init(VALUE self)
{
  const char* audio_driver_env = getenv("SDL_AUDIODRIVER");
  fprintf(stderr, "[native_audio] Audio.init: start (SDL_AUDIODRIVER=%s)\n", audio_driver_env ? audio_driver_env : "NOT SET");
  fflush(stderr);

  if (audio_initialized) {
    fprintf(stderr, "[native_audio] Audio.init: already initialized\n");
    fflush(stderr);
    return Qtrue;
  }

  // DEBUGGING: comment out SDL init to find crash point
  /*
  fprintf(stderr, "[native_audio] Audio.init: calling SDL_Init\n");
  fflush(stderr);

  if (SDL_Init(SDL_INIT_AUDIO) != 0) {
    fprintf(stderr, "[native_audio] Audio.init: SDL_Init FAILED: %s\n", SDL_GetError());
    fflush(stderr);
    rb_raise(rb_eRuntimeError, "Failed to initialize SDL audio: %s", SDL_GetError());
    return Qfalse;
  }

  fprintf(stderr, "[native_audio] Audio.init: SDL_Init OK\n");
  fflush(stderr);

  fprintf(stderr, "[native_audio] Audio.init: calling Mix_Init\n");
  fflush(stderr);

  int mix_flags = MIX_INIT_FLAC | MIX_INIT_OGG | MIX_INIT_MP3;
  int mix_initted = Mix_Init(mix_flags);
  if ((mix_initted & mix_flags) != mix_flags) {
    fprintf(stderr, "[native_audio] Audio.init: Mix_Init partial/failed: %s\n", Mix_GetError());
    fflush(stderr);
    // Continue anyway - not all formats may be available
  }

  fprintf(stderr, "[native_audio] Audio.init: Mix_Init OK\n");
  fflush(stderr);

  int audio_rate = 44100;
  Uint16 audio_format = MIX_DEFAULT_FORMAT;
  int audio_channels = 2;
  int audio_buffers = 4096;

  fprintf(stderr, "[native_audio] Audio.init: calling Mix_OpenAudio\n");
  fflush(stderr);

  if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) != 0) {
    fprintf(stderr, "[native_audio] Audio.init: Mix_OpenAudio FAILED: %s\n", Mix_GetError());
    fflush(stderr);
    Mix_Quit();
    SDL_Quit();
    rb_raise(rb_eRuntimeError, "Failed to open audio: %s", Mix_GetError());
    return Qfalse;
  }

  fprintf(stderr, "[native_audio] Audio.init: Mix_OpenAudio OK\n");
  fflush(stderr);

  audio_initialized = 1;
  */

  fprintf(stderr, "[native_audio] Audio.init: done (no-op for debugging)\n");
  fflush(stderr);

  return Qtrue;
}

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
  fprintf(stderr, "[native_audio] Init_audio: defining module\n");
  fflush(stderr);

  VALUE mAudio = rb_define_module("Audio");
  rb_define_singleton_method(mAudio, "init", audio_init, 0);
  rb_define_singleton_method(mAudio, "cleanup", audio_cleanup, 0);
  rb_define_singleton_method(mAudio, "load", audio_load, 1);
  rb_define_singleton_method(mAudio, "play", audio_play, 2);
  rb_define_singleton_method(mAudio, "set_pos", audio_set_pos, 3);
  rb_define_singleton_method(mAudio, "stop", audio_stop, 1);
  rb_define_singleton_method(mAudio, "pause", audio_pause, 1);
  rb_define_singleton_method(mAudio, "resume", audio_resume, 1);
  rb_define_singleton_method(mAudio, "set_volume", audio_set_volume, 2);

  fprintf(stderr, "[native_audio] Init_audio: done\n");
  fflush(stderr);
}
