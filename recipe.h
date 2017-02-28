#ifndef RECIPE_H
#define RECIPE_H

#define MOV 			0x20
#define LOOP 			0x80
#define ENDLOOP		0xA0
#define WAIT			0x40
#define RECIPEEND	0x00


uint8_t recipeTest[20] = {	MOV 	+ 0,	//mov0
														MOV 	+ 5,	//mov5
														MOV 	+ 0,	//mov0
														MOV 	+ 3,	//mov3
														LOOP 	+ 0,	//loop0
														MOV 	+ 1,	//mov1
														MOV 	+ 4,	//mov4
														ENDLOOP	 ,	//end loop
														MOV 	+ 0,	//mov0
														MOV 	+ 2,	//mov2
														WAIT 	+ 0,	//wait0
														MOV 	+ 3,	//mov3
														WAIT 	+ 0,	//wait0
														MOV 	+ 2,	//mov2
														MOV 	+ 3,	//mov3
														WAIT 	+ 31,	//wait31
														WAIT 	+ 31,	//wait31
														WAIT 	+ 31,	//wait31
														MOV 	+ 4,	//mov4
														RECIPEEND	};//end recipe


//Time in us
uint16_t pwmDuty[6] = {525, 738, 1089, 1439, 1790, 2040 };


#endif
