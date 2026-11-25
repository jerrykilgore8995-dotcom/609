// Offscreen capture of the ACTUAL compiled Nickel Opto JUCE editor.
// No redraw, no mock-up, no recreation. Component's own paint() is used.
#include "Plugin.cpp"
#include <memory>
// A JUCE Component without a native desktop peer may report not showing
// and paintEntireComponent() skips it. Render each actual Component::paint
// implementation recursively instead; never synthesize a UI image.
static void paintCompiledComponentTree(juce::Component& comp,juce::Graphics& g) {
    comp.paint(g);
    for(int i=0;i<comp.getNumChildComponents();++i) {
        auto* child=comp.getChildComponent(i);
        if(child==nullptr || !child->isVisible()) continue;
        juce::Graphics::ScopedSaveState scoped(g);
        g.addTransform(juce::AffineTransform::translation(
            float(child->getX()),float(child->getY())));
        g.reduceClipRegion(0,0,child->getWidth(),child->getHeight());
        paintCompiledComponentTree(*child,g);
    }
}
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
    paintCompiledComponentTree(*editor,g);
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
