#pragma once
// Frozen 1.25 ARA renderer, test-only regression oracle.
#include "PluginProcessor.h"
#include <array>
namespace LegacyAra125 {
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

juce::String buildRecordingFingerprint(double sampleRate, int channels, juce::int64 samples)
{
    if (sampleRate <= 0.0 || channels <= 0 || samples <= 0)
        return {};

    return fnv1a64("vst3-recorded|sr=" + juce::String(sampleRate, 8)
                 + "|ch=" + juce::String(channels)
                 + "|samples=" + juce::String(samples));
}

juce::String buildAraSourceFingerprint(const juce::ARAAudioSource& source)
{
    const auto id = juce::String::fromUTF8(source.getPersistentID().c_str());
    return fnv1a64(id + "|sr=" + juce::String(source.getSampleRate(), 8)
                 + "|ch=" + juce::String(static_cast<int>(source.getChannelCount()))
                 + "|samples=" + juce::String(static_cast<juce::int64>(source.getSampleCount())));
}

double dbToGain(double db)
{
    return std::pow(10.0, db / 20.0);
}

double regionWeight(juce::int64 sample,
                    juce::int64 start,
                    juce::int64 end,
                    int fadeInSamples,
                    int fadeOutSamples)
{
    if (sample < start || sample >= end)
        return 0.0;

    auto weight = 1.0;
    if (fadeInSamples > 0)
        weight = juce::jmin(weight, static_cast<double>(sample - start) / juce::jmax(1, fadeInSamples - 1));

    if (fadeOutSamples > 0)
        weight = juce::jmin(weight, static_cast<double>(end - 1 - sample) / juce::jmax(1, fadeOutSamples - 1));

    return juce::jlimit(0.0, 1.0, weight);
}

float sampleAt(const juce::AudioBuffer<float>& buffer, int channel, juce::int64 sample)
{
    if (sample < 0 || sample >= buffer.getNumSamples() || buffer.getNumChannels() <= 0)
        return 0.0f;

    return buffer.getSample(juce::jmin(channel, buffer.getNumChannels() - 1), static_cast<int>(sample));
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

class QQDeBreathARAPlaybackRenderer final : public juce::ARAPlaybackRenderer
{
public:
    QQDeBreathARAPlaybackRenderer(ARA::PlugIn::DocumentController* documentController,
                                  QQDeBreathARADocumentController& ownerIn)
        : juce::ARAPlaybackRenderer(documentController),
          owner(ownerIn)
    {
    }

    void prepareToPlay(double sampleRateIn,
                       int maximumSamplesPerBlockIn,
                       int numChannelsIn,
                       juce::AudioProcessor::ProcessingPrecision precision,
                       AlwaysNonRealtime alwaysNonRealtime = AlwaysNonRealtime::no) override
    {
        juce::ARAPlaybackRenderer::prepareToPlay(sampleRateIn, maximumSamplesPerBlockIn, numChannelsIn, precision, alwaysNonRealtime);
        sampleRate = sampleRateIn;
        numChannels = numChannelsIn;
        tempBuffer.setSize(numChannels, maximumSamplesPerBlockIn);
        breathBuffer.setSize(numChannels, maximumSamplesPerBlockIn);
        sourceReadBuffer.setSize(juce::jmax(2, numChannels), maximumSamplesPerBlockIn * 16 + 32);
        breathEqProcessor.reset();
        regionEqProcessor.reset();
        sourceCaches = {};
        nextSourceCache = 0;
        cachedPlaybackParams = owner.getPlaybackParams();
        cachedPlaybackParamsRevision = owner.getPlaybackParamsRevision();
    }

    bool processBlock(juce::AudioBuffer<float>& buffer,
                      juce::AudioProcessor::Realtime /*realtime*/,
                      const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept override
    {
        buffer.clear();

        if (! positionInfo.getIsPlaying() || sampleRate <= 0.0)
            return true;

        juce::int64 blockStart = 0;
        if (const auto timeInSamples = positionInfo.getTimeInSamples(); timeInSamples.hasValue())
            blockStart = *timeInSamples;
        else if (const auto timeInSeconds = positionInfo.getTimeInSeconds(); timeInSeconds.hasValue())
            blockStart = static_cast<juce::int64>(std::llround(*timeInSeconds * sampleRate));
        else
            return true;

        const auto blockEnd = blockStart + buffer.getNumSamples();
        refreshPlaybackParams();

        if (tempBuffer.getNumChannels() != buffer.getNumChannels() || tempBuffer.getNumSamples() < buffer.getNumSamples())
            tempBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);

        for (const auto* playbackRegion : getPlaybackRegions<juce::ARAPlaybackRegion>())
        {
            if (playbackRegion == nullptr)
                continue;

            const auto* audioModification = playbackRegion->getAudioModification();
            if (audioModification == nullptr)
                continue;

            auto* audioSource = audioModification->getAudioSource<juce::ARAAudioSource>();
            if (audioSource == nullptr || audioSource->getSampleRate() <= 0.0)
                continue;

            const auto sourceSampleRate = audioSource->getSampleRate();
            const auto regionStart = playbackRegion->getStartInPlaybackSamples(sampleRate);
            const auto regionEnd = playbackRegion->getEndInPlaybackSamples(sampleRate);
            if (blockEnd <= regionStart || regionEnd <= blockStart)
                continue;

            const auto copyStartInSong = juce::jmax<juce::int64>(blockStart, regionStart);
            const auto copyEndInSong = juce::jmin<juce::int64>(blockEnd, regionEnd);
            if (copyEndInSong <= copyStartInSong)
                continue;

            const auto destOffset = static_cast<int>(copyStartInSong - blockStart);
            const auto numSamples = static_cast<int>(copyEndInSong - copyStartInSong);
            const auto sourceStep = sourceSampleRate / sampleRate;
            const auto copyStartTime = static_cast<double>(copyStartInSong) / sampleRate;
            const auto sourceStartPosition = (playbackRegion->getStartInAudioModificationTime()
                                            + copyStartTime
                                            - playbackRegion->getStartInPlaybackTime()) * sourceSampleRate;
            auto& sourceCache = getSourceCache(*audioSource);
            if (! renderSourceAudio(*audioSource, sourceCache, sourceStartPosition, sourceStep, numSamples))
                continue;
            const auto& state = sourceCache.state;
            const auto hasState = sourceCache.hasState;
            const auto& params = sourceCache.hasPersistentState
                               ? sourceCache.state.playbackParams
                               : cachedPlaybackParams;

            if (! hasState)
            {
                // Before analysis, the whole source is Voice and must obey its monitor switch.
                if (params.monitorVoice)
                    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
                        buffer.addFrom(channel, destOffset, tempBuffer, juce::jmin(channel, tempBuffer.getNumChannels() - 1), 0, numSamples);
                continue;
            }

            const auto fadeInSamples = params.enableFade ? static_cast<int>(std::llround(params.fadeInMs * sourceSampleRate / 1000.0)) : 0;
            const auto fadeOutSamples = params.enableFade ? static_cast<int>(std::llround(params.fadeOutMs * sourceSampleRate / 1000.0)) : 0;
            const auto breathTargetGain = dbToGain(params.breathTargetDb);
            const auto breathAdjustGain = dbToGain(juce::jlimit(-60.0, 30.0, params.breathGainDb));
            if (breathBuffer.getNumChannels() != buffer.getNumChannels() || breathBuffer.getNumSamples() < numSamples)
                breathBuffer.setSize(buffer.getNumChannels(), numSamples, false, false, true);
            for (auto channel = 0; channel < breathBuffer.getNumChannels(); ++channel)
                breathBuffer.clear(channel, 0, numSamples);
            QQDeBreathEqState blockRegionEqState;
            auto blockRegionEqIndex = -1;
            auto hasSingleRegionEqForBlock = false;
            auto hasMultipleRegionEqForBlock = false;

            for (auto sampleOffset = 0; sampleOffset < numSamples; ++sampleOffset)
            {
                const auto absoluteSourceSample = static_cast<juce::int64>(std::llround(sourceStartPosition + sampleOffset * sourceStep));
                double breathWeight = 0.0;
                double noizeWeight = 0.0;
                double breathNormGain = 1.0;
                double regionGain = 1.0;

                for (auto regionIndex = 0; regionIndex < state.analysisResult.regions.size(); ++regionIndex)
                {
                    const auto& region = state.analysisResult.regions.getReference(regionIndex);
                    const auto weight = regionWeightForIndex(state.analysisResult.regions,
                                                             regionIndex,
                                                             absoluteSourceSample,
                                                             audioSource->getSampleRate(),
                                                             static_cast<int>(audioSource->getSampleCount()),
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
                        if (region.eqState.hasActiveProcessing())
                        {
                            if (! hasSingleRegionEqForBlock)
                            {
                                blockRegionEqState = region.eqState;
                                blockRegionEqIndex = regionIndex;
                                hasSingleRegionEqForBlock = true;
                            }
                            else if (blockRegionEqIndex != regionIndex)
                            {
                                hasMultipleRegionEqForBlock = true;
                            }
                        }
                        if (params.normalizeBreath)
                        {
                            const auto peak = regionIndex < state.regionPeakCache.size()
                                                ? state.regionPeakCache.getReference(regionIndex)
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

                for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
                {
                    const auto dry = static_cast<double>(sampleAt(tempBuffer,
                                                                  juce::jmin(channel, tempBuffer.getNumChannels() - 1),
                                                                  sampleOffset));
                    auto mixed = 0.0;
                    if (params.monitorVoice)
                        mixed += dry * voiceWeight;
                    if (params.monitorBreath)
                        breathBuffer.setSample(channel, sampleOffset, static_cast<float>(dry * breathWeight * breathNormGain * breathAdjustGain * regionGain));
                    if (params.monitorNoize)
                        mixed += dry * noizeWeight;

                    buffer.addSample(channel, destOffset + sampleOffset, static_cast<float>(mixed));
                }
            }

            if (params.monitorBreath)
            {
                if (params.breathEqState.hasActiveProcessing())
                {
                    breathEqProcessor.prepare(sampleRate, breathBuffer.getNumChannels(), params.breathEqState);
                    breathEqProcessor.process(breathBuffer);
                }

                if (hasSingleRegionEqForBlock && ! hasMultipleRegionEqForBlock && blockRegionEqState.hasActiveProcessing())
                {
                    regionEqProcessor.prepare(sampleRate, breathBuffer.getNumChannels(), blockRegionEqState);
                    regionEqProcessor.process(breathBuffer);
                }

                for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
                    buffer.addFrom(channel, destOffset, breathBuffer, channel, 0, numSamples);
            }
        }

        return true;
    }

private:
    struct SourceRuntimeCache
    {
        juce::ARAAudioSource* source = nullptr;
        juce::String fingerprint;
        QQDeBreathARAPersistentState state;
        std::uint64_t revision = 0;
        bool hasState = false;
        bool hasPersistentState = false;
        QQDeBreathARASourceAudioBuffer audio;
        double audioSampleRate = 0.0;
        std::uint64_t audioRevision = 0;
        bool hasAudio = false;
    };

    void refreshPlaybackParams() noexcept
    {
        const auto revision = owner.getPlaybackParamsRevision();
        if (revision == cachedPlaybackParamsRevision)
            return;

        QQDeBreathARAPlaybackParams updated;
        if (owner.tryGetPlaybackParams(updated))
        {
            cachedPlaybackParams = updated;
            cachedPlaybackParamsRevision = revision;
        }
    }

    bool renderSourceAudio(juce::ARAAudioSource& source,
                           const SourceRuntimeCache& cache,
                           double sourceStartPosition,
                           double sourceStep,
                           int samplesToRender) noexcept
    {
        for (auto channel = 0; channel < tempBuffer.getNumChannels(); ++channel)
            tempBuffer.clear(channel, 0, samplesToRender);

        if (cache.hasAudio && cache.audio != nullptr)
        {
            const auto cachedSamples = cache.audio->getNumSamples();
            const auto cachedChannels = cache.audio->getNumChannels();
            if (cachedSamples <= 0 || cachedChannels <= 0)
                return false;

            for (auto channel = 0; channel < tempBuffer.getNumChannels(); ++channel)
            {
                const auto sourceChannel = juce::jmin(channel, cachedChannels - 1);
                const auto* sourceData = cache.audio->getReadPointer(sourceChannel);
                auto* destData = tempBuffer.getWritePointer(channel);

                for (auto sampleOffset = 0; sampleOffset < samplesToRender; ++sampleOffset)
                {
                    const auto position = sourceStartPosition + sampleOffset * sourceStep;
                    if (position < 0.0 || position >= static_cast<double>(cachedSamples))
                        continue;

                    const auto index0 = juce::jlimit(0, cachedSamples - 1, static_cast<int>(std::floor(position)));
                    const auto index1 = juce::jmin(cachedSamples - 1, index0 + 1);
                    const auto fraction = static_cast<float>(position - std::floor(position));
                    destData[sampleOffset] = sourceData[index0]
                                           + (sourceData[index1] - sourceData[index0]) * fraction;
                }
            }

            return true;
        }

        // ARA sample access is deliberately never performed from the realtime callback.
        // The document controller warms this immutable cache on a worker thread after
        // project restoration. Until then, the renderer returns the already-cleared block.
        return false;
    }
    SourceRuntimeCache& getSourceCache(juce::ARAAudioSource& source) noexcept
    {
        SourceRuntimeCache* cache = nullptr;

        for (auto& candidate : sourceCaches)
        {
            if (candidate.source == &source)
            {
                cache = &candidate;
                break;
            }
        }

        if (cache == nullptr)
        {
            for (auto& candidate : sourceCaches)
            {
                if (candidate.source == nullptr)
                {
                    cache = &candidate;
                    break;
                }
            }
        }

        if (cache == nullptr)
        {
            cache = &sourceCaches[nextSourceCache];
            nextSourceCache = (nextSourceCache + 1) % sourceCaches.size();
        }

        if (cache->source != &source)
        {
            *cache = {};
            cache->source = &source;
            cache->fingerprint = buildAraSourceFingerprint(source);
        }

        const auto revision = owner.getPersistentStateRevision();
        if (cache->revision != revision)
        {
            QQDeBreathARAPersistentState updated;
            auto found = false;
            if (owner.tryGetPersistentStateForSource(cache->fingerprint, updated, found))
            {
                cache->state = found ? std::move(updated) : QQDeBreathARAPersistentState {};
                cache->hasPersistentState = found;
                cache->hasState = found && cache->state.analysisResult.succeeded;
                cache->revision = revision;
            }
        }

        const auto audioRevision = owner.getSourceAudioCacheRevision();
        if (cache->audioRevision != audioRevision)
        {
            QQDeBreathARASourceAudioBuffer updatedAudio;
            auto updatedSampleRate = 0.0;
            auto found = false;
            if (owner.tryGetSourceAudioCache(cache->fingerprint, updatedAudio, updatedSampleRate, found))
            {
                cache->audio = found ? std::move(updatedAudio) : QQDeBreathARASourceAudioBuffer {};
                cache->audioSampleRate = found ? updatedSampleRate : 0.0;
                cache->hasAudio = found && cache->audio != nullptr;
                cache->audioRevision = audioRevision;
            }
        }

        return *cache;
    }

    QQDeBreathARADocumentController& owner;
    double sampleRate = 0.0;
    int numChannels = 0;
    juce::AudioBuffer<float> tempBuffer;
    juce::AudioBuffer<float> sourceReadBuffer;
    juce::AudioBuffer<float> breathBuffer;
    QQDeBreathEqProcessor breathEqProcessor;
    QQDeBreathEqProcessor regionEqProcessor;
    std::array<SourceRuntimeCache, 32> sourceCaches {};
    size_t nextSourceCache = 0;
    QQDeBreathARAPlaybackParams cachedPlaybackParams;
    std::uint64_t cachedPlaybackParamsRevision = 0;
};


}
