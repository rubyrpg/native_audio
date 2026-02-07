// ============================================================================
// delay_node.c - Multi-tap delay node implementation
// ============================================================================

#include <stdlib.h>
#include <string.h>
#include "delay_node.h"

// ============================================================================
// DSP Callback
// ============================================================================

static void multi_tap_delay_process(ma_node *pNode, const float **ppFramesIn,
                                     ma_uint32 *pFrameCountIn, float **ppFramesOut,
                                     ma_uint32 *pFrameCountOut)
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

// ============================================================================
// Lifecycle
// ============================================================================

ma_result multi_tap_delay_init(multi_tap_delay_node *pNode, ma_node_graph *pNodeGraph,
                                ma_uint32 sampleRate, ma_uint32 numChannels)
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

void multi_tap_delay_uninit(multi_tap_delay_node *pNode)
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

// ============================================================================
// Tap Management
// ============================================================================

int multi_tap_delay_add_tap(multi_tap_delay_node *pNode, float time_ms, float volume)
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

void multi_tap_delay_remove_tap(multi_tap_delay_node *pNode, int tap_id)
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

void multi_tap_delay_set_volume(multi_tap_delay_node *pNode, int tap_id, float volume)
{
    if (pNode == NULL || tap_id < 0 || tap_id >= MAX_TAPS_PER_CHANNEL) {
        return;
    }

    if (pNode->taps[tap_id].active) {
        pNode->taps[tap_id].volume = volume;
    }
}

void multi_tap_delay_set_time(multi_tap_delay_node *pNode, int tap_id, float time_ms)
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
