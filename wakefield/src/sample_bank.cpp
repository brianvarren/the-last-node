#include "sample_bank.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

SampleBank::SampleBank() {
}

SampleBank::~SampleBank() {
    clear();
}

void SampleBank::clear() {
    for (auto* sample : samples) {
        delete sample;
    }
    samples.clear();
}

int SampleBank::loadSamplesFromDirectory(const char* directory) {
    DIR* dir = opendir(directory);
    if (!dir) {
        std::cerr << "Failed to open samples directory: " << directory << std::endl;
        return 0;
    }

    int loadedCount = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        // Skip directories
        if (entry->d_type == DT_DIR) {
            continue;
        }

        // Check for .wav extension (case-insensitive)
        const char* filename = entry->d_name;
        size_t len = strlen(filename);
        if (len < 5) continue;

        const char* ext = filename + len - 4;
        if (strcasecmp(ext, ".wav") != 0) {
            continue;
        }

        // Build full path
        std::string fullPath = std::string(directory) + "/" + filename;

        // Load the WAV file
        if (loadWAVFile(fullPath.c_str())) {
            loadedCount++;
            std::cout << "Loaded sample: " << filename << std::endl;
        } else {
            std::cerr << "Failed to load sample: " << filename << std::endl;
        }
    }

    closedir(dir);

    std::cout << "Loaded " << loadedCount << " samples from " << directory << std::endl;
    return loadedCount;
}

const SampleData* SampleBank::getSample(int index) const {
    if (index < 0 || index >= static_cast<int>(samples.size())) {
        return nullptr;
    }
    return samples[index];
}

const char* SampleBank::getSampleName(int index) const {
    const SampleData* sample = getSample(index);
    return sample ? sample->name.c_str() : nullptr;
}

int SampleBank::findSampleByName(const char* name) const {
    for (int i = 0; i < static_cast<int>(samples.size()); ++i) {
        if (samples[i]->name == name) {
            return i;
        }
    }
    return -1;
}

std::string SampleBank::getFilenameWithoutExtension(const char* filepath) const {
    std::string path(filepath);

    // Find last path separator
    size_t lastSlash = path.find_last_of("/\\");
    std::string filename = (lastSlash != std::string::npos) ?
                          path.substr(lastSlash + 1) : path;

    // Remove extension
    size_t lastDot = filename.find_last_of('.');
    if (lastDot != std::string::npos) {
        filename = filename.substr(0, lastDot);
    }

    return filename;
}

bool SampleBank::loadWAVFile(const char* filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return false;
    }

    // Read WAV header
    WAVHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));

    if (strncmp(header.riff, "RIFF", 4) != 0 ||
        strncmp(header.wave, "WAVE", 4) != 0) {
        std::cerr << "Not a valid WAV file: " << filepath << std::endl;
        return false;
    }

    // Read format chunk
    WAVFormat format;
    file.read(reinterpret_cast<char*>(&format), sizeof(WAVFormat));

    if (strncmp(format.fmt, "fmt ", 4) != 0) {
        std::cerr << "Invalid WAV format chunk: " << filepath << std::endl;
        return false;
    }

    // Check for PCM format
    if (format.audioFormat != 1) {
        std::cerr << "Only PCM WAV files are supported: " << filepath << std::endl;
        return false;
    }

    // Check for supported bit depths
    if (format.bitsPerSample != 8 && format.bitsPerSample != 16 &&
        format.bitsPerSample != 24 && format.bitsPerSample != 32) {
        std::cerr << "Unsupported bit depth (" << format.bitsPerSample
                  << "): " << filepath << std::endl;
        return false;
    }

    // Skip any extra format bytes
    if (format.fmtSize > 16) {
        file.seekg(format.fmtSize - 16, std::ios::cur);
    }

    // Find data chunk (there may be other chunks before it)
    WAVData dataHeader;
    bool foundData = false;
    while (file.read(reinterpret_cast<char*>(&dataHeader), sizeof(WAVData))) {
        if (strncmp(dataHeader.data, "data", 4) == 0) {
            foundData = true;
            break;
        }
        // Skip this chunk and try next
        file.seekg(dataHeader.dataSize, std::ios::cur);
    }

    if (!foundData) {
        std::cerr << "No data chunk found in WAV file: " << filepath << std::endl;
        return false;
    }

    // Calculate number of sample frames
    uint32_t bytesPerFrame = format.numChannels * (format.bitsPerSample / 8);
    uint32_t numFrames = dataHeader.dataSize / bytesPerFrame;

    if (numFrames == 0) {
        std::cerr << "Empty WAV file: " << filepath << std::endl;
        return false;
    }

    // Allocate buffer for raw data
    std::vector<uint8_t> rawData(dataHeader.dataSize);
    file.read(reinterpret_cast<char*>(rawData.data()), dataHeader.dataSize);

    if (!file) {
        std::cerr << "Failed to read WAV data: " << filepath << std::endl;
        return false;
    }

    // Create sample data structure
    SampleData* sample = new SampleData();
    sample->sampleRate = format.sampleRate;
    sample->sampleCount = numFrames;  // Mono output
    sample->samples = new int16_t[numFrames];
    sample->name = getFilenameWithoutExtension(filepath);
    sample->path = filepath;

    // Convert to Q15 mono
    convertToQ15Mono(rawData.data(), dataHeader.dataSize,
                    sample->samples, numFrames,
                    format.numChannels, format.bitsPerSample);

    // Normalize to -3dB
    normalizeSamples(sample->samples, numFrames);

    // Add to bank
    samples.push_back(sample);

    return true;
}

void SampleBank::convertToQ15Mono(const uint8_t* srcData, uint32_t srcBytes,
                                 int16_t* dst, uint32_t dstSamples,
                                 int channels, int bitsPerSample) {
    const int bytesPerSample = bitsPerSample / 8;
    const int bytesPerFrame = channels * bytesPerSample;

    for (uint32_t frame = 0; frame < dstSamples; ++frame) {
        const uint8_t* frameData = srcData + (frame * bytesPerFrame);
        int32_t accum = 0;

        // Sum all channels
        for (int ch = 0; ch < channels; ++ch) {
            const uint8_t* sampleData = frameData + (ch * bytesPerSample);
            int32_t value = 0;

            switch (bitsPerSample) {
                case 8:
                    // 8-bit is unsigned (0-255), convert to signed
                    value = (static_cast<int32_t>(sampleData[0]) - 128) << 8;
                    break;

                case 16:
                    // 16-bit signed little-endian
                    value = static_cast<int16_t>(
                        sampleData[0] | (sampleData[1] << 8)
                    );
                    break;

                case 24: {
                    // 24-bit signed little-endian
                    int32_t temp = sampleData[0] |
                                  (sampleData[1] << 8) |
                                  (sampleData[2] << 16);
                    // Sign extend
                    if (temp & 0x800000) {
                        temp |= 0xFF000000;
                    }
                    // Shift down to 16-bit
                    value = temp >> 8;
                    break;
                }

                case 32: {
                    // 32-bit signed little-endian
                    int32_t temp = sampleData[0] |
                                  (sampleData[1] << 8) |
                                  (sampleData[2] << 16) |
                                  (sampleData[3] << 24);
                    // Shift down to 16-bit
                    value = temp >> 16;
                    break;
                }
            }

            accum += value;
        }

        // Average channels for mono output
        if (channels > 1) {
            accum /= channels;
        }

        // Clamp to 16-bit range
        if (accum > 32767) accum = 32767;
        if (accum < -32768) accum = -32768;

        dst[frame] = static_cast<int16_t>(accum);
    }
}

void SampleBank::normalizeSamples(int16_t* samples, uint32_t count) {
    if (count == 0) return;

    // Find peak amplitude
    int16_t peak = 0;
    for (uint32_t i = 0; i < count; ++i) {
        int16_t absSample = std::abs(samples[i]);
        if (absSample > peak) {
            peak = absSample;
        }
    }

    if (peak == 0) return;  // Silent sample

    // Normalize to -3dB (0.707 of full scale)
    // Target peak = 32767 * 0.707 = 23170
    const int16_t targetPeak = 23170;

    if (peak < targetPeak) {
        // Scale up
        float scale = static_cast<float>(targetPeak) / static_cast<float>(peak);
        for (uint32_t i = 0; i < count; ++i) {
            int32_t scaled = static_cast<int32_t>(samples[i] * scale);
            samples[i] = static_cast<int16_t>(std::clamp(scaled, -32768, 32767));
        }
    } else if (peak > 32767) {
        // Scale down (prevent clipping)
        float scale = 32767.0f / static_cast<float>(peak);
        for (uint32_t i = 0; i < count; ++i) {
            samples[i] = static_cast<int16_t>(samples[i] * scale);
        }
    }
    // If peak is already in the sweet spot (23170-32767), leave it alone
}
