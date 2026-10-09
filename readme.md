# quemao

QUEMAO, a 4-lane percussion designer for Ableton (VST3, Mac): toms, congas, snares
and hand drums for tribal/minimal techno, tribal/organic/progressive house, Cumbia and Bullerengue.

**Complete: stage 7 – MIDI out, presets, hover help, final layout**

- Four lanes, each with a 16-step pattern (click: hit → ghost → ratchet → off, right-click clears).
- Six engines per lane, each giving the ARTIC knob its own job:
  - MEMBRANA – drumhead model (toms, floor toms, frame drums). EDGE: strike center → rim.
  - MANO – hand drum (conga, bongo, tambor alegre, llamador). ARTIC: muted → open → slap.
  - CAJA – snare, body + wires. STROKE: cross-stick → hit → rimshot. Decay sets the wires.
  - ANÁLOGO – drum-machine circuits. TYPE: TOM | SNARE | RIM | CLAP.
  - MADERA – wood shell + plucked knock (cajón, tambora shell, wood block). RIM: shell → rim.
  - FM – two-operator FM toms. METAL: round → metallic.
- Knobs per lane: Tune (±24 st), Fine (±50 ct), P.Env, P.Dec, Attack, Decay, Artic, Tone, Drive,
  Level, Cutoff, Res, Pan. Double-click resets.
  - P.ENV / P.DEC scale each engine's own pitch sweep (centre = the engine's default).
  - ATTACK fades in the strike (0–30 ms). TONE tilts dark ↔ bright. DRIVE saturates the lane.
  - FILTER: LP or HP per lane; Cutoff fully right is open in both modes.
  - CHOKE: lanes in the same group (A or B) cut each other off, like an open and a muted conga.
- Generator per lane: Minimal Techno, Tribal Techno, Tribal House, Organic House, Progressive,
  Cumbia, Bullerengue, Euclidean, Manual. Picking a genre writes a pattern for the lane's role
  (toms/tambora, hand drums, or back-beat/palmas, from its engine). GENERATE re-rolls.
  DENS / GHOST / VAR reshape the same pattern live as you turn them.
- Step lock: option-click a step (or right-click › Lock step). GENERATE never touches locked steps.
  Right-click a step for Hit / Ghost / Ratchet / Off.
- Feel per lane: PROB (chance each hit plays), HUMAN (up to 12 ms laid-back drift + velocity),
  SWING (50–75%).
- Sends per lane: DLY, VERB, CHOR, plus D.TIME (GLOBAL or the lane's own synced time).
  Returns are added on top of the dry hit, so the original sound is never dampened.
  - CHORUS: Tone, Rate, Mix.  DELAY: synced ping-pong, Time, Feedback, Tone, Mix.
  - REVERB: Room / Plate / Hall, Size, Damp, Mix.  OUTPUT: bus Comp, Crush, Gain.
- MOD SEQ: one shared 16-step sequencer, synced to Ableton (free-runs at host tempo when stopped).
  Pick a TARGET (any lane's knob or send), drag the bars to draw, double-click a bar to reset.
  Every knob keeps its own lane, relative to the knob's value; all drawn lanes play together.
  RATE 1/32 – 1/2, DEPTH, CLEAR, RANDOM. A dot marks knobs that are being modulated.
  Tune moves in whole semitones, so the toms can play melodies.
- The window scales: drag its corner.
- PRESETS: 9 factory kits (INIT, RITUAL 132, CUMBIA ROOTS, BULLERENGUE, MINIMAL RIM, ORGANIC HOUSE,
  TRIBAL HOUSE, PROGRESSIVE DUB, FUEGO LENTO) plus your own: SAVE stores everything in
  Music > ZOONIDO > QUEMAO Presets. Lane names are part of a preset; double-click a name to rename it.
- MIDI OUT: every pattern hit leaves as a note (C1 D1 E1 F1, channel 1). Notes played in on
  TRIGGER lanes are never echoed. In Ableton, set another MIDI track's MIDI From to QUEMAO's track.
- HELP: hover any control and the bar at the bottom explains it.
- MIDI per lane, notes C1, D1, E1, F1:
  - LAUNCH: note-on starts the pattern from step 1 instantly (or restarts it), note-off stops it.
    LATCH makes the lane follow Ableton's transport, locked to the grid.
  - TRIGGER: the note plays the drum directly with velocity (pad mode).

Every push builds the plugin for Mac automatically, as an Audio Unit (quemao.component)
and a VST3 (quemao.vst3), both in quemao-mac.zip.
Download it from the **Actions** tab → latest run → **Artifacts**.
