#pragma once

#include <juce_dsp/juce_dsp.h>
#include <memory>

inline std::unique_ptr<juce::dsp::Oversampling<float>>
makeOptoOversampler(std::size_t channels, std::size_t maxBlockSize) {
    using Oversampler = juce::dsp::Oversampling<float>;
    auto filter = std::make_unique<Oversampler>(channels);
    filter->clearOversamplingStages();
    // Linear phase keeps the return filter from shifting upper-band phase.
    // The narrow half-band transition leaves 20 kHz in band at 44.1 kHz.
    filter->addOversamplingStage(Oversampler::filterHalfBandFIREquiripple,
                                 .04f, -105.f, .04f, -105.f);
    // At the second stage the passband of interest is farther below Nyquist.
    filter->addOversamplingStage(Oversampler::filterHalfBandFIREquiripple,
                                 .15f, -105.f, .15f, -105.f);
    filter->setUsingIntegerLatency(true);
    filter->initProcessing(maxBlockSize);
    return filter;
}
