	/*
 *  kernel/kernel.c
 *
 *  Minikernel. Version 2.0
 *
 *  Alex Quesada Mendo , David Correas Oliver , Jose Miguel Zamora Batista.
 *
 */

/*
 *
 * Fichero que contiene la funcionalidad del sistema operativo
 *
 */

#include "kernel.h"	/* Contiene defs. usadas por este modulo */

/*
 *
 * Funciones relacionadas con la tabla de priocesos:
 *	iniciar_tabla_proc buscar_BCP_libre
 *
 */

/*
 * Funci�n que inicia la tabla de procesos
 */
static void iniciar_tabla_proc()
{
	int i;

	for (i=0; i<MAX_PROC; i++)
		tabla_procs[i].estado=NO_USADA;
}

/*
 * Funci�n que busca una entrada libre en la tabla de procesos
 */
static int buscar_BCP_libre()
{
	int i;

	for (i=0; i<MAX_PROC; i++)
		if (tabla_procs[i].estado==NO_USADA)
			return i;
	return -1;
}

/*
 *
 * Funciones que facilitan el manejo de las listas de BCPs
 *	insertar_ultimo eliminar_primero eliminar_elem
 *
 * NOTA: PRIMERO SE DEBE LLAMAR A eliminar Y LUEGO A insertar
 */

/*
 * Inserta un BCP al final de la lista.
 */
static void insertar_ultimo(lista_BCPs *lista, BCP * proc)
{
	if (lista->primero==NULL)
		lista->primero= proc;
	else
		lista->ultimo->siguiente=proc;
	lista->ultimo= proc;
	proc->siguiente=NULL;
}

/*
 * Elimina el primer BCP de la lista.
 */
static void eliminar_primero(lista_BCPs *lista)
{

	if (lista->ultimo==lista->primero)
		lista->ultimo=NULL;
	lista->primero=lista->primero->siguiente;
}

/*
 * Elimina un determinado BCP de la lista.
 */
static void eliminar_elem(lista_BCPs *lista, BCP * proc)
{
	BCP *paux=lista->primero;

	if (paux==proc)
		eliminar_primero(lista);
	else 
	{
		for ( ; ((paux) && (paux->siguiente!=proc));
			paux=paux->siguiente);
		if (paux) 
		{
			if (lista->ultimo==paux->siguiente)
				lista->ultimo=paux;
			paux->siguiente=paux->siguiente->siguiente;
		}
	}
}

/*
 *
 * Funciones relacionadas con la planificacion
 *	espera_int planificador
 */

/*
 * Espera a que se produzca una interrupcion
 */
static void espera_int()
{
	int nivel;

	//printk("-> NO HAY LISTOS. ESPERA INT\n");

	/* Baja al m�nimo el nivel de interrupci�n mientras espera */
	nivel=fijar_nivel_int(NIVEL_1);
	halt();
	fijar_nivel_int(nivel);
}

/*
 * Funci�n de planificacion que implementa un algoritmo FIFO.
 */
static BCP * planificador()
{
	while (lista_listos.primero==NULL)
		espera_int();		/* No hay nada que hacer */
		
	return lista_listos.primero;
}

/*
 *
 * Funcion auxiliar que termina proceso actual liberando sus recursos.
 * Usada por llamada terminar_proceso y por rutinas que tratan excepciones
 *
 */
static void liberar_proceso()
{
	BCP * p_proc_anterior;

	liberar_imagen(p_proc_actual->info_mem); /* liberar mapa */

	p_proc_actual->estado=TERMINADO;
	eliminar_primero(&lista_listos); /* proc. fuera de listos */

	/* Realizar cambio de contexto */
	p_proc_anterior=p_proc_actual;
	p_proc_actual=planificador();

	printk("-> C.CONTEXTO POR FIN: de %d a %d\n",
			p_proc_anterior->id, p_proc_actual->id);

	liberar_pila(p_proc_anterior->pila);
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
        return; /* no deber�a llegar aqui */
}

/*
 *
 * Funciones relacionadas con el tratamiento de interrupciones
 *	excepciones: exc_arit exc_mem
 *	interrupciones de reloj: int_reloj
 *	interrupciones del terminal: int_terminal
 *	llamadas al sistemas: llam_sis
 *	interrupciones SW: int_sw
 *
 */

/*
 * Tratamiento de excepciones aritmeticas
 */
static void exc_arit()
{

	if (!viene_de_modo_usuario())
		panico("excepcion aritmetica cuando estaba dentro del kernel");

	printk("-> EXCEPCION ARITMETICA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de excepciones en el acceso a memoria
 */
static void exc_mem()
{

	if (!viene_de_modo_usuario())
		panico("excepcion de memoria cuando estaba dentro del kernel");


	printk("-> EXCEPCION DE MEMORIA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de interrupciones de terminal
 */



static void int_terminal()
{
	char car;

	car = leer_puerto(DIR_TERMINAL);
	printk("-> TRATANDO INT. DE TERMINAL %c\n", car);

	if (!es_buffer_lleno()) //Si el buffer NO está lleno
	{
		insertar_buffer(car);

		if(lista_bloqueados_terminal.primero!=NULL)
			desbloquear_proceso(lista_bloqueados_terminal.primero, BLOQUEO_TERMINAL);		
	}
	//else -> ignora el caracter porque el buffer esta lleno
}

int sis_leer_caracter()
{
	char borrado;
	int nivel_int;

	if(es_buffer_vacio())		//si el buffer está vacio
	{ 	
		
		nivel_int=fijar_nivel_int(2);
		
		bloquear_proceso(p_proc_actual, BLOQUEO_TERMINAL);
		
		BCP * p_proc_anterior;
		p_proc_anterior=p_proc_actual;
		p_proc_actual=planificador();
		
		fijar_nivel_int(nivel_int);

		cambio_contexto(&(p_proc_anterior->contexto_regs), 				// cambio de contexto.		
						&(p_proc_actual->contexto_regs));
	}

	borrado = borrar_buffer();

	return (int)borrado;
}

void insertar_buffer(char car)
{
	buffer_terminal.num_elementos++;
	buffer_terminal.buffer_terminal[buffer_terminal.in_insertar] = car;
	buffer_terminal.in_insertar =(buffer_terminal.in_insertar +1)%TAM_BUF_TERM;
}

char borrar_buffer ()
{	

	char borrado;

	if(!es_buffer_vacio())
	{
		borrado = buffer_terminal.buffer_terminal[buffer_terminal.in_borrar];

		buffer_terminal.buffer_terminal[buffer_terminal.in_borrar] = '\0';	
		buffer_terminal.in_borrar = (buffer_terminal.in_borrar + 1)%TAM_BUF_TERM;
		buffer_terminal.num_elementos--;
	}
	else
		borrado = '\0';
	
	return borrado;
}

/*
 * Devuelve 1 si el buffer SÍ está lleno
 * Devuelve 0 si el buffer NO está lleno 
 */
int es_buffer_lleno()
{
	return buffer_terminal.num_elementos == TAM_BUF_TERM;
}

/*
 * Devuelve 1 si el buffer SÍ está vacio
 * Devuelve 0 si el buffer NO está vacio (hay elementos dentro del buffer)
 */
int es_buffer_vacio()
{
	return buffer_terminal.num_elementos == 0;
}

/**
 * Inicializa el buffer con caracteres vacíos
 */
static void iniciar_buffer()
{
	int i = 0;
	for(;i<TAM_BUF_TERM;i++)
	{
		buffer_terminal.buffer_terminal[i] = '\0';
	}

	buffer_terminal.num_elementos = 0;
	buffer_terminal.in_borrar = 0;
	buffer_terminal.in_insertar = 0;
}

/*
 * Tratamiento de interrupciuones software
 */
static void int_sw()
{
	printk("-> TRATANDO INT. SW\n");
	imprimir_lista(lista_listos);
	inter_sw_fin_rodaja_RR();
	return;
}

void inter_sw_fin_rodaja_RR()
{
	
	int nivel_int = fijar_nivel_int(1);

	BCP * proc_a_bloquear=p_proc_actual;
	proc_a_bloquear->tick_round_robin = TICKS_POR_RODAJA;
	
	printk("Proceso (%d): se bloquea.\n", p_proc_actual->id);
	bloquear_proceso(proc_a_bloquear, BLOQUEO_RR);

	p_proc_actual=planificador();
	
	fijar_nivel_int(nivel_int);
	
	cambio_contexto(&(proc_a_bloquear->contexto_regs), 
					&(p_proc_actual->contexto_regs));
}

void comprobar_fin_rodaja_RR()
{
	if (p_proc_actual->tick_round_robin == 0)	// Si no tiene llamamos a la interrupción software.
		int_sw();								
	else{
		p_proc_actual->tick_round_robin--;		// Si tiene le restamos 1.
	}	
}

/*
 * Tratamiento de interrupciones de reloj
 */
static void int_reloj()
{

	printk("-> TRATANDO INT. DE RELOJ\n");

	/* 
	 * Comprobamos si el proceso actual tiene ticks que ejecutar.
	 * Para probar Round Robin 1, descomentar.
	 * Para probar cualquier otro, comentar.
	 */
	//comprobar_fin_rodaja_RR();
	
	// Actualizar tiempos de los proceso dormidos.
	actualizar_tiempos();

    return;
}

void actualizar_tiempos(){
	BCP * BCPptr_recorredor;
	BCPptr_recorredor = lista_bloqueados_dormir.primero;

	while(BCPptr_recorredor!=NULL)
	{	
		//printk("Proceso(%d) Dormido, despierta en: (%d) TICKS\n", BCPptr_recorredor->id, BCPptr_recorredor->despertar_en);
		if(BCPptr_recorredor->despertar_en <= 0)
		{
			if(BCPptr_recorredor==lista_bloqueados_dormir.primero)		// Si ha llegado a cero, y es el primero.
			{					
				if(BCPptr_recorredor->siguiente != NULL)
					BCPptr_recorredor->siguiente->despertar_en = BCPptr_recorredor->siguiente->despertar_en - 1;

				desbloquear_proceso(BCPptr_recorredor, BLOQUEO_DORMIR);
				
				if(lista_bloqueados_dormir.primero!=NULL)
					BCPptr_recorredor = lista_bloqueados_dormir.primero;
				else
					return;
			}
			else
			{	
				if(BCPptr_recorredor->siguiente != NULL)
					BCPptr_recorredor->siguiente->despertar_en = BCPptr_recorredor->siguiente->despertar_en - 1;

				desbloquear_proceso(BCPptr_recorredor, BLOQUEO_DORMIR);	
			}		
		}
		else
			BCPptr_recorredor->despertar_en = BCPptr_recorredor->despertar_en - 1;	// Reducimos en 1 tick el tiempo que tiene que esperar.
			
		BCPptr_recorredor = BCPptr_recorredor->siguiente;
	}
}

/*
 * Tratamiento de llamadas al sistema
 */
static void tratar_llamsis()
{
	int nserv, res;

	nserv=leer_registro(0);
	if (nserv<NSERVICIOS)
		res=(tabla_servicios[nserv].fservicio)();
	else
		res=-1;		/* servicio no existente */
	escribir_registro(0,res);
	return;
}


/*
 *
 * Funcion auxiliar que crea un proceso reservando sus recursos.
 * Usada por llamada crear_proceso.
 *
 */
static int crear_tarea(char *prog)
{
	void * imagen, *pc_inicial;
	int error=0;
	int proc;
	BCP *p_proc;

	proc=buscar_BCP_libre();
	if (proc==-1)
		return -1;	/* no hay entrada libre */

	/* A rellenar el BCP ... */
	p_proc=&(tabla_procs[proc]);

	/* crea la imagen de memoria leyendo ejecutable */
	imagen=crear_imagen(prog, &pc_inicial);
	if (imagen)
	{
		p_proc->info_mem=imagen;
		p_proc->pila=crear_pila(TAM_PILA);
		fijar_contexto_ini(p_proc->info_mem, p_proc->pila, TAM_PILA,
			pc_inicial,
			&(p_proc->contexto_regs));
		p_proc->id=proc;
		p_proc->estado=LISTO;
		p_proc->tick_round_robin = TICKS_POR_RODAJA;		// <---------------------esto es nuevo

		/* lo inserta al final de cola de listos */
		insertar_ultimo(&lista_listos, p_proc);
		error= 0;
	}
	else
		error= -1; /* fallo al crear imagen */

	return error;
}

/*
 *
 * Rutinas que llevan a cabo las llamadas al sistema
 *	sis_crear_proceso sis_escribir
 *
 */

/*
 * Tratamiento de llamada al sistema crear_proceso. Llama a la
 * funcion auxiliar crear_tarea sis_terminar_proceso
 */
int sis_crear_proceso()
{
	char *prog;
	int res;

	printk("-> PROC %d: CREAR PROCESO\n", p_proc_actual->id);
	prog=(char *)leer_registro(1);
	res=crear_tarea(prog);
	
	return res;
}

/*
 * Tratamiento de llamada al sistema escribir. Llama simplemente a la
 * funcion de apoyo escribir_ker
 */
int sis_escribir()
{
	char *texto;
	unsigned int longi;

	texto=(char *)leer_registro(1);
	longi=(unsigned int)leer_registro(2);

	escribir_ker(texto, longi);
	return 0;
}

/*
 * Tratamiento de llamada al sistema terminar_proceso. Llama a la
 * funcion auxiliar liberar_proceso y cierra los mutex asociados a ese proceso
 */
int sis_terminar_proceso()
{
	int i, resultado; 
	printk("-> FIN PROCESO %d\n", p_proc_actual->id);

	mutex_ptr mutex_recorredor;

	for (i = 0; i<NUM_MUT_PROC;i++)
	{
		mutex_recorredor = p_proc_actual->descriptores_mutex[i];
		if (mutex_recorredor != NULL){

			p_proc_actual->descriptores_mutex[i]->num_bloqueos=0;
			resultado = aux_unlock_mutex(i);
			if (p_proc_actual ->descriptores_mutex[i]->num_procesos_bloqueados == 0)
				liberar_mutex(i);
		}	
	}

	liberar_proceso();
	resultado = 0;
    return resultado; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de llamada al sistema obtener_id_pr.
 */
int sis_obtener_id(){return p_proc_actual->id;}


/* 
 * Funciones auxiliares para bloquear procesos
 * - tipo = {BLOQUEO_DORMIR, BLOQUEO_MUTEX}
 */

void bloquear_proceso(BCP * proceso , int tipo)
{
	
	proceso->estado=BLOQUEADO;									// cambiamos estado a bloqueado
	eliminar_primero(&lista_listos);							// eliminamos el primero de la lista listos

	switch(tipo)
	{
		case BLOQUEO_DORMIR:
			insertar_ultimo(&lista_bloqueados_dormir, proceso);					// insertar en bloqueados_dormir.
			break;
		case BLOQUEO_MUTEX:
			insertar_ultimo(&lista_bloqueados_mutex_libre, proceso);			// insertar en bloqueados_mutex.
			break;
		case BLOQUEO_RR:
			proceso->estado = LISTO;							// cambiamos estado a listo
			insertar_ultimo(&lista_listos, proceso);			// insertamos al final de la cola de listos.
			break;
		case BLOQUEO_TERMINAL:
			insertar_ultimo(&lista_bloqueados_terminal, proceso);
			break;
		default:
			break;
	}

	return;
}

void desbloquear_proceso(BCP * proceso, int tipo)
{
	
	switch(tipo)
	{
		case BLOQUEO_DORMIR:
			eliminar_elem(&lista_bloqueados_dormir, proceso);		// sacamos de la lista de bloqueados_dorir.
			break;
		case BLOQUEO_MUTEX:
			eliminar_elem(&lista_bloqueados_mutex_libre, proceso);	// sacamos de la lista de bloqueados_mutex.
			break;
		case BLOQUEO_TERMINAL:	
			eliminar_elem(&lista_bloqueados_terminal, proceso);		//sacasmo de la lista bloqueados_terminal.
			break;
		default:
			break;
	}
	
	insertar_ultimo(&lista_listos, proceso);					// añadimos a listos.
	proceso->estado=LISTO;										// cambiamos estado a listo.
	
	return;
}

void imprimir_lista(lista_BCPs lista)
{
	BCP * BCPptr_recorredor = lista.primero;
	printk("-> Lista: {");
	
	while(BCPptr_recorredor){
		printk(" Proceso:(%d) ", BCPptr_recorredor->id);
		BCPptr_recorredor = BCPptr_recorredor->siguiente;
	}
	printk("}\n");
}

/*
 * Tratamiento de llamada al sistema dormir.
 */
int sis_dormir()
{
		
	unsigned int segundos;
	segundos=(unsigned int)leer_registro(1);
	
	int nivel_int = fijar_nivel_int(3);

	BCP * p_proc_anterior;
	p_proc_anterior=p_proc_actual;
	p_proc_anterior->despertar_en = (long)segundos*TICK;
	
	bloquear_proceso(p_proc_actual, BLOQUEO_DORMIR);				// llamamos a bloquear proceso.

	p_proc_actual=planificador();
	
	fijar_nivel_int(nivel_int);

	cambio_contexto(&(p_proc_anterior->contexto_regs), 				// cambio de contexto.		
					&(p_proc_actual->contexto_regs));	
		
	return 0;
}

/* Funciones auxiliares para mutex */

static void iniciar_tabla_mutex()
{
	int i;
	for(i = 0; i < NUM_MUT; i++)
		tabla_mutex[i].estado = LIBRE; /* indica que el mutex esta libre */
}

static int buscar_descriptor_libre()
{
	int i;
	for(i = 0; i < NUM_MUT_PROC; i++)
	{
		if(p_proc_actual->descriptores_mutex[i] == NULL)
			return i; /* devuelve el numero del descriptor */
	}
	
	return -1; /* no hay descriptor libre */
}

static int buscar_nombre_mutex(char *nombre_mutex)
{
	int i;
	for(i = 0; i < NUM_MUT; i++)
	{
		if(tabla_mutex[i].estado == OCUPADO && strcmp(tabla_mutex[i].nombre, nombre_mutex) == 0)
			return i;	/* el nombre existe y devuelve su posicion en la tabla de mutex */
	}
	
	return -1; /* el nombre no existe */
}

static int buscar_mutex_libre()
{
	int i;
	for(i = 0; i < NUM_MUT; i++)
	{
		if(tabla_mutex[i].estado == LIBRE)
			return i;	/* el mutex esta libre y devuelve su posicion en la tabla de mutex */
	}
	
	return -1; /* no hay mutex libre */
}

void asignar_mutex(char* nombre, int tipo, int in_descr, int in_t_mutex)
{
	tabla_mutex[in_descr].nombre = nombre; 			// se copia el nombre al mutex
	tabla_mutex[in_descr].tipo = tipo; 				// se asigna el tipo
	
	tabla_mutex[in_descr].estado = OCUPADO; 		// se cambia el estado a OCUPADO
	tabla_mutex[in_descr].num_bloqueos = 0;
	tabla_mutex[in_descr].num_procesos_bloqueados = 0;
	
	p_proc_actual->descriptores_mutex[in_descr] = &tabla_mutex[in_t_mutex]; // el descr apunta al mutex libre en la tabla
}

/*
 * Llamada al sistema crear_mutex
 * par·metro nombre en registro 1
 * par·metro tipo en registro 2
 */
int sis_crear_mutex()
{
	char * nombre = (char*)leer_registro(1);
	int tipo = (int)leer_registro(2);

	if(strlen(nombre) > MAX_NOM_MUT)
	{
		printk("(SIS_CREAR_MUTEX) Error: Nombre de mutex demasiado largo\n");
		return -1;
	}

	if(buscar_nombre_mutex(nombre) >= 0)
	{
		printk("(SIS_CREAR_MUTEX) Error: Nombre de mutex ya existente\n");
		return -2;
	}

	int descr_mutex_libre = buscar_descriptor_libre();
	
	printf("encontro descriptor %d\n", descr_mutex_libre);
	
	if(descr_mutex_libre < 0)
	{
		printk("(SIS_CREAR_MUTEX) Error: No hay descriptores de mutex libres\n");
		return -3;
	}
	else
		printk("(SIS_CREAR_MUTEX) Devolviendo descriptor numero %d\n", descr_mutex_libre);


	printk("KERNEL: Busca mutex libre\n");
	
	int pos_mutex_libre = buscar_mutex_libre();
	
	if(pos_mutex_libre < 0)
	{
		printk("(SIS_CREAR_MUTEX) No hay mutex libre, bloqueando proceso\n");
		
		// bloquear a la espera de que quede alguno libre
		BCP * proc_a_bloquear=p_proc_actual;
		int nivel_int = fijar_nivel_int(3);
					
		bloquear_proceso(proc_a_bloquear, BLOQUEO_MUTEX);

		p_proc_actual=planificador();
		fijar_nivel_int(nivel_int);
		
		cambio_contexto(&(proc_a_bloquear->contexto_regs), 
						&(p_proc_actual->contexto_regs));
	}
	else
	{
		tabla_mutex[pos_mutex_libre].nombre = nombre; // se copia el nombre al mutex
		tabla_mutex[pos_mutex_libre].tipo = tipo; // se asigna el tipo
		tabla_mutex[pos_mutex_libre].estado = OCUPADO; // se cambia el estado a OCUPADO
		tabla_mutex[pos_mutex_libre].num_bloqueos = 0;
		//jose mira esto
		tabla_mutex[pos_mutex_libre].in_insertar = 0;
		tabla_mutex[pos_mutex_libre].in_borrar = 0;
		tabla_mutex[pos_mutex_libre].num_procesos_bloqueados = 0;
		p_proc_actual->descriptores_mutex[descr_mutex_libre] = &tabla_mutex[pos_mutex_libre]; // el descr apunta al mutex libre en la tabla
	}			
	
	return descr_mutex_libre;
}


/*
 * LLamada al sistema abrir mutex.
 */
int sis_abrir_mutex()
{
	char * nombre = (char *)leer_registro(1);
	
	// Compruebo si hay hay descriptores libres
	int descriptor = buscar_descriptor_libre();
	if(descriptor == -1)
		return -1;

	// Compruebo si hay mutex con el mismo nombre
	int posicion_tabla = buscar_nombre_mutex(nombre);
	if(posicion_tabla == -1)
		return -1;

	p_proc_actual->descriptores_mutex[descriptor] = &tabla_mutex[posicion_tabla];
	
	return descriptor;
}

/*
 * LLamada al sistema abrir mutex.
 */
int sis_cerrar_mutex()
{
	unsigned int descriptor = (unsigned int) leer_registro(1);
	if(p_proc_actual->descriptores_mutex[descriptor] == NULL)
		return -1;

	liberar_mutex(descriptor);

	return 0;
}

/*
 * Funcion auxiliar para cerrar mutex
 */
void liberar_mutex (int descriptor)
{
	p_proc_actual->descriptores_mutex[descriptor]->nombre = NULL;
	p_proc_actual->descriptores_mutex[descriptor]->estado = LIBRE;
	p_proc_actual->descriptores_mutex[descriptor] = NULL;
}

/*
 * LLamada al sistema lock mutex.
 */
int sis_lock_mutex()
{
	unsigned int descriptor = (unsigned int)leer_registro(1);
	
	if (p_proc_actual->descriptores_mutex[descriptor] == NULL)
		return -1; 
	
	// Numero de veces que se ha bloqueado llamando a lock

	if (p_proc_actual->descriptores_mutex[descriptor]->num_bloqueos == 0)
	{
		p_proc_actual->descriptores_mutex[descriptor]->id_proc_poseedor = p_proc_actual->id;
		p_proc_actual->descriptores_mutex[descriptor]->num_bloqueos++;
	}
	else
	{
		if (p_proc_actual->id == p_proc_actual->descriptores_mutex[descriptor]->id_proc_poseedor)
		{
			if (p_proc_actual->descriptores_mutex[descriptor]->tipo == RECURSIVO)
				p_proc_actual->descriptores_mutex[descriptor]->num_bloqueos++;
			else
				return -1; // hubo fail.
		}
		else
		{
			p_proc_actual->descriptores_mutex[descriptor]->num_procesos_bloqueados++;

			//int nbloqueos = p_proc_actual->descriptores_mutex[descriptor]->num_procesos_bloqueados;
			//jose mira esto
			p_proc_actual->descriptores_mutex[descriptor]->procesos_bloqueados[p_proc_actual->descriptores_mutex[descriptor]->in_insertar] = p_proc_actual->id;
			p_proc_actual->descriptores_mutex[descriptor]->in_insertar++;

			int nivel_int=fijar_nivel_int(3);

			bloquear_proceso(p_proc_actual, BLOQUEO_MUTEX);
			
			BCP * p_proc_anterior;
			p_proc_anterior=p_proc_actual;
			p_proc_actual=planificador();
			
			fijar_nivel_int(nivel_int);

			cambio_contexto(&(p_proc_anterior->contexto_regs), 				// cambio de contexto.		
							&(p_proc_actual->contexto_regs));
		}
	}

	return 0;
}

/*
 * Llamada auxiliar de sis_unlock_mutex()
 */
int aux_unlock_mutex(int descriptor)
{
	if (p_proc_actual->descriptores_mutex[descriptor] == NULL)
		return -1; 

	if(p_proc_actual->id != p_proc_actual->descriptores_mutex[descriptor]->id_proc_poseedor)
		return 0;
	
	p_proc_actual->descriptores_mutex[descriptor]->num_bloqueos--;
	
	if(p_proc_actual->descriptores_mutex[descriptor]->num_bloqueos <= 0)
	{
		//jose mira esto:
		int pid_desbloqueo = p_proc_actual->descriptores_mutex[descriptor]->procesos_bloqueados[p_proc_actual->descriptores_mutex[descriptor]->in_borrar];
		p_proc_actual->descriptores_mutex[descriptor]->in_borrar ++;

		BCPptr BCPptr_recorredor = lista_bloqueados_mutex_libre.primero;
		while (BCPptr_recorredor != NULL)
		{
			if (BCPptr_recorredor->id == pid_desbloqueo)
			{
				desbloquear_proceso(BCPptr_recorredor, BLOQUEO_MUTEX);
				break;
			}	
			BCPptr_recorredor = BCPptr_recorredor -> siguiente;
		}
	}
	return 0;
}


/*
 * LLamada al sistema unlock mutex.
 */
int sis_unlock_mutex()
{
	unsigned int descriptor = (unsigned int)leer_registro(1);
	
	return aux_unlock_mutex(descriptor);
}

/*
 *
 * Rutina de inicializaci�n invocada en arranque
 *
 */
int main(){
	/* se llega con las interrupciones prohibidas */

	instal_man_int(EXC_ARITM, exc_arit); 
	instal_man_int(EXC_MEM, exc_mem); 
	instal_man_int(INT_RELOJ, int_reloj); 
	instal_man_int(INT_TERMINAL, int_terminal); 
	instal_man_int(LLAM_SIS, tratar_llamsis); 
	instal_man_int(INT_SW, int_sw); 

	iniciar_cont_int();		/* inicia cont. interr. */
	iniciar_cont_reloj(TICK);	/* fija frecuencia del reloj */
	iniciar_cont_teclado();		/* inici cont. teclado */

	iniciar_buffer();			/* inicia Buffer de terminal */
	iniciar_tabla_proc();		/* inicia BCPs de tabla de procesos */
	iniciar_tabla_mutex();		/* inicia mutexs de tabla de mutex */

	/* crea proceso inicial */
	if (crear_tarea((void *)"init")<0)
		panico("no encontrado el proceso inicial");
	
	/* activa proceso inicial */
	p_proc_actual=planificador();
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
	panico("S.O. reactivado inesperadamente");
	return 0;
}


