#pragma once

#include "BridgeAnalysis.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

// Immutable, source-local DSP metadata. Built on the model/editor thread, never per sample.
struct QQDeBreathARARegionPlan
{
    struct Region
    {
        juce::int64 start = 0, end = 0;
        bool before = false, after = false, noize = false;
        double gain = 1.0, norm = 1.0;

        double weight(juce::int64 sample, int fadeIn, int fadeOut) const noexcept
        {
            if (sample >= start && sample < end)
            {
                double value = 1.0;
                if (fadeIn > 0 && !before)
                    value = juce::jmin(value, static_cast<double>(sample - start) / juce::jmax(1, fadeIn - 1));
                if (fadeOut > 0 && !after)
                    value = juce::jmin(value, static_cast<double>(end - 1 - sample) / juce::jmax(1, fadeOut - 1));
                return juce::jlimit(0.0, 1.0, value);
            }
            if (before && sample >= start - fadeIn && sample < start)
                return juce::jlimit(0.0, 1.0, static_cast<double>(sample - (start - fadeIn)) / juce::jmax(1, fadeIn));
            if (after && sample >= end && sample < end + fadeOut)
                return juce::jlimit(0.0, 1.0, 1.0 - static_cast<double>(sample - end) / juce::jmax(1, fadeOut));
            return 0.0;
        }
    };

    std::vector<Region> regions;
    int fadeIn = 0, fadeOut = 0;
    double breathGain = 1.0;

    template <typename Params>
    static std::shared_ptr<const QQDeBreathARARegionPlan> build(
        const juce::Array<QQDeBreathBridgeRegion>& source, const juce::Array<double>& peaks,
        double rate, juce::int64 samples, const Params& params)
    {
        auto plan = std::make_shared<QQDeBreathARARegionPlan>();
        if (rate <= 0.0 || samples <= 0) return plan;
        plan->fadeIn = params.enableFade ? static_cast<int>(std::llround(params.fadeInMs * rate / 1000.0)) : 0;
        plan->fadeOut = params.enableFade ? static_cast<int>(std::llround(params.fadeOutMs * rate / 1000.0)) : 0;
        plan->breathGain = std::pow(10.0, juce::jlimit(-60.0, 30.0, params.breathGainDb) / 20.0);
        const auto target = std::pow(10.0, params.breathTargetDb / 20.0);
        std::vector<std::pair<juce::int64, int>> starts, ends;
        plan->regions.reserve(static_cast<size_t>(source.size()));
        starts.reserve(static_cast<size_t>(source.size())); ends.reserve(static_cast<size_t>(source.size()));
        for (int i = 0; i < source.size(); ++i)
        {
            const auto& original = source.getReference(i);
            Region r;
            r.start = juce::jlimit<juce::int64>(0, samples, original.startSample > 0 ? original.startSample : static_cast<juce::int64>(std::llround(original.startTime * rate)));
            r.end = juce::jlimit<juce::int64>(0, samples, original.endSample > 0 ? original.endSample : static_cast<juce::int64>(std::llround(original.endTime * rate)));
            r.noize = original.type.equalsIgnoreCase("Noize");
            r.gain = std::pow(10.0, juce::jlimit(-30.0, 30.0, original.gainDb) / 20.0);
            const auto peak = i < peaks.size() ? peaks.getReference(i) : 0.0;
            r.norm = params.normalizeBreath && peak > 1.0e-9 ? target / peak : 1.0;
            plan->regions.push_back(r); starts.emplace_back(r.start, i); ends.emplace_back(r.end, i);
        }
        std::sort(starts.begin(), starts.end()); std::sort(ends.begin(), ends.end());
        const auto adjacent = [](const auto& bounds, juce::int64 point, int fade, int self)
        {
            if (fade <= 0) return false;
            const auto tolerance = static_cast<juce::int64>(juce::jmax(1, fade));
            auto it = std::lower_bound(bounds.begin(), bounds.end(), point - tolerance,
                                      [](const auto& bound, auto value) { return bound.first < value; });
            for (; it != bounds.end() && it->first <= point + tolerance; ++it)
                if (it->second != self) return true;
            return false;
        };
        for (int i = 0; i < source.size(); ++i)
        {
            auto& r = plan->regions[static_cast<size_t>(i)];
            r.before = adjacent(ends, r.start, plan->fadeIn, i);
            r.after = adjacent(starts, r.end, plan->fadeOut, i);
        }
        return plan;
    }
};
