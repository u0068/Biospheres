#pragma once
#include <vector>
#include <cmath>

const double TAU = 4.0 * acos(0.0);

class SynthEngine {
public:
    SynthEngine(double sampleRate = 44100.0);

    float generateSample();

private:
    std::vector<int> melody;
    double sampleRate;
    double phase;
    double time;
    double noteTime;
    int currentNote;
    double noteDuration;

    double midiToFreq(int midiNote) {
        return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
    }
};
