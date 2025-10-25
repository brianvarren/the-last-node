#ifndef CHAOS_H
#define CHAOS_H

#include <cmath>
#include <atomic>

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

            return output;
        }
    }

    // Get Y output (secondary chaos output)
    float getY() const {
        if (fastMode) {
            return static_cast<float>(y);
        } else {
            if (interpMode == 2) {
                // HOLD
                return static_cast<float>(prevY);
            } else if (interpMode == 1) {
                // CUBIC
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
                // LINEAR
                return static_cast<float>(prevY + (y - prevY) * interpPhase);
            }
        }
    }

    // Setters
    void setChaosParameter(float chaos) {
        u = std::max(0.0, std::min(1.0, static_cast<double>(chaos)));
    }
    void setClockFrequency(float freq) {
        clockFrequency = std::max(0.01, std::min(1000.0, static_cast<double>(freq)));
    }
    void setInterpMode(int mode) { interpMode = std::max(0, std::min(2, mode)); }
    void setFastMode(bool fast) { fastMode = fast; }
    void setSampleRate(float sr) { sampleRate = sr; }
    void reset() { x = 0.1; y = 0.1; t = 0.0; clockPhase = 0.0; interpPhase = 0.0; }

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
};

#endif // CHAOS_H
