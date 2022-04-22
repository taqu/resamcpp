/**
@file resamcpp.ispc
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
int32 minimum(int32 x0, int32 x1)
{
    return x0<x1? x0 : x1;
}

export uniform uint32 resample(
    uniform uint32 channels,
    uniform float sampleRatio,
    uniform uint32 oversample,
    uniform uint32 dstSamples,
    int16* uniform dst,
    uniform uint32 srcSamples,
    const int16* uniform src,
    const float uniform * uniform filter,
    const float uniform * uniform filterDelta)
{
    float scale = min(1.0f, sampleRatio);
    float timeStep = 1.0f / sampleRatio;
    int32 indexStep = (int32)(scale * oversample);
    float invIndexStep = 1.0f/indexStep;
    static const int32 taps = 257;

    foreach(i=0 ... dstSamples){
        float time = timeStep * i;
        // Grab the top bits as an index to the input buffer
        int32 n = (int32)time;

        // Grab the fractional component ot the time index
        float frac = scale * (time - n);

        // Offset into the filter
        float indexFrac = frac * oversample;
        int32 offset = (int32)indexFrac;

        // Interpolation factor
        float eta = indexFrac - offset;

        // Compute the left wing of the filter response
        float values[2] = {0.0f, 0.0f};
        int32 maxi = minimum(n + 1, (taps - offset) * invIndexStep);
        for(int32 j = 0; j < maxi; ++j) {
            assert((offset + j * indexStep) < taps);
            float weight = (filter[offset + j * indexStep] + eta * filterDelta[offset + j * indexStep]);
            for(uint32 k = 0; k < channels; ++k) {
                assert((i * channels + k) < (dstSamples*channels));
                assert(((n - j) * channels + k) < (srcSamples*channels));
                values[k] += weight * src[(n - j) * channels + k];
            }
        }
        // Invert P
        frac = scale - frac;
        indexFrac = frac * oversample;
        offset = (int32)indexFrac;

        // Offset into the filter
        eta = indexFrac - offset;

        // Compute the right wing of the filter response
        maxi = minimum(srcSamples - n - 1, (taps - offset) * invIndexStep);
        for(uint32 j = 0; j < maxi; ++j) {
            assert((offset + j * indexStep) < taps);
            float weight = (filter[offset + j * indexStep] + eta * filterDelta[offset + j * indexStep]);
            for(uint32 k = 0; k < channels; ++k) {
                assert((i * channels + k) < (dstSamples*channels));
                assert(((n + j + 1) * channels + k) < (srcSamples*channels));
                values[k] += weight * src[(n + j + 1) * channels + k];
            }
        }
        for(uint32 j = 0; j < channels; ++j) {
            int32 x = (int32)(values[j]*scale);
            dst[i * channels + j] = (int16)clamp(x, -32768, 32767);
        }
    }
    return dstSamples;
}
