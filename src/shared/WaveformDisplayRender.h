#pragma once
#include "StemExport.h"
#include <mutex>
namespace QQDeBreathWaveformDisplay
{
struct Request
{
    std::shared_ptr<const juce::AudioBuffer<float>> source;
    double sampleRate = 0;
    juce::Array<QQDeBreathBridgeRegion> regions;
    QQDeBreathStemExport::Settings settings;
    uint64_t revision = 0;
};
struct Result
{
    juce::AudioBuffer<float> scalable, fixed;
    bool normalised = false;
    uint64_t revision = 0;
};
bool render(const Request&, Result&, const QQDeBreathStemExport::Cancel&);
class Worker final : public juce::Thread
{
public:
    Worker();
    ~Worker() override;
    uint64_t submit(Request);
    void cancel();
    std::unique_ptr<Result> takeResult();
    void run() override;
private:
    std::mutex mutex;
    std::unique_ptr<Request> pending;
    std::unique_ptr<Result> ready;
    std::atomic<uint64_t> revision { 0 };
};
}
