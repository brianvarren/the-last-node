#include "ui.h"

// All UI implementation has been extracted to separate files:
// - ui/ui_core.cpp: Constructor, destructor, initialization
// - ui/ui_input.cpp: Input handling
// - ui/ui_drawing.cpp: Main drawing functions
// - ui/ui_utils.cpp: Utility functions and constants
// - ui/ui_presets.cpp: Preset management
// - ui/ui_parameters.cpp: Parameter handling
// - ui/ui_help.cpp: Help system
// - ui/pages/*.cpp: Individual page implementations
// - ui/sequencer/*.cpp: Sequencer-related functions

// This file is intentionally minimal to avoid duplicate symbols during linking.
