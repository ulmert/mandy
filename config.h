#ifndef CONFIG_H
#define CONFIG_H

#define PPQN 24
#define SEND_MIDI_CLOCK
// #define SEND_MIDI_STOP
// #define SEND_MIDI_START
#define TRACEPAGE

#define C_Min (1 << 4)
#define C_Dim (2 << 4)

#define N_Cb 11
#define N_C 0
#define N_Cs 1
#define N_Db 1
#define N_D 2
#define N_Ds 3
#define N_Eb 3
#define N_E 4
#define N_F 5
#define N_Fs 6
#define N_Gb 6
#define N_G 7
#define N_Gs 8
#define N_Ab 8
#define N_A 9
#define N_As 10
#define N_Bb 10
#define N_B 11

#define N_Cm (0|C_Min)
#define N_Csm (1|C_Min)
#define N_Db 1
#define N_Dm (2|C_Min)
#define N_Dsm (3|C_Min)
#define N_Ebm (3|C_Min)
#define N_Em (4|C_Min)
#define N_Fm (5|C_Min)
#define N_Fsm (6|C_Min)
#define N_Gb 6
#define N_Gm (7|C_Min)
#define N_Gsm (8|C_Min)
#define N_Abm (8|C_Min)
#define N_Am (9|C_Min)
#define N_Bbm (10|C_Min)
#define N_Bm (11|C_Min)

#define N_Cdim (0|C_Dim)
#define N_Csdim (1|C_Dim)
#define N_Dbdim (1|C_Dim)
#define N_D 2
#define N_Dsdim (3|C_Dim)
#define N_Ebdim (3|C_Dim)
#define N_E 4
#define N_Fdim (5|C_Dim)
#define N_Fsdim (6|C_Dim)
#define N_Gb 6
#define N_Gdim (7|C_Dim)
#define N_Gsdim (8|C_Dim)
#define N_Ab 8
#define N_Adim (9|C_Dim)
#define N_Asdim (10|C_Dim)
#define N_Bb 10
#define N_Bdim (11|C_Dim)

#define N_O 12

static const PROGMEM uint8_t g_chord_chart[2][12][7] = {
  {
    {N_C,   N_Dm, N_Em, N_F, N_G, N_Am, N_Bdim},
    {N_Db,  N_Ebm, N_Fm, N_Gb, N_Ab, N_Bbm, N_Cdim},
    {N_D,   N_Em, N_Fsm, N_G, N_A, N_Bm, N_Csdim},
    {N_Eb,  N_Fm, N_Gm, N_Ab, N_Bb, N_Cm, N_Dbdim},
    {N_E,   N_Fsm, N_Gsm, N_A, N_B, N_Csm, N_Dsdim},
    {N_F,   N_Gm, N_Am, N_Bb, N_C, N_Dm, N_Ebdim},
    {N_Gb,  N_Abm, N_Bbm, N_Cb, N_Db, N_Ebm, N_Fdim},
    {N_G,   N_Am, N_Bm, N_C, N_D, N_Em, N_Fsdim},
    {N_Ab,  N_Bbm, N_Cm, N_Db, N_Eb, N_Fm, N_Gdim},
    {N_A,   N_Bm, N_Csm, N_D, N_E, N_Fsm, N_Gsdim},
    {N_Bb,  N_Cm, N_Dm, N_Eb, N_F, N_Gm, N_Adim},
    {N_B,   N_Csm, N_Dsm, N_E, N_Fs, N_Gsm, N_Asdim}
  },
  {
    {N_Cm,  N_Dbdim,  N_Eb, N_Fm,   N_Gm,  N_Ab,  N_Bb},
    {N_Csm, N_Dsdim,  N_E,  N_Fsm,  N_Gsm, N_A,   N_B},
    {N_Dm,  N_Ebdim,  N_F,  N_Gm,   N_Am,  N_Bb,  N_C},
    {N_Ebm, N_Fdim,   N_Gb, N_Abm,  N_Bbm, N_Cb,  N_Db},
    {N_Em,  N_Fsdim,  N_G,  N_Am,   N_Bm,  N_C,   N_D},
    {N_Fm,  N_Gdim,   N_Ab, N_Bbm,  N_Cm,  N_Db,  N_Eb},
    {N_Fsm, N_Gsdim,  N_A,  N_Bm,   N_Csm, N_D,   N_E},
    {N_Gm,  N_Adim,   N_Bb, N_Cm,   N_Dm,  N_Eb,  N_F},
    {N_Gsm, N_Asdim,  N_B,  N_Csm,  N_Dsm, N_E,   N_Fs},
    {N_Am,  N_Bdim,   N_C,  N_Dm,   N_Em,  N_F,   N_G},
    {N_Bbm, N_Cdim,   N_Db, N_Ebm,  N_Fm,  N_Gb,  N_Ab},
    {N_Bm,  N_Csdim,  N_D,  N_Em,   N_Fsm, N_G,   N_A}
  }
};

/*
  0,2,4, 5,7,  9, 11,
  1,3,5, 6,8, 10,  0,
  2,4,6, 7,9, 11,  1,
  3,5,7, 8,10, 0,  1,
  4,6,8, 9,11, 1,  3,
  5,7,9, 10,0, 2,  3,
  6,8,10,11,1, 3,  5,
  7,9,11,0,2,  4,  6,
  8,10,0,1,3,  5,  7,
  9,11,1,2,4,  6,  8,
  10,0,2,3,5,  7,  9,
  11,1,3,4,6,  8, 10,

  0,1,3,5,7,8,10,
  1,3,4,6,8,9,11,
  2,3,5,7,9,10,0,
  3,5,6,8,10,11,1,
  4,6,7,9,11,0,2,
  5,7,8,10,0,1,3,
  6,8,9,11,1,2,4,
  7,9,10,0,2,3,5,
  8,10,11,1,3,4,6,
  9,11,0,2,4,5,7,
  10,0,1,3,5,6,8,
  11,1,2,4,6,7,9,

*/

int8_t g_chordOffsets[3][3] = {
  {0, 4, (4 + 3)},
  {0, 3, (3 + 4)},
  {0, 3, (3 + 3)}
};

typedef struct {
  uint8_t idx;
  uint8_t len;
} generic_pattern_t;

typedef struct {
  uint8_t len;
  const int8_t *pPtrn;
} arp_t;

typedef struct {
  uint8_t len;
  const uint8_t *pPattern;
} attenuator_t;


typedef struct {
  uint8_t idx;
} arp_pattern_t;

#define NUM_TRIG_PULSES 4
static const PROGMEM uint8_t g_trigPulses[NUM_TRIG_PULSES] = {3, 6, 12, 24}; // {3, 6, 9, 15, 18, 21, 24};

#define N_ATTENUATOR_PATTERNS 5
static const PROGMEM uint8_t g_attenuator_pattern0[3] = {100, 50, 25};
static const PROGMEM uint8_t g_attenuator_pattern1[3] = {75, 50, 100};
static const PROGMEM uint8_t g_attenuator_pattern2[4] = {100, 50, 75, 50};
static const PROGMEM uint8_t g_attenuator_pattern3[4] = {50, 25, 5, 25};

attenuator_t g_attenuator[N_ATTENUATOR_PATTERNS] = {{1, &g_attenuator_pattern0[0]}, {3, &g_attenuator_pattern0[0]}, {3, &g_attenuator_pattern1[0]}, {4, &g_attenuator_pattern2[0]}, {4, &g_attenuator_pattern3[0]}};

#define N_ARP_PATTERNS 10
static const PROGMEM int8_t g_arp_pattern0[1] = {0};
static const PROGMEM int8_t g_arp_pattern1[11] = {0, 1, -1, 2, -2, -3, 3, 2, -2, 1, -1};
static const PROGMEM int8_t g_arp_pattern2[4] = { -1, 0, 1, 0};
static const PROGMEM int8_t g_arp_pattern3[8] = {0, 0, 2, 0, 0, 0, -2, 0};
static const PROGMEM int8_t g_arp_pattern4[8] = {0, 1, 0, 2, 0, -1, 0, -2};
static const PROGMEM int8_t g_arp_pattern5[4] = {3, 2, 1, 0};

arp_t g_arp[N_ARP_PATTERNS] = {{1, &g_arp_pattern0[0]}, {11, &g_arp_pattern1[0]}, {4, &g_arp_pattern2[0]}, {2, &g_arp_pattern2[0]}, {2, &g_arp_pattern2[2]}, {8, &g_arp_pattern3[3]}, {2, &g_arp_pattern3[6]}, {4, &g_arp_pattern4[0]}, {4, &g_arp_pattern4[4]}, {4, &g_arp_pattern5[0]}};

#define N_FILTERPATTERNS 6

static const PROGMEM uint16_t g_fltrPtrns[N_FILTERPATTERNS] = {
  0b1111111111111111,
  0b1101110111011101,
  0b1010101010101010,
  0b1000100010001000,
  0b1000000010000000,
  0b0000000000000000,
};

#define N_PATTERNS 9
#define NUM_PATTERNS 38

static const PROGMEM uint16_t g_ptrns[NUM_PATTERNS] = {
  0b0000000000000000,
  0b1111111111111111,
  0b1000100010001000,
  0b0100001000000011,
  0b1001100010001100,
  0b0000000000000010,
  0b0000000000010001,
  0b0100000000000000,
  0b0001000100010100,
  0b0000100010000000,
  0b0000000001000000,
  0b0101010100010111,
  0b0010001000001000,
  0b0000000000011010,
  0b0001000001001000,
  0b0000000000001100,
  0b0000000000000001,
  0b0111000000000000,
  0b0000000101000010,
  0b1001000010001000,
  0b0000001000000011,
  0b0000010000000000,
  0b0010001000001000,
  0b0000010000000010,
  0b0010000000010000,
  0b0000011000000000,
  0b1000000010000000,
  0b0000100000001000,
  0b0000000000010000,
  0b1101110111011101,
  0b0010001000100010,
  0b0000010000000010,
  0b1011001010110010,
  0b0010001000100010,
  0b0100010001000100,
  0b0001100100011001,
  0b0000100000010000,
  0b1000100010000000
};
#endif
