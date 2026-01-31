#include <ruby.h>
#include <math.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#define MAX_SOUNDS 1024
#define MAX_CHANNELS 1024

static ma_engine engine;
static ma_sound *sounds[MAX_SOUNDS];
static ma_sound *channels[MAX_CHANNELS];
static int sound_count = 0;
static int engine_initialized = 0;

VALUE audio_load(VALUE self, VALUE file)
{
    const char *path = StringValueCStr(file);

    ma_sound *sound = (ma_sound *)malloc(sizeof(ma_sound));
    if (sound == NULL) {
        rb_raise(rb_eRuntimeError, "Failed to allocate memory for sound");
        return Qnil;
    }

    ma_result result = ma_sound_init_from_file(&engine, path, MA_SOUND_FLAG_DECODE, NULL, NULL, sound);
    if (result != MA_SUCCESS) {
        free(sound);
        rb_raise(rb_eRuntimeError, "Failed to load audio file: %s", path);
        return Qnil;
    }

    int id = sound_count;
    sounds[id] = sound;
    sound_count++;

    return rb_int2inum(id);
}

VALUE audio_play(VALUE self, VALUE channel_id, VALUE clip)
{
    int channel = NUM2INT(channel_id);
    int clip_id = NUM2INT(clip);

    if (clip_id < 0 || clip_id >= sound_count || sounds[clip_id] == NULL) {
        rb_raise(rb_eArgError, "Invalid clip ID: %d", clip_id);
        return Qnil;
    }

    if (channel < 0 || channel >= MAX_CHANNELS) {
        rb_raise(rb_eArgError, "Invalid channel ID: %d", channel);
        return Qnil;
    }

    // Clean up existing sound on this channel
    if (channels[channel] != NULL) {
        ma_sound_uninit(channels[channel]);
        free(channels[channel]);
        channels[channel] = NULL;
    }

    // Create a copy of the sound for playback
    ma_sound *playback = (ma_sound *)malloc(sizeof(ma_sound));
    if (playback == NULL) {
        rb_raise(rb_eRuntimeError, "Failed to allocate memory for playback");
        return Qnil;
    }

    ma_result result = ma_sound_init_copy(&engine, sounds[clip_id], 0, NULL, playback);
    if (result != MA_SUCCESS) {
        free(playback);
        rb_raise(rb_eRuntimeError, "Failed to create sound copy for playback");
        return Qnil;
    }

    channels[channel] = playback;
    ma_sound_start(playback);

    return rb_int2inum(channel);
}

VALUE audio_stop(VALUE self, VALUE channel_id)
{
    int channel = NUM2INT(channel_id);

    if (channel < 0 || channel >= MAX_CHANNELS || channels[channel] == NULL) {
        return Qnil;
    }

    ma_sound_stop(channels[channel]);
    ma_sound_seek_to_pcm_frame(channels[channel], 0);

    return Qnil;
}

VALUE audio_pause(VALUE self, VALUE channel_id)
{
    int channel = NUM2INT(channel_id);

    if (channel < 0 || channel >= MAX_CHANNELS || channels[channel] == NULL) {
        return Qnil;
    }

    ma_sound_stop(channels[channel]);

    return Qnil;
}

VALUE audio_resume(VALUE self, VALUE channel_id)
{
    int channel = NUM2INT(channel_id);

    if (channel < 0 || channel >= MAX_CHANNELS || channels[channel] == NULL) {
        return Qnil;
    }

    ma_sound_start(channels[channel]);

    return Qnil;
}

VALUE audio_set_volume(VALUE self, VALUE channel_id, VALUE volume)
{
    int channel = NUM2INT(channel_id);
    int vol = NUM2INT(volume);

    if (channel < 0 || channel >= MAX_CHANNELS || channels[channel] == NULL) {
        return Qnil;
    }

    // Convert from 0-128 range to 0.0-1.0
    float normalized_volume = vol / 128.0f;
    ma_sound_set_volume(channels[channel], normalized_volume);

    return Qnil;
}

VALUE audio_set_pos(VALUE self, VALUE channel_id, VALUE angle, VALUE distance)
{
    int channel = NUM2INT(channel_id);
    int ang = NUM2INT(angle);
    int dist = NUM2INT(distance);

    if (channel < 0 || channel >= MAX_CHANNELS || channels[channel] == NULL) {
        return Qnil;
    }

    // Convert polar coordinates (angle/distance) to cartesian (x/y/z)
    // SDL_mixer: angle 0=front, 90=right, 180=back, 270=left
    // distance: 0=close, 255=far
    float rad = ang * (MA_PI / 180.0f);
    float normalized_dist = dist / 255.0f;
    float x = normalized_dist * sinf(rad);
    float z = -normalized_dist * cosf(rad);

    ma_sound_set_position(channels[channel], x, 0.0f, z);

    return Qnil;
}

static void cleanup_audio(void)
{
    if (!engine_initialized) return;

    // Clean up channels
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i] != NULL) {
            ma_sound_uninit(channels[i]);
            free(channels[i]);
            channels[i] = NULL;
        }
    }

    // Clean up loaded sounds
    for (int i = 0; i < sound_count; i++) {
        if (sounds[i] != NULL) {
            ma_sound_uninit(sounds[i]);
            free(sounds[i]);
            sounds[i] = NULL;
        }
    }

    ma_engine_uninit(&engine);
    engine_initialized = 0;
}

void Init_audio()
{
    // Initialize arrays
    for (int i = 0; i < MAX_SOUNDS; i++) sounds[i] = NULL;
    for (int i = 0; i < MAX_CHANNELS; i++) channels[i] = NULL;

    // Initialize miniaudio engine with 3D audio support
    ma_engine_config config = ma_engine_config_init();
    config.listenerCount = 1;

    ma_result result = ma_engine_init(&config, &engine);
    if (result != MA_SUCCESS) {
        rb_raise(rb_eRuntimeError, "Failed to initialize audio engine");
        return;
    }

    engine_initialized = 1;

    // Register cleanup on exit
    atexit(cleanup_audio);

    // Define Ruby module and methods
    VALUE mAudio = rb_define_module("Audio");
    rb_define_singleton_method(mAudio, "load", audio_load, 1);
    rb_define_singleton_method(mAudio, "play", audio_play, 2);
    rb_define_singleton_method(mAudio, "set_pos", audio_set_pos, 3);
    rb_define_singleton_method(mAudio, "stop", audio_stop, 1);
    rb_define_singleton_method(mAudio, "pause", audio_pause, 1);
    rb_define_singleton_method(mAudio, "resume", audio_resume, 1);
    rb_define_singleton_method(mAudio, "set_volume", audio_set_volume, 2);
}
