/*
   Mandy - A MIDI Chord Sequencer sketch
   Copyright (C) 2022 Jacob Ulmert

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include <Midiboy.h>
#include <EEPROM.h>
#include <util/atomic.h>

#include "bitmaps.h"
#include "config.h"

static const PROGMEM char LABEL_SAVESLOT[13] = {"SAVE SLOT\0"};
static const PROGMEM char LABEL_CANCELLED[10] = {"CANCELLED"};
static const PROGMEM char LABEL_SAVED[12] = {"SAVE OK \0"};
static const PROGMEM char LABEL_LOAD_SUCCESS[12] = {"LOAD OK \0"};
static const PROGMEM char LABEL_LOAD_FAIL[12] = {"LOAD NOK\0"};

static const PROGMEM char NOTIF_LOADSAVE[26] = {"HOLD LEFT>SAVE RIGHT>LOAD"};
static const PROGMEM char NOTIF_SYNC[26] =    {"WAITING FOR MIDI CLOCK..."};
static const PROGMEM char NOTIF_WELCOME[26] = {"HELLO HELLO - I AM MANDY!"};

static const PROGMEM char LABELS_SETTINGS[54][12] = {"STEPS   \0", "KEY     \0", 

                                                     "PLSBAR  \0", 
                                                     
                                                     "1 SRESET\0", "2 SRESET\0", "3 SRESET\0",

                                                     "1  SWING\0", "2  SWING\0", "3  SWING\0",

                                                     "1 TRGPTR\0", "2 TRGPTR\0", "3 TRGPTR\0",
                                                     "1 PTRLEN\0", "2 PTRLEN\0", "3 PTRLEN\0",

                                                     "  MODPTR\0",

                                                     "1 VELPTR\0", "2 VELPTR\0", "3 VELPTR\0",
                                                     "1 DURPTR\0", "2 DURPTR\0", "3 DURPTR\0",

                                                     "1 FLTPTR\0", "2 FLTPTR\0", "3 FLTPTR\0",

                                                     "1    ARP\0", "2    ARP\0", "3    ARP\0",

                                                     "1 RPTMDE\0", "2 RPTMDE\0", "3 RPTMDE\0",

                                                     "OCTRANGE\0",

                                                     "1 OCTOFS\0", "2 OCTOFS\0", "3 OCTOFS\0",

                                                     "    ATTN\0",

                                                     "     VEL\0", 
                                                     "  ACCVEL\0", 
                                                     "     DUR\0",
                                                     "  ACCDUR\0",

                                                     "1 RANDOM\0", "2 RANDOM\0", "3 RANDOM\0",

                                                     "1 PLSTRG\0", "2 PLSTRG\0", "3 PLSTRG\0",


                                                     "1 MID CH\0", "2 MID CH\0", "3 MID CH\0",

                                                     "CYCLE CH\0",

                                                     "INT SYNC\0",

                                                     "BPM     \0",

                                                     "MID RECV\0",

                                                     "TRIGMODE\0"
                                                    };

static const PROGMEM char LABEL_RND[4] = {"RND"};
static const PROGMEM char LABEL_OFF[4] = {"OFF"};
static const PROGMEM char LABEL_ON[4] = {" ON"};

static const PROGMEM char LABEL_NOTES[13][3] = {"C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B ", {'E', 'X', 'T'}};

#ifdef TRACEPAGE
#define TRACEFLAG_CONSUME 0b10000000
typedef struct {
  uint8_t col[3];
  uint8_t midiChnls[3];
  uint16_t curTrigPtrn[3];
  uint16_t curStep[3];
  uint8_t frameTime, maxFrameTime;
} trace_t;
trace_t g_trace = {{0, 0, 0}, {0, 0, 0} , {0xffff, 0xffff, 0xffff} , {0xffff, 0xffff, 0xffff}, 0, 0};
#endif

uint8_t g_renderSection = 0;
bool g_renderPlayback = false;

inline uint8_t *notePropToString(uint8_t v, uint8_t a, uint8_t *dest) {
  if (v < 21) {
    *dest++ = pgm_read_byte(&LABEL_NOTES[12][0]);
    *dest++ = pgm_read_byte(&LABEL_NOTES[12][1]);
    *dest++ = pgm_read_byte(&LABEL_NOTES[12][2]);
  } else {
    uint8_t i = (v % 12);
    *dest++ = pgm_read_byte(&LABEL_NOTES[i][0]);
    *dest++ = pgm_read_byte(&LABEL_NOTES[i][1]);
    *dest++ = ('0' + (v / 12 - 1));
  }
  return dest;
}

typedef struct {
  uint8_t chordIdx;
  uint8_t duration;
  uint8_t root;
} seq_step_t;

#define SONG_STATE_IDLE 0
#define SONG_STATE_PLAYING 1

#define NUM_VOICES 3
#define NUM_SEQSTEPS 16

#define TRIGMODE_NORMAL 0
#define TRIGMODE_KEY 1
#define TRIGMODE_KEY_ADVANCE 2
#define TRIGMODE_LAST TRIGMODE_KEY_ADVANCE

typedef struct {
  uint16_t pulseCount;
  uint16_t stepPulseCount;
  
  uint8_t sync;
  uint8_t bpm;
  uint8_t seqStepIdx;
  uint8_t seqNumSteps;
  uint8_t key;

  uint8_t stepReset[NUM_VOICES];
  
  uint8_t swing[NUM_VOICES];
  uint8_t state;

  seq_step_t seqSteps[NUM_SEQSTEPS];

  generic_pattern_t trigPattern[NUM_VOICES];

  generic_pattern_t velPattern[NUM_VOICES];
  generic_pattern_t durPattern[NUM_VOICES];

  uint8_t filterPatternIdx[NUM_VOICES];

  arp_pattern_t arp[NUM_VOICES];

  uint8_t modPatternIdx;

  uint8_t octaveRange;
  int8_t octaveOffset[NUM_VOICES];

  uint8_t attenuatorIdx;

  uint8_t rnd[NUM_VOICES];

  uint8_t cycleMidiChannels;
  uint8_t midiChannelIdxOffset;
  uint8_t midiChannel[NUM_VOICES];

  uint8_t pulsesPerTrigIdx[NUM_VOICES];

  uint8_t masterPulseCount;
  uint8_t masterStepIdx;

  uint8_t velAccented;
  uint8_t vel;

  uint8_t durAccented;
  uint8_t dur;

  uint8_t midiListen;

  uint8_t pulsesPerBar;

  uint8_t trigMode;

  uint8_t repeatMode[NUM_VOICES];

} song_t;

song_t g_song = {
  0, 
  PPQN, 
  
  1, 
  120,
  0,
  4, 
  0,

  {0, 0, 0},
  
  {0, 0, 0},

  0, {{7, 4, 48}, {7, 4, 48}, {7, 4, 48}, {7, 4, 48}, {7, 4, 48}, {7, 4, 48}, {7, 4, 48}, {7, 4, 48}},

  {{1, 16}, {1, 16}, {1, 16}},
  {{0, 16}, {0, 16}, {0, 16}},
  {{0, 16}, {0, 16}, {0, 16}},

  {0, 0, 0},

  {0, 0, 0},

  0,
  
  3,
  {0, 0, 0},

  0,

  {0, 0, 0},

  0, 0, {1, 2, 3},

  {1, 1, 1},

  0, 0,

  127,90, PPQN * 2, 6,
 
  0, PPQN,
  TRIGMODE_NORMAL,

  {0,0,0}
};

uint8_t g_midiChannelIdx = 0;
uint8_t g_notesWritten = 0;

typedef struct {
  bool active;

  uint16_t pulseCount;
  uint8_t triggerShift;

  int16_t pendingNote;

  uint8_t note;
  uint8_t dur;
  uint8_t durArp;
  uint8_t durArpOrg;
  uint8_t vel;
  uint8_t durRemaining;

  uint8_t midiChannel;

  uint8_t stepIdx;

  uint8_t accentStepIdx;

  uint8_t attenuatorStepIdx;

  uint8_t arpStepIdx;
  uint8_t arpPulseCount;

  uint8_t modStepIdx;

  uint8_t filterStepIdx;

  uint8_t filterIdx;

  uint8_t velIdx;
  uint8_t durIdx;

  uint8_t repeatCnt;

} voice_t;

voice_t g_voices[NUM_VOICES] = {
  {false, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {false, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {false, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

uint8_t g_midiMessage[3] = {0, 0, 0};
uint8_t g_midiReceived = 0;
bool g_midiDataIgnore = false;

uint8_t g_midiChannel = 0;

uint8_t g_midiNoteActive = 0x40;
uint8_t g_midiNumActiveKeys = 0;

typedef struct {
  uint8_t col;
  uint8_t row;
} itemColRow_t;

#define GUISTATE_PAGEMAIN 0
#define GUISTATE_PAGESETTINGS 1
#define GUISTATE_PAGELOADSAVE 2

#ifdef TRACEPAGE
#define GUISTATE_PAGETRACE 3
#define GUISTATE_PAGELAST 4
#else
#define GUISTATE_PAGELAST 3
#endif

typedef struct {
  unsigned long timestamp;
  bool inversed;

  uint8_t state;
  itemColRow_t itemColRow[GUISTATE_PAGELAST];

  uint8_t stepIdx;
  uint8_t chordIdx;

  uint8_t propIdx;

  uint32_t playbackRender[32];
  uint8_t renderOffset;

  uint8_t previewDur;

  uint8_t btnPressedTicks;
  uint8_t btnState;

} gui_t;

gui_t g_gui = {
  0, false,
  0,
  {{1, 0}, {0, 0}, {0, 0}},
  0, 0, 0,
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  0, 0,
  0,
};

void setGuiState(uint8_t state);

#define BUTTON_PRESSED 1
#define ACTION_LEFT 2
#define ACTION_RIGHT 4
#define BUTTON_LONGPRESS 8

#define SETTING_PROPERTY_MASK_CHANNEL 0x03

#define SETTING_PROPERTY_MAX_RAND 0b00000100
#define SETTING_PROPERTY_MIN_OFF 0b00001000
#define SETTING_PROPERTY_SIGNED_VALUE 0b00010000
#define SETTING_PROPERTY_SET_TEMPO 0b00100000
#define SETTING_PROPERTY_SET_SYNC 0b01000000
#define SETTING_PROPERTY_MAX_ON 0b10000000

typedef struct setting_t {
  uint8_t properties;
  int8_t *pVal;
  int8_t minVal;
  int8_t maxVal;
  uint8_t flag;
};

#define SETTING_GROUP_TRIG 1
#define SETTING_GROUP_VEL 2
#define SETTING_GROUP_DUR 3
#define SETTING_GROUP_TPN 4
#define SETTING_GROUP_ARP 5
#define SETTING_GROUP_FLT 6
#define SETTING_GROUP_SCL 7
#define SETTING_GROUP_MOD 8

#define SETTING_GROUP_SEQ 9

#define SETTING_RANDOM 0b10000000

#define N_SETTINGS 54

#define N_SAVESLOTS 6

#define MAX_PULSESPERTRIG 25

#define N_REPEAT_MODES 2

setting_t settings[N_SETTINGS] = {
  {0, (int8_t*) &g_song.seqNumSteps, 1, 16, SETTING_GROUP_SEQ},
  {0, (int8_t*) &g_song.key, 0, 1, 0},

  {0 | SETTING_PROPERTY_MAX_RAND, (int8_t*) &g_song.pulsesPerBar, 3, MAX_PULSESPERTRIG, 0},

  {0 | SETTING_PROPERTY_MIN_OFF | SETTING_PROPERTY_MAX_ON, (int8_t*) &g_song.stepReset[0], 0, 1, 0},
  {0 | SETTING_PROPERTY_MIN_OFF | SETTING_PROPERTY_MAX_ON, (int8_t*) &g_song.stepReset[1], 0, 1, 0},
  {0 | SETTING_PROPERTY_MIN_OFF | SETTING_PROPERTY_MAX_ON, (int8_t*) &g_song.stepReset[2], 0, 1, 0},

  {0 | SETTING_PROPERTY_MIN_OFF | SETTING_PROPERTY_MAX_ON, (int8_t*) &g_song.swing[0], 0, 1, 0},
  {1 | SETTING_PROPERTY_MIN_OFF | SETTING_PROPERTY_MAX_ON, (int8_t*) &g_song.swing[1], 0, 1, 0},
  {2 | SETTING_PROPERTY_MIN_OFF | SETTING_PROPERTY_MAX_ON, (int8_t*) &g_song.swing[2], 0, 1, 0},

  {0, (int8_t*) &g_song.trigPattern[0].idx, 0, 37, SETTING_GROUP_TRIG | SETTING_RANDOM},
  {1, (int8_t*) &g_song.trigPattern[1].idx, 0, 37, SETTING_GROUP_TRIG | SETTING_RANDOM},
  {2, (int8_t*) &g_song.trigPattern[2].idx, 0, 37, SETTING_GROUP_TRIG | SETTING_RANDOM},

  {0, (int8_t*) &g_song.trigPattern[0].len, 1, 16, SETTING_GROUP_TRIG},
  {1, (int8_t*) &g_song.trigPattern[1].len, 1, 16, SETTING_GROUP_TRIG},
  {2, (int8_t*) &g_song.trigPattern[2].len, 1, 16, SETTING_GROUP_TRIG},

  {0, (int8_t*) &g_song.modPatternIdx, 0, 37, SETTING_GROUP_MOD | SETTING_RANDOM},

  {0 | SETTING_PROPERTY_MAX_RAND, (int8_t*) &g_song.velPattern[0].idx, 0, NUM_PATTERNS, SETTING_GROUP_VEL | SETTING_RANDOM},
  {1 | SETTING_PROPERTY_MAX_RAND, (int8_t*) &g_song.velPattern[1].idx, 0, NUM_PATTERNS, SETTING_GROUP_VEL | SETTING_RANDOM},
  {2 | SETTING_PROPERTY_MAX_RAND, (int8_t*) &g_song.velPattern[2].idx, 0, NUM_PATTERNS, SETTING_GROUP_VEL | SETTING_RANDOM},

  {0 | SETTING_PROPERTY_MAX_RAND, (int8_t*) &g_song.durPattern[0].idx, 0, NUM_PATTERNS, SETTING_GROUP_DUR | SETTING_RANDOM},
  {1 | SETTING_PROPERTY_MAX_RAND, (int8_t*) &g_song.durPattern[1].idx, 0, NUM_PATTERNS, SETTING_GROUP_DUR | SETTING_RANDOM},
  {2 | SETTING_PROPERTY_MAX_RAND, (int8_t*) &g_song.durPattern[2].idx, 0, NUM_PATTERNS, SETTING_GROUP_DUR | SETTING_RANDOM},

  {0 | SETTING_PROPERTY_MAX_RAND | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.filterPatternIdx[0], 0, N_FILTERPATTERNS, SETTING_GROUP_FLT | SETTING_RANDOM},
  {1 | SETTING_PROPERTY_MAX_RAND | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.filterPatternIdx[1], 0, N_FILTERPATTERNS, SETTING_GROUP_FLT | SETTING_RANDOM},
  {2 | SETTING_PROPERTY_MAX_RAND | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.filterPatternIdx[2], 0, N_FILTERPATTERNS, SETTING_GROUP_FLT | SETTING_RANDOM},

  {0 | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.arp[0].idx, 0, N_ARP_PATTERNS - 1, SETTING_GROUP_ARP | SETTING_RANDOM},
  {1 | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.arp[1].idx, 0, N_ARP_PATTERNS - 1, SETTING_GROUP_ARP | SETTING_RANDOM},
  {2 | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.arp[2].idx, 0, N_ARP_PATTERNS - 1, SETTING_GROUP_ARP | SETTING_RANDOM},

  {0 | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.repeatMode[0], 0, 10, SETTING_RANDOM},
  {1 | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.repeatMode[1], 0, 10, SETTING_RANDOM},
  {2 | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.repeatMode[2], 0, 10, SETTING_RANDOM},

  {0, (int8_t*) &g_song.octaveRange, 1, 4, 0},

  {0 | SETTING_PROPERTY_SIGNED_VALUE, &g_song.octaveOffset[0], -4, 4, 0},
  {1 | SETTING_PROPERTY_SIGNED_VALUE, &g_song.octaveOffset[1], -4, 4, 0},
  {2 | SETTING_PROPERTY_SIGNED_VALUE, &g_song.octaveOffset[2], -4, 4, 0},

  {0 | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.attenuatorIdx, 0, N_ATTENUATOR_PATTERNS - 1, SETTING_GROUP_SCL | SETTING_RANDOM},

  {0, (int8_t*) &g_song.vel, 1, 127, 0},
  {0, (int8_t*) &g_song.velAccented, 1, 127, 0},
  {0, (int8_t*) &g_song.dur, 0, 4 * PPQN, 0},
  {0, (int8_t*) &g_song.durAccented, 0, 4 * PPQN, 0},

  {0 | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.rnd[0], 0, 100, 0},
  {1 | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.rnd[1], 0, 100, 0},
  {2 | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.rnd[2], 0, 100, 0},

  {0 | SETTING_PROPERTY_MAX_RAND, (int8_t*) &g_song.pulsesPerTrigIdx[0], 0, NUM_TRIG_PULSES + 1, SETTING_GROUP_TPN | SETTING_RANDOM},
  {1 | SETTING_PROPERTY_MAX_RAND, (int8_t*) &g_song.pulsesPerTrigIdx[1], 0, NUM_TRIG_PULSES + 1, SETTING_GROUP_TPN | SETTING_RANDOM},
  {2 | SETTING_PROPERTY_MAX_RAND, (int8_t*) &g_song.pulsesPerTrigIdx[2], 0, NUM_TRIG_PULSES + 1, SETTING_GROUP_TPN | SETTING_RANDOM},

  {0 | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.midiChannel[0], 0, 16, 0}, 
  {1 | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.midiChannel[1], 0, 16, 0}, 
  {2 | SETTING_PROPERTY_MIN_OFF, (int8_t*) &g_song.midiChannel[2], 0, 16, 0}, 
  {0 | SETTING_PROPERTY_MIN_OFF | SETTING_PROPERTY_MAX_RAND, (int8_t*) &g_song.cycleMidiChannels, 0, NUM_VOICES + 2, 0},

  {0 | SETTING_PROPERTY_SET_SYNC | SETTING_PROPERTY_MIN_OFF | SETTING_PROPERTY_MAX_ON, (int8_t*) &g_song.sync, 0, 1, 0},
  {0 | SETTING_PROPERTY_SET_TEMPO, (int8_t*) &g_song.bpm, 40, 240, 0},

  {0, (int8_t*) &g_song.midiListen, 0, 16, 0},

  {0, (int8_t*) &g_song.trigMode, TRIGMODE_NORMAL, TRIGMODE_LAST, 0}

};

#define FONT MIDIBOY_FONT_5X7

#define GET_PROP_FLAG 0b10000000

typedef struct printStepCtx_t {
  uint8_t bitmapY;
  uint8_t screenY;
  uint8_t screenX;
  uint8_t printVal;

  uint8_t printBuf[4];
  uint8_t bufIdx;
  uint8_t propIdx;
};
printStepCtx_t g_printStepCtx[3] = { {8, 2, 0, 0, {0, 0, 0, 0}, 0, GET_PROP_FLAG}, {8, 4, 0, 0, {0, 0, 0, 0}, 0, GET_PROP_FLAG}, {8, 6, 0, 0, {0, 0, 0, 0}, 0, GET_PROP_FLAG}};

typedef struct printCtx_t {
  uint8_t *pText;
  uint8_t textIdx;
  uint8_t bitmapY;
  uint8_t screenY;
  uint8_t printVal;

  uint8_t printBuf[4];
};
uint8_t g_printBuf[4][14] = {{'A', 'A', 32, 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'B', 0, 0}, {'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'B', 0, 0}, {'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'B', 0, 0}, {'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'B', 0, 0}};

printCtx_t g_settingsPrintCtx[4] = {{&g_printBuf[0][0], 0, 0, 6, 0, {0, 0, 0, 0}}, {&g_printBuf[1][0], 0, 0, 4, 0, {0, 0, 0, 0}}, {&g_printBuf[2][0], 0, 0, 2, 0, {0, 0, 0, 0}}, {&g_printBuf[2][0], 0, 0, 6, 0, {0, 0, 0, 0}}};

void printStep(printStepCtx_t *pCtx, seq_step_t *pSeqStp, uint8_t stepIdx, uint8_t selPropIdx, uint8_t stepIdxPlaying);

void printString_P(const char *pText, bool inverted) {
  char v = pgm_read_byte(pText++);
  while (v != 0) {
    Midiboy.drawBitmap_P(&FONT::DATA_P[(v - ' ') * FONT::WIDTH], FONT::WIDTH, inverted);
    v = pgm_read_byte(pText++);
  }
}

void printString(const char *pText, bool inverted) {
  while (*pText != 0) {
    Midiboy.drawBitmap_P(&FONT::DATA_P[(*pText++ - ' ') * FONT::WIDTH], FONT::WIDTH, inverted);
  }
}

inline uint8_t *bytePropToString(uint8_t v, uint8_t a, uint8_t *dest) {
  uint8_t t = (v / 100);
  if (v < 100) {
    *dest++ = ' ';
  }
  if (v < 10) {
    *dest++ = ' ';
  }

  if (t > 0)
    *dest++ = '0' + t | a;
  v -= (t * 100);
  if (t > 0 || v > 9) {
    t = (v / 10);
    *dest++ = '0' + t | a;
  }
  t = (v - (v / 10 * 10));
  *dest++ = '0' + t | a;
  return dest;
}

inline uint8_t *stringPropToString(const char *text, uint8_t a, uint8_t *dest) {
  uint8_t i = 0;
  uint8_t c = pgm_read_byte(text++);
  while (c != 0) {
    *dest++ = c | a;
    c = pgm_read_byte(text++);
  }
  return dest;
}

void printByte(uint8_t v, bool inverted) {
  uint8_t t = (v / 100);
  if (t > 0)
    Midiboy.drawBitmap_P(&FONT::DATA_P[('0' + t - ' ') * FONT::WIDTH], FONT::WIDTH, inverted);
  v -= (t * 100);
  if (t > 0 || v > 9) {
    t = (v / 10);
    Midiboy.drawBitmap_P(&FONT::DATA_P[('0' + t - ' ') * FONT::WIDTH], FONT::WIDTH, inverted);
  }
  t = (v - (v / 10 * 10));
  Midiboy.drawBitmap_P(&FONT::DATA_P[('0' + t - ' ') * FONT::WIDTH], FONT::WIDTH, inverted);
}

void printByteSigned(int8_t v, bool inverted) {
  if (v > 0) {
    Midiboy.drawBitmap_P(&FONT::DATA_P[('+' - ' ') * FONT::WIDTH], FONT::WIDTH, inverted);
  } else if (v < 0) {
    Midiboy.drawBitmap_P(&FONT::DATA_P[('-' - ' ') * FONT::WIDTH], FONT::WIDTH, inverted);
    v = -v;
  }
  printByte(v, inverted);
}

#define INT16_BITS 16

inline uint16_t leftRotate(uint16_t n, uint16_t d) {
  return (n << d) | (n >> (INT16_BITS - d));
}

inline uint16_t rightRotate(uint16_t n, uint16_t d) {
  return (n >> d) | (n << (INT16_BITS - d));
}

enum { EEPROM_SAVE_START_ADDRESS = 20 };

inline void setTimer(uint8_t bpm) {
  OCR1A = (uint16_t) ((16.0 * (10e6)) / ((1.0 / 60.0) * (2 * (float)bpm * ((48.0) * 2.0)) * 64.0) + 0.5) - 3;
}

void enableTimer(uint8_t bpm) {
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  setTimer(bpm);

  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS11) | (1 << CS10);

  TIMSK1 |= (1 << OCIE1A);
  sei();
}

void disableTimer() {
  TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10));
}

void setup() {
  
  Midiboy.begin();
  Midiboy.setButtonRepeatMs(50);

  Midiboy.setDrawPosition(0, 0);

  disableTimer();

#ifdef TRACEPAGE
  g_trace.frameTime = millis();
#endif

  uint8_t i = 0;
  while (i < N_SETTINGS) {
    applySetting((const struct setting_t*) &settings[i]);
    i++;
  }
  
  printNotification(&NOTIF_WELCOME[0]);
  g_gui.previewDur = 32;

  randomSeed(millis());
}

ISR(TIMER1_COMPA_vect) {
  if (g_song.sync) {
    tick();
  }
}

#define VERSION_SAVE 1
#define LEN_SAVE 0

bool loadSong(uint8_t slotIdx) {

  if (slotIdx >= N_SAVESLOTS) {
    slotIdx = N_SAVESLOTS - 1;
  }

  uint16_t offset = slotIdx * 128;
  uint16_t d = offset;

  if (EEPROM.read(EEPROM_SAVE_START_ADDRESS + (d++)) == VERSION_SAVE) {
    EEPROM.read(EEPROM_SAVE_START_ADDRESS + (d++)); // Length

    if (EEPROM.read(EEPROM_SAVE_START_ADDRESS + (d++)) == NUM_SEQSTEPS) {
      uint8_t i = 0;
      while (i < NUM_SEQSTEPS) {
        g_song.seqSteps[i].chordIdx = EEPROM.read(EEPROM_SAVE_START_ADDRESS + (d++));
        g_song.seqSteps[i].duration = EEPROM.read(EEPROM_SAVE_START_ADDRESS + (d++));
        g_song.seqSteps[i].root = EEPROM.read(EEPROM_SAVE_START_ADDRESS + (d++));
        i++;
      }

      i = 0;
      while (i < N_SETTINGS) {
        *(settings[i].pVal) = EEPROM.read(EEPROM_SAVE_START_ADDRESS + (d++));
        i++;
      }

      while (i > 0) {
        i--;
        applySetting((const struct setting_t*) &settings[i]);
      }
      
      randomSeed(millis());
      
      return true;
    }

  }
  return false;
}

void saveSong(uint8_t slotIdx) {
  if (slotIdx >= N_SAVESLOTS) {
    slotIdx = N_SAVESLOTS - 1;
  }

  uint16_t offset = slotIdx * 128;
  uint16_t d = offset;
  EEPROM.update(EEPROM_SAVE_START_ADDRESS + (d++), VERSION_SAVE);

  d++; // Length

  EEPROM.update(EEPROM_SAVE_START_ADDRESS + (d++), NUM_SEQSTEPS);
  uint8_t i = 0;
  while (i < NUM_SEQSTEPS) {
    EEPROM.update(EEPROM_SAVE_START_ADDRESS + (d++), g_song.seqSteps[i].chordIdx);
    EEPROM.update(EEPROM_SAVE_START_ADDRESS + (d++), g_song.seqSteps[i].duration);
    EEPROM.update(EEPROM_SAVE_START_ADDRESS + (d++), g_song.seqSteps[i].root);
    i++;
  }

  i = 0;
  while (i < N_SETTINGS) {
    EEPROM.update(EEPROM_SAVE_START_ADDRESS + (d++),  *(settings[i].pVal));
    i++;
  }

  EEPROM.update(EEPROM_SAVE_START_ADDRESS + 1, d - offset);
}

void randomizeSettings() {
  uint8_t i = N_SETTINGS;
  while (i > 0) {
    i--;
    if (settings[i].flag & SETTING_RANDOM) {
      uint8_t flag = settings[i].flag ^ SETTING_RANDOM;
      uint8_t r;
      switch (flag) {
        case SETTING_GROUP_TRIG:
          r = random(1, settings[i].maxVal + 1);
          *(settings[i].pVal) = r;
          break;

        case SETTING_GROUP_TPN:
          *(settings[i].pVal) = 6;
          break;


        case SETTING_GROUP_ARP:
          r = random(settings[i].minVal, settings[i].maxVal + 1);
          *(settings[i].pVal) = r;
          break;

        default:
          r = random(settings[i].minVal, settings[i].maxVal + 1);
          *(settings[i].pVal) = r;
          break;
      }
    }
  }
}

void applySetting(const struct setting_t *pSetting) {
  
  if ((pSetting->flag & 0b01111111) == SETTING_GROUP_SEQ) {
    g_gui.itemColRow[GUISTATE_PAGEMAIN].row = 0;
  }
  
  if (pSetting->properties & SETTING_PROPERTY_SET_TEMPO) {
    if (g_song.sync) {
      setTimer(*pSetting->pVal);
    }
  } else if (pSetting->properties & SETTING_PROPERTY_SET_SYNC) {
    if (g_song.sync) {
      enableTimer(g_song.bpm);
    } else {
      disableTimer();
    }
  }
}

void incSetting(const struct setting_t *pSetting) {
  if (pSetting->properties & SETTING_PROPERTY_SIGNED_VALUE) {
    int8_t v = *pSetting->pVal;
    if (v < pSetting->maxVal) {
      v++;
      *pSetting->pVal = v;
    }
  } else {
    uint8_t v = *pSetting->pVal;
    if (v < (uint8_t)pSetting->maxVal) {
      v++;
      *pSetting->pVal = v;
    }
  }
  applySetting(pSetting);
}

void decSetting(const struct setting_t *pSetting) {
  if (pSetting->properties & SETTING_PROPERTY_SIGNED_VALUE) {
    int8_t v = *pSetting->pVal;
    if (v > pSetting->minVal) {
      v--;
      *pSetting->pVal = v;
    }
  } else {
    uint8_t v = *pSetting->pVal;
    if (v > (uint8_t)pSetting->minVal) {
      v--;
      *pSetting->pVal = v;
    }
  }
  applySetting(pSetting);
}

void setSetting(const struct setting_t *pSetting, int8_t v) {
  if (pSetting->properties & SETTING_PROPERTY_SIGNED_VALUE) {
    if (v < pSetting->minVal) {
      v = pSetting->minVal;
    } else if (v > pSetting->maxVal) {
      v = pSetting->maxVal;
    }
  } else {
    if ((uint8_t) v < (uint8_t) pSetting->minVal) {
      v = pSetting->minVal;
    } else if ((uint8_t) v > (uint8_t) pSetting->maxVal) {
      v = pSetting->maxVal;
    }
  }
  *pSetting->pVal = v;
  if (pSetting->properties & SETTING_PROPERTY_SET_TEMPO) {
    if (g_song.sync) {
      setTimer(*pSetting->pVal);
    }
  } else if (pSetting->properties & SETTING_PROPERTY_SET_SYNC) {
    if (g_song.sync) {
      enableTimer((float)g_song.bpm);
    } else {
      disableTimer();
    }
  }
}

void printNotification(const char *pText) {
  Midiboy.setDrawPosition(0, 0);
  printString_P(pText, false);
  Midiboy.drawSpace(128 - Midiboy.getDrawPositionX(), false);
  g_gui.previewDur = 8;
}

void stopPlayback() {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    g_song.state = SONG_STATE_IDLE;
    uint8_t i = NUM_VOICES;
    while (i > 0) {
      i--;
      if (g_voices[i].durRemaining) {
        Midiboy.dinMidi().write(0x80 | g_voices[i].midiChannel);
        Midiboy.dinMidi().write(g_voices[i].note);
        Midiboy.dinMidi().write((uint8_t)0x00);
        g_voices[i].durRemaining = 0;
      }
      Midiboy.dinMidi().write(0xB0 | g_voices[i].midiChannel);
      Midiboy.dinMidi().write(123);
      Midiboy.dinMidi().write((uint8_t)0x00);
    }

#ifdef SEND_MIDI_STOP
    Midiboy.dinMidi().write(0xFC);
#endif

  }
}

inline uint16_t getPulseCount() {
  if (g_song.pulsesPerBar != MAX_PULSESPERTRIG) {
    return(g_song.seqSteps[g_song.seqStepIdx].duration * g_song.pulsesPerBar - 1);
  } else {
    uint8_t r = pgm_read_word(&g_trigPulses[random(0, NUM_TRIG_PULSES - 1)]);
    return(g_song.seqSteps[g_song.seqStepIdx].duration * (uint16_t)r) - 1; // TODO: Check the -1 needed ?
  }
}

void startPlayback() {
  if (g_song.state != SONG_STATE_PLAYING) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
  
      g_song.pulseCount = 0xffff;
      g_song.stepPulseCount = g_song.pulseCount;
      
      g_song.seqStepIdx = 0xff;
  
      g_song.masterPulseCount = 0;
      g_song.masterStepIdx = 0;
  
      uint8_t i = NUM_VOICES;
      while (i > 0) {
        i--;
        g_voices[i].pulseCount = 0;
        g_voices[i].active = false;
        g_voices[i].stepIdx = 0;
        g_voices[i].arpStepIdx = 0;
        g_voices[i].accentStepIdx = 0;
        g_voices[i].modStepIdx = 0;
        g_voices[i].attenuatorStepIdx = 0;
        g_voices[i].filterStepIdx = 0;
  
        g_voices[i].triggerShift = 0;
  
        g_voices[i].midiChannel = g_song.midiChannel[i] - 1;
  
        g_voices[i].velIdx = (g_song.velPattern[i].idx == NUM_PATTERNS) ? random(0, NUM_PATTERNS - 1) : g_song.velPattern[i].idx;
        g_voices[i].durIdx = (g_song.durPattern[i].idx == NUM_PATTERNS) ? random(0, NUM_PATTERNS - 1) : g_song.durPattern[i].idx;
  
        g_voices[i].durArp = 0;
      }
      g_song.midiChannelIdxOffset = 0;
      g_song.state = SONG_STATE_PLAYING;
  
      g_midiChannelIdx = 0;
      g_notesWritten = 0;
      
  #ifdef SEND_MIDI_START
      Midiboy.dinMidi().write(0xFA);
  #endif
    }
  }
}

void setGuiState(uint8_t state) {
  if (state >= GUISTATE_PAGELAST) {
    state = 0;
  }
  if (g_gui.state != state) {
    g_gui.state = state;
    uint8_t i = NUM_VOICES;
    while (i > 0) {
      i--;
      g_settingsPrintCtx[i].textIdx = 0;
      g_settingsPrintCtx[i].printVal = false;
    }

    Midiboy.clearScreen();
    switch (state) {
      case GUISTATE_PAGEMAIN:
        break;

      case GUISTATE_PAGESETTINGS:
        break;

      case GUISTATE_PAGELOADSAVE:
        printNotification(&NOTIF_LOADSAVE[0]);
        break;

#ifdef TRACEPAGE
      case GUISTATE_PAGETRACE:
        break;
#endif
    }
  }
}

void incSeqItem(uint8_t col, uint8_t row) {
  switch (col) {
    case 1:
      if (g_song.seqSteps[row].chordIdx < 7) {
        g_song.seqSteps[row].chordIdx++;
      }
      break;
    case 2:
      if (g_song.seqSteps[row].duration < 64) {
        g_song.seqSteps[row].duration++;
      }
      break;
    case 3:
      if (g_song.seqSteps[row].root < 127) {
        g_song.seqSteps[row].root++;
      }
      break;
  }
}

void decSeqItem(uint8_t col, uint8_t row) {
  switch (col) {
    case 1:
      if (g_song.seqSteps[row].chordIdx > 0) {
        g_song.seqSteps[row].chordIdx--;
      }
      break;
    case 2:
      if (g_song.seqSteps[row].duration > 1) {
        g_song.seqSteps[row].duration--;
      }
      break;
    case 3:
      if (g_song.seqSteps[row].root > 20) {
        g_song.seqSteps[row].root--;
      }
      break;
  }
}

void setSeqItem(int8_t v) {
  uint8_t row = g_gui.itemColRow[g_gui.state].row;
  switch (g_gui.itemColRow[g_gui.state].col) {
    case 1:
      if (v < 0) {
        v = 0;
      }
      if (v > 7) {
        v = 7;
      }
      g_song.seqSteps[row].chordIdx = v;
      break;
    case 2:
      if (v < 1) {
        v = 1;
      }
      if (v > 64) {
        v = 64;
      }
      g_song.seqSteps[row].duration = v;
      break;
    case 3:
      if (v < 20) {
        v = 20;
      } else if (v > 127) {
        v = 127;
      }
      g_song.seqSteps[row].root = v;
      break;
  }
}

void setSelectedItem(int8_t v) {
  switch (g_gui.state) {
    case GUISTATE_PAGESETTINGS:
      setSetting(&settings[g_gui.itemColRow[g_gui.state].row], v);
      break;

    case GUISTATE_PAGEMAIN:
      setSeqItem(v);
      break;
  }
}

void decSelectedItem() {
  switch (g_gui.state) {
    case GUISTATE_PAGESETTINGS:
      decSetting(&settings[g_gui.itemColRow[g_gui.state].row]);
      break;

    case GUISTATE_PAGEMAIN:
      decSeqItem(g_gui.itemColRow[g_gui.state].col, g_gui.itemColRow[g_gui.state].row);
      break;
  }
}

void incSelectedItem() {
  switch (g_gui.state ) {
    case GUISTATE_PAGESETTINGS:
      incSetting(&settings[g_gui.itemColRow[g_gui.state].row]);
      break;

    case GUISTATE_PAGEMAIN:
      incSeqItem(g_gui.itemColRow[g_gui.state].col, g_gui.itemColRow[g_gui.state].row);
      break;
  }
}

uint8_t getGuiMaxRow() {
  switch (g_gui.state ) {
    case GUISTATE_PAGESETTINGS:
      return (N_SETTINGS - 1);
      break;

    case GUISTATE_PAGEMAIN:
      return (g_song.seqNumSteps - 1);
      break;

    case GUISTATE_PAGELOADSAVE:
      return (N_SAVESLOTS - 1);
      break;

    default:
      return (127);
      break;
  }
}

void setGuiRow(int8_t row) {
  if (row <= 0) {
    g_gui.itemColRow[g_gui.state].row = 0;
  } else {
    g_gui.itemColRow[g_gui.state].row = row;

    switch (g_gui.state ) {
      case GUISTATE_PAGESETTINGS:
        if (g_gui.itemColRow[g_gui.state].row > (N_SETTINGS - 1)) {
          g_gui.itemColRow[g_gui.state].row = (N_SETTINGS - 1);
        }
        break;

      case GUISTATE_PAGEMAIN:
        if (g_gui.itemColRow[g_gui.state].row > (g_song.seqNumSteps - 1)) {
          g_gui.itemColRow[g_gui.state].row = (g_song.seqNumSteps - 1);
        }
        break;

      case GUISTATE_PAGELOADSAVE:
        if (g_gui.itemColRow[g_gui.state].row > (N_SAVESLOTS - 1)) {
          g_gui.itemColRow[g_gui.state].row = (N_SAVESLOTS - 1);
        }
        break;
    }
  }
}

void setGuiCol(int8_t col) {
  switch (g_gui.state) {
    case GUISTATE_PAGEMAIN:
      if (col > 4) {
        g_gui.itemColRow[g_gui.state].col = 4;
      } else if (col < 1) {
        g_gui.itemColRow[g_gui.state].col = 1;
      } else {
        g_gui.itemColRow[g_gui.state].col = col;
      }
      break;
  }
}

void decGuiCol() {
  if (g_gui.state == GUISTATE_PAGEMAIN) {
    if (g_gui.itemColRow[g_gui.state].col < 2) {
      if (g_gui.itemColRow[g_gui.state].row > 0) {
        g_gui.itemColRow[g_gui.state].row--;
        g_gui.itemColRow[g_gui.state].col = 3;
      }
    } else {
      g_gui.itemColRow[g_gui.state].col--;
    }
  }
}


void incGuiCol() {
  if (g_gui.state == GUISTATE_PAGEMAIN) {
    if (g_gui.itemColRow[g_gui.state].col > 2) {
      if (g_gui.itemColRow[g_gui.state].row < (g_song.seqNumSteps - 1)) {
        g_gui.itemColRow[g_gui.state].row++;
        g_gui.itemColRow[g_gui.state].col = 1;
      }
    } else {
      g_gui.itemColRow[g_gui.state].col++;
    }
  }
}

void handleInput() {
  MidiboyInput::Event event;
  while (Midiboy.readInputEvent(event)) {

    if (event.m_type == MidiboyInput::EVENT_DOWN) {

      g_gui.btnState |= BUTTON_PRESSED;

      switch (event.m_button) {
        case MidiboyInput::BUTTON_RIGHT:
          if (g_gui.state == GUISTATE_PAGELOADSAVE) {
            if (!(g_gui.btnState & ACTION_RIGHT)) {
              g_gui.btnState |= BUTTON_LONGPRESS;
            }
          } else {
            incSelectedItem();
          }
          g_gui.btnState |= ACTION_RIGHT;
          break;

        case MidiboyInput::BUTTON_LEFT:
          if (g_gui.state == GUISTATE_PAGELOADSAVE) {
            if (!(g_gui.btnState & ACTION_LEFT)) {
              g_gui.btnState |= BUTTON_LONGPRESS;
            }
          } else {
            decSelectedItem();
          }
          g_gui.btnState |= ACTION_LEFT;
          break;

        case MidiboyInput::BUTTON_DOWN:
          if (g_gui.state == GUISTATE_PAGEMAIN) {
            incGuiCol();
          } else {
            setGuiRow(g_gui.itemColRow[g_gui.state].row + 1);
          }
          break;

        case MidiboyInput::BUTTON_UP:
          if (g_gui.state == GUISTATE_PAGEMAIN) {
            decGuiCol();
          } else {
            setGuiRow(g_gui.itemColRow[g_gui.state].row - 1);
          }
          break;

        case MidiboyInput::BUTTON_A:
          if (g_song.state != SONG_STATE_PLAYING) {
            if (!g_song.sync) {
              printNotification(&NOTIF_SYNC[0]);
            }
            startPlayback();
          } else {
            stopPlayback();
          }
          break;

        case MidiboyInput::BUTTON_B:
          setGuiState(g_gui.state + 1);
          break;
      }
    } else if (event.m_type == MidiboyInput::EVENT_UP) {

      if (g_gui.state == GUISTATE_PAGELOADSAVE && g_gui.btnState & BUTTON_LONGPRESS) {
        printNotification(&NOTIF_LOADSAVE[0]);
        g_gui.previewDur = 0;
      }

      g_gui.btnState &= (0xff ^ (BUTTON_PRESSED | BUTTON_LONGPRESS));

      switch (event.m_button) {
        case MidiboyInput::BUTTON_LEFT:
          g_gui.btnState &= (0xff ^ ACTION_LEFT);
          break;

        case MidiboyInput::BUTTON_RIGHT:
          g_gui.btnState &= (0xff ^ ACTION_RIGHT);
          break;
      }
    }
  }
}

void printHeader(struct printCtx_t *pCtx) {

  char c = pCtx->pText[pCtx->textIdx++];

  uint8_t *pFontBitmap = (uint8_t*) &FONT::DATA_P[(c - ' ') * FONT::WIDTH];

  Midiboy.setDrawPosition((pCtx->textIdx + pCtx->printVal * 9) * (FONT::WIDTH * 2), (pCtx->bitmapY / 4) + pCtx->screenY - 1);

  uint16_t bm = 0;

  uint8_t x = 0;
  while (x != FONT::WIDTH) {
    uint8_t b = pgm_read_byte(pFontBitmap + x);
    if (b & (1 << pCtx->bitmapY - 4)) {
      bm = 0b0000001100000011;
    } else {
      bm = 0;
    }
    if (b & (1 << (pCtx->bitmapY - 3))) {
      bm |= 0b0000110000001100;
    }
    if (b & (1 << (pCtx->bitmapY - 2))) {
      bm |= 0b0011000000110000;
    }
    if (b & (1 << (pCtx->bitmapY - 1))) {
      bm |= 0b1100000011000000;
    }
    Midiboy.drawBitmap((uint8_t *)&bm, 2, false);
    x++;
  }
  if (pCtx->bitmapY <= 4) {
    pCtx->bitmapY = 8;
    pCtx->textIdx++;
    if (pCtx->pText[pCtx->textIdx] == 0) {
      pCtx->textIdx = 0;
    }
  } else {
    pCtx->bitmapY -= 4;
  }
}

void printSaveslot(struct printCtx_t *pCtx, uint8_t slotIdx, bool inverted) {

  char c;
  if (!pCtx->printVal) {
    c = pgm_read_byte(LABEL_SAVESLOT + pCtx->textIdx);
  } else {
    c = pCtx->printBuf[pCtx->textIdx];
  }

  uint8_t *pFontBitmap = (uint8_t*) &FONT::DATA_P[(c - ' ') * FONT::WIDTH];

  Midiboy.setDrawPosition((pCtx->textIdx + pCtx->printVal * 9.8) * (FONT::WIDTH * 2), (pCtx->bitmapY / 4) + pCtx->screenY - 1);

  uint16_t bm = 0;

  uint8_t x = 0;
  while (x != FONT::WIDTH) {
    uint8_t b = pgm_read_byte(pFontBitmap + x);
    if (b & (1 << pCtx->bitmapY - 4)) {
      bm = 0b0000001100000011;
    } else {
      bm = 0;
    }
    if (b & (1 << (pCtx->bitmapY - 3))) {
      bm |= 0b0000110000001100;
    }
    if (b & (1 << (pCtx->bitmapY - 2))) {
      bm |= 0b0011000000110000;
    }
    if (b & (1 << (pCtx->bitmapY - 1))) {
      bm |= 0b1100000011000000;
    }
    Midiboy.drawBitmap((uint8_t *)&bm, 2, inverted);
    x++;
  }
  if (pCtx->bitmapY <= 4) {
    pCtx->bitmapY = 8;
    pCtx->textIdx++;
    if (!pCtx->printVal) {
      if (pgm_read_byte(LABEL_SAVESLOT + pCtx->textIdx) == 0) {
        *bytePropToString(slotIdx, 0, &pCtx->printBuf[0]) = 0;
        pCtx->printVal = true;
        pCtx->textIdx = 0;
      }
    } else if (pCtx->printBuf[pCtx->textIdx] == 0) {
      pCtx->textIdx = 0;
      pCtx->printVal = false;
    }
  } else {
    pCtx->bitmapY -= 4;
  }
}

void printSetting(uint8_t settingIdx, struct printCtx_t *pCtx, const struct setting_t *pSetting, bool inverted) {

  char c;
  if (!pCtx->printVal) {
    c = pgm_read_byte(&LABELS_SETTINGS[settingIdx][0] + pCtx->textIdx);
  } else {
    c = pCtx->printBuf[pCtx->textIdx];
  }

  uint8_t *pFontBitmap = (uint8_t*) &FONT::DATA_P[(c - ' ') * FONT::WIDTH];

  Midiboy.setDrawPosition((pCtx->textIdx + pCtx->printVal * 9) * (FONT::WIDTH * 2), (pCtx->bitmapY / 4) + pCtx->screenY - 1);

  uint16_t bm = 0;

  uint8_t x = 0;
  while (x != FONT::WIDTH) {
    uint8_t b = pgm_read_byte(pFontBitmap + x);
    if (b & (1 << pCtx->bitmapY - 4)) {
      bm = 0b0000001100000011;
    } else {
      bm = 0;
    }
    if (b & (1 << (pCtx->bitmapY - 3))) {
      bm |= 0b0000110000001100;
    }
    if (b & (1 << (pCtx->bitmapY - 2))) {
      bm |= 0b0011000000110000;
    }
    if (b & (1 << (pCtx->bitmapY - 1))) {
      bm |= 0b1100000011000000;
    }
    Midiboy.drawBitmap((uint8_t *)&bm, 2, inverted);
    x++;
  }
  if (pCtx->bitmapY <= 4) {
    pCtx->bitmapY = 8;
    pCtx->textIdx++;
    if (!pCtx->printVal) {

      if (pgm_read_byte(&LABELS_SETTINGS[settingIdx][0] + pCtx->textIdx) == 0) {
        if (pSetting->pVal) {
          if (pSetting->properties & SETTING_PROPERTY_MAX_ON && *pSetting->pVal == pSetting->maxVal) {
            *stringPropToString(&LABEL_ON[0], 0, &pCtx->printBuf[0]) = 0;
          } else if (pSetting->properties & SETTING_PROPERTY_MIN_OFF && *pSetting->pVal == pSetting->minVal) {
            *stringPropToString(&LABEL_OFF[0], 0, &pCtx->printBuf[0]) = 0;
          } else if (pSetting->properties & SETTING_PROPERTY_MAX_RAND && *pSetting->pVal == pSetting->maxVal) {
            *stringPropToString(&LABEL_RND[0], 0, &pCtx->printBuf[0]) = 0;
          } else if (pSetting->properties & SETTING_PROPERTY_SIGNED_VALUE) {
            if (*pSetting->pVal < 0) {
              *bytePropToString(-(*pSetting->pVal), 0, &pCtx->printBuf[0]) = 0;
              pCtx->printBuf[0] = '-';
            } else {
              *bytePropToString(*pSetting->pVal, 0, &pCtx->printBuf[0]) = 0;
            }
          } else {
            *bytePropToString(*pSetting->pVal, 0, &pCtx->printBuf[0]) = 0;
          }
          pCtx->printVal = true;
        }
        pCtx->textIdx = 0;
      }
    } else if (pCtx->printBuf[pCtx->textIdx] == 0) {
      pCtx->textIdx = 0;
      pCtx->printVal = false;
    }
  } else {
    pCtx->bitmapY -= 4;
  }
}

void printStep(printStepCtx_t *pCtx, seq_step_t *pSeqStp, uint8_t stepIdx, uint8_t selPropIdx, uint8_t stepIdxPlaying) {

  uint8_t propIdx = pCtx->propIdx & 0b01111111;

  bool inverted = false;

  if (pCtx->propIdx & GET_PROP_FLAG) {

    switch (propIdx) {
      case 0:
        *(bytePropToString(stepIdx + 1, 0, &pCtx->printBuf[0])) = 0;
        pCtx->propIdx = propIdx + 1;
        pCtx->screenX = 0;
        break;
      case 1:
        if (propIdx == selPropIdx) {
          inverted = true;
        }

        Midiboy.setDrawPosition(pCtx->screenX * (FONT::WIDTH * 2) + 2, pCtx->screenY);
        Midiboy.drawBitmap_P(BITMAP_CHORDS[g_song.key & 1][pSeqStp->chordIdx * 2], 16, inverted);

        Midiboy.setDrawPosition(pCtx->screenX * (FONT::WIDTH * 2) + 2, pCtx->screenY + 1);
        Midiboy.drawBitmap_P(BITMAP_CHORDS[g_song.key & 1][pSeqStp->chordIdx * 2 + 1], 16, inverted);

        pCtx->propIdx = (propIdx + 1) | GET_PROP_FLAG;
        pCtx->screenX++;
        break;

      case 2:
        *(bytePropToString(pSeqStp->duration, 0, &pCtx->printBuf[0])) = 0;
        pCtx->propIdx = propIdx + 1;
        pCtx->screenX++;
        break;

      case 3:
        *(notePropToString(pSeqStp->root, 0, &pCtx->printBuf[0])) = 0;
        pCtx->propIdx = propIdx + 1;
        pCtx->screenX++;
        break;

      default:
        pCtx->propIdx = GET_PROP_FLAG;
        return;
    }
    pCtx->bufIdx = 0;
    if (pCtx->propIdx & GET_PROP_FLAG) {
      return; // Compiler workaround
    }

  }

  if (pCtx->propIdx == 1 && stepIdxPlaying == stepIdx) {
    inverted = true;
  }

  if (pCtx->propIdx > 1 && selPropIdx && (pCtx->propIdx - 1) == selPropIdx) {
    inverted = true;
  }


  char c = pCtx->printBuf[pCtx->bufIdx];
  if (!c) {
    pCtx->propIdx |= GET_PROP_FLAG;
    return;
  }

  uint8_t *pFontBitmap = (uint8_t*) &FONT::DATA_P[(c - ' ') * FONT::WIDTH];

  Midiboy.setDrawPosition(pCtx->screenX * (FONT::WIDTH * 2), (pCtx->bitmapY / 4) + pCtx->screenY - 1);

  uint16_t bm = 0;

  uint8_t x = 0;
  while (x != FONT::WIDTH) {
    uint8_t b = pgm_read_byte(pFontBitmap + x);
    if (b & (1 << pCtx->bitmapY - 4)) {
      bm = 0b0000001100000011;
    } else {
      bm = 0;
    }
    if (b & (1 << (pCtx->bitmapY - 3))) {
      bm |= 0b0000110000001100;
    }
    if (b & (1 << (pCtx->bitmapY - 2))) {
      bm |= 0b0011000000110000;
    }
    if (b & (1 << (pCtx->bitmapY - 1))) {
      bm |= 0b1100000011000000;
    }
    Midiboy.drawBitmap((uint8_t *)&bm, 2, inverted);
    x++;
  }
  if (pCtx->bitmapY <= 4) {
    pCtx->bitmapY = 8;
    pCtx->bufIdx++;
    pCtx->screenX++;

  } else {
    pCtx->bitmapY -= 4;
  }
}

inline uint8_t getMidiChannel(bool getFixed) {
  uint8_t channel = 0;

  if (getFixed) {
    channel = g_song.midiChannel[g_song.cycleMidiChannels - 1] - 1;
    g_midiChannelIdx++;
    if (g_midiChannelIdx >= NUM_VOICES) {
      g_midiChannelIdx = 0;  
    }
  } else if (g_song.cycleMidiChannels == (NUM_VOICES + 2)) {
    if (random(0, 2) & 1) {
      if (g_midiChannelIdx == (NUM_VOICES - 1)) {
        g_midiChannelIdx = 0;    
      } else {
        g_midiChannelIdx++;  
      }
    } else {
      if (g_midiChannelIdx == 0) {
        g_midiChannelIdx = (NUM_VOICES - 1);    
      } else {
        g_midiChannelIdx--;  
      }
    }
    channel = g_song.midiChannel[g_midiChannelIdx] - 1;
  } else {
    if (g_midiChannelIdx == g_song.cycleMidiChannels - 1) {
      g_midiChannelIdx++;
      if (g_midiChannelIdx >= NUM_VOICES) {
        g_midiChannelIdx = 0;  
      }
    }
    channel = g_song.midiChannel[g_midiChannelIdx] - 1;
    g_midiChannelIdx++;
    if (g_midiChannelIdx >= NUM_VOICES) {
      g_midiChannelIdx = 0;  
    }
  }
  return channel;  
}

void tick() {

  if (g_song.state == SONG_STATE_IDLE) {
    return;
  }

#ifdef SEND_MIDI_CLOCK
  Midiboy.dinMidi().write(0xF8);
#endif

  if (g_song.trigMode != TRIGMODE_NORMAL && !g_midiNumActiveKeys || (g_song.trigMode == TRIGMODE_KEY_ADVANCE && !g_song.pulseCount)) {
    return;
  }

  g_gui.renderOffset++;
  if (g_gui.renderOffset >= 32) {
    g_gui.renderOffset = 0;
  }

  voice_t *pVoice;

  if (g_song.pulseCount >= g_song.stepPulseCount) {
    g_song.seqStepIdx++;
    if (g_song.seqStepIdx >= g_song.seqNumSteps) {
      g_song.seqStepIdx = 0;
    }

    uint8_t baseNote = g_midiNoteActive;
    if (g_song.seqSteps[g_song.seqStepIdx].root > 20) {
      baseNote = g_song.seqSteps[g_song.seqStepIdx].root;
    }
    uint8_t rootNote = baseNote % 12;

    uint8_t chordIdx = g_song.seqSteps[g_song.seqStepIdx].chordIdx;
    if (chordIdx < 7) {
      uint8_t pendingNote = pgm_read_byte(&(g_chord_chart[g_song.key][rootNote][chordIdx]));
      chordIdx = (pendingNote >> 4) & 0x0f;
      pendingNote = (pendingNote & 0x0f);
      if (pendingNote < rootNote) {
        pendingNote += 12;
      }
      baseNote = pendingNote + ((baseNote / 12)) * 12;

      uint8_t voiceIdx = 3;
      while (voiceIdx > 0) {
        voiceIdx--;
        pVoice = &g_voices[voiceIdx];
        if (g_song.midiChannel[voiceIdx]) {
          int8_t ofs = g_chordOffsets[chordIdx][voiceIdx];
          int16_t note = baseNote + ofs;
          pVoice->active = true;

          pVoice->pendingNote = note;

          if (g_song.stepReset[voiceIdx]) {
            pVoice->stepIdx = 0;
            pVoice->modStepIdx = 0;

            pVoice->attenuatorStepIdx = 0;
          }
        }
      }
    } else {
      uint8_t voiceIdx = 3;
      while (voiceIdx > 0) {
        voiceIdx--;
        g_voices[voiceIdx].active = false;
      }
    }

    g_song.stepPulseCount = getPulseCount();
    g_song.pulseCount = 0;
 
  } else {
    g_song.pulseCount++;
  }
   
  uint8_t voiceIdx = NUM_VOICES;

  while (voiceIdx > 0) {   
    uint8_t write_note = false;
    voiceIdx--;

    pVoice = &g_voices[voiceIdx];

    uint8_t pulseCount;
    if (g_song.pulsesPerTrigIdx[voiceIdx] != (NUM_TRIG_PULSES + 1)) {
      pulseCount = pgm_read_word(&g_trigPulses[g_song.pulsesPerTrigIdx[voiceIdx]]);
    } else {
      pulseCount = pgm_read_word(&g_trigPulses[random(1, NUM_TRIG_PULSES - 1)]);
    }

    if (pVoice->repeatCnt) {
      pVoice->arpPulseCount--;
      if (!pVoice->arpPulseCount) {
        write_note = true;
        pVoice->repeatCnt--;
        if (pVoice->repeatCnt) {
          pVoice->arpPulseCount = pVoice->durArp;
        }
      }
    }

    if (pVoice->dur != 0xff && pVoice->durRemaining) {
      pVoice->durRemaining--;
      if (!pVoice->durRemaining) {
        Midiboy.dinMidi().write(0x80 | pVoice->midiChannel);
        Midiboy.dinMidi().write(pVoice->note);
        Midiboy.dinMidi().write((uint8_t)0x00);
      }
    }

    if (!pVoice->pulseCount) {

      if (pVoice->active) {

        uint16_t mod = pgm_read_word(&g_ptrns[g_song.trigPattern[voiceIdx].idx]);
        uint16_t stp = (0b1000000000000000 >> pVoice->stepIdx);

        mod ^= leftRotate(pgm_read_word(&g_ptrns[g_song.modPatternIdx]), pVoice->modStepIdx);

#ifdef TRACEPAGE
        g_trace.curTrigPtrn[voiceIdx] = mod;
        g_trace.curStep[voiceIdx] = stp;
#endif

        if (mod & stp && (pgm_read_word(&g_fltrPtrns[pVoice->filterIdx]) & (0b1000000000000000 >> pVoice->filterStepIdx))) {

          stp = (0b1000000000000000 >> pVoice->accentStepIdx);

          if (g_song.swing[voiceIdx] && (g_song.masterStepIdx & 1)) {
            pVoice->triggerShift = 1;
          } else {
            pVoice->triggerShift = 0;
            write_note = true;
          }

          if (g_song.repeatMode[voiceIdx] && random(1, 11) < g_song.repeatMode[voiceIdx]) {
            pVoice->durArp = 3 * random(1, 3);
            pVoice->arpPulseCount = pVoice->durArp;
            pVoice->durArp -= pVoice->triggerShift;
            pVoice->dur = pVoice->durArp > 1;
            pVoice->repeatCnt = random(1, 3);
            
          } else {
            uint8_t v = pgm_read_word(&g_ptrns[pVoice->durIdx]) & stp ? g_song.durAccented : g_song.dur;
            pVoice->dur = v ? v - pVoice->triggerShift : 0xff;
            pVoice->durArp = 0;
            pVoice->repeatCnt = 0;
          }

          pVoice->vel = pgm_read_word(&g_ptrns[pVoice->velIdx]) & stp ? g_song.velAccented : g_song.vel;

          pVoice->accentStepIdx++;
          if (pVoice->accentStepIdx >= 16) {

            pVoice->velIdx = (g_song.velPattern[voiceIdx].idx == NUM_PATTERNS) ? random(0, NUM_PATTERNS - 1) : g_song.velPattern[voiceIdx].idx;
            pVoice->durIdx = (g_song.durPattern[voiceIdx].idx == NUM_PATTERNS) ? random(0, NUM_PATTERNS - 1) : g_song.durPattern[voiceIdx].idx;

            pVoice->accentStepIdx = 0;

          }
          g_notesWritten++;
        }
      }

      pVoice->filterStepIdx++;
      if (pVoice->filterStepIdx >= 16) {
        pVoice->filterIdx = (g_song.filterPatternIdx[voiceIdx] == N_FILTERPATTERNS) ? random(0, N_FILTERPATTERNS - 1) : g_song.filterPatternIdx[voiceIdx];
        pVoice->filterStepIdx = 0;
      }

      pVoice->stepIdx++;
      if (pVoice->stepIdx >= g_song.trigPattern[voiceIdx].len) {
        pVoice->stepIdx = 0;
        pVoice->modStepIdx++;
        if (pVoice->modStepIdx >= 16) {
          pVoice->modStepIdx = 0;
        }
      }
      pVoice->pulseCount = pulseCount - 1;

    } else {
      pVoice->pulseCount = pVoice->pulseCount - 1;
      if (pVoice->triggerShift) {
        pVoice->triggerShift--;
        if (!pVoice->triggerShift) {
          write_note = true;
        }
      }
    }

    if (write_note) {
      if (pVoice->durRemaining) {
        Midiboy.dinMidi().write(0x80 | pVoice->midiChannel);
        Midiboy.dinMidi().write(pVoice->note);
        Midiboy.dinMidi().write((uint8_t)0x00);
        pVoice->durRemaining = 0;
      }

      uint8_t c = g_song.midiChannel[voiceIdx] - 1;
      if (g_song.cycleMidiChannels) {
        c = getMidiChannel((g_song.cycleMidiChannels - 1) == voiceIdx);
      }
      if (c != pVoice->midiChannel) {
        uint8_t t = 3;
        while (t > 0) {
          t--;
          if (g_voices[t].durRemaining && g_voices[t].midiChannel == c) {
            Midiboy.dinMidi().write(0x80 | g_voices[t].midiChannel);
            Midiboy.dinMidi().write(g_voices[t].note);
            Midiboy.dinMidi().write((uint8_t)0x00);
            g_voices[t].durRemaining = 0;
          }
        }
      }
      pVoice->midiChannel = c;

      int16_t note = pVoice->pendingNote;

      int8_t octaveOffset = pgm_read_byte(g_arp[g_song.arp[voiceIdx].idx].pPtrn + pVoice->arpStepIdx);

      if (!pVoice->repeatCnt) {
        pVoice->arpStepIdx++;
        if (pVoice->arpStepIdx >= g_arp[g_song.arp[voiceIdx].idx].len) {
          pVoice->arpStepIdx = 0;
        }
      }

      if (octaveOffset > g_song.octaveRange) {
        octaveOffset = g_song.octaveRange;
      } else if (octaveOffset < -g_song.octaveRange) {
        octaveOffset = -g_song.octaveRange;
      }

      note += (octaveOffset + g_song.octaveOffset[voiceIdx]) * 12;
      if (note >= 21 && note <= 127) {
        pVoice->note = note;

        float scl = ((float)pgm_read_byte(g_attenuator[g_song.attenuatorIdx].pPattern + pVoice->attenuatorStepIdx) / 100.0);

        float v = pVoice->dur;
        if (v != 0xff) {
          v = v * scl;
          if (v < 1) {
            v = 1;
          }

          if (g_song.rnd[voiceIdx]) {
            float r = random(0, g_song.rnd[voiceIdx]);
            v = v - (uint8_t)(v * (r / 100.0));
            if (v < 1) {
              v = 1;
            }
          }
        }
        pVoice->dur = v;
        pVoice->durRemaining = v;

        v = pVoice->vel;

        v = v * scl;

        if (v < 1) {
          v = 1;
        }

        if (g_song.rnd[voiceIdx]) {
          float r = random(0, g_song.rnd[voiceIdx]);
          v = v - (uint8_t)(v * (r / 100.0));
          if (v < 1) {
            v = 1;
          }
        }

        pVoice->vel = v;

        Midiboy.dinMidi().write(0x90 | pVoice->midiChannel);
        Midiboy.dinMidi().write(pVoice->note);
        Midiboy.dinMidi().write(pVoice->vel);

#ifdef TRACEPAGE
        g_trace.midiChnls[voiceIdx] = pVoice->midiChannel | TRACEFLAG_CONSUME;
#endif

        pVoice->attenuatorStepIdx++;
        if (pVoice->attenuatorStepIdx >= g_attenuator[g_song.attenuatorIdx].len) {
          pVoice->attenuatorStepIdx = 0;
        }

      } else {
        pVoice->durRemaining = 0;
      }
    }

  } 
  
  if (g_song.cycleMidiChannels == 4 && (g_notesWritten >= 3)) {
    g_midiChannelIdx++;
    if (g_midiChannelIdx >= NUM_VOICES) {
      g_midiChannelIdx = 0;  
    }
  }
  g_notesWritten = 0;

  g_song.masterPulseCount++;
  if (g_song.masterPulseCount > 5) {
    g_song.masterPulseCount = 0;
    g_song.masterStepIdx++;
  }

  Midiboy.dinMidi().flush();

  g_renderPlayback = true;
}

void setStep(uint8_t s, uint8_t v) {
  if (v >= 7) {
    v = 6;
  }
  g_song.seqSteps[s].chordIdx = v;
}


void pollMidi() {
  while (Midiboy.dinMidi().available()) {
    uint8_t b = Midiboy.dinMidi().read();
    if (b & 0x80) {
      g_midiDataIgnore = true;
      switch(b) {
        case 0xfa:
          startPlayback();
          break;
  
        case 0xfc:
          g_midiNumActiveKeys = 0;
          stopPlayback();
          break;
  
        case 0xf8:
          if (!g_song.sync) {
            tick();
          }
          break;
  
        default:
          g_midiReceived = 0;
          g_midiDataIgnore = false;
          break;
      }
    } 
    
    if (g_midiDataIgnore == false) {
      g_midiMessage[g_midiReceived++] = b;
      if (g_midiReceived >= 3) { // NB; Midi running not handled properly
        if (g_midiMessage[1] && (!g_song.midiListen || ((g_midiMessage[0] & 0x0f) == (g_song.midiListen - 1)))) {
          if ((g_midiMessage[0] & 0xf0) == 0x90) {
            g_midiNoteActive = g_midiMessage[1];
            if (g_midiNumActiveKeys < 0xff) {
              g_midiNumActiveKeys++;
            }
            if (g_song.trigMode != TRIGMODE_NORMAL) {
              g_song.pulseCount = 0xffff;
            }
          } else if ((g_midiMessage[0] & 0xf0) == 0x80) {
            if (g_midiNumActiveKeys > 0) {
              g_midiNumActiveKeys--;
            }
          }
        }
        g_midiDataIgnore = true;
        g_midiReceived = 0;
      }
    }
  }
}

void renderSequence() {

  uint8_t stepOffset = 0;
  if (g_gui.itemColRow[g_gui.state].row > 2) {
    stepOffset = (g_gui.itemColRow[g_gui.state].row - 2);
  }
  uint8_t x = 0;
  uint8_t i = 3;
  uint8_t stepIdx = stepOffset;
  while (i > 0 && stepIdx < g_song.seqNumSteps) {
    i--;
    if (g_renderSection == i) {
      bool inv = false ^ (stepIdx == g_gui.itemColRow[g_gui.state].row);
      uint8_t selPropIdx = inv ? g_gui.itemColRow[g_gui.state].col : 0;
      printStep(&g_printStepCtx[i], &g_song.seqSteps[stepIdx], stepIdx, selPropIdx, g_song.seqStepIdx);
    }
    stepIdx++;
  }
}

void renderSaveSlots() {
  uint8_t slotOffset = 0;
  if (g_gui.itemColRow[g_gui.state].row > 2) {
    slotOffset = (g_gui.itemColRow[g_gui.state].row - 2);
  }
  uint8_t i = 0;
  while (i < 3) {
    if (g_renderSection == i) {
      printSaveslot(&g_settingsPrintCtx[i], slotOffset + i, slotOffset + i == g_gui.itemColRow[g_gui.state].row);
    }
    i++;
  }
}

void renderSettings() {
  uint8_t settingsOfst = 0;
  if (g_gui.itemColRow[g_gui.state].row > 2) {
    settingsOfst = (g_gui.itemColRow[g_gui.state].row - 2);
  }
  uint8_t i = 0;
  while (i < 3) {
    if (g_renderSection == i) {
      printSetting(settingsOfst + i, &g_settingsPrintCtx[i], &settings[settingsOfst + i], settingsOfst + i == g_gui.itemColRow[g_gui.state].row);
    }
    i++;
  }
}

void renderPress() {
  Midiboy.setDrawPosition(0, 0);
  uint8_t i = 0;
  while (i < 16) {
    Midiboy.drawSpace(8, i < g_gui.btnPressedTicks);
    i++;
  }
}

inline void renderPlayhead(uint8_t curIdx, uint8_t res) {
  Midiboy.setDrawPosition(0, 1);
  if (g_song.state == SONG_STATE_PLAYING && curIdx != 0xff) {
    uint8_t m = 128 / res;
    uint32_t renderBitmap = 0x0f;

    if (curIdx) {
      Midiboy.drawSpace(curIdx * m, false);
    }
    Midiboy.drawBitmap(&renderBitmap, m >> 1, false);
    Midiboy.drawSpace(m >> 1, false);
    curIdx++;
    if (curIdx < res) {
      Midiboy.drawSpace((res - curIdx) * m, false);
    }
  } else {
    Midiboy.drawSpace(128, false);
  }
}

void renderPattern(uint16_t ptrn, uint8_t len, uint8_t curIdx) {
  if (curIdx != 0xff) {
    curIdx--;
    if (curIdx >= len) {
      curIdx = (len - 1);
    }
  }
  renderPlayhead(curIdx, 16);
  
  Midiboy.setDrawPosition(0, 0);
  uint8_t i = 0;
  while (i < 16) {
    uint32_t renderBitmap = 0;
    if (ptrn & (0b1000000000000000 >> i)) {
      if (i <= len) {
        renderBitmap |= 0b00111100001111000011110000111100;
      } else {
        renderBitmap |= 0b00101000000101000010100000010100;
      }
    }
    uint8_t inversed = true;
    if (i != 0 && !(i % 4)) {
      uint32_t tmp = (renderBitmap | 0x00ff);
      Midiboy.drawBitmap(&tmp, 4, inversed);
    } else {
      if (i == curIdx && g_song.state == SONG_STATE_PLAYING) {
        uint32_t tmp = (renderBitmap | 0x00ff);
        Midiboy.drawBitmap(&tmp, 4, inversed);
      } else {
        Midiboy.drawBitmap(&renderBitmap, 4, inversed);
      }
    }
    Midiboy.drawBitmap(&renderBitmap, 4, inversed);
    i++;
  }
}


void renderPlayback() {
  if (g_song.state == SONG_STATE_PLAYING) {
    uint8_t i = (((float)g_song.pulseCount / (float)(g_song.stepPulseCount)) * 32 + 0.5);    
    if (i > 31) { // 32
      i = 31;
      i = 0;
    }
    renderPlayhead(i, 32);
    Midiboy.setDrawPosition(0, 0);

    uint32_t renderBitmap = 0;
    i = 3;
    while (i > 0) {
      i--;
      if (g_voices[i].durRemaining) {
        if (g_voices[i].durRemaining == g_voices[i].dur) {
          renderBitmap |= 0b00000110000001100000011000000000 << (i * 2);
        } else {
          renderBitmap |= 0b00000110000001100000011000000110 << (i * 2);
        }
      }
    }
    g_gui.playbackRender[g_gui.renderOffset] = renderBitmap;
    if (g_song.pulseCount % PPQN == 0) {
      g_gui.playbackRender[g_gui.renderOffset] |= 0xff;
    }
    i = 32;
    while (i > 0) {
      i--;
      Midiboy.drawBitmap(&g_gui.playbackRender[(g_gui.renderOffset - i) & 0x1f], 4, true);
    }
  } else {
    Midiboy.setDrawPosition(0, 0);
    Midiboy.drawSpace(128, true);
  }
}

#ifdef TRACEPAGE
void renderTrace() {
  uint8_t i = 3;
  uint8_t b[4] = {0, 0, 0, 0};

  while (i > 0) {
    i--;
    uint8_t t = 16;
    Midiboy.setDrawPosition(0, i + 4);
    while (t > 0) {
      t--;
      if (g_trace.curTrigPtrn[i] & (0b1000000000000000 >> t)) {
        Midiboy.drawSpace(4, true);
      } else {
        Midiboy.drawSpace(4, false);
      }
    }

    t = 16;
    while (t > 0) {
      t--;
      if (g_trace.curStep[i] & (0b1000000000000000 >> t)) {
        Midiboy.drawSpace(4, true);
      } else {
        Midiboy.drawSpace(4, false);
      }
    }

    Midiboy.setDrawPosition(0, i);

    float v = (((float)g_voices[i].durRemaining / (4.0 * PPQN)) * PPQN);
    if (v > 0) {
      Midiboy.drawSpace((uint16_t)v, true);
    }
    if (v < (24)) {
      Midiboy.drawSpace((24) - (uint16_t)v, false);
    }

    if (g_trace.midiChnls[i] & TRACEFLAG_CONSUME) {
      if (g_trace.col[i] == 0) {
        Midiboy.setDrawPosition(24, i);
        Midiboy.drawSpace(128 - 24, 0);
      }

      g_trace.midiChnls[i] ^= TRACEFLAG_CONSUME;
      *bytePropToString(g_trace.midiChnls[i], 0, &b[0]) = 0;
      Midiboy.setDrawPosition(g_trace.col[i] * 24 + 24, i);
      printString((const char*)&b[0], false);

      g_trace.col[i]++;
      if (g_trace.col[i] >= 4) {
        g_trace.col[i] = 0;
      }

    }

  }

  *bytePropToString(g_trace.frameTime, 0, &b[0]) = 0;
  Midiboy.setDrawPosition(0, 3);
  printString((const char*)&b[0], false);

}
#endif

void renderArp(const int8_t *pPtrn, uint8_t len, uint8_t curIdx) {
  renderPlayhead(curIdx, 16);

  Midiboy.setDrawPosition(0, 0);
  uint8_t i = 0;
  while (i < len) {
    int8_t v = pgm_read_byte(pPtrn + i) + 3;
    if (v > 6) {
      v = 6;
    } else if (v < 0) {
      v = 0;
    }
    uint32_t renderBitmap = 0b00000011000000110000001100000011 << v;
    if (i == curIdx && g_song.state == SONG_STATE_PLAYING) {
      uint32_t tmp = (renderBitmap | 0x00ff);
      Midiboy.drawBitmap(&tmp, 4, true);
    } else {
      Midiboy.drawBitmap(&renderBitmap, 4, true);
    }
    Midiboy.drawBitmap(&renderBitmap, 4, true);
    i++;

  }
  Midiboy.drawSpace(128 - Midiboy.getDrawPositionX(), true);
}

void renderScale(const uint8_t *pPtrn, uint8_t len, uint8_t curIdx) {
  renderPlayhead(curIdx, 16);

  Midiboy.setDrawPosition(0, 0);
  uint8_t i = 0;
  while (i < len) {
    uint8_t v = (pgm_read_byte(pPtrn + i) / 100.0) * 6;

    uint32_t renderBitmap = 0b00000011000000110000001100000011 << v;
    if (i == curIdx && g_song.state == SONG_STATE_PLAYING) {
      uint32_t tmp = (renderBitmap | 0x00ff);
      Midiboy.drawBitmap(&tmp, 4, true);
    } else {
      Midiboy.drawBitmap(&renderBitmap, 4, true);
    }
    Midiboy.drawBitmap(&renderBitmap, 4, true);
    i++;

  }
  Midiboy.drawSpace(128 - Midiboy.getDrawPositionX(), true);
}


void loop() {
  unsigned long timestamp = millis();
#ifdef TRACEPAGE
  g_trace.frameTime = timestamp - g_trace.frameTime;
#endif

  if (timestamp - g_gui.timestamp >= (60)) {
    g_trace.maxFrameTime = 0;
    if (g_gui.btnState & BUTTON_PRESSED) {
      if (g_gui.btnPressedTicks < 16) {
        g_gui.btnPressedTicks++;
        if (g_gui.btnPressedTicks == 16) {
          if (g_gui.btnState & BUTTON_LONGPRESS) {
            if (g_gui.btnState & ACTION_LEFT) {
              if (g_gui.state == GUISTATE_PAGELOADSAVE) {
                saveSong(g_gui.itemColRow[g_gui.state].row);
                printNotification(&LABEL_SAVED[0]);
              }
            } else if (g_gui.btnState & ACTION_RIGHT) {
              if (g_gui.state == GUISTATE_PAGELOADSAVE) {
                if (loadSong(g_gui.itemColRow[g_gui.state].row)) {
                  printNotification(&LABEL_LOAD_SUCCESS[0]);
                } else {
                  printNotification(&LABEL_LOAD_FAIL[0]);
                }
              }
            }
            g_gui.btnState &= (0xff ^ BUTTON_LONGPRESS);
          }
        }
      }
    } else {
      g_gui.btnPressedTicks = 0;
    }

    g_gui.inversed ^= true;
    g_gui.timestamp = timestamp;

    if (g_gui.previewDur) {
      g_gui.previewDur--;
    }
  }

  Midiboy.think();

  pollMidi();

  handleInput();

  bool callRenderPlayback = g_renderPlayback;
  g_renderPlayback = false;

  if (g_song.state == SONG_STATE_PLAYING) {
    if (callRenderPlayback) {
      g_renderSection = 3;
    }
  } else {
    callRenderPlayback = true;
  }

  switch (g_gui.state) {
    case GUISTATE_PAGEMAIN:
      if (g_renderSection != 3) {
        renderSequence();
      } else if (callRenderPlayback && !g_gui.previewDur) {
        renderPlayback();
      }
      break;

    case GUISTATE_PAGELOADSAVE:
      if (g_renderSection != 3) {
        renderSaveSlots();
      } else  {
        if (!g_gui.previewDur && g_gui.btnState & BUTTON_LONGPRESS) {
          renderPress();
        }
      }
      break;

    case GUISTATE_PAGESETTINGS:
      if (g_renderSection != 3) {
        renderSettings();
      } else if (!g_gui.previewDur && callRenderPlayback) {
        uint8_t settingIdx = g_gui.itemColRow[g_gui.state].row;
        uint8_t chnlIdx = settings[settingIdx].properties & SETTING_PROPERTY_MASK_CHANNEL;
        switch (settings[settingIdx].flag & 0b01111111) {
          case SETTING_GROUP_TRIG:
            renderPattern(pgm_read_word(&g_ptrns[g_song.trigPattern[chnlIdx].idx]), g_song.trigPattern[chnlIdx].len - 1, g_voices[chnlIdx].stepIdx);
            break;

          case SETTING_GROUP_VEL:
            if (g_song.velPattern[chnlIdx].idx != NUM_PATTERNS) {
              renderPattern(pgm_read_word(&g_ptrns[g_song.velPattern[chnlIdx].idx]), g_song.velPattern[chnlIdx].len, 0xff);
            } else {
              renderPlayback();
            }
            break;

          case SETTING_GROUP_DUR:
            if (g_song.durPattern[chnlIdx].idx != NUM_PATTERNS) {
              renderPattern(pgm_read_word(&g_ptrns[g_song.durPattern[chnlIdx].idx]), g_song.durPattern[chnlIdx].len, 0xff);
            } else {
              renderPlayback();
            }
            break;

          case SETTING_GROUP_MOD:
            renderPattern(pgm_read_word(&g_ptrns[g_song.modPatternIdx]), 16, 0xff);
            break;

          case SETTING_GROUP_FLT:
            if (g_song.filterPatternIdx[chnlIdx] != N_FILTERPATTERNS) {
              renderPattern(pgm_read_word(&g_fltrPtrns[g_song.filterPatternIdx[chnlIdx]]), 16, g_voices[chnlIdx].filterStepIdx);
            } else {
              renderPattern(pgm_read_word(&g_fltrPtrns[g_voices[chnlIdx].filterIdx]), 16, g_voices[chnlIdx].filterStepIdx);
            }
            break;

          case SETTING_GROUP_ARP:
            renderArp(g_arp[g_song.arp[chnlIdx].idx].pPtrn, g_arp[g_song.arp[chnlIdx].idx].len, g_voices[chnlIdx].arpStepIdx);
            break;

          case SETTING_GROUP_SCL:
            renderScale(g_attenuator[g_song.attenuatorIdx].pPattern, g_attenuator[g_song.attenuatorIdx].len, 0xff);
            break;

          default:
            renderPlayback();
            break;
        }
      }
      break;

#ifdef TRACEPAGE
    case GUISTATE_PAGETRACE:
      if (callRenderPlayback) {
        renderTrace();
      }
      break;
#endif

    default:
      break;
  }

  g_renderSection++;
  if (g_renderSection > 4) {
    g_renderSection = 0;
  }

#ifdef DEBUGFRAMETIME
    uint8_t b[4] = {0,0,0,0};
    if (g_trace.frameTime > g_trace.maxFrameTime) {
      g_trace.maxFrameTime = g_trace.frameTime;
    }
     bytePropToString(g_trace.maxFrameTime, 0, &b[0]) = 0;
    Midiboy.setDrawPosition(0, 2);
    printString((const char*)&b[0], false);
#endif

  g_trace.frameTime = timestamp;

}
