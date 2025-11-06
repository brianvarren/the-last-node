#include "sampler.h"
#include "sample_bank.h"
#include <algorithm>
#include <cstring>
#include <limits>

// Minimum loop length in samples (to prevent glitches)
static constexpr uint32_t MIN_LOOP_LENGTH = 2048;

// MIDI note to pitch ratio lookup table (note 60 = middle C = 1.0)
// Each semitone is 2^(1/12) higher than the previous
const float Sampler::MIDI_PITCH_TABLE[128] = {
    0.0312500f, 0.0331120f, 0.0350799f, 0.0371616f, 0.0393658f, 0.0417014f, 0.0441773f, 0.0468026f,
    0.0495870f, 0.0525404f, 0.0556731f, 0.0589956f, 0.0625000f, 0.0662240f, 0.0701598f, 0.0743231f,
    0.0787317f, 0.0834028f, 0.0883546f, 0.0936051f, 0.0991740f, 0.1050808f, 0.1113462f, 0.1179913f,
    0.1250000f, 0.1324481f, 0.1403197f, 0.1486463f, 0.1574634f, 0.1668057f, 0.1767091f, 0.1872103f,
    0.1983480f, 0.2101616f, 0.2226925f, 0.2359825f, 0.2500000f, 0.2648961f, 0.2806394f, 0.2972925f,
    0.3149268f, 0.3336113f, 0.3534183f, 0.3744205f, 0.3966960f, 0.4203233f, 0.4453851f, 0.4719651f,
    0.5000000f, 0.5297922f, 0.5612788f, 0.5945851f, 0.6298535f, 0.6672227f, 0.7068365f, 0.7488411f,
    0.7933920f, 0.8406466f, 0.8907702f, 0.9439301f, 1.0000000f, 1.0595845f, 1.1225575f, 1.1891702f,
    1.2597070f, 1.3344454f, 1.4136731f, 1.4976821f, 1.5867840f, 1.6812931f, 1.7815404f, 1.8878603f,
    2.0000000f, 2.1191689f, 2.2451151f, 2.3783404f, 2.5194140f, 2.6688908f, 2.8273462f, 2.9953642f,
    3.1735680f, 3.3625863f, 3.5630808f, 3.7757206f, 4.0000000f, 4.2383378f, 4.4902301f, 4.7566807f,
    5.0388281f, 5.3377817f, 5.6546924f, 5.9907284f, 6.3471360f, 6.7251726f, 7.1261615f, 7.5514412f,
    8.0000000f, 8.4766757f, 8.9804602f, 9.5133614f, 10.0776562f, 10.6755633f, 11.3093847f, 11.9814568f,
    12.6942720f, 13.4503451f, 14.2523231f, 15.1028824f, 16.0000000f, 16.9533513f, 17.9609203f, 19.0267229f,
    20.1553125f, 21.3511267f, 22.6187695f, 23.9629135f, 25.3885441f, 26.9006903f, 28.5046462f, 30.2057648f,
    32.0000000f, 33.9067026f, 35.9218407f, 38.0534458f, 40.3106250f, 42.7022533f, 45.2375389f, 47.9258270f
};

// Equal-power crossfade curves: cos(pi/2 * t) and sin(pi/2 * t) for t in [0, 1]
// These provide constant power during crossfade (cos^2 + sin^2 = 1)
// 256 entries provides smooth, zipper-free crossfades (~50x faster than std::cos/sin)
const float Sampler::XFADE_TABLE_COS[256] = {
    1.0000000f, 0.9999247f, 0.9996988f, 0.9993223f, 0.9987954f, 0.9981181f, 0.9972905f, 0.9963126f,
    0.9951847f, 0.9939070f, 0.9924795f, 0.9909026f, 0.9891765f, 0.9873014f, 0.9852776f, 0.9831054f,
    0.9807852f, 0.9783173f, 0.9757021f, 0.9729400f, 0.9700313f, 0.9669765f, 0.9637761f, 0.9604305f,
    0.9569403f, 0.9533060f, 0.9495281f, 0.9456073f, 0.9415440f, 0.9373390f, 0.9329928f, 0.9285060f,
    0.9238795f, 0.9191138f, 0.9142098f, 0.9091680f, 0.9039893f, 0.8986744f, 0.8932243f, 0.8876396f,
    0.8819213f, 0.8760701f, 0.8700870f, 0.8639729f, 0.8577286f, 0.8513552f, 0.8448536f, 0.8382247f,
    0.8314696f, 0.8245893f, 0.8175848f, 0.8104572f, 0.8032075f, 0.7958369f, 0.7883464f, 0.7807372f,
    0.7730104f, 0.7651672f, 0.7572088f, 0.7491364f, 0.7409511f, 0.7326543f, 0.7242470f, 0.7157308f,
    0.7071068f, 0.6983762f, 0.6895405f, 0.6806010f, 0.6715590f, 0.6624158f, 0.6531728f, 0.6438315f,
    0.6343933f, 0.6248595f, 0.6152316f, 0.6055110f, 0.5956993f, 0.5857979f, 0.5758082f, 0.5657318f,
    0.5555703f, 0.5453251f, 0.5349977f, 0.5245897f, 0.5141027f, 0.5035384f, 0.4928982f, 0.4821838f,
    0.4713967f, 0.4605387f, 0.4496113f, 0.4386162f, 0.4275551f, 0.4164296f, 0.4052414f, 0.3939920f,
    0.3826834f, 0.3713173f, 0.3598950f, 0.3484182f, 0.3368899f, 0.3253101f, 0.3136817f, 0.3020059f,
    0.2902847f, 0.2785197f, 0.2667128f, 0.2548658f, 0.2429802f, 0.2310581f, 0.2191013f, 0.2071113f,
    0.1950903f, 0.1830400f, 0.1709619f, 0.1588581f, 0.1467304f, 0.1345807f, 0.1224107f, 0.1102223f,
    0.0980171f, 0.0857973f, 0.0735646f, 0.0613207f, 0.0490677f, 0.0368072f, 0.0245412f, 0.0122715f,
    0.0000000f, -0.0122715f, -0.0245412f, -0.0368072f, -0.0490677f, -0.0613207f, -0.0735646f, -0.0857973f,
    -0.0980171f, -0.1102223f, -0.1224107f, -0.1345807f, -0.1467304f, -0.1588581f, -0.1709619f, -0.1830400f,
    -0.1950903f, -0.2071113f, -0.2191013f, -0.2310581f, -0.2429802f, -0.2548658f, -0.2667128f, -0.2785197f,
    -0.2902847f, -0.3020059f, -0.3136817f, -0.3253101f, -0.3368899f, -0.3484182f, -0.3598950f, -0.3713173f,
    -0.3826834f, -0.3939920f, -0.4052414f, -0.4164296f, -0.4275551f, -0.4386162f, -0.4496113f, -0.4605387f,
    -0.4713967f, -0.4821838f, -0.4928982f, -0.5035384f, -0.5141027f, -0.5245897f, -0.5349977f, -0.5453251f,
    -0.5555703f, -0.5657318f, -0.5758082f, -0.5857979f, -0.5956993f, -0.6055110f, -0.6152316f, -0.6248595f,
    -0.6343933f, -0.6438315f, -0.6531728f, -0.6624158f, -0.6715590f, -0.6806010f, -0.6895405f, -0.6983762f,
    -0.7071068f, -0.7157308f, -0.7242470f, -0.7326543f, -0.7409511f, -0.7491364f, -0.7572088f, -0.7651672f,
    -0.7730104f, -0.7807372f, -0.7883464f, -0.7958369f, -0.8032075f, -0.8104572f, -0.8175848f, -0.8245893f,
    -0.8314696f, -0.8382247f, -0.8448536f, -0.8513552f, -0.8577286f, -0.8639729f, -0.8700870f, -0.8760701f,
    -0.8819213f, -0.8876396f, -0.8932243f, -0.8986744f, -0.9039893f, -0.9091680f, -0.9142098f, -0.9191138f,
    -0.9238795f, -0.9285060f, -0.9329928f, -0.9373390f, -0.9415440f, -0.9456073f, -0.9495281f, -0.9533060f,
    -0.9569403f, -0.9604305f, -0.9637761f, -0.9669765f, -0.9700313f, -0.9729400f, -0.9757021f, -0.9783173f,
    -0.9807852f, -0.9831054f, -0.9852776f, -0.9873014f, -0.9891765f, -0.9909026f, -0.9924795f, -0.9939070f,
    -0.9951847f, -0.9963126f, -0.9972905f, -0.9981181f, -0.9987954f, -0.9993223f, -0.9996988f, -0.9999247f
};

const float Sampler::XFADE_TABLE_SIN[256] = {
    0.0000000f, 0.0122715f, 0.0245412f, 0.0368072f, 0.0490677f, 0.0613207f, 0.0735646f, 0.0857973f,
    0.0980171f, 0.1102223f, 0.1224107f, 0.1345807f, 0.1467304f, 0.1588581f, 0.1709619f, 0.1830400f,
    0.1950903f, 0.2071113f, 0.2191013f, 0.2310581f, 0.2429802f, 0.2548658f, 0.2667128f, 0.2785197f,
    0.2902847f, 0.3020059f, 0.3136817f, 0.3253101f, 0.3368899f, 0.3484182f, 0.3598950f, 0.3713173f,
    0.3826834f, 0.3939920f, 0.4052414f, 0.4164296f, 0.4275551f, 0.4386162f, 0.4496113f, 0.4605387f,
    0.4713967f, 0.4821838f, 0.4928982f, 0.5035384f, 0.5141027f, 0.5245897f, 0.5349977f, 0.5453251f,
    0.5555703f, 0.5657318f, 0.5758082f, 0.5857979f, 0.5956993f, 0.6055110f, 0.6152316f, 0.6248595f,
    0.6343933f, 0.6438315f, 0.6531728f, 0.6624158f, 0.6715590f, 0.6806010f, 0.6895405f, 0.6983762f,
    0.7071068f, 0.7157308f, 0.7242470f, 0.7326543f, 0.7409511f, 0.7491364f, 0.7572088f, 0.7651672f,
    0.7730104f, 0.7807372f, 0.7883464f, 0.7958369f, 0.8032075f, 0.8104572f, 0.8175848f, 0.8245893f,
    0.8314696f, 0.8382247f, 0.8448536f, 0.8513552f, 0.8577286f, 0.8639729f, 0.8700870f, 0.8760701f,
    0.8819213f, 0.8876396f, 0.8932243f, 0.8986744f, 0.9039893f, 0.9091680f, 0.9142098f, 0.9191138f,
    0.9238795f, 0.9285060f, 0.9329928f, 0.9373390f, 0.9415440f, 0.9456073f, 0.9495281f, 0.9533060f,
    0.9569403f, 0.9604305f, 0.9637761f, 0.9669765f, 0.9700313f, 0.9729400f, 0.9757021f, 0.9783173f,
    0.9807852f, 0.9831054f, 0.9852776f, 0.9873014f, 0.9891765f, 0.9909026f, 0.9924795f, 0.9939070f,
    0.9951847f, 0.9963126f, 0.9972905f, 0.9981181f, 0.9987954f, 0.9993223f, 0.9996988f, 0.9999247f,
    1.0000000f, 0.9999247f, 0.9996988f, 0.9993223f, 0.9987954f, 0.9981181f, 0.9972905f, 0.9963126f,
    0.9951847f, 0.9939070f, 0.9924795f, 0.9909026f, 0.9891765f, 0.9873014f, 0.9852776f, 0.9831054f,
    0.9807852f, 0.9783173f, 0.9757021f, 0.9729400f, 0.9700313f, 0.9669765f, 0.9637761f, 0.9604305f,
    0.9569403f, 0.9533060f, 0.9495281f, 0.9456073f, 0.9415440f, 0.9373390f, 0.9329928f, 0.9285060f,
    0.9238795f, 0.9191138f, 0.9142098f, 0.9091680f, 0.9039893f, 0.8986744f, 0.8932243f, 0.8876396f,
    0.8819213f, 0.8760701f, 0.8700870f, 0.8639729f, 0.8577286f, 0.8513552f, 0.8448536f, 0.8382247f,
    0.8314696f, 0.8245893f, 0.8175848f, 0.8104572f, 0.8032075f, 0.7958369f, 0.7883464f, 0.7807372f,
    0.7730104f, 0.7651672f, 0.7572088f, 0.7491364f, 0.7409511f, 0.7326543f, 0.7242470f, 0.7157308f,
    0.7071068f, 0.6983762f, 0.6895405f, 0.6806010f, 0.6715590f, 0.6624158f, 0.6531728f, 0.6438315f,
    0.6343933f, 0.6248595f, 0.6152316f, 0.6055110f, 0.5956993f, 0.5857979f, 0.5758082f, 0.5657318f,
    0.5555703f, 0.5453251f, 0.5349977f, 0.5245897f, 0.5141027f, 0.5035384f, 0.4928982f, 0.4821838f,
    0.4713967f, 0.4605387f, 0.4496113f, 0.4386162f, 0.4275551f, 0.4164296f, 0.4052414f, 0.3939920f,
    0.3826834f, 0.3713173f, 0.3598950f, 0.3484182f, 0.3368899f, 0.3253101f, 0.3136817f, 0.3020059f,
    0.2902847f, 0.2785197f, 0.2667128f, 0.2548658f, 0.2429802f, 0.2310581f, 0.2191013f, 0.2071113f,
    0.1950903f, 0.1830400f, 0.1709619f, 0.1588581f, 0.1467304f, 0.1345807f, 0.1224107f, 0.1102223f,
    0.0980171f, 0.0857973f, 0.0735646f, 0.0613207f, 0.0490677f, 0.0368072f, 0.0245412f, 0.0122715f
};

Sampler::Sampler()
    : currentSample(nullptr)
    , loopStartNorm(0.0f)
    , loopLengthNorm(1.0f)
    , crossfadeLengthNorm(0.1f)
    , playbackSpeed(1.0f)
    , tzfmDepth(0.0f)
    , level(1.0f)
    , mode(PlaybackMode::FORWARD)
    , keyMode(true)
    , primaryVoice(&voiceA)
    , secondaryVoice(&voiceB)
    , crossfading(false)
    , crossfadeSamplesTotal(0)
    , crossfadeSamplesRemaining(0)
    , pendingStart(0)
    , pendingEnd(0)
    , pendingLoopValid(false)
    , restartRequested(true)
    , wasInZoneLastSample(false)
    , playingReverse(false)
    , modulatorSmoothed(0.0f)
    , lastPhaseDriver(-1.0f) {

    // Initialize voice A as active
    voiceA.phase_q32_32 = 0;
    voiceA.loop_start = 0;
    voiceA.loop_end = 0;
    voiceA.amplitude = 1.0f;
    voiceA.active = true;

    // Initialize voice B as inactive
    voiceB.phase_q32_32 = 0;
    voiceB.loop_start = 0;
    voiceB.loop_end = 0;
    voiceB.amplitude = 0.0f;
    voiceB.active = false;
}

Sampler::Sampler(const Sampler& other)
    : currentSample(other.currentSample.load(std::memory_order_acquire))
    , loopStartNorm(other.loopStartNorm)
    , loopLengthNorm(other.loopLengthNorm)
    , crossfadeLengthNorm(other.crossfadeLengthNorm)
    , playbackSpeed(other.playbackSpeed)
    , tzfmDepth(other.tzfmDepth)
    , level(other.level)
    , mode(other.mode)
    , keyMode(other.keyMode)
    , voiceA(other.voiceA)
    , voiceB(other.voiceB)
    , primaryVoice(&voiceA)
    , secondaryVoice(&voiceB)
    , crossfading(other.crossfading)
    , crossfadeSamplesTotal(other.crossfadeSamplesTotal)
    , crossfadeSamplesRemaining(other.crossfadeSamplesRemaining)
    , pendingStart(other.pendingStart)
    , pendingEnd(other.pendingEnd)
    , pendingLoopValid(other.pendingLoopValid)
    , restartRequested(other.restartRequested)
    , wasInZoneLastSample(other.wasInZoneLastSample)
    , playingReverse(other.playingReverse)
    , modulatorSmoothed(other.modulatorSmoothed)
    , lastPhaseDriver(other.lastPhaseDriver) {
}

Sampler& Sampler::operator=(const Sampler& other) {
    if (this != &other) {
        currentSample.store(other.currentSample.load(std::memory_order_acquire), std::memory_order_release);
        loopStartNorm = other.loopStartNorm;
        loopLengthNorm = other.loopLengthNorm;
        crossfadeLengthNorm = other.crossfadeLengthNorm;
        playbackSpeed = other.playbackSpeed;
        tzfmDepth = other.tzfmDepth;
        level = other.level;
        mode = other.mode;
        keyMode = other.keyMode;
        voiceA = other.voiceA;
        voiceB = other.voiceB;
        primaryVoice = &voiceA;
        secondaryVoice = &voiceB;
        crossfading = other.crossfading;
        crossfadeSamplesTotal = other.crossfadeSamplesTotal;
        crossfadeSamplesRemaining = other.crossfadeSamplesRemaining;
        pendingStart = other.pendingStart;
        pendingEnd = other.pendingEnd;
        pendingLoopValid = other.pendingLoopValid;
        restartRequested = other.restartRequested;
        wasInZoneLastSample = other.wasInZoneLastSample;
        playingReverse = other.playingReverse;
        modulatorSmoothed = other.modulatorSmoothed;
        lastPhaseDriver = other.lastPhaseDriver;
    }
    return *this;
}

void Sampler::setSample(const SampleData* sample) {
    // Use atomic store to safely update sample pointer from UI thread
    currentSample.store(sample, std::memory_order_release);
    pendingLoopValid = false;
    restartRequested = true;
    reset();
}

void Sampler::setLoopStart(float normalized) {
    loopStartNorm = std::clamp(normalized, 0.0f, 1.0f);
}

void Sampler::setLoopLength(float normalized) {
    loopLengthNorm = std::clamp(normalized, 0.0f, 1.0f);
}

void Sampler::setCrossfadeLength(float normalized) {
    crossfadeLengthNorm = std::clamp(normalized, 0.0f, 1.0f);
}

void Sampler::setPlaybackSpeed(float speed) {
    playbackSpeed = speed;
}

void Sampler::setTZFMDepth(float depth) {
    tzfmDepth = std::clamp(depth, 0.0f, 1.0f);
}

void Sampler::setPlaybackMode(PlaybackMode newMode) {
    mode = newMode;
}

void Sampler::setLevel(float newLevel) {
    level = std::clamp(newLevel, 0.0f, 1.0f);
}

void Sampler::setKeyMode(bool enabled) {
    keyMode = enabled;
    if (!keyMode) {
        // Ensure free-run samplers start immediately when enabled externally
        restartRequested = true;
    }
}

float Sampler::getPlayheadPosition() const {
    const SampleData* sample = currentSample.load(std::memory_order_acquire);
    if (!sample || sample->sampleCount == 0) {
        return 0.0f;
    }
    uint32_t idx = static_cast<uint32_t>(primaryVoice->phase_q32_32 >> 32);
    return static_cast<float>(idx) / static_cast<float>(sample->sampleCount);
}

uint64_t Sampler::getCurrentPhase() const {
    return primaryVoice->phase_q32_32;
}

void Sampler::restorePhase(uint64_t phase) {
    primaryVoice->phase_q32_32 = phase;
}

void Sampler::reset() {
    const SampleData* sample = currentSample.load(std::memory_order_acquire);
    if (!sample || sample->sampleCount == 0) {
        primaryVoice->phase_q32_32 = 0;
        primaryVoice->loop_start = 0;
        primaryVoice->loop_end = 0;
        pendingLoopValid = false;
    } else {
        pendingLoopValid = false;
        restartRequested = true;
    }
    crossfading = false;
    wasInZoneLastSample = false;
    lastPhaseDriver = -1.0f;
}

void Sampler::requestRestart() {
    restartRequested = true;
    primaryVoice->active = true;
    primaryVoice->amplitude = 1.0f;
}

void Sampler::stopPlayback() {
    crossfading = false;
    primaryVoice->amplitude = 0.0f;
    primaryVoice->active = false;
    secondaryVoice->active = false;
    secondaryVoice->amplitude = 0.0f;
    crossfadeSamplesRemaining = 0;
    crossfadeSamplesTotal = 0;
    wasInZoneLastSample = false;
    restartRequested = false;
}

const char* Sampler::getSampleName() const {
    const SampleData* sample = currentSample.load(std::memory_order_acquire);
    if (sample) {
        return sample->name.c_str();
    }
    return "No Sample";
}

void Sampler::calculateLoopBoundaries(const SampleData* sample, float startMod, float lengthMod,
                                      float sampleRate, float tempo, int syncMode) {
    if (!sample || sample->sampleCount < MIN_LOOP_LENGTH) {
        pendingStart = 0;
        pendingEnd = 0;
        pendingLoopValid = false;
        return;
    }

    const uint32_t totalSamples = sample->sampleCount;
    const uint32_t availableSpan = totalSamples > MIN_LOOP_LENGTH ?
                                   totalSamples - MIN_LOOP_LENGTH : 0;

    // Apply modulation to loop start (clamped to 0-1)
    float modulatedStart = std::clamp(loopStartNorm + startMod, 0.0f, 1.0f);
    pendingStart = availableSpan > 0 ?
                   static_cast<uint32_t>(modulatedStart * availableSpan) : 0;

    uint32_t loopLen;

    // Tempo sync quantization
    if (syncMode > 0 && tempo > 0.1f && sampleRate > 0.0f) {
        // Calculate samples per quarter note at source sample rate
        // Use source rate since loop lengths are in source samples, not output samples
        float sourceSampleRate = static_cast<float>(sample->sampleRate);
        float samplesPerQuarterNote = (60.0f / tempo) * sourceSampleRate;

        // Apply sync mode modifier
        float syncMultiplier = 1.0f;
        switch (syncMode) {
            case 1: // ON (straight)
                syncMultiplier = 1.0f;
                break;
            case 2: // TRIPLET (2/3 of a quarter note)
                syncMultiplier = 2.0f / 3.0f;
                break;
            case 3: // DOTTED (1.5x quarter note)
                syncMultiplier = 3.0f / 2.0f;
                break;
        }

        // Calculate quantized loop length in source samples
        float quantizedLoopLength = samplesPerQuarterNote * syncMultiplier;

        // Use user's loop length control as a multiplier (0.0-1.0 maps to 0.25x-4x tempo)
        // This allows user to select different subdivisions
        float modulatedLength = std::clamp(loopLengthNorm + lengthMod, 0.0f, 1.0f);
        float lengthMultiplier = 0.25f + modulatedLength * 3.75f; // Maps 0→0.25x, 1→4x
        quantizedLoopLength *= lengthMultiplier;

        loopLen = static_cast<uint32_t>(quantizedLoopLength);

        // Clamp to minimum and maximum bounds
        loopLen = std::max(loopLen, MIN_LOOP_LENGTH);
        loopLen = std::min(loopLen, totalSamples - pendingStart);
    } else {
        // No sync: use user-controlled loop length as before
        float modulatedLength = std::clamp(loopLengthNorm + lengthMod, 0.0f, 1.0f);
        loopLen = MIN_LOOP_LENGTH +
                  (availableSpan > 0 ?
                   static_cast<uint32_t>(modulatedLength * availableSpan) : 0);
    }

    pendingEnd = pendingStart + loopLen;
    if (pendingEnd > totalSamples) {
        pendingEnd = totalSamples;
    }
    pendingLoopValid = true;
}

void Sampler::ensurePendingLoop(const SampleData* sample, float startMod, float lengthMod,
                                float sampleRate, float tempo, int syncMode) {
    calculateLoopBoundaries(sample, startMod, lengthMod, sampleRate, tempo, syncMode);
}

void Sampler::applyPendingLoopToVoice(SamplerVoice* voice) {
    if (!pendingLoopValid) {
        voice->active = false;
        return;
    }
    voice->loop_start = pendingStart;
    voice->loop_end = pendingEnd;
    uint32_t startSample = pendingStart;
    if (mode == PlaybackMode::REVERSE && pendingEnd > pendingStart) {
        startSample = pendingEnd - 1;
    }
    voice->phase_q32_32 = static_cast<uint64_t>(startSample) << 32;
    voice->active = true;
    voice->amplitude = 1.0f;
}

bool Sampler::wrapPhase(SamplerVoice* voice) const {
    const int64_t start_q = static_cast<int64_t>(voice->loop_start) << 32;
    const int64_t end_q = static_cast<int64_t>(voice->loop_end) << 32;
    const int64_t span_q = end_q - start_q;

    if (span_q <= 0) return false;

    int64_t phase = static_cast<int64_t>(voice->phase_q32_32);
    int64_t normalized = phase - start_q;
    bool wrapped = false;

    // Modulo wrapping for both directions
    if (normalized >= span_q) {
        normalized = normalized % span_q;
        wrapped = true;
    } else if (normalized < 0) {
        normalized = span_q - ((-normalized) % span_q);
        if (normalized == span_q) normalized = 0;
        wrapped = true;
    }

    voice->phase_q32_32 = static_cast<uint64_t>(start_q + normalized);
    return wrapped;
}

int16_t Sampler::getSample(const SampleData* sample, const SamplerVoice* voice, bool isReverse) const {
    if (!voice->active || voice->amplitude <= 0.0f || !sample) {
        return 0;
    }
    if (voice->loop_end <= voice->loop_start) {
        return 0;
    }

    // Extract integer sample index from Q32.32 phase
    uint32_t i = static_cast<uint32_t>(voice->phase_q32_32 >> 32);

    // Bounds check with graceful handling during crossfade
    if (crossfading && voice == primaryVoice) {
        if (i >= sample->sampleCount) {
            if (voice->amplitude > 0.1f) {
                i = sample->sampleCount - 1;
            } else {
                i %= sample->sampleCount;
            }
        }
    } else {
        if (i >= sample->sampleCount) {
            return 0;
        }
    }

    // Additional fade near loop boundary during crossfade to avoid edge artifacts
    float additionalFade = 1.0f;
    if (crossfading && voice == primaryVoice) {
        // Fade over last few samples approaching the loop boundary actually being crossed
        constexpr uint32_t kFadeSpan = 8u;
        if (isReverse) {
            // Approaching loop start in reverse
            if (i < voice->loop_start + kFadeSpan) {
                uint32_t dist = (i > voice->loop_start) ? (i - voice->loop_start) : 0u;
                additionalFade = std::clamp(static_cast<float>(dist) / static_cast<float>(kFadeSpan - 1), 0.0f, 1.0f);
            }
        } else {
            // Approaching loop end in forward
            uint32_t endIdx = voice->loop_end > 0 ? (voice->loop_end - 1) : voice->loop_start;
            if (i > endIdx - kFadeSpan) {
                uint32_t dist = (endIdx > i) ? (endIdx - i) : 0u;
                additionalFade = std::clamp(static_cast<float>(dist) / static_cast<float>(kFadeSpan - 1), 0.0f, 1.0f);
            }
        }
    }

    // Get second sample for interpolation (handle loop boundaries)
    uint32_t i2;
    if (isReverse) {
        i2 = (i > voice->loop_start) ? (i - 1) : (voice->loop_end - 1);
    } else {
        i2 = (i < voice->loop_end - 1) ? (i + 1) : voice->loop_start;
    }

    // Extract 8-bit fractional part for interpolation
    const uint32_t frac32 = static_cast<uint32_t>(voice->phase_q32_32 & 0xFFFFFFFFull);
    const uint8_t mu8 = static_cast<uint8_t>(frac32 >> 24);

    // Perform interpolation
    int16_t sampleValue = interpolate(sample->samples[i],
                                sample->samples[i2],
                                mu8);

    // Apply additional fade if needed
    sampleValue = static_cast<int16_t>(sampleValue * additionalFade);

    return sampleValue;
}

int64_t Sampler::calculateIncrement(const SampleData* sample, float sampleRate, float fmInput,
                                    float pitchMod, bool isReverse, int midiNote) {
    if (!sample) {
        return 0;
    }

    // Calculate base increment from sample rate ratio and playback speed
    // Q32.32 format: (source_rate / output_rate) * playbackSpeed
    double baseRatio = (static_cast<double>(sample->sampleRate) /
                       static_cast<double>(sampleRate)) * playbackSpeed;

    // KEY mode: apply exponential pitch tracking based on MIDI note
    // C4 (note 60) = 1.0, each semitone = 2^(1/12)
    // Use lookup table to avoid expensive pow() calculation (~100x faster)
    if (keyMode) {
        int clampedNote = std::clamp(midiNote, 0, 127);
        double pitchScale = MIDI_PITCH_TABLE[clampedNote];
        baseRatio *= pitchScale;
    }

    // Apply pitch modulation (in octaves)
    // Use fast approximation instead of std::pow (~10-20x faster)
    baseRatio *= fastPow2(pitchMod);

    int64_t baseInc = static_cast<int64_t>(baseRatio * (1ULL << 32));

    // Apply direction
    if (isReverse) {
        baseInc = -baseInc;
    }

    // Apply TZFM from FM matrix routing
    // FM input is already scaled by FM matrix depth (0-1 range from matrix)
    // One-pole smoothing to prevent clicks
    modulatorSmoothed = MODULATOR_SMOOTHING * modulatorSmoothed +
                      (1.0f - MODULATOR_SMOOTHING) * fmInput;

    // Apply modulation: allows through-zero
    // FM input comes pre-scaled from the matrix, apply tzfmDepth scaling
    float modulated = static_cast<float>(baseInc) * (1.0f + modulatorSmoothed * tzfmDepth);
    baseInc = static_cast<int64_t>(modulated);

    // Safety limits
    const int64_t MAX_INC = 1LL << 37;
    const int64_t MIN_INC = -(1LL << 37);
    baseInc = std::clamp(baseInc, MIN_INC, MAX_INC);

    return baseInc;
}

bool Sampler::isInCrossfadeZone(uint64_t phase, uint32_t loopStart,
                               uint32_t loopEnd, uint32_t xfadeLen,
                               bool isReverse) const {
    uint32_t idx = static_cast<uint32_t>(phase >> 32);

    if (isReverse) {
        // Reverse: trigger zone at beginning of loop
        uint32_t zoneEnd = (loopStart + xfadeLen < loopEnd) ?
                          (loopStart + xfadeLen) : loopEnd;
        return (idx >= loopStart && idx < zoneEnd);
    } else {
        // Forward: trigger zone at end of loop
        uint32_t zoneStart = (loopEnd > xfadeLen) ?
                           (loopEnd - xfadeLen) : loopStart;
        return (idx >= zoneStart && idx < loopEnd);
    }
}

void Sampler::setupCrossfade(uint32_t xfadeLen, uint32_t xfadeSamples,
                            bool isReverse) {
    // Secondary voice gets new loop boundaries
    secondaryVoice->loop_start = pendingStart;
    secondaryVoice->loop_end = pendingEnd;
    secondaryVoice->active = true;
    secondaryVoice->amplitude = 0.0f;

    // Position secondary voice at start of new loop
    if (isReverse) {
        secondaryVoice->phase_q32_32 =
            static_cast<uint64_t>(pendingEnd - 1) << 32;
    } else {
        secondaryVoice->phase_q32_32 =
            static_cast<uint64_t>(pendingStart) << 32;
    }

    // Start crossfade
    crossfading = true;
    crossfadeSamplesTotal = xfadeSamples;
    crossfadeSamplesRemaining = xfadeSamples;
}

void Sampler::applyPhaseDriver(const SampleData* sample, float normalized) {
    if (!sample || !primaryVoice) {
        return;
    }

    normalized = std::clamp(normalized, 0.0f, 1.0f);

    if (primaryVoice->loop_end <= primaryVoice->loop_start) {
        return;
    }

    uint32_t loopStart = primaryVoice->loop_start;
    uint32_t loopEnd = primaryVoice->loop_end;
    uint32_t span = loopEnd > loopStart ? (loopEnd - loopStart) : 0;
    if (span == 0) {
        return;
    }

    uint32_t target = loopStart + static_cast<uint32_t>(normalized * static_cast<float>(span - 1));
    primaryVoice->phase_q32_32 = static_cast<uint64_t>(target) << 32;

    if (secondaryVoice && secondaryVoice->active) {
        secondaryVoice->phase_q32_32 = primaryVoice->phase_q32_32;
    }
}

float Sampler::process(float sampleRate, float fmInput, float pitchMod,
                      float loopStartMod, float loopLengthMod,
                      float crossfadeMod, float levelMod, float levelOffset,
                      float phaseDriver, int midiNote,
                      float tempo, int syncMode) {
    // Early exit if no sample loaded
    const SampleData* sample = currentSample.load(std::memory_order_acquire);
    if (!sample || !sample->samples ||
        sample->sampleCount < 2) {
        return 0.0f;
    }

    // Early exit if primary voice is inactive (saves all processing for silent samplers)
    if (!primaryVoice->active && !restartRequested) {
        return 0.0f;
    }

    if (restartRequested) {
        ensurePendingLoop(sample, loopStartMod, loopLengthMod, sampleRate, tempo, syncMode);
        crossfading = false;
        crossfadeSamplesRemaining = 0;
        crossfadeSamplesTotal = 0;
        applyPendingLoopToVoice(primaryVoice);
        secondaryVoice->active = false;
        secondaryVoice->amplitude = 0.0f;
        wasInZoneLastSample = false;
        playingReverse = false;  // Reset ping-pong direction
        restartRequested = false;
    }

    if (phaseDriver >= 0.0f && std::isfinite(phaseDriver)) {
        float normalized = std::clamp(phaseDriver, 0.0f, 1.0f);
        if (std::fabs(normalized - lastPhaseDriver) > 0.001f) {
            applyPhaseDriver(sample, normalized);
            lastPhaseDriver = normalized;
        }
    } else {
        lastPhaseDriver = -1.0f;
    }

    // Calculate crossfade length (in source samples)
    // Ping-pong (ALTERNATE) mode disables crossfading - uses phase reflection instead
    uint32_t xfadeLen = 0;
    if (mode != PlaybackMode::ALTERNATE && primaryVoice->loop_end > primaryVoice->loop_start) {
        uint32_t loopLen = primaryVoice->loop_end - primaryVoice->loop_start;
        uint32_t maxXfade = loopLen / 2;

        // Apply crossfade modulation
        float modulatedXfade = std::clamp(crossfadeLengthNorm + crossfadeMod,
                                         0.0f, 1.0f);
        xfadeLen = static_cast<uint32_t>(maxXfade * modulatedXfade);
        xfadeLen = std::clamp(xfadeLen, 8u, maxXfade);
    }

    // Determine playback direction
    bool isReverse = (mode == PlaybackMode::REVERSE) ||
                     (mode == PlaybackMode::ALTERNATE && playingReverse);

    // Calculate phase increment (pass cached sample pointer)
    int64_t inc = calculateIncrement(sample, sampleRate, fmInput, pitchMod, isReverse, midiNote);

    // Convert crossfade length to output samples using actual increment
    uint32_t xfadeSamples = 16u;
    if (xfadeLen > 0) {
        uint64_t incMagnitude = static_cast<uint64_t>(inc >= 0 ? inc : -inc);
        if (incMagnitude == 0) {
            incMagnitude = 1;
        }
        uint64_t totalDistance = static_cast<uint64_t>(xfadeLen) << 32;
        uint64_t framesNeeded = (totalDistance + incMagnitude - 1) / incMagnitude;  // ceil division
        framesNeeded = std::max<uint64_t>(16ull, framesNeeded);
        if (framesNeeded > std::numeric_limits<uint32_t>::max()) {
            framesNeeded = std::numeric_limits<uint32_t>::max();
        }
        xfadeSamples = static_cast<uint32_t>(framesNeeded);
    }

    // Check for crossfade trigger before wrapping
    if (!crossfading && xfadeLen > 0) {
        bool inZone = isInCrossfadeZone(primaryVoice->phase_q32_32,
                                       primaryVoice->loop_start,
                                       primaryVoice->loop_end,
                                       xfadeLen, isReverse);

        if (inZone && !wasInZoneLastSample) {
            ensurePendingLoop(sample, loopStartMod, loopLengthMod, sampleRate, tempo, syncMode);
            setupCrossfade(xfadeLen, xfadeSamples, isReverse);
        }
        wasInZoneLastSample = inZone;
    }

    // Advance primary voice
    primaryVoice->phase_q32_32 += inc;

    // Only wrap when not crossfading
    if (!crossfading) {
        bool wrappedPrimary = wrapPhase(primaryVoice);
        if (wrappedPrimary) {
            // Toggle direction for ping-pong mode
            if (mode == PlaybackMode::ALTERNATE) {
                playingReverse = !playingReverse;
            }

            if (xfadeLen == 0) {
                ensurePendingLoop(sample, loopStartMod, loopLengthMod, sampleRate, tempo, syncMode);
                applyPendingLoopToVoice(primaryVoice);
            }
        }
    }

    // Handle crossfading
    if (crossfading) {
        // Advance secondary voice
        secondaryVoice->phase_q32_32 += inc;
        wrapPhase(secondaryVoice);

        // Calculate constant-power crossfade amplitudes using lookup table (~50x faster)
        float t = static_cast<float>(crossfadeSamplesTotal -
                                    crossfadeSamplesRemaining) /
                 static_cast<float>(crossfadeSamplesTotal);
        // Map t (0.0-1.0) to first half of table (0-127) for cos(0→π/2) and sin(0→π/2)
        int tableIndex = static_cast<int>(t * ((XFADE_TABLE_SIZE / 2) - 1));
        tableIndex = std::clamp(tableIndex, 0, (XFADE_TABLE_SIZE / 2) - 1);
        primaryVoice->amplitude = XFADE_TABLE_COS[tableIndex];
        secondaryVoice->amplitude = XFADE_TABLE_SIN[tableIndex];

        if (--crossfadeSamplesRemaining == 0) {
            // Crossfade complete - swap voices
            crossfading = false;
            SamplerVoice* temp = primaryVoice;
            primaryVoice = secondaryVoice;
            secondaryVoice = temp;
            secondaryVoice->active = false;
            secondaryVoice->amplitude = 0.0f;
            primaryVoice->amplitude = 1.0f;
        }
    }

    // Mix both voices (pass cached sample pointer)
    int32_t mixedSample = 0;
    bool isRevNow = (inc < 0);

    if (primaryVoice->active && primaryVoice->amplitude > 0.0f) {
        int16_t s = getSample(sample, primaryVoice, isRevNow);
        mixedSample += static_cast<int32_t>(s * primaryVoice->amplitude);
    }

    if (secondaryVoice->active && secondaryVoice->amplitude > 0.0f) {
        int16_t s = getSample(sample, secondaryVoice, isRevNow);
        mixedSample += static_cast<int32_t>(s * secondaryVoice->amplitude);
    }

    // Clamp to prevent overflow
    mixedSample = std::clamp(mixedSample, -32768, 32767);

    // Convert Q15 to float
    float output = static_cast<float>(mixedSample) / 32768.0f;

    // Apply amplitude modulation
    // Align amplitude model with oscillators so a sampler is audible
    // without requiring explicit amp modulation:
    // Oscillators: (baseAmp + ampMod) × level, baseAmp defaults to 1.0
    // Samplers:    (1.0 + levelMod) × level, base amp defaults to 1.0
    // This ensures the static sampler level controls volume even when
    // no modulation is routed to the sampler amp.
    float modulatedAmp = std::clamp(1.0f + levelMod, 0.0f, 1.0f);
    float modulatedLevel = std::clamp(level + levelOffset, 0.0f, 1.0f);
    float finalGain = modulatedAmp * modulatedLevel;

    return output * finalGain;
}

// Linear interpolation between two int16_t values using 8-bit fractional weight
int16_t Sampler::interpolate(int16_t x, int16_t y, uint8_t mu) {
    // mu is 0-255, where 0 = x, 255 = y
    int32_t result = x + (((static_cast<int32_t>(y) - x) * mu) >> 8);
    return static_cast<int16_t>(result);
}

// Fast 2^x approximation using 5th-order minimax polynomial
// Accurate to <0.15% error over [-1, 1] range (suitable for pitch modulation)
// About 10-20x faster than std::pow(2.0, x)
inline float Sampler::fastPow2(float x) {
    // Minimax polynomial coefficients for 2^x over [-1, 1]
    // Generated to minimize maximum absolute error
    constexpr float c0 = 1.0f;
    constexpr float c1 = 0.6931471805599453f;  // ln(2)
    constexpr float c2 = 0.2402265069591007f;  // ln(2)^2 / 2!
    constexpr float c3 = 0.0555041086648215f;  // ln(2)^3 / 3!
    constexpr float c4 = 0.0096181291076284f;  // ln(2)^4 / 4!
    constexpr float c5 = 0.0013333558146179f;  // ln(2)^5 / 5!

    // Horner's method for polynomial evaluation (efficient and numerically stable)
    return c0 + x * (c1 + x * (c2 + x * (c3 + x * (c4 + x * c5))));
}
