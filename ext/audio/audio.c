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
#define MAX_TAPS_PER_CHANNEL 16
#define MAX_DELAY_SECONDS 2.0f

static ma_engine engine;
static ma_context context;
static ma_sound *sounds[MAX_SOUNDS];      // Loaded audio clips
static ma_sound *channels[MAX_CHANNELS];  // Playback instances
static int sound_count = 0;
static int engine_initialized = 0;
static int context_initialized = 0;
static int using_null_backend = 0;

// ============================================================================
// Multi-Tap Delay Node
// ============================================================================

typedef struct {
    ma_uint32 delay_frames;
    float volume;
    ma_bool32 active;
} delay_tap;

typedef struct {
    ma_node_base base;
    float *buffer;
    ma_uint32 buffer_size;      // Size in frames
    ma_uint32 write_pos;
    ma_uint32 channels;         // Audio channels (stereo = 2)
    delay_tap taps[MAX_TAPS_PER_CHANNEL];
    ma_uint32 tap_count;
    ma_uint32 sample_rate;
} multi_tap_delay_node;

static multi_tap_delay_node *delay_nodes[MAX_CHANNELS];

// DSP callback for the delay node
static void multi_tap_delay_process(ma_node *pNode, const float **ppFramesIn, ma_uint32 *pFrameCountIn,
                                     float **ppFramesOut, ma_uint32 *pFrameCountOut)
{
    multi_tap_delay_node *node = (multi_tap_delay_node *)pNode;
    const float *pFramesIn = ppFramesIn[0];
    float *pFramesOut = ppFramesOut[0];
    ma_uint32 frameCount = *pFrameCountOut;
    ma_uint32 numChannels = node->channels;

    for (ma_uint32 iFrame = 0; iFrame < frameCount; iFrame++) {
        for (ma_uint32 iChannel = 0; iChannel < numChannels; iChannel++) {
            ma_uint32 sampleIndex = iFrame * numChannels + iChannel;
            float inputSample = pFramesIn[sampleIndex];

            // Write to circular buffer
            ma_uint32 writeIndex = (node->write_pos * numChannels) + iChannel;
            node->buffer[writeIndex] = inputSample;

            // Start with dry signal
            float outputSample = inputSample;

            // Mix in all active taps
            for (ma_uint32 iTap = 0; iTap < MAX_TAPS_PER_CHANNEL; iTap++) {
                if (node->taps[iTap].active) {
                    ma_uint32 delayFrames = node->taps[iTap].delay_frames;
                    if (delayFrames > 0 && delayFrames <= node->buffer_size) {
                        ma_uint32 readPos = (node->write_pos + node->buffer_size - delayFrames) % node->buffer_size;
                        ma_uint32 readIndex = (readPos * numChannels) + iChannel;
                        outputSample += node->buffer[readIndex] * node->taps[iTap].volume;
                    }
                }
            }

            pFramesOut[sampleIndex] = outputSample;
        }

        // Advance write position
        node->write_pos = (node->write_pos + 1) % node->buffer_size;
    }
}

static ma_node_vtable g_multi_tap_delay_vtable = {
    multi_tap_delay_process,
    NULL,  // onGetRequiredInputFrameCount
    1,     // inputBusCount
    1,     // outputBusCount
    0      // flags
};

static ma_result multi_tap_delay_init(multi_tap_delay_node *pNode, ma_node_graph *pNodeGraph, ma_uint32 sampleRate, ma_uint32 numChannels)
{
    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    memset(pNode, 0, sizeof(*pNode));

    pNode->sample_rate = sampleRate;
    pNode->channels = numChannels;
    pNode->buffer_size = (ma_uint32)(sampleRate * MAX_DELAY_SECONDS);
    pNode->write_pos = 0;
    pNode->tap_count = 0;

    // Allocate circular buffer (frames * channels)
    pNode->buffer = (float *)calloc(pNode->buffer_size * numChannels, sizeof(float));
    if (pNode->buffer == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    // Initialize all taps as inactive
    for (int i = 0; i < MAX_TAPS_PER_CHANNEL; i++) {
        pNode->taps[i].active = MA_FALSE;
        pNode->taps[i].delay_frames = 0;
        pNode->taps[i].volume = 0.0f;
    }

    // Set up node configuration
    ma_uint32 channelsArray[1] = { numChannels };
    ma_node_config nodeConfig = ma_node_config_init();
    nodeConfig.vtable = &g_multi_tap_delay_vtable;
    nodeConfig.pInputChannels = channelsArray;
    nodeConfig.pOutputChannels = channelsArray;

    return ma_node_init(pNodeGraph, &nodeConfig, NULL, &pNode->base);
}

static void multi_tap_delay_uninit(multi_tap_delay_node *pNode)
{
    if (pNode == NULL) {
        return;
    }

    ma_node_uninit(&pNode->base, NULL);

    if (pNode->buffer != NULL) {
        free(pNode->buffer);
        pNode->buffer = NULL;
    }
}

static int multi_tap_delay_add_tap(multi_tap_delay_node *pNode, float time_ms, float volume)
{
    if (pNode == NULL) {
        return -1;
    }

    // Find first inactive tap slot
    for (int i = 0; i < MAX_TAPS_PER_CHANNEL; i++) {
        if (!pNode->taps[i].active) {
            ma_uint32 delayFrames = (ma_uint32)((time_ms / 1000.0f) * pNode->sample_rate);
            if (delayFrames > pNode->buffer_size) {
                delayFrames = pNode->buffer_size;
            }
            pNode->taps[i].delay_frames = delayFrames;
            pNode->taps[i].volume = volume;
            pNode->taps[i].active = MA_TRUE;
            pNode->tap_count++;
            return i;
        }
    }

    return -1;  // No slots available
}

static void multi_tap_delay_remove_tap(multi_tap_delay_node *pNode, int tap_id)
{
    if (pNode == NULL || tap_id < 0 || tap_id >= MAX_TAPS_PER_CHANNEL) {
        return;
    }

    if (pNode->taps[tap_id].active) {
        pNode->taps[tap_id].active = MA_FALSE;
        pNode->taps[tap_id].delay_frames = 0;
        pNode->taps[tap_id].volume = 0.0f;
        pNode->tap_count--;
    }
}

static void multi_tap_delay_set_volume(multi_tap_delay_node *pNode, int tap_id, float volume)
{
    if (pNode == NULL || tap_id < 0 || tap_id >= MAX_TAPS_PER_CHANNEL) {
        return;
    }

    if (pNode->taps[tap_id].active) {
        pNode->taps[tap_id].volume = volume;
    }
}

static void multi_tap_delay_set_time(multi_tap_delay_node *pNode, int tap_id, float time_ms)
{
    if (pNode == NULL || tap_id < 0 || tap_id >= MAX_TAPS_PER_CHANNEL) {
        return;
    }

    if (pNode->taps[tap_id].active) {
        ma_uint32 delayFrames = (ma_uint32)((time_ms / 1000.0f) * pNode->sample_rate);
        if (delayFrames > pNode->buffer_size) {
            delayFrames = pNode->buffer_size;
        }
        pNode->taps[tap_id].delay_frames = delayFrames;
    }
}

// ============================================================================
// Schroeder Reverb Node
// ============================================================================

#define NUM_COMBS 4
#define NUM_ALLPASSES 2

typedef struct {
    float *buffer;
    ma_uint32 size;
    ma_uint32 pos;
} delay_line;

typedef struct {
    ma_node_base base;
    ma_uint32 channels;
    ma_uint32 sample_rate;

    // 4 parallel comb filters per audio channel
    delay_line combs[2][NUM_COMBS];  // [audio_channel][comb_index]
    float comb_feedback;
    float comb_damp;
    float comb_damp_prev[2][NUM_COMBS];

    // 2 series allpass filters per audio channel
    delay_line allpasses[2][NUM_ALLPASSES];
    float allpass_feedback;

    // Mix control
    float wet;
    float dry;
    float room_size;  // Scales comb delays
    ma_bool32 enabled;
} reverb_node;

static reverb_node *reverb_nodes[MAX_CHANNELS];

// Base delay times in seconds (Schroeder-style, prime-ish ratios)
static const float COMB_DELAYS[NUM_COMBS] = { 0.0297f, 0.0371f, 0.0411f, 0.0437f };
static const float ALLPASS_DELAYS[NUM_ALLPASSES] = { 0.005f, 0.0017f };

static void delay_line_init(delay_line *dl, ma_uint32 size)
{
    dl->size = size;
    dl->pos = 0;
    dl->buffer = (float *)calloc(size, sizeof(float));
}

static void delay_line_free(delay_line *dl)
{
    if (dl->buffer) {
        free(dl->buffer);
        dl->buffer = NULL;
    }
}

static inline float delay_line_read(delay_line *dl)
{
    return dl->buffer[dl->pos];
}

static inline void delay_line_write(delay_line *dl, float value)
{
    dl->buffer[dl->pos] = value;
    dl->pos = (dl->pos + 1) % dl->size;
}

// Comb filter: output = buffer[pos], then write input + feedback * output (with damping)
static inline float comb_process(delay_line *dl, float input, float feedback, float damp, float *damp_prev)
{
    float output = delay_line_read(dl);
    // Low-pass filter on feedback for damping (high freq decay faster)
    *damp_prev = output * (1.0f - damp) + (*damp_prev) * damp;
    delay_line_write(dl, input + feedback * (*damp_prev));
    return output;
}

// Allpass filter: output = -g*input + buffer[pos], write input + g*buffer[pos]
static inline float allpass_process(delay_line *dl, float input, float feedback)
{
    float buffered = delay_line_read(dl);
    float output = buffered - feedback * input;
    delay_line_write(dl, input + feedback * buffered);
    return output;
}

static void reverb_process(ma_node *pNode, const float **ppFramesIn, ma_uint32 *pFrameCountIn,
                           float **ppFramesOut, ma_uint32 *pFrameCountOut)
{
    reverb_node *node = (reverb_node *)pNode;
    const float *pFramesIn = ppFramesIn[0];
    float *pFramesOut = ppFramesOut[0];
    ma_uint32 frameCount = *pFrameCountOut;
    ma_uint32 numChannels = node->channels;

    if (!node->enabled) {
        // Bypass: copy input to output
        memcpy(pFramesOut, pFramesIn, frameCount * numChannels * sizeof(float));
        return;
    }

    for (ma_uint32 iFrame = 0; iFrame < frameCount; iFrame++) {
        for (ma_uint32 iChannel = 0; iChannel < numChannels && iChannel < 2; iChannel++) {
            ma_uint32 sampleIndex = iFrame * numChannels + iChannel;
            float input = pFramesIn[sampleIndex];

            // Sum of parallel comb filters
            float combSum = 0.0f;
            for (int c = 0; c < NUM_COMBS; c++) {
                combSum += comb_process(&node->combs[iChannel][c], input,
                                        node->comb_feedback, node->comb_damp,
                                        &node->comb_damp_prev[iChannel][c]);
            }
            combSum *= 0.25f;  // Average the 4 combs

            // Series allpass filters
            float allpassOut = combSum;
            for (int a = 0; a < NUM_ALLPASSES; a++) {
                allpassOut = allpass_process(&node->allpasses[iChannel][a],
                                             allpassOut, node->allpass_feedback);
            }

            // Mix dry and wet
            pFramesOut[sampleIndex] = input * node->dry + allpassOut * node->wet;
        }

        // Handle mono->stereo or more channels by copying
        for (ma_uint32 iChannel = 2; iChannel < numChannels; iChannel++) {
            ma_uint32 sampleIndex = iFrame * numChannels + iChannel;
            pFramesOut[sampleIndex] = pFramesIn[sampleIndex];
        }
    }
}

static ma_node_vtable g_reverb_vtable = {
    reverb_process,
    NULL,
    1,
    1,
    0
};

static ma_result reverb_init(reverb_node *pNode, ma_node_graph *pNodeGraph,
                             ma_uint32 sampleRate, ma_uint32 numChannels)
{
    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    memset(pNode, 0, sizeof(*pNode));
    pNode->sample_rate = sampleRate;
    pNode->channels = numChannels;
    pNode->enabled = MA_FALSE;

    // Default parameters
    pNode->room_size = 0.5f;
    pNode->comb_feedback = 0.7f;
    pNode->comb_damp = 0.3f;
    pNode->allpass_feedback = 0.5f;
    pNode->wet = 0.3f;
    pNode->dry = 1.0f;

    // Initialize delay lines for up to 2 audio channels
    ma_uint32 chans = numChannels < 2 ? numChannels : 2;
    for (ma_uint32 ch = 0; ch < chans; ch++) {
        for (int c = 0; c < NUM_COMBS; c++) {
            ma_uint32 delaySize = (ma_uint32)(COMB_DELAYS[c] * pNode->room_size * 2.0f * sampleRate);
            if (delaySize < 1) delaySize = 1;
            delay_line_init(&pNode->combs[ch][c], delaySize);
        }
        for (int a = 0; a < NUM_ALLPASSES; a++) {
            ma_uint32 delaySize = (ma_uint32)(ALLPASS_DELAYS[a] * sampleRate);
            if (delaySize < 1) delaySize = 1;
            delay_line_init(&pNode->allpasses[ch][a], delaySize);
        }
    }

    // Set up node
    ma_uint32 channelsArray[1] = { numChannels };
    ma_node_config nodeConfig = ma_node_config_init();
    nodeConfig.vtable = &g_reverb_vtable;
    nodeConfig.pInputChannels = channelsArray;
    nodeConfig.pOutputChannels = channelsArray;

    return ma_node_init(pNodeGraph, &nodeConfig, NULL, &pNode->base);
}

static void reverb_uninit(reverb_node *pNode)
{
    if (pNode == NULL) return;

    ma_node_uninit(&pNode->base, NULL);

    for (int ch = 0; ch < 2; ch++) {
        for (int c = 0; c < NUM_COMBS; c++) {
            delay_line_free(&pNode->combs[ch][c]);
        }
        for (int a = 0; a < NUM_ALLPASSES; a++) {
            delay_line_free(&pNode->allpasses[ch][a]);
        }
    }
}

static void reverb_set_enabled(reverb_node *pNode, ma_bool32 enabled)
{
    if (pNode) pNode->enabled = enabled;
}

static void reverb_set_room_size(reverb_node *pNode, float size)
{
    if (pNode == NULL) return;
    pNode->room_size = size;
    // Note: changing room_size after init would require reallocating buffers
    // For now, this affects feedback calculation
    pNode->comb_feedback = 0.6f + size * 0.35f;  // 0.6 to 0.95
}

static void reverb_set_damping(reverb_node *pNode, float damp)
{
    if (pNode) pNode->comb_damp = damp;
}

static void reverb_set_wet(reverb_node *pNode, float wet)
{
    if (pNode) pNode->wet = wet;
}

static void reverb_set_dry(reverb_node *pNode, float dry)
{
    if (pNode) pNode->dry = dry;
}

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

        // Clean up delay nodes
        if (delay_nodes[i] != NULL) {
            multi_tap_delay_uninit(delay_nodes[i]);
            free(delay_nodes[i]);
            delay_nodes[i] = NULL;
        }

        // Clean up reverb nodes
        if (reverb_nodes[i] != NULL) {
            reverb_uninit(reverb_nodes[i]);
            free(reverb_nodes[i]);
            reverb_nodes[i] = NULL;
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

    // Clean up existing delay node on this channel
    if (delay_nodes[channel] != NULL) {
        multi_tap_delay_uninit(delay_nodes[channel]);
        free(delay_nodes[channel]);
        delay_nodes[channel] = NULL;
    }

    // Clean up existing reverb node on this channel
    if (reverb_nodes[channel] != NULL) {
        reverb_uninit(reverb_nodes[channel]);
        free(reverb_nodes[channel]);
        reverb_nodes[channel] = NULL;
    }

    // Create a copy of the sound for playback (without default attachment)
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

    // Create delay node for this channel
    multi_tap_delay_node *delayNode = (multi_tap_delay_node *)malloc(sizeof(multi_tap_delay_node));
    if (delayNode == NULL) {
        ma_sound_uninit(playback);
        free(playback);
        rb_raise(rb_eRuntimeError, "Failed to allocate memory for delay node");
        return Qnil;
    }

    ma_uint32 sampleRate = ma_engine_get_sample_rate(&engine);
    ma_uint32 numChannels = ma_engine_get_channels(&engine);

    result = multi_tap_delay_init(delayNode, ma_engine_get_node_graph(&engine), sampleRate, numChannels);
    if (result != MA_SUCCESS) {
        free(delayNode);
        ma_sound_uninit(playback);
        free(playback);
        rb_raise(rb_eRuntimeError, "Failed to initialize delay node");
        return Qnil;
    }

    // Create reverb node for this channel
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
    // ma_sound has ma_engine_node as first member for node API compatibility
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

// Audio.add_delay_tap(channel, time_ms, volume) - Add a delay tap, returns tap ID
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

// Audio.remove_delay_tap(channel, tap_id) - Remove a delay tap
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

// Audio.set_delay_tap_volume(channel, tap_id, volume) - Set tap volume
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

// Audio.set_delay_tap_time(channel, tap_id, time_ms) - Set tap delay time
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

// Audio.enable_reverb(channel, enabled) - Enable/disable reverb
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

// Audio.set_reverb_room_size(channel, size) - Set room size (0.0 to 1.0)
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

// Audio.set_reverb_damping(channel, damp) - Set damping (0.0 to 1.0)
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

// Audio.set_reverb_wet(channel, wet) - Set wet level (0.0 to 1.0)
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

// Audio.set_reverb_dry(channel, dry) - Set dry level (0.0 to 1.0)
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
// Engine Initialization
// ============================================================================

// Audio.init - Initialize the audio engine
VALUE audio_init(VALUE self)
{
    if (engine_initialized) {
        return Qnil;
    }

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
