// Frozen reference of the 1.23 export DSP for sample-by-sample regression only.
#pragma once
#include "shared/StemExport.h"
#include <cmath>
namespace LegacyStemExport123 {
double dbToGain(double db) { return std::pow(10.0, db / 20.0); }
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

void render(const juce::AudioBuffer<float>& source, double sampleRate,
 const juce::Array<QQDeBreathBridgeRegion>& regions, const QQDeBreathStemExport::Settings& settings,
 QQDeBreathStemExport::Stems& stems) {
 QQDeBreathBridgeAnalysisResult result; result.regions = regions;
 const auto applyEq = [&](juce::AudioBuffer<float>& buffer, double rate) {
 if (!settings.globalEq.hasActiveProcessing()) return;
 QQDeBreathEqProcessor eq; eq.prepare(rate, buffer.getNumChannels(), settings.globalEq); eq.process(buffer); };
    juce::AudioBuffer<float> vocalOnly(source.getNumChannels(), source.getNumSamples());
    juce::AudioBuffer<float> breath(source.getNumChannels(), source.getNumSamples());
    juce::AudioBuffer<float> noize(source.getNumChannels(), source.getNumSamples());
    vocalOnly.clear();
    breath.clear();
    noize.clear();

    const auto enableFade = settings.enableFade;
    const auto normalizeBreath = settings.normalizeBreath;
    const auto breathTargetDb = static_cast<double>(settings.breathTargetDb);
    const auto breathTargetGain = dbToGain(breathTargetDb);
    const auto breathAdjustGain = dbToGain(juce::jlimit(-60.0, 30.0, static_cast<double>(settings.breathGainDb)));
    const auto fadeInSamples = enableFade
                             ? static_cast<int>(std::llround(settings.fadeInMs * sampleRate / 1000.0))
                             : 0;
    const auto fadeOutSamples = enableFade
                              ? static_cast<int>(std::llround(settings.fadeOutMs * sampleRate / 1000.0))
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

    applyEq(breath, sampleRate);

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

        applyEq(regionBreath, sampleRate);
        QQDeBreathEqProcessor regionProcessor;
        regionProcessor.prepare(sampleRate, regionBreath.getNumChannels(), region.eqState);
        regionProcessor.process(regionBreath);

        for (auto channel = 0; channel < breath.getNumChannels(); ++channel)
        {
            breath.clear(channel, static_cast<int>(start), regionBreath.getNumSamples());
            breath.addFrom(channel, static_cast<int>(start), regionBreath, channel, 0, regionBreath.getNumSamples());
        }
    }


stems.vocal=std::move(vocalOnly);stems.breath=std::move(breath);stems.noize=std::move(noize);
}
}
