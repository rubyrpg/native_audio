// ============================================================================
// reverb_node.c - Schroeder reverb node implementation
// ============================================================================

#include <stdlib.h>
#include <string.h>
#include "reverb_node.h"

// ============================================================================
// Delay Line Helpers
// ============================================================================

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

// ============================================================================
// Filter Processing
// ============================================================================

// Comb filter: output = buffer[pos], then write input + feedback * output (with damping)
static inline float comb_process(delay_line *dl, float input, float feedback,
                                  float damp, float *damp_prev)
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

// ============================================================================
// DSP Callback
// ============================================================================

static void reverb_process(ma_node *pNode, const float **ppFramesIn,
                           ma_uint32 *pFrameCountIn, float **ppFramesOut,
                           ma_uint32 *pFrameCountOut)
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

// ============================================================================
// Lifecycle
// ============================================================================

ma_result reverb_init(reverb_node *pNode, ma_node_graph *pNodeGraph,
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

void reverb_uninit(reverb_node *pNode)
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

// ============================================================================
// Parameter Control
// ============================================================================

void reverb_set_enabled(reverb_node *pNode, ma_bool32 enabled)
{
    if (pNode) pNode->enabled = enabled;
}

void reverb_set_room_size(reverb_node *pNode, float size)
{
    if (pNode == NULL) return;
    pNode->room_size = size;
    // Note: changing room_size after init would require reallocating buffers
    // For now, this affects feedback calculation
    pNode->comb_feedback = 0.6f + size * 0.35f;  // 0.6 to 0.95
}

void reverb_set_damping(reverb_node *pNode, float damp)
{
    if (pNode) pNode->comb_damp = damp;
}

void reverb_set_wet(reverb_node *pNode, float wet)
{
    if (pNode) pNode->wet = wet;
}

void reverb_set_dry(reverb_node *pNode, float dry)
{
    if (pNode) pNode->dry = dry;
}
