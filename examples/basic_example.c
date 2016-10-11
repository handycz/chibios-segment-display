/* main.c -- display example application
 *
 * Copyright (C) 2016 Ondrej Novak
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */


#include "ch.h"
#include "hal.h"
#include "test.h"
#include "segdisp.h"
#include "string.h"

/* #define _16SEG */

/*
 * Application entry point.
 */
int main(void) {

  halInit();
  chSysInit();

  /* configure display */
  #ifdef _16SEG
  /* One digit connected to pin GPIOC 2  */
  static segdisp_pins_t digits = {
    number: 1,
    pins: {
      {pin: 2, port: GPIOC}
    }
  };

  static segdisp_pins_t segments = {
    number: 17,
    pins: {
      {pin: 10, port: GPIOE}, //A1
      {pin: 7, port: GPIOE}, //A2
      {pin: 6, port: GPIOE}, //B
      {pin: 2, port: GPIOE}, //C
      {pin: 15, port: GPIOC}, //D1
      {pin: 0, port: GPIOE}, //D2
      {pin: 14, port: GPIOC}, //E
      {pin: 13, port: GPIOE}, //F
      {pin: 11, port: GPIOE}, //G1
      {pin: 4, port: GPIOE}, //G2
      {pin: 15, port: GPIOE}, //J
      {pin: 9, port: GPIOE}, //K
      {pin: 8, port: GPIOE}, //L
      {pin: 3, port: GPIOE}, //M
      {pin: 12, port: GPIOE}, //N
      {pin: 14, port: GPIOE}, //P
      {pin: 13, port: GPIOC} //DP
    }
  };

  #else
  /* 
    There's 4 digits, 
    common electrodes are connected to 
    GPIOE's pads 10, 9, 8, 7
  */
  static segdisp_pins_t digits = {
    number: 4,
    pins: {
      {pin: 10, port: GPIOE},
      {pin: 9, port: GPIOE},
      {pin: 8, port: GPIOE},
      {pin: 7, port: GPIOE}
    }
  };

  /* 
    There's 7 segments connected to 
    GPIOA's pads
    0, 1, 2, 3, 4, 5, 6
  */
  
  static segdisp_pins_t segments = {
    number: 7,
    pins:{
      {pin: 0, port: GPIOA},
      {pin: 1, port: GPIOA},
      {pin: 2, port: GPIOA},
      {pin: 3, port: GPIOA},
      {pin: 4, port: GPIOA},
      {pin: 5, port: GPIOA},
      {pin: 6, port: GPIOA}
    }
  };

  #endif


  static segdisp_t disp;

  /* In case you want to enable automatic scrolling, configure this structure */
  static segdisp_scroll_conf_t scroll = {
    delay: 1000, /* delay (milliseconds) between steps */
    step: 1/* how many letters shift in one step */
  };

  disp.scroll = &scroll;
  /* to change refresh rate (microseconds): */
  /* disp.refresh = 2500; // default value is 5000 */

  /* initialize and run */
  #ifdef _16SEG
  segdisp_init(&disp, &segments, &digits, SEGDISP_INVERTED_SEGMENT | SEGDISP_NONINVERTED_DRIVER | SEGDISP_SEGMENTS_SIXTEEN);
  #else
  segdisp_init(&disp, &segments, &digits, SEGDISP_INVERTED_SEGMENT | SEGDISP_NONINVERTED_DRIVER | SEGDISP_SEGMENTS_SEVEN);
  #endif

  char input[50];
  char input2[10];
  strcpy(input, "-123- ");
  strcpy(input2, "12345 ");

  /* set output string */
  segdisp_set_str(&disp, input);
  /* run it */
  segdisp_run(&disp, NORMALPRIO+5);
  

  /* run the scrolling*/
  segdisp_scroll_run(&disp, NORMALPRIO+2);
  chThdSleepMilliseconds(5600);

  /* after 5600 ms change the output string and stop scrolling */
  segdisp_set_str(&disp, input2);
  segdisp_scroll_stop(&disp);
  chThdSleepMilliseconds(1000);

  /* after 1000 ms move text manually 2 positions to the left */
  segdisp_move_cont(&disp, 2);

  /* after another 1000 ms, shut down everything */
  chThdSleepMilliseconds(1000);
  segdisp_stop(&disp);

  while (true){
    chThdSleepMilliseconds(1000);
  }
}
