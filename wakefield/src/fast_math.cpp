#include "fast_math.h"

namespace FastMath {

// Sine lookup table storage
float sinTable[kSinTableSize + 1];
bool sinTableInitialized = false;

} // namespace FastMath
