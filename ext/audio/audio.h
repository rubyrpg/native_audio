// ============================================================================
// audio.h - Shared types, constants, and globals for native_audio
// ============================================================================

#ifndef NATIVE_AUDIO_H
#define NATIVE_AUDIO_H

#include "miniaudio.h"
#include "delay_node.h"
#include "reverb_node.h"

// ============================================================================
// Constants
// ============================================================================

#define MAX_SOUNDS 1024
#define MAX_CHANNELS 1024

// ============================================================================
// Globals (defined in audio.c)
// ============================================================================

extern ma_engine engine;
extern ma_context context;
extern ma_sound *sounds[MAX_SOUNDS];
extern ma_sound *channels[MAX_CHANNELS];
extern multi_tap_delay_node *delay_nodes[MAX_CHANNELS];
extern reverb_node *reverb_nodes[MAX_CHANNELS];
extern int sound_count;
extern int engine_initialized;
extern int context_initialized;
extern int using_null_backend;

#endif // NATIVE_AUDIO_H
