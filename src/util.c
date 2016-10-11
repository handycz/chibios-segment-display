/* util.c -- Utilities for Segdisp library
 *
 * Copyright (C) 2016 Ondrej Novak
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include "util.h"
#include "string.h"

void utils_str_shift(char *str, int step){
	int len = strlen(str);
	int steps;
	int i;
	char tmp;

	if(len < 2)
		return;
	if(len == 2){
		tmp = str[0];
		str[0] = str[1];
		str[1] = tmp;
	}

	if(step < 0){
		for(steps = 0; steps < -step; steps++){
			tmp = str[len - 1];
			for(i = len - 1; i > 0; i--){
				str[i] = str[i-1]; 
			}
			str[0] = tmp;
		}
	}
	else if(step > 0){
		for(steps = 0; steps < step; steps++){
			tmp = str[0];
			for(i = 0; i < len; i++){
				str[i] = str[i+1]; 
			}
			str[len - 1] = tmp;
		}
	}
}