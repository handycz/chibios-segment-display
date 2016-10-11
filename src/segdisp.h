/* segdisp.h -- 7 and 16 segment display library for ChibiOS
 *
 * Copyright (C) 2016 Ondrej Novak
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

/**
 * @file
 * @brief Segdisp library header
 */

#ifndef SEGDISP_H
#define SEGDISP_H


#include "stdint.h"
#include "ch.h"

/** Configuration flag mask */
#define SEGDISP_COMMON_ELECTRODE_FLAG 0b1
/** Flag indicating binary number (non-negated) is used for the output */
#define SEGDISP_NONINVERTED_SEGMENT 0
/** Flag indicating binary negation is used for the output */
#define SEGDISP_INVERTED_SEGMENT 1

/** Configuration flag mask */
#define SEGDISP_DRIVER_FLAG 0b01
/** Flag indicating that normal (non-inverted) output is used for the driving transistor*/
#define SEGDISP_NONINVERTED_DRIVER 0b00
/** Flag indicating that inverted output is used for the driving transistor*/
#define SEGDISP_INVERTED_DRIVER 0b01

/** Configuration flag mask */
#define SEGDISP_SEGMENTS_FLAG 0b001
/** Flag indicating 7 segment display */
#define SEGDISP_SEGMENTS_SEVEN 0b000
/** Flag indicating 16 segment display */
#define SEGDISP_SEGMENTS_SIXTEEN 0b001


/**
 * Configuration structure defining port and pin number within port of specific pin
 */
typedef struct segdisp_pin_def {
	typeof(GPIOA) port;
	uint8_t pin;
} segdisp_pin_def_t;

/**
 * Structure with output port configuration. It's used for defining which pins of which port are used for output.
 */
typedef struct segdisp_pins {
	/** Number of pads (pins) */
	int number;
	segdisp_pin_def_t pins[];
} segdisp_pins_t;

/**
 * Structure used for configuring the scrolling
 */
typedef struct segdisp_scroll_conf {
	/** Delay between steps (in ms) */
	int delay;
	/** Number of characters shifted in one step */
	int step;
} segdisp_scroll_conf_t;

/**
 * Display configuration structure
 */
typedef struct segdisp {
	/** Pointer to the structure defining output pins for segment control */
	segdisp_pins_t *segments;
	/** Pointer to the structure defining output pins for digit control */
	segdisp_pins_t *digits;
	/** Scrolling configuration structure */
	segdisp_scroll_conf_t *scroll;
	/** Pointer to the refreshing thread */
	thread_t *thread; 
	/** Pointer to the scrolling thread */
	thread_t *scroll_thd;
	/** Display buffer mutex (actual inside segdisp struct) */
	mutex_t *display_buffer_mtx;
	/** String buffer mutex (buffer inside segdisp struct) */
	mutex_t *string_buffer_mtx;
	/** Pointer to the buffer with currently displayed characters. Characters are already mapped to integer output */
	uint32_t *actual;

	/** Refresh rate of the display in microseconds (default - 5000 us) */
	int refresh;
	/** Current offset of the text */
	int offset;
	/** Currently displayed string */
	char *buffer;
	/** Lenght of the displayed string */
	int buffer_strlen;
	/** Configuration flags. For common anode and cathode configuration use SEGDISP_COMMON_ANODE and SEGDISP_COMMON_CATHODE respectively. 
		To configure NPN and PNP driver, use SEGDISP_NPN_DRIVER and SEGDISP_PNP_DRIVER constants.
		For 7 segment display use SEGDISP_SEGMENTS_SEVEN, for 16 segment display use SEGDISP_SEGMENTS_SIXTEEN
	*/
	uint8_t flags;
} segdisp_t;


void segdisp_seg_out(segdisp_t *disp, int seg, int out);
void segdisp_dig_ena(segdisp_t *disp, int digit, int out);
void segdisp_show_digit(segdisp_t *disp, int position, int output);
int segdisp_move_cont(segdisp_t *disp, int step);
int segdisp_move_abs(segdisp_t *disp, int offset);

int segdisp_init(segdisp_t *disp, segdisp_pins_t *segments, segdisp_pins_t *digits, uint8_t flags);
int segdisp_run(segdisp_t *disp, tprio_t priority);
void segdisp_stop(segdisp_t *disp); 
int segdisp_set(segdisp_t *disp, int position, char output);
int segdisp_set_str(segdisp_t *disp, char *text);
int segdisp_scroll_run(segdisp_t *disp, tprio_t priority);
void segdisp_scroll_stop(segdisp_t *disp);

uint32_t segdisp_7seg_char2int(char c);
uint32_t segdisp_16seg_char2ing(char c);

#endif