#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <thread>

class QQDeBreathMonitorRoutingProbe
{
public:
    static void configurePreview(QQDeBreathAudioProcessor& processor, const juce::String& regionType)
    {
        {
            const juce::ScopedLock lock(processor.recordedBufferLock);
            processor.recordedBuffer.setSize(2, 512);
            processor.recordedBuffer.clear();
            for (auto channel = 0; channel < processor.recordedBuffer.getNumChannels(); ++channel)
                processor.recordedBuffer.applyGain(channel, 0, 512, 0.0f);
            for (auto channel = 0; channel < processor.recordedBuffer.getNumChannels(); ++channel)
                juce::FloatVectorOperations::fill(processor.recordedBuffer.getWritePointer(channel), 0.5f, 512);
            processor.recordedLengthSamples = 512;
            processor.recordedSampleRate = 48000.0;
            processor.recordingStartTimelineSeconds = 0.0;
        }

        QQDeBreathBridgeAnalysisResult result;
        result.hasResult = true;
        result.succeeded = true;
        result.sampleRate = 48000;
        result.channels = 2;
        result.numSamples = 512;
        result.durationSeconds = 512.0 / 48000.0;
        QQDeBreathBridgeRegion region;
        region.type = regionType;
        region.startSample = 0;
        region.endSample = 512;
        region.startTime = 0.0;
        region.endTime = result.durationSeconds;
        result.regions.add(region);
        result.breathCount = regionType.equalsIgnoreCase("Breath") ? 1 : 0;
        result.noizeCount = regionType.equalsIgnoreCase("Noize") ? 1 : 0;

        {
            const juce::ScopedLock lock(processor.analysisLock);
            processor.analysisResult = result;
            processor.analysisRegionPeakCache.clear();
            processor.analysisRegionPeakCache.add(0.5);
        }
        processor.recordedPreviewReady.store(true, std::memory_order_release);
        processor.analysisPreviewReady.store(true, std::memory_order_release);
        processor.clearInternalPreviewPosition();
    }

    static juce::CriticalSection& recordedLock(QQDeBreathAudioProcessor& processor) { return processor.recordedBufferLock; }
    static juce::CriticalSection& analysisLock(QQDeBreathAudioProcessor& processor) { return processor.analysisLock; }
};

namespace
{
class PlayingTestPlayHead final : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo position;
        position.setIsPlaying(true);
        position.setTimeInSamples(0);
        position.setTimeInSeconds(0.0);
        return position;
    }
};
bool setParameter(juce::AudioProcessorValueTreeState& state, const char* id, float value)
{
    auto* parameter = state.getParameter(id);
    if (parameter == nullptr)
        return false;

    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    return true;
}

bool near(double actual, double expected, double tolerance = 0.001)
{
    return std::abs(actual - expected) <= tolerance;
}

float renderPreviewMagnitude(QQDeBreathAudioProcessor& processor)
{
    juce::AudioBuffer<float> buffer(2, 128);
    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill(buffer.getWritePointer(channel), 0.75f, buffer.getNumSamples());
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    auto magnitude = 0.0f;
    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
        magnitude = juce::jmax(magnitude, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
    return magnitude;
}

bool runMonitorRoutingProbe(QQDeBreathAudioProcessor& processor)
{
    PlayingTestPlayHead playHead;
    processor.setPlayHead(&playHead);
    processor.prepareToPlay(48000.0, 128);
    setParameter(processor.parameters, QQDeBreath::ParamIDs::bypass, 0.0f);
    setParameter(processor.parameters, QQDeBreath::ParamIDs::enableFade, 0.0f);
    setParameter(processor.parameters, QQDeBreath::ParamIDs::normalizeBreath, 0.0f);
    setParameter(processor.parameters, QQDeBreath::ParamIDs::breathGainDb, 0.0f);

    QQDeBreathMonitorRoutingProbe::configurePreview(processor, "Breath");
    setParameter(processor.parameters, QQDeBreath::ParamIDs::monitorVoice, 0.0f);
    setParameter(processor.parameters, QQDeBreath::ParamIDs::monitorBreath, 0.0f);
    setParameter(processor.parameters, QQDeBreath::ParamIDs::monitorNoize, 1.0f);
    const auto breathOff = renderPreviewMagnitude(processor);
    setParameter(processor.parameters, QQDeBreath::ParamIDs::monitorBreath, 1.0f);
    const auto breathOn = renderPreviewMagnitude(processor);

    QQDeBreathMonitorRoutingProbe::configurePreview(processor, "Noize");
    setParameter(processor.parameters, QQDeBreath::ParamIDs::monitorBreath, 0.0f);
    setParameter(processor.parameters, QQDeBreath::ParamIDs::monitorNoize, 0.0f);
    const auto noizeOff = renderPreviewMagnitude(processor);
    setParameter(processor.parameters, QQDeBreath::ParamIDs::monitorNoize, 1.0f);
    const auto noizeOn = renderPreviewMagnitude(processor);

    setParameter(processor.parameters, QQDeBreath::ParamIDs::monitorVoice, 0.0f);
    setParameter(processor.parameters, QQDeBreath::ParamIDs::monitorBreath, 0.0f);
    setParameter(processor.parameters, QQDeBreath::ParamIDs::monitorNoize, 0.0f);
    const auto allOff = renderPreviewMagnitude(processor);

    auto renderWhileLockHeld = [&](juce::CriticalSection& lockToHold)
    {
        juce::WaitableEvent locked;
        juce::WaitableEvent release;
        std::thread holder([&]
        {
            const juce::ScopedLock lock(lockToHold);
            locked.signal();
            release.wait(2000);
        });
        const auto acquired = locked.wait(2000);
        const auto magnitude = acquired ? renderPreviewMagnitude(processor) : 1.0f;
        release.signal();
        holder.join();
        return magnitude;
    };

    const auto recordingLockFallback = renderWhileLockHeld(QQDeBreathMonitorRoutingProbe::recordedLock(processor));
    const auto analysisLockFallback = renderWhileLockHeld(QQDeBreathMonitorRoutingProbe::analysisLock(processor));
    processor.setPlayHead(nullptr);

    const auto passed = breathOff < 1.0e-6f
                     && breathOn > 0.1f
                     && noizeOff < 1.0e-6f
                     && noizeOn > 0.1f
                     && allOff < 1.0e-6f
                     && recordingLockFallback < 1.0e-6f
                     && analysisLockFallback < 1.0e-6f;
    if (! passed)
    {
        std::cerr << "FAIL: monitor routing leaked audio"
                  << " breathOff=" << breathOff << " breathOn=" << breathOn
                  << " noizeOff=" << noizeOff << " noizeOn=" << noizeOn
                  << " allOff=" << allOff
                  << " recordingLock=" << recordingLockFallback
                  << " analysisLock=" << analysisLockFallback << "\n";
    }
    return passed;
}
} // namespace

#if JUCE_WINDOWS
int wmain()
#else
int main()
#endif
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    QQDeBreathAudioProcessor processor;

    auto firstEditor = std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
    if (firstEditor == nullptr)
    {
        std::cerr << "FAIL: first editor was not created\n";
        return 1;
    }

    if (! setParameter(processor.parameters, QQDeBreath::ParamIDs::normalizeBreath, 1.0f)
        || ! setParameter(processor.parameters, QQDeBreath::ParamIDs::breathTargetDb, -37.3f)
        || ! setParameter(processor.parameters, QQDeBreath::ParamIDs::breathGainDb, 7.2f))
    {
        std::cerr << "FAIL: required parameter was not found\n";
        return 1;
    }

    QQDeBreathEqState expectedEq;
    expectedEq.enabled = true;
    expectedEq.highPassEnabled = true;
    expectedEq.highPassHz = 173.0;
    expectedEq.highPassSlopeDbPerOct = 24;
    expectedEq.bands[0].enabled = true;
    expectedEq.bands[0].frequencyHz = 1840.0;
    expectedEq.bands[0].gainDb = -4.7;
    expectedEq.bands[0].q = 1.8;
    processor.setBreathEqState(expectedEq);

    QQDeBreathEqState expectedSelectedEq;
    expectedSelectedEq.enabled = true;
    expectedSelectedEq.lowPassEnabled = true;
    expectedSelectedEq.lowPassHz = 9230.0;
    expectedSelectedEq.lowPassSlopeDbPerOct = 36;
    expectedSelectedEq.bands[1].enabled = true;
    expectedSelectedEq.bands[1].frequencyHz = 3270.0;
    expectedSelectedEq.bands[1].gainDb = 3.4;
    expectedSelectedEq.bands[1].q = 2.1;

    QQDeBreathBridgeAnalysisResult expectedAnalysis;
    expectedAnalysis.hasResult = true;
    expectedAnalysis.succeeded = true;
    expectedAnalysis.sampleRate = 48000;
    expectedAnalysis.channels = 1;
    expectedAnalysis.numSamples = 96000;
    expectedAnalysis.durationSeconds = 2.0;
    expectedAnalysis.breathCount = 1;
    QQDeBreathBridgeRegion expectedRegion;
    expectedRegion.type = "Breath";
    expectedRegion.startSample = 12000;
    expectedRegion.endSample = 24000;
    expectedRegion.startTime = 0.25;
    expectedRegion.endTime = 0.5;
    expectedRegion.gainDb = -8.6;
    expectedRegion.eqState = expectedSelectedEq;
    expectedAnalysis.regions.add(expectedRegion);
    processor.setAnalysisResult(expectedAnalysis);

    firstEditor.reset();
    auto reopenedEditor = std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
    if (reopenedEditor == nullptr)
    {
        std::cerr << "FAIL: reopened editor was not created\n";
        return 1;
    }

    const auto norm = processor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::normalizeBreath)->load();
    const auto target = processor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::breathTargetDb)->load();
    const auto gain = processor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::breathGainDb)->load();
    const auto actualEq = processor.getBreathEqState();
    const auto actualAnalysis = processor.getAnalysisResult();
    const auto selectedRegionPreserved = actualAnalysis.regions.size() == 1
                                      && near(actualAnalysis.regions[0].gainDb, -8.6)
                                      && actualAnalysis.regions[0].eqState.enabled
                                      && actualAnalysis.regions[0].eqState.lowPassEnabled
                                      && near(actualAnalysis.regions[0].eqState.lowPassHz, 9230.0)
                                      && actualAnalysis.regions[0].eqState.lowPassSlopeDbPerOct == 36
                                      && actualAnalysis.regions[0].eqState.bands[1].enabled
                                      && near(actualAnalysis.regions[0].eqState.bands[1].frequencyHz, 3270.0)
                                      && near(actualAnalysis.regions[0].eqState.bands[1].gainDb, 3.4)
                                      && near(actualAnalysis.regions[0].eqState.bands[1].q, 2.1);

    const auto passed = norm >= 0.5f
                     && near(target, -37.3)
                     && near(gain, 7.2)
                     && actualEq.enabled
                     && actualEq.highPassEnabled
                     && near(actualEq.highPassHz, 173.0)
                     && actualEq.highPassSlopeDbPerOct == 24
                     && actualEq.bands[0].enabled
                     && near(actualEq.bands[0].frequencyHz, 1840.0)
                     && near(actualEq.bands[0].gainDb, -4.7)
                     && near(actualEq.bands[0].q, 1.8)
                     && selectedRegionPreserved;

    if (! passed)
    {
        std::cerr << "FAIL: reopening the editor changed live instance state\n"
                  << "  norm=" << norm << " target=" << target << " gain=" << gain << "\n"
                  << "  eqEnabled=" << actualEq.enabled << " hp=" << actualEq.highPassEnabled
                  << " hpHz=" << actualEq.highPassHz << " bandGain=" << actualEq.bands[0].gainDb << "\n";
        return 1;
    }

    if (! runMonitorRoutingProbe(processor))
        return 1;

    QQDeBreathAudioProcessor secondProcessor;
    setParameter(processor.parameters, QQDeBreath::ParamIDs::monitorBreath, 0.0f);
    setParameter(secondProcessor.parameters, QQDeBreath::ParamIDs::monitorBreath, 1.0f);
    const auto firstInstanceBreath = processor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::monitorBreath)->load();
    const auto secondInstanceBreath = secondProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::monitorBreath)->load();
    if (firstInstanceBreath >= 0.5f || secondInstanceBreath < 0.5f)
    {
        std::cerr << "FAIL: separate plugin instances shared Monitor Breath state\n";
        return 1;
    }

    std::cout << "PASS: editor state, Breath/Noize monitor routing, lock fallback, and plugin-instance parameter isolation\n";
    return 0;
}
