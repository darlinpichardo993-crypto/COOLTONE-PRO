#pragma once
// COOLTONE by ORYMY - YMY RECORDS
// WAV writer / reader used by EXPORT AUDIO and DRAG TO PLAYLIST.
// Writes 16-bit PCM, 24-bit PCM or 32-bit IEEE float, mono or multichannel (interleaved).
#include <cstddef>
#include <string>
#include <vector>

namespace cooltone
{
enum class WavFormat { PCM16, PCM24, Float32 };

struct WavData
{
    int sampleRate = 44100;
    int channels = 1;
    WavFormat format = WavFormat::Float32;
    std::vector<float> samples;   // interleaved
    std::size_t frames() const { return channels > 0 ? samples.size() / static_cast<std::size_t> (channels) : 0; }
};

bool writeWav (const std::string& path, const WavData& data, std::string* error = nullptr);

// Fully parses and validates the file (chunk sizes vs. real file size, format, data length).
bool readWav (const std::string& path, WavData& out, std::string* error = nullptr);
} // namespace cooltone
