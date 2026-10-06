#include "plugin_editor.h"

using namespace quemao;

namespace
{
    juce::Font monoFont (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }
    juce::Font displayFont (float h)
    {
        return juce::Font (juce::FontOptions (h, juce::Font::bold)).withExtraKerningFactor (0.02f);
    }

    // three rows of five; row 3 leaves room for the filter-mode and choke buttons
    const char* kKnobIds[13]    = { "tune", "fine", "penv", "pdec", "attack",
                                    "decay", "artic", "tone", "drive", "level",
                                    "cutoff", "res", "pan" };
    const char* kKnobLabels[13] = { "TUNE", "FINE", "P.ENV", "P.DEC", "ATTACK",
                                    "DECAY", "ARTIC", "TONE", "DRIVE", "LEVEL",
                                    "CUTOFF", "RES", "PAN" };
    constexpr int kArticKnob = 6;
    juce::String percent (double v) { return juce::String ((int) std::round (v * 100.0)) + "%"; }

    // stage 7: hover help. Each control carries its sentence; the bar at the bottom shows it.
    void help (juce::Component& c, const char* text) { c.getProperties().set ("help", juce::String::fromUTF8 (text)); }

    const char* engineHelp (int e)
    {
        static const char* t[6] = {
            "MEMBRANA: drumhead model for toms, floor toms and frame drums. EDGE moves the strike from centre to rim.",
            "MANO: hand drum for conga, bongo, tambor alegre and llamador. ARTIC goes muted > open > slap.",
            "CAJA: snare, body + wires. STROKE goes cross-stick > hit > rimshot; DECAY sets the wires.",
            "ANÁLOGO: drum-machine circuits. TYPE picks TOM, SNARE, RIM or CLAP.",
            "MADERA: wood shell + plucked knock for cajón, tambora shell and wood block. RIM moves shell > rim.",
            "FM: two-operator FM toms. METAL goes from round to metallic." };
        return t[juce::jlimit (0, 5, e)];
    }
    const char* knobHelp (int k)
    {
        static const char* t[13] = {
            "TUNE: pitch in semitones (±24). The mod sequencer can walk it as a melody.",
            "FINE: fine pitch in cents (±50).",
            "P.ENV: depth of the pitch drop at the strike. Centre = the engine's own, left = none, right = deep laser-tom sweep.",
            "P.DEC: speed of the pitch drop. Left = 4x faster, right = 4x slower.",
            "ATTACK: fades in the strike (0-30 ms) to take the click off.",
            "DECAY: how long the drum rings. On CAJA it also sets the wires.",
            "",
            "TONE: tilts the lane dark <> bright. Centre is flat.",
            "DRIVE: saturates the lane for grit and weight.",
            "LEVEL: the lane's volume. Velocity and ghost notes scale from here.",
            "CUTOFF: filter cutoff. Fully right is open in both LP and HP.",
            "RES: filter resonance, a whistle at the cutoff.",
            "PAN: places the lane left <> right; its echoes follow." };
        return t[juce::jlimit (0, 12, k)];
    }
    const char* articHelp (int e)
    {
        static const char* t[6] = {
            "EDGE: where the stick hits the head. Left = centre (deep, round), right = rim (bright, ringing).",
            "ARTIC: the hand stroke. Left = muted (palm), centre = open tone, right = slap.",
            "STROKE: left = cross-stick, centre = normal hit, right = rimshot.",
            "TYPE: four circuits across the knob: TOM, SNARE, RIM, CLAP. Within each zone it tweaks the circuit.",
            "RIM: left = low shell knock, right = bright wood-block rim.",
            "METAL: the FM ratio and brightness. Left = round tom, right = metallic, bell-like." };
        return t[juce::jlimit (0, 5, e)];
    }
    const char* kNoteNames[4]  = { "C1", "D1", "E1", "F1" };
}

// ---------------------------------------------------------------- look

QuemaoLook::QuemaoLook()
{
    setColour (juce::Slider::textBoxTextColourId, ui::text);
    setColour (juce::Label::textColourId, ui::muted);
    setColour (juce::BubbleComponent::backgroundColourId, ui::well);
    setColour (juce::BubbleComponent::outlineColourId, ui::wellEdge);
    setColour (juce::TooltipWindow::textColourId, ui::text);
    setColour (juce::ComboBox::backgroundColourId, ui::well);
    setColour (juce::ComboBox::outlineColourId, ui::wellEdge);
    setColour (juce::ComboBox::textColourId, ui::text);
    setColour (juce::ComboBox::arrowColourId, ui::muted);
    setColour (juce::PopupMenu::backgroundColourId, ui::panel);
    setColour (juce::PopupMenu::textColourId, ui::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::ember);
    setColour (juce::PopupMenu::highlightedTextColourId, ui::ground);
}

void QuemaoLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                   float startAngle, float endAngle, juce::Slider& s)
{
    const auto colour = s.getProperties().getWithDefault ("colour", (int) ui::ember.getARGB());
    const juce::Colour c ((juce::uint32) (int) colour);
    const float size = (float) juce::jmin (w, h) - (juce::jmin (w, h) >= 40 ? 12.0f : 6.0f);
    const auto centre = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const auto r = juce::Rectangle<float> (size, size).withCentre (centre);
    const float angle = startAngle + pos * (endAngle - startAngle);

    if ((bool) s.getProperties().getWithDefault ("modActive", false))
    {
        // this knob is moved by the mod sequencer
        g.setColour (ui::text);
        g.fillEllipse ((float) x + (float) w - 7.0f, (float) y + 1.0f, 5.0f, 5.0f);
    }

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, size * 0.5f + 2.0f, size * 0.5f + 2.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (ui::wellEdge);
    g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, size * 0.5f + 2.0f, size * 0.5f + 2.0f, 0.0f, startAngle, angle, true);
    g.setColour (c.withAlpha (0.85f));
    g.strokePath (value, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (ui::well);
    g.fillEllipse (r.reduced (2.0f));
    g.setColour (ui::wellEdge);
    g.drawEllipse (r.reduced (2.0f), 1.0f);

    const float len = size * 0.5f - 6.0f;
    juce::Line<float> pointer (centre, centre.getPointOnCircumference (len, angle));
    g.setColour (c);
    g.drawLine (pointer.withShortenedStart (len * 0.25f), 2.2f);
}

void QuemaoLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    const auto c = juce::Colour ((juce::uint32) (int) b.getProperties().getWithDefault ("colour", (int) ui::ember.getARGB()));
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour (on ? c : (over ? ui::well.brighter (0.08f) : ui::well));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (on ? c : ui::wellEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void QuemaoLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? ui::ground : (b.isEnabled() ? ui::muted : ui::muted.withAlpha (0.35f)));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

juce::Font QuemaoLook::getComboBoxFont (juce::ComboBox&) { return monoFont (10.5f); }
juce::Font QuemaoLook::getPopupMenuFont() { return monoFont (12.0f); }
void QuemaoLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font QuemaoLook::getTextButtonFont (juce::TextButton& b, int)
{
    return monoFont (10.5f, b.getToggleState());
}

// ---------------------------------------------------------------- step grid

void StepGrid::paint (juce::Graphics& g)
{
    const float gap = 2.0f;
    const float cw = ((float) getWidth() - gap * 15.0f) / 16.0f;
    const int playing = lane.uiStep.load();

    for (int i = 0; i < 16; ++i)
    {
        juce::Rectangle<float> cell (i * (cw + gap), 0.0f, cw, (float) getHeight() - 6.0f);
        if (lane.locks[i].load())
        {
            g.setColour (ui::text);
            g.fillRoundedRectangle (cell.getX() + 2.0f, (float) getHeight() - 3.0f, cw - 4.0f, 3.0f, 1.5f);
        }
        const auto st = lane.steps[i].load();

        if (st == stepOff)          g.setColour (i % 4 == 0 ? ui::wellEdge : ui::well);
        else if (st == stepGhost)   g.setColour (colour.withAlpha (0.38f));
        else                        g.setColour (colour);
        g.fillRoundedRectangle (cell, 2.0f);

        if (st == stepRatchet)
        {
            g.setColour (ui::ground);
            g.setFont (monoFont (10.0f, true));
            g.drawText ("2", cell, juce::Justification::centred);
        }
        if (i == playing)
        {
            g.setColour (ui::text);
            g.drawRoundedRectangle (cell.reduced (0.5f), 2.0f, 1.5f);
        }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int i = stepAt (e.position.x);

    if (e.mods.isAltDown())          // option-click: lock / unlock the step
    {
        lane.locks[i].store (! lane.locks[i].load());
        repaint();
        return;
    }

    if (e.mods.isPopupMenu())        // right-click: step menu
    {
        const auto st = lane.steps[i].load();
        juce::PopupMenu m;
        m.addItem (1, "Hit",     true, st == stepHit);
        m.addItem (2, "Ghost",   true, st == stepGhost);
        m.addItem (3, "Ratchet", true, st == stepRatchet);
        m.addItem (4, "Off",     true, st == stepOff);
        m.addSeparator();
        m.addItem (5, "Lock step (GENERATE keeps it)", true, lane.locks[i].load());
        juce::Component::SafePointer<StepGrid> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, i] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& l = safe->lane;
            if (r == 1) l.steps[i].store (stepHit);
            else if (r == 2) l.steps[i].store (stepGhost);
            else if (r == 3) l.steps[i].store (stepRatchet);
            else if (r == 4) l.steps[i].store (stepOff);
            else if (r == 5) l.locks[i].store (! l.locks[i].load());
            safe->repaint();
        });
        return;
    }

    // click: off -> hit -> ghost -> ratchet -> off
    const auto st = lane.steps[i].load();
    lane.steps[i].store ((uint8_t) ((st + 1) % 4));
    repaint();
}

// ---------------------------------------------------------------- lane strip

LaneStrip::LaneStrip (QuemaoProcessor& p, int i)
    : proc (p), index (i), colour (ui::laneColours[i]), grid (p.lanes[i], ui::laneColours[i])
{
    for (int e = 0; e < kNumEngines; ++e)
    {
        auto& b = engines[e];
        b.setButtonText (e == (int) Engine::analogo ? juce::String::fromUTF8 ("ANÁLOGO") : juce::String (engineName ((Engine) e)));
        b.getProperties().set ("colour", (int) colour.getARGB());
        b.onClick = [this, e] { setChoice ("eng", e); };
        addAndMakeVisible (b);
    }
    lp.onClick = [this] { setChoice ("fmode", 0); };
    hp.onClick = [this] { setChoice ("fmode", 1); };
    const char* chokeNames[3] = { "-", "A", "B" };
    for (int c = 0; c < 3; ++c)
    {
        chokeButtons[c].setButtonText (chokeNames[c]);
        chokeButtons[c].onClick = [this, c] { setChoice ("choke", c); };
        chokeButtons[c].getProperties().set ("colour", (int) colour.getARGB());
        addAndMakeVisible (chokeButtons[c]);
    }
    for (auto* b : { &launch, &trigger, &latch, &lp, &hp })
    {
        b->getProperties().set ("colour", (int) colour.getARGB());
        addAndMakeVisible (b);
    }
    launch.onClick   = [this] { setChoice ("mode", 0); };
    trigger.onClick  = [this] { setChoice ("mode", 1); };

    latch.setClickingTogglesState (true);
    latchAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, QuemaoProcessor::pid ("latch", index), latch);

    for (int k = 0; k < kNumKnobs; ++k)
    {
        auto& s = knobs[k];
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setPopupDisplayEnabled (true, true, nullptr);
        s.getProperties().set ("colour", (int) colour.getARGB());
        addAndMakeVisible (s);
        knobAttach[k] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, QuemaoProcessor::pid (kKnobIds[k], index), s);
        if (auto* param = proc.apvts.getParameter (QuemaoProcessor::pid (kKnobIds[k], index)))
            s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

        auto& l = knobLabels[k];
        l.setText (kKnobLabels[k], juce::dontSendNotification);
        l.setFont (monoFont (9.5f));
        l.setJustificationType (juce::Justification::centred);
        l.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (l);
    }
    knobs[0].textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + juce::String ((int) v) + " st"; };
    knobs[1].textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " ct"; };
    knobs[2].textFromValueFunction = [] (double v) { return v < 0.005 ? juce::String ("OFF") : "x" + juce::String (pitchEnvFactor ((float) v), 2); };
    knobs[3].textFromValueFunction = [] (double v) { return "x" + juce::String (pitchDecayFactor ((float) v), 2); };
    knobs[4].textFromValueFunction = [] (double v) { return v < 0.005 ? juce::String ("SNAP") : juce::String (attackSeconds ((float) v) * 1000.0f, 1) + " ms"; };
    for (int k : { 5, 8, 9, 11 }) knobs[k].textFromValueFunction = [] (double v) { return percent (v); };
    knobs[7].textFromValueFunction = [] (double v) { return std::abs (v - 0.5) < 0.02 ? juce::String ("FLAT") : (v < 0.5 ? "DARK " : "BRIGHT ") + percent (std::abs (v - 0.5) * 2.0); };
    knobs[10].textFromValueFunction = [this] (double v)
    {
        const float hz = cutoffHz ((float) v, choice ("fmode") == 1);
        return (choice ("fmode") == 1 ? juce::String ("HP ") : juce::String ("LP "))
             + (hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String ((int) hz) + " Hz");
    };
    knobs[12].textFromValueFunction = [] (double v)
    {
        if (std::abs (v) < 0.02) return juce::String ("C");
        return (v < 0 ? juce::String ("L ") : juce::String ("R ")) + juce::String ((int) std::round (std::abs (v) * 100.0));
    };
    knobs[kArticKnob].textFromValueFunction = [this] (double v)
    {
        const auto eng = (Engine) choice ("eng");
        if (eng == Engine::analogo) return juce::String (analogTypeName (analogType ((float) v)));
        if (eng == Engine::mano)    return juce::String (v < 0.25 ? "MUTED " : v < 0.6 ? "OPEN " : "SLAP ") + percent (v);
        if (eng == Engine::caja)    return juce::String (v < 0.3 ? "CROSS-STICK " : v < 0.7 ? "HIT " : "RIMSHOT ") + percent (v);
        return percent (v);
    };

    // generator
    for (int g = 0; g < kNumGenres; ++g) genreBox.addItem (genreName ((Genre) g), g + 1);
    genreBox.onChange = [this]
    {
        const int g = genreBox.getSelectedId() - 1;
        if (g < 0 || g == choice ("genre")) return;
        setChoice ("genre", g);
        proc.generate (index, false);
        grid.repaint();
    };
    addAndMakeVisible (genreBox);

    generateButton.getProperties().set ("colour", (int) colour.getARGB());
    generateButton.onClick = [this] { proc.generate (index, true); grid.repaint(); };
    addAndMakeVisible (generateButton);

    const char* genIds[kNumGenKnobs]    = { "dens", "ghost", "var", "prob", "human", "swing" };
    const char* genNames[kNumGenKnobs]  = { "DENS", "GHOST", "VAR", "PROB", "HUMAN", "SWING" };
    for (int k = 0; k < kNumGenKnobs; ++k)
    {
        auto& s = genKnobs[k];
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setPopupDisplayEnabled (true, true, nullptr);
        s.getProperties().set ("colour", (int) colour.getARGB());
        s.textFromValueFunction = [] (double v) { return percent (v); };
        addAndMakeVisible (s);
        genAttach[k] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, QuemaoProcessor::pid (genIds[k], index), s);
        if (auto* param = proc.apvts.getParameter (QuemaoProcessor::pid (genIds[k], index)))
            s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        if (k < 3)   // Density, Ghost and Variation reshape the pattern live (same seed) while you turn them
            s.onValueChange = [this, k]
            {
                if (genKnobs[k].isMouseButtonDown()) { proc.generate (index, false); grid.repaint(); }
            };

        auto& l = genLabels[k];
        l.setText (genNames[k], juce::dontSendNotification);
        l.setFont (monoFont (9.0f));
        l.setJustificationType (juce::Justification::centred);
        l.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (l);
    }
    genKnobs[4].textFromValueFunction = [] (double v) { return v < 0.005 ? juce::String ("TIGHT") : juce::String (v * 12.0, 1) + " ms"; };
    genKnobs[5].textFromValueFunction = [] (double v) { return juce::String ((int) std::round (50.0 + v * 25.0)) + "%"; };

    // sends
    const char* sendIds[3]   = { "sendd", "sendv", "sendc" };
    const char* sendNames[3] = { "DLY", "VERB", "CHOR" };
    for (int k = 0; k < 3; ++k)
    {
        auto& s = sendKnobs[k];
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setPopupDisplayEnabled (true, true, nullptr);
        s.getProperties().set ("colour", (int) colour.getARGB());
        s.textFromValueFunction = [] (double v) { return percent (v); };
        addAndMakeVisible (s);
        sendAttach[k] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, QuemaoProcessor::pid (sendIds[k], index), s);
        if (auto* param = proc.apvts.getParameter (QuemaoProcessor::pid (sendIds[k], index)))
            s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        auto& l = sendLabels[k];
        l.setText (sendNames[k], juce::dontSendNotification);
        l.setFont (monoFont (9.0f));
        l.setJustificationType (juce::Justification::centred);
        l.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (l);
    }
    delayTimeBox.addItem ("D.TIME: GLOBAL", 1);
    for (int d = 0; d < kNumDivisions; ++d) delayTimeBox.addItem (juce::String ("D.TIME: ") + divisionName (d), d + 2);
    addAndMakeVisible (delayTimeBox);
    delayTimeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, QuemaoProcessor::pid ("dtime", index), delayTimeBox);

    addAndMakeVisible (grid);

    // stage 7: editable lane name (double-click)
    nameLabel.setFont (displayFont (18.0f));
    nameLabel.setColour (juce::Label::textColourId, ui::text);
    nameLabel.setColour (juce::Label::textWhenEditingColourId, ui::text);
    nameLabel.setColour (juce::Label::backgroundWhenEditingColourId, ui::well);
    nameLabel.setColour (juce::Label::outlineWhenEditingColourId, colour);
    nameLabel.setEditable (false, true, false);
    nameLabel.onTextChange = [this] { proc.setLaneName (index, nameLabel.getText()); nameLabel.setText (proc.getLaneName (index), juce::dontSendNotification); };
    addAndMakeVisible (nameLabel);
    help (nameLabel, "Lane name: double-click to rename. Presets set it too.");

    for (int e = 0; e < kNumEngines; ++e) help (engines[e], engineHelp (e));
    help (launch, "LAUNCH: hold the lane's note to play its pattern from step 1; play it again to restart, let go to stop.");
    help (trigger, "TRIGGER: the lane's note plays the drum directly with velocity, like a pad. The pattern rests.");
    help (latch, "LATCH: the lane follows Ableton's transport, locked to the grid. No notes needed.");
    for (int k = 0; k < 13; ++k) if (k != kArticKnob) help (knobs[k], knobHelp (k));
    help (lp, "LP: low-pass filter. Lower the cutoff to darken the lane.");
    help (hp, "HP: high-pass filter. Lower the cutoff to thin the body out.");
    for (auto& c : chokeButtons) help (c, "CHOKE: lanes in the same group (A or B) cut each other off, like an open and a muted conga.");
    help (genreBox, "GENRE: picking one writes a pattern for this drum's role (toms, hand drums or back-beat). MANUAL clears the row.");
    help (generateButton, "GENERATE: rolls a new pattern from the genre. Locked steps are never touched.");
    help (genKnobs[0], "DENS: how many hits. Reshapes the same pattern live as you turn it.");
    help (genKnobs[1], "GHOST: how many soft ghost notes fill the gaps.");
    help (genKnobs[2], "VAR: how far the pattern strays from the genre's template (Euclidean: rotation).");
    help (genKnobs[3], "PROB: the chance that each hit plays. Lower it for a pattern that breathes.");
    help (genKnobs[4], "HUMAN: up to 12 ms of laid-back timing drift plus velocity variation.");
    help (genKnobs[5], "SWING: pushes the off 16ths late, 50% to 75%.");
    help (sendKnobs[0], "DLY: send to the ping-pong delay. The echo is added on top of the dry hit.");
    help (sendKnobs[1], "VERB: send to the reverb, added on top of the dry hit.");
    help (sendKnobs[2], "CHOR: send to the chorus, added on top of the dry hit.");
    help (delayTimeBox, "D.TIME: this lane's echo spacing. GLOBAL follows the DELAY block's TIME.");
    help (grid, "STEPS: click cycles hit > ghost > ratchet > off. Right-click for the step menu, option-click to lock a step.");

    refresh();
}

LaneStrip::~LaneStrip()
{
    for (auto& s : knobs) s.setLookAndFeel (nullptr);
}

int LaneStrip::choice (const char* base) const
{
    return (int) proc.apvts.getRawParameterValue (QuemaoProcessor::pid (base, index))->load();
}

void LaneStrip::setChoice (const char* base, int value)
{
    if (auto* p = proc.apvts.getParameter (QuemaoProcessor::pid (base, index)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) value));
        p->endChangeGesture();
    }
    refresh();
}

void LaneStrip::refresh()
{
    const int eng = choice ("eng"), mode = choice ("mode");
    for (int e = 0; e < kNumEngines; ++e)
        engines[e].setToggleState (eng == e, juce::dontSendNotification);
    launch.setToggleState (mode == 0, juce::dontSendNotification);
    trigger.setToggleState (mode == 1, juce::dontSendNotification);
    latch.setEnabled (mode == 0);
    grid.setAlpha (mode == 0 ? 1.0f : 0.35f);
    help (knobs[kArticKnob], articHelp (eng));
    if (proc.presetVersion.load() != lastPresetVersion)
    {
        lastPresetVersion = proc.presetVersion.load();
        nameLabel.setText (proc.getLaneName (index), juce::dontSendNotification);
    }
    juce::String artic (articLabel ((Engine) eng));
    if ((Engine) eng == Engine::analogo)
        artic = analogTypeName (analogType (proc.apvts.getRawParameterValue (QuemaoProcessor::pid ("artic", index))->load()));
    knobLabels[kArticKnob].setText (artic, juce::dontSendNotification);
    const int fmode = choice ("fmode"), chokeGroup = choice ("choke");
    lp.setToggleState (fmode == 0, juce::dontSendNotification);
    hp.setToggleState (fmode == 1, juce::dontSendNotification);
    for (int c = 0; c < 3; ++c) chokeButtons[c].setToggleState (chokeGroup == c, juce::dontSendNotification);
    // a dot on every knob the mod sequencer moves
    auto markMod = [this] (juce::Slider& sl, int targetIndex)
    {
        const bool a = proc.modSeq.targetActive (index * kModTargetsPerLane + targetIndex);
        if ((bool) sl.getProperties().getWithDefault ("modActive", false) != a) { sl.getProperties().set ("modActive", a); sl.repaint(); }
    };
    for (int k = 0; k < 13; ++k) markMod (knobs[k], k);
    for (int k = 0; k < 3; ++k) markMod (sendKnobs[k], 13 + k);

    if (genreBox.getSelectedId() != choice ("genre") + 1)
        genreBox.setSelectedId (choice ("genre") + 1, juce::dontSendNotification);

    const int f = proc.lanes[index].uiFlash.load();
    if (f != lastFlash) { lastFlash = f; ledLevel = 1.0f; }
    else ledLevel *= 0.78f;

    grid.repaint();
    if (eng != lastEng) { lastEng = eng; repaint(); }   // header shows the engine name
    else repaint (0, getHeight() - 52, getWidth(), 52);
}

void LaneStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ui::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (ui::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    g.setColour (colour);
    g.fillEllipse (14.0f, 19.0f, 10.0f, 10.0f);

    g.setColour (ui::muted);
    g.setFont (monoFont (10.0f));
    g.drawText ("LANE " + juce::String (index + 1) + juce::String::fromUTF8 (" · ") + kNoteNames[index], getWidth() - 112, 12, 100, 24, juce::Justification::centredRight);

    g.drawText ("ENGINE", 14, 44, 60, 14, juce::Justification::centredLeft);
    g.drawText ("MIDI", 14, 128, 60, 14, juce::Justification::centredLeft);
    g.setColour (ui::edge);
    g.drawHorizontalLine (516, 14.0f, (float) getWidth() - 14.0f);
    g.setColour (ui::muted);
    {
        const int kw = (getWidth() - 28) / 5, bx = 14 + 3 * kw + 4, y3 = 186 + 2 * 66;
        g.setFont (monoFont (9.5f));
        g.drawText ("FILTER", bx, y3 + 2, 44, 24, juce::Justification::centredLeft);
        g.drawText ("CHOKE", bx, y3 + 32, 44, 24, juce::Justification::centredLeft);
    }

    // status LED
    const int y = getHeight() - 48;
    const auto& lane = proc.lanes[index];
    const int status = lane.uiStatus.load();
    const bool live = status != statusIdle;
    g.setColour (ui::wellEdge);
    g.fillEllipse (14.0f, (float) y + 3.0f, 9.0f, 9.0f);
    g.setColour (colour.withAlpha (juce::jlimit (0.0f, 1.0f, (live ? 0.35f : 0.0f) + ledLevel)));
    g.fillEllipse (14.0f, (float) y + 3.0f, 9.0f, 9.0f);

    juce::String s = status == statusHeld ? juce::String::fromUTF8 ("PLAYING · HELD")
                   : status == statusLatched ? juce::String::fromUTF8 ("PLAYING · LATCHED")
                   : status == statusTrigger ? juce::String::fromUTF8 ("TRIGGER · PLAY ") + kNoteNames[index]
                   : juce::String ("STOPPED");
    if ((status == statusHeld || status == statusLatched) && lane.uiStep.load() >= 0)
        s << juce::String::fromUTF8 ("  ·  STEP ") << (lane.uiStep.load() + 1);
    g.setColour (live ? ui::text : ui::muted);
    g.setFont (monoFont (10.5f));
    g.drawText (s, 30, y, getWidth() - 44, 15, juce::Justification::centredLeft);

    g.setColour (ui::muted.withAlpha (0.8f));
    g.setFont (monoFont (9.5f));
    g.drawText (juce::String::fromUTF8 ("click · right-click: menu · opt-click: lock"), 14, y + 22, getWidth() - 28, 14, juce::Justification::centredLeft);
}

void LaneStrip::resized()
{
    nameLabel.setBounds (26, 10, getWidth() - 130, 28);
    const int pad = 14, w = getWidth() - pad * 2;
    const int tw = (w - 8) / 3;
    for (int e = 0; e < kNumEngines; ++e)
    {
        const int col = e % 3, row = e / 3;
        engines[e].setBounds (pad + col * (tw + 4), 60 + row * 32, col == 2 ? w - (tw + 4) * 2 : tw, 28);
    }

    launch.setBounds (pad, 144, tw, 28);
    trigger.setBounds (pad + tw + 4, 144, tw, 28);
    latch.setBounds (pad + (tw + 4) * 2, 144, w - (tw + 4) * 2, 28);

    const int kw = w / 5;
    for (int k = 0; k < kNumKnobs; ++k)
    {
        const int col = k % 5, row = k / 5, y = 186 + row * 66;
        knobs[k].setBounds (pad + col * kw, y, kw, 46);
        knobLabels[k].setBounds (pad + col * kw, y + 47, kw, 14);
    }
    // row 3, last two columns: filter mode and choke group
    const int bx = pad + 3 * kw + 4, bw = w - 3 * kw - 4, y3 = 186 + 2 * 66;
    lp.setBounds (bx + 44, y3 + 2, (bw - 44) / 2 - 2, 24);
    hp.setBounds (bx + 44 + (bw - 44) / 2 + 1, y3 + 2, (bw - 44) / 2 - 2, 24);
    const int cw = (bw - 44) / 3;
    for (int c = 0; c < 3; ++c) chokeButtons[c].setBounds (bx + 44 + c * cw, y3 + 32, cw - 3, 24);
    genreBox.setBounds (pad, 392, w - 110, 26);
    generateButton.setBounds (pad + w - 104, 392, 104, 26);
    grid.setBounds (pad, 426, w, 36);
    const int gw = w / kNumGenKnobs;
    for (int k = 0; k < kNumGenKnobs; ++k)
    {
        genKnobs[k].setBounds (pad + k * gw, 468, gw, 32);
        genLabels[k].setBounds (pad + k * gw, 500, gw, 12);
    }
    const int sw = (w - 120) / 3;
    for (int k = 0; k < 3; ++k)
    {
        sendKnobs[k].setBounds (pad + k * sw, 522, sw, 30);
        sendLabels[k].setBounds (pad + k * sw, 552, sw, 12);
    }
    delayTimeBox.setBounds (pad + w - 116, 528, 116, 24);
}

// ---------------------------------------------------------------- fx panel

FxPanel::FxPanel (QuemaoProcessor& p) : proc (p)
{
    addKnob (chTone, "chtone", "TONE"); addKnob (chRate, "chrate", "RATE"); addKnob (chMix, "chmix", "MIX");
    addKnob (dFdbk, "dfdbk", "FDBK");   addKnob (dTone, "dtone", "TONE");   addKnob (dMix, "dmix", "MIX");
    addKnob (vSize, "vsize", "SIZE");   addKnob (vDamp, "vdamp", "DAMP");   addKnob (vMix, "vmix", "MIX");
    addKnob (comp, "comp", "COMP");     addKnob (crush, "crush", "CRUSH");  addKnob (gain, "master", "GAIN");
    chRate.s.textFromValueFunction = [] (double v) { return juce::String (0.1 + v * v * 4.9, 2) + " Hz"; };
    dFdbk.s.textFromValueFunction = [] (double v) { return percent (v * 0.92); };
    comp.s.textFromValueFunction = [] (double v) { return v < 0.005 ? juce::String ("OFF") : percent (v); };
    crush.s.textFromValueFunction = [] (double v) { return v < 0.005 ? juce::String ("OFF") : juce::String (16.0 - 12.0 * v, 1) + " bit"; };

    for (int d = 0; d < kNumDivisions; ++d) dTime.addItem (juce::String ("TIME ") + divisionName (d), d + 1);
    addAndMakeVisible (dTime);
    dTimeA = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "gdtime", dTime);
    help (chTone.s, "CHORUS TONE: brightness of the chorus return.");
    help (chRate.s, "CHORUS RATE: speed of the chorus wobble, 0.1-5 Hz.");
    help (chMix.s, "CHORUS MIX: level of the chorus return, added on top.");
    help (dFdbk.s, "DELAY FDBK: how many repeats.");
    help (dTone.s, "DELAY TONE: darker or brighter repeats.");
    help (dMix.s, "DELAY MIX: level of the echoes, added on top.");
    help (dTime, "DELAY TIME: the global echo spacing. Lanes set to GLOBAL follow it.");
    help (vSize.s, "REVERB SIZE: length of the tail.");
    help (vDamp.s, "REVERB DAMP: softens the tail's highs.");
    help (vMix.s, "REVERB MIX: level of the reverb, added on top.");
    help (vType, "REVERB TYPE: ROOM (tight), PLATE (dense, bright) or HALL (big, with pre-delay).");
    help (comp.s, "COMP: punchy bus compressor. The attack gets through, the body gets squeezed and lifted.");
    help (crush.s, "CRUSH: bit and sample-rate reduction, 16 down to 4 bits.");
    help (gain.s, "GAIN: QUEMAO's output level.");
    for (int t = 0; t < 3; ++t) vType.addItem (reverbTypeName (t), t + 1);
    addAndMakeVisible (vType);
    vTypeA = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "vtype", vType);
}

void FxPanel::addKnob (Knob& k, const char* id, const char* label)
{
    k.s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.s.setPopupDisplayEnabled (true, true, nullptr);
    k.s.textFromValueFunction = [] (double v) { return percent (v); };
    addAndMakeVisible (k.s);
    k.a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.s);
    if (auto* param = proc.apvts.getParameter (id))
        k.s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    k.l.setText (label, juce::dontSendNotification);
    k.l.setFont (monoFont (9.5f));
    k.l.setJustificationType (juce::Justification::centred);
    k.l.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.l);
}

void FxPanel::paint (juce::Graphics& g)
{
    const char* titles[4] = { "CHORUS", "DELAY", "REVERB", "OUTPUT" };
    const char* subs[4]   = { "SEND", "PING-PONG", "SEND", "BUS" };
    const int bw = (getWidth() - 36) / 4;
    for (int b = 0; b < 4; ++b)
    {
        auto r = juce::Rectangle<float> ((float) (b * (bw + 12)), 0.0f, (float) bw, (float) getHeight());
        g.setColour (ui::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (ui::edge);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (ui::text);
        g.setFont (displayFont (15.0f));
        g.drawText (titles[b], (int) r.getX() + 14, 10, 120, 20, juce::Justification::centredLeft);
        g.setColour (ui::muted);
        g.setFont (monoFont (9.0f));
        g.drawText (juce::String::fromUTF8 (subs[b]), (int) r.getX() + 14, 30, bw - 28, 14, juce::Justification::centredLeft);
    }
}

void FxPanel::resized()
{
    const int bw = (getWidth() - 36) / 4;
    auto place = [&] (int block, int slot, Knob& k)
    {
        const int x0 = block * (bw + 12) + 12, y = 80 + slot * 42;
        k.s.setBounds (x0, y, 38, 38);
        k.l.setBounds (x0 + 42, y + 12, bw - 56, 14);
        k.l.setJustificationType (juce::Justification::centredLeft);
    };
    place (0, 0, chTone); place (0, 1, chRate); place (0, 2, chMix);
    place (1, 0, dFdbk);  place (1, 1, dTone);  place (1, 2, dMix);
    place (2, 0, vSize);  place (2, 1, vDamp);  place (2, 2, vMix);
    place (3, 0, comp);   place (3, 1, crush);  place (3, 2, gain);
    dTime.setBounds (1 * (bw + 12) + 10, 48, bw - 20, 24);
    vType.setBounds (2 * (bw + 12) + 10, 48, bw - 20, 24);
}

// ---------------------------------------------------------------- mod sequencer

void ModBars::paint (juce::Graphics& g)
{
    const int t = proc.modTarget.load();
    const auto colour = ui::laneColours[modTargetLane (t)];
    const int playing = proc.modSeq.uiStep.load();
    const float gap = 4.0f, cw = ((float) getWidth() - gap * 15.0f) / 16.0f;
    const float h = (float) getHeight() - 16.0f, mid = h * 0.5f;

    g.setColour (ui::wellEdge);
    for (float y : { 0.0f, mid, h }) g.drawHorizontalLine ((int) y, 0.0f, (float) getWidth());

    for (int i = 0; i < 16; ++i)
    {
        const float x = (float) i * (cw + gap);
        const float v = proc.modSeq.values[t][i].load();
        if (i % 4 == 0) { g.setColour (ui::well); g.fillRect (x, 0.0f, cw, h); }
        auto bar = v >= 0.0f ? juce::Rectangle<float> (x, mid - v * mid, cw, std::max (1.5f, v * mid))
                             : juce::Rectangle<float> (x, mid, cw, std::max (1.5f, -v * mid));
        g.setColour (i == playing ? colour.brighter (0.35f) : colour.withAlpha (std::abs (v) < 1.0e-4f ? 0.35f : 0.85f));
        g.fillRect (bar);
        if (i == hover) { g.setColour (ui::text.withAlpha (0.5f)); g.drawRect (x, 0.0f, cw, h, 1.0f); }
        g.setColour (i == playing ? colour : ui::muted);
        g.setFont (monoFont (9.0f, i == playing));
        g.drawText (juce::String (i + 1), (int) x, (int) h + 3, (int) cw, 12, juce::Justification::centred);
    }
}

void ModBars::setAt (juce::Point<float> p)
{
    const int i = stepAt (p.x);
    const float h = (float) getHeight() - 16.0f, mid = h * 0.5f;
    float v = juce::jlimit (-1.0f, 1.0f, (mid - p.y) / mid);
    if (std::abs (v) < 0.04f) v = 0.0f;   // easy to land on the centre
    proc.modSeq.values[proc.modTarget.load()][i].store (v);
    hover = i;
    repaint();
    if (onEdit) onEdit();
}
void ModBars::mouseDown (const juce::MouseEvent& e) { setAt (e.position); }
void ModBars::mouseDrag (const juce::MouseEvent& e) { setAt (e.position); }
void ModBars::mouseMove (const juce::MouseEvent& e) { hover = stepAt (e.position.x); repaint(); if (onEdit) onEdit(); }
void ModBars::mouseDoubleClick (const juce::MouseEvent& e)
{
    proc.modSeq.values[proc.modTarget.load()][stepAt (e.position.x)].store (0.0f);
    repaint();
    if (onEdit) onEdit();
}

ModPanel::ModPanel (QuemaoProcessor& p) : proc (p), bars (p)
{
    for (int lane = 0; lane < 4; ++lane)
    {
        target.addSectionHeading ("LANE " + juce::String (lane + 1));
        for (int k = 0; k < kModTargetsPerLane; ++k)
        {
            const int t = lane * kModTargetsPerLane + k;
            target.addItem ("L" + juce::String (lane + 1) + juce::String::fromUTF8 (" › ") + modTargetName (t), t + 1);
        }
    }
    target.setSelectedId (proc.modTarget.load() + 1, juce::dontSendNotification);
    target.onChange = [this] { if (target.getSelectedId() > 0) { proc.modTarget.store (target.getSelectedId() - 1); bars.repaint(); repaint(); } };
    addAndMakeVisible (target);

    for (int r = 0; r < kNumModRates; ++r) rate.addItem (juce::String ("RATE ") + modRateName (r), r + 1);
    addAndMakeVisible (rate);
    rateA = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "modrate", rate);

    depth.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    depth.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    depth.setPopupDisplayEnabled (true, true, nullptr);
    depth.textFromValueFunction = [] (double v) { return juce::String::fromUTF8 ("±") + percent (v); };
    addAndMakeVisible (depth);
    depthA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "moddepth", depth);
    depth.setDoubleClickReturnValue (true, 0.6);

    clear.onClick = [this] { proc.modSeq.clearTarget (proc.modTarget.load()); bars.repaint(); updateTargetNames(); };
    dice.onClick = [this]
    {
        auto& r = juce::Random::getSystemRandom();
        const int t = proc.modTarget.load();
        for (int i = 0; i < 16; ++i)
            proc.modSeq.values[t][i].store (r.nextFloat() < 0.35f ? 0.0f : std::round ((r.nextFloat() * 2.0f - 1.0f) * 4.0f) / 4.0f);
        bars.repaint(); updateTargetNames();
    };
    for (auto* b : { &clear, &dice }) addAndMakeVisible (b);

    bars.onEdit = [this] { repaint (0, getHeight() - 26, getWidth(), 26); updateTargetNames(); };
    help (target, "TARGET: the knob this lane of the mod sequencer moves. Every knob keeps its own lane; a dot marks the ones in use.");
    help (rate, "RATE: how long each of the 16 steps lasts, synced to Ableton.");
    help (depth, "DEPTH: how far the drawn steps push their knobs. 0 switches the movement off.");
    help (clear, "CLEAR: flattens the selected target's lane.");
    help (dice, "RANDOM: draws a random lane for the selected target.");
    help (bars, "MOD STEPS: drag to draw, double-click a bar to reset. The centre line is the knob's own value.");
    addAndMakeVisible (bars);
    updateTargetNames();
}

void ModPanel::updateTargetNames()
{
    // a dot marks every knob that has a mod lane
    for (int t = 0; t < kNumModTargets; ++t)
    {
        const juce::String name = (proc.modSeq.targetActive (t) ? juce::String::fromUTF8 ("● ") : juce::String())
                                + "L" + juce::String (modTargetLane (t) + 1) + juce::String::fromUTF8 (" › ") + modTargetName (t);
        if (target.getItemText (target.indexOfItemId (t + 1)) != name)
            target.changeItemText (t + 1, name);
    }
}

juce::String ModPanel::describe (int t, float offset) const
{
    LaneSettings s; LaneSends d;
    float lo, hi; modTargetRange (t, lo, hi);
    *modTargetField (t, s, d) = 0.5f * (lo + hi);           // offset relative to a centred knob
    const float depthV = proc.apvts.getRawParameterValue ("moddepth")->load();
    applyMod (t, offset, depthV, s, d);
    const float delta = *modTargetField (t, s, d) - 0.5f * (lo + hi);
    const int k = t % kModTargetsPerLane;
    juce::String sign = delta > 0 ? "+" : (delta < 0 ? juce::String::fromUTF8 ("−") : juce::String());
    if (k == 0) return sign + juce::String ((int) std::abs (delta)) + " st";
    if (k == 1) return sign + juce::String (std::abs (delta), 1) + " ct";
    if (k == 12) return sign + juce::String ((int) std::round (std::abs (delta) * 100.0f)) + (delta < 0 ? " L" : " R");
    return sign + juce::String ((int) std::round (std::abs (delta) * 100.0f)) + "%";
}

void ModPanel::refresh()
{
    const int st = proc.modSeq.uiStep.load(), t = proc.modTarget.load();
    if (st != lastStep || t != lastTarget) { lastStep = st; bars.repaint(); }
    if (t != lastTarget)
    {
        lastTarget = t;
        target.setSelectedId (t + 1, juce::dontSendNotification);
        repaint();
    }
}

void ModPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ui::panel); g.fillRoundedRectangle (r, 8.0f);
    g.setColour (ui::edge);  g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (ui::text);
    g.setFont (displayFont (15.0f));
    g.drawText ("MOD SEQ", 14, 10, 130, 22, juce::Justification::centredLeft);
    g.setFont (monoFont (9.0f));
    g.setColour (ui::muted);
    g.drawText ("16 STEPS", 84, 14, 60, 16, juce::Justification::centredLeft);

    g.setFont (monoFont (9.5f));
    g.setColour (ui::muted);
    const int t = proc.modTarget.load();
    juce::String caption = juce::String::fromUTF8 ("relative  ·  centre line = the knob's own value  ·  drag to draw, double-click resets");
    if (bars.hover >= 0)
        caption = "STEP " + juce::String (bars.hover + 1) + ":  " + modTargetName (t) + " "
                + describe (t, proc.modSeq.values[t][bars.hover].load()) + juce::String::fromUTF8 ("  ·  lane ") + juce::String (modTargetLane (t) + 1);
    g.drawText (caption, 14, getHeight() - 24, getWidth() - 28, 16, juce::Justification::centredLeft);
}

void ModPanel::resized()
{
    const int w = getWidth();
    target.setBounds (148, 10, 150, 24);
    rate.setBounds (304, 10, 88, 24);
    depth.setBounds (398, 6, 32, 32);
    clear.setBounds (w - 140, 10, 60, 24);
    dice.setBounds (w - 76, 10, 64, 24);
    bars.setBounds (14, 46, getWidth() - 28, getHeight() - 46 - 30);
}

// ---------------------------------------------------------------- main view

MainView::MainView (QuemaoProcessor& p) : proc (p), modPanel (p), fxPanel (p)
{
    for (int i = 0; i < 4; ++i)
    {
        strips[i] = std::make_unique<LaneStrip> (proc, i);
        addAndMakeVisible (*strips[i]);
    }
    addAndMakeVisible (modPanel);
    addAndMakeVisible (fxPanel);

    // presets
    presetBox.setTextWhenNothingSelected ("PRESET");
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) loadPresetById (presetBox.getSelectedId()); };
    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    savePreset.onClick = [this] { askToSave(); };
    midiOut.setClickingTogglesState (true);
    midiOutA = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "midiout", midiOut);
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevPreset, &nextPreset, &savePreset, &midiOut })
        addAndMakeVisible (c);
    help (presetBox, "PRESET: factory kits and your own saved kits (Music > ZOONIDO > QUEMAO Presets).");
    help (prevPreset, "Previous preset.");
    help (nextPreset, "Next preset.");
    help (savePreset, "SAVE: stores everything (sounds, patterns, locks, mod lanes, lane names) as your own preset.");
    help (midiOut, "MIDI OUT: sends every pattern hit as a note (C1 D1 E1 F1). In another MIDI track set MIDI From to this track.");
    rebuildPresetList();

    setSize (kWidth, kHeight);
}

void MainView::rebuildPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("FACTORY");
    for (int i = 0; i < kNumFactoryPresets; ++i) presetBox.addItem (factoryPreset (i).name, i + 1);
    userFiles = proc.userPresets();
    if (! userFiles.isEmpty())
    {
        presetBox.addSectionHeading ("YOURS");
        for (int i = 0; i < userFiles.size(); ++i)
            presetBox.addItem (userFiles[i].getFileNameWithoutExtension().toUpperCase(), 101 + i);
    }
    presetBox.setText (proc.getPresetName(), juce::dontSendNotification);
}

void MainView::loadPresetById (int id)
{
    if (id >= 1 && id <= kNumFactoryPresets) proc.applyFactoryPreset (id - 1);
    else if (id >= 101 && id - 101 < userFiles.size()) proc.loadUserPreset (userFiles[id - 101]);
    presetBox.setText (proc.getPresetName(), juce::dontSendNotification);
}

void MainView::stepPreset (int delta)
{
    juce::Array<int> ids;
    for (int i = 0; i < kNumFactoryPresets; ++i) ids.add (i + 1);
    for (int i = 0; i < userFiles.size(); ++i) ids.add (101 + i);
    int current = -1;
    for (int i = 0; i < ids.size(); ++i)
        if (presetBox.getItemText (presetBox.indexOfItemId (ids[i])) == proc.getPresetName()) current = i;
    const int next = current < 0 ? 0 : (current + delta + ids.size()) % ids.size();
    loadPresetById (ids[next]);
}

void MainView::askToSave()
{
    auto* w = new juce::AlertWindow ("SAVE PRESET", "Name your kit:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.getPresetName());
    w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<MainView> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (safe == nullptr || result != 1) return;
        const auto name = w->getTextEditorContents ("name");
        if (safe->proc.saveUserPreset (name)) safe->rebuildPresetList();
    }), true);
}

void MainView::refresh()
{
    for (auto& s : strips) s->refresh();
    modPanel.refresh();
    repaint (getWidth() - 210, 14, 120, 40);

    if (proc.presetVersion.load() != lastPresetVersion)
    {
        lastPresetVersion = proc.presetVersion.load();
        presetBox.setText (proc.getPresetName(), juce::dontSendNotification);
    }

    // hover help: the sentence of whatever control is under the mouse
    juce::String text;
    auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
    while (c != nullptr && c != this)
    {
        if (c->getProperties().contains ("help")) { text = c->getProperties()["help"].toString(); break; }
        c = c->getParentComponent();
    }
    if (c != this && c == nullptr) text = {};
    if (text != helpText) { helpText = text; repaint (0, getHeight() - 36, getWidth(), 36); }
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (ui::ground);

    g.setColour (ui::ember);
    g.setFont (displayFont (34.0f));
    g.drawText ("QUEMAO", 20, 14, 200, 40, juce::Justification::centredLeft);

    g.setColour (ui::muted);
    g.setFont (monoFont (11.0f));
    g.drawText ("PERCUSSION DESIGNER", 196, 24, 180, 22, juce::Justification::centredLeft);

    const juce::String bpm = juce::String (proc.uiBpm.load(), 1) + " BPM";
    auto box = juce::Rectangle<float> ((float) getWidth() - 205.0f, 18.0f, 104.0f, 30.0f);
    g.setColour (ui::wellEdge);
    g.drawRoundedRectangle (box, 4.0f, 1.0f);
    g.setColour (ui::muted);
    g.drawText (bpm, box, juce::Justification::centred);
    g.setColour (ui::muted.withAlpha (0.6f));
    g.drawText ("ZOONIDO", getWidth() - 90, 24, 70, 20, juce::Justification::centredRight);

    g.setColour (ui::edge);
    g.drawHorizontalLine (64, 20.0f, (float) getWidth() - 20.0f);

    // help bar
    auto bar = juce::Rectangle<float> (20.0f, (float) getHeight() - 34.0f, (float) getWidth() - 40.0f, 26.0f);
    g.setColour (ui::panel);
    g.fillRoundedRectangle (bar, 5.0f);
    g.setColour (ui::edge);
    g.drawRoundedRectangle (bar.reduced (0.5f), 5.0f, 1.0f);
    g.setFont (monoFont (10.5f, true));
    g.setColour (ui::ember);
    g.drawText ("?", bar.withWidth (26.0f), juce::Justification::centred);
    g.setFont (monoFont (10.5f));
    g.setColour (helpText.isEmpty() ? ui::muted : ui::text);
    g.drawText (helpText.isEmpty() ? juce::String::fromUTF8 ("Hover any control for help  ·  lane notes C1 D1 E1 F1  ·  LAUNCH holds, TRIGGER plays, LATCH follows the transport")
                                   : helpText,
                bar.withTrimmedLeft (28.0f).withTrimmedRight (8.0f), juce::Justification::centredLeft, true);
}

void MainView::resized()
{
    const int gap = 12, top = 78, h = 640;
    const int w = (getWidth() - 40 - gap * 3) / 4;
    for (int i = 0; i < 4; ++i)
        strips[i]->setBounds (20 + i * (w + gap), top, w, h);
    const int y = top + h + gap, bh = 206;
    prevPreset.setBounds (400, 20, 26, 26);
    presetBox.setBounds (430, 20, 220, 26);
    nextPreset.setBounds (654, 20, 26, 26);
    savePreset.setBounds (688, 20, 64, 26);
    midiOut.setBounds (getWidth() - 320, 20, 100, 26);
    modPanel.setBounds (20, y, 2 * w + gap, bh);
    fxPanel.setBounds (20 + 2 * (w + gap), y, 2 * w + gap, bh);
}

// ---------------------------------------------------------------- editor

QuemaoEditor::QuemaoEditor (QuemaoProcessor& p) : AudioProcessorEditor (&p), view (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (view);
    // the window scales: drag the corner, the layout keeps its proportions
    setResizable (true, true);
    setResizeLimits (MainView::kWidth / 2, MainView::kHeight / 2, MainView::kWidth * 3 / 2, MainView::kHeight * 3 / 2);
    getConstrainer()->setFixedAspectRatio ((double) MainView::kWidth / (double) MainView::kHeight);
    setSize ((int) (MainView::kWidth * 0.82), (int) (MainView::kHeight * 0.82));
    startTimerHz (30);
}

QuemaoEditor::~QuemaoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void QuemaoEditor::paint (juce::Graphics& g) { g.fillAll (ui::ground); }

void QuemaoEditor::resized()
{
    const float scale = (float) getWidth() / (float) MainView::kWidth;
    view.setTransform (juce::AffineTransform::scale (scale));
    view.setBounds (0, 0, MainView::kWidth, MainView::kHeight);
}

void QuemaoEditor::timerCallback() { view.refresh(); }
