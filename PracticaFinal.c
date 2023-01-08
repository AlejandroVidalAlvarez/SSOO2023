#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

//Estructuras
struct Clientes{
    char id[20];
    //Estado del cliente: 0 = no atendido, 1 = en proceso de atencion y 2 = terminado de atender.
    int atendido;
    // a = clientes con problemas en la app y r = clientes con problemas de red.
    char tipo;
    //Su valor va desde 1 hasta 10(ambos incluidos), siendo 1 la prioridad mas baja y 10 la más alta.
    int prioridad;
    //SOLO CLINETES DE RED 0=todo en regla, 1=mal identificados y 2=compañia equivocada
    int tipoDeAtencion;
    //Hilo que ejecutan los clientes
    pthread_t hiloCliente;
};

struct Tecnico{
    //Identificador para los tecnicos, como en este caso solo se disponen de 2 tecnicos, su valor sera 1 o 2.
    int id;
    //Contador de clientes  atendidos, en el momento que llegue a 5, se tomará un descanso de 5 segundos
    int count;
    //Hilo que ejecutan los tecnicos
    pthread_t hiloTecnico;
};

struct ResponsableReps{
    //Identificador que funciona igual que el del tecnico, su valor sera 1 o 2 puesto que solo se disponen de 2 responsables.
    int id;
    //Contador de clientes  atendidos, en el momento que llegue a 6, se tomará un descanso de 6 segundos
    int count;
    //Hilo que ejecutan los responsables
    pthread_t hiloResponsable;
};
//Semaforos y variables condición
pthread_mutex_t semaforoFichero;
pthread_mutex_t semaforoColaClientes;
pthread_mutex_t semaforoSolicitudes;


//Variables globales
int peticionesMax;
int contadorPeticiones;
int numTecnicos;
int numResponsables;
int contadorClientesApp;
int contadorClientesRed;
int nSolicitudesDomiciliarias;
struct Clientes *listaClientes;
FILE *ficheroLogs;
struct Tecnico *listaTecnicos;
struct ResponsableReps *listaResponsables;


//Definicion de las funciones
int calculaAleatorio(int inicio, int fin);
void escribirEnLog(char *id, char *mensaje);
void nuevoClienteRed();
void nuevoClienteApp();
void *accionesCliente();
void *accionesTecnico(void *arg);
void *accionesEncargado(void *arg);
void *accionesTecnicoDomiciliario(void *arg);
void *accionesresponsablesReparacion(void *arg);
void manejadora_fin();
int buscarPrioridad(char tipo);

int main(int argc, char *argv[]){

	struct sigaction sapp;
	sapp.sa_handler = nuevoClienteApp;
    if(-1==sigaction(SIGUSR1, &sapp,NULL)){
	      perror("ClienteAPP: sigaction\n");
	      return -1;}
	struct sigaction sred;
	sred.sa_handler = nuevoClienteRed;
	if(-1 == sigaction(SIGUSR2, &sred, NULL)){
		perror("ClienteRED: sigaction\n");
		exit(-1);
	}
    struct sigaction sfin;
	sfin.sa_handler = manejadora_fin;
	if( -1 == sigaction(SIGINT, &sfin, NULL) ){
		perror("Fin Solicitudes: sigaction\n");
		exit(-1);
	}
    //Asignar valores por defecto a algunas variables
    peticionesMax = 20;
    contadorPeticiones = 0;
    numTecnicos = 2;
    numResponsables = 2;
    contadorClientesApp = 0;
    contadorClientesRed = 0;
    nSolicitudesDomiciliarias = 0;
    
    //Definicion de los punteros de las listas de clientes, tecnicos y responsables
    listaClientes =(struct Clientes*) malloc(sizeof(struct Clientes) * peticionesMax);
    listaTecnicos =(struct Tecnico*) malloc(sizeof(struct Tecnico) * numTecnicos);
    listaResponsables =(struct ResponsableReps*) malloc(sizeof(struct ResponsableReps) * numResponsables);
    srand(getpid());
    
    
    //Controlamos argumentos programa
	switch(argc){
		case 1:
            break;
		default:
            /* Si se introducen mas de 2 parametros, se muestra un error y se procede a la ejecucion con los valores por defecto. */
		    printf("WARNING: Este programa no contempla el uso de parametros de entrada, se procede a ignorar los parametros introducidos continuar la ejecución por defecto");
		    break;
	}


    //Crear el archivo donde se almacenen los logs
    ficheroLogs = fopen("registroTiempos.log" , "wt");
    if(ficheroLogs == NULL){
        perror("Error en la apertura del archivo de logs");
        exit(-1);
    }

    
    //Creamos los semaforos que usaremos luego
    if (pthread_mutex_init(&semaforoFichero, NULL) != 0) {
        perror("Error en la creacion del semaforo de Ficheros");
        exit (-1);
    }
    if (pthread_mutex_init(&semaforoColaClientes, NULL) != 0) {
        perror("Error en la creacion del semaforo de la cola de clientes");
        exit (-1);
    }
    if (pthread_mutex_init(&semaforoSolicitudes, NULL) != 0) {
        perror("Error en la creacion del semaforo de solicitudes");
        exit (-1);
    }
    

    //Creacion de los hilos de los tecnicos.
    for(int i=0; i<numTecnicos; i++){
        listaTecnicos[i].id=i;
        listaTecnicos[i].count=0;
        pthread_create(&listaTecnicos[i].hiloTecnico, NULL, accionesTecnico, "Tecnico creado");
    }

    //Creacion de los hilos de los responsables
    for(int i=0; i<numResponsables; i++){
        listaResponsables[i].id=i;
        listaResponsables[i].count=0;
        pthread_create(&listaResponsables[i].hiloResponsable, NULL, accionesresponsablesReparacion, "Respondable creado");
    }
    
    //Creacion del hilo encargado
    pthread_t encargado;
    pthread_create(&encargado, NULL, accionesEncargado, "Creado el encargado");
    
    //Creacion del hilo tecnico domiciliario
    pthread_t tecnicoDomiciliario;
    pthread_create(&tecnicoDomiciliario, NULL, accionesTecnicoDomiciliario, "Creado el tecnico domiciliario");
    
    int valor = 1;
    while (valor != 0) {
        wait(&valor);
    }
    free(listaClientes);
    free(listaTecnicos);
    free(listaResponsables);

    return 0;
}


int calculaAleatorio(int inicio, int fin){
    srand(time(NULL)); 
    return rand() % (fin-inicio+1) + inicio;
}

void escribirEnLog(char *id, char *mensaje){

    pthread_mutex_lock(&semaforoFichero);
    //Obtencion de la fecha y hora actuales
    time_t now = time(0);
    struct tm *tlocal = localtime(&now);
    char stnow[25];
    strftime(stnow, 25, "%d/ %m/ %y %H: %M: %S", tlocal);
    //Se escribe el mensaje en el fichero con la hora y el identificador
    ficheroLogs = fopen("registroTiempos.log", "a");
    fprintf(ficheroLogs, "[%s] %s: %s\n", stnow, id, mensaje);
    fclose(ficheroLogs);
    pthread_mutex_unlock(&semaforoFichero);
}

//Metodo que va a compactar la lista de clientes cuando un cliente se marche, ya sea por haber sido atendido o porque abandona la cola por otros motivos
void compactarListaClientes(int pos){
    
    pthread_mutex_lock(&semaforoColaClientes);
    int i=0;
    for(i=pos; i<contadorPeticiones-1; i++){
        //Para compactar, se mueven elementos de las posiciones siguientes a la pasada como parámetro una posicion a la izquierda
        listaClientes[i] = listaClientes[i+1];
    }
    contadorPeticiones--;
    pthread_mutex_unlock(&semaforoColaClientes);

}

//Estas funciones las realizaran los distintos thread
void nuevoClienteRed(int signal) {
    printf("Nueva peticion cliente red, actualmente hay %d peticiones.\n", contadorPeticiones+1);
    
    if(contadorPeticiones>=peticionesMax){
        printf("Peticion ignorada\n");
    }else{
        pthread_mutex_lock(&semaforoColaClientes);
        contadorClientesRed++;
        strcpy(listaClientes[contadorPeticiones].id, "clired_"+contadorClientesRed);
        listaClientes[contadorPeticiones].atendido = 0;
        listaClientes[contadorPeticiones].tipo = 'r';
        listaClientes[contadorPeticiones].prioridad = calculaAleatorio(1,10);
        int aux = calculaAleatorio(0,100);
        if(aux<80){
            listaClientes[contadorPeticiones].tipoDeAtencion=0;
        }else if(80<aux<90){
            listaClientes[contadorPeticiones].tipoDeAtencion=1;
        }else{
            listaClientes[contadorPeticiones].tipoDeAtencion=2;
        }
        pthread_create(&listaClientes[contadorPeticiones].hiloCliente,NULL,accionesCliente,(void *)(intptr_t)contadorPeticiones);
        contadorPeticiones++;
        pthread_mutex_unlock(&semaforoColaClientes);
    } 
}
void nuevoClienteApp(int signal) {
    printf("Nueva peticion cliente app, actualmente hay %d peticiones.\n", contadorPeticiones+1);
    
    if(contadorPeticiones>=peticionesMax){
        printf("Peticion ignorada\n");
    }else{
        pthread_mutex_lock(&semaforoColaClientes);
        contadorClientesApp++;
        strcpy(listaClientes[contadorPeticiones].id, "cliapp_"+contadorClientesApp);
        listaClientes[contadorPeticiones].atendido = 0;
        listaClientes[contadorPeticiones].tipo = 'a';
        listaClientes[contadorPeticiones].prioridad = calculaAleatorio(1,10);
        pthread_create(&listaClientes[contadorPeticiones].hiloCliente,NULL,accionesCliente,(void *)(intptr_t)contadorPeticiones);
        contadorPeticiones++;
        pthread_mutex_unlock(&semaforoColaClientes);
    } 
}


void manejadora_fin(int signal){
    printf("La llegada de solicitudes ha sido desactivada\n");
    escribirEnLog("FINAL","La llegada de solicitudes ha sido desactivada");
    while(1==1){
        pthread_mutex_lock(&semaforoSolicitudes);
        if(contadorPeticiones==0){
            pthread_mutex_unlock(&semaforoSolicitudes);
            printf("Saliendo del programa\n");
            escribirEnLog("TERMINADO","Saliendo del programa");
            exit(0);
        }else{
            pthread_mutex_unlock(&semaforoSolicitudes);
            sleep(1);
        }
    }
}


//codigo de todo el proceso que realizan los clientes en la app
void *accionesCliente(void *arg) {
    
    //mientras no esta siendo atendido, calculamos su comportamiento
    while(listaClientes[(intptr_t)arg].atendido==0){
        int num=0;
        for(num=0; num<4; num++){
            //10% de que se vayan por encontrar muy dificil la aplicacion(cada 2 segundos)
            if(calculaAleatorio(0, 100)< 10){
            compactarListaClientes((intptr_t)arg);
            //no hace  falta decrementar el contador de peticiones porque lo decrementa el metodo de compactar
            pthread_exit(NULL);
            }
            sleep(2);
        }
        //20% de que se vayan por cansarse de esperar(cada 8 segundos)
        if(calculaAleatorio(0, 100)<20){
            compactarListaClientes((intptr_t)arg);
            pthread_exit(NULL);
        }
        //5% que pierde la conexion
        if(calculaAleatorio(0, 100)<5){
            compactarListaClientes((intptr_t)arg);
            pthread_exit(NULL);
        }
    }
   
    //mientras esta siendo atendido, no hace nada
    while(listaClientes[(intptr_t)arg].atendido!=2){
        sleep(1);
    }

    


}
void *accionesTecnico(void *arg) {
    printf("%s\n", (char *)arg);
    while (1==1) {
        pthread_mutex_lock(&semaforoColaClientes);
        if (contadorClientesApp > 0) {
            pthread_mutex_unlock(&semaforoColaClientes);
            int prioridadMaxima = buscarPrioridad('a');
            for (int i = 0; i<contadorPeticiones; i++) {
                if (listaClientes[i].prioridad == prioridadMaxima && listaClientes[i].tipo == 'a') {
                    listaClientes[i].atendido = 1;
                    int probabilidad = calculaAleatorio(1, 100);
                    if (probabilidad <= 80) {
                        listaClientes[i].tipoDeAtencion = 0;
                        escribirEnLog("Tecnico", "atendiendo a cliente");
                        sleep(calculaAleatorio(1,4));
                        escribirEnLog("Tecnico", "fin de atencion al cliente");
                    } else if (probabilidad <= 90) {
                        listaClientes[i].tipoDeAtencion = 1;
                        escribirEnLog("Tecnico", "atendiendo a cliente");
                        sleep(calculaAleatorio(2,6));
                        escribirEnLog("Tecnico", "fin de atencion al cliente");
                    } else {
                        listaClientes[i].tipoDeAtencion = 2;
                        escribirEnLog("Tecnico", "atendiendo a cliente");
                        sleep(calculaAleatorio(1,2));
                        escribirEnLog("Tecnico", "fin de atencion al cliente");
                    }
                    listaClientes[i].atendido = 2;
                }

            }
        } else {
            pthread_mutex_unlock(&semaforoColaClientes);
            sleep(1);
        }
    }
    pthread_exit(NULL);
}

void *accionesEncargado(void *arg) {
    printf("%s\n", (char *)arg);
    pthread_exit(NULL);
}

void *accionesTecnicoDomiciliario(void *arg) {
    printf("%s\n", (char *)arg);
    pthread_exit(NULL);
}

void *accionesresponsablesReparacion(void *arg) {
    printf("%s\n", (char *)arg);
    pthread_exit(NULL);
}

int buscarPrioridad(char tipo) {
    int maximo = 0;
    for (int i = 0; i<contadorPeticiones; i++) {
        if (listaClientes[i].tipo == tipo && maximo < listaClientes[i].prioridad) {
            maximo = listaClientes[i].prioridad;
        }
    }
    printf("%d", maximo);
    return maximo;
}