# Mandy
An **experimental MIDI chord sequencer** sketch for the  **Midiboy** (Arduino) platform - written to act as a small hub generating notes in a semi randomised fashion (e.g. triggering a setup with multiple mono synths and sources).

In its essence, notes are derived from chords programmed in the step sequencer which are automatically assigned to tracks (all chords consist of 3 notes). Notes are then triggered by applying track specific patterns, lengths and tempo dividers in order to achieve a fair amount of varied MIDI output.

In addition, tracks can be routed to different MIDI channels using random or fixed modes, making it easy to achieve polyphony among several sound sources.

Mandy also supports external MIDI control (sync, play / stop and note offset).

[![](http://img.youtube.com/vi/PFX4NWRe-rE/0.jpg)](http://www.youtube.com/watch?v=PFX4NWRe-rE)

## Input

Key(s) | Action
------------ | -------------
A | **Playback** start/stop
B | Switch **Page**
U/D | Move between steps or parameters
L/R | Toggle parameter or step value

## Pages
### Sequencer
Step number, Chord, Bars, Note offset (EXT = external MIDI input)

### Configuration
Parameter | Description
------------- | -------------
Steps (**STEPS**) | Number of sequencer steps
Key (**KEY**) | 0 Major, 1 Minor
Pulses per bar (**PLSBAR**) | Number of pulses per (sequencer) step (max = random)
Step reset (1..3 **SRESET**) | Reset pattern index between (sequencer) steps
Swing (1..3 **SWING**) | Enable / Disable swing
Trigger pattern (1..3 **TRGPTR**) | Pattern used to trigger note
Trigger pattern length (1..3 **PTRLEN**) | Number of pattern steps
Modify pattern (**MODPTR**) | (Fixed length) Rotating pattern xor’d with a trigger pattern
Velocity pattern (1..3 **VELPTR**) | (Fixed length) Pattern accenting note velocity
Duration pattern (1..3 **VELPTR**) | (Fixed length) Pattern accenting note duration
Filter pattern (1..3 **MODPTR**) | (Fixed length) Pattern used to skip not from being triggered (max = random)
Arpeggiator pattern (1..3 **ARP**) | Apply arpeggiation pattern to note
Repeat mode (1..3 **RPTMDE**) | Re-triggering note
Octave range | Maximum number of octaves an arpeggiated note can span
Octave offset (1..3 **OCTOFS**) | Fixed octave offset
Attenuator pattern (**ATTN**) | Attenuate note velocity and duration
Note velocity (**VEL**) | Note velocity
Accented note velocity (**ACCVEL**) | Accented note velocity
Note duration(**DUR**) | Note duration (0 for hold)
Accented note duration (**ACCDUR**) | Accented note duration (0 for hold)
Randomization (1..3 **RANDOM**) | Applied randomization (%) for velocity and duration
Pulses per trigger (1..3 **PLSTRG**) | Number of pulses per (trigger) step (max = random)
Midi channel (1..3 **MID CH**) | MIDI channel (0 disables channel output)
Cycle midi channels (1..3 **CYCLE CH**) | Cycle or randomize MIDI channels among tracks (0 disabled, 1-3 skip channel, 4 all channels, random)
Internal sync (**INT SYNC**) | Use internal clock (disable for external MIDI sync)
Tempo (**BPM**) | Tempo (when clocked using internal sync)
Midi receive (**MID RECV**) | Incoming/listen MIDI channel used to set sequence note offset **when sequencer step is set to external** (0 accepts all channels)
Trigger mode (**TRIGMODE**) | 0 = Normal (start/stop button or MIDI control), 1 = playback while (MIDI) key is held, 2 = Advance one sequence step upon (MIDI) key trigger.

### Save / Load
Hold **left** button to **save** and **right** button to **load**.

### Debug
Get a visual idea of notes triggered.
