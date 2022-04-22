#include "resamcpp.h"
#include <cstdlib>
#include <chrono>
#include <iostream>

int main(void)
{
    resamcpp::WAVE wave = resamcpp::load("AirOnTheGString.wav");

    resamcpp::WAVE wave2 = wave;
    {
        wave2.format_.frequency_ = static_cast<resamcpp::u32>(44100*2.0);//48000;
        wave2.format_.bytesPerSec_ = wave2.format_.frequency_ * wave2.format_.blockAlign_;

        resamcpp::u64 numSamples = static_cast<resamcpp::u64>(static_cast<resamcpp::f64>(wave2.format_.frequency_) / wave.format_.frequency_ * wave.numSamples_);

        wave2.numSamples_ = numSamples;
        wave2.data_ = reinterpret_cast<resamcpp::u8*>(::malloc(numSamples * wave2.format_.blockAlign_));
    }

    std::chrono::high_resolution_clock::time_point start;
    std::chrono::high_resolution_clock::duration duration;
    resamcpp::Resampler resampler = resamcpp::Resampler::initialize(wave.format_.frequency_, wave2.format_.frequency_);
    //
    start = std::chrono::high_resolution_clock::now();
    resampler.run(wave2.format_.channels_, static_cast<resamcpp::u32>(wave2.numSamples_), reinterpret_cast<resamcpp::s16*>(wave2.data_), static_cast<resamcpp::u32>(wave.numSamples_), reinterpret_cast<resamcpp::s16*>(wave.data_));
    duration = std::chrono::high_resolution_clock::now() - start;
    std::cout << "samples: " << wave.numSamples_ << ", microsec: " << std::chrono::duration_cast<std::chrono::microseconds>(duration).count() << std::endl;
    resamcpp::save("music00_1.wav", wave2);

    //
    start = std::chrono::high_resolution_clock::now();
    resampler.run_ispc(wave2.format_.channels_, static_cast<resamcpp::u32>(wave2.numSamples_), reinterpret_cast<resamcpp::s16*>(wave2.data_), static_cast<resamcpp::u32>(wave.numSamples_), reinterpret_cast<resamcpp::s16*>(wave.data_));
    duration = std::chrono::high_resolution_clock::now() - start;
    std::cout << "samples: " << wave.numSamples_ << ", microsec: " << std::chrono::duration_cast<std::chrono::microseconds>(duration).count() << std::endl;
    resamcpp::save("music00_2.wav", wave2);

    resamcpp::destroy(wave2);
    resamcpp::destroy(wave);
    return 0;
}
