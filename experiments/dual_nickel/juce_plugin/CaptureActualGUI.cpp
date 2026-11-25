// Offscreen capture of the ACTUAL compiled Nickel Opto JUCE editor.
// No redraw, no mock-up, no recreation. Component's own paint() is used.
#include "Plugin.cpp"
#include <memory>
static bool capture(const juce::String& filename, int mode) {
    auto processor=std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    auto* nickel=dynamic_cast<NickelOptoProcessor*>(processor.get());
    if(nickel==nullptr)return false;
    if(auto* selected=nickel->state.getParameter("cell"))
        selected->setValueNotifyingHost(float(mode)/3.0f);
    auto editor=std::unique_ptr<juce::AudioProcessorEditor>(processor->createEditor());
    editor->setVisible(true);
    editor->resized();
    const int width=editor->getWidth(),height=editor->getHeight();
    juce::Image snapshot(juce::Image::ARGB,width,height,true);
    juce::Graphics g(snapshot);
    editor->paintEntireComponent(g,true);
    juce::File out=juce::File::getCurrentWorkingDirectory().getChildFile(filename);
    juce::FileOutputStream stream(out);
    if(!stream.openedOk())return false;
    juce::PNGImageFormat png;
    if(!png.writeImageToStream(snapshot,stream))return false;
    stream.flush();
    return out.existsAsFile();
}
int main() {
    juce::ScopedJuceInitialiser_GUI initialise;
    if(!capture("NickelOpto-Actual-GUI-NSL32.png",0))return 2;
    if(!capture("NickelOpto-Actual-GUI-VTL5C1.png",3))return 3;
    return 0;
}
