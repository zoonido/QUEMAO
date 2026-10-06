// Local-only tool: renders the QUEMAO editor to a PNG (not part of the GitHub build).
#include "plugin_processor.h"
#include "plugin_editor.h"
#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    QuemaoProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    proc.applyFactoryPreset (argc > 2 ? juce::String (argv[2]).getIntValue() : 8);
    proc.modTarget.store (6);
    for (int i = 0; i < 4; ++i) { proc.lanes[i].uiStatus.store (quemao::statusLatched); proc.lanes[i].uiStep.store (4); }
    proc.lanes[1].locks[0].store (true);
    proc.modSeq.uiStep.store (4);
    proc.uiBpm.store (124.0);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (1200, 982);   // full size for the picture
    ed->setVisible (true);
    if (auto* q = dynamic_cast<QuemaoEditor*> (ed.get())) q->refreshNow();
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::File out (argc > 1 ? argv[1] : "window.png");
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
