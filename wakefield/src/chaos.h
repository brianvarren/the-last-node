#ifndef CHAOS_H
#define CHAOS_H

#include <cmath>
#include <atomic>

/**
 * Chaos generator using the Ikeda map
 * The Ikeda map is defined as:
 *   t_{n+1} = u + rho * (x_n * cos(t_n) - y_n * sin(t_n))
 *   x_{n+1} = rho * (x_n * cos(t_n) - y_n * sin(t_n))
 *   y_{n+1} = rho * (x_n * sin(t_n) + y_n * cos(t_n))
 * where rho is typically 0.9 and u controls the chaos parameter
 */
class ChaosGenerator {
public:
    ChaosGenerator()
        : x(0.1)
        , y(0.1)
        , t(0.0)
        , u(0.6)  // Chaos parameter (0.0 = periodic, ~0.6 = chaotic, >0.9 = very chaotic)
        , rho(0.9)
        , sampleRate(48000.0f)
        , clockPhase(0.0)
        , clockFrequency(1.0)  // 1 Hz default
        , cubicInterpolation(false)
        , fastMode(false)
        , prevX(0.1)
        , prevY(0.1)
        , prevT(0.0)
        , interpPhase(0.0) {}

    // Process one sample and return the chaos output (x coordinate, -1 to 1 range typically)
    float process() {
        if (fastMode) {
            // Audio rate: iterate the map every sample
            iterate();
            return static_cast<float>(x);
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

                // Reset interpolation phase
                interpPhase = 0.0;
            }

            // Interpolate between previous and current state
            interpPhase += 1.0 / sampleRate * clockFrequency;
            interpPhase = std::min(interpPhase, 1.0);

            float output;
            if (cubicInterpolation) {
                // Hermite cubic interpolation for smoother transitions
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
                // Linear interpolation
                output = static_cast<float>(prevX + (x - prevX) * interpPhase);
            }

            return output;
        }
    }

    // Get Y output (secondary chaos output)
    float getY() const {
        if (fastMode) {
            return static_cast<float>(y);
        } else {
            if (cubicInterpolation) {
                double mu2 = interpPhase * interpPhase;
                double mu3 = mu2 * interpPhase;
                double m0 = (y - prevY);
                double m1 = (y - prevY);

                return static_cast<float>(
                    (2.0 * mu3 - 3.0 * mu2 + 1.0) * prevY +
                    (mu3 - 2.0 * mu2 + interpPhase) * m0 +
                    (-2.0 * mu3 + 3.0 * mu2) * y +
                    (mu3 - mu2) * m1
                );
            } else {
                return static_cast<float>(prevY + (y - prevY) * interpPhase);
            }
        }
    }

    // Setters
    void setChaosParameter(float chaos) {
        u = std::max(0.0, std::min(2.0, static_cast<double>(chaos)));
    }
    void setClockFrequency(float freq) {
        clockFrequency = std::max(0.01, std::min(1000.0, static_cast<double>(freq)));
    }
    void setCubicInterpolation(bool enable) { cubicInterpolation = enable; }
    void setFastMode(bool fast) { fastMode = fast; }
    void setSampleRate(float sr) { sampleRate = sr; }
    void reset() { x = 0.1; y = 0.1; t = 0.0; clockPhase = 0.0; interpPhase = 0.0; }

    // Getters
    float getChaosParameter() const { return static_cast<float>(u); }
    float getClockFrequency() const { return static_cast<float>(clockFrequency); }
    bool getCubicInterpolation() const { return cubicInterpolation; }
    bool getFastMode() const { return fastMode; }

private:
    void iterate() {
        // Ikeda map iteration
        t = u + rho * (x * std::cos(t) - y * std::sin(t));

        double newX = rho * (x * std::cos(t) - y * std::sin(t));
        double newY = rho * (x * std::sin(t) + y * std::cos(t));

        x = newX;
        y = newY;
    }

    // Ikeda map state
    double x, y, t;
    double u;    // Chaos parameter
    double rho;  // Typically 0.9

    // Clocking
    float sampleRate;
    double clockPhase;
    double clockFrequency;  // Hz

    // Interpolation
    bool cubicInterpolation;
    bool fastMode;
    double prevX, prevY, prevT;
    double interpPhase;
};

#endif // CHAOS_H
