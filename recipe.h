#ifndef RECIPE_H
#define RECIPE_H

//Sevo Positions
uint16_t pwm_pos[6] = {};//todo

char recipeTest[20] = {	0b00100000,//mov0
						0b00100101,//mov5
						0b00100000,//mov0
						0b00100011,//mov3
						0b10000000,//loop0
						0b00100001,//mov1
						0b00100100,//mov4
						0b10100000,//end loop
						0b00100000,//mov0
						0b00100010,//mov2
						0b01000000,//wait0
						0b00100011,//mov3
						0b01000000,//wait0
						0b00100010,//mov2
						0b00100011,//mov3
						0b01011111,//wait31
						0b01011111,//wait31
						0b01011111,//wait31
						0b00100100,//mov4
						0b00000000}//end recipe






#endif
