#ifndef SEG7_FONT_H
#define SEG7_FONT_H

#include <stdint.h>

/**
 * @file seg7_font.h
 * @brief Font table for a 7-segment display
 *
 * BIT MAPPING: 1 = SEGMENT ON!!
 *
 *   bit:  7    6    5    4    3    2    1    0
 *   seg:  DP   g    f    e    d    c    b    a
 *
 *       aaaaa
 *       f   b
 *       f   b
 *       ggggg
 *       e   c
 *       e   c
 *       ddddd [DP]
 */

/* ------------------------------------------------------------------ */
/*  Segment bit masks                                                   */
/* ------------------------------------------------------------------ */

#define SEG_A_MASK (1 << 0)
#define SEG_B_MASK (1 << 1)
#define SEG_C_MASK (1 << 2)
#define SEG_D_MASK (1 << 3)
#define SEG_E_MASK (1 << 4)
#define SEG_F_MASK (1 << 5)
#define SEG_G_MASK (1 << 6)
#define SEG_DP_MASK (1 << 7)

/* ------------------------------------------------------------------ */
/*  Digit font table (0–9)                                             */
/* ------------------------------------------------------------------ */

/*
 *  Digit |  DP  g  f  e  d  c  b  a  | Hex
 *  ------+---------------------------+-----
 *    0   |   0  0  1  1  1  1  1  1  | 0x3F
 *    1   |   0  0  0  0  0  1  1  0  | 0x06
 *    2   |   0  1  0  1  1  0  1  1  | 0x5B
 *    3   |   0  1  0  0  1  1  1  1  | 0x4F
 *    4   |   0  1  1  0  0  1  1  0  | 0x66
 *    5   |   0  1  1  0  1  1  0  1  | 0x6D
 *    6   |   0  1  1  1  1  1  0  1  | 0x7D
 *    7   |   0  0  0  0  0  1  1  1  | 0x07
 *    8   |   0  1  1  1  1  1  1  1  | 0x7F
 *    9   |   0  1  1  0  1  1  1  1  | 0x6F
 */

static const uint8_t Seg7DigitCode[10] = {
	0x3F, /* 0 */
	0x06, /* 1 */
	0x5B, /* 2 */
	0x4F, /* 3 */
	0x66, /* 4 */
	0x6D, /* 5 */
	0x7D, /* 6 */
	0x07, /* 7 */
	0x7F, /* 8 */
	0x6F  /* 9 */
};

/* ------------------------------------------------------------------ */
/*  Uppercase letter font table (A–Z)                                  */
/* ------------------------------------------------------------------ */

/*
 *  Char |  DP  g  f  e  d  c  b  a  | Hex   | Note
 *  -----+---------------------------+-------+----------------------
 *   A   |   0  1  1  1  0  1  1  1  | 0x77  |
 *   B   |   0  1  1  1  1  1  0  0  | 0x7C  | looks like 'b'
 *   C   |   0  0  1  1  1  0  0  1  | 0x39  |
 *   D   |   0  1  0  1  1  1  1  0  | 0x5E  | looks like 'd'
 *   E   |   0  1  1  1  1  0  0  1  | 0x79  |
 *   F   |   0  1  1  1  0  0  0  1  | 0x71  |
 *   G   |   0  0  1  1  1  1  0  1  | 0x3D  |
 *   H   |   0  1  1  1  0  1  1  0  | 0x76  |
 *   I   |   0  0  0  0  0  1  1  0  | 0x06  | same as 1
 *   J   |   0  0  0  0  1  1  1  0  | 0x1E  |
 *   K   |   0  1  1  1  0  1  0  1  | 0x75  | approximation
 *   L   |   0  0  1  1  1  0  0  0  | 0x38  |
 *   M   |   0  0  1  0  0  1  0  1  | 0x25  | approximation
 *   N   |   0  1  0  1  0  1  0  0  | 0x54  | approximation
 *   O   |   0  0  1  1  1  1  1  1  | 0x3F  | same as 0
 *   P   |   0  1  1  1  0  0  1  1  | 0x73  |
 *   Q   |   0  1  1  0  0  1  1  1  | 0x67  |
 *   R   |   0  1  0  1  0  0  0  0  | 0x50  | approximation to r
 *   S   |   0  1  1  0  1  1  0  1  | 0x6D  | same as 5
 *   T   |   0  1  1  1  1  0  0  0  | 0x78  |
 *   U   |   0  0  1  1  1  1  1  0  | 0x3E  |
 *   V   |   0  0  0  1  1  1  1  0  | 0x1E  | approximation
 *   W   |   0  0  1  1  1  1  1  0  | 0x3E  | approximation
 *   X   |   0  1  1  1  0  1  1  0  | 0x76  | same as H
 *   Y   |   0  1  1  0  1  1  1  0  | 0x6E  |
 *   Z   |   0  1  0  1  1  0  1  1  | 0x5B  | same as 2
 *
 *   !   |.  1. 0  0  0  0  0  1  0. |. 0x82
 */

static const uint8_t Seg7AlphaCode[26] = {
	0x77, /* A */
	0x7C, /* B */
	0x39, /* C */
	0x5E, /* D */
	0x79, /* E */
	0x71, /* F */
	0x3D, /* G */
	0x76, /* H */
	0x06, /* I */
	0x1E, /* J */
	0x75, /* K */
	0x38, /* L */
	0x25, /* M */
	0x54, /* N */
	0x3F, /* O */
	0x73, /* P */
	0x67, /* Q */
	0x50, /* R */
	0x6D, /* S */
	0x78, /* T */
	0x3E, /* U */
	0x1E, /* V */
	0x3E, /* W */
	0x76, /* X */
	0x6E, /* Y */
	0x5B  /* Z */
};

/* ------------------------------------------------------------------ */
/*  Special characters                                                  */
/* ------------------------------------------------------------------ */

#define SEG7_BLANK 0x00		 /* all segments off        */
#define SEG7_MINUS 0x40		 /* minus sign  ( g only )  */
#define SEG7_UNDERSCORE 0x08 /* underscore  ( d only )  */
#define SEG7_DP 0x80		 /* decimal point only      */
#define SEG7_EXCL 0x82		 /* exclamation mark*/

/* ------------------------------------------------------------------ */
/*  Macros                                                       */
/* ------------------------------------------------------------------ */

/** Add decimal point to any glyph */
#define SEG7_WITH_DP(glyph) ((glyph) | SEG_DP)

/** Look up a digit glyph safely */
#define SEG7_DIGIT(n) (((n) < 10) ? Seg7DigitCode[(n)] : SEG7_BLANK)

/** Look up an uppercase letter glyph safely */
#define SEG7_CHAR(c) ((((c) >= 'A') && ((c) <= 'Z')) ? Seg7AlphaCode[(c) - 'A'] : SEG7_BLANK)

/*** for constant arrays */
#define SEG7_A 0x77 /* A */
#define SEG7_B 0x7C /* B */
#define SEG7_C 0x39 /* C */
#define SEG7_D 0x5E /* D */
#define SEG7_E 0x79 /* E */
#define SEG7_F 0x71 /* F */
#define SEG7_G 0x3D /* G */
#define SEG7_H 0x76 /* H */
#define SEG7_I 0x06 /* I */
#define SEG7_J 0x1E /* J */
#define SEG7_K 0x75 /* K */
#define SEG7_L 0x38 /* L */
#define SEG7_M 0x25 /* M */
#define SEG7_N 0x54 /* N */
#define SEG7_O 0x3F /* O */
#define SEG7_P 0x73 /* P */
#define SEG7_Q 0x67 /* Q */
#define SEG7_R 0x50 /* R */
#define SEG7_S 0x6D /* S */
#define SEG7_T 0x78 /* T */
#define SEG7_U 0x3E /* U */
#define SEG7_V 0x1E /* V */
#define SEG7_W 0x3E /* W */
#define SEG7_X 0x76 /* X */
#define SEG7_Y 0x6E /* Y */
#define SEG7_Z 0x5B /* Z */

#define SEG7_0 0x3F /* 0 */
#define SEG7_1 0x06 /* 1 */
#define SEG7_2 0x5B /* 2 */
#define SEG7_3 0x4F /* 3 */
#define SEG7_4 0x66 /* 4 */
#define SEG7_5 0x6D /* 5 */
#define SEG7_6 0x7D /* 6 */
#define SEG7_7 0x07 /* 7 */
#define SEG7_8 0x7F /* 8 */
#define SEG7_9 0x6F /* 9 */

/********************************* CONSTANT ARRAYS *********************************/

#endif /* SEG7_FONT_H */