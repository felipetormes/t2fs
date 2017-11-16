#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t2fs.h"

int cidentify(char *name, int size){

  /* Check if internal variables was initialized */
	if(control.init == FALSE)
		init();

  char* str ="\nAlfeu Uzai Tavares\nEduardo Bassani Chandelier\nFelipe Barbosa Tormes\n\n";

  memcpy(name, str, size);
	if (size == 0)
		name[0] = '\0';
	else
		name[size-1] = '\0';
	return 0;

}
