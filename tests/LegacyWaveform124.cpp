// Frozen 1.24 UI display reference; tests only.
#include "LegacyWaveform124.h"

#include <cmath>

namespace
{
constexpr auto minViewSeconds = 0.1;
constexpr auto minRegionSeconds = 0.02;

juce::String normalizedType(const juce::String& type)
{
    return type.equalsIgnoreCase("Noize") ? "Noize" : "Breath";
}

bool sameRegionTime(const QQDeBreathBridgeRegion& a, const QQDeBreathBridgeRegion& b)
{
    return std::abs(a.startTime - b.startTime) < 1.0e-9
        && std::abs(a.endTime - b.endTime) < 1.0e-9
        && normalizedType(a.type) == normalizedType(b.type);
}
} // namespace

LegacyWaveform124::LegacyWaveform124()
{
    formatManager.registerBasicFormats();
    setWantsKeyboardFocus(true);
    horizontalScrollBar.addListener(this);
    addAndMakeVisible(horizontalScrollBar);
}

bool LegacyWaveform124::loadAudioFile(const juce::File& file, juce::String& status)
{
    if (! file.existsAsFile())
    {
        status = "Waveform source file not found: " + file.getFullPathName();
        clearAudio();
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
    {
        status = "Could not read waveform source: " + file.getFullPathName();
        clearAudio();
        return false;
    }

    juce::AudioBuffer<float> temp(static_cast<int>(reader->numChannels),
                                  static_cast<int>(reader->lengthInSamples));
    reader->read(&temp, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

    monoWaveform.setSize(1, temp.getNumSamples());
    monoWaveform.clear();

    for (auto channel = 0; channel < temp.getNumChannels(); ++channel)
        monoWaveform.addFrom(0, 0, temp, channel, 0, temp.getNumSamples(), 1.0f / juce::jmax(1, temp.getNumChannels()));

    sampleRate = reader->sampleRate;
    timelineDuration = getDurationSeconds();
    playhead = 0.0;
    rebuildBreathNormGainCache();
    fitView();
    updateScrollBar();
    status = "Waveform loaded.";
    repaint();
    return true;
}

void LegacyWaveform124::setAudioBuffer(const juce::AudioBuffer<float>& audio,
                                              double sourceSampleRate,
                                              double timelineDurationSeconds,
                                              bool preserveView)
{
    const auto hadAudio = monoWaveform.getNumSamples() > 0 && sampleRate > 0.0;
    const auto oldViewStart = viewStart;
    const auto oldViewEnd = viewEnd;

    sampleRate = sourceSampleRate;
    monoWaveform.setSize(1, audio.getNumSamples());
    monoWaveform.clear();

    for (auto channel = 0; channel < audio.getNumChannels(); ++channel)
        monoWaveform.addFrom(0, 0, audio, channel, 0, audio.getNumSamples(), 1.0f / juce::jmax(1, audio.getNumChannels()));

    timelineDuration = juce::jmax(getDurationSeconds(), timelineDurationSeconds);
    rebuildBreathNormGainCache();

    if (preserveView && hadAudio)
        setView(oldViewStart, oldViewEnd);
    else
        fitView();

    updateScrollBar();
    repaint();
}

void LegacyWaveform124::clearAudio()
{
    recordingOverlay = false;
    recordingOverlayText.clear();
    monoWaveform.setSize(0, 0);
    sampleRate = 0.0;
    timelineDuration = 0.0;
    viewStart = 0.0;
    viewEnd = 1.0;
    playhead = 0.0;
    selectedRegion = -1;
    undoStack.clear();
    redoStack.clear();
    breathNormGainCache.clear();
    displayRegions.clear();
    processedBreathDisplay.setSize(0, 0);
    processedBreathDisplayKey.clear();
    updateScrollBar();
    repaint();
}

void LegacyWaveform124::setAnalysisResult(const QQDeBreathBridgeAnalysisResult& result)
{
    regions = result.regions;
    selectedRegion = -1;
    undoStack.clear();
    redoStack.clear();
    rebuildBreathNormGainCache();
    repaint();
}

void LegacyWaveform124::clearRegions()
{
    if (! regions.isEmpty())
        pushUndoState();

    regions.clear();
    selectedRegion = -1;
    notifyRegionsChanged();
    repaint();
}

QQDeBreathBridgeRegion LegacyWaveform124::getRegion(int index) const
{
    if (index >= 0 && index < regions.size())
        return regions.getReference(index);

    return {};
}

juce::Array<double> LegacyWaveform124::buildRegionPeakCache(
    const juce::Array<QQDeBreathBridgeRegion>& regionsToMeasure) const
{
    juce::Array<double> peaks;
    peaks.ensureStorageAllocated(regionsToMeasure.size());

    if (sampleRate <= 0.0 || monoWaveform.getNumSamples() <= 0)
    {
        for (auto i = 0; i < regionsToMeasure.size(); ++i)
            peaks.add(0.0);
        return peaks;
    }

    for (const auto& region : regionsToMeasure)
    {
        const auto start = regionStartSample(region);
        const auto end = regionEndSample(region);
        const auto length = static_cast<int>(juce::jmax<juce::int64>(0, end - start));
        const auto peak = length > 0
                        ? monoWaveform.getMagnitude(0, static_cast<int>(start), length)
                        : 0.0f;
        peaks.add(static_cast<double>(peak));
    }

    return peaks;
}

void LegacyWaveform124::setRegionProcessing(int index, double gainDb, const QQDeBreathEqState& eqState, bool notifyChange, bool shouldRebuildDisplay)
{
    if (index < 0 || index >= regions.size())
        return;

    auto& region = regions.getReference(index);
    if (region.type.equalsIgnoreCase("Noize"))
        return;

    const auto oldGainDb = region.gainDb;
    const auto oldEq = serializeBreathEqState(region.eqState);
    region.gainDb = juce::jlimit(-30.0, 30.0, gainDb);
    region.eqState = sanitizeBreathEqState(eqState);
    if (notifyChange)
        notifyRegionsChanged(std::abs(oldGainDb - region.gainDb) > 1.0e-6);
    else if (shouldRebuildDisplay
             && (std::abs(oldGainDb - region.gainDb) > 1.0e-6
                 || oldEq != serializeBreathEqState(region.eqState)))
        rebuildBreathNormGainCache();
    repaint();
}

double LegacyWaveform124::getDurationSeconds() const
{
    if (sampleRate <= 0.0 || monoWaveform.getNumSamples() <= 0)
        return 0.0;

    return static_cast<double>(monoWaveform.getNumSamples()) / sampleRate;
}

double LegacyWaveform124::getTimelineDurationSeconds() const
{
    return juce::jmax(getDurationSeconds(), timelineDuration);
}

bool LegacyWaveform124::canUndo() const
{
    return ! undoStack.isEmpty();
}

bool LegacyWaveform124::canRedo() const
{
    return ! redoStack.isEmpty();
}

void LegacyWaveform124::undo()
{
    if (undoStack.isEmpty())
        return;

    redoStack.add(regions);
    regions = undoStack.getLast();
    undoStack.removeLast();
    selectedRegion = -1;
    rebuildBreathNormGainCache();
    notifyRegionsChanged();
    repaint();
}

void LegacyWaveform124::redo()
{
    if (redoStack.isEmpty())
        return;

    undoStack.add(regions);
    regions = redoStack.getLast();
    redoStack.removeLast();
    selectedRegion = -1;
    rebuildBreathNormGainCache();
    notifyRegionsChanged();
    repaint();
}

void LegacyWaveform124::setTimelineDurationSeconds(double seconds)
{
    timelineDuration = juce::jmax(getDurationSeconds(), seconds);
    if (followPlayhead && playhead > viewEnd)
    {
        const auto span = juce::jmax(minViewSeconds, viewEnd - viewStart);
        setView(playhead, playhead + span);
    }
    updateScrollBar();
    repaint();
}

void LegacyWaveform124::setPlayheadSeconds(double seconds)
{
    const auto previousPlayhead = playhead;
    const auto wasVisible = previousPlayhead >= viewStart && previousPlayhead <= viewEnd;
    playhead = juce::jmax(0.0, seconds);
    timelineDuration = juce::jmax(timelineDuration, playhead);
    const auto isVisible = playhead >= viewStart && playhead <= viewEnd;

    if (followPlayhead && (playhead < viewStart || playhead > viewEnd))
    {
        const auto span = juce::jmax(minViewSeconds, viewEnd - viewStart);
        setView(playhead, playhead + span);
        return;
    }

    if (wasVisible || isVisible)
        repaint();
}

void LegacyWaveform124::setWaveformDisplayGain(double gain)
{
    waveformDisplayGain = juce::jlimit(0.25, 8.0, gain);
    repaint();
}

void LegacyWaveform124::setFollowPlayhead(bool shouldFollow)
{
    if (followPlayhead == shouldFollow)
        return;

    followPlayhead = shouldFollow;
    if (followPlayhead && (playhead < viewStart || playhead > viewEnd))
    {
        const auto span = juce::jmax(minViewSeconds, viewEnd - viewStart);
        setView(playhead, playhead + span);
        return;
    }

    repaint();
}

void LegacyWaveform124::setRecordingOverlay(bool shouldShow, const juce::String& text)
{
    if (recordingOverlay == shouldShow && recordingOverlayText == text)
        return;

    recordingOverlay = shouldShow;
    recordingOverlayText = text;
    updateScrollBar();
    repaint();
}

void LegacyWaveform124::setMonitorState(bool voiceEnabled, bool breathEnabled, bool noizeEnabled)
{
    if (monitorVoice == voiceEnabled && monitorBreath == breathEnabled && monitorNoize == noizeEnabled)
        return;

    monitorVoice = voiceEnabled;
    monitorBreath = breathEnabled;
    monitorNoize = noizeEnabled;
    repaint();
}

void LegacyWaveform124::setProcessingParams(const DisplayProcessingParams& params)
{
    const auto normChanged = processingParams.normalizeBreath != params.normalizeBreath
                          || std::abs(processingParams.breathTargetDb - params.breathTargetDb) > 1.0e-6;
    const auto eqChanged = serializeBreathEqState(processingParams.breathEqState) != serializeBreathEqState(params.breathEqState);
    const auto fadeChanged = processingParams.enableFade != params.enableFade
                          || std::abs(processingParams.fadeInMs - params.fadeInMs) > 1.0e-6
                          || std::abs(processingParams.fadeOutMs - params.fadeOutMs) > 1.0e-6;
    const auto breathGainChanged = std::abs(processingParams.breathGainDb - params.breathGainDb) > 1.0e-6;
    processingParams = params;

    if (normChanged || fadeChanged || breathGainChanged || eqChanged)
        rebuildBreathNormGainCache();

    repaint();
}

void LegacyWaveform124::refreshProcessedDisplay()
{
    processedBreathDisplayKey.clear();
    rebuildBreathNormGainCache();
    repaint();
}

void LegacyWaveform124::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff07111f));
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 1.0f);

    auto area = getWaveformArea();
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(13.0f);

    if (recordingOverlay)
    {
        g.setColour(juce::Colour(0xff07111f));
        g.fillRoundedRectangle(area.toFloat(), 5.0f);
        g.setColour(juce::Colour(0xff1e293b));
        g.drawRoundedRectangle(area.toFloat(), 5.0f, 1.0f);
        g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.16f));
        g.fillRect(area.reduced(12).toFloat());
        g.setColour(juce::Colour(0xffe2e8f0));
        g.setFont(juce::Font(42.0f, juce::Font::bold));
        g.drawText(recordingOverlayText.isNotEmpty() ? recordingOverlayText : "Recording...",
                   area,
                   juce::Justification::centred);
        g.setFont(14.0f);
        g.setColour(juce::Colour(0xff94a3b8));
        g.drawText("Waveform will be generated after recording stops.",
                   area.reduced(0, 54),
                   juce::Justification::centredBottom);
        return;
    }

    if (monoWaveform.getNumSamples() <= 0 || sampleRate <= 0.0)
    {
        g.drawText("Waveform: load or analyze audio to display source and Breath / Noize regions.",
                   area, juce::Justification::centred);
        return;
    }

    const auto duration = getTimelineDurationSeconds();
    viewStart = juce::jlimit(0.0, juce::jmax(0.0, duration - minViewSeconds), viewStart);
    viewEnd = juce::jlimit(viewStart + minViewSeconds, duration, viewEnd);

    for (auto i = 0; i < regions.size(); ++i)
    {
        const auto& region = regions.getReference(i);
        const auto x1 = timeToX(region.startTime);
        const auto x2 = timeToX(region.endTime);

        if (x2 < area.getX() || x1 > area.getRight())
            continue;

        g.setColour(regionColour(region.type));
        g.fillRect(juce::Rectangle<int>(x1, area.getY(), juce::jmax(1, x2 - x1), area.getHeight()).toFloat());

        if (i == selectedRegion)
        {
            g.setColour(juce::Colour(0xfff8fafc));
            g.drawRect(juce::Rectangle<int>(x1, area.getY(), juce::jmax(1, x2 - x1), area.getHeight()), 2);
        }
    }

    if (dragMode == DragMode::create)
    {
        const auto x1 = timeToX(juce::jmin(createStart, createEnd));
        const auto x2 = timeToX(juce::jmax(createStart, createEnd));
        g.setColour(regionColour("Breath").withAlpha(0.45f));
        g.fillRect(juce::Rectangle<int>(x1, area.getY(), juce::jmax(1, x2 - x1), area.getHeight()).toFloat());
        g.setColour(juce::Colour(0xfff8fafc));
        g.drawRect(juce::Rectangle<int>(x1, area.getY(), juce::jmax(1, x2 - x1), area.getHeight()), 1);
    }

    drawFadeGuides(g, area);

    const auto midY = area.getCentreY();
    g.setColour(juce::Colour(0xff334155));
    g.drawHorizontalLine(midY, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));

    const auto audioDuration = getDurationSeconds();
    const auto drawStartTime = juce::jlimit(0.0, audioDuration, viewStart);
    const auto drawEndTime = juce::jlimit(drawStartTime, audioDuration, viewEnd);
    const auto startSample = juce::jlimit(0, monoWaveform.getNumSamples() - 1, static_cast<int>(std::floor(drawStartTime * sampleRate)));
    const auto endSample = juce::jlimit(startSample + 1, monoWaveform.getNumSamples(), static_cast<int>(std::ceil(drawEndTime * sampleRate)));
    auto visiblePeak = 0.0f;
    const auto peakStep = juce::jmax(1, (endSample - startSample) / 4000);
    const auto* data = monoWaveform.getReadPointer(0);
    for (auto s = startSample; s < endSample; s += peakStep)
        visiblePeak = juce::jmax(visiblePeak, std::abs(data[s]));

    const auto autoGain = visiblePeak > 1.0e-5f
                        ? juce::jlimit(1.0, 32.0, 0.86 / static_cast<double>(visiblePeak))
                        : 1.0;
    const auto totalDisplayGain = static_cast<float>(autoGain * waveformDisplayGain);
    const auto heightScale = area.getHeight() * 0.42f;
    const auto drawLeft = timeToX(drawStartTime);
    const auto drawRight = timeToX(drawEndTime);

    const auto drawWave = [&](bool activeComponents, juce::Colour colour)
    {
        g.setColour(colour);
        for (auto x = juce::jmax(area.getX(), drawLeft); x < juce::jmin(area.getRight(), drawRight); ++x)
        {
            const auto ratio1 = (xToTime(x) - drawStartTime) / juce::jmax(1.0e-9, drawEndTime - drawStartTime);
            const auto ratio2 = (xToTime(x + 1) - drawStartTime) / juce::jmax(1.0e-9, drawEndTime - drawStartTime);
            auto s1 = startSample + static_cast<int>(ratio1 * (endSample - startSample));
            auto s2 = startSample + static_cast<int>(ratio2 * (endSample - startSample));
            s1 = juce::jlimit(startSample, endSample - 1, s1);
            s2 = juce::jlimit(s1 + 1, endSample, s2);

            auto lo = 0.0;
            auto hi = 0.0;
            const auto sampleStep = juce::jmax(1, (s2 - s1) / 12);
            for (auto s = s1; s < s2; s += sampleStep)
            {
                const auto sample = renderedDisplaySample(s, activeComponents);
                lo = juce::jmin(lo, sample);
                hi = juce::jmax(hi, sample);
            }

            if (std::abs(lo) < 1.0e-9 && std::abs(hi) < 1.0e-9)
                continue;

            const auto y1 = midY - static_cast<int>(juce::jlimit(-1.0, 1.0, hi * totalDisplayGain) * heightScale);
            const auto y2 = midY - static_cast<int>(juce::jlimit(-1.0, 1.0, lo * totalDisplayGain) * heightScale);
            g.drawVerticalLine(x, static_cast<float>(y1), static_cast<float>(y2));
        }
    };

    drawWave(false, juce::Colour(0xff64748b).withAlpha(0.48f));
    drawWave(true, juce::Colour(0xffe2e8f0));

    if (playhead >= viewStart && playhead <= viewEnd)
    {
        const auto x = timeToX(playhead);
        g.setColour(juce::Colour(0xfffacc15));
        g.drawVerticalLine(x, static_cast<float>(area.getY()), static_cast<float>(area.getBottom()));
    }

    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(12.0f);
    g.drawText(juce::String(viewStart, 2) + "s", area.removeFromBottom(18), juce::Justification::centredLeft);
    g.drawText(juce::String(viewEnd, 2) + "s", area, juce::Justification::bottomRight);
}

void LegacyWaveform124::resized()
{
    auto area = getLocalBounds().reduced(10);
    horizontalScrollBar.setBounds(area.removeFromBottom(14));
    updateScrollBar();
}

void LegacyWaveform124::mouseDown(const juce::MouseEvent& event)
{
    grabKeyboardFocus();

    const auto startEdgeHit = hitTestRegionStartEdge(event.x);
    const auto endEdgeHit = hitTestRegionEndEdge(event.x);
    if (event.mods.isLeftButtonDown() && ! event.mods.isPopupMenu() && (startEdgeHit >= 0 || endEdgeHit >= 0))
    {
        pushUndoState();
        resizeRegion = startEdgeHit >= 0 ? startEdgeHit : endEdgeHit;
        dragMode = startEdgeHit >= 0 ? DragMode::resizeStart : DragMode::resizeEnd;
        selectRegion(resizeRegion);
        repaint();
        return;
    }

    if (event.mods.isShiftDown() && event.mods.isLeftButtonDown())
    {
        dragMode = DragMode::create;
        createStart = xToTime(event.x);
        createEnd = createStart;
        repaint();
        return;
    }

    const auto hit = hitTestRegion(event.x);
    selectRegion(hit);

    if (event.getNumberOfClicks() >= 2 && event.mods.isLeftButtonDown() && hit >= 0)
    {
        if (onRegionDoubleClicked)
            onRegionDoubleClicked(hit);
        return;
    }

    if (event.mods.isLeftButtonDown() && ! event.mods.isPopupMenu())
    {
        const auto time = xToTime(event.x);

        if (hit >= 0)
        {
            const auto& region = regions.getReference(hit);
            dragMode = DragMode::move;
            moveRegion = hit;
            moveAnchorTime = time;
            moveOriginalStart = region.startTime;
            moveOriginalEnd = region.endTime;
            moveUndoPushed = false;
        }

        setPlayheadSeconds(time);
        if (onSeekRequested)
            onSeekRequested(time);
    }

    if (event.mods.isRightButtonDown() && hit >= 0)
    {
        pushUndoState();
        auto& region = regions.getReference(hit);
        region.type = region.type.equalsIgnoreCase("Breath") ? "Noize" : "Breath";
        notifyRegionsChanged();
        repaint();
    }
}

void LegacyWaveform124::mouseMove(const juce::MouseEvent& event)
{
    updateMouseCursor(event.x);
}

void LegacyWaveform124::mouseDrag(const juce::MouseEvent& event)
{
    if (dragMode == DragMode::create)
    {
        createEnd = xToTime(event.x);
        repaint();
    }
    else if (dragMode == DragMode::move && moveRegion >= 0 && moveRegion < regions.size())
    {
        const auto duration = moveOriginalEnd - moveOriginalStart;
        const auto previousEnd = moveRegion > 0 ? regions.getReference(moveRegion - 1).endTime : 0.0;
        const auto nextStart = moveRegion + 1 < regions.size()
                             ? regions.getReference(moveRegion + 1).startTime
                             : getTimelineDurationSeconds();
        const auto latestStart = juce::jmax(previousEnd, nextStart - duration);
        const auto proposedStart = moveOriginalStart + xToTime(event.x) - moveAnchorTime;
        const auto newStart = juce::jlimit(previousEnd, latestStart, proposedStart);
        const auto newEnd = newStart + duration;
        auto& region = regions.getReference(moveRegion);

        if (! moveUndoPushed && std::abs(newStart - region.startTime) > 1.0e-9)
        {
            pushUndoState();
            moveUndoPushed = true;
        }

        if (moveUndoPushed)
        {
            region.startTime = newStart;
            region.endTime = newEnd;
            region.startSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(newStart * sampleRate)) : region.startSample;
            region.endSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(newEnd * sampleRate)) : region.endSample;
            deferredRegionDisplayRebuild = true;
            repaint();
        }
    }
    else if ((dragMode == DragMode::resizeStart || dragMode == DragMode::resizeEnd)
          && resizeRegion >= 0
          && resizeRegion < regions.size())
    {
        auto& region = regions.getReference(resizeRegion);
        const auto time = juce::jlimit(0.0, getDurationSeconds(), xToTime(event.x));
        const auto previousEnd = resizeRegion > 0 ? regions.getReference(resizeRegion - 1).endTime : 0.0;
        const auto nextStart = resizeRegion + 1 < regions.size() ? regions.getReference(resizeRegion + 1).startTime : getTimelineDurationSeconds();

        if (dragMode == DragMode::resizeStart)
            region.startTime = juce::jlimit(previousEnd, region.endTime - minRegionSeconds, time);
        else
            region.endTime = juce::jlimit(region.startTime + minRegionSeconds, nextStart, time);

        region.startSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(region.startTime * sampleRate)) : region.startSample;
        region.endSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(region.endTime * sampleRate)) : region.endSample;
        deferredRegionDisplayRebuild = true;
        repaint();
    }
}

void LegacyWaveform124::mouseUp(const juce::MouseEvent& /*event*/)
{
    if (dragMode == DragMode::create)
    {
        const auto start = juce::jlimit(0.0, getDurationSeconds(), juce::jmin(createStart, createEnd));
        const auto end = juce::jlimit(0.0, getDurationSeconds(), juce::jmax(createStart, createEnd));

        if (end - start >= 0.02)
        {
            QQDeBreathBridgeRegion region;
            region.type = "Breath";
            region.startTime = start;
            region.endTime = end;
            region.startSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(start * sampleRate)) : 0;
            region.endSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(end * sampleRate)) : 0;
            const auto newIndex = insertRegionReplacingOverlaps(region);
            selectRegion(newIndex);
            notifyRegionsChanged();
        }
    }

    if (dragMode == DragMode::move && moveUndoPushed)
    {
        deferredRegionDisplayRebuild = false;
        notifyRegionsChanged();
    }

    if (dragMode == DragMode::resizeStart || dragMode == DragMode::resizeEnd)
    {
        deferredRegionDisplayRebuild = false;
        notifyRegionsChanged();
    }

    dragMode = DragMode::none;
    moveRegion = -1;
    moveUndoPushed = false;
    resizeRegion = -1;
    repaint();
}

void LegacyWaveform124::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto duration = getTimelineDurationSeconds();
    if (duration <= 0.0)
        return;

    auto span = juce::jmax(minViewSeconds, viewEnd - viewStart);

    if (event.mods.isShiftDown())
    {
        const auto amount = std::abs(wheel.deltaY) >= std::abs(wheel.deltaX) ? wheel.deltaY : -wheel.deltaX;
        const auto step = span * (amount < 0.0f ? 0.18 : -0.18);
        setView(viewStart + step, viewEnd + step);
    }
    else if (std::abs(wheel.deltaY) > std::abs(wheel.deltaX))
    {
        const auto centre = xToTime(event.x);
        const auto factor = wheel.deltaY > 0.0f ? 0.8 : 1.25;
        const auto newSpan = juce::jlimit(minViewSeconds, duration, span * factor);
        const auto ratio = (centre - viewStart) / span;
        setView(centre - ratio * newSpan, centre + (1.0 - ratio) * newSpan);
    }
    else
    {
        const auto step = span * (wheel.deltaX < 0.0f ? 0.18 : -0.18);
        setView(viewStart + step, viewEnd + step);
    }
}

bool LegacyWaveform124::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (selectedRegion >= 0 && selectedRegion < regions.size())
        {
            pushUndoState();
            regions.remove(selectedRegion);
            selectRegion(-1);
            notifyRegionsChanged();
            repaint();
            return true;
        }
    }

    return false;
}

int LegacyWaveform124::timeToX(double seconds) const
{
    const auto area = getWaveformArea();
    return area.getX() + static_cast<int>((seconds - viewStart) / juce::jmax(1.0e-9, viewEnd - viewStart) * area.getWidth());
}

double LegacyWaveform124::xToTime(int x) const
{
    const auto area = getWaveformArea();
    return viewStart + (static_cast<double>(x - area.getX()) / juce::jmax(1, area.getWidth())) * (viewEnd - viewStart);
}

int LegacyWaveform124::hitTestRegion(int x) const
{
    const auto time = xToTime(x);
    for (auto i = regions.size(); --i >= 0;)
    {
        const auto& region = regions.getReference(i);
        if (time >= region.startTime && time <= region.endTime)
            return i;
    }

    return -1;
}

int LegacyWaveform124::hitTestRegionStartEdge(int x) const
{
    const auto tolerance = juce::jmax(5, getWaveformArea().getWidth() / 240);
    for (auto i = regions.size(); --i >= 0;)
    {
        const auto edgeX = timeToX(regions.getReference(i).startTime);
        if (std::abs(edgeX - x) <= tolerance)
            return i;
    }

    return -1;
}

int LegacyWaveform124::hitTestRegionEndEdge(int x) const
{
    const auto tolerance = juce::jmax(5, getWaveformArea().getWidth() / 240);
    for (auto i = regions.size(); --i >= 0;)
    {
        const auto edgeX = timeToX(regions.getReference(i).endTime);
        if (std::abs(edgeX - x) <= tolerance)
            return i;
    }

    return -1;
}

void LegacyWaveform124::updateMouseCursor(int x)
{
    if (hitTestRegionStartEdge(x) >= 0 || hitTestRegionEndEdge(x) >= 0)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else if (hitTestRegion(x) >= 0)
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void LegacyWaveform124::selectRegion(int index)
{
    selectedRegion = index >= 0 && index < regions.size() ? index : -1;

    if (onSelectedRegionChanged != nullptr)
        onSelectedRegionChanged(selectedRegion);
}

void LegacyWaveform124::pushUndoState()
{
    undoStack.add(regions);
    redoStack.clear();

    constexpr auto maxUndoSteps = 64;
    while (undoStack.size() > maxUndoSteps)
        undoStack.remove(0);
}

void LegacyWaveform124::notifyRegionsChanged(bool rebuildDisplay)
{
    for (auto& region : regions)
    {
        region.type = normalizedType(region.type);
        region.startTime = juce::jlimit(0.0, getTimelineDurationSeconds(), region.startTime);
        region.endTime = juce::jlimit(region.startTime, getTimelineDurationSeconds(), region.endTime);
        region.startSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(region.startTime * sampleRate)) : region.startSample;
        region.endSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(region.endTime * sampleRate)) : region.endSample;
        region.gainDb = juce::jlimit(-30.0, 30.0, region.gainDb);
        region.eqState = sanitizeBreathEqState(region.eqState);
    }

    if (rebuildDisplay)
        rebuildBreathNormGainCache();

    if (onRegionsChanged != nullptr)
        onRegionsChanged(regions);
}

int LegacyWaveform124::insertRegionReplacingOverlaps(QQDeBreathBridgeRegion region)
{
    pushUndoState();

    region.type = normalizedType(region.type);
    region.startTime = juce::jlimit(0.0, getTimelineDurationSeconds(), region.startTime);
    region.endTime = juce::jlimit(region.startTime, getTimelineDurationSeconds(), region.endTime);
    region.startSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(region.startTime * sampleRate)) : 0;
    region.endSample = sampleRate > 0.0 ? static_cast<juce::int64>(std::round(region.endTime * sampleRate)) : 0;

    juce::Array<QQDeBreathBridgeRegion> adjusted;
    for (const auto& oldRegion : regions)
    {
        if (oldRegion.endTime <= region.startTime || oldRegion.startTime >= region.endTime)
        {
            adjusted.add(oldRegion);
            continue;
        }

        if (oldRegion.startTime < region.startTime && region.startTime - oldRegion.startTime >= minRegionSeconds)
        {
            auto left = oldRegion;
            left.endTime = region.startTime;
            left.endSample = region.startSample;
            adjusted.add(left);
        }

        if (oldRegion.endTime > region.endTime && oldRegion.endTime - region.endTime >= minRegionSeconds)
        {
            auto right = oldRegion;
            right.startTime = region.endTime;
            right.startSample = region.endSample;
            adjusted.add(right);
        }
    }

    adjusted.add(region);

    for (auto i = 0; i < adjusted.size(); ++i)
        for (auto j = i + 1; j < adjusted.size(); ++j)
            if (adjusted.getReference(j).startTime < adjusted.getReference(i).startTime)
                adjusted.swap(i, j);

    regions = adjusted;
    rebuildBreathNormGainCache();

    for (auto i = 0; i < regions.size(); ++i)
        if (sameRegionTime(regions.getReference(i), region))
            return i;

    return -1;
}

void LegacyWaveform124::fitView()
{
    viewStart = 0.0;
    viewEnd = juce::jmax(minViewSeconds, getTimelineDurationSeconds());
    updateScrollBar();
}

void LegacyWaveform124::setView(double start, double end)
{
    const auto duration = getTimelineDurationSeconds();
    auto span = juce::jlimit(minViewSeconds, juce::jmax(minViewSeconds, duration), end - start);
    start = juce::jlimit(0.0, juce::jmax(0.0, duration - span), start);
    viewStart = start;
    viewEnd = juce::jmin(duration, start + span);
    updateScrollBar();
    repaint();
}

juce::Rectangle<int> LegacyWaveform124::getWaveformArea() const
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromBottom(16);
    return area;
}

void LegacyWaveform124::updateScrollBar()
{
    if (recordingOverlay)
    {
        horizontalScrollBar.setVisible(false);
        return;
    }

    const auto duration = getTimelineDurationSeconds();
    const auto span = juce::jmax(minViewSeconds, viewEnd - viewStart);
    updatingScrollBar = true;
    horizontalScrollBar.setRangeLimits(0.0, juce::jmax(minViewSeconds, duration));
    horizontalScrollBar.setCurrentRange(viewStart, span);
    horizontalScrollBar.setVisible(duration > span + 0.001);
    updatingScrollBar = false;
}

void LegacyWaveform124::scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (updatingScrollBar || scrollBarThatHasMoved != &horizontalScrollBar)
        return;

    const auto span = juce::jmax(minViewSeconds, viewEnd - viewStart);
    setView(newRangeStart, newRangeStart + span);
}

juce::Colour LegacyWaveform124::regionColour(const juce::String& type) const
{
    if (type.equalsIgnoreCase("Noize"))
        return monitorNoize ? juce::Colour(0xff8b5cf6).withAlpha(0.42f)
                            : juce::Colour(0xff64748b).withAlpha(0.28f);

    return monitorBreath ? juce::Colour(0xff22c55e).withAlpha(0.38f)
                         : juce::Colour(0xff64748b).withAlpha(0.28f);
}

juce::int64 LegacyWaveform124::regionStartSample(const QQDeBreathBridgeRegion& region) const
{
    if (sampleRate <= 0.0)
        return 0;

    return juce::jlimit<juce::int64>(0,
                                     monoWaveform.getNumSamples(),
                                     region.startSample > 0 ? region.startSample
                                                            : static_cast<juce::int64>(std::llround(region.startTime * sampleRate)));
}

juce::int64 LegacyWaveform124::regionEndSample(const QQDeBreathBridgeRegion& region) const
{
    if (sampleRate <= 0.0)
        return 0;

    return juce::jlimit<juce::int64>(0,
                                     monoWaveform.getNumSamples(),
                                     region.endSample > 0 ? region.endSample
                                                          : static_cast<juce::int64>(std::llround(region.endTime * sampleRate)));
}

bool LegacyWaveform124::hasAdjacentRegionBefore(int regionIndex, juce::int64 start, int fadeSamples) const
{
    const auto tolerance = juce::jmax<juce::int64>(1, fadeSamples);
    for (auto i = 0; i < regions.size(); ++i)
    {
        if (i == regionIndex)
            continue;

        const auto otherEnd = regionEndSample(regions.getReference(i));
        if (std::abs(otherEnd - start) <= tolerance)
            return true;
    }

    return false;
}

bool LegacyWaveform124::hasAdjacentRegionAfter(int regionIndex, juce::int64 end, int fadeSamples) const
{
    const auto tolerance = juce::jmax<juce::int64>(1, fadeSamples);
    for (auto i = 0; i < regions.size(); ++i)
    {
        if (i == regionIndex)
            continue;

        const auto otherStart = regionStartSample(regions.getReference(i));
        if (std::abs(otherStart - end) <= tolerance)
            return true;
    }

    return false;
}

double LegacyWaveform124::regionWeightAtSample(int regionIndex,
                                                      juce::int64 sampleIndex,
                                                      int fadeInSamples,
                                                      int fadeOutSamples) const
{
    const auto& region = regions.getReference(regionIndex);
    const auto start = regionStartSample(region);
    const auto end = regionEndSample(region);
    const auto adjacentBefore = fadeInSamples > 0 && hasAdjacentRegionBefore(regionIndex, start, fadeInSamples);
    const auto adjacentAfter = fadeOutSamples > 0 && hasAdjacentRegionAfter(regionIndex, end, fadeOutSamples);

    if (sampleIndex >= start && sampleIndex < end)
    {
        auto weight = 1.0;
        if (fadeInSamples > 0 && ! adjacentBefore)
            weight = juce::jmin(weight, static_cast<double>(sampleIndex - start) / juce::jmax(1, fadeInSamples - 1));

        if (fadeOutSamples > 0 && ! adjacentAfter)
            weight = juce::jmin(weight, static_cast<double>(end - 1 - sampleIndex) / juce::jmax(1, fadeOutSamples - 1));

        return juce::jlimit(0.0, 1.0, weight);
    }

    if (adjacentBefore && sampleIndex >= start - fadeInSamples && sampleIndex < start)
        return juce::jlimit(0.0, 1.0, static_cast<double>(sampleIndex - (start - fadeInSamples)) / juce::jmax(1, fadeInSamples));

    if (adjacentAfter && sampleIndex >= end && sampleIndex < end + fadeOutSamples)
        return juce::jlimit(0.0, 1.0, 1.0 - static_cast<double>(sampleIndex - end) / juce::jmax(1, fadeOutSamples));

    return 0.0;
}

double LegacyWaveform124::breathNormGainForRegion(int regionIndex) const
{
    if (regionIndex >= 0 && regionIndex < breathNormGainCache.size())
        return breathNormGainCache.getReference(regionIndex);

    return 1.0;
}

double LegacyWaveform124::displayRegionWeightAtSample(const DisplayRegion& region,
                                                             juce::int64 sampleIndex,
                                                             int fadeInSamples,
                                                             int fadeOutSamples) const
{
    const auto start = region.startSample;
    const auto end = region.endSample;

    if (sampleIndex >= start && sampleIndex < end)
    {
        auto weight = 1.0;
        if (fadeInSamples > 0 && ! region.adjacentBefore)
            weight = juce::jmin(weight, static_cast<double>(sampleIndex - start) / juce::jmax(1, fadeInSamples - 1));

        if (fadeOutSamples > 0 && ! region.adjacentAfter)
            weight = juce::jmin(weight, static_cast<double>(end - 1 - sampleIndex) / juce::jmax(1, fadeOutSamples - 1));

        return juce::jlimit(0.0, 1.0, weight);
    }

    if (region.adjacentBefore && sampleIndex >= start - fadeInSamples && sampleIndex < start)
        return juce::jlimit(0.0, 1.0, static_cast<double>(sampleIndex - (start - fadeInSamples)) / juce::jmax(1, fadeInSamples));

    if (region.adjacentAfter && sampleIndex >= end && sampleIndex < end + fadeOutSamples)
        return juce::jlimit(0.0, 1.0, 1.0 - static_cast<double>(sampleIndex - end) / juce::jmax(1, fadeOutSamples));

    return 0.0;
}

double LegacyWaveform124::renderedDisplaySample(juce::int64 sampleIndex, bool activeComponents) const
{
    if (sampleIndex < 0 || sampleIndex >= monoWaveform.getNumSamples())
        return 0.0;

    const auto dry = static_cast<double>(monoWaveform.getSample(0, static_cast<int>(sampleIndex)));
    const auto fadeInSamples = processingParams.enableFade && sampleRate > 0.0
                             ? static_cast<int>(std::llround(processingParams.fadeInMs * sampleRate / 1000.0))
                             : 0;
    const auto fadeOutSamples = processingParams.enableFade && sampleRate > 0.0
                              ? static_cast<int>(std::llround(processingParams.fadeOutMs * sampleRate / 1000.0))
                              : 0;
    auto breathWeight = 0.0;
    auto noizeWeight = 0.0;
    auto breathNormGain = 1.0;

    for (const auto& region : displayRegions)
    {
        const auto influenceStart = region.startSample - fadeInSamples;
        const auto influenceEnd = region.endSample + fadeOutSamples;
        if (sampleIndex < influenceStart)
            break;
        if (sampleIndex >= influenceEnd)
            continue;

        const auto weight = displayRegionWeightAtSample(region, sampleIndex, fadeInSamples, fadeOutSamples);
        if (weight <= 0.0)
            continue;

        if (region.isNoize)
        {
            noizeWeight = juce::jmax(noizeWeight, weight);
        }
        else if (weight >= breathWeight)
        {
            breathWeight = weight;
            breathNormGain = processingParams.normalizeBreath ? region.normGain : 1.0;
        }
    }

    const auto nonVoiceSum = breathWeight + noizeWeight;
    if (nonVoiceSum > 1.0)
    {
        breathWeight /= nonVoiceSum;
        noizeWeight /= nonVoiceSum;
    }

    const auto voiceWeight = juce::jlimit(0.0, 1.0, 1.0 - breathWeight - noizeWeight);
    auto mixed = 0.0;

    if (monitorVoice == activeComponents)
        mixed += dry * voiceWeight;
    if (monitorBreath == activeComponents)
    {
        if (processedBreathDisplay.getNumSamples() == monoWaveform.getNumSamples()
            && sampleIndex < processedBreathDisplay.getNumSamples())
            mixed += static_cast<double>(processedBreathDisplay.getSample(0, static_cast<int>(sampleIndex)));
        else
            mixed += dry * breathWeight * breathNormGain * dbToGain(processingParams.breathGainDb);
    }
    if (monitorNoize == activeComponents)
        mixed += dry * noizeWeight;

    return mixed;
}

void LegacyWaveform124::drawFadeGuides(juce::Graphics& g, juce::Rectangle<int> area) const
{
    if (! processingParams.enableFade || sampleRate <= 0.0 || displayRegions.isEmpty())
        return;

    const auto fadeInSamples = static_cast<juce::int64>(std::llround(processingParams.fadeInMs * sampleRate / 1000.0));
    const auto fadeOutSamples = static_cast<juce::int64>(std::llround(processingParams.fadeOutMs * sampleRate / 1000.0));
    if (fadeInSamples <= 0 && fadeOutSamples <= 0)
        return;

    const auto top = static_cast<float>(area.getY() + 4);
    const auto bottom = static_cast<float>(area.getBottom() - 4);
    g.setColour(juce::Colour(0xffe0f2fe).withAlpha(0.50f));

    for (const auto& region : displayRegions)
    {
        const auto startSeconds = static_cast<double>(region.startSample) / sampleRate;
        const auto endSeconds = static_cast<double>(region.endSample) / sampleRate;

        if (fadeInSamples > 0 && ! region.adjacentBefore)
        {
            const auto x1 = static_cast<float>(timeToX(startSeconds));
            const auto x2 = static_cast<float>(timeToX(static_cast<double>(region.startSample + fadeInSamples) / sampleRate));
            if (x2 >= area.getX() && x1 <= area.getRight())
                g.drawLine(x1, bottom, x2, top, 1.0f);
        }

        if (region.adjacentAfter && fadeOutSamples > 0)
        {
            const auto x1 = static_cast<float>(timeToX(static_cast<double>(region.endSample - fadeOutSamples) / sampleRate));
            const auto x2 = static_cast<float>(timeToX(static_cast<double>(region.endSample + fadeInSamples) / sampleRate));
            if (x2 >= area.getX() && x1 <= area.getRight())
            {
                g.drawLine(x1, top, x2, bottom, 1.0f);
                g.drawLine(x1, bottom, x2, top, 1.0f);
            }
        }
        else if (fadeOutSamples > 0)
        {
            const auto x1 = static_cast<float>(timeToX(static_cast<double>(region.endSample - fadeOutSamples) / sampleRate));
            const auto x2 = static_cast<float>(timeToX(endSeconds));
            if (x2 >= area.getX() && x1 <= area.getRight())
                g.drawLine(x1, top, x2, bottom, 1.0f);
        }
    }
}

void LegacyWaveform124::rebuildBreathNormGainCache()
{
    breathNormGainCache.clear();
    displayRegions.clear();

    for (auto regionIndex = 0; regionIndex < regions.size(); ++regionIndex)
    {
        const auto& region = regions.getReference(regionIndex);
        auto gain = 1.0;
        if (sampleRate > 0.0 && monoWaveform.getNumSamples() > 0 && ! region.type.equalsIgnoreCase("Noize"))
        {
            const auto start = regionStartSample(region);
            const auto end = regionEndSample(region);
            auto peak = 0.0f;
            const auto* data = monoWaveform.getReadPointer(0);

            for (auto sample = start; sample < end; ++sample)
                peak = juce::jmax(peak, std::abs(data[static_cast<int>(sample)]));

            if (peak > 1.0e-9f)
                gain = dbToGain(processingParams.breathTargetDb) / static_cast<double>(peak);
        }

        breathNormGainCache.add(gain);

        DisplayRegion displayRegion;
        displayRegion.sourceIndex = regionIndex;
        displayRegion.isNoize = region.type.equalsIgnoreCase("Noize");
        displayRegion.startSample = regionStartSample(region);
        displayRegion.endSample = regionEndSample(region);
        displayRegion.normGain = gain;
        displayRegions.add(displayRegion);
    }

    for (auto i = 0; i < displayRegions.size(); ++i)
        for (auto j = i + 1; j < displayRegions.size(); ++j)
            if (displayRegions.getReference(j).startSample < displayRegions.getReference(i).startSample)
                displayRegions.swap(i, j);

    const auto fadeTolerance = processingParams.enableFade && sampleRate > 0.0
                             ? juce::jmax<juce::int64>(1, static_cast<juce::int64>(std::llround(juce::jmax(processingParams.fadeInMs, processingParams.fadeOutMs) * sampleRate / 1000.0)))
                             : 1;

    for (auto i = 0; i < displayRegions.size(); ++i)
    {
        auto& region = displayRegions.getReference(i);
        for (auto j = 0; j < displayRegions.size(); ++j)
        {
            if (i == j)
                continue;

            const auto& other = displayRegions.getReference(j);
            const auto beforeDistance = other.endSample > region.startSample ? other.endSample - region.startSample
                                                                             : region.startSample - other.endSample;
            const auto afterDistance = other.startSample > region.endSample ? other.startSample - region.endSample
                                                                            : region.endSample - other.startSample;
            if (beforeDistance <= fadeTolerance)
                region.adjacentBefore = true;
            if (afterDistance <= fadeTolerance)
                region.adjacentAfter = true;
        }
    }

    rebuildProcessedBreathDisplay();
}

double LegacyWaveform124::dbToGain(double db)
{
    return std::pow(10.0, db / 20.0);
}

juce::String LegacyWaveform124::buildProcessedBreathDisplayKey() const
{
    juce::String key;
    key << "samples=" << monoWaveform.getNumSamples()
        << "|sr=" << juce::String(sampleRate, 6)
        << "|fade=" << (processingParams.enableFade ? 1 : 0)
        << "|" << juce::String(processingParams.fadeInMs, 4)
        << "|" << juce::String(processingParams.fadeOutMs, 4)
        << "|norm=" << (processingParams.normalizeBreath ? 1 : 0)
        << "|" << juce::String(processingParams.breathTargetDb, 4)
        << "|gain=" << juce::String(processingParams.breathGainDb, 4)
        << "|geq=" << serializeBreathEqState(processingParams.breathEqState);

    for (const auto& region : regions)
    {
        key << "|" << region.type
            << ":" << juce::String(region.startSample)
            << "-" << juce::String(region.endSample)
            << ":" << juce::String(region.startTime, 6)
            << "-" << juce::String(region.endTime, 6)
            << ":g" << juce::String(region.gainDb, 3)
            << ":eq" << serializeBreathEqState(region.eqState);
    }

    return key;
}

void LegacyWaveform124::rebuildProcessedBreathDisplay()
{
    const auto key = buildProcessedBreathDisplayKey();
    if (key == processedBreathDisplayKey
        && processedBreathDisplay.getNumSamples() == monoWaveform.getNumSamples())
        return;

    processedBreathDisplayKey = key;
    processedBreathDisplay.setSize(1, monoWaveform.getNumSamples(), false, false, true);
    processedBreathDisplay.clear();

    if (monoWaveform.getNumSamples() <= 0 || sampleRate <= 0.0 || displayRegions.isEmpty())
        return;

    const auto fadeInSamples = processingParams.enableFade
                             ? static_cast<int>(std::llround(processingParams.fadeInMs * sampleRate / 1000.0))
                             : 0;
    const auto fadeOutSamples = processingParams.enableFade
                              ? static_cast<int>(std::llround(processingParams.fadeOutMs * sampleRate / 1000.0))
                              : 0;
    const auto globalBreathGain = dbToGain(processingParams.breathGainDb);

    auto fillRegionContribution = [&](juce::AudioBuffer<float>& dest,
                                      int destOffset,
                                      int regionIndex,
                                      juce::int64 sourceStart,
                                      int numSamples)
    {
        if (regionIndex < 0 || regionIndex >= regions.size())
            return;

        const auto& region = regions.getReference(regionIndex);
        if (region.type.equalsIgnoreCase("Noize"))
            return;

        juce::AudioBuffer<float> regionBuffer(1, numSamples);
        regionBuffer.clear();

        const auto normGain = processingParams.normalizeBreath ? breathNormGainForRegion(regionIndex) : 1.0;
        const auto regionGain = dbToGain(juce::jlimit(-30.0, 30.0, region.gainDb));
        for (auto i = 0; i < numSamples; ++i)
        {
            const auto sourceSample = sourceStart + i;
            if (sourceSample < 0 || sourceSample >= monoWaveform.getNumSamples())
                continue;

            const auto weight = regionWeightAtSample(regionIndex,
                                                     sourceSample,
                                                     fadeInSamples,
                                                     fadeOutSamples);
            if (weight <= 0.0)
                continue;

            const auto dry = static_cast<double>(monoWaveform.getSample(0, static_cast<int>(sourceSample)));
            regionBuffer.addSample(0, i, static_cast<float>(dry * weight * normGain * globalBreathGain * regionGain));
        }

        if (processingParams.breathEqState.hasActiveProcessing())
        {
            QQDeBreathEqProcessor processor;
            processor.prepare(sampleRate, 1, processingParams.breathEqState);
            processor.process(regionBuffer);
        }

        if (region.eqState.hasActiveProcessing())
        {
            QQDeBreathEqProcessor processor;
            processor.prepare(sampleRate, 1, region.eqState);
            processor.process(regionBuffer);
        }

        dest.addFrom(0, destOffset, regionBuffer, 0, 0, numSamples);
    };

    for (auto regionIndex = 0; regionIndex < regions.size(); ++regionIndex)
    {
        const auto& region = regions.getReference(regionIndex);
        if (region.type.equalsIgnoreCase("Noize"))
            continue;

        const auto start = juce::jmax<juce::int64>(0, regionStartSample(region) - fadeInSamples);
        const auto end = juce::jmin<juce::int64>(monoWaveform.getNumSamples(), regionEndSample(region) + fadeOutSamples);
        if (end > start)
            fillRegionContribution(processedBreathDisplay, static_cast<int>(start), regionIndex, start, static_cast<int>(end - start));
    }
}
