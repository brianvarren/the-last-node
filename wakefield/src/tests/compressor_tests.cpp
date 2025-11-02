#include "../compressor.h"
#include <cstdio>
#include <vector>
#include <cmath>
#include <algorithm>

static float dbToLin(float db) { return std::pow(10.0f, db / 20.0f); }

struct TestContext {
    int failures = 0;
    void assertTrue(bool cond, const char* msg) {
        if (!cond) {
            std::fprintf(stderr, "[FAIL] %s\n", msg);
            failures++;
        }
    }
    void assertNear(float a, float b, float tol, const char* msg) {
        if (std::fabs(a - b) > tol) {
            std::fprintf(stderr, "[FAIL] %s (got %.3f, expected %.3f, tol %.3f)\n", msg, a, b, tol);
            failures++;
        }
    }
};

int main() {
    constexpr float fs = 48000.0f;
    Compressor comp(fs);

    // Configure
    comp.setThreshold(-20.0f);
    comp.setRatio(4.0f);
    comp.setAttack(0.1f);
    comp.setRelease(10.0f);
    comp.setKnee(6.0f);
    comp.setMix(1.0f);
    comp.setAutoMakeup(false);
    comp.setManualMakeup(0.0f);
    comp.setDetectionMode(false); // peak

    TestContext t;

    // Sweep around knee and check continuity/monotonicity
    const float T = -20.0f;
    const float K = 6.0f;
    const float R = 4.0f;
    std::vector<float> levelsDb;
    for (float d = T - K/2.0f - 5.0f; d <= T + K/2.0f + 5.0f; d += 0.5f) {
        levelsDb.push_back(d);
    }
    std::vector<float> grOut(levelsDb.size(), 0.0f);

    std::vector<float> buf; buf.resize(4096 * 2);
    for (size_t i = 0; i < levelsDb.size(); ++i) {
        float amp = dbToLin(levelsDb[i]);
        for (size_t n = 0; n < buf.size(); n += 2) {
            buf[n] = amp; buf[n+1] = amp;
        }
        comp.process(buf.data(), buf.data(), (int)buf.size());
        grOut[i] = comp.getGainReduction(); // negative dB
    }

    // Below knee start => ~0 dB GR
    t.assertTrue(std::fabs(grOut.front()) < 0.2f, "GR ~ 0 dB below knee start");

    // Above knee end => close to hard-knee reduction
    float x = levelsDb.back();
    float yHard = T + (x - T) / R;
    float grExpected = yHard - x; // negative
    t.assertNear(grOut.back(), grExpected, 1.0f /*tolerance*/, "GR near hard-knee above knee end");

    // Monotonic non-increasing (more negative as level increases)
    for (size_t i = 1; i < grOut.size(); ++i) {
        if (!(grOut[i] <= grOut[i-1] + 0.25f)) {
            std::fprintf(stderr, "[FAIL] GR not monotonic at i=%zu: %.3f -> %.3f\n", i-1, grOut[i-1], grOut[i]);
            t.failures++;
            break;
        }
        // No large jumps (continuity)
        if (std::fabs(grOut[i] - grOut[i-1]) > 1.0f) {
            std::fprintf(stderr, "[FAIL] GR jump > 1 dB at i=%zu: delta=%.3f\n", i, grOut[i] - grOut[i-1]);
            t.failures++;
            break;
        }
    }

    // Auto makeup update triggers after ~100ms
    comp.setAutoMakeup(true);
    // Drive with constant level to induce GR
    float amp = dbToLin(T + 10.0f); // 10 dB above threshold
    for (size_t n = 0; n < buf.size(); n += 2) { buf[n] = amp; buf[n+1] = amp; }
    int total = (int)(fs * 0.5f); // 0.5 sec
    int processed = 0;
    while (processed < total) {
        int chunk = std::min((int)buf.size(), total - processed);
        // ensure even number of samples (stereo)
        if (chunk % 2) chunk--;
        comp.process(buf.data(), buf.data(), chunk);
        processed += chunk;
    }
    float mk = comp.getMakeupGain();
    t.assertTrue(mk >= 0.1f, "Auto makeup gain increased after 100ms");

    if (t.failures == 0) {
        std::printf("[OK] compressor_tests passed\n");
        return 0;
    }
    std::printf("[ERR] compressor_tests had %d failure(s)\n", t.failures);
    return 1;
}
