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




// Single bandpass cell: TPT 1-pole with separate LP and HP outputs
// Can be cascaded (LP → HP) to create bandpass character
struct BandpassCellOutput {
    float lp;  // Lowpass output (saturated)
    float hp;  // Highpass output (saturated)
};

class BandpassCellZdf {
public:
    BandpassCellZdf() = default;

    void setSampleRate(float sr) {
        sampleRate = std::max(1.0f, sr);
        updateCoeffs();
    }

    void setCutoff(float hz) {
        cutoffHz = std::clamp(hz, 20.0f, 0.45f * sampleRate);
        updateCoeffs();
    }

    void setDrive(float drv) {
        drive = std::clamp(drv, 0.1f, 15.0f);
    }

    void reset() {
        z = 0.0f;
    }

    BandpassCellOutput process(float x) {
        // TPT one-pole: d = input - state
        float d = x - z;

        // LP output
        float lp = z + b * d;

        // HP output (complementary)
        float hp = a * d;

        // Trapezoidal state update
        z = lp + b * d;

        // Return both saturated outputs
        return {
            std::tanh(lp * drive),  // Saturated LP
            std::tanh(hp * drive)   // Saturated HP
        };
    }

    // Getter for debugging
    float getG() const { return g; }

private:
    void updateCoeffs() {
        // Clamp to safe fraction of Nyquist
        float fc = std::clamp(cutoffHz, 0.0f, 0.49f * sampleRate);
        g = std::tan(static_cast<float>(M_PI) * (fc / sampleRate));
        a = 1.0f / (1.0f + g);
        b = g * a;
    }

    float sampleRate = 48000.0f;
    float cutoffHz = 1000.0f;
    float drive = 1.0f;

    // TPT coefficients
    float g = 0.0f;  // tan(pi*fc/fs)
    float a = 1.0f;  // 1/(1+g)
    float b = 0.0f;  // g/(1+g)

    // Integrator state
    float z = 0.0f;
};

// Normalized 2-pole bandpass block (LP→BP transform of the halved 1-pole lowpass)
// Damping R (and thus bandwidth) is controlled via setWidth; no internal feedback
class Bandpass2PoleZdf {
public:
    explicit Bandpass2PoleZdf(float sampleRate = 48000.0f) {
        setSampleRate(sampleRate);
        setCutoff(1000.0f);
        setDrive(1.0f);
        setWidth(0.5f);
    }

    void setSampleRate(float sr) {
        sampleRate = std::max(1.0f, sr);
        updateCutoffs();
    }

    void setCutoff(float hz) {
        centerFreq = std::clamp(hz, 20.0f, 0.42f * sampleRate);
        updateCutoffs();
    }

    void setResonance(float amount) { (void)amount; }

    void setDrive(float driveAmount) {
        drive = std::clamp(driveAmount, 0.1f, 15.0f);
    }

    void setWidth(float widthAmount) {
        widthNorm = std::clamp(widthAmount, 0.0f, 1.0f);
        updateDamping();
    }

    void setFeedbackHighpass(float) { }

    void reset() {
        ic1 = 0.0f;
        ic2 = 0.0f;
        lastOutput = 0.0f;
    }

    float process(float in) {
        const float hp = (in - damping * ic1 - ic2) * invDenom;
        const float bp = hp * g + ic1;
        const float lp = bp * g + ic2;

        ic1 = bp + hp * g;
        ic2 = lp + bp * g;

        // State limiting to prevent explosion at high frequencies
        constexpr float STATE_LIMIT = 4.0f;
        ic1 = std::clamp(ic1, -STATE_LIMIT, STATE_LIMIT);
        ic2 = std::clamp(ic2, -STATE_LIMIT, STATE_LIMIT);

        const float normalized = bp * normGain;
        lastOutput = std::tanh(normalized * drive);
        return lastOutput;
    }

    float getWidthOct() const { return widthOct; }
    float getWidthRatio() const { return widthRatio; }
    float getTwoR() const { return twoR; }
    float getDamping() const { return damping; }
    float getNormGain() const { return normGain; }
    float getCenterFreq() const { return centerFreq; }
    float getG() const { return g; }

private:
    void updateCutoffs() {
        float fc = std::clamp(centerFreq, 20.0f, 0.42f * sampleRate);
        g = std::tan(static_cast<float>(M_PI) * (fc / sampleRate));
        updateDenominator();
    }

    void updateDamping() {
        constexpr float MIN_OCT = 0.25f;
        constexpr float MAX_OCT = 10.0f;
        widthOct = MIN_OCT + widthNorm * (MAX_OCT - MIN_OCT);

        const float halfSpread = 0.5f * widthOct;
        const float ratio = std::pow(2.0f, halfSpread);
        widthRatio = ratio;

        twoR = std::max(0.005f, ratio - (1.0f / ratio));
        damping = twoR;
        normGain = twoR;
        updateDenominator();
    }

    void updateDenominator() {
        invDenom = 1.0f / (1.0f + g * damping + g * g + 1e-12f);
    }

    float sampleRate = 48000.0f;
    float centerFreq = 1000.0f;
    float widthNorm = 0.5f;
    float drive = 1.0f;

    float g = 0.0f;
    float damping = 1.0f;
    float twoR = 2.0f;
    float normGain = 0.5f;
    float invDenom = 1.0f;
    float ic1 = 0.0f;
    float ic2 = 0.0f;

    float lastOutput = 0.0f;
    float widthOct = 1.0f;
    float widthRatio = 1.0f;
};

// 4-pole bandpass ladder built from two normalized 2-pole bandpass stages
// Positive feedback coefficient k (<4) controls resonance; width maps to damping R
class LadderBandpassZdf {
public:
    explicit LadderBandpassZdf(float sampleRate = 48000.0f) {
        setSampleRate(sampleRate);
        setCutoff(1000.0f);
        setResonance(0.0f);
        setDrive(1.0f);
        setWidth(0.5f);
    }

    void setSampleRate(float sr) {
        sampleRate = std::max(1.0f, sr);
        for (auto& stage : stages) stage.setSampleRate(sampleRate);
        setCutoff(cutoffHz);
    }

    void setCutoff(float hz) {
        cutoffHz = std::clamp(hz, 20.0f, 0.42f * sampleRate);
        for (auto& stage : stages) stage.setCutoff(cutoffHz);
    }

    void setResonance(float amount) {
        resonance = std::clamp(amount, 0.0f, 1.2f);
        feedbackGain = std::clamp(resonance * (3.8f / 1.2f), 0.0f, 3.8f);
    }

    void setDrive(float driveAmount) {
        drive = std::clamp(driveAmount, 0.1f, 15.0f);
        for (auto& stage : stages) stage.setDrive(drive);
    }

    void setFeedbackHighpass(float) { }

    void setWidth(float w) {
        widthNorm = std::clamp(w, 0.0f, 1.0f);
        for (auto& stage : stages) stage.setWidth(widthNorm);
    }

    void reset() {
        for (auto& stage : stages) stage.reset();
        stageOutputs.fill(0.0f);
        lastFeedbackNode = 0.0f;
        lastOutput = 0.0f;
    }

    float process(float in) {
        const float feedback = feedbackGain * std::tanh(lastFeedbackNode);
        float x = std::tanh(in + feedback);  // Limit input to prevent explosion

        float stage1 = stages[0].process(x);
        stageOutputs[0] = stage1;

        float stage2Input = 0.5f * stage1;
        float stage2 = stages[1].process(stage2Input);
        stageOutputs[1] = stage2;

        float feedforward = 0.5f * stage2;
        lastFeedbackNode = std::clamp(feedforward, -2.0f, 2.0f);  // Limit feedback node

        lastOutput = feedforward * (4.0f - feedbackGain);
        return lastOutput;
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

    std::array<Bandpass2PoleZdf, 2> stages;
    std::array<float, 2> stageOutputs {0.0f, 0.0f};
    float lastFeedbackNode = 0.0f;
    float lastOutput = 0.0f;
};

// 8-pole bandpass ladder built from four normalized 2-pole bandpass stages
// Per Zavalishin Fig. 5.43: 4× BPn stages with 1/2 scaling, feedback k
// Passband gain is 1/16 (vs 1/4 for 4-pole), requiring more output compensation
class Ladder8PoleBandpassZdf {
public:
    explicit Ladder8PoleBandpassZdf(float sampleRate = 48000.0f) {
        setSampleRate(sampleRate);
        setCutoff(1000.0f);
        setResonance(0.0f);
        setDrive(1.0f);
        setWidth(0.5f);
    }

    void setSampleRate(float sr) {
        sampleRate = std::max(1.0f, sr);
        for (auto& stage : stages) stage.setSampleRate(sampleRate);
        setCutoff(cutoffHz);
    }

    void setCutoff(float hz) {
        cutoffHz = std::clamp(hz, 20.0f, 0.42f * sampleRate);
        for (auto& stage : stages) stage.setCutoff(cutoffHz);
    }

    void setResonance(float amount) {
        resonance = std::clamp(amount, 0.0f, 1.2f);
        // 8-pole ladder: self-oscillation at k=16 (vs k=4 for 4-pole)
        // Scale feedback range accordingly
        feedbackGain = std::clamp(resonance * (15.8f / 1.2f), 0.0f, 15.8f);
    }

    void setDrive(float driveAmount) {
        drive = std::clamp(driveAmount, 0.1f, 15.0f);
        for (auto& stage : stages) stage.setDrive(drive);
    }

    void setFeedbackHighpass(float) { }

    void setWidth(float w) {
        widthNorm = std::clamp(w, 0.0f, 1.0f);
        for (auto& stage : stages) stage.setWidth(widthNorm);
    }

    void reset() {
        for (auto& stage : stages) stage.reset();
        stageOutputs.fill(0.0f);
        lastFeedbackNode = 0.0f;
        lastOutput = 0.0f;
    }

    float process(float in) {
        const float feedback = feedbackGain * std::tanh(lastFeedbackNode);
        float x = std::tanh(in + feedback);  // Limit input to prevent explosion

        // 4 stages with 0.5 scaling between each
        float stage1 = stages[0].process(x);
        stageOutputs[0] = stage1;

        float stage2Input = 0.5f * stage1;
        float stage2 = stages[1].process(stage2Input);
        stageOutputs[1] = stage2;

        float stage3Input = 0.5f * stage2;
        float stage3 = stages[2].process(stage3Input);
        stageOutputs[2] = stage3;

        float stage4Input = 0.5f * stage3;
        float stage4 = stages[3].process(stage4Input);
        stageOutputs[3] = stage4;

        // Final output after all 4 stages (each with 0.5 scaling = 1/16 total)
        float feedforward = 0.5f * stage4;
        lastFeedbackNode = std::clamp(feedforward, -2.0f, 2.0f);  // Limit feedback node

        // Compensate for 1/16 passband gain: (16 - feedbackGain)
        lastOutput = feedforward * (16.0f - feedbackGain);
        return lastOutput;
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

    std::array<Bandpass2PoleZdf, 4> stages;  // 4 stages for 8-pole
    std::array<float, 4> stageOutputs {0.0f, 0.0f, 0.0f, 0.0f};
    float lastFeedbackNode = 0.0f;
    float lastOutput = 0.0f;
};



// 2-pole allpass for multinotch filters (Zavalishin Chapter 11)
// Transfer function: G₂(s) = (1 - 2Rs + s²)/(1 + 2Rs + s²)
class Allpass2PoleZdf {
public:
    Allpass2PoleZdf() = default;

    void setSampleRate(float sr) {
        sampleRate = std::max(1.0f, sr);
        updateCoefficients();
    }

    void setCutoff(float hz) {
        cutoffHz = std::clamp(hz, 20.0f, 0.45f * sampleRate);
        updateCoefficients();
    }

    void setDamping(float R_value) {
        R = std::clamp(R_value, 0.01f, 10.0f);
        updateCoefficients();
    }

    void reset() {
        s1 = 0.0f;
        s2 = 0.0f;
    }

    float process(float input) {
        // ZDF implementation of 2-pole allpass
        // G₂(s) = (1 - 2Rs + s²)/(1 + 2Rs + s²)

        const float hp = (input - twoR * s1 - s2) * h;
        const float bp = g * hp + s1;
        const float lp = g * bp + s2;

        // Update state
        s1 = g * hp + bp;
        s2 = g * bp + lp;

        // Correct 2-pole allpass output (Zavalishin):
        // G2(s) = (1 - 2Rs + s^2)/(1 + 2Rs + s^2)
        // In SVF terms this is hp - 2R*bp + lp
        return hp - twoR * bp + lp;
    }

private:
    void updateCoefficients() {
        g = std::tan(static_cast<float>(M_PI) * cutoffHz / sampleRate);
        twoR = 2.0f * R;
        h = 1.0f / (1.0f + twoR * g + g * g);
    }

    float sampleRate = 48000.0f;
    float cutoffHz = 1000.0f;
    float R = 1.0f;
    float g = 0.1f;
    float twoR = 2.0f;
    float h = 1.0f;
    float s1 = 0.0f;  // First integrator state
    float s2 = 0.0f;  // Second integrator state
};

// Multinotch filter with feedback and corrected mixing (Zavalishin Chapter 11, Fig 11.13)
// Uses N=2 cascaded 2-pole allpasses to create 2 notches
// H(s) = (1/2) · (1 + G(s))/(1 - kG(s)) where G(s) = G₂²(s)
class DualNotchZdf {
public:
    explicit DualNotchZdf(float sampleRate = 48000.0f) {
        setSampleRate(sampleRate);
        setCutoff(1000.0f);
        setSpread(0.5f);      // Initialize spread (controls width via R)
        setResonance(0.0f);   // Initialize resonance (controls peak height via k)
        setDryWet(1.0f);
    }

    void setSampleRate(float sr) {
        sampleRate = std::max(1.0f, sr);
        for (auto& stage : allpassStages) stage.setSampleRate(sampleRate);
        setCutoff(cutoffHz);
    }

    void setCutoff(float hz) {
        cutoffHz = std::clamp(hz, 20.0f, 0.45f * sampleRate);
        for (auto& stage : allpassStages) stage.setCutoff(cutoffHz);
    }

    void setResonance(float amount) {
        // CORRECTED: Resonance now controls feedback k (peak height)
        // Higher resonance = taller peaks, notches stay deep
        resonance = std::clamp(amount, 0.0f, 1.0f);

        // Map resonance to k: 0.0 to 0.95 (stay well below 1.0 for stability)
        feedbackK = std::clamp(resonance * 0.95f, 0.0f, 0.95f);
    }

    void setSpread(float value) {
        // CORRECTED: Spread now controls damping R (notch/peak width)
        // Higher spread = wider notches/peaks (higher R)
        spread = std::clamp(value, 0.0f, 1.0f);

        // Map spread to R: 0.3 (narrow) to 5.0 (wide)
        const float R = 0.3f + spread * 4.7f;
        for (auto& stage : allpassStages) stage.setDamping(R);
    }

    void setNotchFeedback(float value) {
        // Legacy parameter - now just forwards to setResonance for compatibility
        // Users should use Resonance parameter instead
        setResonance(value);
    }

    void setDryWet(float value) {
        // Dry/wet mix: a parameter from Zavalishin Fig 11.17
        // a=0: dry (input only), a=1: wet (full multinotch)
        dryWet = std::clamp(value, 0.0f, 1.0f);
    }

    void setDrive(float driveAmount) {
        // Not used in this corrected implementation
        (void)driveAmount;
    }

    void reset() {
        for (auto& stage : allpassStages) stage.reset();
        lastAllpassOut = 0.0f;
    }

    float process(float input) {
        // Zavalishin Fig 11.13: Correct feedback and mixing topology

        // Limit input to prevent DC buildup
        input = std::clamp(input, -4.0f, 4.0f);

        // Pre-allpass signal with feedback: x̃ = x + k*ỹ
        const float preAllpass = std::tanh(input + feedbackK * lastAllpassOut);

        // Process through cascaded allpasses: ỹ = G(x̃)
        float postAllpass = preAllpass;
        for (auto& stage : allpassStages) {
            postAllpass = stage.process(postAllpass);
        }

        // Soft clip and limit feedback signal for stability
        postAllpass = std::clamp(std::tanh(postAllpass), -3.0f, 3.0f);

        // Check for NaN/Inf and reset if necessary
        if (!std::isfinite(postAllpass)) {
            reset();
            postAllpass = 0.0f;
        }

        // Store for next feedback iteration
        lastAllpassOut = postAllpass;

        // Correct mixing: (x̃ + ỹ)/2
        const float wetSignal = 0.5f * (preAllpass + postAllpass);

        // Dry/wet mixing (Zavalishin Fig 11.17):
        // y = (1 - a/2)x + (a/2)(1+k)ỹ
        // Simplified for our use: y = (1-a)*x + a*wet
        const float output = (1.0f - dryWet) * input + dryWet * wetSignal;

        return output;
    }

private:
    float sampleRate = 48000.0f;
    float cutoffHz = 1000.0f;
    float resonance = 0.0f;
    float spread = 0.5f;
    float feedbackK = 0.5f;
    float dryWet = 1.0f;

    std::array<Allpass2PoleZdf, 2> allpassStages;  // N=2 for 2-notch filter
    float lastAllpassOut = 0.0f;
};
