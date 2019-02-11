/*
 *  minikernel/include/kernel.h
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene definiciones usadas por kernel.c
 *
 *      SE DEBE MODIFICAR PARA INCLUIR NUEVA FUNCIONALIDAD
 *
 */

#ifndef _KERNEL_H
#define _KERNEL_H

/* Definicion del tipo de Mutex */
#define NO_RECURSIVO 0
#define RECURSIVO 1
/* Definicion del estado del Mutex */
#define LIBRE 0
#define OCUPADO 1
/* Definicion de tipos de bloqueo de un  proceso */
#define BLOQUEO_DORMIR 0
#define BLOQUEO_MUTEX 1
#define BLOQUEO_RR 2
#define BLOQUEO_TERMINAL 3

#include "const.h"
#include "HAL.h"
#include "llamsis.h"
#include "string.h"

/*
 *
 * Definicion del tipo que corresponde con el BCP.
 * Se va a modificar al incluir la funcionalidad pedida.
 *
 */
typedef struct BCP_t *BCPptr;
typedef struct mutex_t * mutex_ptr;

typedef struct BCP_t {
    
	BCPptr siguiente;			/* puntero a otro BCP */
	int id;						/* ident. del proceso */
    int estado;					/* TERMINADO|LISTO|EJECUCION|BLOQUEADO*/
    contexto_t contexto_regs;	/* copia de regs. de UCP */
    void * pila;				/* dir. inicial de la pila */
	void *info_mem;				/* descriptor del mapa de memoria */
	long despertar_en; 			/* tiempo en "segundos" que el BCP tiene que desbloquearse "por tiempo". */
	mutex_ptr descriptores_mutex[NUM_MUT_PROC];
	int tick_round_robin;
} BCP;

typedef struct mutex_t {
	char * nombre;					/* nombre del mutex */
	int tipo;						/* RECURSIVO | NO RECURSIVO */
	int estado;						/* LIBRE | OCUPADO */
	int num_bloqueos;				/* numero de veces que se ha bloqueado llamando a lock(); */
	int id_proc_poseedor;
	
	// BUFFER DE PIDS 
	int procesos_bloqueados[MAX_PROC]; 	/* array de PID de procesos bloqueados por el mutex. */
	int in_insertar;
	int in_borrar;
	int num_procesos_bloqueados;	/* numero de procesos que se han bloqueado con lock() y no poseian el mutex. */

} mutex;


/*
 *
 * Definicion del tipo que corresponde con la cabecera de una lista
 * de BCPs. Este tipo se puede usar para diversas listas (procesos listos,
 * procesos bloqueados en sem�foro, etc.).
 *
 */

typedef struct{
	
	BCP *primero;
	BCP *ultimo;

} lista_BCPs;

typedef struct 
{
	char buffer_terminal[TAM_BUF_TERM];
	int in_borrar;
	int in_insertar;
	int num_elementos;
	
}buffer;

/*
 * Variable global que identifica el proceso actual
 */

BCP * p_proc_actual=NULL;


/*
 * Variable global que representa la tabla de procesos
 */

BCP tabla_procs[MAX_PROC];

/*
 * Variable global que representa la tabla de mutex
 */

mutex tabla_mutex[NUM_MUT];

/*
 * Variable global que representa el buffer del terminal
 */
buffer buffer_terminal;

/*
 * Variable global que representa la cola de procesos listos
 */
lista_BCPs lista_listos= {NULL, NULL};

/*
 * Variable global que representa la cola de procesos bloqueados
 */
lista_BCPs lista_bloqueados={NULL, NULL};

/*
 * Variable global que representa la cola de procesos bloqueados
 */
lista_BCPs lista_bloqueados_dormir={NULL, NULL};

/*
 * Variable global que representa la cola de procesos bloqueados por mutex.
 */
lista_BCPs lista_bloqueados_mutex_libre={NULL, NULL};

/*
 * Variable global que representa la cola de procesos bloqueados por terminal.
 */
lista_BCPs lista_bloqueados_terminal={NULL, NULL};

/*
 *
 * Definici�n del tipo que corresponde con una entrada en la tabla de
 * llamadas al sistema.
 *
 */
typedef struct{
	int (*fservicio)();
} servicio;


/*
 * Prototipos de las rutinas que realizan cada llamada al sistema
 */
int sis_crear_proceso();
int sis_terminar_proceso();
int sis_escribir();
int sis_obtener_id();
int sis_dormir();
int sis_crear_mutex();
int sis_abrir_mutex();
int sis_cerrar_mutex();
int sis_lock_mutex();
int sis_unlock_mutex();
int sis_leer_caracter();

void bloquear_proceso(BCP * proceso, int tipo);
void desbloquear_proceso(BCP * proceso, int tipo);
void inter_sw_fin_rodaja_RR();
void comprobar_fin_rodaja_RR();
void actualizar_tiempos();
void insertar_buffer(char car);
int es_buffer_vacio();
int es_buffer_lleno();
char borrar_buffer();
void imprimir_lista(lista_BCPs lista);
void liberar_mutex(int descriptor);
int aux_unlock_mutex(int descriptor);

// void asignar_mutex(char* nombre, int tipo, int in_desc, int in_t_mutex); // esta es para el tocho que hay en crear_mutex, no funciona :( .

/*
 * Variable global que contiene las rutinas que realizan cada llamada
 */
servicio tabla_servicios[NSERVICIOS]={	{sis_crear_proceso},
										{sis_terminar_proceso},
										{sis_escribir},
										{sis_obtener_id},
										{sis_dormir},
										{sis_crear_mutex},
										{sis_abrir_mutex},
										{sis_cerrar_mutex},
										{sis_lock_mutex},
										{sis_unlock_mutex},
										{sis_leer_caracter}
										};

#endif /* _KERNEL_H */

