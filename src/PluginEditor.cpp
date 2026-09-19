#include "PluginEditor.h"

#include "shared/NativeAnalysis.h"
#include "Version.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <juce_dsp/juce_dsp.h>

namespace
{
void setupInfoLabel(juce::Label& label)
{
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    label.setFont(juce::Font(13.0f, juce::Font::plain));
}

void setupButton(juce::TextButton& button, juce::Colour colour)
{
    button.setColour(juce::TextButton::buttonColourId, colour);
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
}

juce::String fnv1a64(const juce::String& text)
{
    constexpr std::uint64_t offset = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;

    auto hash = offset;
    const auto utf8 = text.toRawUTF8();

    for (auto* p = reinterpret_cast<const unsigned char*>(utf8); *p != 0; ++p)
    {
        hash ^= static_cast<std::uint64_t>(*p);
        hash *= prime;
    }

    return juce::String::toHexString(static_cast<juce::int64>(hash)).paddedLeft('0', 16);
}

void setupSlider(juce::Slider& slider, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::LinearBar);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setTextValueSuffix(suffix);
    slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff60a5fa));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xfff8fafc));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe2e8f0));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0f172a));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff334155));
}

double dbToGain(double db)
{
    return std::pow(10.0, db / 20.0);
}

bool hasEqContent(const QQDeBreathEqState& state)
{
    if (state.highPassEnabled || state.lowPassEnabled)
        return true;

    return std::any_of(state.bands.begin(), state.bands.end(), [](const auto& band) { return band.enabled; });
}

juce::int64 regionStartSample(const QQDeBreathBridgeRegion& region, double sampleRate, int totalSamples)
{
    return juce::jlimit<juce::int64>(0,
                                     totalSamples,
                                     region.startSample > 0 ? region.startSample
                                                            : static_cast<juce::int64>(std::llround(region.startTime * sampleRate)));
}

juce::int64 regionEndSample(const QQDeBreathBridgeRegion& region, double sampleRate, int totalSamples)
{
    return juce::jlimit<juce::int64>(0,
                                     totalSamples,
                                     region.endSample > 0 ? region.endSample
                                                          : static_cast<juce::int64>(std::llround(region.endTime * sampleRate)));
}

bool hasAdjacentRegionBefore(const juce::Array<QQDeBreathBridgeRegion>& regions,
                             int regionIndex,
                             juce::int64 start,
                             int fadeSamples,
                             double sampleRate,
                             int totalSamples)
{
    const auto tolerance = juce::jmax<juce::int64>(1, fadeSamples);
    for (auto i = 0; i < regions.size(); ++i)
    {
        if (i == regionIndex)
            continue;

        const auto otherEnd = regionEndSample(regions.getReference(i), sampleRate, totalSamples);
        const auto distance = otherEnd > start ? otherEnd - start : start - otherEnd;
        if (distance <= tolerance)
            return true;
    }

    return false;
}

bool hasAdjacentRegionAfter(const juce::Array<QQDeBreathBridgeRegion>& regions,
                            int regionIndex,
                            juce::int64 end,
                            int fadeSamples,
                            double sampleRate,
                            int totalSamples)
{
    const auto tolerance = juce::jmax<juce::int64>(1, fadeSamples);
    for (auto i = 0; i < regions.size(); ++i)
    {
        if (i == regionIndex)
            continue;

        const auto otherStart = regionStartSample(regions.getReference(i), sampleRate, totalSamples);
        const auto distance = otherStart > end ? otherStart - end : end - otherStart;
        if (distance <= tolerance)
            return true;
    }

    return false;
}

double regionWeightForIndex(const juce::Array<QQDeBreathBridgeRegion>& regions,
                            int regionIndex,
                            juce::int64 sample,
                            double sampleRate,
                            int totalSamples,
                            int fadeInSamples,
                            int fadeOutSamples)
{
    const auto& region = regions.getReference(regionIndex);
    const auto start = regionStartSample(region, sampleRate, totalSamples);
    const auto end = regionEndSample(region, sampleRate, totalSamples);
    const auto adjacentBefore = fadeInSamples > 0 && hasAdjacentRegionBefore(regions, regionIndex, start, fadeInSamples, sampleRate, totalSamples);
    const auto adjacentAfter = fadeOutSamples > 0 && hasAdjacentRegionAfter(regions, regionIndex, end, fadeOutSamples, sampleRate, totalSamples);

    if (sample >= start && sample < end)
    {
        auto weight = 1.0;
        if (fadeInSamples > 0 && ! adjacentBefore)
            weight = juce::jmin(weight, static_cast<double>(sample - start) / juce::jmax(1, fadeInSamples - 1));

        if (fadeOutSamples > 0 && ! adjacentAfter)
            weight = juce::jmin(weight, static_cast<double>(end - 1 - sample) / juce::jmax(1, fadeOutSamples - 1));

        return juce::jlimit(0.0, 1.0, weight);
    }

    if (adjacentBefore && sample >= start - fadeInSamples && sample < start)
        return juce::jlimit(0.0, 1.0, static_cast<double>(sample - (start - fadeInSamples)) / juce::jmax(1, fadeInSamples));

    if (adjacentAfter && sample >= end && sample < end + fadeOutSamples)
        return juce::jlimit(0.0, 1.0, 1.0 - static_cast<double>(sample - end) / juce::jmax(1, fadeOutSamples));

    return 0.0;
}

float sampleAt(const juce::AudioBuffer<float>& buffer, int channel, juce::int64 sample)
{
    if (sample < 0 || sample >= buffer.getNumSamples() || buffer.getNumChannels() <= 0)
        return 0.0f;

    return buffer.getSample(juce::jmin(channel, buffer.getNumChannels() - 1), static_cast<int>(sample));
}

bool readAudioFile(const juce::File& file, juce::AudioBuffer<float>& buffer, double& sampleRate, juce::String& status)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
    {
        status = "Could not read source wav: " + file.getFullPathName();
        return false;
    }

    if (reader->lengthInSamples <= 0 || reader->lengthInSamples > std::numeric_limits<int>::max())
    {
        status = "Source wav length is unsupported for export.";
        return false;
    }

    buffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples), false, false, true);
    buffer.clear();
    reader->read(&buffer, 0, buffer.getNumSamples(), 0, true, true);
    sampleRate = reader->sampleRate;
    return true;
}

bool writeWavFile(const juce::File& file,
                  const juce::AudioBuffer<float>& buffer,
                  double sampleRate,
                  juce::String& status)
{
    file.deleteFile();

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
    if (stream == nullptr)
    {
        status = "Could not open export file: " + file.getFullPathName();
        return false;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(stream.get(),
                                                                              sampleRate,
                                                                              static_cast<unsigned int>(buffer.getNumChannels()),
                                                                              32,
                                                                              {},
                                                                              0));
    if (writer == nullptr)
    {
        status = "Could not create wav writer for: " + file.getFileName();
        return false;
    }

    stream.release();
    if (! writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()))
    {
        status = "Failed while writing export file: " + file.getFullPathName();
        return false;
    }

    return true;
}

std::vector<double> spectrumDbFromBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    constexpr auto fftOrder = 11;
    constexpr auto fftSize = 1 << fftOrder;
    constexpr auto spectrumBins = 128;

    std::vector<double> dbBins(spectrumBins, -120.0);
    if (buffer.getNumSamples() <= 16 || buffer.getNumChannels() <= 0 || sampleRate <= 0.0)
        return dbBins;

    juce::dsp::FFT fft(fftOrder);
    std::vector<float> fftData(static_cast<size_t>(fftSize * 2), 0.0f);
    std::vector<double> accum(spectrumBins, 0.0);

    const auto copySamples = juce::jmin(fftSize, buffer.getNumSamples());
    const auto sourceOffset = juce::jmax(0, (buffer.getNumSamples() - copySamples) / 2);
    const auto fftOffset = juce::jmax(0, (fftSize - copySamples) / 2);

    for (auto i = 0; i < copySamples; ++i)
    {
        auto mono = 0.0f;
        for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            mono += buffer.getSample(channel, sourceOffset + i) / static_cast<float>(buffer.getNumChannels());

        const auto fftIndex = fftOffset + i;
        const auto window = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(fftIndex) / static_cast<float>(fftSize - 1));
        fftData[static_cast<size_t>(fftIndex)] = mono * window;
    }

    fft.performFrequencyOnlyForwardTransform(fftData.data());

    for (auto bin = 1; bin <= fftSize / 2; ++bin)
    {
        const auto frequency = static_cast<double>(bin) * sampleRate / static_cast<double>(fftSize);
        if (frequency < 20.0 || frequency > 20000.0)
            continue;

        const auto norm = (std::log(frequency) - std::log(20.0)) / (std::log(20000.0) - std::log(20.0));
        const auto index = juce::jlimit(0, spectrumBins - 1, static_cast<int>(std::floor(norm * static_cast<double>(spectrumBins - 1))));
        accum[static_cast<size_t>(index)] += static_cast<double>(fftData[static_cast<size_t>(bin)]);
    }

    for (size_t i = 0; i < accum.size(); ++i)
        dbBins[i] = 20.0 * std::log10(accum[i] + 1.0e-9);

    return dbBins;
}

std::vector<float> displaySpectrumFromDb(const std::vector<double>& dbBins, double maxDb)
{
    std::vector<float> display(dbBins.size(), 0.0f);
    for (size_t i = 0; i < dbBins.size(); ++i)
        display[i] = static_cast<float>(juce::jlimit(0.0, 1.0, (dbBins[i] - (maxDb - 64.0)) / 64.0));
    return display;
}

std::vector<float> spectrumFromBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const auto dbBins = spectrumDbFromBuffer(buffer, sampleRate);
    auto maxDb = -120.0;
    for (auto db : dbBins)
        maxDb = juce::jmax(maxDb, db);

    return displaySpectrumFromDb(dbBins, maxDb);
}

void holdSpectrumPeak(std::vector<float>& peak, const std::vector<float>& current)
{
    if (current.empty())
    {
        peak.clear();
        return;
    }

    if (peak.size() != current.size())
        peak.assign(current.size(), 0.0f);

    for (size_t i = 0; i < current.size(); ++i)
        peak[i] = juce::jmax(peak[i], current[i]);
}

void spectraFromBuffers(const juce::AudioBuffer<float>& pre,
                        const juce::AudioBuffer<float>& post,
                        double sampleRate,
                        std::vector<float>& preDisplay,
                        std::vector<float>& postDisplay)
{
    const auto preDb = spectrumDbFromBuffer(pre, sampleRate);
    const auto postDb = spectrumDbFromBuffer(post, sampleRate);
    auto maxDb = -120.0;
    for (auto db : preDb)
        maxDb = juce::jmax(maxDb, db);
    for (auto db : postDb)
        maxDb = juce::jmax(maxDb, db);

    preDisplay = displaySpectrumFromDb(preDb, maxDb);
    postDisplay = displaySpectrumFromDb(postDb, maxDb);
}

juce::File globalDefaultsFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("QQDeBreath")
        .getChildFile("ARA_VST3")
        .getChildFile("global_defaults.json");
}

juce::var boolVar(bool value)
{
    return juce::var(value);
}

juce::var doubleVar(double value)
{
    return juce::var(value);
}
} // namespace

class QQDeBreathAudioProcessorEditor::AnalysisThread final : public juce::Thread
{
public:
    AnalysisThread(QQDeBreathAudioProcessorEditor& ownerIn, QQDeBreathBridgeAnalysisConfig configIn)
        : juce::Thread("QQDeBreath VST3 Native Analysis"),
          owner(&ownerIn),
          config(std::move(configIn))
    {
    }

    ~AnalysisThread() override
    {
        signalThreadShouldExit();
        waitForThreadToExit(1500);
    }

    void run() override
    {
        auto result = QQDeBreathNativeAnalysis::run(config, [this] { return threadShouldExit(); });

        juce::MessageManager::callAsync([safeOwner = owner, result]
        {
            if (safeOwner != nullptr)
                safeOwner->handleAnalysisFinished(result);
        });
    }

private:
    juce::Component::SafePointer<QQDeBreathAudioProcessorEditor> owner;
    QQDeBreathBridgeAnalysisConfig config;
};

QQDeBreathAudioProcessorEditor::QQDeBreathAudioProcessorEditor(QQDeBreathAudioProcessor& processor)
    : AudioProcessorEditor(&processor),
      AudioProcessorEditorARAExtension(&processor),
      audioProcessor(processor)
{
    setWantsKeyboardFocus(true);

    titleLabel.setText("QQDeBreath", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont(juce::Font(30.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);
    titleLabel.setVisible(false);

    phaseLabel.setText("QQDeBreath ARA 1.23 Native + Global/Selected Breath EQ", juce::dontSendNotification);
    phaseLabel.setJustificationType(juce::Justification::centred);
    phaseLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    phaseLabel.setFont(juce::Font(18.0f, juce::Font::plain));
    addAndMakeVisible(phaseLabel);
    phaseLabel.setVisible(false);

    passthroughLabel.setText("Audio passthrough + per-instance recorded buffer", juce::dontSendNotification);
    passthroughLabel.setJustificationType(juce::Justification::centred);
    passthroughLabel.setColour(juce::Label::textColourId, juce::Colour(0xff93c5fd));
    passthroughLabel.setFont(juce::Font(16.0f, juce::Font::plain));
    addAndMakeVisible(passthroughLabel);
    passthroughLabel.setVisible(false);

    parameterHintLabel.setText("Recorded buffer is saved with this DAW project/plugin instance.", juce::dontSendNotification);
    parameterHintLabel.setJustificationType(juce::Justification::centred);
    parameterHintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    parameterHintLabel.setFont(juce::Font(13.0f, juce::Font::plain));
    addAndMakeVisible(parameterHintLabel);
    parameterHintLabel.setVisible(false);

    setupButton(loadAraButton, juce::Colour(0xff0f766e));
    loadAraButton.onClick = [this] { reloadAraSource(); };
    addAndMakeVisible(loadAraButton);

    setupButton(recordButton, juce::Colour(0xff16a34a));
    recordButton.onClick = [this]
    {
        sourceMode = SourceMode::recorded;
        araSourceInfo = {};
        audioProcessor.startRecording();
        lastExportedFile = juce::File();
        updateRecordingInfo();
    };
    addAndMakeVisible(recordButton);

    setupButton(stopButton, juce::Colour(0xffb45309));
    stopButton.onClick = [this]
    {
        audioProcessor.stopRecording();
        updateRecordingInfo();
    };
    addAndMakeVisible(stopButton);

    setupButton(clearButton, juce::Colour(0xff475569));
    clearButton.onClick = [this]
    {
        audioProcessor.clearRecording();
        if (sourceMode == SourceMode::recorded)
            sourceMode = SourceMode::none;
        lastExportedFile = juce::File();
        lastDisplayedRecordedSamples = 0;
        lastWaveformRefreshMs = 0;
        showingLiveRecordingPreview = false;
        waveformEditor.clearAudio();
        waveformEditor.setAnalysisResult({});
        selectedRegionIndex = -1;
        updateRecordingInfo();
    };
    addAndMakeVisible(clearButton);

    setupButton(undoButton, juce::Colour(0xff334155));
    undoButton.onClick = [this] { performUndo(); };
    addAndMakeVisible(undoButton);

    setupButton(redoButton, juce::Colour(0xff334155));
    redoButton.onClick = [this] { performRedo(); };
    addAndMakeVisible(redoButton);

    setupButton(exportButton, juce::Colour(0xff2563eb));
    exportButton.onClick = [this] { exportRecording(); };
    addAndMakeVisible(exportButton);

    setupButton(breathDetailButton, juce::Colour(0xff475569));
    breathDetailButton.setClickingTogglesState(false);
    breathDetailButton.onClick = [this]
    {
        if (selectedRegionIsBreath())
            showBreathDetailPage();
    };
    addAndMakeVisible(breathDetailButton);

    breathDetailBypassButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffffd166));
    breathDetailBypassButton.onClick = [this] { applyBreathDetailFromUi(); };
    addAndMakeVisible(breathDetailBypassButton);

    setupInfoLabel(breathDetailTopGainLabel);
    breathDetailTopGainLabel.setText("Gain", juce::dontSendNotification);
    addAndMakeVisible(breathDetailTopGainLabel);

    breathEqMainEnableButton.setButtonText("ON");
    breathEqMainEnableButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff93c5fd));
    breathEqMainEnableButton.onClick = [this] { setBreathEqEnabled(breathEqMainEnableButton.getToggleState()); };
    addAndMakeVisible(breathEqMainEnableButton);

    setupButton(breathEqPageButton, juce::Colour(0xff0f766e));
    breathEqPageButton.onClick = [this] { showBreathEqPage(); };
    addAndMakeVisible(breathEqPageButton);

    setupButton(setDefaultButton, juce::Colour(0xff475569));
    setDefaultButton.onClick = [this]
    {
        juce::Component::SafePointer<QQDeBreathAudioProcessorEditor> safeThis(this);
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::QuestionIcon)
                .withTitle("Set as Default")
                .withMessage("Save the current global settings as the default for new QQDeBreath instances?\n\nPer-Breath Gain and EQ are not included.")
                .withButton("Save")
                .withButton("Cancel")
                .withAssociatedComponent(this),
            [safeThis](int result)
            {
                if (result == 1 && safeThis != nullptr)
                    safeThis->saveCurrentGlobalDefaults();
            });
    };
    addAndMakeVisible(setDefaultButton);

    setupButton(analyzeButton, juce::Colour(0xff7c3aed));
    analyzeButton.onClick = [this] { startAnalysis(); };
    addAndMakeVisible(analyzeButton);

    setupButton(cancelAnalyzeButton, juce::Colour(0xffb45309));
    cancelAnalyzeButton.onClick = [this] { cancelAnalysis(); };
    addAndMakeVisible(cancelAnalyzeButton);

    setupButton(clearAnalysisButton, juce::Colour(0xff475569));
    clearAnalysisButton.onClick = [this] { clearAnalysis(); };
    addAndMakeVisible(clearAnalysisButton);

    for (auto* label : { &sampleRateLabel, &channelsLabel, &samplesLabel, &durationLabel, &exportPathLabel,
                         &analysisStatusLabel, &breathCountLabel, &noizeCountLabel, &analysisErrorLabel,
                         &sourceMismatchLabel, &statusLabel, &workflowLabel })
    {
        setupInfoLabel(*label);
        addAndMakeVisible(*label);
    }

    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xfffacc15));
    workflowLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    workflowLabel.setText("Click waveform: internal preview seek; Global EQ/Gain affects all Breath; Breath Adjust affects selected Breath; Drag values up/down, Shift=fine, Alt-click=default.",
                          juce::dontSendNotification);
    analysisErrorLabel.setColour(juce::Label::textColourId, juce::Colour(0xfffca5a5));
    sourceMismatchLabel.setColour(juce::Label::textColourId, juce::Colour(0xfffacc15));
    monitorVoiceButton.setToggleState(true, juce::dontSendNotification);
    monitorBreathButton.setToggleState(true, juce::dontSendNotification);
    monitorNoizeButton.setToggleState(true, juce::dontSendNotification);
    followButton.setToggleState(false, juce::dontSendNotification);
    fadeButton.setToggleState(true, juce::dontSendNotification);
    breathNormButton.setToggleState(false, juce::dontSendNotification);
    for (auto* button : { &monitorVoiceButton, &monitorBreathButton, &monitorNoizeButton, &followButton, &fadeButton, &breathNormButton })
    {
        button->setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
        addAndMakeVisible(*button);
    }
    monitorVoiceButton.onClick = [this] { syncAraPlaybackParams(); };
    monitorBreathButton.onClick = [this] { syncAraPlaybackParams(); };
    monitorNoizeButton.onClick = [this] { syncAraPlaybackParams(); };
    followButton.onClick = [this] { syncAraPlaybackParams(); };
    fadeButton.onClick = [this] { syncAraPlaybackParams(); };
    breathNormButton.onClick = [this]
    {
        syncAraPlaybackParams();
        waveformEditor.refreshProcessedDisplay();
        refreshBreathEqSpectrum();
    };

    monitorVoiceAttachment = std::make_unique<ButtonAttachment>(audioProcessor.parameters, QQDeBreath::ParamIDs::monitorVoice, monitorVoiceButton);
    monitorBreathAttachment = std::make_unique<ButtonAttachment>(audioProcessor.parameters, QQDeBreath::ParamIDs::monitorBreath, monitorBreathButton);
    monitorNoizeAttachment = std::make_unique<ButtonAttachment>(audioProcessor.parameters, QQDeBreath::ParamIDs::monitorNoize, monitorNoizeButton);
    followAttachment = std::make_unique<ButtonAttachment>(audioProcessor.parameters, QQDeBreath::ParamIDs::followPlayhead, followButton);
    fadeAttachment = std::make_unique<ButtonAttachment>(audioProcessor.parameters, QQDeBreath::ParamIDs::enableFade, fadeButton);
    breathNormAttachment = std::make_unique<ButtonAttachment>(audioProcessor.parameters, QQDeBreath::ParamIDs::normalizeBreath, breathNormButton);

    for (auto* label : { &fadeInLabel, &fadeOutLabel, &breathTargetLabel, &breathGainLabel, &waveformSizeLabel })
    {
        setupInfoLabel(*label);
        addAndMakeVisible(*label);
    }
    fadeInLabel.setText("In", juce::dontSendNotification);
    fadeOutLabel.setText("Out", juce::dontSendNotification);
    breathTargetLabel.setText("Target", juce::dontSendNotification);
    breathGainLabel.setText("Global Gain", juce::dontSendNotification);
    waveformSizeLabel.setText("Wave", juce::dontSendNotification);

    setupSlider(fadeInSlider, " ms");
    setupSlider(fadeOutSlider, " ms");
    setupSlider(breathTargetSlider, " dB");
    setupSlider(breathGainSlider, " dB");
    setupSlider(waveformSizeSlider, " x");
    fadeInSlider.configure(" ms", 10.0, 1.0, 0.1, 1);
    fadeOutSlider.configure(" ms", 10.0, 1.0, 0.1, 1);
    breathTargetSlider.configure(" dB", -6.0, 1.0, 0.1, 1);
    breathGainSlider.configure(" dB", 0.0, 1.0, 0.1, 1);
    breathGainSlider.setRange(-60.0, 30.0, 0.1);
    waveformSizeSlider.configure(" x", 1.0, 0.25, 0.05, 2);
    waveformSizeSlider.setRange(0.25, 8.0, 0.05);
    waveformSizeSlider.setValue(1.0, juce::dontSendNotification);
    waveformSizeSlider.onValueChange = [this]
    {
        waveformEditor.setWaveformDisplayGain(waveformSizeSlider.getValue());
        syncAraPlaybackParams();
    };
    fadeInSlider.onValueChange = [this] { syncAraPlaybackParams(); };
    fadeOutSlider.onValueChange = [this] { syncAraPlaybackParams(); };
    breathTargetSlider.onValueChange = [this] { syncAraPlaybackParams(); };
    breathGainSlider.onValueChange = [this]
    {
        syncAraPlaybackParams();
        requestDeferredSpectrumRefresh();
        requestDeferredWaveformRefresh(false);
    };
    juce::Slider* sliders[] = { &fadeInSlider, &fadeOutSlider, &breathTargetSlider, &breathGainSlider, &waveformSizeSlider };
    for (auto* slider : sliders)
        addAndMakeVisible(*slider);

    fadeInAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, QQDeBreath::ParamIDs::fadeInMs, fadeInSlider);
    fadeOutAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, QQDeBreath::ParamIDs::fadeOutMs, fadeOutSlider);
    breathTargetAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, QQDeBreath::ParamIDs::breathTargetDb, breathTargetSlider);
    breathGainAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, QQDeBreath::ParamIDs::breathGainDb, breathGainSlider);
    waveformSizeAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, QQDeBreath::ParamIDs::waveformDisplayGain, waveformSizeSlider);

    waveformEditor.onRegionsChanged = [this](const juce::Array<QQDeBreathBridgeRegion>& regions)
    {
        waveformRegionsChanged(regions);
    };
    waveformEditor.onSeekRequested = [this](double localSeconds)
    {
        waveformSeekRequested(localSeconds);
    };
    waveformEditor.onSelectedRegionChanged = [this](int index)
    {
        waveformSelectionChanged(index);
    };
    waveformEditor.onRegionDoubleClicked = [this](int index)
    {
        selectedRegionIndex = index;
        if (selectedRegionIsBreath())
            showBreathDetailPage();
    };
    addAndMakeVisible(waveformEditor);
    waveformEditor.setMonitorState(monitorVoiceButton.getToggleState(), monitorBreathButton.getToggleState(), monitorNoizeButton.getToggleState());
    waveformEditor.setFollowPlayhead(followButton.getToggleState());

    setupButton(closeBreathEqButton, juce::Colour(0xff991b1b));
    closeBreathEqButton.onClick = [this] { showMainPage(); };
    addChildComponent(closeBreathEqButton);
    breathEqEnableButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff93c5fd));
    breathEqHighPassButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    breathEqLowPassButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    breathEqLoopButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    breathEqAutoApplyButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    breathEqEnableButton.onClick = [this]
    {
        setBreathEqEnabled(breathEqEnableButton.getToggleState());
    };
    breathEqHighPassButton.onClick = [this]
    {
        auto state = audioProcessor.getBreathEqState();
        state.enabled = true;
        state.highPassEnabled = breathEqHighPassButton.getToggleState();
        applyBreathEqStateFromUi(state);
    };
    breathEqLowPassButton.onClick = [this]
    {
        auto state = audioProcessor.getBreathEqState();
        state.enabled = true;
        state.lowPassEnabled = breathEqLowPassButton.getToggleState();
        applyBreathEqStateFromUi(state);
    };
    addChildComponent(breathEqEnableButton);
    addChildComponent(breathEqHighPassButton);
    addChildComponent(breathEqLowPassButton);
    breathEqLoopButton.onClick = [this] { setBreathEqLoopEnabled(breathEqLoopButton.getToggleState()); };
    addChildComponent(breathEqLoopButton);
    breathEqAutoApplyButton.setToggleState(true, juce::dontSendNotification);
    breathEqAutoApplyButton.onClick = [this]
    {
        setAutoApplyEnabled(breathEqAutoApplyButton.getToggleState(), true);
    };
    addChildComponent(breathEqAutoApplyButton);
    setupButton(breathEqApplyButton, juce::Colour(0xff2563eb));
    breathEqApplyButton.onClick = [this] { applyBreathEqPreviewToWaveform(); };
    addChildComponent(breathEqApplyButton);
    setupButton(breathEqClearButton, juce::Colour(0xff475569));
    breathEqClearButton.onClick = [this]
    {
        const QQDeBreathEqState cleared;
        audioProcessor.setBreathEqState(cleared);
        breathEqPageSnapshot = cleared;
        breathEqPreviewDirty = false;
        globalPreSpectrumPeak.clear();
        globalPostSpectrumPeak.clear();
        refreshBreathEqUi();
        syncAraPlaybackParams();
        waveformEditor.refreshProcessedDisplay();
        refreshBreathEqSpectrum();
        statusLabel.setText("Status: Global Breath EQ cleared.", juce::dontSendNotification);
    };
    addChildComponent(breathEqClearButton);
    breathEqEditor.onStateChanged = [this](const QQDeBreathEqState& state)
    {
        applyBreathEqStateFromUi(state);
    };
    breathEqEditor.setTheme("Global Breath EQ", juce::Colour(0xff38bdf8), juce::Colour(0xffffd166));
    addChildComponent(breathEqEditor);
    refreshBreathEqUi();

    setupButton(closeBreathDetailButton, juce::Colour(0xff991b1b));
    closeBreathDetailButton.onClick = [this] { showMainPage(); };
    addChildComponent(closeBreathDetailButton);
    breathDetailTitleLabel.setJustificationType(juce::Justification::centredLeft);
    breathDetailTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffd166));
    breathDetailTitleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    addChildComponent(breathDetailTitleLabel);
    breathDetailEqPowerButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffffd166));
    breathDetailEqPowerButton.onClick = [this] { applyBreathDetailFromUi(); };
    addChildComponent(breathDetailEqPowerButton);
    breathDetailEqEnableButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffffd166));
    breathDetailEqEnableButton.onClick = [this] { applyBreathDetailFromUi(); };
    addChildComponent(breathDetailEqEnableButton);
    setupInfoLabel(breathDetailGainLabel);
    breathDetailGainLabel.setText("Gain", juce::dontSendNotification);
    addChildComponent(breathDetailGainLabel);
    setupSlider(breathDetailGainSlider, " dB");
    breathDetailGainSlider.configure(" dB", 0.0, 1.0, 0.1, 1);
    breathDetailGainSlider.setRange(-30.0, 30.0, 0.1);
    breathDetailGainSlider.setValue(0.0, juce::dontSendNotification);
    breathDetailGainSlider.onValueChange = [this] { applyBreathDetailFromUi(); };
    addChildComponent(breathDetailGainSlider);
    breathDetailAutoApplyButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    breathDetailAutoApplyButton.setToggleState(true, juce::dontSendNotification);
    breathDetailAutoApplyButton.onClick = [this]
    {
        setAutoApplyEnabled(breathDetailAutoApplyButton.getToggleState(), true);
    };
    addChildComponent(breathDetailAutoApplyButton);
    setupButton(breathDetailApplyButton, juce::Colour(0xff2563eb));
    breathDetailApplyButton.onClick = [this] { applyBreathDetailPreviewToWaveform(); };
    addChildComponent(breathDetailApplyButton);
    setupButton(breathDetailClearButton, juce::Colour(0xff475569));
    breathDetailClearButton.onClick = [this]
    {
        if (! selectedRegionIsBreath())
            return;

        const QQDeBreathEqState cleared;
        updatingBreathDetailUi = true;
        breathDetailEqPowerButton.setToggleState(false, juce::dontSendNotification);
        breathDetailBypassButton.setToggleState(false, juce::dontSendNotification);
        breathDetailEqEditor.setState(cleared, false);
        updatingBreathDetailUi = false;
        breathDetailSnapshotEq = cleared;
        breathDetailPreviewDirty = false;
        detailPreSpectrumPeak.clear();
        detailPostSpectrumPeak.clear();
        waveformEditor.setRegionProcessing(selectedRegionIndex,
                                            breathDetailGainSlider.getValue(),
                                            cleared,
                                            true,
                                            true);
        refreshBreathDetailUi();
        refreshBreathEqSpectrum();
        statusLabel.setText("Status: Selected Breath EQ cleared; Gain preserved.", juce::dontSendNotification);
    };
    addChildComponent(breathDetailClearButton);
    breathDetailEqEditor.onStateChanged = [this](const QQDeBreathEqState& state)
    {
        if (updatingBreathDetailUi || ! selectedRegionIsBreath())
            return;

        auto eq = state;
        eq.enabled = breathDetailEqPowerButton.getToggleState();
        eq.bypassed = false;
        const auto autoApply = breathDetailAutoApplyButton.getToggleState();
        if (autoApply)
        {
            waveformEditor.setRegionProcessing(selectedRegionIndex, breathDetailGainSlider.getValue(), eq, false, false);
            audioProcessor.updateAnalysisRegionsPreservingCaches(waveformEditor.getRegions());
            requestDeferredAraRuntimeUpdate();
        }
        else
        {
            updateBreathDetailPreviewForListening(breathDetailGainSlider.getValue(), eq);
        }
        breathDetailPreviewDirty = ! autoApply;

        if (autoApply)
        {
            breathDetailSnapshotEq = eq;
            breathDetailSnapshotGainDb = breathDetailGainSlider.getValue();
            requestDeferredWaveformRefresh(true);
        }

        requestDeferredSpectrumRefresh();
    };
    breathDetailEqEditor.setTheme("Selected Breath EQ", juce::Colour(0xfff59e0b), juce::Colour(0xffa78bfa));
    addChildComponent(breathDetailEqEditor);

    araSourceInfo = audioProcessor.getAraSourceInfo();
    if (araSourceInfo.exportedWav.existsAsFile())
    {
        sourceMode = SourceMode::ara;
        lastExportedFile = araSourceInfo.exportedWav;
        juce::String waveformStatus;
        waveformEditor.loadAudioFile(araSourceInfo.exportedWav, waveformStatus);
    }
    applySavedGlobalDefaultsIfFreshInstance();
    waveformEditor.setAnalysisResult(audioProcessor.getAnalysisResult());
    updateRecordingInfo();
    startTimerHz(30);

    // Establish the default size before the host negotiates the embedded ARA view.
    setSize(1180, 720);
    setResizable(true, false);
    setResizeLimits(960, 560, 8192, 8192);
}

QQDeBreathAudioProcessorEditor::~QQDeBreathAudioProcessorEditor()
{
    cancelAnalysis();
}

void QQDeBreathAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient gradient(juce::Colour(0xff111827), bounds.getTopLeft(),
                                  juce::Colour(0xff0f172a), bounds.getBottomRight(), false);
    g.setGradientFill(gradient);
    g.fillAll();

    g.setColour(juce::Colour(0xff334155));
    g.drawRoundedRectangle(bounds.reduced(12.0f), 8.0f, 1.0f);
}

void QQDeBreathAudioProcessorEditor::resized()
{
    const auto araContext = isAraContext();
    const auto scale = juce::jlimit(0.85f, 1.45f, juce::jmin(static_cast<float>(getWidth()) / 1180.0f,
                                                            static_cast<float>(getHeight()) / 720.0f));
    const auto S = [scale](int v) { return static_cast<int>(std::round(static_cast<float>(v) * scale)); };
    auto area = getLocalBounds().reduced(S(10));
    const auto gap = S(6);
    const auto rowHeight = S(36);

    if (showingBreathEqPage)
    {
        auto eqArea = area;
        auto eqHeader = eqArea.removeFromTop(S(48));
        closeBreathEqButton.setBounds(eqHeader.removeFromRight(S(44)).reduced(S(4), S(4)));
        breathEqEnableButton.setBounds(eqHeader.removeFromLeft(S(58)).reduced(S(3), S(3)));
        breathEqLoopButton.setBounds(eqHeader.removeFromLeft(S(96)).reduced(S(3), S(3)));
        breathGainLabel.setBounds(eqHeader.removeFromLeft(S(96)).reduced(S(3), S(3)));
        breathGainSlider.setBounds(eqHeader.removeFromLeft(S(112)).reduced(S(3), S(6)));
        breathEqAutoApplyButton.setBounds(eqHeader.removeFromLeft(S(132)).reduced(S(3), S(3)));
        breathEqApplyButton.setBounds(eqHeader.removeFromLeft(S(92)).reduced(S(3), S(3)));
        breathEqClearButton.setBounds(eqHeader.removeFromLeft(S(82)).reduced(S(3), S(3)));
        breathEqHighPassButton.setBounds({});
        breathEqLowPassButton.setBounds({});
        eqArea.removeFromTop(S(6));
        auto compactPlayback = eqArea.removeFromBottom(S(222));
        eqArea.removeFromBottom(gap);
        breathEqEditor.setBounds(eqArea);

        waveformEditor.setBounds(compactPlayback.removeFromTop(S(150)));
        auto monitor = compactPlayback.removeFromTop(rowHeight);
        monitorVoiceButton.setBounds(monitor.removeFromLeft(S(86)).reduced(S(3), S(3)));
        monitorBreathButton.setBounds(monitor.removeFromLeft(S(94)).reduced(S(3), S(3)));
        monitorNoizeButton.setBounds(monitor.removeFromLeft(S(92)).reduced(S(3), S(3)));
        followButton.setBounds(monitor.removeFromLeft(S(92)).reduced(S(3), S(3)));
        workflowLabel.setBounds(compactPlayback.removeFromTop(S(24)));
        statusLabel.setBounds(compactPlayback.removeFromTop(S(24)));
        return;
    }

    if (showingBreathDetailPage)
    {
        auto detailArea = area;
        auto detailHeader = detailArea.removeFromTop(S(48));
        closeBreathDetailButton.setBounds(detailHeader.removeFromRight(S(44)).reduced(S(4), S(4)));
        breathDetailTitleLabel.setBounds(detailHeader.removeFromLeft(S(260)).reduced(S(6), S(4)));
        breathDetailEqPowerButton.setBounds(detailHeader.removeFromLeft(S(78)).reduced(S(3), S(3)));
        breathEqLoopButton.setBounds(detailHeader.removeFromLeft(S(82)).reduced(S(3), S(3)));
        breathDetailAutoApplyButton.setBounds(detailHeader.removeFromLeft(S(132)).reduced(S(3), S(3)));
        breathDetailApplyButton.setBounds(detailHeader.removeFromLeft(S(92)).reduced(S(3), S(3)));
        breathDetailClearButton.setBounds(detailHeader.removeFromLeft(S(82)).reduced(S(3), S(3)));
        breathDetailEqEnableButton.setBounds({});
        breathDetailGainLabel.setBounds(detailHeader.removeFromLeft(S(54)).reduced(S(3), S(3)));
        breathDetailGainSlider.setBounds(detailHeader.removeFromLeft(S(110)).reduced(S(3), S(6)));
        detailArea.removeFromTop(S(6));
        auto compactPlayback = detailArea.removeFromBottom(S(222));
        detailArea.removeFromBottom(gap);
        breathDetailEqEditor.setBounds(detailArea);

        waveformEditor.setBounds(compactPlayback.removeFromTop(S(150)));
        auto monitor = compactPlayback.removeFromTop(rowHeight);
        monitorVoiceButton.setBounds(monitor.removeFromLeft(S(86)).reduced(S(3), S(3)));
        monitorBreathButton.setBounds(monitor.removeFromLeft(S(94)).reduced(S(3), S(3)));
        monitorNoizeButton.setBounds(monitor.removeFromLeft(S(92)).reduced(S(3), S(3)));
        followButton.setBounds(monitor.removeFromLeft(S(92)).reduced(S(3), S(3)));
        workflowLabel.setBounds(compactPlayback.removeFromTop(S(24)));
        statusLabel.setBounds(compactPlayback.removeFromTop(S(24)));
        return;
    }

    const auto compactToolbar = area.getWidth() < S(1440);
    auto top = area.removeFromTop(rowHeight);
    juce::Rectangle<int> selectedTools;

    if (compactToolbar)
    {
        area.removeFromTop(gap);
        auto secondary = area.removeFromTop(rowHeight);
        selectedTools = secondary.removeFromLeft(S(370));
        exportButton.setBounds(secondary.removeFromRight(S(168)).reduced(S(3), S(3)));
    }
    else
    {
        auto topRight = top.removeFromRight(S(560));
        selectedTools = topRight.removeFromLeft(S(370));
        exportButton.setBounds(topRight.removeFromRight(S(168)).reduced(S(3), S(3)));
    }

    breathDetailBypassButton.setBounds(selectedTools.removeFromLeft(S(52)).reduced(S(3), S(3)));
    breathDetailButton.setBounds(selectedTools.removeFromLeft(S(128)).reduced(S(3), S(3)));
    breathDetailTopGainLabel.setBounds(selectedTools.removeFromLeft(S(44)).reduced(S(3), S(3)));
    breathDetailGainSlider.setBounds(selectedTools.removeFromLeft(S(102)).reduced(S(3), S(6)));

    if (araContext)
    {
        loadAraButton.setBounds(top.removeFromLeft(S(104)).reduced(S(3), S(3)));
        recordButton.setBounds({});
        stopButton.setBounds({});
        clearButton.setBounds({});
    }
    else
    {
        loadAraButton.setBounds({});
        recordButton.setBounds(top.removeFromLeft(S(88)).reduced(S(3), S(3)));
        stopButton.setBounds(top.removeFromLeft(S(78)).reduced(S(3), S(3)));
        clearButton.setBounds(top.removeFromLeft(S(132)).reduced(S(3), S(3)));
    }

    top.removeFromLeft(gap);
    undoButton.setBounds(top.removeFromLeft(S(74)).reduced(S(3), S(3)));
    redoButton.setBounds(top.removeFromLeft(S(74)).reduced(S(3), S(3)));
    top.removeFromLeft(gap);
    breathEqMainEnableButton.setBounds(top.removeFromLeft(S(52)).reduced(S(3), S(3)));
    breathEqPageButton.setBounds(top.removeFromLeft(S(104)).reduced(S(3), S(3)));
    setDefaultButton.setBounds(top.removeFromLeft(S(132)).reduced(S(3), S(3)));
    analyzeButton.setBounds(top.removeFromLeft(S(96)).reduced(S(3), S(3)));
    cancelAnalyzeButton.setBounds(top.removeFromLeft(S(92)).reduced(S(3), S(3)));
    clearAnalysisButton.setBounds(top.removeFromLeft(S(130)).reduced(S(3), S(3)));

    area.removeFromTop(gap);
    auto parameters = area.removeFromTop(rowHeight);
    fadeButton.setBounds(parameters.removeFromLeft(S(70)).reduced(S(3), S(3)));
    fadeInLabel.setBounds(parameters.removeFromLeft(S(22)).reduced(S(2), S(3)));
    fadeInSlider.setBounds(parameters.removeFromLeft(S(76)).reduced(S(2), S(5)));
    fadeOutLabel.setBounds(parameters.removeFromLeft(S(30)).reduced(S(2), S(3)));
    fadeOutSlider.setBounds(parameters.removeFromLeft(S(76)).reduced(S(2), S(5)));
    breathNormButton.setBounds(parameters.removeFromLeft(S(126)).reduced(S(3), S(3)));
    breathTargetLabel.setBounds(parameters.removeFromLeft(S(52)).reduced(S(2), S(3)));
    breathTargetSlider.setBounds(parameters.removeFromLeft(S(86)).reduced(S(2), S(5)));
    breathGainLabel.setBounds(parameters.removeFromLeft(S(92)).reduced(S(2), S(3)));
    breathGainSlider.setBounds(parameters.removeFromLeft(S(92)).reduced(S(2), S(5)));

    area.removeFromTop(gap);
    const auto bottomHeight = S(118);
    waveformEditor.setBounds(area.removeFromTop(juce::jmax(S(220), area.getHeight() - bottomHeight)));

    area.removeFromTop(gap);
    auto monitor = area.removeFromTop(rowHeight);
    monitorVoiceButton.setBounds(monitor.removeFromLeft(S(86)).reduced(S(3), S(3)));
    monitorBreathButton.setBounds(monitor.removeFromLeft(S(94)).reduced(S(3), S(3)));
    monitorNoizeButton.setBounds(monitor.removeFromLeft(S(92)).reduced(S(3), S(3)));
    followButton.setBounds(monitor.removeFromLeft(S(92)).reduced(S(3), S(3)));
    if (! araContext)
    {
        monitor.removeFromLeft(gap);
        waveformSizeLabel.setBounds(monitor.removeFromLeft(S(50)).reduced(S(2), S(3)));
        waveformSizeSlider.setBounds(monitor.removeFromLeft(S(82)).reduced(S(2), S(5)));
    }
    else
    {
        waveformSizeLabel.setBounds({});
        waveformSizeSlider.setBounds({});
    }

    auto info = area.removeFromTop(S(24));
    sampleRateLabel.setBounds(info.removeFromLeft(S(150)));
    channelsLabel.setBounds(info.removeFromLeft(S(88)));
    durationLabel.setBounds(info.removeFromLeft(S(170)));
    samplesLabel.setBounds(info.removeFromLeft(S(170)));
    analysisStatusLabel.setBounds(info.removeFromLeft(S(220)));
    breathCountLabel.setBounds(info.removeFromLeft(S(130)));
    noizeCountLabel.setBounds(info.removeFromLeft(S(130)));

    workflowLabel.setBounds(area.removeFromTop(S(24)));
    statusLabel.setBounds(area.removeFromTop(S(24)));
    sourceMismatchLabel.setBounds(area.removeFromTop(S(24)));
    analysisErrorLabel.setBounds(area.removeFromTop(S(24)));
    exportPathLabel.setBounds(0, 0, 0, 0);
}

bool QQDeBreathAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    const auto modifiers = key.getModifiers();
    const auto keyCode = key.getKeyCode();
    const auto isZ = keyCode == 'z' || keyCode == 'Z';

    if (isZ && (modifiers.isCommandDown() || modifiers.isCtrlDown()))
    {
        if (modifiers.isShiftDown())
            performRedo();
        else
            performUndo();

        return true;
    }

    return juce::AudioProcessorEditor::keyPressed(key);
}

void QQDeBreathAudioProcessorEditor::performUndo()
{
    if (! waveformEditor.canUndo())
        return;

    waveformEditor.undo();
    updateAnalysisInfo();
}

void QQDeBreathAudioProcessorEditor::performRedo()
{
    if (! waveformEditor.canRedo())
        return;

    waveformEditor.redo();
    updateAnalysisInfo();
}

void QQDeBreathAudioProcessorEditor::setAutoApplyEnabled(bool enabled, bool applyPendingPreview)
{
    breathEqAutoApplyButton.setToggleState(enabled, juce::dontSendNotification);
    breathDetailAutoApplyButton.setToggleState(enabled, juce::dontSendNotification);

    if (! enabled || ! applyPendingPreview)
        return;

    if (showingBreathEqPage && breathEqPreviewDirty)
        applyBreathEqPreviewToWaveform();
    else if (showingBreathDetailPage && breathDetailPreviewDirty)
        applyBreathDetailPreviewToWaveform();
}

void QQDeBreathAudioProcessorEditor::parentHierarchyChanged()
{
    juce::AudioProcessorEditor::parentHierarchyChanged();
    scheduleInitialHostLayoutSync();
}

void QQDeBreathAudioProcessorEditor::visibilityChanged()
{
    juce::AudioProcessorEditor::visibilityChanged();

    if (isShowing())
        scheduleInitialHostLayoutSync();
}

void QQDeBreathAudioProcessorEditor::setScaleFactor(float newScaleFactor)
{
    juce::AudioProcessorEditor::setScaleFactor(newScaleFactor);
    resized();
    repaint();
    scheduleInitialHostLayoutSync();
}

void QQDeBreathAudioProcessorEditor::scheduleInitialHostLayoutSync()
{
    if (! isAraContext())
        return;

    initialHostLayoutSyncPending = true;
    initialHostLayoutSyncAttempts = 0;
}

void QQDeBreathAudioProcessorEditor::performInitialHostLayoutSync()
{
    if (! initialHostLayoutSyncPending)
        return;

    if (! isAraContext())
    {
        initialHostLayoutSyncPending = false;
        return;
    }

    // Some ARA hosts attach the native view first and send its final bounds a
    // few message-loop turns later. Give that handshake a bounded opportunity
    // to complete without continuously fighting the host during normal resize.
    if (getPeer() == nullptr || getWidth() <= 0 || getHeight() <= 0)
        return;

    ++initialHostLayoutSyncAttempts;
    if (initialHostLayoutSyncAttempts == 1)
    {
        setResizable(true, false);
        setResizeLimits(960, 560, 8192, 8192);
    }

    // A one-pixel nudge is enough to make hosts that defer their first editor
    // resize send the real embedded bounds. It is attempted only twice.
    if (initialHostLayoutSyncAttempts == 1 || initialHostLayoutSyncAttempts == 4)
    {
        const auto width = getWidth();
        const auto height = getHeight();
        setSize(width + 1, height);
        setSize(width, height);
    }

    resized();
    repaint();

    if (initialHostLayoutSyncAttempts >= 8)
        initialHostLayoutSyncPending = false;
}

void QQDeBreathAudioProcessorEditor::timerCallback()
{
    if (initialHostLayoutSyncPending)
        performInitialHostLayoutSync();

    updateRecordingInfo();

    if (pendingBreathDetailPersist)
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (lastBreathDetailPersistMs == 0 || now - lastBreathDetailPersistMs >= 500u)
        {
            pendingBreathDetailPersist = false;
            lastBreathDetailPersistMs = now;
            persistAraState();
        }
    }

    if (pendingWaveformRefresh)
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (now - pendingWaveformRefreshMs >= 90u)
        {
            pendingWaveformRefresh = false;
            waveformEditor.refreshProcessedDisplay();
            if (pendingWaveformRefreshShouldPersist)
                persistAraState();
            pendingWaveformRefreshShouldPersist = false;
        }
    }

    if (pendingAraRuntimeUpdate)
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (now - pendingAraRuntimeUpdateMs >= 25u)
        {
            pendingAraRuntimeUpdate = false;
            updateAraRuntimeState();
        }
    }

    if (pendingSpectrumRefresh)
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (now - pendingSpectrumRefreshMs >= 45u)
        {
            pendingSpectrumRefresh = false;
            refreshBreathEqSpectrum();
        }
    }
}

void QQDeBreathAudioProcessorEditor::updateRecordingInfo()
{
    restoreAraStateIfNeeded();

    const auto info = audioProcessor.getRecordedBufferInfo();
    const auto usingAraSource = sourceMode == SourceMode::ara && araSourceInfo.exportedWav.existsAsFile();

    sampleRateLabel.setText("SR: " + juce::String(usingAraSource ? araSourceInfo.sampleRate : info.sampleRate, 1) + " Hz", juce::dontSendNotification);
    channelsLabel.setText("Ch: " + juce::String(usingAraSource ? araSourceInfo.channelCount : info.channelCount), juce::dontSendNotification);
    samplesLabel.setText("Samples: " + juce::String(usingAraSource ? araSourceInfo.numSamples : info.numSamples), juce::dontSendNotification);
    durationLabel.setText("Duration: " + juce::String(usingAraSource ? araSourceInfo.durationSeconds : info.durationSeconds, 3) + " s", juce::dontSendNotification);
    exportPathLabel.setText("Temp wav: " + (lastExportedFile.existsAsFile() ? lastExportedFile.getFullPathName() : "(not exported)"), juce::dontSendNotification);
    statusLabel.setText("Status: " + (sourceMode == SourceMode::ara ? araSourceInfo.exportStatus : info.status), juce::dontSendNotification);
    QQDeBreathWaveformEditor::DisplayProcessingParams waveformParams;
    waveformParams.enableFade = audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::enableFade)->load() >= 0.5f;
    waveformParams.normalizeBreath = audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::normalizeBreath)->load() >= 0.5f;
    waveformParams.fadeInMs = static_cast<double>(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::fadeInMs)->load());
    waveformParams.fadeOutMs = static_cast<double>(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::fadeOutMs)->load());
    waveformParams.breathTargetDb = static_cast<double>(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::breathTargetDb)->load());
    waveformParams.breathGainDb = juce::jlimit(-60.0, 30.0, static_cast<double>(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::breathGainDb)->load()));
    waveformParams.breathEqState = showingBreathEqPage && breathEqPreviewDirty && ! breathEqAutoApplyButton.getToggleState()
                                 ? breathEqPageSnapshot
                                 : audioProcessor.getBreathEqState();
    waveformEditor.setProcessingParams(waveformParams);
    waveformEditor.setMonitorState(monitorVoiceButton.getToggleState(), monitorBreathButton.getToggleState(), monitorNoizeButton.getToggleState());
    waveformEditor.setFollowPlayhead(followButton.getToggleState());
    syncAraPlaybackParams();
    waveformEditor.setRecordingOverlay(! isAraContext() && (info.isRecordArmed || info.isRecording),
                                       info.isRecording ? "Recording..." : "Record armed");
    updateContextUi();
    refreshBreathDetailControlsVisibility();
    refreshRecordedWaveformIfNeeded(info);
    updateBreathEqDynamicSpectrum();

    const auto analysisRunning = analysisThread != nullptr && analysisThread->isThreadRunning();
    const auto araContext = isAraContext();
    const auto result = audioProcessor.getAnalysisResult();
    const auto canExportStems = result.succeeded && hasAnalyzableSource(info);
    loadAraButton.setEnabled(araContext && ! analysisRunning);
    recordButton.setEnabled(! araContext && ! info.isRecordArmed && ! analysisRunning);
    stopButton.setEnabled(! araContext && (info.isRecordArmed || info.isRecording) && ! analysisRunning);
    clearButton.setEnabled(! araContext && (info.isRecordArmed || info.hasRecording) && ! analysisRunning);
    exportButton.setEnabled(! info.isRecordArmed && ! info.isRecording && canExportStems && ! analysisRunning);
    undoButton.setEnabled(waveformEditor.canUndo() && ! analysisRunning);
    redoButton.setEnabled(waveformEditor.canRedo() && ! analysisRunning);

    updateAnalysisInfo();
    updatePlayheadFromHost(info);
}

bool QQDeBreathAudioProcessorEditor::isAraContext() const
{
    return audioProcessor.isBoundToAraHost();
}

void QQDeBreathAudioProcessorEditor::setMainPageComponentsVisible(bool visible)
{
    for (auto* component : {
             static_cast<juce::Component*>(&loadAraButton),
             static_cast<juce::Component*>(&recordButton),
             static_cast<juce::Component*>(&stopButton),
             static_cast<juce::Component*>(&clearButton),
             static_cast<juce::Component*>(&undoButton),
             static_cast<juce::Component*>(&redoButton),
             static_cast<juce::Component*>(&exportButton),
             static_cast<juce::Component*>(&breathDetailButton),
             static_cast<juce::Component*>(&breathDetailBypassButton),
             static_cast<juce::Component*>(&breathDetailTopGainLabel),
             static_cast<juce::Component*>(&breathEqMainEnableButton),
             static_cast<juce::Component*>(&breathEqPageButton),
             static_cast<juce::Component*>(&setDefaultButton),
             static_cast<juce::Component*>(&analyzeButton),
             static_cast<juce::Component*>(&cancelAnalyzeButton),
             static_cast<juce::Component*>(&clearAnalysisButton),
             static_cast<juce::Component*>(&sampleRateLabel),
             static_cast<juce::Component*>(&channelsLabel),
             static_cast<juce::Component*>(&samplesLabel),
             static_cast<juce::Component*>(&durationLabel),
             static_cast<juce::Component*>(&exportPathLabel),
             static_cast<juce::Component*>(&analysisStatusLabel),
             static_cast<juce::Component*>(&breathCountLabel),
             static_cast<juce::Component*>(&noizeCountLabel),
             static_cast<juce::Component*>(&analysisErrorLabel),
             static_cast<juce::Component*>(&sourceMismatchLabel),
             static_cast<juce::Component*>(&statusLabel),
             static_cast<juce::Component*>(&workflowLabel),
             static_cast<juce::Component*>(&monitorVoiceButton),
             static_cast<juce::Component*>(&monitorBreathButton),
             static_cast<juce::Component*>(&monitorNoizeButton),
             static_cast<juce::Component*>(&followButton),
             static_cast<juce::Component*>(&fadeButton),
             static_cast<juce::Component*>(&breathNormButton),
             static_cast<juce::Component*>(&fadeInLabel),
             static_cast<juce::Component*>(&fadeOutLabel),
             static_cast<juce::Component*>(&breathTargetLabel),
             static_cast<juce::Component*>(&breathGainLabel),
             static_cast<juce::Component*>(&waveformSizeLabel),
             static_cast<juce::Component*>(&fadeInSlider),
             static_cast<juce::Component*>(&fadeOutSlider),
             static_cast<juce::Component*>(&breathTargetSlider),
             static_cast<juce::Component*>(&breathGainSlider),
             static_cast<juce::Component*>(&waveformSizeSlider),
             static_cast<juce::Component*>(&waveformEditor) })
    {
        component->setVisible(visible);
    }
}

void QQDeBreathAudioProcessorEditor::showMainPage()
{
    const auto wasEqPage = showingBreathEqPage || showingBreathDetailPage;
    rollbackBreathEqPreviewIfNeeded();
    rollbackBreathDetailPreviewIfNeeded();
    if (breathEqLoopButton.getToggleState())
        setBreathEqLoopEnabled(false);

    showingBreathEqPage = false;
    showingBreathDetailPage = false;
    closeBreathEqButton.setVisible(false);
    breathEqEnableButton.setVisible(false);
    breathEqHighPassButton.setVisible(false);
    breathEqLowPassButton.setVisible(false);
    breathEqLoopButton.setVisible(false);
    breathEqAutoApplyButton.setVisible(false);
    breathEqApplyButton.setVisible(false);
    breathEqClearButton.setVisible(false);
    breathEqEditor.setVisible(false);
    closeBreathDetailButton.setVisible(false);
    breathDetailTitleLabel.setVisible(false);
    breathDetailEqPowerButton.setVisible(false);
    breathDetailEqEnableButton.setVisible(false);
    breathDetailGainLabel.setVisible(false);
    breathDetailGainSlider.setVisible(selectedRegionIsBreath());
    breathDetailAutoApplyButton.setVisible(false);
    breathDetailApplyButton.setVisible(false);
    breathDetailClearButton.setVisible(false);
    breathDetailEqEditor.setVisible(false);
    setMainPageComponentsVisible(true);
    if (wasEqPage)
        waveformEditor.refreshProcessedDisplay();
    updateContextUi();
    resized();
    repaint();
}

void QQDeBreathAudioProcessorEditor::showBreathEqPage()
{
    showingBreathEqPage = true;
    showingBreathDetailPage = false;
    setMainPageComponentsVisible(false);
    closeBreathDetailButton.setVisible(false);
    breathDetailTitleLabel.setVisible(false);
    breathDetailEqPowerButton.setVisible(false);
    breathDetailEqEnableButton.setVisible(false);
    breathDetailGainLabel.setVisible(false);
    breathDetailGainSlider.setVisible(false);
    breathDetailAutoApplyButton.setVisible(false);
    breathDetailApplyButton.setVisible(false);
    breathDetailClearButton.setVisible(false);
    breathDetailEqEditor.setVisible(false);
    closeBreathEqButton.setVisible(true);
    breathEqEnableButton.setVisible(true);
    breathEqHighPassButton.setVisible(false);
    breathEqLowPassButton.setVisible(false);
    breathEqLoopButton.setVisible(true);
    breathGainLabel.setVisible(true);
    breathGainSlider.setVisible(true);
    breathEqAutoApplyButton.setVisible(true);
    breathEqApplyButton.setVisible(true);
    breathEqClearButton.setVisible(true);
    breathEqEditor.setVisible(true);
    waveformEditor.setVisible(true);
    monitorVoiceButton.setVisible(true);
    monitorBreathButton.setVisible(true);
    monitorNoizeButton.setVisible(true);
    followButton.setVisible(true);
    workflowLabel.setVisible(true);
    statusLabel.setVisible(true);
    refreshBreathEqUi();
    breathEqPageSnapshot = audioProcessor.getBreathEqState();
    breathEqPreviewDirty = false;
    refreshBreathEqSpectrum();
    lastBreathEqDynamicSpectrumMs = 0;
    resized();
    repaint();
}

void QQDeBreathAudioProcessorEditor::showBreathDetailPage()
{
    if (! selectedRegionIsBreath())
        return;

    if (breathEqLoopButton.getToggleState())
        setBreathEqLoopEnabled(false);

    showingBreathEqPage = false;
    showingBreathDetailPage = true;
    setMainPageComponentsVisible(false);
    closeBreathEqButton.setVisible(false);
    breathEqEnableButton.setVisible(false);
    breathEqHighPassButton.setVisible(false);
    breathEqLowPassButton.setVisible(false);
    breathEqLoopButton.setVisible(true);
    breathGainLabel.setVisible(false);
    breathGainSlider.setVisible(false);
    breathEqAutoApplyButton.setVisible(false);
    breathEqApplyButton.setVisible(false);
    breathEqClearButton.setVisible(false);
    breathEqEditor.setVisible(false);
    closeBreathDetailButton.setVisible(true);
    breathDetailTitleLabel.setVisible(true);
    breathDetailEqPowerButton.setVisible(true);
    breathDetailEqEnableButton.setVisible(false);
    breathDetailGainLabel.setVisible(true);
    breathDetailGainSlider.setVisible(true);
    breathDetailAutoApplyButton.setVisible(true);
    breathDetailApplyButton.setVisible(true);
    breathDetailClearButton.setVisible(true);
    breathDetailEqEditor.setVisible(true);
    waveformEditor.setVisible(true);
    monitorVoiceButton.setVisible(true);
    monitorBreathButton.setVisible(true);
    monitorNoizeButton.setVisible(true);
    followButton.setVisible(true);
    workflowLabel.setVisible(true);
    statusLabel.setVisible(true);
    refreshBreathDetailUi();
    const auto region = waveformEditor.getRegion(selectedRegionIndex);
    breathDetailSnapshotIndex = selectedRegionIndex;
    breathDetailSnapshotGainDb = region.gainDb;
    breathDetailSnapshotEq = region.eqState;
    breathDetailPreviewDirty = false;
    refreshBreathEqSpectrum();
    resized();
    repaint();
}

void QQDeBreathAudioProcessorEditor::updateContextUi()
{
    const auto araContext = isAraContext();

    if (showingBreathEqPage || showingBreathDetailPage)
        return;

    if (araUiMode != araContext)
    {
        araUiMode = araContext;
        setResizable(true, false);
        resized();

        if (araContext)
            scheduleInitialHostLayoutSync();
    }

    waveformSizeLabel.setVisible(! araContext);
    waveformSizeSlider.setVisible(! araContext);
    loadAraButton.setVisible(araContext);
    recordButton.setVisible(! araContext);
    stopButton.setVisible(! araContext);
    clearButton.setVisible(! araContext);

    if (araContext && std::abs(waveformSizeSlider.getValue() - 1.0) > 1.0e-6)
        waveformSizeSlider.setValue(1.0, juce::sendNotificationSync);

    workflowLabel.setText(araContext
        ? juce::String(QQDEBREATH_PLUGIN_VERSION) + " | " + QQDEBREATH_APP_BRIDGE_VERSION + " | Click: move playhead; Drag region: move; Drag edge: resize; Shift+drag: create/replace; Ctrl+Z/Ctrl+Shift+Z: Undo/Redo; Right-click: Breath/Noize; Delete: remove."
        : juce::String(QQDEBREATH_PLUGIN_VERSION) + " | " + QQDEBREATH_APP_BRIDGE_VERSION + " | Click: preview seek; Drag region: move; Drag edge: resize; Shift+drag: create/replace; Ctrl+Z/Ctrl+Shift+Z: Undo/Redo; Global EQ/Gain = all Breath.",
        juce::dontSendNotification);

    refreshBreathDetailControlsVisibility();
}

void QQDeBreathAudioProcessorEditor::waveformSelectionChanged(int index)
{
    if (showingBreathDetailPage)
    {
        if (index >= 0)
        {
            const auto candidate = waveformEditor.getRegion(index);
            if (candidate.endTime > candidate.startTime && candidate.type.equalsIgnoreCase("Breath"))
                selectedRegionIndex = index;
        }

        refreshBreathDetailUi();
        if (breathEqLoopButton.getToggleState())
            setBreathEqLoopEnabled(true);
        return;
    }

    selectedRegionIndex = index;

    refreshBreathDetailControlsVisibility();
}

bool QQDeBreathAudioProcessorEditor::selectedRegionIsBreath() const
{
    if (selectedRegionIndex < 0)
        return false;

    const auto region = waveformEditor.getRegion(selectedRegionIndex);
    return region.endTime > region.startTime && region.type.equalsIgnoreCase("Breath");
}

void QQDeBreathAudioProcessorEditor::refreshBreathDetailControlsVisibility()
{
    if (showingBreathDetailPage || showingBreathEqPage)
        return;

    const auto visible = ! showingBreathEqPage && ! showingBreathDetailPage && selectedRegionIsBreath();
    for (auto* component : {
             static_cast<juce::Component*>(&breathDetailBypassButton),
             static_cast<juce::Component*>(&breathDetailButton),
             static_cast<juce::Component*>(&breathDetailTopGainLabel),
             static_cast<juce::Component*>(&breathDetailGainSlider) })
    {
        component->setVisible(visible);
        component->setEnabled(visible);
    }

    if (visible)
        refreshBreathDetailUi();
}

void QQDeBreathAudioProcessorEditor::refreshBreathDetailUi()
{
    updatingBreathDetailUi = true;

    if (! selectedRegionIsBreath())
    {
        breathDetailTitleLabel.setText("Breath Adjust", juce::dontSendNotification);
        breathDetailButton.setToggleState(false, juce::dontSendNotification);
        breathDetailBypassButton.setToggleState(false, juce::dontSendNotification);
        breathDetailEqPowerButton.setToggleState(false, juce::dontSendNotification);
        breathDetailEqEnableButton.setToggleState(false, juce::dontSendNotification);
        breathDetailGainSlider.setValue(0.0, juce::dontSendNotification);
        breathDetailEqEditor.setState({}, false);
        updatingBreathDetailUi = false;
        return;
    }

    const auto region = waveformEditor.getRegion(selectedRegionIndex);
    breathDetailTitleLabel.setText("Selected Breath "
                                   + juce::String(selectedRegionIndex + 1)
                                   + "  "
                                   + juce::String(region.startTime, 3)
                                   + " - "
                                   + juce::String(region.endTime, 3)
                                   + " s",
                                   juce::dontSendNotification);
    breathDetailGainSlider.setValue(juce::jlimit(-30.0, 30.0, region.gainDb), juce::dontSendNotification);
    breathDetailBypassButton.setToggleState(region.eqState.enabled, juce::dontSendNotification);
    breathDetailEqPowerButton.setToggleState(region.eqState.enabled, juce::dontSendNotification);
    breathDetailEqEnableButton.setToggleState(false, juce::dontSendNotification);
    breathDetailButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xff475569));
    breathDetailEqEditor.setState(region.eqState, false);
    updatingBreathDetailUi = false;
}

void QQDeBreathAudioProcessorEditor::applyBreathDetailFromUi()
{
    if (updatingBreathDetailUi || ! selectedRegionIsBreath())
        return;

    detailPostSpectrumPeak.clear();
    auto eq = breathDetailEqEditor.getState();
    const auto eqEnabled = showingBreathDetailPage ? breathDetailEqPowerButton.getToggleState()
                                                   : breathDetailBypassButton.getToggleState();
    breathDetailBypassButton.setToggleState(eqEnabled, juce::dontSendNotification);
    breathDetailEqPowerButton.setToggleState(eqEnabled, juce::dontSendNotification);
    eq.enabled = eqEnabled;
    eq.bypassed = false;
    const auto autoApply = ! showingBreathDetailPage || breathDetailAutoApplyButton.getToggleState();
    const auto waveformEq = autoApply ? eq : breathDetailSnapshotEq;
    waveformEditor.setRegionProcessing(selectedRegionIndex, breathDetailGainSlider.getValue(), waveformEq, false, false);
    updateBreathDetailPreviewForListening(breathDetailGainSlider.getValue(), eq);
    syncAraPlaybackParams();
    breathDetailPreviewDirty = showingBreathDetailPage && ! autoApply;
    if (autoApply)
    {
        breathDetailSnapshotEq = eq;
        breathDetailSnapshotGainDb = breathDetailGainSlider.getValue();
        requestDeferredWaveformRefresh(true);
    }
    else
    {
        requestDeferredWaveformRefresh(false);
    }
    if (showingBreathDetailPage)
        requestDeferredSpectrumRefresh();
}

void QQDeBreathAudioProcessorEditor::updateBreathDetailPreviewForListening(double gainDb, const QQDeBreathEqState& eqState)
{
    if (! selectedRegionIsBreath())
        return;

    auto regions = waveformEditor.getRegions();
    if (selectedRegionIndex < 0 || selectedRegionIndex >= regions.size())
        return;

    auto& region = regions.getReference(selectedRegionIndex);
    region.gainDb = juce::jlimit(-30.0, 30.0, gainDb);
    region.eqState = sanitizeBreathEqState(eqState);
    audioProcessor.updateAnalysisRegionsPreservingCaches(regions);
    requestDeferredAraRuntimeUpdate();
}

void QQDeBreathAudioProcessorEditor::applyBreathDetailPreviewToWaveform()
{
    if (! selectedRegionIsBreath())
        return;

    auto eq = breathDetailEqEditor.getState();
    eq.enabled = breathDetailEqPowerButton.getToggleState();
    eq.bypassed = false;

    waveformEditor.setRegionProcessing(selectedRegionIndex, breathDetailGainSlider.getValue(), eq, true, true);
    audioProcessor.updateAnalysisRegionsPreservingCaches(waveformEditor.getRegions());
    updateAraRuntimeState();
    syncAraPlaybackParams();
    persistAraState();

    const auto region = waveformEditor.getRegion(selectedRegionIndex);
    breathDetailSnapshotIndex = selectedRegionIndex;
    breathDetailSnapshotGainDb = region.gainDb;
    breathDetailSnapshotEq = region.eqState;
    breathDetailPreviewDirty = false;
    refreshBreathDetailUi();
    refreshBreathEqSpectrum();
}

void QQDeBreathAudioProcessorEditor::rollbackBreathDetailPreviewIfNeeded()
{
    if (! showingBreathDetailPage || ! breathDetailPreviewDirty || breathDetailAutoApplyButton.getToggleState())
        return;

    if (breathDetailSnapshotIndex >= 0)
    {
        selectedRegionIndex = breathDetailSnapshotIndex;
        waveformEditor.setRegionProcessing(breathDetailSnapshotIndex, breathDetailSnapshotGainDb, breathDetailSnapshotEq, true, true);
        audioProcessor.updateAnalysisRegionsPreservingCaches(waveformEditor.getRegions());
        updateAraRuntimeState();
        persistAraState();
    }

    breathDetailPreviewDirty = false;
    refreshBreathDetailUi();
    refreshBreathEqSpectrum();
}

QQDeBreathARADocumentController* QQDeBreathAudioProcessorEditor::getAraDocumentController() const
{
    auto* editorView = getARAEditorView();
    if (editorView == nullptr)
        return nullptr;

    auto* documentController = editorView->getDocumentController();
    if (documentController == nullptr)
        return nullptr;

    return juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<QQDeBreathARADocumentController>(documentController);
}

void QQDeBreathAudioProcessorEditor::persistAraState()
{
    if (! isAraContext() || araSourceInfo.sourceFingerprint.isEmpty())
        return;

    const auto result = audioProcessor.getAnalysisResult();
    if (auto* documentController = getAraDocumentController())
    {
        const auto peaks = waveformEditor.buildRegionPeakCache(result.regions);
        documentController->upsertPersistentState(araSourceInfo, result, buildAraPlaybackParams(), peaks);
    }

    if (araSourceInfo.isComposite)
        persistAraMappedSourceStates(result);
}

void QQDeBreathAudioProcessorEditor::persistAraMappedSourceStates(const QQDeBreathBridgeAnalysisResult& result)
{
    if (araSourceInfo.playbackMappings.isEmpty())
        return;

    auto* documentController = getAraDocumentController();
    if (documentController == nullptr)
        return;

    for (const auto& mapping : araSourceInfo.playbackMappings)
    {
        if (mapping.sourceFingerprint.isEmpty() || mapping.sourceSampleRate <= 0.0)
            continue;

        QQDeBreathBridgeAnalysisResult mappedResult;
        juce::Array<QQDeBreathBridgeRegion> compositePeakRegions;
        mappedResult.hasResult = result.hasResult;
        mappedResult.succeeded = result.succeeded;
        mappedResult.cancelled = result.cancelled;
        mappedResult.sourceKey = mapping.sourceFingerprint;
        mappedResult.status = result.status;
        mappedResult.errorMessage = result.errorMessage;
        mappedResult.schemaVersion = result.schemaVersion;
        mappedResult.sampleRate = static_cast<int>(std::round(mapping.sourceSampleRate));
        mappedResult.channels = mapping.sourceChannelCount;
        mappedResult.numSamples = mapping.sourceNumSamples;
        mappedResult.durationSeconds = mapping.sourceSampleRate > 0.0
                                     ? static_cast<double>(mapping.sourceNumSamples) / mapping.sourceSampleRate
                                     : 0.0;

        for (const auto& region : result.regions)
        {
            const auto overlapStart = juce::jmax(region.startTime, mapping.compositeStartSeconds);
            const auto overlapEnd = juce::jmin(region.endTime, mapping.compositeEndSeconds);
            if (overlapEnd - overlapStart < 0.001)
                continue;

            auto mappedRegion = region;
            mappedRegion.startTime = mapping.sourceStartSeconds + (overlapStart - mapping.compositeStartSeconds);
            mappedRegion.endTime = mapping.sourceStartSeconds + (overlapEnd - mapping.compositeStartSeconds);
            mappedRegion.startSample = static_cast<juce::int64>(std::llround(mappedRegion.startTime * mapping.sourceSampleRate));
            mappedRegion.endSample = static_cast<juce::int64>(std::llround(mappedRegion.endTime * mapping.sourceSampleRate));
            mappedRegion.startSample = juce::jlimit<juce::int64>(0, mapping.sourceNumSamples, mappedRegion.startSample);
            mappedRegion.endSample = juce::jlimit<juce::int64>(mappedRegion.startSample, mapping.sourceNumSamples, mappedRegion.endSample);
            mappedResult.regions.add(mappedRegion);

            auto peakRegion = mappedRegion;
            peakRegion.startTime = overlapStart;
            peakRegion.endTime = overlapEnd;
            peakRegion.startSample = static_cast<juce::int64>(std::llround(overlapStart * araSourceInfo.sampleRate));
            peakRegion.endSample = static_cast<juce::int64>(std::llround(overlapEnd * araSourceInfo.sampleRate));
            compositePeakRegions.add(peakRegion);

            if (mappedRegion.type.equalsIgnoreCase("Breath"))
                ++mappedResult.breathCount;
            else if (mappedRegion.type.equalsIgnoreCase("Noize"))
                ++mappedResult.noizeCount;
        }

        QQDeBreathARASourceInfo sourceInfo;
        sourceInfo.name = mapping.sourceName;
        sourceInfo.persistentId = mapping.persistentId;
        sourceInfo.sourceFingerprint = mapping.sourceFingerprint;
        sourceInfo.sampleRate = mapping.sourceSampleRate;
        sourceInfo.channelCount = mapping.sourceChannelCount;
        sourceInfo.numSamples = mapping.sourceNumSamples;
        sourceInfo.durationSeconds = mappedResult.durationSeconds;
        sourceInfo.exportStatus = "Mapped from selected ARA events.";
        documentController->upsertPersistentState(sourceInfo, mappedResult, buildAraPlaybackParams(),
                                                  waveformEditor.buildRegionPeakCache(compositePeakRegions));
    }
}

void QQDeBreathAudioProcessorEditor::updateAraRuntimeState()
{
    if (! isAraContext() || araSourceInfo.sourceFingerprint.isEmpty())
        return;

    const auto result = audioProcessor.getAnalysisResult();
    auto* documentController = getAraDocumentController();
    if (documentController == nullptr)
        return;

    const auto peaks = waveformEditor.buildRegionPeakCache(result.regions);
    documentController->updateRuntimePersistentState(araSourceInfo, result, buildAraPlaybackParams(), peaks);

    if (araSourceInfo.isComposite)
        updateAraRuntimeMappedSourceStates(result);
}

void QQDeBreathAudioProcessorEditor::updateAraRuntimeMappedSourceStates(const QQDeBreathBridgeAnalysisResult& result)
{
    if (araSourceInfo.playbackMappings.isEmpty())
        return;

    auto* documentController = getAraDocumentController();
    if (documentController == nullptr)
        return;

    for (const auto& mapping : araSourceInfo.playbackMappings)
    {
        if (mapping.sourceFingerprint.isEmpty() || mapping.sourceSampleRate <= 0.0)
            continue;

        QQDeBreathBridgeAnalysisResult mappedResult;
        juce::Array<QQDeBreathBridgeRegion> compositePeakRegions;
        mappedResult.hasResult = result.hasResult;
        mappedResult.succeeded = result.succeeded;
        mappedResult.cancelled = result.cancelled;
        mappedResult.sourceKey = mapping.sourceFingerprint;
        mappedResult.status = result.status;
        mappedResult.errorMessage = result.errorMessage;
        mappedResult.schemaVersion = result.schemaVersion;
        mappedResult.sampleRate = static_cast<int>(std::round(mapping.sourceSampleRate));
        mappedResult.channels = mapping.sourceChannelCount;
        mappedResult.numSamples = mapping.sourceNumSamples;
        mappedResult.durationSeconds = mapping.sourceSampleRate > 0.0
                                     ? static_cast<double>(mapping.sourceNumSamples) / mapping.sourceSampleRate
                                     : 0.0;

        for (const auto& region : result.regions)
        {
            const auto overlapStart = juce::jmax(region.startTime, mapping.compositeStartSeconds);
            const auto overlapEnd = juce::jmin(region.endTime, mapping.compositeEndSeconds);
            if (overlapEnd - overlapStart < 0.001)
                continue;

            auto mappedRegion = region;
            mappedRegion.startTime = mapping.sourceStartSeconds + (overlapStart - mapping.compositeStartSeconds);
            mappedRegion.endTime = mapping.sourceStartSeconds + (overlapEnd - mapping.compositeStartSeconds);
            mappedRegion.startSample = static_cast<juce::int64>(std::llround(mappedRegion.startTime * mapping.sourceSampleRate));
            mappedRegion.endSample = static_cast<juce::int64>(std::llround(mappedRegion.endTime * mapping.sourceSampleRate));
            mappedRegion.startSample = juce::jlimit<juce::int64>(0, mapping.sourceNumSamples, mappedRegion.startSample);
            mappedRegion.endSample = juce::jlimit<juce::int64>(mappedRegion.startSample, mapping.sourceNumSamples, mappedRegion.endSample);
            mappedResult.regions.add(mappedRegion);

            auto peakRegion = mappedRegion;
            peakRegion.startTime = overlapStart;
            peakRegion.endTime = overlapEnd;
            peakRegion.startSample = static_cast<juce::int64>(std::llround(overlapStart * araSourceInfo.sampleRate));
            peakRegion.endSample = static_cast<juce::int64>(std::llround(overlapEnd * araSourceInfo.sampleRate));
            compositePeakRegions.add(peakRegion);

            if (mappedRegion.type.equalsIgnoreCase("Breath"))
                ++mappedResult.breathCount;
            else if (mappedRegion.type.equalsIgnoreCase("Noize"))
                ++mappedResult.noizeCount;
        }

        QQDeBreathARASourceInfo sourceInfo;
        sourceInfo.name = mapping.sourceName;
        sourceInfo.persistentId = mapping.persistentId;
        sourceInfo.sourceFingerprint = mapping.sourceFingerprint;
        sourceInfo.sampleRate = mapping.sourceSampleRate;
        sourceInfo.channelCount = mapping.sourceChannelCount;
        sourceInfo.numSamples = mapping.sourceNumSamples;
        sourceInfo.durationSeconds = mappedResult.durationSeconds;
        sourceInfo.exportStatus = "Mapped from selected ARA events.";
        documentController->updateRuntimePersistentState(sourceInfo, mappedResult, buildAraPlaybackParams(),
                                                         waveformEditor.buildRegionPeakCache(compositePeakRegions));
    }
}

void QQDeBreathAudioProcessorEditor::restoreAraStateIfNeeded()
{
    if (! isAraContext())
        return;

    if (sourceMode == SourceMode::ara && araSourceInfo.exportedWav.existsAsFile()
        && araPlaybackCacheReady)
        return;

    const auto now = juce::Time::getMillisecondCounter();
    if (lastAraRestoreAttemptMs != 0 && now - lastAraRestoreAttemptMs < 1000u)
        return;

    lastAraRestoreAttemptMs = now;

    auto* documentController = getAraDocumentController();
    if (documentController == nullptr)
        return;

    auto* source = findCurrentAudioSource();
    QQDeBreathARAPersistentState restoredState;
    auto haveState = false;

    if (source != nullptr)
        haveState = documentController->getPersistentStateForSource(buildAraSourceFingerprint(*source), restoredState);

    if (! haveState || restoredState.sourceInfo.sourceFingerprint.isEmpty())
        return;

    if (source == nullptr || buildAraSourceFingerprint(*source) != restoredState.sourceInfo.sourceFingerprint)
    {
        if (auto* editorView = getARAEditorView())
        {
            if (auto* araDocumentController = editorView->getDocumentController())
            {
                if (auto* document = araDocumentController->getDocument<juce::ARADocument>())
                {
                    for (auto* candidate : document->getAudioSources<juce::ARAAudioSource>())
                    {
                        if (candidate != nullptr && buildAraSourceFingerprint(*candidate) == restoredState.sourceInfo.sourceFingerprint)
                        {
                            source = candidate;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (source == nullptr)
        return;

    auto restoredInfo = restoredState.sourceInfo;
    restoredInfo.name = restoredInfo.name.isNotEmpty() ? restoredInfo.name : makeAraSourceName(*source);
    restoredInfo.persistentId = restoredInfo.persistentId.isNotEmpty()
                              ? restoredInfo.persistentId
                              : juce::String::fromUTF8(source->getPersistentID().c_str());
    restoredInfo.sampleRate = restoredInfo.sampleRate > 0.0 ? restoredInfo.sampleRate : source->getSampleRate();
    restoredInfo.channelCount = restoredInfo.channelCount > 0 ? restoredInfo.channelCount : static_cast<int>(source->getChannelCount());
    restoredInfo.numSamples = restoredInfo.numSamples > 0 ? restoredInfo.numSamples : static_cast<juce::int64>(source->getSampleCount());
    restoredInfo.durationSeconds = restoredInfo.durationSeconds > 0.0
                                 ? restoredInfo.durationSeconds
                                 : (restoredInfo.sampleRate > 0.0 ? static_cast<double>(restoredInfo.numSamples) / restoredInfo.sampleRate : 0.0);

    if (! restoredInfo.exportedWav.existsAsFile())
    {
        if (! exportAraSourceToWav(*source, restoredInfo))
        {
            statusLabel.setText("Status: " + restoredInfo.exportStatus, juce::dontSendNotification);
            return;
        }
    }

    juce::String cacheStatus;
    if (! cacheAraSourceForPlayback(*source, cacheStatus))
    {
        statusLabel.setText("Status: " + cacheStatus, juce::dontSendNotification);
        return;
    }
    araPlaybackCacheReady = true;

    auto restoredAnalysis = restoredState.analysisResult;
    if (restoredAnalysis.hasResult && restoredAnalysis.sourceKey.isEmpty())
        restoredAnalysis.sourceKey = restoredInfo.sourceFingerprint;

    sourceMode = SourceMode::ara;
    araSourceInfo = restoredInfo;
    araSourceInfo.exportStatus = "Restored ARA source from project: " + araSourceInfo.name;
    lastExportedFile = araSourceInfo.exportedWav;
    lastDisplayedRecordedSamples = 0;
    showingLiveRecordingPreview = false;

    juce::String waveformStatus;
    waveformEditor.loadAudioFile(araSourceInfo.exportedWav, waveformStatus);
    audioProcessor.setAraSourceInfo(araSourceInfo);
    audioProcessor.setAnalysisResult(restoredAnalysis);
    waveformEditor.setAnalysisResult(restoredAnalysis);
    selectedRegionIndex = -1;

    const auto restoredParams = restoredState.playbackParams;
    monitorVoiceButton.setToggleState(restoredParams.monitorVoice, juce::sendNotificationSync);
    monitorBreathButton.setToggleState(restoredParams.monitorBreath, juce::sendNotificationSync);
    monitorNoizeButton.setToggleState(restoredParams.monitorNoize, juce::sendNotificationSync);
    followButton.setToggleState(restoredParams.followPlayhead, juce::sendNotificationSync);
    fadeButton.setToggleState(restoredParams.enableFade, juce::sendNotificationSync);
    breathNormButton.setToggleState(restoredParams.normalizeBreath, juce::sendNotificationSync);
    fadeInSlider.setValue(restoredParams.fadeInMs, juce::sendNotificationSync);
    fadeOutSlider.setValue(restoredParams.fadeOutMs, juce::sendNotificationSync);
    breathTargetSlider.setValue(restoredParams.breathTargetDb, juce::sendNotificationSync);
    breathGainSlider.setValue(restoredParams.breathGainDb, juce::sendNotificationSync);
    waveformSizeSlider.setValue(restoredParams.waveformDisplayGain, juce::sendNotificationSync);
    audioProcessor.setBreathEqState(restoredParams.breathEqState);
    refreshBreathEqUi();
    refreshBreathEqSpectrum();
}

QQDeBreathARAPlaybackParams QQDeBreathAudioProcessorEditor::buildAraPlaybackParams() const
{
    QQDeBreathARAPlaybackParams params;
    params.monitorVoice = monitorVoiceButton.getToggleState();
    params.monitorBreath = monitorBreathButton.getToggleState();
    params.monitorNoize = monitorNoizeButton.getToggleState();
    params.followPlayhead = followButton.getToggleState();
    params.enableFade = fadeButton.getToggleState();
    params.normalizeBreath = breathNormButton.getToggleState();
    params.fadeInMs = fadeInSlider.getValue();
    params.fadeOutMs = fadeOutSlider.getValue();
    params.breathTargetDb = breathTargetSlider.getValue();
    params.breathGainDb = juce::jlimit(-60.0, 30.0, breathGainSlider.getValue());
    params.waveformDisplayGain = waveformSizeSlider.getValue();
    params.breathEqState = audioProcessor.getBreathEqState();
    return params;
}

void QQDeBreathAudioProcessorEditor::syncAraPlaybackParams()
{
    auto* documentController = getAraDocumentController();
    if (documentController == nullptr)
        return;

    const auto params = buildAraPlaybackParams();
    if (isAraContext() && araSourceInfo.sourceFingerprint.isNotEmpty())
    {
        documentController->setPlaybackParamsForSource(araSourceInfo, params);
        for (const auto& mapping : araSourceInfo.playbackMappings)
        {
            QQDeBreathARASourceInfo mappedInfo;
            mappedInfo.name = mapping.sourceName;
            mappedInfo.persistentId = mapping.persistentId;
            mappedInfo.sourceFingerprint = mapping.sourceFingerprint;
            mappedInfo.sampleRate = mapping.sourceSampleRate;
            mappedInfo.channelCount = mapping.sourceChannelCount;
            mappedInfo.numSamples = mapping.sourceNumSamples;
            mappedInfo.durationSeconds = mapping.sourceSampleRate > 0.0
                                       ? static_cast<double>(mapping.sourceNumSamples) / mapping.sourceSampleRate
                                       : 0.0;
            documentController->setPlaybackParamsForSource(mappedInfo, params);
        }
        return;
    }

    documentController->setPlaybackParams(params);
}

void QQDeBreathAudioProcessorEditor::refreshRecordedWaveformIfNeeded(const QQDeBreathAudioProcessor::RecordedBufferInfo& info)
{
    if (sourceMode == SourceMode::ara)
        return;

    if (! info.hasRecording)
    {
        showingLiveRecordingPreview = false;
        return;
    }

    const auto now = juce::Time::getMillisecondCounter();
    const auto isLiveCapture = info.isRecording || info.isRecordArmed;
    if (isLiveCapture)
    {
        showingLiveRecordingPreview = true;
        lastWaveformRefreshMs = now;
        lastExportedFile = juce::File();
        return;
    }

    if (info.numSamples == lastDisplayedRecordedSamples && (isLiveCapture || ! showingLiveRecordingPreview))
        return;

    juce::AudioBuffer<float> snapshot;
    double sampleRate = 0.0;
    const auto copied = audioProcessor.copyRecordedBuffer(snapshot, sampleRate);

    if (copied)
    {
        waveformEditor.setAudioBuffer(snapshot, sampleRate, info.durationSeconds, isLiveCapture);
        if (! isLiveCapture)
            waveformEditor.setAnalysisResult(audioProcessor.getAnalysisResult());
        lastDisplayedRecordedSamples = info.numSamples;
        lastWaveformRefreshMs = now;
        showingLiveRecordingPreview = isLiveCapture;
        lastExportedFile = juce::File();
    }
}

void QQDeBreathAudioProcessorEditor::applyBreathEqStateFromUi(const QQDeBreathEqState& state)
{
    globalPostSpectrumPeak.clear();
    auto sanitized = sanitizeBreathEqState(state);
    sanitized.bypassed = false;
    audioProcessor.setBreathEqState(sanitized);
    breathEqMainEnableButton.setToggleState(sanitized.enabled, juce::dontSendNotification);
    breathEqEnableButton.setToggleState(sanitized.enabled, juce::dontSendNotification);
    breathEqHighPassButton.setToggleState(sanitized.highPassEnabled, juce::dontSendNotification);
    breathEqLowPassButton.setToggleState(sanitized.lowPassEnabled, juce::dontSendNotification);
    if (! showingBreathEqPage)
        breathEqEditor.setState(sanitized, false);
    syncAraPlaybackParams();
    if (! showingBreathEqPage || breathEqAutoApplyButton.getToggleState())
    {
        breathEqPageSnapshot = sanitized;
        breathEqPreviewDirty = false;
        requestDeferredWaveformRefresh(false);
    }
    else
    {
        breathEqPreviewDirty = showingBreathEqPage;
    }

    requestDeferredSpectrumRefresh();
}

void QQDeBreathAudioProcessorEditor::applyBreathEqPreviewToWaveform()
{
    breathEqPageSnapshot = audioProcessor.getBreathEqState();
    breathEqPreviewDirty = false;
    syncAraPlaybackParams();
    waveformEditor.refreshProcessedDisplay();
    refreshBreathEqSpectrum();
}

void QQDeBreathAudioProcessorEditor::saveCurrentGlobalDefaults()
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("schema_version", 2);
    root->setProperty("monitor_voice", boolVar(monitorVoiceButton.getToggleState()));
    root->setProperty("monitor_breath", boolVar(monitorBreathButton.getToggleState()));
    root->setProperty("monitor_noize", boolVar(monitorNoizeButton.getToggleState()));
    root->setProperty("follow", boolVar(followButton.getToggleState()));
    root->setProperty("fade", boolVar(fadeButton.getToggleState()));
    root->setProperty("fade_in_ms", doubleVar(fadeInSlider.getValue()));
    root->setProperty("fade_out_ms", doubleVar(fadeOutSlider.getValue()));
    root->setProperty("breath_norm", boolVar(breathNormButton.getToggleState()));
    root->setProperty("breath_target_db", doubleVar(breathTargetSlider.getValue()));
    root->setProperty("global_gain_db", doubleVar(breathGainSlider.getValue()));
    const auto autoApply = breathEqAutoApplyButton.getToggleState();
    root->setProperty("auto_apply", boolVar(autoApply));
    root->setProperty("global_auto_apply", boolVar(autoApply));
    root->setProperty("selected_auto_apply", boolVar(autoApply));
    root->setProperty("global_eq", serializeBreathEqState(audioProcessor.getBreathEqState()));

    const auto file = globalDefaultsFile();
    file.getParentDirectory().createDirectory();
    const auto ok = file.replaceWithText(juce::JSON::toString(juce::var(root.release()), true), false, false, "\n");
    statusLabel.setText(ok ? "Status: Global defaults saved."
                           : "Status: Could not save global defaults.",
                        juce::dontSendNotification);
}

void QQDeBreathAudioProcessorEditor::applySavedGlobalDefaultsIfFreshInstance()
{
    if (attemptedGlobalDefaultsLoad)
        return;

    attemptedGlobalDefaultsLoad = true;

    // The processor outlives editor windows. Claim this once per plugin instance
    // so reopening the same editor cannot overwrite its live APVTS state with defaults.
    if (! audioProcessor.claimInitialGlobalDefaultsApplication())
        return;

    auto araHasProjectState = false;
    if (auto* documentController = getAraDocumentController())
        araHasProjectState = documentController->hasRestoredPlaybackParams();

    if (audioProcessor.hasRestoredStateInformation() || araHasProjectState)
        return;

    applySavedGlobalDefaults();
}

void QQDeBreathAudioProcessorEditor::applySavedGlobalDefaults()
{
    const auto file = globalDefaultsFile();
    auto parsed = file.existsAsFile() ? juce::JSON::parse(file) : juce::var();
    if (! parsed.isObject())
        parsed = juce::var(new juce::DynamicObject());

    applyGlobalDefaultsFromValue(parsed);
}

void QQDeBreathAudioProcessorEditor::applyGlobalDefaultsFromValue(const juce::var& value)
{
    const auto boolProp = [&value](const char* name, bool fallback)
    {
        return static_cast<bool>(value.getProperty(name, fallback));
    };
    const auto doubleProp = [&value](const char* name, double fallback)
    {
        return static_cast<double>(value.getProperty(name, fallback));
    };

    monitorVoiceButton.setToggleState(boolProp("monitor_voice", true), juce::sendNotificationSync);
    monitorBreathButton.setToggleState(boolProp("monitor_breath", true), juce::sendNotificationSync);
    monitorNoizeButton.setToggleState(boolProp("monitor_noize", true), juce::sendNotificationSync);
    followButton.setToggleState(boolProp("follow", false), juce::sendNotificationSync);
    fadeButton.setToggleState(boolProp("fade", true), juce::sendNotificationSync);
    fadeInSlider.setValue(juce::jlimit(0.0, 200.0, doubleProp("fade_in_ms", 10.0)), juce::sendNotificationSync);
    fadeOutSlider.setValue(juce::jlimit(0.0, 200.0, doubleProp("fade_out_ms", 10.0)), juce::sendNotificationSync);
    breathNormButton.setToggleState(boolProp("breath_norm", false), juce::sendNotificationSync);
    breathTargetSlider.setValue(juce::jlimit(-80.0, 0.0, doubleProp("breath_target_db", -6.0)), juce::sendNotificationSync);
    breathGainSlider.setValue(juce::jlimit(-60.0, 30.0, doubleProp("global_gain_db", 0.0)), juce::sendNotificationSync);
    const auto legacyAutoApply = boolProp("global_auto_apply", boolProp("selected_auto_apply", true));
    setAutoApplyEnabled(boolProp("auto_apply", legacyAutoApply), false);

    QQDeBreathEqState eq;
    if (deserializeBreathEqState(value.getProperty("global_eq", {}).toString(), eq))
        audioProcessor.setBreathEqState(eq);
    else
        audioProcessor.setBreathEqState({});

    refreshBreathEqUi();
    syncAraPlaybackParams();
    waveformEditor.refreshProcessedDisplay();
}

void QQDeBreathAudioProcessorEditor::requestDeferredWaveformRefresh(bool persistAfterRefresh)
{
    pendingWaveformRefresh = true;
    pendingWaveformRefreshShouldPersist = pendingWaveformRefreshShouldPersist || persistAfterRefresh;
    pendingWaveformRefreshMs = juce::Time::getMillisecondCounter();
}

void QQDeBreathAudioProcessorEditor::requestDeferredSpectrumRefresh()
{
    pendingSpectrumRefresh = true;
    pendingSpectrumRefreshMs = juce::Time::getMillisecondCounter();
}

void QQDeBreathAudioProcessorEditor::requestDeferredAraRuntimeUpdate()
{
    pendingAraRuntimeUpdate = true;
    pendingAraRuntimeUpdateMs = juce::Time::getMillisecondCounter();
}

void QQDeBreathAudioProcessorEditor::rollbackBreathEqPreviewIfNeeded()
{
    if (! showingBreathEqPage || ! breathEqPreviewDirty || breathEqAutoApplyButton.getToggleState())
        return;

    audioProcessor.setBreathEqState(breathEqPageSnapshot);
    refreshBreathEqUi();
    syncAraPlaybackParams();
    breathEqPreviewDirty = false;
    refreshBreathEqSpectrum();
}

void QQDeBreathAudioProcessorEditor::setBreathEqEnabled(bool enabled)
{
    auto state = audioProcessor.getBreathEqState();
    state.enabled = enabled;
    state.bypassed = false;
    applyBreathEqStateFromUi(state);
    refreshBreathEqSpectrum();
}

void QQDeBreathAudioProcessorEditor::refreshBreathEqUi()
{
    const auto state = audioProcessor.getBreathEqState();
    breathEqMainEnableButton.setToggleState(state.enabled, juce::dontSendNotification);
    breathEqEnableButton.setToggleState(state.enabled, juce::dontSendNotification);
    breathEqHighPassButton.setToggleState(state.highPassEnabled, juce::dontSendNotification);
    breathEqLowPassButton.setToggleState(state.lowPassEnabled, juce::dontSendNotification);
    breathEqEditor.setState(state, false);
}

void QQDeBreathAudioProcessorEditor::refreshBreathEqSpectrumSource()
{
    const auto info = audioProcessor.getRecordedBufferInfo();
    const auto result = audioProcessor.getAnalysisResult();
    const auto key = getActiveSourceKey(info) + "|"
                   + juce::String(sourceMode == SourceMode::ara ? araSourceInfo.numSamples : info.numSamples) + "|"
                   + juce::String(result.regions.size());

    if (key.isNotEmpty() && key == breathEqSpectrumSourceKey && breathEqSpectrumSourceBuffer.getNumSamples() > 0)
        return;

    breathEqSpectrumSourceKey.clear();
    breathEqSpectrumSampleRate = 0.0;
    breathEqSpectrumSourceBuffer.setSize(0, 0);

    juce::String status;
    if (sourceMode == SourceMode::ara && araSourceInfo.exportedWav.existsAsFile())
    {
        if (! readAudioFile(araSourceInfo.exportedWav, breathEqSpectrumSourceBuffer, breathEqSpectrumSampleRate, status))
            return;
    }
    else
    {
        if (! audioProcessor.copyRecordedBuffer(breathEqSpectrumSourceBuffer, breathEqSpectrumSampleRate))
            return;
    }

    if (breathEqSpectrumSourceBuffer.getNumSamples() > 0 && breathEqSpectrumSampleRate > 0.0)
        breathEqSpectrumSourceKey = key;
}

void QQDeBreathAudioProcessorEditor::refreshBreathEqSpectrum()
{
    auto result = audioProcessor.getAnalysisResult();
    if (! result.succeeded || result.regions.isEmpty())
    {
        breathEqEditor.setSpectrum({});
        return;
    }

    juce::AudioBuffer<float> source;
    double sampleRate = 0.0;
    juce::String status;
    if (sourceMode == SourceMode::ara && araSourceInfo.exportedWav.existsAsFile())
    {
        if (! readAudioFile(araSourceInfo.exportedWav, source, sampleRate, status))
        {
            breathEqEditor.setSpectrum({});
            return;
        }
    }
    else if (! audioProcessor.copyRecordedBuffer(source, sampleRate))
    {
        breathEqEditor.setSpectrum({});
        return;
    }

    if (source.getNumSamples() <= 0 || source.getNumChannels() <= 0 || sampleRate <= 0.0)
    {
        breathEqEditor.setSpectrum({});
        return;
    }

    constexpr auto fftOrder = 11;
    constexpr auto fftSize = 1 << fftOrder;
    constexpr auto spectrumBins = 128;
    constexpr auto maxBlocks = 96;
    juce::dsp::FFT fft(fftOrder);
    std::vector<float> fftData(static_cast<size_t>(fftSize * 2), 0.0f);
    std::vector<double> accum(spectrumBins, 0.0);
    auto blocks = 0;

    for (const auto& region : result.regions)
    {
        if (! region.type.equalsIgnoreCase("Breath") || blocks >= maxBlocks)
            continue;

        auto start = regionStartSample(region, sampleRate, source.getNumSamples());
        auto end = regionEndSample(region, sampleRate, source.getNumSamples());
        if (end <= start + 64)
            continue;

        const auto regionSamples = end - start;
        const auto step = juce::jmax<juce::int64>(fftSize / 2, regionSamples / 3);
        for (auto pos = start; pos < end && blocks < maxBlocks; pos += step)
        {
            auto blockStart = pos;
            if (blockStart + fftSize > end)
                blockStart = juce::jmax<juce::int64>(start, end - fftSize);

            std::fill(fftData.begin(), fftData.end(), 0.0f);
            for (auto i = 0; i < fftSize; ++i)
            {
                const auto sampleIndex = blockStart + i;
                if (sampleIndex < start || sampleIndex >= end || sampleIndex >= source.getNumSamples())
                    continue;

                auto mono = 0.0f;
                for (auto channel = 0; channel < source.getNumChannels(); ++channel)
                    mono += source.getSample(channel, static_cast<int>(sampleIndex)) / static_cast<float>(source.getNumChannels());
                const auto window = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(i) / static_cast<float>(fftSize - 1));
                fftData[static_cast<size_t>(i)] = mono * window;
            }

            fft.performFrequencyOnlyForwardTransform(fftData.data());

            for (auto bin = 1; bin <= fftSize / 2; ++bin)
            {
                const auto frequency = static_cast<double>(bin) * sampleRate / static_cast<double>(fftSize);
                if (frequency < 20.0 || frequency > 20000.0)
                    continue;

                const auto norm = (std::log(frequency) - std::log(20.0)) / (std::log(20000.0) - std::log(20.0));
                const auto index = juce::jlimit(0, spectrumBins - 1, static_cast<int>(std::floor(norm * static_cast<double>(spectrumBins - 1))));
                accum[static_cast<size_t>(index)] += static_cast<double>(fftData[static_cast<size_t>(bin)]);
            }

            ++blocks;
        }
    }

    if (blocks == 0)
    {
        breathEqEditor.setSpectrum({});
        return;
    }

    auto maxDb = -120.0;
    std::vector<float> display(spectrumBins, 0.0f);
    for (auto value : accum)
        maxDb = juce::jmax(maxDb, 20.0 * std::log10(value / static_cast<double>(blocks) + 1.0e-9));

    for (size_t i = 0; i < accum.size(); ++i)
    {
        const auto db = 20.0 * std::log10(accum[i] / static_cast<double>(blocks) + 1.0e-9);
        display[i] = static_cast<float>(juce::jlimit(0.0, 1.0, (db - (maxDb - 64.0)) / 64.0));
    }

    if (showingBreathEqPage)
    {
        constexpr auto maxPreviewSamples = 16384;
        juce::AudioBuffer<float> globalPre(source.getNumChannels(), maxPreviewSamples);
        globalPre.clear();
        auto writeOffset = 0;

        for (const auto& region : result.regions)
        {
            if (! region.type.equalsIgnoreCase("Breath") || writeOffset >= maxPreviewSamples)
                continue;

            const auto start = regionStartSample(region, sampleRate, source.getNumSamples());
            const auto end = regionEndSample(region, sampleRate, source.getNumSamples());
            const auto count = juce::jmin<int>(maxPreviewSamples - writeOffset,
                                               static_cast<int>(juce::jmax<juce::int64>(0, end - start)));
            for (auto channel = 0; channel < source.getNumChannels(); ++channel)
                for (auto i = 0; i < count; ++i)
                    globalPre.setSample(channel, writeOffset + i, sampleAt(source, channel, start + i));

            writeOffset += count;
        }

        if (writeOffset > 64)
        {
            juce::AudioBuffer<float> globalPost;
            globalPost.makeCopyOf(globalPre, true);
            audioProcessor.applyBreathEqToBuffer(globalPost, sampleRate);

            std::vector<float> preSpectrum;
            std::vector<float> postSpectrum;
            spectraFromBuffers(globalPre, globalPost, sampleRate, preSpectrum, postSpectrum);
            globalPreSpectrumPeak = preSpectrum;
            globalPostSpectrumPeak = postSpectrum;
            breathEqEditor.setSpectra(preSpectrum, postSpectrum);
        }
        else
        {
            breathEqEditor.setSpectrum(display);
        }
    }
    else
    {
        breathEqEditor.setSpectrum(display);
    }

    if (! selectedRegionIsBreath())
    {
        breathDetailEqEditor.setSpectrum({});
        return;
    }

    const auto selected = waveformEditor.getRegion(selectedRegionIndex);
    auto start = regionStartSample(selected, sampleRate, source.getNumSamples());
    auto end = regionEndSample(selected, sampleRate, source.getNumSamples());
    if (end <= start + 64)
    {
        breathDetailEqEditor.setSpectrum({});
        return;
    }

    constexpr auto maxDetailSamples = 8192;
    const auto center = (start + end) / 2;
    auto windowStart = juce::jmax<juce::int64>(start, center - maxDetailSamples / 2);
    auto windowEnd = juce::jmin<juce::int64>(end, windowStart + maxDetailSamples);
    windowStart = juce::jmax<juce::int64>(start, windowEnd - maxDetailSamples);
    const auto detailSamples = static_cast<int>(windowEnd - windowStart);
    if (detailSamples <= 64)
    {
        breathDetailEqEditor.setSpectrum({});
        return;
    }

    const auto enableFade = audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::enableFade)->load() >= 0.5f;
    const auto normalizeBreath = audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::normalizeBreath)->load() >= 0.5f;
    const auto breathTargetDb = static_cast<double>(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::breathTargetDb)->load());
    const auto breathTargetGain = dbToGain(breathTargetDb);
    const auto globalGain = dbToGain(juce::jlimit(-60.0, 30.0, static_cast<double>(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::breathGainDb)->load())));
    const auto selectedGainDb = showingBreathDetailPage ? breathDetailGainSlider.getValue() : selected.gainDb;
    const auto selectedGain = dbToGain(juce::jlimit(-30.0, 30.0, selectedGainDb));
    const auto fadeInSamples = enableFade
                             ? static_cast<int>(std::llround(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::fadeInMs)->load() * sampleRate / 1000.0))
                             : 0;
    const auto fadeOutSamples = enableFade
                              ? static_cast<int>(std::llround(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::fadeOutMs)->load() * sampleRate / 1000.0))
                              : 0;
    auto selectedPeak = 0.0f;
    for (auto channel = 0; channel < source.getNumChannels(); ++channel)
        for (auto sample = start; sample < end; ++sample)
            selectedPeak = juce::jmax(selectedPeak, std::abs(sampleAt(source, channel, sample)));
    const auto normGain = normalizeBreath && selectedPeak > 1.0e-9f
                        ? breathTargetGain / static_cast<double>(selectedPeak)
                        : 1.0;

    juce::AudioBuffer<float> detailPre(source.getNumChannels(), detailSamples);
    for (auto channel = 0; channel < source.getNumChannels(); ++channel)
    {
        for (auto i = 0; i < detailSamples; ++i)
        {
            const auto sourceSample = windowStart + i;
            const auto weight = regionWeightForIndex(result.regions,
                                                     selectedRegionIndex,
                                                     sourceSample,
                                                     sampleRate,
                                                     source.getNumSamples(),
                                                     fadeInSamples,
                                                     fadeOutSamples);
            const auto dry = static_cast<double>(sampleAt(source, channel, sourceSample));
            detailPre.setSample(channel, i, static_cast<float>(dry * weight * normGain * globalGain));
        }
    }

    audioProcessor.applyBreathEqToBuffer(detailPre, sampleRate);

    juce::AudioBuffer<float> detailPost;
    detailPost.makeCopyOf(detailPre, true);
    auto selectedEq = showingBreathDetailPage ? breathDetailEqEditor.getState() : selected.eqState;
    if (showingBreathDetailPage)
    {
        selectedEq.enabled = breathDetailEqPowerButton.getToggleState();
        selectedEq.bypassed = false;
    }

    if (selectedEq.hasActiveProcessing())
    {
        QQDeBreathEqProcessor regionProcessor;
        regionProcessor.prepare(sampleRate, detailPost.getNumChannels(), selectedEq);
        regionProcessor.process(detailPost);
    }

    detailPost.applyGain(static_cast<float>(selectedGain));

    std::vector<float> detailPreSpectrum;
    std::vector<float> detailPostSpectrum;
    spectraFromBuffers(detailPre, detailPost, sampleRate, detailPreSpectrum, detailPostSpectrum);
    detailPreSpectrumPeak = detailPreSpectrum;
    detailPostSpectrumPeak = detailPostSpectrum;
    breathDetailEqEditor.setSpectra(detailPreSpectrum, detailPostSpectrum);
}

double QQDeBreathAudioProcessorEditor::getCurrentLocalPlayheadSeconds(const QQDeBreathAudioProcessor::RecordedBufferInfo& info) const
{
    const auto hostTimeSeconds = getHostTimeSeconds();
    if (hostTimeSeconds < 0.0)
        return -1.0;

    if (sourceMode == SourceMode::ara && araSourceInfo.exportedWav.existsAsFile())
        return getAraLocalPlayheadSeconds(hostTimeSeconds);

    if (info.hasRecording)
        return audioProcessor.getInternalPreviewPosition(hostTimeSeconds);

    return -1.0;
}

void QQDeBreathAudioProcessorEditor::updateBreathEqDynamicSpectrum()
{
    if (! showingBreathEqPage && ! showingBreathDetailPage)
        return;

    const auto detailPage = showingBreathDetailPage && selectedRegionIsBreath();
    if (showingBreathDetailPage && ! detailPage)
    {
        breathDetailEqEditor.setSpectrum({});
        return;
    }

    const auto now = juce::Time::getMillisecondCounter();
    if (lastBreathEqDynamicSpectrumMs != 0 && now - lastBreathEqDynamicSpectrumMs < 80u)
        return;
    lastBreathEqDynamicSpectrumMs = now;

    const auto result = audioProcessor.getAnalysisResult();
    if (! result.succeeded || result.regions.isEmpty())
        return;

    refreshBreathEqSpectrumSource();
    if (breathEqSpectrumSourceBuffer.getNumSamples() <= 0 || breathEqSpectrumSampleRate <= 0.0)
        return;

    const auto info = audioProcessor.getRecordedBufferInfo();
    const auto localSeconds = getCurrentLocalPlayheadSeconds(info);
    if (localSeconds < 0.0)
        return;

    constexpr auto fftSize = 2048;
    const auto channels = breathEqSpectrumSourceBuffer.getNumChannels();
    juce::AudioBuffer<float> pre(channels, fftSize);
    pre.clear();
    const auto selectedRegion = detailPage ? waveformEditor.getRegion(selectedRegionIndex) : QQDeBreathBridgeRegion {};
    const auto enableFade = audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::enableFade)->load() >= 0.5f;
    const auto normalizeBreath = audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::normalizeBreath)->load() >= 0.5f;
    const auto breathTargetDb = static_cast<double>(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::breathTargetDb)->load());
    const auto breathTargetGain = dbToGain(breathTargetDb);
    const auto breathAdjustGain = dbToGain(juce::jlimit(-60.0, 30.0, static_cast<double>(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::breathGainDb)->load())));
    const auto fadeInSamples = enableFade
                             ? static_cast<int>(std::llround(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::fadeInMs)->load() * breathEqSpectrumSampleRate / 1000.0))
                             : 0;
    const auto fadeOutSamples = enableFade
                              ? static_cast<int>(std::llround(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::fadeOutMs)->load() * breathEqSpectrumSampleRate / 1000.0))
                              : 0;

    std::vector<double> peakCache(static_cast<size_t>(result.regions.size()), -1.0);
    auto peakForRegion = [&](int regionIndex)
    {
        auto& cached = peakCache[static_cast<size_t>(regionIndex)];
        if (cached >= 0.0)
            return cached;

        const auto& region = result.regions.getReference(regionIndex);
        const auto start = regionStartSample(region, breathEqSpectrumSampleRate, breathEqSpectrumSourceBuffer.getNumSamples());
        const auto end = regionEndSample(region, breathEqSpectrumSampleRate, breathEqSpectrumSourceBuffer.getNumSamples());
        auto peak = 0.0f;
        for (auto channel = 0; channel < breathEqSpectrumSourceBuffer.getNumChannels(); ++channel)
        {
            const auto* data = breathEqSpectrumSourceBuffer.getReadPointer(channel);
            for (auto sample = start; sample < end; ++sample)
                peak = juce::jmax(peak, std::abs(data[static_cast<int>(sample)]));
        }

        cached = static_cast<double>(peak);
        return cached;
    };

    const auto centerSample = static_cast<juce::int64>(std::llround(localSeconds * breathEqSpectrumSampleRate));
    const auto startSample = centerSample - fftSize / 2;
    auto hasBreath = false;

    for (auto i = 0; i < fftSize; ++i)
    {
        const auto sourceSample = startSample + i;
        if (sourceSample < 0 || sourceSample >= breathEqSpectrumSourceBuffer.getNumSamples())
            continue;

        double breathWeight = 0.0;
        double breathNormGain = 1.0;
        double regionGain = 1.0;

        for (auto regionIndex = 0; regionIndex < result.regions.size(); ++regionIndex)
        {
            const auto& region = result.regions.getReference(regionIndex);
            if (region.type.equalsIgnoreCase("Noize"))
                continue;

            if (detailPage && regionIndex != selectedRegionIndex)
                continue;

            const auto weight = regionWeightForIndex(result.regions,
                                                     regionIndex,
                                                     sourceSample,
                                                     breathEqSpectrumSampleRate,
                                                     breathEqSpectrumSourceBuffer.getNumSamples(),
                                                     fadeInSamples,
                                                     fadeOutSamples);
            if (weight >= breathWeight)
            {
                breathWeight = weight;
                const auto previewGainDb = detailPage && regionIndex == selectedRegionIndex
                                         ? 0.0
                                         : region.gainDb;
                regionGain = dbToGain(juce::jlimit(-30.0, 30.0, previewGainDb));
                if (normalizeBreath)
                {
                    const auto peak = peakForRegion(regionIndex);
                    breathNormGain = peak > 1.0e-9 ? breathTargetGain / peak : 1.0;
                }
            }
        }

        if (breathWeight <= 0.0)
            continue;

        hasBreath = true;
        for (auto channel = 0; channel < channels; ++channel)
        {
            const auto dry = static_cast<double>(sampleAt(breathEqSpectrumSourceBuffer, channel, sourceSample));
            pre.setSample(channel, i, static_cast<float>(dry * breathWeight * breathNormGain * breathAdjustGain * regionGain));
        }
    }

    if (! hasBreath)
        return;

    juce::AudioBuffer<float> globalPost;
    globalPost.makeCopyOf(pre, true);
    audioProcessor.applyBreathEqToBuffer(globalPost, breathEqSpectrumSampleRate);

    juce::AudioBuffer<float> post;
    post.makeCopyOf(globalPost, true);
    if (detailPage)
    {
        auto previewEq = breathDetailEqEditor.getState();
        previewEq.enabled = breathDetailEqPowerButton.getToggleState();
        previewEq.bypassed = false;
        if (previewEq.hasActiveProcessing())
        {
            QQDeBreathEqProcessor regionProcessor;
            regionProcessor.prepare(breathEqSpectrumSampleRate, post.getNumChannels(), previewEq);
            regionProcessor.process(post);
        }

        post.applyGain(static_cast<float>(dbToGain(juce::jlimit(-30.0, 30.0, breathDetailGainSlider.getValue()))));
    }

    std::vector<float> preSpectrum;
    std::vector<float> postSpectrum;
    spectraFromBuffers(detailPage ? globalPost : pre, post, breathEqSpectrumSampleRate, preSpectrum, postSpectrum);

    if (detailPage)
    {
        holdSpectrumPeak(detailPreSpectrumPeak, preSpectrum);
        holdSpectrumPeak(detailPostSpectrumPeak, postSpectrum);
        breathDetailEqEditor.setSpectra(detailPreSpectrumPeak, detailPostSpectrumPeak);
    }
    else
    {
        holdSpectrumPeak(globalPreSpectrumPeak, preSpectrum);
        holdSpectrumPeak(globalPostSpectrumPeak, postSpectrum);
        breathEqEditor.setSpectra(globalPreSpectrumPeak, globalPostSpectrumPeak);
    }
}

bool QQDeBreathAudioProcessorEditor::findNextBreathLoopRange(double& startSeconds, double& endSeconds) const
{
    const auto result = audioProcessor.getAnalysisResult();
    if (! result.succeeded || result.regions.isEmpty())
        return false;

    if (showingBreathDetailPage && selectedRegionIsBreath())
    {
        const auto region = waveformEditor.getRegion(selectedRegionIndex);
        if (region.endTime > region.startTime)
        {
            startSeconds = region.startTime;
            endSeconds = region.endTime;
            return true;
        }
    }

    const auto info = audioProcessor.getRecordedBufferInfo();
    const auto current = juce::jmax(0.0, getCurrentLocalPlayheadSeconds(info));
    auto bestStart = std::numeric_limits<double>::max();
    auto bestEnd = 0.0;
    auto firstStart = std::numeric_limits<double>::max();
    auto firstEnd = 0.0;

    for (const auto& region : result.regions)
    {
        if (! region.type.equalsIgnoreCase("Breath"))
            continue;

        const auto start = region.startTime;
        const auto end = region.endTime;
        if (end <= start)
            continue;

        if (start < firstStart)
        {
            firstStart = start;
            firstEnd = end;
        }

        if (end >= current && start < bestStart)
        {
            bestStart = start;
            bestEnd = end;
        }
    }

    if (bestStart == std::numeric_limits<double>::max())
    {
        bestStart = firstStart;
        bestEnd = firstEnd;
    }

    if (bestStart == std::numeric_limits<double>::max() || bestEnd <= bestStart)
        return false;

    startSeconds = bestStart;
    endSeconds = bestEnd;
    return true;
}

void QQDeBreathAudioProcessorEditor::setBreathEqLoopEnabled(bool enabled)
{
    breathEqLoopButton.setToggleState(enabled, juce::dontSendNotification);

    if (! enabled)
    {
        audioProcessor.clearInternalPreviewLoop();
        if (auto* editorView = getARAEditorView())
            if (auto* documentController = editorView->getDocumentController())
                if (auto* playbackController = documentController->getHostPlaybackController())
                    playbackController->requestEnableCycle(false);

        statusLabel.setText("Status: Breath EQ loop off.", juce::dontSendNotification);
        return;
    }

    double localStart = 0.0;
    double localEnd = 0.0;
    if (! findNextBreathLoopRange(localStart, localEnd))
    {
        breathEqLoopButton.setToggleState(false, juce::dontSendNotification);
        statusLabel.setText("Status: No Breath region available for EQ loop.", juce::dontSendNotification);
        return;
    }

    const auto hostTimeSeconds = getHostTimeSeconds();
    if (isAraContext())
    {
        auto* editorView = getARAEditorView();
        auto* documentController = editorView != nullptr ? editorView->getDocumentController() : nullptr;
        auto* playbackController = documentController != nullptr ? documentController->getHostPlaybackController() : nullptr;
        if (playbackController == nullptr)
        {
            breathEqLoopButton.setToggleState(false, juce::dontSendNotification);
            statusLabel.setText("Status: ARA host playback controller is not available for EQ loop.", juce::dontSendNotification);
            return;
        }

        auto playbackStart = araSourceInfo.isComposite
                           ? araSourceInfo.compositePlaybackStartSeconds + localStart
                           : localStart;
        if (! araSourceInfo.isComposite)
        {
            const auto selectedRegions = findSelectedAraPlaybackRegions();
            if (! selectedRegions.isEmpty() && selectedRegions.getFirst() != nullptr)
            {
                auto* playbackRegion = selectedRegions.getFirst();
                playbackStart = playbackRegion->getStartInPlaybackTime()
                              + juce::jmax(0.0, localStart - playbackRegion->getStartInAudioModificationTime());
            }
        }

        playbackController->requestSetCycleRange(playbackStart, juce::jmax(0.05, localEnd - localStart));
        playbackController->requestEnableCycle(true);
        playbackController->requestSetPlaybackPosition(playbackStart);
        playbackController->requestStartPlayback();
        statusLabel.setText("Status: Breath EQ loop requested: " + juce::String(localStart, 3) + " - " + juce::String(localEnd, 3) + " s.", juce::dontSendNotification);
        return;
    }

    audioProcessor.setInternalPreviewLoopRange(localStart, localEnd, hostTimeSeconds);
    statusLabel.setText("Status: Internal Breath EQ loop: " + juce::String(localStart, 3) + " - " + juce::String(localEnd, 3) + " s.", juce::dontSendNotification);
}

double QQDeBreathAudioProcessorEditor::getHostTimeSeconds(bool* hostIsPlaying) const
{
    if (hostIsPlaying != nullptr)
        *hostIsPlaying = false;
    if (! isAraContext())
    {
        double seconds = -1.0;
        bool playing = false;
        audioProcessor.getCachedHostPosition(seconds, playing);
        if (hostIsPlaying != nullptr)
            *hostIsPlaying = playing;
        return seconds;
    }
    auto* playHead = audioProcessor.getPlayHead();
    if (playHead == nullptr)
        return -1.0;

    const auto position = playHead->getPosition();
    if (! position.hasValue())
        return -1.0;

    if (hostIsPlaying != nullptr)
        *hostIsPlaying = position->getIsPlaying();

    if (auto timeInSamples = position->getTimeInSamples(); timeInSamples.hasValue() && audioProcessor.getSampleRate() > 0.0)
        return static_cast<double>(*timeInSamples) / audioProcessor.getSampleRate();

    if (auto time = position->getTimeInSeconds())
        return *time;

    return -1.0;
}

void QQDeBreathAudioProcessorEditor::updatePlayheadFromHost(const QQDeBreathAudioProcessor::RecordedBufferInfo& info)
{
    bool hostIsPlaying = false;
    const auto hostTimeSeconds = getHostTimeSeconds(&hostIsPlaying);
    if (! isAraContext())
    {
        if (! hostIsPlaying && hostTimeSeconds >= 0.0)
            audioProcessor.syncStoppedPreviewWithHost(hostTimeSeconds);
        breathEqLoopButton.setToggleState(audioProcessor.isInternalPreviewLoopEnabled(), juce::dontSendNotification);
    }
    if (hostTimeSeconds >= 0.0)
    {
        if (sourceMode == SourceMode::ara && araSourceInfo.exportedWav.existsAsFile())
            waveformEditor.setPlayheadSeconds(getAraLocalPlayheadSeconds(hostTimeSeconds));
        else if (info.hasRecording && info.recordingStartTimelineSeconds >= 0.0)
            waveformEditor.setPlayheadSeconds(audioProcessor.getInternalPreviewPosition(hostTimeSeconds));
    }
}

void QQDeBreathAudioProcessorEditor::reloadAraSource()
{
    araSourceInfo = {};
    audioProcessor.clearAraSourceInfo();
    audioProcessor.clearAnalysisResult();
    waveformEditor.setAnalysisResult({});
    selectedRegionIndex = -1;
    araPlaybackCacheReady = false;
    applySavedGlobalDefaults();

    const auto selectedPlaybackRegions = findSelectedAraPlaybackRegions();
    if (selectedPlaybackRegions.size() > 1)
    {
        auto compositeInfo = QQDeBreathARASourceInfo();
        if (exportSelectedAraPlaybackRegionsToWav(compositeInfo))
        {
            juce::StringArray cachedFingerprints;
            for (auto* playbackRegion : selectedPlaybackRegions)
            {
                auto* modification = playbackRegion != nullptr ? playbackRegion->getAudioModification() : nullptr;
                auto* selectedSource = modification != nullptr
                                     ? modification->getAudioSource<juce::ARAAudioSource>()
                                     : nullptr;
                if (selectedSource == nullptr)
                    continue;

                const auto fingerprint = buildAraSourceFingerprint(*selectedSource);
                if (cachedFingerprints.contains(fingerprint))
                    continue;

                juce::String cacheStatus;
                if (! cacheAraSourceForPlayback(*selectedSource, cacheStatus))
                {
                    compositeInfo.exportStatus = cacheStatus;
                    statusLabel.setText("Status: " + cacheStatus, juce::dontSendNotification);
                    updateAnalysisInfo();
                    return;
                }
                cachedFingerprints.add(fingerprint);
            }
            araPlaybackCacheReady = ! cachedFingerprints.isEmpty();

            sourceMode = SourceMode::ara;
            araSourceInfo = compositeInfo;
            lastExportedFile = araSourceInfo.exportedWav;
            lastDisplayedRecordedSamples = 0;
            showingLiveRecordingPreview = false;
            juce::String waveformStatus;
            waveformEditor.loadAudioFile(araSourceInfo.exportedWav, waveformStatus);
            audioProcessor.setAraSourceInfo(araSourceInfo);
            persistAraState();
            updateRecordingInfo();
            return;
        }

        araSourceInfo = compositeInfo;
        statusLabel.setText("Status: " + araSourceInfo.exportStatus, juce::dontSendNotification);
        updateAnalysisInfo();
        return;
    }

    auto* source = findCurrentAudioSource();
    if (source == nullptr)
    {
        araSourceInfo.exportStatus = "No ARA audio source found. Use QQDeBreath from Cubase Audio Extensions, then click Load.";
        statusLabel.setText("Status: " + araSourceInfo.exportStatus, juce::dontSendNotification);
        updateAnalysisInfo();
        return;
    }

    araSourceInfo.name = makeAraSourceName(*source);
    araSourceInfo.persistentId = juce::String::fromUTF8(source->getPersistentID().c_str());
    araSourceInfo.sampleRate = source->getSampleRate();
    araSourceInfo.channelCount = static_cast<int>(source->getChannelCount());
    araSourceInfo.numSamples = static_cast<juce::int64>(source->getSampleCount());
    araSourceInfo.durationSeconds = araSourceInfo.sampleRate > 0.0
                                  ? static_cast<double>(araSourceInfo.numSamples) / araSourceInfo.sampleRate
                                  : 0.0;
    araSourceInfo.sourceFingerprint = buildAraSourceFingerprint(*source);

    if (exportAraSourceToWav(*source, araSourceInfo))
    {
        juce::String cacheStatus;
        if (! cacheAraSourceForPlayback(*source, cacheStatus))
        {
            araSourceInfo.exportStatus = cacheStatus;
            statusLabel.setText("Status: " + cacheStatus, juce::dontSendNotification);
            updateAnalysisInfo();
            return;
        }
        araPlaybackCacheReady = true;

        sourceMode = SourceMode::ara;
        lastExportedFile = araSourceInfo.exportedWav;
        lastDisplayedRecordedSamples = 0;
        showingLiveRecordingPreview = false;
        juce::String waveformStatus;
        waveformEditor.loadAudioFile(araSourceInfo.exportedWav, waveformStatus);
        araSourceInfo.exportStatus = "Loaded ARA source: " + araSourceInfo.name;
        audioProcessor.setAraSourceInfo(araSourceInfo);
        persistAraState();
    }

    updateRecordingInfo();
}

juce::ARAAudioSource* QQDeBreathAudioProcessorEditor::findCurrentAudioSource() const
{
    const auto instanceRegions = findSelectedAraPlaybackRegions();
    if (! instanceRegions.isEmpty() && instanceRegions.getFirst() != nullptr)
        if (auto* modification = instanceRegions.getFirst()->getAudioModification())
            return modification->getAudioSource();

    auto* editorView = getARAEditorView();
    auto* documentController = editorView != nullptr ? editorView->getDocumentController() : nullptr;
    auto* document = documentController != nullptr
                   ? documentController->getDocument<juce::ARADocument>()
                   : nullptr;
    if (document == nullptr)
        return nullptr;

    const auto& sources = document->getAudioSources<juce::ARAAudioSource>();
    return sources.size() == 1 ? sources.front() : nullptr;
}

juce::Array<juce::ARAPlaybackRegion*> QQDeBreathAudioProcessorEditor::findSelectedAraPlaybackRegions() const
{
    juce::Array<juce::ARAPlaybackRegion*> out;
    const auto assignedRegions = audioProcessor.getAssignedAraPlaybackRegions();

    if (auto* editorView = getARAEditorView())
    {
        const auto selectedRegions = editorView->getViewSelection().getEffectivePlaybackRegions<juce::ARAPlaybackRegion>();
        for (auto* region : selectedRegions)
        {
            if (region != nullptr && (assignedRegions.isEmpty() || assignedRegions.contains(region)))
                out.addIfNotAlreadyThere(region);
        }
    }

    // ARA selection is document-wide. When another track is selected, this editor must stay
    // bound to the playback regions assigned to its own plugin instance.
    if (out.isEmpty())
        for (auto* region : assignedRegions)
            if (region != nullptr)
                out.addIfNotAlreadyThere(region);

    for (auto i = 0; i < out.size(); ++i)
        for (auto j = i + 1; j < out.size(); ++j)
            if (out.getReference(j)->getStartInPlaybackTime() < out.getReference(i)->getStartInPlaybackTime())
                out.swap(i, j);

    return out;
}

bool QQDeBreathAudioProcessorEditor::exportSelectedAraPlaybackRegionsToWav(QQDeBreathARASourceInfo& info)
{
    const auto selectedRegions = findSelectedAraPlaybackRegions();
    if (selectedRegions.size() <= 1)
        return false;

    auto sampleRate = 0.0;
    auto channelCount = 0;
    juce::int64 compositeStart = std::numeric_limits<juce::int64>::max();
    juce::int64 compositeEnd = 0;
    juce::String fingerprintSeed = "ara-composite";

    for (auto* playbackRegion : selectedRegions)
    {
        auto* modification = playbackRegion != nullptr ? playbackRegion->getAudioModification() : nullptr;
        auto* source = modification != nullptr ? modification->getAudioSource<juce::ARAAudioSource>() : nullptr;
        if (source == nullptr)
        {
            info.exportStatus = "Selected ARA event has no audio source.";
            return false;
        }

        if (! source->isSampleAccessEnabled())
        {
            info.exportStatus = "ARA sample access is not enabled for one selected event.";
            return false;
        }

        if (source->getSampleRate() <= 0.0 || source->getSampleCount() <= 0 || source->getChannelCount() <= 0)
        {
            info.exportStatus = "Selected ARA source metadata is incomplete.";
            return false;
        }

        if (sampleRate <= 0.0)
            sampleRate = source->getSampleRate();
        else if (! juce::approximatelyEqual(sampleRate, source->getSampleRate()))
        {
            info.exportStatus = "Selected ARA events have different sample rates. Multi-event Load currently requires one sample rate.";
            return false;
        }

        channelCount = juce::jmax(channelCount, static_cast<int>(source->getChannelCount()));
        const auto regionStart = playbackRegion->getStartInPlaybackSamples(sampleRate);
        const auto regionEnd = playbackRegion->getEndInPlaybackSamples(sampleRate);
        compositeStart = juce::jmin(compositeStart, regionStart);
        compositeEnd = juce::jmax(compositeEnd, regionEnd);

        fingerprintSeed << "|" << buildAraSourceFingerprint(*source)
                        << "@" << juce::String(regionStart)
                        << "-" << juce::String(regionEnd)
                        << ":" << juce::String(playbackRegion->getStartInAudioModificationSamples())
                        << "-" << juce::String(playbackRegion->getEndInAudioModificationSamples());
    }

    if (sampleRate <= 0.0 || channelCount <= 0 || compositeEnd <= compositeStart)
    {
        info.exportStatus = "Selected ARA events could not be combined.";
        return false;
    }

    const auto totalSamples64 = compositeEnd - compositeStart;
    if (totalSamples64 > std::numeric_limits<int>::max())
    {
        info.exportStatus = "Selected ARA events are too long for the current composite loader.";
        return false;
    }

    juce::AudioBuffer<float> composite(channelCount, static_cast<int>(totalSamples64));
    composite.clear();
    info.playbackMappings.clear();

    for (auto* playbackRegion : selectedRegions)
    {
        auto* source = playbackRegion->getAudioModification()->getAudioSource<juce::ARAAudioSource>();
        const auto regionStart = playbackRegion->getStartInPlaybackSamples(sampleRate);
        const auto regionEnd = playbackRegion->getEndInPlaybackSamples(sampleRate);
        const auto sourceStart = playbackRegion->getStartInAudioModificationSamples();
        const auto sourceEnd = playbackRegion->getEndInAudioModificationSamples();
        const auto samplesToRead64 = juce::jmin<juce::int64>(regionEnd - regionStart, sourceEnd - sourceStart);
        if (samplesToRead64 <= 0 || samplesToRead64 > std::numeric_limits<int>::max())
            continue;

        const auto samplesToRead = static_cast<int>(samplesToRead64);
        juce::AudioBuffer<float> temp(static_cast<int>(source->getChannelCount()), samplesToRead);
        temp.clear();
        juce::ARAAudioSourceReader reader(source);
        if (! reader.read(&temp, 0, samplesToRead, sourceStart, true, true))
        {
            info.exportStatus = "Failed while reading one selected ARA event.";
            return false;
        }

        const auto destOffset = static_cast<int>(regionStart - compositeStart);
        for (auto channel = 0; channel < channelCount; ++channel)
        {
            if (channel < temp.getNumChannels())
                composite.copyFrom(channel, destOffset, temp, channel, 0, samplesToRead);
            else
                composite.clear(channel, destOffset, samplesToRead);
        }

        QQDeBreathARASourceInfo::PlaybackMapping mapping;
        mapping.sourceName = makeAraSourceName(*source);
        mapping.persistentId = juce::String::fromUTF8(source->getPersistentID().c_str());
        mapping.sourceFingerprint = buildAraSourceFingerprint(*source);
        mapping.sourceSampleRate = source->getSampleRate();
        mapping.sourceChannelCount = static_cast<int>(source->getChannelCount());
        mapping.sourceNumSamples = static_cast<juce::int64>(source->getSampleCount());
        mapping.compositeStartSeconds = static_cast<double>(regionStart - compositeStart) / sampleRate;
        mapping.compositeEndSeconds = mapping.compositeStartSeconds + static_cast<double>(samplesToRead) / sampleRate;
        mapping.sourceStartSeconds = static_cast<double>(sourceStart) / source->getSampleRate();
        info.playbackMappings.add(mapping);
    }

    auto outDir = getAraExportDirectory();
    if (! outDir.createDirectory())
    {
        info.exportStatus = "Could not create ARA temp export folder: " + outDir.getFullPathName();
        return false;
    }

    info.isComposite = true;
    info.name = "Selected ARA Events (" + juce::String(selectedRegions.size()) + ")";
    info.persistentId = {};
    info.sourceFingerprint = fnv1a64(fingerprintSeed);
    info.compositePlaybackStartSeconds = static_cast<double>(compositeStart) / sampleRate;
    info.compositePlaybackEndSeconds = static_cast<double>(compositeEnd) / sampleRate;
    info.sampleRate = sampleRate;
    info.channelCount = channelCount;
    info.numSamples = totalSamples64;
    info.durationSeconds = static_cast<double>(totalSamples64) / sampleRate;
    info.exportedWav = outDir.getChildFile("ara_selected_" + info.sourceFingerprint + ".wav");

    juce::String writeStatus;
    if (! writeWavFile(info.exportedWav, composite, sampleRate, writeStatus))
    {
        info.exportStatus = writeStatus;
        return false;
    }

    info.exportStatus = "Loaded " + juce::String(selectedRegions.size()) + " selected ARA events.";
    return true;
}

bool QQDeBreathAudioProcessorEditor::exportAraSourceToWav(juce::ARAAudioSource& source, QQDeBreathARASourceInfo& info)
{
    if (! source.isSampleAccessEnabled())
    {
        info.exportStatus = "ARA sample access is not enabled by the host yet.";
        return false;
    }

    if (info.sampleRate <= 0.0 || info.channelCount <= 0 || info.numSamples <= 0)
    {
        info.exportStatus = "ARA source metadata is incomplete.";
        return false;
    }

    auto outDir = getAraExportDirectory();
    if (! outDir.createDirectory())
    {
        info.exportStatus = "Could not create ARA temp export folder: " + outDir.getFullPathName();
        return false;
    }

    const auto safeName = info.name.fromLastOccurrenceOf("\\", false, false)
                              .fromLastOccurrenceOf("/", false, false)
                              .retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ .")
                              .trim();
    const auto fileStem = safeName.isNotEmpty() ? safeName : "ara_source";
    auto outFile = outDir.getChildFile(fileStem + "_" + info.sourceFingerprint + ".wav");
    outFile.deleteFile();

    juce::ARAAudioSourceReader reader(&source);
    if (! reader.isValid())
    {
        info.exportStatus = "Could not create ARA source reader.";
        return false;
    }

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> stream(outFile.createOutputStream());
    if (stream == nullptr)
    {
        info.exportStatus = "Could not open ARA temp wav: " + outFile.getFullPathName();
        return false;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(stream.get(),
                                                                              info.sampleRate,
                                                                              static_cast<unsigned int>(info.channelCount),
                                                                              32,
                                                                              {},
                                                                              0));
    if (writer == nullptr)
    {
        info.exportStatus = "Could not create ARA wav writer.";
        return false;
    }
    stream.release();

    constexpr int blockSize = 16384;
    juce::AudioBuffer<float> buffer(info.channelCount, blockSize);
    juce::int64 position = 0;

    while (position < info.numSamples)
    {
        const auto samplesThisBlock = static_cast<int>(std::min<juce::int64>(blockSize, info.numSamples - position));
        buffer.clear();

        if (! reader.read(&buffer, 0, samplesThisBlock, position, true, true))
        {
            info.exportStatus = "Failed while reading ARA source samples.";
            return false;
        }

        if (! writer->writeFromAudioSampleBuffer(buffer, 0, samplesThisBlock))
        {
            info.exportStatus = "Failed while writing ARA temp wav.";
            return false;
        }

        position += samplesThisBlock;
    }

    writer.reset();
    info.exportedWav = outFile;
    return true;
}

bool QQDeBreathAudioProcessorEditor::cacheAraSourceForPlayback(juce::ARAAudioSource& source,
                                                               juce::String& status)
{
    auto* documentController = getAraDocumentController();
    if (documentController == nullptr)
    {
        status = "ARA document controller is unavailable for playback cache.";
        return false;
    }

    if (! source.isSampleAccessEnabled())
    {
        status = "ARA sample access is not enabled for playback cache.";
        return false;
    }

    const auto sampleRate = source.getSampleRate();
    const auto channels = static_cast<int>(source.getChannelCount());
    const auto samples64 = static_cast<juce::int64>(source.getSampleCount());
    if (sampleRate <= 0.0 || channels <= 0 || samples64 <= 0
        || samples64 > std::numeric_limits<int>::max())
    {
        status = "ARA source is too large or invalid for realtime playback cache.";
        return false;
    }

    auto audio = std::make_shared<juce::AudioBuffer<float>>(channels, static_cast<int>(samples64));
    audio->clear();

    juce::ARAAudioSourceReader reader(&source);
    if (! reader.isValid())
    {
        status = "Could not create ARA reader for playback cache.";
        return false;
    }

    constexpr int blockSize = 65536;
    for (juce::int64 position = 0; position < samples64;)
    {
        const auto samplesThisBlock = static_cast<int>(juce::jmin<juce::int64>(blockSize, samples64 - position));
        if (! reader.read(audio.get(),
                          static_cast<int>(position),
                          samplesThisBlock,
                          position,
                          true,
                          true))
        {
            status = "Failed while preloading ARA source for realtime playback.";
            return false;
        }

        position += samplesThisBlock;
    }

    documentController->setSourceAudioCache(buildAraSourceFingerprint(source), sampleRate, audio);
    status = "ARA source preloaded for realtime playback.";
    return true;
}
double QQDeBreathAudioProcessorEditor::getAraLocalPlayheadSeconds(double hostTimeSeconds) const
{
    if (araSourceInfo.isComposite)
        return juce::jmax(0.0, hostTimeSeconds - araSourceInfo.compositePlaybackStartSeconds);

    auto localTime = hostTimeSeconds;

    if (auto* editorView = getARAEditorView())
    {
        const auto selectedRegions = findSelectedAraPlaybackRegions();
        if (! selectedRegions.isEmpty() && selectedRegions.getFirst() != nullptr)
        {
            auto* playbackRegion = selectedRegions.getFirst();
            const auto playbackRange = playbackRegion->getTimeRange();
            localTime = hostTimeSeconds - playbackRange.getStart() + playbackRegion->getStartInAudioModificationTime();
        }
    }

    return juce::jmax(0.0, localTime);
}

juce::String QQDeBreathAudioProcessorEditor::getActiveSourceKey(const QQDeBreathAudioProcessor::RecordedBufferInfo& info) const
{
    if (sourceMode == SourceMode::ara && araSourceInfo.sourceFingerprint.isNotEmpty())
        return araSourceInfo.sourceFingerprint;

    return info.sourceFingerprint;
}

bool QQDeBreathAudioProcessorEditor::hasAnalyzableSource(const QQDeBreathAudioProcessor::RecordedBufferInfo& info) const
{
    if (sourceMode == SourceMode::ara)
        return araSourceInfo.exportedWav.existsAsFile();

    return info.hasRecording;
}

void QQDeBreathAudioProcessorEditor::exportRecording()
{
    const auto info = audioProcessor.getRecordedBufferInfo();
    if (! hasAnalyzableSource(info))
    {
        statusLabel.setText("Status: No loaded ARA source or recorded buffer to export.", juce::dontSendNotification);
        return;
    }

    fileChooser = std::make_unique<juce::FileChooser>("Choose QQDeBreath export folder",
                                                      juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                      "*");

    fileChooser->launchAsync(juce::FileBrowserComponent::openMode
                           | juce::FileBrowserComponent::canSelectDirectories,
                             [this](const juce::FileChooser& chooser)
                             {
                                 const auto directory = chooser.getResult();
                                 if (directory != juce::File{})
                                     exportToDirectory(directory);

                                 fileChooser.reset();
                             });
}

void QQDeBreathAudioProcessorEditor::exportToDirectory(const juce::File& directory)
{
    if (! directory.createDirectory())
    {
        statusLabel.setText("Status: Could not create export folder: " + directory.getFullPathName(), juce::dontSendNotification);
        return;
    }

    updateRecordingInfo();
    juce::String status;
    renderCurrentStemsToDirectory(directory, status);
    statusLabel.setText("Status: " + status, juce::dontSendNotification);
}

bool QQDeBreathAudioProcessorEditor::renderCurrentStemsToDirectory(const juce::File& directory, juce::String& status)
{
    auto result = audioProcessor.getAnalysisResult();
    if (! result.succeeded)
    {
        status = "Analyze first to export Vocal Only/Breath/Noize stems.";
        return false;
    }

    juce::AudioBuffer<float> source;
    double sampleRate = 0.0;

    if (sourceMode == SourceMode::ara)
    {
        if (! araSourceInfo.exportedWav.existsAsFile())
        {
            if (auto* sourceObject = findCurrentAudioSource())
                exportAraSourceToWav(*sourceObject, araSourceInfo);
        }

        if (! araSourceInfo.exportedWav.existsAsFile())
        {
            status = "No ARA source wav is available for stem export.";
            return false;
        }

        if (! readAudioFile(araSourceInfo.exportedWav, source, sampleRate, status))
            return false;
    }
    else
    {
        if (! audioProcessor.copyRecordedBuffer(source, sampleRate))
        {
            status = "No recorded buffer is available for stem export.";
            return false;
        }
    }

    if (source.getNumSamples() <= 0 || source.getNumChannels() <= 0 || sampleRate <= 0.0)
    {
        status = "Source audio is empty.";
        return false;
    }

    juce::AudioBuffer<float> vocalOnly(source.getNumChannels(), source.getNumSamples());
    juce::AudioBuffer<float> breath(source.getNumChannels(), source.getNumSamples());
    juce::AudioBuffer<float> noize(source.getNumChannels(), source.getNumSamples());
    vocalOnly.clear();
    breath.clear();
    noize.clear();

    const auto enableFade = audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::enableFade)->load() >= 0.5f;
    const auto normalizeBreath = audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::normalizeBreath)->load() >= 0.5f;
    const auto breathTargetDb = static_cast<double>(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::breathTargetDb)->load());
    const auto breathTargetGain = dbToGain(breathTargetDb);
    const auto breathAdjustGain = dbToGain(juce::jlimit(-60.0, 30.0, static_cast<double>(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::breathGainDb)->load())));
    const auto fadeInSamples = enableFade
                             ? static_cast<int>(std::llround(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::fadeInMs)->load() * sampleRate / 1000.0))
                             : 0;
    const auto fadeOutSamples = enableFade
                              ? static_cast<int>(std::llround(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::fadeOutMs)->load() * sampleRate / 1000.0))
                              : 0;

    juce::Array<double> breathPeakCache;
    for (const auto& region : result.regions)
    {
        auto peak = 0.0f;
        if (! region.type.equalsIgnoreCase("Noize"))
        {
            const auto start = regionStartSample(region, sampleRate, source.getNumSamples());
            const auto end = regionEndSample(region, sampleRate, source.getNumSamples());
            for (auto channel = 0; channel < source.getNumChannels(); ++channel)
            {
                const auto* data = source.getReadPointer(channel);
                for (auto sample = start; sample < end; ++sample)
                    peak = juce::jmax(peak, std::abs(data[static_cast<int>(sample)]));
            }
        }
        breathPeakCache.add(static_cast<double>(peak));
    }

    for (auto sample = 0; sample < source.getNumSamples(); ++sample)
    {
        double breathWeight = 0.0;
        double noizeWeight = 0.0;
        double breathNormGain = 1.0;
        double regionGain = 1.0;

        for (auto regionIndex = 0; regionIndex < result.regions.size(); ++regionIndex)
        {
            const auto& region = result.regions.getReference(regionIndex);
            const auto weight = regionWeightForIndex(result.regions,
                                                     regionIndex,
                                                     sample,
                                                     sampleRate,
                                                     source.getNumSamples(),
                                                     fadeInSamples,
                                                     fadeOutSamples);
            if (weight <= 0.0)
                continue;

            if (region.type.equalsIgnoreCase("Noize"))
            {
                noizeWeight = juce::jmax(noizeWeight, weight);
            }
            else if (weight >= breathWeight)
            {
                breathWeight = weight;
                regionGain = dbToGain(juce::jlimit(-30.0, 30.0, region.gainDb));
                if (normalizeBreath)
                {
                    const auto peak = regionIndex < breathPeakCache.size()
                                    ? breathPeakCache.getReference(regionIndex)
                                    : 0.0;
                    breathNormGain = peak > 1.0e-9 ? breathTargetGain / peak : 1.0;
                }
            }
        }

        const auto nonVoiceSum = breathWeight + noizeWeight;
        if (nonVoiceSum > 1.0)
        {
            breathWeight /= nonVoiceSum;
            noizeWeight /= nonVoiceSum;
        }

        const auto voiceWeight = juce::jlimit(0.0, 1.0, 1.0 - breathWeight - noizeWeight);

        for (auto channel = 0; channel < source.getNumChannels(); ++channel)
        {
            const auto dry = static_cast<double>(sampleAt(source, channel, sample));
            vocalOnly.setSample(channel, sample, static_cast<float>(dry * voiceWeight));
            breath.setSample(channel, sample, static_cast<float>(dry * breathWeight * breathNormGain * breathAdjustGain * regionGain));
            noize.setSample(channel, sample, static_cast<float>(dry * noizeWeight));
        }
    }

    audioProcessor.applyBreathEqToBuffer(breath, sampleRate);

    for (auto regionIndex = 0; regionIndex < result.regions.size(); ++regionIndex)
    {
        const auto& region = result.regions.getReference(regionIndex);
        if (region.type.equalsIgnoreCase("Noize") || ! region.eqState.hasActiveProcessing())
            continue;

        const auto start = juce::jmax<juce::int64>(0, regionStartSample(region, sampleRate, source.getNumSamples()) - fadeInSamples);
        const auto end = juce::jmin<juce::int64>(source.getNumSamples(), regionEndSample(region, sampleRate, source.getNumSamples()) + fadeOutSamples);
        if (end <= start)
            continue;

        juce::AudioBuffer<float> regionBreath(source.getNumChannels(), static_cast<int>(end - start));
        regionBreath.clear();

        const auto peak = regionIndex < breathPeakCache.size() ? breathPeakCache.getReference(regionIndex) : 0.0;
        const auto normGain = normalizeBreath && peak > 1.0e-9 ? breathTargetGain / peak : 1.0;
        const auto regionGain = dbToGain(juce::jlimit(-30.0, 30.0, region.gainDb));

        for (auto sample = start; sample < end; ++sample)
        {
            const auto weight = regionWeightForIndex(result.regions,
                                                     regionIndex,
                                                     sample,
                                                     sampleRate,
                                                     source.getNumSamples(),
                                                     fadeInSamples,
                                                     fadeOutSamples);
            if (weight <= 0.0)
                continue;

            const auto destSample = static_cast<int>(sample - start);
            for (auto channel = 0; channel < source.getNumChannels(); ++channel)
            {
                const auto dry = static_cast<double>(sampleAt(source, channel, sample));
                regionBreath.setSample(channel,
                                       destSample,
                                       static_cast<float>(dry * weight * normGain * breathAdjustGain * regionGain));
            }
        }

        audioProcessor.applyBreathEqToBuffer(regionBreath, sampleRate);
        QQDeBreathEqProcessor regionProcessor;
        regionProcessor.prepare(sampleRate, regionBreath.getNumChannels(), region.eqState);
        regionProcessor.process(regionBreath);

        for (auto channel = 0; channel < breath.getNumChannels(); ++channel)
        {
            breath.clear(channel, static_cast<int>(start), regionBreath.getNumSamples());
            breath.addFrom(channel, static_cast<int>(start), regionBreath, channel, 0, regionBreath.getNumSamples());
        }
    }

    if (! writeWavFile(directory.getChildFile("Vocal Only.wav"), vocalOnly, sampleRate, status))
        return false;
    if (! writeWavFile(directory.getChildFile("Breath.wav"), breath, sampleRate, status))
        return false;
    if (! writeWavFile(directory.getChildFile("Noize.wav"), noize, sampleRate, status))
        return false;

    status = "Exported Vocal Only.wav, Breath.wav, and Noize.wav with current Fade/Norm/Global Gain/EQ.";
    return true;
}

void QQDeBreathAudioProcessorEditor::updateAnalysisInfo()
{
    const auto info = audioProcessor.getRecordedBufferInfo();
    const auto result = audioProcessor.getAnalysisResult();
    const auto analysisRunning = analysisThread != nullptr && analysisThread->isThreadRunning();

    if (analysisRunning)
    {
        const auto elapsed = analysisStartMs == 0 ? 0u : juce::Time::getMillisecondCounter() - analysisStartMs;
        const auto estimatedMs = static_cast<uint32_t>(juce::jlimit(8000.0, 90000.0, info.durationSeconds * 350.0 + 5000.0));
        const auto percent = juce::jlimit(5, 95, 5 + static_cast<int>(std::round(90.0 * static_cast<double>(elapsed) / static_cast<double>(estimatedMs))));
        analysisStatusLabel.setText(percent < 95 ? "Analysis: " + juce::String(percent) + "%"
                                                : "Analysis: Finalizing analysis...",
                                    juce::dontSendNotification);
    }
    else
    {
        analysisStatusLabel.setText("Analysis: " + result.status, juce::dontSendNotification);
    }
    breathCountLabel.setText("Breath: " + juce::String(result.breathCount), juce::dontSendNotification);
    noizeCountLabel.setText("Noize: " + juce::String(result.noizeCount), juce::dontSendNotification);
    analysisErrorLabel.setText(result.errorMessage.isNotEmpty() ? "Error: " + result.errorMessage : "", juce::dontSendNotification);

    const auto activeSourceKey = getActiveSourceKey(info);
    const auto mismatch = result.hasResult
                       && activeSourceKey.isNotEmpty()
                       && result.sourceKey.isNotEmpty()
                       && result.sourceKey != activeSourceKey;
    sourceMismatchLabel.setText(mismatch ? "Source mismatch: recording changed after analysis. Re-analyze." : "",
                                juce::dontSendNotification);

    analyzeButton.setEnabled(! analysisRunning && hasAnalyzableSource(info) && ! info.isRecordArmed && ! info.isRecording);
    cancelAnalyzeButton.setEnabled(analysisRunning);
    clearAnalysisButton.setEnabled(! analysisRunning && result.hasResult);
}

void QQDeBreathAudioProcessorEditor::startAnalysis()
{
    const auto info = audioProcessor.getRecordedBufferInfo();
    if (! hasAnalyzableSource(info))
    {
        statusLabel.setText("Status: No loaded ARA source or recorded buffer to analyze.", juce::dontSendNotification);
        return;
    }

    if (info.isRecordArmed || info.isRecording)
    {
        statusLabel.setText("Status: Stop DAW playback or clear recording before analysis.", juce::dontSendNotification);
        return;
    }

    juce::String status;
    juce::File inputWav;
    if (sourceMode == SourceMode::ara && araSourceInfo.exportedWav.existsAsFile())
    {
        inputWav = araSourceInfo.exportedWav;
    }
    else
    {
        if (! audioProcessor.exportRecordedBufferToTempWav(inputWav, status))
        {
            statusLabel.setText("Status: " + status, juce::dontSendNotification);
            return;
        }
    }

    lastExportedFile = inputWav;
    juce::String waveformStatus;
    waveformEditor.loadAudioFile(lastExportedFile, waveformStatus);

    const auto sourceKey = getActiveSourceKey(info).isNotEmpty() ? getActiveSourceKey(info) : inputWav.getFileNameWithoutExtension();
    auto outDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("QQDeBreath")
        .getChildFile(sourceMode == SourceMode::ara ? "ARA" : "VST3")
        .getChildFile("Phase7B")
        .getChildFile(sourceKey);

    QQDeBreathBridgeAnalysisConfig config;
    config.inputWav = inputWav;
    config.outputJson = outDir.getChildFile("result.json");
    config.outDir = outDir.getChildFile("stems");
    config.cancelFile = outDir.getChildFile("cancel.flag");
    config.sourceKey = sourceKey;
    const auto fadeEnabled = audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::enableFade)->load() >= 0.5f;
    const auto fadeIn = audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::fadeInMs)->load();
    const auto fadeOut = audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::fadeOutMs)->load();
    config.fadeMs = fadeEnabled ? static_cast<double>(juce::jmax(fadeIn, fadeOut)) : 0.0;
    config.breathTargetDb = static_cast<double>(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::breathTargetDb)->load());
    config.breathGainDb = juce::jlimit(-60.0, 30.0, static_cast<double>(audioProcessor.parameters.getRawParameterValue(QQDeBreath::ParamIDs::breathGainDb)->load()));

    QQDeBreathBridgeAnalysisResult pending;
    pending.status = "Reading audio...";
    pending.sourceKey = sourceKey;
    audioProcessor.setAnalysisResult(pending);

    analysisStartMs = juce::Time::getMillisecondCounter();
    analysisThread = std::make_unique<AnalysisThread>(*this, config);
    analysisThread->startThread();
    updateRecordingInfo();
}

void QQDeBreathAudioProcessorEditor::cancelAnalysis()
{
    if (analysisThread != nullptr)
    {
        analysisThread->signalThreadShouldExit();
        QQDeBreathBridgeAnalysisResult cancelling;
        cancelling.status = "Cancelling analysis...";
        audioProcessor.setAnalysisResult(cancelling);
    }

    updateAnalysisInfo();
}

void QQDeBreathAudioProcessorEditor::clearAnalysis()
{
    cancelAnalysis();
    audioProcessor.clearAnalysisResult();
    waveformEditor.setAnalysisResult({});
    selectedRegionIndex = -1;
    persistAraState();
    updateAnalysisInfo();
}

void QQDeBreathAudioProcessorEditor::handleAnalysisFinished(const QQDeBreathBridgeAnalysisResult& result)
{
    auto finalResult = result;
    if (finalResult.succeeded)
        finalResult.status = "Analysis complete.";

    analysisStatusLabel.setText("Analysis: Building regions...", juce::dontSendNotification);
    audioProcessor.setAnalysisResult(finalResult);
    analysisStatusLabel.setText("Analysis: Preparing waveform...", juce::dontSendNotification);
    waveformEditor.setAnalysisResult(finalResult);
    selectedRegionIndex = -1;
    analysisStatusLabel.setText("Analysis: Saving state...", juce::dontSendNotification);
    persistAraState();
    refreshBreathEqSpectrum();
    analysisStartMs = 0;
    analysisThread.reset();
    updateRecordingInfo();
}

void QQDeBreathAudioProcessorEditor::waveformRegionsChanged(const juce::Array<QQDeBreathBridgeRegion>& regions)
{
    const auto info = audioProcessor.getRecordedBufferInfo();
    auto result = audioProcessor.getAnalysisResult();
    result.hasResult = true;
    result.succeeded = true;
    result.status = "Analysis edited.";
    if (result.sourceKey.isEmpty())
        result.sourceKey = getActiveSourceKey(info);
    result.regions = regions;
    result.breathCount = 0;
    result.noizeCount = 0;

    for (const auto& region : regions)
    {
        if (region.type.equalsIgnoreCase("Breath"))
            ++result.breathCount;
        else if (region.type.equalsIgnoreCase("Noize"))
            ++result.noizeCount;
    }

    audioProcessor.setAnalysisResult(result);
    persistAraState();
    refreshBreathEqSpectrum();
    updateAnalysisInfo();
}

void QQDeBreathAudioProcessorEditor::waveformSeekRequested(double localSeconds)
{
    if (isAraContext())
    {
        auto* editorView = getARAEditorView();
        if (editorView == nullptr)
        {
            statusLabel.setText("Status: ARA editor view is not available for DAW seek.", juce::dontSendNotification);
            return;
        }

        auto* documentController = editorView->getDocumentController();
        if (documentController == nullptr || documentController->getHostPlaybackController() == nullptr)
        {
            statusLabel.setText("Status: ARA host playback controller is not available.", juce::dontSendNotification);
            return;
        }

        auto targetTime = localSeconds;
        if (araSourceInfo.isComposite)
        {
            targetTime = araSourceInfo.compositePlaybackStartSeconds + localSeconds;
        }
        else
        {
        const auto selectedRegions = findSelectedAraPlaybackRegions();
        if (! selectedRegions.isEmpty() && selectedRegions.getFirst() != nullptr)
        {
            auto* playbackRegion = selectedRegions.getFirst();
            targetTime = playbackRegion->getStartInPlaybackTime()
                       + juce::jmax(0.0, localSeconds - playbackRegion->getStartInAudioModificationTime());
        }
        }

        documentController->getHostPlaybackController()->requestSetPlaybackPosition(targetTime);
        waveformEditor.setPlayheadSeconds(localSeconds);
        statusLabel.setText("Status: Requested Cubase playhead seek to " + juce::String(targetTime, 3) + " s.", juce::dontSendNotification);
        return;
    }

    audioProcessor.setInternalPreviewPosition(localSeconds, getHostTimeSeconds());
    waveformEditor.setPlayheadSeconds(localSeconds);
    statusLabel.setText("Status: Internal preview position set. Cubase main playhead is unchanged.", juce::dontSendNotification);
}

juce::String QQDeBreathAudioProcessorEditor::buildAraSourceFingerprint(const juce::ARAAudioSource& source)
{
    const auto id = juce::String::fromUTF8(source.getPersistentID().c_str());
    const auto seed = id + "|sr=" + juce::String(source.getSampleRate(), 8)
                    + "|ch=" + juce::String(static_cast<int>(source.getChannelCount()))
                    + "|samples=" + juce::String(static_cast<juce::int64>(source.getSampleCount()));

    return fnv1a64(seed);
}

juce::String QQDeBreathAudioProcessorEditor::makeAraSourceName(const juce::ARAAudioSource& source)
{
    if (auto name = source.getName())
        return juce::String::fromUTF8(name);

    return "ARA Audio Source";
}

juce::File QQDeBreathAudioProcessorEditor::getAraExportDirectory()
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("QQDeBreath")
        .getChildFile("ARA")
        .getChildFile("Unified");
}
