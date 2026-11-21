#include <juce_audio_processors/juce_audio_processors.h>
#include "Render.hpp"

namespace api550a {

constexpr std::array ids{"lg", "mg", "hg", "lf", "mf", "hf", "ls", "hs", "bp", "eq"};
constexpr std::array names{"LF Gain", "MF Gain", "HF Gain", "LF Frequency",
                           "MF Frequency", "HF Frequency", "LF Shelf",
                           "HF Shelf", "Filter", "EQ In"};
const std::array<juce::StringArray, 4> choices{{
    {"−12", "−9", "−6", "−4", "−2", "0", "+2", "+4", "+6", "+9", "+12"},
    {"50 Hz", "100 Hz", "200 Hz", "300 Hz", "400 Hz"},
    {"400 Hz", "800 Hz", "1.5 kHz", "3 kHz", "5 kHz"},
    {"5 kHz", "7 kHz", "10 kHz", "12.5 kHz", "15 kHz"}
}};

class Processor final : public juce::AudioProcessor {
public:
    Processor()
        : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          state(*this, nullptr, "Settings", makeParameters()) {
        for (std::size_t i = 0; i < ids.size(); ++i)
            parameters[i] = state.getRawParameterValue(ids[i]);
    }
    const juce::String getName() const override { return "550A Nodal"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    bool isBusesLayoutSupported(const BusesLayout& layout) const override {
        auto set = layout.getMainInputChannelSet();
        return set == layout.getMainOutputChannelSet()
               && (set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo());
    }
    void prepareToPlay(double sampleRate, int) override {
        ready = render.prepare(sampleRate, getTotalNumInputChannels());
        setLatencySamples(ready ? render.latency() : 0);
    }
    void releaseResources() override { ready = false; }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        juce::ScopedNoDenormals noDenormals;
        if (!ready) return;
        auto controls = mkii::native::defaults;
        for (std::size_t i = 0; i < ids.size(); ++i) {
            const int offset = i < 3 ? -5 : (i < 6 ? 1 : 0);
            controls[i] = static_cast<int>(parameters[i]->load(std::memory_order_relaxed)) + offset;
        }
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getNumChannels() == 2 ? buffer.getWritePointer(1) : nullptr;
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            const auto frame = render.step({left[i], right ? right[i] : 0.0}, controls);
            left[i] = static_cast<float>(frame[0]);
            if (right) right[i] = static_cast<float>(frame[1]);
        }
    }
    void getStateInformation(juce::MemoryBlock& data) override {
        if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, data);
    }
    void setStateInformation(const void* data, int size) override {
        if (auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName(state.state.getType()))
            state.replaceState(juce::ValueTree::fromXml(*xml));
    }

    juce::AudioProcessorValueTreeState state;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters() {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (std::size_t i = 0; i < ids.size(); ++i) {
            const juce::ParameterID id{ids[i], 1};
            if (i < 6)
                layout.add(std::make_unique<juce::AudioParameterChoice>(
                    id, names[i], choices[i < 3 ? 0 : i - 2], i < 3 ? 5 : 0));
            else
                layout.add(std::make_unique<juce::AudioParameterBool>(id, names[i], i == 9));
        }
        return layout;
    }

    std::array<std::atomic<float>*, ids.size()> parameters{};
    Render render;
    bool ready = false;
};

class Panel final : public juce::AudioProcessorEditor {
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    class KnobStyle final : public juce::LookAndFeel_V4 {
    public:
        void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                              float position, float, float, juce::Slider& slider) override {
            const bool inner = slider.getComponentID() == "frequency";
            const auto cx = static_cast<float>(x) + static_cast<float>(w) * .5f;
            const auto cy = static_cast<float>(y) + static_cast<float>(h) * .56f;
            const float radius = static_cast<float>(w) * (inner ? .37f : .38f);
            const float angle = inner ? 4.01f - 1.74f*position : -2.72f + 5.44f*position;
            juce::Path pointer;
            pointer.startNewSubPath(-radius * .15f, -radius * 1.52f);
            pointer.lineTo(radius * .15f, -radius * 1.52f);
            pointer.lineTo(radius * .35f, -radius * .84f);
            pointer.lineTo(-radius * .35f, -radius * .84f);
            pointer.closeSubPath();
            pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(cx, cy));
            const auto paint = inner ? juce::Colour(0xff1487b7) : juce::Colour(0xffb8b4ab);
            g.setColour(paint);
            g.fillPath(pointer);
            g.fillEllipse(cx-radius, cy-radius, 2*radius, 2*radius);
            g.setGradientFill({paint.brighter(.35f), cx-radius*.6f, cy-radius, paint.darker(.55f),
                               cx+radius*.65f, cy+radius, false});
            g.fillEllipse(cx-radius+1, cy-radius+1, 2*radius-2, 2*radius-2);
            if (inner) {
                g.setColour(juce::Colour(0xffa1a0a0));
                g.fillEllipse(cx-radius*.80f, cy-radius*.80f, 1.6f*radius, 1.6f*radius);
                g.setGradientFill({juce::Colour(0xffd0cdca), cx-7, cy-15,
                                  juce::Colour(0xff96928a), cx+12, cy+18, false});
                g.fillEllipse(cx-radius*.76f, cy-radius*.76f, 1.52f*radius, 1.52f*radius);
            }
        }
        void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                              bool highlight, bool down) override {
            const auto bounds = button.getLocalBounds().toFloat().reduced(1);
            const bool toggle = button.getComponentID() == "filter";
            if (toggle) {
                g.setColour(juce::Colour(0xffb8aba0));
                g.fillEllipse(bounds);
                g.setColour(juce::Colour(0xff2f2e2c));
                g.fillEllipse(bounds.reduced(5));
                auto lever = bounds.withSizeKeepingCentre(12, 19)
                               .translated(-6, button.getToggleState() ? -7.f : 7.f);
                g.setGradientFill({juce::Colour(0xfff7f3e8), lever.getTopLeft(),
                                   juce::Colour(0xff888983), lever.getBottomRight(), true});
                g.fillEllipse(lever);
            } else {
                g.setColour(juce::Colour(0xff2c2520));
                g.fillEllipse(bounds);
                g.setGradientFill({juce::Colour(0xffffe9bc), bounds.getTopLeft(),
                                   juce::Colour(0xffc59b69), bounds.getBottomRight(), false});
                g.fillEllipse(bounds.reduced((down || button.getToggleState()) ? 4.f : 2.f));
                if (highlight) {
                    g.setColour(juce::Colours::white.withAlpha(.2f));
                    g.drawEllipse(bounds.reduced(2), 1);
                }
            }
        }
    };

public:
    explicit Panel(Processor& owner) : AudioProcessorEditor(owner) {
        constexpr float pi = juce::MathConstants<float>::pi;
        for (int i = 0; i < 6; ++i) {
            auto& knob = knobs[static_cast<std::size_t>(i)];
            knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            knob.setLookAndFeel(&style);
            knob.setComponentID(i < 3 ? "gain" : "frequency");
            knob.setTitle(names[static_cast<std::size_t>(i)]);
            knob.setTooltip(names[static_cast<std::size_t>(i)]);
            if (i < 3) knob.setRotaryParameters(pi - 2.48f, pi + 2.48f, true);
            else knob.setRotaryParameters(pi + 1.94f, 3*pi - 1.94f, true);
            addAndMakeVisible(knob);
            sliders[static_cast<std::size_t>(i)] =
                std::make_unique<SliderAttachment>(owner.state, ids[static_cast<std::size_t>(i)], knob);
        }
        for (std::size_t i = 0; i < buttons.size(); ++i) {
            auto& button = buttons[i];
            button.setLookAndFeel(&style);
            button.setComponentID(i == 2 ? "filter" : "push");
            button.setTitle(names[i+6]);
            button.setTooltip(names[i+6]);
            addAndMakeVisible(button);
            switches[i] = std::make_unique<ButtonAttachment>(owner.state, ids[i+6], button);
        }
        buttons.back().onStateChange = [this] { repaint(143, 579, 32, 32); };
        setSize(207, 736);
    }
    ~Panel() override {
        for (auto& knob : knobs) knob.setLookAndFeel(nullptr);
        for (auto& button : buttons) button.setLookAndFeel(nullptr);
    }

    void resized() override {
        constexpr std::array rows{127, 303, 480};
        for (std::size_t band = 0; band < rows.size(); ++band) {
            const int r = rows[band];
            knobs[2-band].setBounds(51, r-60, 104, 107);
            knobs[5-band].setBounds(67, r-35, 72, 60);
        }
        buttons[0].setBounds(26, 628, 44, 44);
        buttons[1].setBounds(81, 628, 44, 44);
        buttons[2].setBounds(21, 569, 53, 53);
        buttons[3].setBounds(137, 628, 44, 44);
    }

    void paint(juce::Graphics& g) override {
        using namespace juce;
        g.setGradientFill({Colour(0xff1b1b1d), 0, 90, Colour(0xff0c0d10), 207, 700, false});
        g.fillAll();
        g.setColour(Colour(0xff3d3c3c));
        g.drawRect(getLocalBounds(), 1);
        const Colour blue(0xff24a7e0), ink(0xfff3edda);
        auto print = [&](const String& value, int x, int y, int w, Colour colour, float height = 16.f) {
            g.setColour(colour);
            g.setFont(FontOptions(height, Font::bold));
            g.drawText(value, x, y, w, 19, Justification::centred);
        };
        auto screw = [&](float y) {
            g.setColour(Colour(0xffa19a88)); g.drawEllipse(90, y-12, 24, 24, 2);
            g.setColour(Colour(0xff3e3732)); g.fillEllipse(94, y-8, 16, 16);
            g.setColour(Colour(0xff262023)); g.drawLine(96, y+4, 109, y-2, 2);
        };
        screw(21);
        screw(715);
        print("550A", 119, 22, 79, blue, 19);
        constexpr std::array base{127, 303, 480};
        constexpr std::array gainX{75, 50, 42, 51, 75};
        constexpr std::array gainY{-36, -13, 16, 46, 68};
        constexpr std::array gain{"2", "4", "6", "9", "12"};
        constexpr std::array labels{"HF", "MF", "LF"};
        constexpr std::array<std::array<const char*,5>,3> freq{{
            {"5","7","10","12.5","15"}, {".4",".8","1.5","3","5"}, {"50","100","200","300","400"}
        }};
        for (std::size_t row = 0; row < base.size(); ++row) {
            const int y = base[row];
            print("−", 20, y-59, 30, ink, 19);
            print("+", 157, y-59, 30, ink, 19);
            print("0", 93, y-58, 20, ink);
            for (std::size_t i=0; i<gain.size(); ++i) {
                print(gain[i], gainX[i]-14, y+gainY[i]-10, 28, ink, 18);
                print(gain[i], 206-gainX[i]-14, y+gainY[i]-10, 28, ink, 18);
            }
            print(freq[row][0], 4, y+53, 35, blue);
            print(freq[row][4], 164, y+53, 38, blue);
            for (int i=1; i<=3; ++i)
                print(freq[row][static_cast<std::size_t>(i)], 38+(i-1)*45, y+84, 52, blue);
            print(labels[row], 1, y+84, 30, ink);
            print(row == 2 ? "Hz" : "kHz", 174, y+84, 30, blue, 14);
        }
        print("FLTR", 76, 582, 46, ink);
        print("L⌣F", 17, 681, 60, ink);
        print("H⌢F", 73, 681, 60, ink);
        print("IN", 137, 681, 44, ink);
        g.setColour(buttons.back().getToggleState() ? Colour(0xfff14c41) : Colour(0xff451516));
        g.fillEllipse(152, 585, 16, 16);
        g.setColour(Colour(0xff9e5d59));
        g.fillEllipse(156, 588, 4, 4);
    }
private:
    KnobStyle style;
    std::array<juce::Slider, 6> knobs;
    std::array<juce::ToggleButton, 4> buttons;
    std::array<std::unique_ptr<SliderAttachment>, 6> sliders;
    std::array<std::unique_ptr<ButtonAttachment>, 4> switches;
};

juce::AudioProcessorEditor* Processor::createEditor() { return new Panel(*this); }

}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new api550a::Processor; }
