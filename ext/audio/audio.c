// ============================================================================
// native_audio - Ruby audio library using miniaudio
// ============================================================================

#include <ruby.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// ============================================================================
// Constants & Globals
// ============================================================================

#define MAX_SOUNDS 1024
#define MAX_CHANNELS 1024

static ma_engine engine;
static ma_context context;
static ma_sound *sounds[MAX_SOUNDS];      // Loaded audio clips
static ma_sound *channels[MAX_CHANNELS];  // Playback instances
static int sound_count = 0;
static int engine_initialized = 0;
static int context_initialized = 0;
static int using_null_backend = 0;

// ============================================================================
// Cleanup (called on Ruby exit)
// ============================================================================

static void cleanup_audio(VALUE unused)
{
    (void)unused;

    if (!engine_initialized) {
        return;
    }

    // Stop and clean up all channels
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i] != NULL) {
            ma_sound_stop(channels[i]);
            ma_sound_uninit(channels[i]);
            free(channels[i]);
            channels[i] = NULL;
        }
    }

    // Stop and clean up all loaded sounds
    for (int i = 0; i < sound_count; i++) {
        if (sounds[i] != NULL) {
            ma_sound_stop(sounds[i]);
            ma_sound_uninit(sounds[i]);
            free(sounds[i]);
            sounds[i] = NULL;
        }
    }

    // On Windows, ma_engine_uninit crashes with the null backend
    // Since null backend has no real resources, we can skip cleanup
#ifdef _WIN32
    if (using_null_backend) {
        engine_initialized = 0;
        context_initialized = 0;
    } else {
#endif
        ma_engine_uninit(&engine);
        engine_initialized = 0;

        if (context_initialized) {
            ma_context_uninit(&context);
            context_initialized = 0;
        }
#ifdef _WIN32
    }
#endif
}


// ============================================================================
// Audio Loading
// ============================================================================

// Audio.load(path) - Load an audio file, returns clip ID
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

// Audio.duration(clip) - Get duration of clip in seconds
VALUE audio_duration(VALUE self, VALUE clip)
{
    int clip_id = NUM2INT(clip);

    if (clip_id < 0 || clip_id >= sound_count || sounds[clip_id] == NULL) {
        rb_raise(rb_eArgError, "Invalid clip ID: %d", clip_id);
        return Qnil;
    }

    float length;
    ma_result result = ma_sound_get_length_in_seconds(sounds[clip_id], &length);
    if (result != MA_SUCCESS) {
        return Qnil;
    }

    return rb_float_new(length);
}

// ============================================================================
// Playback Controls
// ============================================================================

// Audio.play(channel, clip) - Play a clip on a channel
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
        ma_sound_stop(channels[channel]);
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

// Audio.stop(channel) - Stop playback and rewind
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

// Audio.pause(channel) - Pause playback
VALUE audio_pause(VALUE self, VALUE channel_id)
{
    int channel = NUM2INT(channel_id);

    if (channel < 0 || channel >= MAX_CHANNELS || channels[channel] == NULL) {
        return Qnil;
    }

    ma_sound_stop(channels[channel]);

    return Qnil;
}

// Audio.resume(channel) - Resume playback
VALUE audio_resume(VALUE self, VALUE channel_id)
{
    int channel = NUM2INT(channel_id);

    if (channel < 0 || channel >= MAX_CHANNELS || channels[channel] == NULL) {
        return Qnil;
    }

    ma_sound_start(channels[channel]);

    return Qnil;
}

// ============================================================================
// Audio Effects
// ============================================================================

// Audio.set_volume(channel, volume) - Set volume (0-128)
VALUE audio_set_volume(VALUE self, VALUE channel_id, VALUE volume)
{
    int channel = NUM2INT(channel_id);
    int vol = NUM2INT(volume);

    if (channel < 0 || channel >= MAX_CHANNELS || channels[channel] == NULL) {
        return Qnil;
    }

    float normalized_volume = vol / 128.0f;
    ma_sound_set_volume(channels[channel], normalized_volume);

    return Qnil;
}

// Audio.set_pitch(channel, pitch) - Set pitch (1.0 = normal)
VALUE audio_set_pitch(VALUE self, VALUE channel_id, VALUE pitch)
{
    int channel = NUM2INT(channel_id);
    float p = (float)NUM2DBL(pitch);

    if (channel < 0 || channel >= MAX_CHANNELS || channels[channel] == NULL) {
        return Qnil;
    }

    ma_sound_set_pitch(channels[channel], p);

    return Qnil;
}

// Audio.set_pos(channel, angle, distance) - Set 3D position
// angle: 0=front, 90=right, 180=back, 270=left
// distance: 0=close, 255=far
VALUE audio_set_pos(VALUE self, VALUE channel_id, VALUE angle, VALUE distance)
{
    int channel = NUM2INT(channel_id);
    int ang = NUM2INT(angle);
    int dist = NUM2INT(distance);

    if (channel < 0 || channel >= MAX_CHANNELS || channels[channel] == NULL) {
        return Qnil;
    }

    // Convert polar to cartesian
    float rad = ang * (MA_PI / 180.0f);
    float normalized_dist = dist / 255.0f;
    float x = normalized_dist * sinf(rad);
    float z = -normalized_dist * cosf(rad);

    ma_sound_set_position(channels[channel], x, 0.0f, z);

    return Qnil;
}

// ============================================================================
// Ruby Module Setup
// ============================================================================

void Init_audio(void)
{
    for (int i = 0; i < MAX_SOUNDS; i++) sounds[i] = NULL;
    for (int i = 0; i < MAX_CHANNELS; i++) channels[i] = NULL;

    // Initialize audio engine
    // Check for null driver (for CI environments without audio devices)
    // Usage: NATIVE_AUDIO_DRIVER=null ruby script.rb
    const char *driver = getenv("NATIVE_AUDIO_DRIVER");
    int use_null = (driver != NULL && strcmp(driver, "null") == 0);

    ma_engine_config config = ma_engine_config_init();
    config.listenerCount = 1;

    if (use_null) {
        ma_backend backends[] = { ma_backend_null };
        ma_result ctx_result = ma_context_init(backends, 1, NULL, &context);
        if (ctx_result != MA_SUCCESS) {
            rb_raise(rb_eRuntimeError, "Failed to initialize null audio context");
            return;
        }
        context_initialized = 1;
        using_null_backend = 1;
        config.pContext = &context;
    }

    ma_result result = ma_engine_init(&config, &engine);

    if (result != MA_SUCCESS) {
        if (context_initialized) {
            ma_context_uninit(&context);
            context_initialized = 0;
        }
        rb_raise(rb_eRuntimeError, "Failed to initialize audio engine");
        return;
    }

    engine_initialized = 1;
    rb_set_end_proc(cleanup_audio, Qnil);

    // Define Ruby module
    VALUE mAudio = rb_define_module("Audio");

    // Loading
    rb_define_singleton_method(mAudio, "load", audio_load, 1);
    rb_define_singleton_method(mAudio, "duration", audio_duration, 1);

    // Playback
    rb_define_singleton_method(mAudio, "play", audio_play, 2);
    rb_define_singleton_method(mAudio, "stop", audio_stop, 1);
    rb_define_singleton_method(mAudio, "pause", audio_pause, 1);
    rb_define_singleton_method(mAudio, "resume", audio_resume, 1);

    // Effects
    rb_define_singleton_method(mAudio, "set_volume", audio_set_volume, 2);
    rb_define_singleton_method(mAudio, "set_pitch", audio_set_pitch, 2);
    rb_define_singleton_method(mAudio, "set_pos", audio_set_pos, 3);
}
