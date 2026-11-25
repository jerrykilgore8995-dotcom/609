#pragma once

#include <juce_dsp/juce_dsp.h>
#include <memory>

inline std::unique_ptr<juce::dsp::Oversampling<float>>
makeOptoOversampler(std::size_t channels, std::size_t maxBlockSize) {
    using Oversampler = juce::dsp::Oversampling<float>;
    auto filter = std::make_unique<Oversampler>(channels);
    filter->clearOversamplingStages();
    // Polyphase allpass half-band filters keep the host delay low.
    // The first stage's narrow transition leaves 20 kHz in band at 44.1 kHz.
    filter->addOversamplingStage(Oversampler::filterHalfBandPolyphaseIIR,
                                 .04f, -105.f, .04f, -105.f);
    // At the second stage the passband of interest is farther below Nyquist.
    filter->addOversamplingStage(Oversampler::filterHalfBandPolyphaseIIR,
                                 .15f, -105.f, .15f, -105.f);
    filter->setUsingIntegerLatency(true);
    filter->initProcessing(maxBlockSize);
    return filter;
}
