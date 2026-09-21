#include "StemExport.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace QQDeBreathStemExport
{
namespace
{
constexpr int blockSize = 4096;
double gain(double db) { return std::pow(10.0, db / 20.0); }
struct Region
{
    juce::int64 start = 0, end = 0, first = 0, last = 0;
    bool before = false, after = false, noize = false;
    double peak = 0, localGain = 1, normGain = 1;
    double weight(juce::int64 sample, int fadeIn, int fadeOut) const
    {
        if (sample >= start && sample < end)
        {
            double w = 1.0;
            if (fadeIn > 0 && !before)
                w = juce::jmin(w, static_cast<double>(sample - start) / juce::jmax(1, fadeIn - 1));
            if (fadeOut > 0 && !after)
                w = juce::jmin(w, static_cast<double>(end - 1 - sample) / juce::jmax(1, fadeOut - 1));
            return juce::jlimit(0.0, 1.0, w);
        }
        if (before && sample >= start - fadeIn && sample < start)
            return juce::jlimit(0.0, 1.0, static_cast<double>(sample - (start - fadeIn)) / juce::jmax(1, fadeIn));
        if (after && sample >= end && sample < end + fadeOut)
            return juce::jlimit(0.0, 1.0, 1.0 - static_cast<double>(sample - end) / juce::jmax(1, fadeOut));
        return 0.0;
    }
};
bool applyEq(juce::AudioBuffer<float>& buffer, double rate, const QQDeBreathEqState& state, const Cancel& cancel)
{
    if (!state.hasActiveProcessing()) return !cancel();
    QQDeBreathEqProcessor eq;
    eq.prepare(rate, buffer.getNumChannels(), state);
    for (int offset = 0; offset < buffer.getNumSamples(); offset += blockSize)
    {
        if (cancel()) return false;
        juce::AudioBuffer<float> view(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), offset,
                                      juce::jmin(blockSize, buffer.getNumSamples() - offset));
        eq.process(view);
    }
    return !cancel();
}
}

bool render(const juce::AudioBuffer<float>& source, double rate,
            const juce::Array<QQDeBreathBridgeRegion>& regions, const Settings& settings,
            Stems& stems, const Cancel& cancel, const Progress& progress)
{
    const auto samples = source.getNumSamples(), channels = source.getNumChannels();
    if (cancel() || samples <= 0 || channels <= 0 || rate <= 0.0) return false;
    const auto fadeIn = settings.enableFade ? static_cast<int>(std::llround(settings.fadeInMs * rate / 1000.0)) : 0;
    const auto fadeOut = settings.enableFade ? static_cast<int>(std::llround(settings.fadeOutMs * rate / 1000.0)) : 0;
    const auto target = gain(settings.breathTargetDb);
    const auto adjust = gain(juce::jlimit(-60.0, 30.0, settings.breathGainDb));
    std::vector<Region> cached(static_cast<size_t>(regions.size()));
    for (int i = 0; i < regions.size(); ++i)
    {
        const auto& original = regions.getReference(i);
        auto& r = cached[static_cast<size_t>(i)];
        r.start = juce::jlimit<juce::int64>(0, samples, original.startSample > 0 ? original.startSample : static_cast<juce::int64>(std::llround(original.startTime * rate)));
        r.end = juce::jlimit<juce::int64>(0, samples, original.endSample > 0 ? original.endSample : static_cast<juce::int64>(std::llround(original.endTime * rate)));
        r.noize = original.type.equalsIgnoreCase("Noize");
        r.localGain = gain(juce::jlimit(-30.0, 30.0, original.gainDb));
    }
    // Adjacency is constant throughout this snapshot: compute it once, not per sample.
    for (size_t i = 0; i < cached.size(); ++i)
    {
        if (cancel()) return false;
        auto& r = cached[i];
        for (size_t j = 0; j < cached.size(); ++j)
        {
            if (j == i) continue;
            if (fadeIn > 0 && std::abs(cached[j].end - r.start) <= juce::jmax(1, fadeIn)) r.before = true;
            if (fadeOut > 0 && std::abs(cached[j].start - r.end) <= juce::jmax(1, fadeOut)) r.after = true;
        }
        r.first = juce::jmax<juce::int64>(0, juce::jmin(r.start, r.end) - (r.before ? fadeIn : 0));
        r.last = juce::jmin<juce::int64>(samples, juce::jmax(r.start, r.end) + (r.after ? fadeOut : 0));
        float peak = 0;
        if (!r.noize)
            for (int channel = 0; channel < channels; ++channel)
                for (auto offset = r.start; offset < r.end; offset += blockSize)
                {
                    if (cancel()) return false;
                    const auto end = juce::jmin<juce::int64>(r.end, offset + blockSize);
                    const auto* data = source.getReadPointer(channel);
                    for (auto sample = offset; sample < end; ++sample)
                        peak = juce::jmax(peak, std::abs(data[static_cast<int>(sample)]));
                }
        r.peak = peak;
        r.normGain = settings.normalizeBreath && peak > 1.0e-9 ? target / r.peak : 1.0;
    }
    for (auto* buffer : { &stems.vocal, &stems.breath, &stems.noize })
    {
        if (cancel()) return false;
        buffer->setSize(channels, samples);
    }
    std::array<double, blockSize> breathWeight, noizeWeight, normGain, localGain;
    for (int offset = 0; offset < samples; offset += blockSize)
    {
        if (cancel()) return false;
        const auto count = juce::jmin(blockSize, samples - offset);
        breathWeight.fill(0); noizeWeight.fill(0); normGain.fill(1); localGain.fill(1);
        // Preserve original region order, maximum weight and last-equal-weight priority.
        for (const auto& r : cached)
        {
            if (cancel()) return false;
            const auto first = juce::jmax<juce::int64>(offset, r.first);
            const auto last = juce::jmin<juce::int64>(offset + count, r.last);
            for (auto sample = first; sample < last; ++sample)
            {
                const auto w = r.weight(sample, fadeIn, fadeOut);
                if (w <= 0) continue;
                const auto index = static_cast<size_t>(sample - offset);
                if (r.noize) noizeWeight[index] = juce::jmax(noizeWeight[index], w);
                else if (w >= breathWeight[index])
                {
                    breathWeight[index] = w;
                    normGain[index] = r.normGain;
                    localGain[index] = r.localGain;
                }
            }
        }
        for (int i = 0; i < count; ++i)
        {
            const auto index = static_cast<size_t>(i);
            auto bw = breathWeight[index], nw = noizeWeight[index];
            const auto sum = bw + nw;
            if (sum > 1.0) { bw /= sum; nw /= sum; }
            const auto vw = juce::jlimit(0.0, 1.0, 1.0 - bw - nw);
            for (int channel = 0; channel < channels; ++channel)
            {
                const auto dry = static_cast<double>(source.getSample(channel, offset + i));
                stems.vocal.setSample(channel, offset + i, static_cast<float>(dry * vw));
                stems.breath.setSample(channel, offset + i, static_cast<float>(dry * bw * normGain[index] * adjust * localGain[index]));
                stems.noize.setSample(channel, offset + i, static_cast<float>(dry * nw));
            }
        }
        progress(0.1f + 0.5f * static_cast<float>(offset + count) / static_cast<float>(samples));
    }
    if (!applyEq(stems.breath, rate, settings.globalEq, cancel)) return false;
    // Retain 1.23's per-region EQ replacement order and complete fade window.
    for (int i = 0; i < regions.size(); ++i)
    {
        if (cancel()) return false;
        const auto& original = regions.getReference(i);
        const auto& r = cached[static_cast<size_t>(i)];
        if (r.noize || !original.eqState.hasActiveProcessing()) continue;
        const auto start = juce::jmax<juce::int64>(0, r.start - fadeIn);
        const auto end = juce::jmin<juce::int64>(samples, r.end + fadeOut);
        if (end <= start) continue;
        juce::AudioBuffer<float> contribution(channels, static_cast<int>(end - start));
        contribution.clear();
        for (auto offset = start; offset < end; offset += blockSize)
        {
            if (cancel()) return false;
            for (auto sample = offset; sample < juce::jmin<juce::int64>(end, offset + blockSize); ++sample)
            {
                const auto w = r.weight(sample, fadeIn, fadeOut);
                if (w <= 0) continue;
                for (int channel = 0; channel < channels; ++channel)
                {
                    const auto dry = static_cast<double>(source.getSample(channel, static_cast<int>(sample)));
                    contribution.setSample(channel, static_cast<int>(sample - start), static_cast<float>(dry * w * r.normGain * adjust * r.localGain));
                }
            }
        }
        if (!applyEq(contribution, rate, settings.globalEq, cancel)
            || !applyEq(contribution, rate, original.eqState, cancel)) return false;
        for (int channel = 0; channel < channels; ++channel)
        {
            stems.breath.clear(channel, static_cast<int>(start), contribution.getNumSamples());
            stems.breath.addFrom(channel, static_cast<int>(start), contribution, channel, 0, contribution.getNumSamples());
        }
    }
    progress(0.75f);
    return !cancel();
}

Job::Job(Request requestIn) : Thread("QQDeBreath Stem Export"), request(std::move(requestIn)) {}
Job::~Job()
{
    signalThreadShouldExit();
    // Cooperative cancellation; never kill a thread holding a writer or leave code running on unload.
    waitForThreadToExit(-1);
}
void Job::run()
{
    try { execute(); }
    catch (const std::exception& e) { status = "Export failed: " + juce::String(e.what()); }
    catch (...) { status = "Export failed unexpectedly."; }
    done.store(true, std::memory_order_release);
}
void Job::execute()
{
    const auto cancelled = [this]
    {
        if (!threadShouldExit()) return false;
        status = "Export cancelled. Existing stems were not replaced.";
        return true;
    };
    if (cancelled()) return;
    if (request.reader)
    {
        auto& reader = *request.reader;
        if (reader.lengthInSamples <= 0 || reader.lengthInSamples > std::numeric_limits<int>::max()
            || reader.numChannels == 0 || reader.sampleRate <= 0)
        { status = "Source wav length or format is unsupported for export."; return; }
        request.sampleRate = reader.sampleRate;
        request.source.setSize(static_cast<int>(reader.numChannels), static_cast<int>(reader.lengthInSamples));
        for (int offset = 0; offset < request.source.getNumSamples(); offset += blockSize)
        {
            if (cancelled()) return;
            const auto count = juce::jmin(blockSize, request.source.getNumSamples() - offset);
            if (!reader.read(&request.source, offset, count, offset, true, true))
            { status = "Failed reading source wav."; return; }
        }
        request.reader.reset();
    }
    if (request.source.getNumSamples() <= 0 || request.source.getNumChannels() <= 0 || request.sampleRate <= 0)
    { status = "Source audio is empty."; return; }
    Stems stems;
    if (!render(request.source, request.sampleRate, request.regions, request.settings, stems,
                cancelled, [this](float value) { progress.store(value, std::memory_order_relaxed); })) return;
    if (cancelled()) return;
    if (!request.directory.createDirectory()) { status = "Could not create export folder."; return; }
    const std::array<juce::String, 3> names { "Vocal Only.wav", "Breath.wav", "Noize.wav" };
    const std::array<const juce::AudioBuffer<float>*, 3> buffers { &stems.vocal, &stems.breath, &stems.noize };
    std::array<std::unique_ptr<juce::TemporaryFile>, 3> files;
    for (size_t i = 0; i < files.size(); ++i)
    {
        if (cancelled()) return;
        files[i] = std::make_unique<juce::TemporaryFile>(request.directory.getChildFile(names[i]));
        std::unique_ptr<juce::FileOutputStream> stream(files[i]->getFile().createOutputStream());
        if (!stream || stream->failedToOpen()) { status = "Could not open export file: " + names[i]; return; }
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(), request.sampleRate,
                     static_cast<unsigned int>(buffers[i]->getNumChannels()), 32, {}, 0));
        if (!writer) { status = "Could not create wav writer: " + names[i]; return; }
        auto* output = stream.release();
        const auto samples = buffers[i]->getNumSamples();
        for (int offset = 0; offset < samples; offset += blockSize)
        {
            if (cancelled()) return;
            const auto count = juce::jmin(blockSize, samples - offset);
            if (!writer->writeFromAudioSampleBuffer(*buffers[i], offset, count))
            { status = "Failed writing export file: " + names[i]; return; }
            progress.store(0.75f + 0.24f * (static_cast<float>(i) + static_cast<float>(offset + count) / static_cast<float>(samples)) / 3.0f,
                           std::memory_order_relaxed);
        }
        if (!writer->flush())
        { status = "Could not finalize wav header: " + names[i]; return; }
        output->flush();
        if (output->getStatus().failed())
        { status = "Could not flush export file: " + names[i]; return; }
        writer.reset();
    }
    if (cancelled()) return;
    // A short commit phase: cancellation applies before it, not halfway through replacing the set.
    for (size_t i = 0; i < files.size(); ++i)
        if (!files[i]->overwriteTargetFileWithTemporary())
        { status = "Could not replace " + names[i] + ". Some earlier stems may already have been replaced."; return; }
    succeeded = true;
    progress.store(1.0f, std::memory_order_relaxed);
    status = "Exported Vocal Only.wav, Breath.wav, and Noize.wav with captured Fade/Norm/Global Gain/EQ.";
}
}
