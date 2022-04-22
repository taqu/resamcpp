#ifndef INC_RESAMCPP_H_
#define INC_RESAMCPP_H_
/**
@file resamcpp.h
@author t-sakai

# License
This software is ported from the resampy.

## The resampy license

ISC License

Copyright (c) 2016, Brian McFee

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <cstdint>

namespace resamcpp
{
#ifndef RESAMCPP_NULL
#    ifdef __cplusplus
#        if 201103L <= __cplusplus || 1700 <= _MSC_VER
#            define RESAMCPP_NULL nullptr
#        else
#            define RESAMCPP_NULL 0
#        endif
#    else
#        define RESAMCPP_NULL (void*)0
#    endif
#endif

#ifndef RESAMCPP_TYPES
#    define RESAMCPP_TYPES
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef uint64_t size_t;
#endif // RESAMCPP_TYPES

#ifndef RESAMCPP_ASSERT
#    define RESAMCPP_ASSERT(exp) assert(exp)
#endif

#ifdef RESAMCPP_WAV
struct HEAD
{
    u32 id_;
    u32 size_;
};

struct RIFF
{
    static constexpr u32 ID = 0x46464952U;
    static constexpr u32 Format_Wave = 0x45564157U;
    u32 format_;
};

struct FMT
{
    static constexpr u32 ID = 0x20746d66U;
    u16 format_;
    u16 channels_;
    u32 frequency_;
    u32 bytesPerSec_;
    u16 blockAlign_;
    u16 bitsPerSample_;
};

struct DATA
{
    static constexpr u32 ID = 0x61746164U;
};

struct WAVE
{
    bool valid() const;

    FMT format_;
    u64 numSamples_;
    u8* data_;
};

WAVE load(const char* filepath);
bool save(const char* filepath, const WAVE& wave);
void destroy(WAVE& wave);
#endif

class Resampler
{
public:
    static constexpr u32 MaxFilterSize = 257;
    enum class Quality
    {
        Fast,
        Best,
    };

    static Resampler initialize(u32 srcFrequency, u32 dstFrequency, Quality quality = Quality::Best);
    u32 run(u32 channels, u32 dstSamples, s16* dst, u32 srcSamples, const s16* src);
    u32 run_ispc(u32 channels, u32 dstSamples, s16* dst, u32 srcSamples, const s16* src);
private:
    u32 srcFrequency_;
    u32 dstFrequency_;
    f32 sampleRatio_;
    u32 quality_;
};
}
#endif // INC_RESAMCPP_H_

