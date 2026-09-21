#pragma once
#include "BridgeAnalysis.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace QQDeBreathStemExport
{
struct Settings
{
    bool enableFade = true, normalizeBreath = true;
    double fadeInMs = 10.0, fadeOutMs = 10.0;
    double breathTargetDb = -6.0, breathGainDb = 0.0;
    QQDeBreathEqState globalEq;
};
struct Stems { juce::AudioBuffer<float> vocal, breath, noize; };
using Cancel = std::function<bool()>;
using Progress = std::function<void(float)>;
bool render(const juce::AudioBuffer<float>& source, double sampleRate,
            const juce::Array<QQDeBreathBridgeRegion>& regions, const Settings& settings,
            Stems& stems, const Cancel& cancel, const Progress& progress);

struct Request
{
    Settings settings;
    juce::Array<QQDeBreathBridgeRegion> regions;
    juce::AudioBuffer<float> source;
    double sampleRate = 0.0;
    // Opened on the message thread; all reads and destruction belong to the worker.
    std::unique_ptr<juce::AudioFormatReader> reader;
    juce::File directory;
};

// Owns its snapshot. Never touches editor, processor, host or ARA objects.
class Job final : public juce::Thread
{
public:
    explicit Job(Request requestIn);
    ~Job() override;
    void run() override;
    bool finished() const { return done.load(std::memory_order_acquire); }
    float getProgress() const { return progress.load(std::memory_order_relaxed); }
    // Read only after finished() or after joining the worker.
    juce::String status;
    bool succeeded = false;
private:
    void execute();
    Request request;
    std::atomic<bool> done { false };
    std::atomic<float> progress { 0.0f };
};
}
