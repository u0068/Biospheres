#pragma once
#include "../../third_party/include/miniaudio.h"
#include "synthesizer.h"
#include <iostream>

class AudioEngine {
public:
    AudioEngine()
        : synth(44100.0), sampleRate(44100.0) {
        config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = 2;
        config.sampleRate = (ma_uint32)sampleRate;
        config.dataCallback = AudioEngine::dataCallback;
        config.pUserData = this;
    }

    ~AudioEngine()
    {
        shutdown();
    }

    bool init() {
        if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) {
            std::cerr << "Failed to initialize audio device.\n";
            return false;
        }
        return true;
    }

    bool start() {
        if (ma_device_start(&device) != MA_SUCCESS) {
            std::cerr << "Failed to start audio device.\n";
            return false;
        }
        return true;
    }

    void shutdown() {
        ma_device_uninit(&device);
    }

private:
    ma_device_config config;
    ma_device device;
    SynthEngine synth;
    double sampleRate;

    static void dataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
        auto* engine = static_cast<AudioEngine*>(device->pUserData);
        float* out = static_cast<float*>(output);
        for (ma_uint32 i = 0; i < frameCount; ++i) {
            float sample = engine->synth.generateSample();
            out[i * 2 + 0] = sample; // left
            out[i * 2 + 1] = sample; // right
        }
    }
};
