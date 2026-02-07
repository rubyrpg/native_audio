// ============================================================================
// audio.c - Main entry point and Ruby bindings for native_audio
// ============================================================================

#include <ruby.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audio.h"

// ============================================================================
// Global Definitions
// ============================================================================

ma_engine engine;
ma_context context;
ma_sound *sounds[MAX_SOUNDS];
ma_sound *channels[MAX_CHANNELS];
multi_tap_delay_node *delay_nodes[MAX_CHANNELS];
reverb_node *reverb_nodes[MAX_CHANNELS];
int sound_count = 0;
int engine_initialized = 0;
int context_initialized = 0;
int using_null_backend = 0;

// ============================================================================
// Cleanup (called on Ruby exit)
// ============================================================================

static void cleanup_audio(VALUE unused)
{
    (void)unused;

    if (!engine_initialized) {
        return;
    }

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i] != NULL) {
            ma_sound_stop(channels[i]);
            ma_sound_uninit(channels[i]);
            free(channels[i]);
            channels[i] = NULL;
        }

        if (delay_nodes[i] != NULL) {
            multi_tap_delay_uninit(delay_nodes[i]);
            free(delay_nodes[i]);
            delay_nodes[i] = NULL;
        }

        if (reverb_nodes[i] != NULL) {
            reverb_uninit(reverb_nodes[i]);
            free(reverb_nodes[i]);
            reverb_nodes[i] = NULL;
        }
    }

    for (int i = 0; i < sound_count; i++) {
        if (sounds[i] != NULL) {
            ma_sound_stop(sounds[i]);
            ma_sound_uninit(sounds[i]);
            free(sounds[i]);
            sounds[i] = NULL;
        }
    }

#ifdef _WIN32
    if (using_null_backend) {
        engine_initialized = 0;
        context_initialized = 0;
        return;
    }
#endif

    ma_engine_uninit(&engine);
    engine_initialized = 0;

    if (context_initialized) {
        ma_context_uninit(&context);
        context_initialized = 0;
    }
}

// ============================================================================
// Engine Initialization
// ============================================================================

VALUE audio_init(VALUE self)
{
    if (engine_initialized) {
        return Qnil;
    }

    const char *driver = getenv("NATIVE_AUDIO_DRIVER");
    int use_null = (driver != NULL && strcmp(driver, "null") == 0);

    ma_engine_config config = ma_engine_config_init();
    config.listenerCount = 1;

    if (use_null) {
        ma_backend backends[] = { ma_backend_null };
        ma_result ctx_result = ma_context_init(backends, 1, NULL, &context);
        if (ctx_result != MA_SUCCESS) {
            rb_raise(rb_eRuntimeError, "Failed to initialize null audio context");
            return Qnil;
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
        return Qnil;
    }

    engine_initialized = 1;
    rb_set_end_proc(cleanup_audio, Qnil);

    return Qnil;
}

// ============================================================================
// Audio Loading
// ============================================================================

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

    // Clean up existing resources on this channel
    if (channels[channel] != NULL) {
        ma_sound_stop(channels[channel]);
        ma_sound_uninit(channels[channel]);
        free(channels[channel]);
        channels[channel] = NULL;
    }

    if (delay_nodes[channel] != NULL) {
        multi_tap_delay_uninit(delay_nodes[channel]);
        free(delay_nodes[channel]);
        delay_nodes[channel] = NULL;
    }

    if (reverb_nodes[channel] != NULL) {
        reverb_uninit(reverb_nodes[channel]);
        free(reverb_nodes[channel]);
        reverb_nodes[channel] = NULL;
    }

    // Create sound copy for playback
    ma_sound *playback = (ma_sound *)malloc(sizeof(ma_sound));
    if (playback == NULL) {
        rb_raise(rb_eRuntimeError, "Failed to allocate memory for playback");
        return Qnil;
    }

    ma_result result = ma_sound_init_copy(&engine, sounds[clip_id], MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT, NULL, playback);
    if (result != MA_SUCCESS) {
        free(playback);
        rb_raise(rb_eRuntimeError, "Failed to create sound copy for playback");
        return Qnil;
    }

    // Create delay node
    ma_uint32 sampleRate = ma_engine_get_sample_rate(&engine);
    ma_uint32 numChannels = ma_engine_get_channels(&engine);

    multi_tap_delay_node *delayNode = (multi_tap_delay_node *)malloc(sizeof(multi_tap_delay_node));
    if (delayNode == NULL) {
        ma_sound_uninit(playback);
        free(playback);
        rb_raise(rb_eRuntimeError, "Failed to allocate memory for delay node");
        return Qnil;
    }

    result = multi_tap_delay_init(delayNode, ma_engine_get_node_graph(&engine), sampleRate, numChannels);
    if (result != MA_SUCCESS) {
        free(delayNode);
        ma_sound_uninit(playback);
        free(playback);
        rb_raise(rb_eRuntimeError, "Failed to initialize delay node");
        return Qnil;
    }

    // Create reverb node
    reverb_node *reverbNode = (reverb_node *)malloc(sizeof(reverb_node));
    if (reverbNode == NULL) {
        multi_tap_delay_uninit(delayNode);
        free(delayNode);
        ma_sound_uninit(playback);
        free(playback);
        rb_raise(rb_eRuntimeError, "Failed to allocate memory for reverb node");
        return Qnil;
    }

    result = reverb_init(reverbNode, ma_engine_get_node_graph(&engine), sampleRate, numChannels);
    if (result != MA_SUCCESS) {
        free(reverbNode);
        multi_tap_delay_uninit(delayNode);
        free(delayNode);
        ma_sound_uninit(playback);
        free(playback);
        rb_raise(rb_eRuntimeError, "Failed to initialize reverb node");
        return Qnil;
    }

    // Route: sound -> delay_node -> reverb_node -> endpoint
    ma_node *endpoint = ma_engine_get_endpoint(&engine);
    ma_node_attach_output_bus(&reverbNode->base, 0, endpoint, 0);
    ma_node_attach_output_bus(&delayNode->base, 0, &reverbNode->base, 0);
    ma_node_attach_output_bus((ma_node *)playback, 0, &delayNode->base, 0);

    delay_nodes[channel] = delayNode;
    reverb_nodes[channel] = reverbNode;
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

// ============================================================================
// Sound Effects
// ============================================================================

VALUE audio_set_volume(VALUE self, VALUE channel_id, VALUE volume)
{
    int channel = NUM2INT(channel_id);
    int vol = NUM2INT(volume);

    if (channel < 0 || channel >= MAX_CHANNELS || channels[channel] == NULL) {
        return Qnil;
    }

    ma_sound_set_volume(channels[channel], vol / 128.0f);

    return Qnil;
}

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

VALUE audio_set_pos(VALUE self, VALUE channel_id, VALUE angle, VALUE distance)
{
    int channel = NUM2INT(channel_id);
    int ang = NUM2INT(angle);
    int dist = NUM2INT(distance);

    if (channel < 0 || channel >= MAX_CHANNELS || channels[channel] == NULL) {
        return Qnil;
    }

    float rad = ang * (MA_PI / 180.0f);
    float normalized_dist = dist / 255.0f;
    float x = normalized_dist * sinf(rad);
    float z = -normalized_dist * cosf(rad);

    ma_sound_set_position(channels[channel], x, 0.0f, z);

    return Qnil;
}

VALUE audio_set_looping(VALUE self, VALUE channel_id, VALUE looping)
{
    int channel = NUM2INT(channel_id);
    ma_bool32 loop = RTEST(looping) ? MA_TRUE : MA_FALSE;

    if (channel < 0 || channel >= MAX_CHANNELS || channels[channel] == NULL) {
        return Qnil;
    }

    ma_sound_set_looping(channels[channel], loop);

    return Qnil;
}

// ============================================================================
// Delay Tap Controls
// ============================================================================

VALUE audio_add_delay_tap(VALUE self, VALUE channel_id, VALUE time_ms, VALUE volume)
{
    int channel = NUM2INT(channel_id);
    float ms = (float)NUM2DBL(time_ms);
    float vol = (float)NUM2DBL(volume);

    if (channel < 0 || channel >= MAX_CHANNELS || delay_nodes[channel] == NULL) {
        rb_raise(rb_eArgError, "Invalid channel or no delay node: %d", channel);
        return Qnil;
    }

    int tap_id = multi_tap_delay_add_tap(delay_nodes[channel], ms, vol);
    if (tap_id < 0) {
        rb_raise(rb_eRuntimeError, "Failed to add delay tap (max taps reached)");
        return Qnil;
    }

    return rb_int2inum(tap_id);
}

VALUE audio_remove_delay_tap(VALUE self, VALUE channel_id, VALUE tap_id)
{
    int channel = NUM2INT(channel_id);
    int tap = NUM2INT(tap_id);

    if (channel < 0 || channel >= MAX_CHANNELS || delay_nodes[channel] == NULL) {
        return Qnil;
    }

    multi_tap_delay_remove_tap(delay_nodes[channel], tap);

    return Qnil;
}

VALUE audio_set_delay_tap_volume(VALUE self, VALUE channel_id, VALUE tap_id, VALUE volume)
{
    int channel = NUM2INT(channel_id);
    int tap = NUM2INT(tap_id);
    float vol = (float)NUM2DBL(volume);

    if (channel < 0 || channel >= MAX_CHANNELS || delay_nodes[channel] == NULL) {
        return Qnil;
    }

    multi_tap_delay_set_volume(delay_nodes[channel], tap, vol);

    return Qnil;
}

VALUE audio_set_delay_tap_time(VALUE self, VALUE channel_id, VALUE tap_id, VALUE time_ms)
{
    int channel = NUM2INT(channel_id);
    int tap = NUM2INT(tap_id);
    float ms = (float)NUM2DBL(time_ms);

    if (channel < 0 || channel >= MAX_CHANNELS || delay_nodes[channel] == NULL) {
        return Qnil;
    }

    multi_tap_delay_set_time(delay_nodes[channel], tap, ms);

    return Qnil;
}

// ============================================================================
// Reverb Controls
// ============================================================================

VALUE audio_enable_reverb(VALUE self, VALUE channel_id, VALUE enabled)
{
    int channel = NUM2INT(channel_id);
    ma_bool32 en = RTEST(enabled) ? MA_TRUE : MA_FALSE;

    if (channel < 0 || channel >= MAX_CHANNELS || reverb_nodes[channel] == NULL) {
        return Qnil;
    }

    reverb_set_enabled(reverb_nodes[channel], en);
    return Qnil;
}

VALUE audio_set_reverb_room_size(VALUE self, VALUE channel_id, VALUE size)
{
    int channel = NUM2INT(channel_id);
    float s = (float)NUM2DBL(size);

    if (channel < 0 || channel >= MAX_CHANNELS || reverb_nodes[channel] == NULL) {
        return Qnil;
    }

    reverb_set_room_size(reverb_nodes[channel], s);
    return Qnil;
}

VALUE audio_set_reverb_damping(VALUE self, VALUE channel_id, VALUE damp)
{
    int channel = NUM2INT(channel_id);
    float d = (float)NUM2DBL(damp);

    if (channel < 0 || channel >= MAX_CHANNELS || reverb_nodes[channel] == NULL) {
        return Qnil;
    }

    reverb_set_damping(reverb_nodes[channel], d);
    return Qnil;
}

VALUE audio_set_reverb_wet(VALUE self, VALUE channel_id, VALUE wet)
{
    int channel = NUM2INT(channel_id);
    float w = (float)NUM2DBL(wet);

    if (channel < 0 || channel >= MAX_CHANNELS || reverb_nodes[channel] == NULL) {
        return Qnil;
    }

    reverb_set_wet(reverb_nodes[channel], w);
    return Qnil;
}

VALUE audio_set_reverb_dry(VALUE self, VALUE channel_id, VALUE dry)
{
    int channel = NUM2INT(channel_id);
    float d = (float)NUM2DBL(dry);

    if (channel < 0 || channel >= MAX_CHANNELS || reverb_nodes[channel] == NULL) {
        return Qnil;
    }

    reverb_set_dry(reverb_nodes[channel], d);
    return Qnil;
}

// ============================================================================
// Ruby Module Setup
// ============================================================================

void Init_audio(void)
{
    for (int i = 0; i < MAX_SOUNDS; i++) sounds[i] = NULL;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        channels[i] = NULL;
        delay_nodes[i] = NULL;
        reverb_nodes[i] = NULL;
    }

    VALUE mAudio = rb_define_module("Audio");

    // Initialization
    rb_define_singleton_method(mAudio, "init", audio_init, 0);

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
    rb_define_singleton_method(mAudio, "set_looping", audio_set_looping, 2);

    // Delay taps
    rb_define_singleton_method(mAudio, "add_delay_tap", audio_add_delay_tap, 3);
    rb_define_singleton_method(mAudio, "remove_delay_tap", audio_remove_delay_tap, 2);
    rb_define_singleton_method(mAudio, "set_delay_tap_volume", audio_set_delay_tap_volume, 3);
    rb_define_singleton_method(mAudio, "set_delay_tap_time", audio_set_delay_tap_time, 3);

    // Reverb
    rb_define_singleton_method(mAudio, "enable_reverb", audio_enable_reverb, 2);
    rb_define_singleton_method(mAudio, "set_reverb_room_size", audio_set_reverb_room_size, 2);
    rb_define_singleton_method(mAudio, "set_reverb_damping", audio_set_reverb_damping, 2);
    rb_define_singleton_method(mAudio, "set_reverb_wet", audio_set_reverb_wet, 2);
    rb_define_singleton_method(mAudio, "set_reverb_dry", audio_set_reverb_dry, 2);
}
