#include "WavIO.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>

namespace cooltone
{
namespace
{
void put16 (std::vector<unsigned char>& b, std::uint32_t v) { b.push_back (v & 0xFF); b.push_back ((v >> 8) & 0xFF); }
void put32 (std::vector<unsigned char>& b, std::uint32_t v) { put16 (b, v & 0xFFFF); put16 (b, v >> 16); }
void putTag (std::vector<unsigned char>& b, const char* t) { for (int i = 0; i < 4; ++i) b.push_back (static_cast<unsigned char> (t[i])); }
std::uint32_t get16 (const unsigned char* p) { return p[0] | (static_cast<std::uint32_t> (p[1]) << 8); }
std::uint32_t get32 (const unsigned char* p) { return get16 (p) | (get16 (p + 2) << 16); }
bool fail (std::string* e, const char* m) { if (e) *e = m; return false; }

std::int32_t quantise (float v, int bits)
{
    const double full = static_cast<double> (1u << (bits - 1)); // 32768 or 8388608
    double s = std::floor (static_cast<double> (v) * full + 0.5);
    if (s > full - 1.0) s = full - 1.0;
    if (s < -full) s = -full;
    return static_cast<std::int32_t> (s);
}
} // namespace

bool writeWav (const std::string& path, const WavData& d, std::string* err)
{
    if (d.channels < 1 || d.sampleRate < 1) return fail (err, "invalid channels / sample rate");
    if (d.samples.size() % static_cast<std::size_t> (d.channels) != 0) return fail (err, "sample count not a multiple of channels");

    const bool isFloat = d.format == WavFormat::Float32;
    const int bits = d.format == WavFormat::PCM16 ? 16 : d.format == WavFormat::PCM24 ? 24 : 32;
    const std::uint32_t bytesPer = static_cast<std::uint32_t> (bits / 8);
    const std::uint64_t dataBytes64 = static_cast<std::uint64_t> (d.samples.size()) * bytesPer;
    if (dataBytes64 > 0xFFFFFF00ull) return fail (err, "audio too long for a RIFF/WAV file");
    const std::uint32_t dataBytes = static_cast<std::uint32_t> (dataBytes64);
    const std::uint32_t pad = dataBytes & 1u;
    const std::uint32_t fmtSize = isFloat ? 18u : 16u;
    const std::uint32_t factSize = isFloat ? 12u : 0u;

    std::vector<unsigned char> b;
    b.reserve (64 + dataBytes);
    putTag (b, "RIFF"); put32 (b, 4 + (8 + fmtSize) + factSize + 8 + dataBytes + pad);
    putTag (b, "WAVE");
    putTag (b, "fmt "); put32 (b, fmtSize);
    put16 (b, isFloat ? 3u : 1u); put16 (b, static_cast<std::uint32_t> (d.channels));
    put32 (b, static_cast<std::uint32_t> (d.sampleRate));
    put32 (b, static_cast<std::uint32_t> (d.sampleRate) * static_cast<std::uint32_t> (d.channels) * bytesPer);
    put16 (b, static_cast<std::uint32_t> (d.channels) * bytesPer); put16 (b, static_cast<std::uint32_t> (bits));
    if (isFloat) { put16 (b, 0); putTag (b, "fact"); put32 (b, 4); put32 (b, static_cast<std::uint32_t> (d.frames())); }
    putTag (b, "data"); put32 (b, dataBytes);

    for (float v : d.samples)
    {
        if (isFloat) { std::uint32_t u; std::memcpy (&u, &v, 4); put32 (b, u); }
        else
        {
            const std::int32_t s = quantise (v, bits);
            const std::uint32_t u = static_cast<std::uint32_t> (s);
            b.push_back (u & 0xFF); b.push_back ((u >> 8) & 0xFF);
            if (bits == 24) b.push_back ((u >> 16) & 0xFF);
        }
    }
    if (pad) b.push_back (0);

    std::ofstream f (path, std::ios::binary | std::ios::trunc);
    if (! f) return fail (err, "cannot open file for writing");
    f.write (reinterpret_cast<const char*> (b.data()), static_cast<std::streamsize> (b.size()));
    f.flush();
    if (! f) return fail (err, "write failed");
    return true;
}

bool readWav (const std::string& path, WavData& out, std::string* err)
{
    std::ifstream f (path, std::ios::binary | std::ios::ate);
    if (! f) return fail (err, "cannot open file");
    const std::streamsize size = f.tellg();
    if (size < 44) return fail (err, "file too small");
    f.seekg (0);
    std::vector<unsigned char> b (static_cast<std::size_t> (size));
    f.read (reinterpret_cast<char*> (b.data()), size);
    if (! f) return fail (err, "read failed");

    if (std::memcmp (b.data(), "RIFF", 4) != 0 || std::memcmp (b.data() + 8, "WAVE", 4) != 0) return fail (err, "not a RIFF/WAVE file");
    if (static_cast<std::uint64_t> (get32 (b.data() + 4)) + 8 > b.size()) return fail (err, "RIFF size exceeds file size (truncated)");

    bool haveFmt = false;
    std::uint32_t tag = 0, ch = 0, sr = 0, bits = 0;
    std::size_t pos = 12;
    while (pos + 8 <= b.size())
    {
        const std::uint32_t sz = get32 (b.data() + pos + 4);
        const unsigned char* body = b.data() + pos + 8;
        if (pos + 8 + static_cast<std::uint64_t> (sz) > b.size()) return fail (err, "chunk exceeds file size");
        if (std::memcmp (b.data() + pos, "fmt ", 4) == 0 && sz >= 16)
        {
            tag = get16 (body); ch = get16 (body + 2); sr = get32 (body + 4); bits = get16 (body + 14);
            if (tag == 0xFFFE && sz >= 26) tag = get16 (body + 24);
            haveFmt = true;
        }
        else if (std::memcmp (b.data() + pos, "data", 4) == 0)
        {
            if (! haveFmt) return fail (err, "data chunk before fmt chunk");
            if (ch < 1 || sr < 1) return fail (err, "invalid fmt");
            const bool okPcm = tag == 1 && (bits == 16 || bits == 24);
            const bool okFlt = tag == 3 && bits == 32;
            if (! okPcm && ! okFlt) return fail (err, "unsupported WAV format");
            const std::uint32_t bytesPer = bits / 8;
            if (sz % (bytesPer * ch) != 0) return fail (err, "data size not a whole number of frames");
            out.sampleRate = static_cast<int> (sr); out.channels = static_cast<int> (ch);
            out.format = okFlt ? WavFormat::Float32 : bits == 16 ? WavFormat::PCM16 : WavFormat::PCM24;
            const std::size_t n = sz / bytesPer;
            out.samples.assign (n, 0.f);
            for (std::size_t i = 0; i < n; ++i)
            {
                const unsigned char* p = body + i * bytesPer;
                if (okFlt) { std::uint32_t u = get32 (p); float v; std::memcpy (&v, &u, 4); out.samples[i] = v; }
                else if (bits == 16) out.samples[i] = static_cast<float> (static_cast<std::int16_t> (get16 (p))) / 32768.0f;
                else
                {
                    std::int32_t s = static_cast<std::int32_t> (p[0] | (p[1] << 8) | (p[2] << 16));
                    if (s & 0x800000) s -= 0x1000000;
                    out.samples[i] = static_cast<float> (s) / 8388608.0f;
                }
            }
            return true;
        }
        pos += 8 + sz + (sz & 1u);
    }
    return fail (err, "no data chunk");
}
} // namespace cooltone
