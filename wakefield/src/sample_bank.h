#ifndef SAMPLE_BANK_H
#define SAMPLE_BANK_H

#include <cstdint>
#include <vector>
#include <string>

// Audio sample data in Q15 format (16-bit signed PCM)
struct SampleData {
    int16_t* samples;           // Q15 sample data (mono)
    uint32_t sampleCount;       // Number of samples
    uint32_t sampleRate;        // Original sample rate (Hz)
    std::string name;           // Sample name (filename without extension)
    std::string path;           // Full file path

    SampleData()
        : samples(nullptr)
        , sampleCount(0)
        , sampleRate(48000)
        , name("")
        , path("") {}

    ~SampleData() {
        if (samples) {
            delete[] samples;
            samples = nullptr;
        }
    }

    // Prevent copying (to avoid double-free)
    SampleData(const SampleData&) = delete;
    SampleData& operator=(const SampleData&) = delete;

    // Allow moving
    SampleData(SampleData&& other) noexcept
        : samples(other.samples)
        , sampleCount(other.sampleCount)
        , sampleRate(other.sampleRate)
        , name(std::move(other.name))
        , path(std::move(other.path)) {
        other.samples = nullptr;
        other.sampleCount = 0;
    }

    SampleData& operator=(SampleData&& other) noexcept {
        if (this != &other) {
            if (samples) delete[] samples;
            samples = other.samples;
            sampleCount = other.sampleCount;
            sampleRate = other.sampleRate;
            name = std::move(other.name);
            path = std::move(other.path);
            other.samples = nullptr;
            other.sampleCount = 0;
        }
        return *this;
    }
};

class SampleBank {
public:
    SampleBank();
    ~SampleBank();

    // Load all WAV files from a directory
    // Returns number of samples loaded
    int loadSamplesFromDirectory(const char* directory);

    // Get sample by index (nullptr if out of range)
    const SampleData* getSample(int index) const;

    // Get number of loaded samples
    int getSampleCount() const { return static_cast<int>(samples.size()); }

    // Get sample name by index (nullptr if out of range)
    const char* getSampleName(int index) const;

    // Find sample index by name (returns -1 if not found)
    int findSampleByName(const char* name) const;

    // Clear all loaded samples
    void clear();

private:
    std::vector<SampleData*> samples;

    // Load a single WAV file
    // Returns true on success
    bool loadWAVFile(const char* filepath);

    // WAV file header structures
    struct WAVHeader {
        char riff[4];           // "RIFF"
        uint32_t fileSize;      // File size - 8
        char wave[4];           // "WAVE"
    };

    struct WAVFormat {
        char fmt[4];            // "fmt "
        uint32_t fmtSize;       // Format chunk size
        uint16_t audioFormat;   // 1 = PCM
        uint16_t numChannels;   // 1 = mono, 2 = stereo
        uint32_t sampleRate;    // Sample rate (Hz)
        uint32_t byteRate;      // Bytes per second
        uint16_t blockAlign;    // Bytes per sample frame
        uint16_t bitsPerSample; // Bits per sample (8, 16, 24, 32)
    };

    struct WAVData {
        char data[4];           // "data"
        uint32_t dataSize;      // Data chunk size
    };

    // Convert various bit depths to Q15 mono
    void convertToQ15Mono(const uint8_t* srcData, uint32_t srcBytes,
                         int16_t* dst, uint32_t dstSamples,
                         int channels, int bitsPerSample);

    // Normalize samples to -3dB peak
    void normalizeSamples(int16_t* samples, uint32_t count);

    // Extract filename without extension
    std::string getFilenameWithoutExtension(const char* filepath) const;
};

#endif // SAMPLE_BANK_H
