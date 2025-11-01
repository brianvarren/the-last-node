#ifndef CHAOS_H
#define CHAOS_H

#include <algorithm>
#include <atomic>
#include <cmath>

/**
 * Chaos generator using the Ikeda map
 * The Ikeda map is defined as:
 *   t = 0.4 - 6/(1 + x_n² + y_n²)
 *   x_{n+1} = 1 + u*(x_n*cos(t) - y_n*sin(t))
 *   y_{n+1} = u*(x_n*sin(t) + y_n*cos(t))
 * where u controls the chaos parameter (typically 0.6-0.99 for chaotic behavior)
 */
class ChaosGenerator {
public:
    ChaosGenerator()
        : x(0.1)
        , y(0.1)
        , t(0.0)
        , u(0.918)  // Chaos parameter (0.6-0.99 for chaotic behavior, 0.918 is typical)
        , sampleRate(48000.0f)
        , clockPhase(0.0)
        , clockFrequency(1.0)  // 1 Hz default
        , interpMode(0)  // 0=LINEAR, 1=CUBIC, 2=HOLD
        , fastMode(false)
        , prevX(0.1)
        , prevY(0.1)
        , prevT(0.0)
        , interpPhase(0.0)
        , fastAccumulator(0.0)
        , lastValidX(0.1)
        , lastValidY(0.1) {}

    // Process one sample and return the chaos output (x coordinate, -1 to 1 range typically)
    float process() {
        if (fastMode) {
            // Audio rate: iterate according to clockFrequency using an accumulator
            // This ties the effective iteration rate to the user clock in FAST mode
            double inc = clockFrequency / sampleRate;
            fastAccumulator += inc;
            // Iterate as many whole steps as accumulated (can be >1)
            int steps = static_cast<int>(fastAccumulator);
            if (steps > 0) {
                for (int i = 0; i < steps; ++i) {
                    iterate();
                    sanitizeState();
                }
                fastAccumulator -= steps;
            }
            return sanitizeOutput(x, lastValidX);
        } else {
            // Slow/clocked mode: iterate when clock triggers, interpolate between
            clockPhase += clockFrequency / sampleRate;

            if (clockPhase >= 1.0) {
                clockPhase -= 1.0;

                // Save previous state for interpolation
                prevX = x;
                prevY = y;
                prevT = t;

                // Iterate to next state
                iterate();
                sanitizeState();

                // Reset interpolation phase
                interpPhase = 0.0;
            }

            // Interpolate between previous and current state
            interpPhase += 1.0 / sampleRate * clockFrequency;
            interpPhase = std::min(interpPhase, 1.0);

            float output;
            if (interpMode == 2) {
                // HOLD: zero-order hold (step function)
                output = static_cast<float>(prevX);
            } else if (interpMode == 1) {
                // CUBIC: Hermite cubic interpolation for smoother transitions
                double mu2 = interpPhase * interpPhase;
                double mu3 = mu2 * interpPhase;
                double m0 = (x - prevX);  // Tangent at start
                double m1 = (x - prevX);  // Tangent at end

                output = static_cast<float>(
                    (2.0 * mu3 - 3.0 * mu2 + 1.0) * prevX +
                    (mu3 - 2.0 * mu2 + interpPhase) * m0 +
                    (-2.0 * mu3 + 3.0 * mu2) * x +
                    (mu3 - mu2) * m1
                );
            } else {
                // LINEAR: linear interpolation (default)
                output = static_cast<float>(prevX + (x - prevX) * interpPhase);
            }

            return sanitizeOutput(output, lastValidX);
        }
    }

    // Get Y output (secondary chaos output)
    float getY() const {
        double output;
        if (fastMode) {
            output = y;
        } else {
            if (interpMode == 2) {
                // HOLD
                output = prevY;
            } else if (interpMode == 1) {
                // CUBIC
                double mu2 = interpPhase * interpPhase;
                double mu3 = mu2 * interpPhase;
                double m0 = (y - prevY);
                double m1 = (y - prevY);

                output =
                    (2.0 * mu3 - 3.0 * mu2 + 1.0) * prevY +
                    (mu3 - 2.0 * mu2 + interpPhase) * m0 +
                    (-2.0 * mu3 + 3.0 * mu2) * y +
                    (mu3 - mu2) * m1;
            } else {
                // LINEAR
                output = prevY + (y - prevY) * interpPhase;
            }
        }

        return sanitizeOutput(output, lastValidY);
    }

    // Setters
    void setChaosParameter(float chaos) {
        u = std::max(0.0, std::min(1.0, static_cast<double>(chaos)));
    }
    void setClockFrequency(float freq) {
        // Allow full range from 0.0001Hz to 20kHz regardless of fast/slow mode
        // Fast mode: per-sample iteration, Slow mode: per-block iteration
        clockFrequency = std::max(0.0001, std::min(20000.0, static_cast<double>(freq)));
    }
    void setInterpMode(int mode) { interpMode = std::max(0, std::min(2, mode)); }
    void setFastMode(bool fast) { fastMode = fast; }
    void setSampleRate(float sr) { sampleRate = sr; }
    void reset() {
        x = 0.1;
        y = 0.1;
        t = 0.0;
        clockPhase = 0.0;
        interpPhase = 0.0;
        lastValidX = x;
        lastValidY = y;
    }

    // Getters
    float getChaosParameter() const { return static_cast<float>(u); }
    float getClockFrequency() const { return static_cast<float>(clockFrequency); }
    int getInterpMode() const { return interpMode; }
    bool getFastMode() const { return fastMode; }

private:
    void iterate() {
        // Ikeda map iteration (standard formulation)
        // t = 0.4 - 6/(1 + x² + y²)
        t = 0.4 - 6.0 / (1.0 + x * x + y * y);

        // x_{n+1} = 1 + u*(x_n*cos(t) - y_n*sin(t))
        // y_{n+1} = u*(x_n*sin(t) + y_n*cos(t))
        double cosT = std::cos(t);
        double sinT = std::sin(t);

        double newX = 1.0 + u * (x * cosT - y * sinT);
        double newY = u * (x * sinT + y * cosT);

        x = newX;
        y = newY;
    }

    // Ikeda map state
    double x, y, t;
    double u;    // Chaos parameter (0.6-0.99 for chaotic behavior)

    // Clocking
    float sampleRate;
    double clockPhase;
    double clockFrequency;  // Hz

    // Interpolation
    int interpMode;  // 0=LINEAR, 1=CUBIC, 2=HOLD
    bool fastMode;
    double prevX, prevY, prevT;
    double interpPhase;
    double fastAccumulator; // For FAST mode clocked iterations per sample
    mutable double lastValidX;
    mutable double lastValidY;

    static constexpr double kMaxAbsValue = 8.0;

    void sanitizeState() {
        if (!std::isfinite(x)) {
            x = lastValidX;
        }
        if (!std::isfinite(y)) {
            y = lastValidY;
        }
        x = std::clamp(x, -kMaxAbsValue, kMaxAbsValue);
        y = std::clamp(y, -kMaxAbsValue, kMaxAbsValue);
        lastValidX = x;
        lastValidY = y;
    }

    float sanitizeOutput(double value, double& lastValid) const {
        if (!std::isfinite(value)) {
            return static_cast<float>(lastValid);
        }
        double clamped = std::clamp(value, -kMaxAbsValue, kMaxAbsValue);
        lastValid = clamped;
        return static_cast<float>(clamped);
    }
};

#endif // CHAOS_H
