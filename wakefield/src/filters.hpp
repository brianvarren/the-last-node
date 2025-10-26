#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>







// 1-pole (opt)
// One-pole lowpass/highpass filter using the "TPT" (Trapezoidal) method.

struct OnePoleTPT {
    explicit OnePoleTPT(float sampleRate = 48000.f) : sr(std::max(1.f, sampleRate)) {
        setCutoff(1000.f);
    }

    void setSampleRate(float sampleRate) {
        sr = std::max(1.f, sampleRate);
        setCutoff(fcHz); // recompute coeffs for new SR
    }

    void setCutoff(float hz) {
        // clamp to [0, Nyquist*0.49] to avoid tan() blowup
        fcHz = std::clamp(hz, 0.0f, 0.49f * sr);
        constexpr float PI = 3.14159265358979323846f;
        const float g = std::tan(PI * fcHz / sr);   // trapezoidal (BLT) prewarp
        tpt_g   = g;
        tpt_inv = 1.0f / (1.0f + g);                // precompute 1/(1+g)
    }

    // Process one sample; returns {lp, hp}
    inline std::pair<float, float> process(float x) {
        // v = (x - s) * g/(1+g)
        const float v  = (x - s) * (tpt_g * tpt_inv);
        const float lp = v + s;
        s = lp + v;                 // state update (integrator)
        const float hp = x - lp;    // complementary HP
        return { lp, hp };
    }

    // Process a block of N samples
    inline void processBlock(const float* in, float* outLP, float* outHP, int n) {
        for (int i = 0; i < n; ++i) {
            const float xi = in[i];
            const float v  = (xi - s) * (tpt_g * tpt_inv);
            const float lp = v + s;
            s = lp + v;
            outLP[i] = lp;
            outHP[i] = xi - lp;
        }
    }

    inline void reset(float val = 0.f) { s = val; }

    // (optional) expose current cutoff and SR
    float sampleRate() const { return sr; }
    float cutoffHz()   const { return fcHz; }

private:
    float sr   = 48000.f;
    float fcHz = 1000.f;

    // integrator state and precomputed coeffs
    float s       = 0.f;   // state (integrator memory)
    float tpt_g   = 0.f;   // tan(pi*f/sr)
    float tpt_inv = 1.f;   // 1/(1+g)
};






// BiLin 1P HS
// One-pole high-shelf filter using the Bilinear Transform

struct OnePoleHighShelfBLT {
    // Public params (read-only)
    float sampleRate {48000.0f};
    float fcHz {1000.0f};     // shelf turnover (prewarped)
    float A {1.0f};           // linear HF gain (A = 10^(dB/20))

    // Coeffs
    float b0{1.0f}, b1{0.0f}, a1{0.0f};

    // State (TDF2)
    float s1{0.0f};

    // --- utility
    static inline float fast_tan(float x) { return std::tan(x); }

    void setSampleRate(float sr) {
        sampleRate = std::max(1.0f, sr);
        updateCoeffs();
    }

    // Cutoff: clamp to (0, 0.49*sr) to keep prewarp sane
    void setCutoff(float hz) {
        fcHz = std::clamp(hz, 1e-3f, 0.49f * sampleRate);
        updateCoeffs();
    }

    // Set high-frequency gain in dB (e.g. +6 dB => A=~2.0)
    void setGainDb(float dB) {
        A = std::pow(10.0f, dB / 20.0f);
        updateCoeffs();
    }

    // Or set directly in linear scale
    void setGainLinear(float linearA) {
        // avoid non-physical negatives or zeros
        A = std::max(1e-6f, linearA);
        updateCoeffs();
    }

    void reset() { s1 = 0.0f; }

    inline float process(float x) {
        // Transposed Direct Form II (one state), modulation-stable
        float y = b0 * x + s1;
        s1 = b1 * x - a1 * y;
        // tiny DC blocker against subnormal build-up (optional)
        return y;
    }

    void processBlock(const float* in, float* out, int N) {
        for (int n = 0; n < N; ++n) out[n] = process(in[n]);
    }

private:
    void updateCoeffs() {
        // prewarp (bilinear): g = tan(pi*fc/fs)
        const float g = fast_tan(float(M_PI) * (fcHz / sampleRate));

        const float denom = 1.0f + g + 1e-30f; // keep safe
        a1 = (g - 1.0f) / denom;               // feedback
        b0 = (A + g) / denom;
        b1 = -(A - g) / denom;

        // simple denormal guard on state if coeffs change wildly
        if (std::abs(s1) < 1e-30f) s1 = 0.0f;
    }
};






// BiLin 1P LS
// One-pole LOW-shelf filter using the Bilinear Transform (prewarped)

struct OnePoleLowShelfBLT {
    float sampleRate {48000.0f};
    float fcHz {1000.0f};     // shelf turnover (prewarped)
    float A {1.0f};           // linear DC gain (A = 10^(dB/20))

    // Coeffs
    float b0{1.0f}, b1{0.0f}, a1{0.0f};

    // State (TDF2)
    float s1{0.0f};

    static inline float fast_tan(float x) { return std::tan(x); }

    void setSampleRate(float sr) { sampleRate = std::max(1.0f, sr); updateCoeffs(); }

    void setCutoff(float hz) {
        fcHz = std::clamp(hz, 1e-3f, 0.49f * sampleRate);
        updateCoeffs();
    }

    // A is the **low-frequency** gain for LS
    void setGainDb(float dB) { A = std::pow(10.0f, dB / 20.0f); updateCoeffs(); }
    void setGainLinear(float linearA) { A = std::max(1e-6f, linearA); updateCoeffs(); }

    void reset() { s1 = 0.0f; }

    inline float process(float x) {
        float y = b0 * x + s1;
        s1 = b1 * x - a1 * y;
        return y;
    }

    void processBlock(const float* in, float* out, int N) {
        for (int n = 0; n < N; ++n) out[n] = process(in[n]);
    }

private:
    void updateCoeffs() {
        const float g = fast_tan(float(M_PI) * (fcHz / sampleRate)); // prewarp
        const float d = 1.0f + g + 1e-30f;

        // --------- ONLY THESE LINES DIFFER FROM HS ----------
        a1 = (g - 1.0f) / d;
        b0 = (1.0f + A * g) / d;
        b1 = (A * g - 1.0f) / d;
        // -----------------------------------------------------

        if (std::abs(s1) < 1e-30f) s1 = 0.0f; // denormal guard
    }
};





// 8-pole ladder lowpass with simple OTA-style saturation and HP-filtered feedback.
class Ladder8PoleZdf {
public:
    explicit Ladder8PoleZdf(float sampleRate = 48000.0f) {
        setSampleRate(sampleRate);
        setCutoff(1000.0f);
    }

    void setSampleRate(float sr) {
        sampleRate = std::max(1.0f, sr);
        for (auto& stage : stages) {
            stage.setSampleRate(sampleRate);
        }
        feedbackHP.setSampleRate(sampleRate);
        setCutoff(cutoffHz);
        setFeedbackHighpass(feedbackHpHz);
    }

    void setCutoff(float hz) {
        cutoffHz = std::clamp(hz, 20.0f, 0.45f * sampleRate);
        for (auto& stage : stages) {
            stage.setCutoff(cutoffHz);
        }
    }

    void setResonance(float amount) {
        // Map 0-1 UI range to a musically useful feedback amount
        resonance = std::clamp(amount, 0.0f, 1.2f);
        resonanceGain = 0.2f + resonance * 3.5f;  // scale to roughly 0.2 - 3.7
    }

    void setDrive(float driveAmount) {
        float drv = std::clamp(driveAmount, 0.1f, 15.0f);
        inputDrive = drv;
        if (drv <= 1.0f) {
            stageDrive = 1.0f;
        } else {
            stageDrive = 1.0f + (drv - 1.0f) * 0.5f;
        }
    }

    void setFeedbackHighpass(float hz) {
        feedbackHpHz = std::clamp(hz, 10.0f, std::min(6000.0f, 0.45f * sampleRate));
        feedbackHP.setCutoff(feedbackHpHz);
    }

    void reset() {
        for (auto& stage : stages) stage.reset();
        stageOutputs.fill(0.0f);
        feedbackHP.reset();
        lastFeedbackHP = 0.0f;
    }

    float process(float in) {
        const float feedback = lastFeedbackHP;
        float x = saturate(in * inputDrive - feedback * resonanceGain);

        for (std::size_t i = 0; i < stages.size(); ++i) {
            auto [lp, hp] = stages[i].process(x);
            stageOutputs[i] = saturate(lp * stageDrive);
            x = stageOutputs[i];
        }

        auto hpPair = feedbackHP.process(stageOutputs.back());
        lastFeedbackHP = hpPair.second;
        return stageOutputs.back();
    }

    float getStageOutput(int stageIndex) const {
        if (stageIndex < 0 || stageIndex >= static_cast<int>(stageOutputs.size())) {
            return stageOutputs.back();
        }
        return stageOutputs[stageIndex];
    }

private:
    float sampleRate = 48000.0f;
    float cutoffHz = 1000.0f;
    float resonance = 0.0f;
    float resonanceGain = 0.2f;
    float inputDrive = 1.0f;
    float stageDrive = 1.0f;
    float feedbackHpHz = 200.0f;

    std::array<OnePoleTPT, 8> stages;
    OnePoleTPT feedbackHP;
    std::array<float, 8> stageOutputs {0};
    float lastFeedbackHP = 0.0f;

    inline float saturate(float x) const {
        return std::tanh(x);
    }
};





class LadderDiodeZdf {
public:
    explicit LadderDiodeZdf(float sampleRate = 48000.0f) {
        setSampleRate(sampleRate);
        setCutoff(1000.0f);
    }

    void setSampleRate(float sr) {
        sampleRate = std::max(1.0f, sr);
        for (auto& stage : stages) stage.setSampleRate(sampleRate);
        feedbackHP.setSampleRate(sampleRate);
        setCutoff(cutoffHz);
        setFeedbackHighpass(feedbackHpHz);
    }

    void setCutoff(float hz) {
        cutoffHz = std::clamp(hz, 20.0f, 0.45f * sampleRate);
        for (auto& stage : stages) stage.setCutoff(cutoffHz);
    }

    void setResonance(float amount) {
        resonance = std::clamp(amount, 0.0f, 1.2f);
        // Diode ladders reach self-oscillation earlier, scale accordingly
        resonanceGain = 0.1f + resonance * 2.8f;
    }

    void setDrive(float driveAmount) {
        float drv = std::clamp(driveAmount, 0.1f, 15.0f);
        inputDrive = drv;
        stageDrive = (drv <= 1.0f) ? 1.0f : (1.0f + (drv - 1.0f) * 0.35f);
        asymAmount = std::clamp(0.15f * drv, 0.05f, 1.0f);
    }

    void setFeedbackHighpass(float hz) {
        feedbackHpHz = std::clamp(hz, 10.0f, std::min(6000.0f, 0.45f * sampleRate));
        feedbackHP.setCutoff(feedbackHpHz);
    }

    void reset() {
        for (auto& stage : stages) stage.reset();
        stageOutputs.fill(0.0f);
        feedbackHP.reset();
        lastFeedbackHP = 0.0f;
    }

    float process(float in) {
        const float feedback = lastFeedbackHP;
        float x = satPair(inputDrive * in - resonanceGain * feedback);

        for (std::size_t i = 0; i < stages.size(); ++i) {
            auto [lp, hp] = stages[i].process(x);
            float shaped = satStage(lp * stageDrive);
            stageOutputs[i] = shaped;
            x = shaped;
        }

        auto hpPair = feedbackHP.process(stageOutputs.back());
        lastFeedbackHP = hpPair.second;
        return stageOutputs.back();
    }

    float getStageOutput(int stageIndex) const {
        if (stageIndex < 0 || stageIndex >= static_cast<int>(stageOutputs.size())) {
            return stageOutputs.back();
        }
        return stageOutputs[stageIndex];
    }

private:
    float sampleRate = 48000.0f;
    float cutoffHz = 1000.0f;
    float resonance = 0.0f;
    float resonanceGain = 0.1f;
    float inputDrive = 1.0f;
    float stageDrive = 1.0f;
    float feedbackHpHz = 200.0f;
    float asymAmount = 0.2f;

    std::array<OnePoleTPT, 4> stages;
    OnePoleTPT feedbackHP;
    std::array<float, 4> stageOutputs {0};
    float lastFeedbackHP = 0.0f;

    inline float satPair(float x) const {
        // Input pair saturation, mostly symmetric
        return std::tanh(x);
    }

    inline float satStage(float x) const {
        // Stage saturation with diode-like asymmetry
        float pos = std::tanh((x + asymAmount) * 1.5f);
        float neg = std::tanh((x - asymAmount) * 1.5f);
        return 0.5f * (pos + neg);
    }
};




// Two-pole ZDF bandpass cell with controllable width via 2R damping
class BandpassCellZdf {
public:
    BandpassCellZdf() = default;

    void setSampleRate(float sr) {
        sampleRate = std::max(1.0f, sr);
        updateCoefficients();
    }

    void setCutoff(float hz) {
        cutoffHz = std::clamp(hz, 20.0f, 0.45f * sampleRate);
        updateCoefficients();
    }

    void setTwoR(float value) {
        twoR = std::max(0.05f, value);
        updateCoefficients();
    }

    void setDrive(float drv) {
        drive = std::clamp(drv, 0.1f, 15.0f);
    }

    void reset() {
        ic1 = 0.0f;
        ic2 = 0.0f;
        lastOutput = 0.0f;
    }

    float process(float in) {
        const float hp = (in - twoR * ic1 - ic2) * a1;
        float bp = hp * a2 + ic1;
        float lp = hp * a3 + ic2;

        ic1 = bp + hp * a2;
        ic2 = lp + hp * a3;

        lastOutput = std::tanh(bp * drive);
        return lastOutput;
    }

    float getLastOutput() const { return lastOutput; }

private:
    void updateCoefficients() {
        const float g = std::tan(float(M_PI) * (cutoffHz / sampleRate));
        const float gg = g * g;
        const float denom = 1.0f / (1.0f + twoR * g + gg + 1e-12f);
        a1 = denom;
        a2 = g * denom;
        a3 = g * a2;
    }

    float sampleRate = 48000.0f;
    float cutoffHz = 1000.0f;
    float twoR = 1.5f;
    float drive = 1.0f;
    float ic1 = 0.0f;
    float ic2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float a3 = 0.0f;
    float lastOutput = 0.0f;
};

// Standalone 2-pole bandpass using the ZDF cell
class Bandpass2PoleZdf {
public:
    explicit Bandpass2PoleZdf(float sampleRate = 48000.0f) {
        setSampleRate(sampleRate);
        setCutoff(1000.0f);
        setResonance(0.0f);
        setDrive(1.0f);
    }

    void setSampleRate(float sr) {
        sampleRate = std::max(1.0f, sr);
        cell.setSampleRate(sampleRate);
        setCutoff(cutoffHz);
        updateTwoR();
    }

    void setCutoff(float hz) {
        cutoffHz = std::clamp(hz, 20.0f, 0.45f * sampleRate);
        cell.setCutoff(cutoffHz);
    }

    void setResonance(float amount) {
        resonance = std::clamp(amount, 0.0f, 1.2f);
        feedbackGain = resonance * 3.0f;
    }

    void setDrive(float driveAmount) {
        drive = std::clamp(driveAmount, 0.1f, 15.0f);
        cell.setDrive(drive);
    }

    void setWidth(float w) {
        widthNorm = std::clamp(w, 0.0f, 1.0f);
        updateTwoR();
    }

    void setFeedbackHighpass(float) {
        // unused, kept for API compatibility
    }

    void reset() {
        cell.reset();
        lastOutput = 0.0f;
    }

    float process(float in) {
        const float feedback = std::tanh(lastOutput * feedbackGain);
        float x = in + feedback;
        lastOutput = cell.process(x);
        return lastOutput;
    }

private:
    void updateTwoR() {
        constexpr float INV_SQRT2 = 0.70710678118f;
        const float bwPrime = std::max(1.0f, widthNorm * 12.0f);
        const float expBw = std::exp(bwPrime * INV_SQRT2);
        const float invExp = 1.0f / expBw;
        const float twoR = std::clamp(expBw - invExp, 0.05f, 20.0f);
        cell.setTwoR(twoR);
    }

    BandpassCellZdf cell;
    float sampleRate = 48000.0f;
    float cutoffHz = 1000.0f;
    float resonance = 0.0f;
    float feedbackGain = 0.0f;
    float drive = 1.0f;
    float widthNorm = 0.5f;
    float lastOutput = 0.0f;
};

// 4 × 2-pole bandpass ladder with global bandwidth control
class LadderBandpassZdf {
public:
    explicit LadderBandpassZdf(float sampleRate = 48000.0f) {
        setSampleRate(sampleRate);
        setCutoff(1000.0f);
        setResonance(0.0f);
        setDrive(1.0f);
    }

    void setSampleRate(float sr) {
        sampleRate = std::max(1.0f, sr);
        for (auto& cell : cells) {
            cell.setSampleRate(sampleRate);
        }
        setCutoff(cutoffHz);
        updateTwoR();
    }

    void setCutoff(float hz) {
        cutoffHz = std::clamp(hz, 20.0f, 0.45f * sampleRate);
        for (auto& cell : cells) {
            cell.setCutoff(cutoffHz);
        }
    }

    void setResonance(float amount) {
        resonance = std::clamp(amount, 0.0f, 1.2f);
        // Scale feedback gain based on resonance
        feedbackGain = resonance * 3.0f;
    }

    void setDrive(float driveAmount) {
        drive = std::clamp(driveAmount, 0.1f, 15.0f);
        for (auto& cell : cells) {
            cell.setDrive(drive);
        }
    }

    void setFeedbackHighpass(float) {
        // Unused in this architecture but kept for API compatibility
    }

    void setWidth(float w) {
        widthNorm = std::clamp(w, 0.0f, 1.0f);
        updateTwoR();
    }

    void reset() {
        for (auto& cell : cells) {
            cell.reset();
        }
        stageOutputs.fill(0.0f);
        lastOutput = 0.0f;
    }

    float process(float in) {
        // Saturated feedback from output
        float feedback = std::tanh(lastOutput * feedbackGain);

        // Mix input with feedback
        float x = in + feedback;

        for (std::size_t i = 0; i < cells.size(); ++i) {
            auto& cell = cells[i];
            x = cell.process(x);
            stageOutputs[i] = x;
        }

        lastOutput = x;
        return x;
    }

    float getStageOutput(int idx) const {
        if (idx >= 0 && idx < static_cast<int>(stageOutputs.size())) {
            return stageOutputs[idx];
        }
        return lastOutput;
    }

private:
    float sampleRate = 48000.0f;
    float cutoffHz = 1000.0f;
    float resonance = 0.0f;
    float feedbackGain = 0.0f;
    float drive = 1.0f;
    float widthNorm = 0.5f;
    float twoR = 1.5f;

    std::array<BandpassCellZdf, 4> cells;
    std::array<float, 4> stageOutputs {0.0f, 0.0f, 0.0f, 0.0f};
    float lastOutput = 0.0f;

    void updateTwoR() {
        constexpr float INV_SQRT2 = 0.70710678118f;
        const float bwPrime = std::max(1.0f, widthNorm * 12.0f);
        const float expBw = std::exp(bwPrime * INV_SQRT2);
        const float invExp = 1.0f / expBw;
        twoR = std::clamp(expBw - invExp, 0.05f, 20.0f);
        for (auto& cell : cells) {
            cell.setTwoR(twoR);
        }
    }
};



class NotchStageZdf {
public:
    NotchStageZdf() = default;

    void setSampleRate(float sr) {
        sampleRate = std::max(1.0f, sr);
        setCutoff(cutoffHz);
    }

    void setCutoff(float hz) {
        cutoffHz = std::clamp(hz, 20.0f, 0.45f * sampleRate);
        const float gRaw = std::tan(static_cast<float>(M_PI) * cutoffHz / sampleRate);
        g = gRaw;
    }

    void setDamping(float value) {
        damping = std::clamp(value, 0.01f, 0.99f);
        R = 2.0f * (1.0f - damping);
    }

    void setDrive(float driveAmount) {
        float drv = std::clamp(driveAmount, 0.1f, 15.0f);
        satGain = 0.5f * drv;
    }

    void reset() {
        ic1 = 0.0f;
        ic2 = 0.0f;
    }

    float process(float input) {
        float t = (input - ic2 - R * ic1);
        float v3 = std::tanh(t * satGain);
        float v1 = g * v3 + ic1;
        float v2 = g * v1 + ic2;
        ic1 = v1 + g * v3;
        ic2 = v2 + g * v1;
        float hp = v3 - R * v1;
        float notch = hp + v2;
        lastOutput = notch;
        return notch;
    }

    float getLastOutput() const { return lastOutput; }

private:
    float sampleRate = 48000.0f;
    float cutoffHz = 1000.0f;
    float g = 0.1f;
    float damping = 0.5f;
    float R = 1.0f;
    float satGain = 0.5f;
    float ic1 = 0.0f;
    float ic2 = 0.0f;
    float lastOutput = 0.0f;
};

class DualNotchZdf {
public:
    explicit DualNotchZdf(float sampleRate = 48000.0f) {
        setSampleRate(sampleRate);
        setCutoff(1000.0f);
    }

    void setSampleRate(float sr) {
        sampleRate = std::max(1.0f, sr);
        for (auto& stage : stages) stage.setSampleRate(sampleRate);
        updateCutoffs();
    }

    void setCutoff(float hz) {
        baseCutoff = std::clamp(hz, 20.0f, 0.45f * sampleRate);
        updateCutoffs();
    }

    void setSpread(float value) {
        spread = std::clamp(value, 0.0f, 1.0f);
        updateCutoffs();
        applyDamping();
    }

    void setResonance(float amount) {
        baseDamping = std::clamp(1.0f - 0.5f * amount, 0.05f, 0.95f);
        applyDamping();
    }

    void setDrive(float driveAmount) {
        for (auto& stage : stages) stage.setDrive(driveAmount);
    }

    void setNotchFeedback(float value) {
        feedbackGain = std::clamp(value, 0.0f, 0.95f);
    }

    void reset() {
        for (auto& stage : stages) stage.reset();
        lastOutput = 0.0f;
    }

    float process(float input) {
        float fb = feedbackGain * std::tanh(lastOutput);
        float x = input + fb;
        float y = x;
        for (auto& stage : stages) {
            y = stage.process(y);
        }
        lastOutput = y;
        return y;
    }

private:
    void updateCutoffs() {
        float ratio = std::pow(2.0f, (spread - 0.5f) * 2.0f);  // +/-1 octave
        stages[0].setCutoff(baseCutoff);
        stages[1].setCutoff(std::clamp(baseCutoff * ratio, 20.0f, 0.45f * sampleRate));
    }

    void applyDamping() {
        stages[0].setDamping(baseDamping);
        float damp2 = std::clamp(baseDamping * (1.0f + spread * 1.5f), 0.05f, 0.99f);
        stages[1].setDamping(damp2);
    }

    float sampleRate = 48000.0f;
    float baseCutoff = 1000.0f;
    float spread = 0.5f;
    float feedbackGain = 0.3f;
    float baseDamping = 0.7f;
    std::array<NotchStageZdf, 2> stages;
    float lastOutput = 0.0f;
};
