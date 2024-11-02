#include <ruby.h>
#include "extconf.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>



VALUE foobar_foo_initialize(VALUE self)
{
  rb_iv_set(self, "@bar", INT2FIX(42));
  return self;
}

VALUE foobar_foo_bar(VALUE self)
{
  return rb_iv_get(self, "@bar");
}

void Init_foobar()
{
  if (SDL_Init(SDL_INIT_AUDIO) != 0)
  {
      exit(1);
  }

  int audio_rate = 22050;
  Uint16 audio_format = AUDIO_S16SYS;
  int audio_channels = 2;
  int audio_buffers = 4096;

  if(Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) != 0)
  {
      exit(1);
  }

  Mix_Chunk *sound = NULL;

  sound = Mix_LoadWAV("boom.wav");

  if(sound == NULL)
  {
    fprintf(stderr, "Unable to load WAV file: %s\n", Mix_GetError());
  }


  int channel;

  channel = Mix_PlayChannel(-1, sound, 0);
  if(channel == -1)
  {
    exit(1);
  }

  while(Mix_Playing(channel) != 0);

  Mix_FreeChunk(sound);

  Mix_CloseAudio();
  SDL_Quit();


  VALUE mFoobar = rb_define_module("Foobar");
  VALUE cFoo = rb_define_class_under(mFoobar, "Foo", rb_cObject);
  rb_define_method(cFoo, "initialize", foobar_foo_initialize, 0);
  rb_define_method(cFoo, "bar", foobar_foo_bar, 0);
}
