/**
 * @file driver_sh1122.h
 * @brief SH1122 OLED display driver and graphics system
 * 
 * This header provides the interface for the SH1122 OLED display (256x64 pixels).
 * The display system supports both text-based UI and graphics rendering, including
 * waveform visualization and real-time audio parameter display.
 * 
 * ## Display Features
 * 
 * **High Resolution**: 256x64 pixel OLED display with 4-bit grayscale support
 * - 16 shades of gray (0=black, 15=white)
 * - Fast SPI communication for real-time updates
 * - Low power consumption suitable for portable devices
 * 
 * **Dual Rendering Modes**:
 * - **Text Mode**: Scrolling log view for status messages and file browsing
 * - **Graphics Mode**: Waveform visualization and parameter displays
 * 
 * **Real-time Updates**: Display updates at ~60Hz independently of audio processing
 * to maintain responsive user interface.
 * 
 * ## Graphics API
 * 
 * The system provides both high-level text functions and low-level graphics
 * primitives for custom rendering:
 * 
 * - Text rendering with automatic scrolling and line wrapping
 * - Pixel-level graphics with 4-bit grayscale support
 * - Drawing primitives (lines, rectangles, filled shapes)
 * - Direct buffer access for custom rendering
 * 
 * ## Performance Considerations
 * 
 * The display runs on Core 1 to avoid interfering with real-time audio processing
 * on Core 0. All display operations are optimized for speed and use efficient
 * SPI communication to minimize CPU overhead.
 * 
 * @author Brian Varren
 * @version 1.0
 * @date 2024
 */

#pragma once
#include <stdint.h>

// Forward declaration; keep U8g2 headers out of most TUs
class U8G2;

namespace sf {

// --- Config for text/scroll view (constexprs: no RAM) ---
constexpr int DISPLAY_WIDTH        = 256;
constexpr int DISPLAY_HEIGHT       = 64;
constexpr int MAX_DISPLAY_LINES    = 20;
constexpr int MAX_LINE_CHARS       = 42;   // adjust for chosen font/width
constexpr int LINES_PER_SCREEN     = 7;
constexpr int LINE_HEIGHT          = 8;
constexpr uint32_t SCROLL_DELAY_MS = 500;

// Clear the log/view model
void view_clear_log();

// Append one line (truncates to MAX_LINE_CHARS)
void view_print_line(const char* s);

// Manual redraw of the current text model
void view_redraw_log(U8G2& g);

// Enable/disable automatic line scrolling for status/log views
void view_set_auto_scroll(bool enabled);

// Simple auto-scroll tick; pass millis()
void view_handle_scroll(uint32_t now_ms);

// Dirty flag if something changed
bool view_needs_redraw();

// If dirty, redraw + flush
void view_flush_if_dirty();

// Quick status card (title + line2)
void view_show_status(const char* title, const char* line2);

// Clear full screen for graphics (not the scrolling log)
void view_clear_screen();

// Draw a mono 16-bit waveform scaled to fill the screen.
// - data: interleaved if channels==2 (we average L/R on the fly)
// - frames: number of sample frames (per channel group)
// - channels: 1 or 2
void view_draw_waveform_16(const int16_t* data, uint32_t frames, uint8_t channels);

// One-time hardware init (pins, reset, SPI, u8g2.begin, defaults)
void  sh1122_init();

// Optional tweak
void  sh1122_set_contrast(uint8_t v);

// Access to the single global U8g2 object (owned in .cpp)
U8G2& sh1122_gfx();

// Convenience passthroughs (optional)
void  sh1122_clear_buffer();
void  sh1122_send_buffer();

// Send raw 4-bit grayscale buffer (128 bytes per row, 64 rows = 8192 bytes)
void  sh1122_send_gray4(const uint8_t* buf_256x64_gray4);

// === 4-bit Grayscale Drawing API ===
// All shade values are 0-15 (0=black, 15=white)

// Clear entire grayscale buffer to a shade
void gray4_clear(uint8_t shade);

// Set/get individual pixel
void gray4_set_pixel(int16_t x, int16_t y, uint8_t shade);
uint8_t gray4_get_pixel(int16_t x, int16_t y);

// Drawing primitives
void gray4_draw_hline(int16_t x0, int16_t x1, int16_t y, uint8_t shade);
void gray4_draw_vline(int16_t x, int16_t y0, int16_t y1, uint8_t shade);
void gray4_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t shade);
void gray4_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t shade);
void gray4_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t shade);

// Send grayscale buffer to sh1122
void gray4_send_buffer();

// Direct access to grayscale buffer (256x64 pixels, 2 pixels per byte = 8192 bytes)
uint8_t* gray4_get_buffer();

} // namespace sf