// ============================================================================
// delay_node.h - Multi-tap delay node for native_audio
// ============================================================================

#ifndef DELAY_NODE_H
#define DELAY_NODE_H

#include "miniaudio.h"

// ============================================================================
// Constants
// ============================================================================

#define MAX_TAPS_PER_CHANNEL 16
#define MAX_DELAY_SECONDS 2.0f

// ============================================================================
// Types
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

// ============================================================================
// Public API
// ============================================================================

ma_result multi_tap_delay_init(multi_tap_delay_node *pNode, ma_node_graph *pNodeGraph,
                                ma_uint32 sampleRate, ma_uint32 numChannels);
void multi_tap_delay_uninit(multi_tap_delay_node *pNode);

int multi_tap_delay_add_tap(multi_tap_delay_node *pNode, float time_ms, float volume);
void multi_tap_delay_remove_tap(multi_tap_delay_node *pNode, int tap_id);
void multi_tap_delay_set_volume(multi_tap_delay_node *pNode, int tap_id, float volume);
void multi_tap_delay_set_time(multi_tap_delay_node *pNode, int tap_id, float time_ms);

#endif // DELAY_NODE_H
