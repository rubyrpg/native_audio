// ============================================================================
// reverb_node.h - Schroeder reverb node for native_audio
// ============================================================================

#ifndef REVERB_NODE_H
#define REVERB_NODE_H

#include "miniaudio.h"

// ============================================================================
// Constants
// ============================================================================

#define NUM_COMBS 4
#define NUM_ALLPASSES 2

// ============================================================================
// Types
// ============================================================================

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
    float room_size;
    ma_bool32 enabled;
} reverb_node;

// ============================================================================
// Public API
// ============================================================================

ma_result reverb_init(reverb_node *pNode, ma_node_graph *pNodeGraph,
                      ma_uint32 sampleRate, ma_uint32 numChannels);
void reverb_uninit(reverb_node *pNode);

void reverb_set_enabled(reverb_node *pNode, ma_bool32 enabled);
void reverb_set_room_size(reverb_node *pNode, float size);
void reverb_set_damping(reverb_node *pNode, float damp);
void reverb_set_wet(reverb_node *pNode, float wet);
void reverb_set_dry(reverb_node *pNode, float dry);

#endif // REVERB_NODE_H
