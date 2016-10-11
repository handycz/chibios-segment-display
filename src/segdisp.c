/* segdisp.c -- 7 and 16 segment display library for ChibiOS
 *
 * Copyright (C) 2016 Ondrej Novak
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

/**
 * @file
 * @brief Segdisp library code
 */


#include "segdisp.h"
#include "hal.h"
#include "ch.h"
#include "string.h"
#include "chthreads.h"
#include "util.h"

/* Threads */

/* This thread periodically runs and refresesh all the digits */
static THD_FUNCTION(segdisp_refresh_thread, arg) {
  segdisp_t *disp = (segdisp_t*)arg;
  chRegSetThreadName("segdisp_refresh");

  while (true) {
  	int i;
  	for(i = 0; i < disp->digits->number; i++){
  		chMtxLock(disp->display_buffer_mtx);
    	segdisp_show_digit(disp, i, disp->actual[i]);
    	chMtxUnlock(disp->display_buffer_mtx);
		chThdSleepMicroseconds(disp->refresh);
  	}

  	if(chThdShouldTerminateX()){
  		chThdExit((msg_t) 0);
  	}
  }
}

/* Thread for text scrolling */
static THD_FUNCTION(segdisp_scroll_thread, arg) {
  segdisp_t *disp = (segdisp_t*)arg;
  chRegSetThreadName("segdisp_scroll");

  while (true) {
	chThdSleepMilliseconds(disp->scroll->delay);

  	if(chThdShouldTerminateX()){
  		chThdExit((msg_t) 0);
  	}
  	
  	segdisp_move_cont(disp, disp->scroll->step);
  }
}

/**
 * @brief Initializes the Display configuration structure [external API]
 * @param disp     Pointer to allocated segdisp_t structure
 * @param segments Pointer to allocated and configured segdisp_pins_t structure defining the segments of one digit
 * @param digits   Pointer to allocated and configured segdisp_pins_t structure defining the digits
 * @param flags    Configuring flags
 * rReturn		   Returns -1 on failure, 0 on success.
 */
int segdisp_init(segdisp_t *disp, segdisp_pins_t *segments, segdisp_pins_t *digits, uint8_t flags){
	int i;

	disp->segments = segments;
	disp->digits = digits;

	disp->flags = flags;
	disp->actual = chCoreAlloc(digits->number * sizeof(typeof(disp->actual)));
	if(disp->actual == NULL){
		return -1;
	}

	disp->buffer = NULL;
	disp->buffer_strlen = 0;
	disp->offset = 0;

	disp->display_buffer_mtx = chCoreAlloc(sizeof(mutex_t));
	disp->string_buffer_mtx = chCoreAlloc(sizeof(mutex_t));

	if(disp->display_buffer_mtx == NULL){
		return -1;
	}

	if(disp->string_buffer_mtx == NULL){
		return -1;
	}

	disp->refresh = 5000;

	for(i = 0; i < disp->segments->number; i++){
		palSetPadMode((ioportid_t) disp->segments->pins[i].port, disp->segments->pins[i].pin, PAL_MODE_OUTPUT_PUSHPULL);
	}

	for(i = 0; i < disp->digits->number; i++){
		palSetPadMode((ioportid_t) disp->digits->pins[i].port, disp->digits->pins[i].pin, PAL_MODE_OUTPUT_PUSHPULL);
	}

	chMtxObjectInit(disp->display_buffer_mtx);
	chMtxObjectInit(disp->string_buffer_mtx);

	return 0 ;
}

/**
 * Set output value for specific segment (one segment enable) [internal]
 * @param disp Display configuration structure
 * @param seg  Segment number
 * @param out  Output value
 */
void segdisp_seg_out(segdisp_t *disp, int seg, int out){
	if((disp->flags & SEGDISP_DRIVER_FLAG) == SEGDISP_INVERTED_DRIVER){
		if(out){
			palClearPad((ioportid_t) disp->segments->pins[seg].port, disp->segments->pins[seg].pin);
		}
		else{
			palSetPad((ioportid_t) disp->segments->pins[seg].port, disp->segments->pins[seg].pin);
		}
	}
	else{
		if(out){
			palSetPad((ioportid_t) disp->segments->pins[seg].port, disp->segments->pins[seg].pin);
		}
		else{
			palClearPad((ioportid_t) disp->segments->pins[seg].port, disp->segments->pins[seg].pin);
		}
	}
}

/**
 * Set output value for specific digit (common electrode enable) [internal]
 * @param disp  Display configuration structure
 * @param digit Digit number
 * @param out   Output value
 */
void segdisp_dig_ena(segdisp_t *disp, int digit, int out){

	if((disp->flags & SEGDISP_COMMON_ELECTRODE_FLAG) == SEGDISP_INVERTED_SEGMENT){
		if(out){
			palSetPad((ioportid_t) disp->digits->pins[digit].port, disp->digits->pins[digit].pin);
		}
		else{
			palClearPad((ioportid_t) disp->digits->pins[digit].port, disp->digits->pins[digit].pin);
		}
	}
	else{
		if(out){
			palClearPad((ioportid_t) disp->digits->pins[digit].port, disp->digits->pins[digit].pin);
		}
		else{
			palSetPad((ioportid_t) disp->digits->pins[digit].port, disp->digits->pins[digit].pin);
		}
	}
}

/**
 * Display one character at specific position [internal]
 * @param disp     Display configuration structure
 * @param position Position
 * @param output   Output value (already mapped character to output number)
 */
void segdisp_show_digit(segdisp_t *disp, int position, int output){
	int i;
	if(position > disp->digits->number){
		return;
	}
	for(i = 0; i < disp->digits->number; i++){
		segdisp_dig_ena(disp, i, 0);
	}
	segdisp_dig_ena(disp, position, 1);
	for(i = 0; i < disp->segments->number; i++){
		segdisp_seg_out(disp, i, output & 1);
		output = output >> 1;
	}
}

/**
 * Sets a character to the specified position that will be displayed [external API]
 * @param  disp     Display configuration structure
 * @param  position Position
 * @param  output   Ascii character to display
 * @return          Returns -1 on failure, 0 on success
 */
int segdisp_set(segdisp_t *disp, int position, char output){
	int tmp;
	if(position > disp->digits->number)
		return -1;

	if((disp->flags & SEGDISP_SEGMENTS_FLAG) == SEGDISP_SEGMENTS_SEVEN){
		tmp = segdisp_7seg_char2int(output);
	}
	else{
		tmp = segdisp_16seg_char2ing(output);
	}
	disp->actual[position] = tmp;

	return 0;
}

/**
 * Move displayed text to the absolute position from the beginning [external API]
 * @param  disp   Display configuration structure
 * @param  offset Position to move the text to
 * @return        Returns -1 on failure, 0 on success
 */
int segdisp_move_abs(segdisp_t *disp, int offset){
	disp->offset = disp->buffer_strlen;
	return segdisp_move_cont(disp, offset);
}

/**
 * Move displayed text relatively to current position [external API]
 * @param  disp  Display configuration structure
 * @param  step  Relative movement offset
 * @return       Returns -1 on failure, 0 on success
 */
int segdisp_move_cont(segdisp_t *disp, int step){
	int i;

	if(disp->scroll == NULL)
		return -1;

	chMtxLock(disp->string_buffer_mtx);
	/* move the offset */
	disp->offset += step;

	if(disp->offset > disp->buffer_strlen){
		disp->offset -= disp->buffer_strlen ;
	}
	else if(disp->offset < 0){
		disp->offset += disp->buffer_strlen - 1;
	}
	
	/* shift the string */
	utils_str_shift(disp->buffer, step);
	chMtxUnlock(disp->string_buffer_mtx);

	chMtxLock(disp->display_buffer_mtx);
	for(i = 0; i < disp->digits->number; i++){
		segdisp_set(disp, i, disp->buffer[i]);
	}
	chMtxUnlock(disp->display_buffer_mtx);

	return true;
}

/**
 * Start the scrolling of the text. Scroll configuration structure has to be configured!
 * @param  disp 	Display configuration structure
 * @param  priority Scroll thread priority
 * @return     		Returns -1 on failure, 0 on success
 */
int segdisp_scroll_run(segdisp_t *disp, tprio_t priority){
	if(disp->scroll == NULL)
		return -1;

	if(disp->scroll_thd != NULL)
		return -1;

	disp->scroll_thd = chThdCreateFromHeap(NULL, THD_WORKING_AREA_SIZE(128), priority, segdisp_scroll_thread, disp);
	if(disp->thread == NULL){
		return -1;
	}
	return 1;
}

/**
 * Stop the scrolling
 * @param disp Display confguration structure
 */
void segdisp_scroll_stop(segdisp_t *disp){
	chThdTerminate(disp->scroll_thd);
	disp->scroll_thd = NULL;
}

/**
 * Run the display refresh
 * @param  disp     Display configuration structure
 * @param  priority Priority of the 
 * @return          Returns -1 on failure, 0 on success
 */
int segdisp_run(segdisp_t *disp, tprio_t priority){
	if(disp->thread != NULL)
		return -1;

	disp->thread = chThdCreateFromHeap(NULL, THD_WORKING_AREA_SIZE(128), priority, segdisp_refresh_thread, disp);
	if(disp->thread == NULL)
		return -1;

	return 0;
}

/**
 * Stop display refresh
 * @param disp Display configuration structure
 */
void segdisp_stop(segdisp_t *disp){
	segdisp_set_str(disp, " ");
	chThdTerminate(disp->thread);
}

/**
 * Set string to display
 * @param  disp Display configuration structure
 * @param  text Pointer to the string to display
 * @return      Returns -1 on failure, 0 on success
 */
int segdisp_set_str(segdisp_t *disp, char *text){
	if(text == NULL)
		return -1;
	if(strlen(text) < 1)
		return -1;

	disp->buffer = text;
	disp->buffer_strlen = strlen(disp->buffer);

	// redraw the text by moving it to current position 
	segdisp_move_abs(disp, 0);

	return 0;
}

/**
 * Function mapping a character to an integer output for 7 segment display
 * @param  c Character to map
 * @return   Integer whose bits are representing individual segments of the display
 */
uint32_t segdisp_7seg_char2int(char c){
	uint32_t out;
	switch(c){
		// case 0:
		case ' ':
			out = 0;
		break;

		case '-':
			out = 0b1000000;
		break;

		case '0':
			out = 0b0111111;
		break;

		case '1':
			out = 0b0000110;
		break;

		case '2':
			out = 0b1011011;
		break;

		case '3':
			out = 0b1001111;
		break;

		case '4':
			out = 0b1100110;
		break;

		case '5':
			out = 0b1101101;
		break;

		case '6':
			out = 0b1111101;
		break;

		case '7':
			out = 0b0000111;
		break;

		case '8':
			out = 0b1111111;
		break;

		case '9':
			out = 0b1101111;
		break;

		case 'a':
		case 'A':
			out = 0b1110111;
			break;

		case 'b':
		case 'B':
			out = 0b1111100;
			break;

		case 'c':
		case 'C':
			out = 0b0111001;
			break;

		case 'd':
		case 'D':
			out = 0b1011110;
			break;

		case 'e':
		case 'E':
			out = 0b1111001;
			break;

		case 'f':
		case 'F':
			out = 0b1110001;
			break;

		default:
			out = 0b0011100;
		break;
	}

	return out;
}

/**
 * Function mapping a character to an integer output for 16 segment display
 * @param  c Character to map
 * @return   Integer whose bits are representing individual segments of the display
 */
uint32_t segdisp_16seg_char2ing(char c){
	uint32_t out;
	switch(c){
		case 'A':
			out = 0b0000001111001111;
			break;
		case 'a':
			out = 0b0100010101010000;
			break;
		case 'B':
			out = 0b0100101000111111;
		case 'b':
			out = 0b0100000111010000;
			break;
		case 'C':
			out = 0b0000000101010000;
			break;
		case 'c':
			out = 0b0000000101010000;
			break;
		case 'D':
			out = 0b0100100000111111; 
			break;
		case 'd':
			out = 0b0100100101010000;
			break;
		case 'E':
			out = 0b0000000111110011;
			break;
		case 'e':
			out = 0b1000000101110000;
			break;
		case 'F':
			out = 0b0000000111000011;
			break;
		case 'f':
			out = 0b0100101100000010;
			break;
		case 'G':
			out = 0b0000001011111011;
			break;
		case 'g':
			out = 0b0100100110010001;
			break;
		case 'H':
			out = 0b0000001111001100;
			break; 
		case 'h':
			out = 0b0100000111000000;
			break;
		case 'I':
			out = 0b0100100000110011;
			break;
		case 'i':
			out = 0b0100000000000000;
			break;
		case 'J':
			out = 0b0000000001111100;
			break;
		case 'j':
			out = 0b0100100001010000;
			break;
		case 'K':
			out = 0b0011000111000000;
			break;
		case 'k':
			out = 0b0111100000000000;
			break;
		case 'L':
			out = 0b0000000011110000;
			break;
		case 'l':
			out = 0b0100100000000000;
			break;
		case 'M':
			out = 0b0001010011001100;
			break;
		case 'm':
			out = 0b0100001101001000;
			break;
		case 'N':
			out = 0b0010010011001100;
			break;
		case 'n':
			out = 0b0100000101000000;
			break;
		case 'O':
			out = 0b0000000011111111;
			break;
		case 'o':
			out = 0b0100000101010000;
			break;
		case 'P':
			out = 0b0000001111000111;
			break;
		case 'p':
			out = 0b0000100111000001;
			break;
		case 'Q':
			out = 0b0010000011111111;
			break;
		case 'q':
			out = 0b0100100110000001;
			break;
		case 'R':
			out = 0b0010001111000111;
			break;
		case 'r':
			out = 0b0000000101000000;
			break;
		case 'S':
			out = 0b0000001110111011;
			break;
		case 's':
			out = 0b0100000110010001;
			break;
		case 'T':
			out = 0b0100100000000011;
			break;
		case 't':
			out = 0b0100101100000000;
			break;
		case 'U':
			out = 0b0000000011111100;
			break;
		case 'u':
			out = 0b0100000001010000;
			break;
		case 'V':
			out = 0b1001000011000000;
			break;
		case 'v':
			out = 0b1000000001000000;
			break;
		case 'W':
			out = 0b1010000011001100;
			break;
		case 'w':
			out = 0b1010000001001000;
			break;
		case 'X':
			out = 0b1011010000000000;
			break;
		case 'x':
			out = 0b1011010000000000;
			break;
		case 'Y':
			out = 0b0100001110000100;
			break;
		case 'y':
			out = 0b0101010000000000;
			break;
		case 'Z':
			out = 0b1001000000110011;
			break;
		case 'z':
			out = 0b1000000100010000;
			break;
		case '0':
			out = 0b1001000011111111;
			break;
		case '1':
			out = 0b0001000000001100;
			break;
		case '2':
			out = 0b0000001101110111;
			break;
		case '3':
			out = 0b0000001000111111;
			break;
		case '4':
			out = 0b0000001110001100;
			break;
		case '5':
			out = 0b0000001110111011;
			break;
		case '6':
			out = 0b0000001111111001;
			break;
		case '7':
			out = 0b0000000000001111;
			break;
		case '8':
			out = 0b0000001111111111;
			break;
		case '9':
			out = 0b0000001110101111;
			break;
		case '=':
			out = 0b0000001100110000;
			break;
		case '*':
			out = 0b1111111100000000;
			break;
		case '+':
			out = 0b0100101100000000;
			break;
		case '-':
			out = 0b0000001100000000;
			break;
		case '.':
			out = 0b10000000000000000;
			break;
		case '|':
			out = 0b0100100000000000;
			break;
		case '\\':
			out = 0b0010010000000000;
			break;
		case '/':
			out = 0b1001000000000000;
			break;
		case '_':
			out = 0b0000000000110000;
			break;
		case '?':
			out = 0b10100001000000111;
			break;
		case '!':
			out = 0b10000000000001100;
			break;
		case '(':
		case '[':
			out = 0b0100100000100010;
			break;
		case ')':
		case ']':
			out = 0b0100100000010001;
			break;
		case '<':
			out = 0b0011000000000000;
			break;
		case '>':
			out = 0b1000010000000000;
			break;
		case '{':
			out = 0b0100100100100010;
			break;
		case '}':
			out = 0b0100101000010001;
			break;

		case ' ':
		default: 
			out = 0;
		break;
	}

	return out;
}