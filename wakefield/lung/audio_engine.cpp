/**
 * @file audio_engine.cpp
 * @brief Audio engine implementation - core sample playback and control processing
 * 
 * This file implements the main audio engine functionality including:
 * - Sample binding and playback initialization
 * - Transport control (play/pause/arm/disarm)
 * - Pitch control with octave switching and fine tuning
 * - Loop parameter mapping from ADC inputs
 * - Audio engine state management
 * 
 * The audio engine uses a phase accumulator approach where a Q32.32 fixed-point
 * value advances through the sample data at a rate determined by the pitch control.
 * This provides smooth, glitch-free playback with sub-sample precision.
 * 
 * @author Brian Varren
 * @version 1.0
 * @date 2024
 */

#include <stdint.h>
#include <math.h>
#include <string.h>
#include "ADCless.h"
#include "adc_filter.h"
#include "audio_engine.h"
#include "pico_interp.h"
#include "sf_globals_bridge.h"
#include "config_pins.h"
#include <hardware/pwm.h>
#include <hardware/gpio.h>
#include <Arduino.h>  // Add for Serial

// Forward declaration from audio_engine_render.cpp
void ae_render_block(const int16_t* samples,
                     uint32_t total_samples,
                     ae_state_t engine_state,
                     volatile uint64_t* io_phase_q32_32);

// Debug moved to audio_engine_debug.cpp

// // Publish the loop + transport state for the display
// static inline void publish_display_state(uint16_t start_q12, uint16_t len_q12, uint32_t playhead, uint32_t total) {
//     disp_write_begin();
//     g_disp.loop_start_q12 = start_q12;
//     g_disp.loop_len_q12   = len_q12;
//     g_disp.playhead       = playhead;
//     g_disp.total          = total;
//     disp_write_end();
// }

// audio_engine_get_last_snapshot moved to audio_engine_debug.cpp

// ── Debug helper functions ──────────────────────────────────────────────────
// dbg_print_snapshot moved to audio_engine_debug.cpp

// audio_engine_debug_poll moved to audio_engine_debug.cpp

// ── Transport/direction state ──────────────────────────────────────────────
// These variables control the audio engine's playback state and direction
static volatile ae_state_t s_state = AE_STATE_IDLE;  // Current playback state
static volatile ae_mode_t  s_mode  = AE_MODE_FORWARD; // Playback direction mode
static int8_t              s_dir   = +1;     // +1 forward, -1 reverse (used for REVERSE/ALTERNATE modes)

// ── Mode Switch State ──────────────────────────────────────────────────────
// SPDT switch state tracking for playback mode control
static bool s_mode_switch_last_fwd = false;  // Previous forward switch state
static bool s_mode_switch_last_rev = false;  // Previous reverse switch state

// // Live playhead for UI (0..65535). 16-bit write/read is atomic on RP2-class MCUs.
// volatile uint16_t g_playhead_norm_u16 = 0;

void audio_engine_set_mode(ae_mode_t m) {
    s_mode = m;
    // sync s_dir with mode
    if (m == AE_MODE_FORWARD)  s_dir = +1;
    if (m == AE_MODE_REVERSE)  s_dir = -1;
    // AE_MODE_ALTERNATE keeps current s_dir until it hits a boundary
}

// ── Mode Switch Functions ───────────────────────────────────────────────────

/**
 * @brief Initialize GPIO pins for playback mode switch
 * 
 * Sets up GPIO16 and GPIO17 as inputs with pull-up resistors for detecting
 * the SPDT switch position. The switch connects one pin to ground when active.
 */
void audio_engine_mode_switch_init(void) {
    gpio_init(MODE_SWITCH_FWD_PIN);
    gpio_set_dir(MODE_SWITCH_FWD_PIN, GPIO_IN);
    gpio_pull_up(MODE_SWITCH_FWD_PIN);
    
    gpio_init(MODE_SWITCH_REV_PIN);
    gpio_set_dir(MODE_SWITCH_REV_PIN, GPIO_IN);
    gpio_pull_up(MODE_SWITCH_REV_PIN);
    
    // Initialize state tracking
    s_mode_switch_last_fwd = !gpio_get(MODE_SWITCH_FWD_PIN);  // Active low
    s_mode_switch_last_rev = !gpio_get(MODE_SWITCH_REV_PIN);  // Active low
    
    // Check initial switch position and set mode accordingly
    if (s_mode_switch_last_fwd && !s_mode_switch_last_rev) {
        audio_engine_set_mode(AE_MODE_FORWARD);
    } else if (!s_mode_switch_last_fwd && s_mode_switch_last_rev) {
        audio_engine_set_mode(AE_MODE_REVERSE);
    } else {
        // Both high or both low - default to forward
        audio_engine_set_mode(AE_MODE_FORWARD);
    }
    
    Serial.println(F("[AE] Mode switch initialized on GPIO16/17"));
}

/**
 * @brief Poll for mode switch changes and update playback mode
 * 
 * This function should be called from the main loop to detect switch position
 * changes and update the playback mode accordingly. Supports forward, reverse,
 * and ping-pong (alternate) modes.
 */
void audio_engine_mode_switch_poll(void) {
    bool current_fwd = !gpio_get(MODE_SWITCH_FWD_PIN);  // Active low
    bool current_rev = !gpio_get(MODE_SWITCH_REV_PIN);  // Active low
    
    // Detect switch position changes
    if (current_fwd != s_mode_switch_last_fwd || current_rev != s_mode_switch_last_rev) {
        if (current_fwd && !s_mode_switch_last_fwd) {
            // Forward switch just activated
            audio_engine_set_mode(AE_MODE_FORWARD);
            Serial.println(F("[AE] Mode switch: FORWARD"));
        } else if (current_rev && !s_mode_switch_last_rev) {
            // Reverse switch just activated
            audio_engine_set_mode(AE_MODE_REVERSE);
            Serial.println(F("[AE] Mode switch: REVERSE"));
        } else if (!current_fwd && !current_rev) {
            // Both switches inactive - default to forward
            audio_engine_set_mode(AE_MODE_FORWARD);
            Serial.println(F("[AE] Mode switch: FORWARD (default)"));
        }
        
        s_mode_switch_last_fwd = current_fwd;
        s_mode_switch_last_rev = current_rev;
    }
}


void audio_engine_arm(bool armed) {
    if (!g_phase_q32_32) { /* optional reset */ }
    s_state = armed ? AE_STATE_READY : AE_STATE_IDLE;
}

void audio_engine_play(bool play) {
    if (s_state == AE_STATE_IDLE) return;    // need a buffer first
    s_state = play ? AE_STATE_PLAYING : AE_STATE_PAUSED;
}

ae_state_t audio_engine_get_state(void){ return s_state; }
ae_mode_t  audio_engine_get_mode(void){  return s_mode;  }



// ── Phasor state ───────────────────────────────────────────────────────────
// The phase accumulator is the heart of the audio engine. It's a Q32.32 fixed-point
// value that represents the current position in the sample data with sub-sample precision.
// 
// Q32.32 format: 32 bits for integer sample index + 32 bits for fractional position
// This prevents phase overflow even with very large samples and provides smooth
// interpolation between samples for pitch shifting.
volatile uint64_t g_phase_q32_32 = 0;

// ── Pitch base ─────────────────────────────────────────────────────────────
// Base increment in Q32.32 format. This determines how fast the phase accumulator
// advances through the sample data. Unity speed (1.0) means the sample plays
// at its original rate.
//
// Formula: base_increment = (source_sample_rate / output_sample_rate) << 32
// Set when a file is loaded, then modified by pitch control knobs.
uint64_t g_inc_base_q32_32 = (1ULL << 32);   // default = 1.0 ratio (no pitch change)

// ── Phase modulation scaling ──────────────────────────────────────────────
// Scaling factor for phase modulation (PM) from ADC input. PM allows real-time
// modulation of the playback position, creating effects like vibrato or tremolo.
// Currently unused but available for future effects.
int64_t g_pm_scale_q32_32 = 0;             // default = no phase modulation

// ── Reset trigger state ──────────────────────────────────────────────────────
// Reset trigger functionality for tempo sync - allows external triggers to reset
// the loop phase and recalculate loop parameters with crossfading
volatile bool g_reset_trigger_pending = false;
static bool s_reset_trigger_last_state = false;

// ── Loop LED state ────────────────────────────────────────────────────────────
// LED control for visual feedback when loop wraps
static bool s_loop_led_state = false;
static uint32_t s_loop_led_off_time = 0;
static const uint32_t LOOP_LED_BLINK_MS = 10;  // LED blink duration in milliseconds


static const int16_t* g_samples_q15 = nullptr;

static uint32_t g_total_samples = 0;      // set when file is bound
static const uint32_t MIN_LOOP_LEN_CONST = 64;  // Fixed minimum loop length
static uint32_t g_span_start    = 0;      // = total - MIN_LOOP_LEN_CONST (precomputed)
static uint32_t g_span_len      = 0;      // = total - MIN_LOOP_LEN_CONST (same span)


// ── Tune knob ──────────────────────────────────────────────────────────────
// Lookup table for exponential pitch control. The tune knob provides smooth
// pitch changes over a limited range (typically ±1 semitone).
//
// The table contains 257 entries (0-256) mapping ADC values to pitch ratios.
// Each entry is a Q16.16 fixed-point value representing the pitch multiplier.
// Formula: ratio = 2^(0.5 * x) where x ranges from -1 to +1
static uint32_t kExpo1Oct_Q16[257];

/**
 * @brief Initialize the exponential pitch control lookup table
 * 
 * This function pre-calculates a lookup table for smooth pitch control.
 * The table maps ADC values (0-4095) to pitch ratios using an exponential
 * curve that feels natural for musical pitch control.
 */
static void init_expo_table_1oct() {
    for (uint32_t i = 0; i <= 256; ++i) {
        float x     = ((int32_t)i - 128) / 128.0f;   // Map to -1..+1 range
        float ratio = exp2f(0.5f * x);               // Exponential curve: 2^(0.5*x)
        kExpo1Oct_Q16[i] = (uint32_t)lrintf(ratio * 65536.0f); // Convert to Q16.16
    }
}

/**
 * @brief Convert ADC value to pitch increment using exponential lookup table
 * 
 * This function maps a 12-bit ADC value (0-4095) to a pitch increment using
 * the exponential lookup table with linear interpolation for smooth control.
 * 
 * @param adc12 12-bit ADC value (0-4095)
 * @param base_inc_q16_16 Base increment in Q16.16 format
 * @return Pitch increment in Q16.16 format
 */
static inline uint32_t inc_from_adc_expo_LUT(uint16_t adc12, uint32_t base_inc_q16_16) {
    // Map 12-bit ADC (0-4095) to table index (0-256) with 8.4 fixed-point precision
    uint32_t t    = ((uint32_t)adc12 * 256u) >> 12;         // 0..256
    uint32_t idx  = (t > 256u) ? 256u : t;                  // Clamp to table bounds
    
    // Calculate table lookup with linear interpolation for smoothness
    uint32_t i0   = (adc12 * 256u) >> 12;                   // Integer table index
    if (i0 >= 256u) {
        // Handle edge case: use last table entry
        return (uint32_t)(((uint64_t)base_inc_q16_16 * kExpo1Oct_Q16[256]) >> 16);
    }
    
    // Extract fractional part for interpolation (8-bit precision)
    uint32_t w8   = (((uint32_t)adc12 * 256u) >> 4) & 0xFF; // 0..255 interpolation weight
    
    // Get adjacent table entries
    uint32_t a    = kExpo1Oct_Q16[i0];      // Lower bound
    uint32_t b    = kExpo1Oct_Q16[i0 + 1];  // Upper bound
    
    // Linear interpolation: result = a + (b-a) * weight
    uint32_t ratio_q16 = a + (((uint64_t)(b - a) * w8) >> 8);
    
    // Apply ratio to base increment and return in Q16.16 format
    return (uint32_t)(((uint64_t)base_inc_q16_16 * ratio_q16) >> 16);
}


// ── Loop zone ──────────────────────────────────────────────────────────────
// Call whenever a new file is loaded or min length changes
static inline void loop_mapper_recalc_spans() {
    uint32_t total = g_total_samples;
    uint32_t minlen = (MIN_LOOP_LEN_CONST < total) ? MIN_LOOP_LEN_CONST : (total ? total : 1);
    g_span_start = (total > minlen) ? (total - minlen) : 0;
    g_span_len   = (total > minlen) ? (total - minlen) : 0;
}

/**
 * @brief Convert ADC values to sample indices for loop boundaries
 * 
 * Maps 12-bit ADC values (0-4095) to actual sample indices within the loaded
 * audio file. Uses precomputed spans for efficient audio-rate processing.
 * 
 * @param adc_start ADC value for loop start (0-4095)
 * @param adc_len ADC value for loop length (0-4095)
 * @param total Total number of samples in the file
 * @param out_start Output: loop start sample index
 * @param out_end Output: loop end sample index (exclusive)
 */

static inline uint16_t q15_to_pwm_u(int16_t s) {
    // Map [-32768..32767] to [0..PWM_RESOLUTION-1]
    uint32_t u = ((uint16_t)s) ^ 0x8000u;        // offset-binary 0..65535
    return (uint16_t)((u * (PWM_RESOLUTION - 1u)) >> 16);
}

// Call this after the loader fills sf::audioData + metadata
void playback_bind_loaded_buffer(uint32_t src_sample_rate_hz,
                                 uint32_t out_sample_rate_hz,
                                 uint32_t sample_count)
{
    g_samples_q15   = reinterpret_cast<const int16_t*>(sf::audioData);
    g_total_samples = sample_count;

    // Unity base: src_hz / out_hz in Q16.16
    g_inc_base_q32_32 = (uint64_t)(((uint64_t)src_sample_rate_hz << 32) / (uint64_t)out_sample_rate_hz);

    // Ensure loop spans are valid for the new file
    loop_mapper_recalc_spans();
    
    // Reset loop boundaries calculation flag for new file
    ae_reset_loop_boundaries_flag();
    
    // Debug log - DISABLED TO PREVENT POPS
    // Serial.print(F("[AE] Buffer bound: "));
    // Serial.print(sample_count);
    // Serial.print(F(" samples @ "));
    // Serial.print(src_sample_rate_hz);
    // Serial.println(F(" Hz"));
}


void audio_init(void) {
    // Initialize all audio buffers to silence to prevent startup pops
    const uint16_t silence_pwm = PWM_RESOLUTION / 2;  // Midpoint = silence
    for (int i = 0; i < AUDIO_BLOCK_SIZE; i++) {
        pwm_out_buf_a[i] = silence_pwm;
        pwm_out_buf_b[i] = silence_pwm;
        pwm_out_buf_c[i] = silence_pwm;
        pwm_out_buf_d[i] = silence_pwm;
    }
    
    init_expo_table_1oct();
    configurePWM_DMA_L();
    configurePWM_DMA_R();
    unmuteAudioOutput();
    
    setupInterpolators();

    // dma_start_channel_mask(1u << dma_chan);
    // Serial.printf("[AE] DMA L started? %d\n", dma_channel_is_busy(dma_chan_a));
    // Serial.printf("[AE] DMA R started? %d\n", dma_channel_is_busy(dma_chan_c));

    // Serial.println(F("[AE] Audio engine initialized"));
}

void audio_tick(void) {
    // Process audio when either channel is ready to prevent buffer underruns
    // This fixes the pop issue caused by waiting for both channels simultaneously
    if (callback_flag_L > 0 || callback_flag_R > 0) {
        adc_filter_update_from_dma();
        ae_render_block(g_samples_q15, g_total_samples, s_state, &g_phase_q32_32);
        callback_flag_L = 0;
        callback_flag_R = 0;
    }
}

// ── Reset Trigger Functions ──────────────────────────────────────────────────────

/**
 * @brief Initialize GPIO18 for reset trigger detection
 * 
 * Sets up GPIO18 as an input with pull-down resistor for detecting external
 * reset triggers. The trigger is active-high (rising edge to trigger).
 */
void audio_engine_reset_trigger_init(void) {
    gpio_init(RESET_TRIGGER_PIN);
    gpio_set_dir(RESET_TRIGGER_PIN, GPIO_IN);
    gpio_pull_down(RESET_TRIGGER_PIN);
    
    // Initialize state tracking
    s_reset_trigger_last_state = gpio_get(RESET_TRIGGER_PIN);
    g_reset_trigger_pending = false;
    
    // Serial.println(F("[AE] Reset trigger initialized on GPIO18 (rising edge)"));
}

/**
 * @brief Poll for reset trigger on GPIO18
 * 
 * This function should be called from the main loop to detect rising edges
 * on the reset trigger pin. When a trigger is detected, it sets the pending
 * flag for processing in the audio engine.
 */
void audio_engine_reset_trigger_poll(void) {
    bool current_state = gpio_get(RESET_TRIGGER_PIN);
    
    // Detect rising edge (active-high trigger)
    if (!s_reset_trigger_last_state && current_state) {
        g_reset_trigger_pending = true;
        // Serial.println(F("[AE] Reset trigger detected (rising edge)"));
    }
    
    s_reset_trigger_last_state = current_state;
}

/**
 * @brief Handle pending reset trigger
 * 
 * This function processes a pending reset trigger by:
 * 1. Resetting the phase accumulator to 0
 * 2. Forcing ADC input re-read/update
 * 3. Recalculating loop region based on current ADC values
 * 4. Initiating crossfade if crossfade length is specified
 * 
 * This function should be called from the audio processing loop when
 * g_reset_trigger_pending is true.
 */


// ── Loop LED Functions ─────────────────────────────────────────────────────────

/**
 * @brief Initialize GPIO15 for loop LED
 * 
 * Sets up GPIO15 as an output for the external loop LED that blinks
 * when the loop wraps to provide visual feedback.
 */
void audio_engine_loop_led_init(void) {
    gpio_init(LOOP_LED_PIN);
    gpio_set_dir(LOOP_LED_PIN, GPIO_OUT);
    gpio_put(LOOP_LED_PIN, 0);  // Start with LED off
    
    // Initialize LED state
    s_loop_led_state = false;
    s_loop_led_off_time = 0;
    
    // Serial.println(F("[AE] Loop LED initialized on GPIO15"));
}

/**
 * @brief Update LED state and handle blinking
 * 
 * This function should be called from the main loop to manage the LED
 * blinking state. It turns the LED off after the blink duration expires.
 */
void audio_engine_loop_led_update(void) {
    // Check if LED is currently on and time to turn it off
    if (s_loop_led_state && millis() >= s_loop_led_off_time) {
        gpio_put(LOOP_LED_PIN, 0);  // Turn LED off
        s_loop_led_state = false;
    }
}

/**
 * @brief Trigger LED blink on loop wrap
 * 
 * This function should be called when the loop wraps to provide
 * visual feedback. It turns the LED on for a brief duration.
 */
void audio_engine_loop_led_blink(void) {
    gpio_put(LOOP_LED_PIN, 1);  // Turn LED on
    s_loop_led_state = true;
    s_loop_led_off_time = millis() + LOOP_LED_BLINK_MS;
}
