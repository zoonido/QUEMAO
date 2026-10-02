// Local-only tool: renders the QUEMAO editor to a PNG (not part of the GitHub build).
#include "plugin_processor.h"
#include "plugin_editor.h"
#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    QuemaoProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    // show the lanes mid-groove
    if (auto* l = proc.apvts.getParameter ("latch1")) l->setValueNotifyingHost (1.0f);
    if (auto* l = proc.apvts.getParameter ("latch2")) l->setValueNotifyingHost (1.0f);
    proc.lanes[0].uiStatus.store (quemao::statusLatched); proc.lanes[0].uiStep.store (6);
    proc.lanes[1].uiStatus.store (quemao::statusLatched); proc.lanes[1].uiStep.store (6);
    proc.lanes[2].uiStatus.store (quemao::statusHeld);    proc.lanes[2].uiStep.store (6);
    proc.lanes[3].uiStatus.store (quemao::statusTrigger);
    proc.uiBpm.store (124.0);
    auto setP = [&] (const char* id, float v) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    setP ("eng1", 4.0f); setP ("artic1", 0.35f);   // MADERA
    setP ("eng3", 2.0f); setP ("artic3", 0.5f);    // CAJA
    setP ("eng4", 3.0f); setP ("artic4", 0.9f);    // ANALOGO clap
    setP ("cutoff1", 0.62f); setP ("res1", 0.35f); setP ("drive1", 0.3f);
    setP ("fmode3", 1.0f); setP ("cutoff3", 0.8f); setP ("tone3", 0.7f);
    setP ("choke2", 1.0f); setP ("choke4", 1.0f); setP ("penv4", 0.2f); setP ("attack2", 0.15f);
    setP ("genre1", 6.0f); setP ("genre2", 1.0f); setP ("genre3", 2.0f); setP ("genre4", 7.0f);
    setP ("swing2", 0.3f); setP ("human2", 0.4f); setP ("dens2", 0.65f); setP ("dens4", 0.4f);
    for (int i = 0; i < 4; ++i) { proc.seeds[i].store ((uint32_t) (11 + i * 7)); proc.generate (i, false); }
    proc.lanes[1].locks[0].store (true); proc.lanes[1].locks[14].store (true);
    { const float art[16] = { -0.8f, 0.6f, 0.0f, 0.3f, 1.0f, -0.4f, -1.0f, 0.5f, -0.8f, 0.7f, 0.0f, 0.25f, 1.0f, -0.5f, -0.9f, 0.6f };
      for (int i = 0; i < 16; ++i) proc.modSeq.values[22][i].store (art[i]);
      const float tn[16] = { 0, 0, 0.125f, 0, 0, 0.21f, 0, 0, 0.29f, 0, 0.21f, 0, 0.125f, 0, 0, -0.08f };
      for (int i = 0; i < 16; ++i) proc.modSeq.values[0][i].store (tn[i]); }
    proc.modSeq.uiStep.store (4);
    setP ("sendd2", 0.45f); setP ("dtime2", 6.0f); setP ("sendv3", 0.5f); setP ("sendc4", 0.35f); setP ("comp", 0.35f);
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
