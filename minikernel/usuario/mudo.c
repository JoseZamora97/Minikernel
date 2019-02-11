/*
 * usuario/mudo.c
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 * Programa de usuario que "gasta CPU".
 */

#include "servicios.h"

#define TOT_ITER 2000000	/* ponga las que considere oportuno */

int main(){
	int i, tot;
	int j=5;

	for (i=0; i<TOT_ITER; i++){
		tot=j*i;
		printf("mudo(%d): %d\n", obtener_id_pr(), tot);
	}
		
	printf("mudo (%d): termina con %d\n", obtener_id_pr(), tot);
	return 0;
}
