#include "synthesizer.h"
#include "../core/config.h"

SynthEngine::SynthEngine(double sampleRate)
    : sampleRate(sampleRate), phase(0), time(0), noteTime(0), currentNote(0) {
    melody = { 60, 63, 64, 65, 68, 70 }; // MIDI notes
    noteDuration = 0.1; // seconds per note
}

float SynthEngine::generateSample() {
    if (noteTime >= noteDuration) {
        currentNote = (currentNote + 1);
        noteTime = 0.0;
    }
    if (currentNote >= melody.size())
    {
        return 0.0;
    }
    if (!config::PLAY_STARTUP_JINGLE)
    {
        return 0.0;
    }

    double freq = midiToFreq(melody[currentNote]);
    double sample = round(0.5 + 0.5 * std::sin(phase));

    // Envelope
    double attack = 0.01;
    double release = 0.02;
    double env = 1.0;
    if (noteTime < attack)
        env = noteTime / attack;
    else if (noteTime > noteDuration - release)
        env = (noteDuration - noteTime) / release;

    phase += TAU * freq / sampleRate;
    if (phase > TAU) phase -= TAU;

    noteTime += 1.0 / sampleRate;
    time += 1.0 / sampleRate;

    return static_cast<float>(sample * env * 0.3f); // Volume
}