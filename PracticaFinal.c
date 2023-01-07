#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h> 
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

//Estructuras
struct Clientes{
    int id;
    
    //Estado del cliente: 0 = no atendido, 1 = en proceso de atencion y 2 = terminado de atender.
    int atendido;

    //Su valor va desde 1 hasta 10(ambos incluidos), siendo 1 la prioridad mas baja y 10 la más alta.
    int prioridad;

    // a = clientes con problemas en la app y r = clientes con problemas de red.
    char tipo;

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

//Variables globales
int peticionesMax;
int countPeticiones;
int numTecnicos;
FILE *ficheroLogs;
struct Clientes *listaClientes;
int contadorClientesApp;
int contadorClientesRed;
int nSolicitudesDomiciliarias;
pthread_mutex_t semaforoFichero;
pthread_mutex_t semaforoColaClientes;
pthread_mutex_t semaforoSolicitudes;


//Definicion de las funciones
int calculaAleatorio(int inicio, int fin);
void escribirEnLog(char *id, char *mensaje);
void nuevoClienteRed();
void accionesCliente();
void *accionesTecnico(void *arg);
void *accionesEncargado(void *arg);
void *accionesTecnicoDomiciliario(void *arg);
void *accionesresponsablesReparacion(void *arg);

int main(int argc, char *argv[]){
    //Asignar valores por defecto a algunas variables
    peticionesMax = 20;
    countPeticiones = 0;
    numTecnicos = 2;
    contadorClientesApp = 0;
    contadorClientesRed = 0;
    nSolicitudesDomiciliarias = 0;
    listaClientes = malloc(sizeof(struct Clientes) * 20);
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
    
    //Crear el archivo donde se almacenen los logs
    ficheroLogs = fopen("registroTiempos.log" , "w");
    if(ficheroLogs == NULL){
        perror("Error en la apertura del archivo de logs");
        exit(-1);
    }
    pthread_t tecnico_1, tecnico_2;
    pthread_create(&tecnico_1, NULL, accionesTecnico, "Creado el tecnico 1");
    pthread_create(&tecnico_2, NULL, accionesTecnico, "Creado el tecnico 2");
    pthread_t responsableReparacion_1, responsableReparacion_2;
    pthread_create(&responsableReparacion_1, NULL, accionesresponsablesReparacion, "Creado el responsable 1");
    pthread_create(&responsableReparacion_2, NULL, accionesresponsablesReparacion, "Creado el responsable 2");
    pthread_t encargado;
    pthread_create(&encargado, NULL, accionesEncargado, "Creado el encargado");
    pthread_t tecnicoDomiciliario;
    pthread_create(&tecnicoDomiciliario, NULL, accionesTecnicoDomiciliario, "Creado el tecnico domiciliario");
    sleep(1);
    printf("Fin del programa\n");

    return 0;
}


int calculaAleatorio(int inicio, int fin){
    int generado;
    srand(time(NULL));
    generado = rand() % (inicio+1) + (fin-inicio);
    return generado;
}

void escribirEnLog(char *id, char *mensaje){

    /*ESTA PARTE TENDRÁ QUE ESTAR CONTROLADA POR UN MUTEX*/
    //Obtencion de la fecha y hora actuales
    time_t now = time(0);
    struct tm *tlocal = localtime(&now);
    char stnow[25];
    strftime(stnow, 25, "%d/ %m/ %y %H: %M: %S", tlocal);

    //Se escribe el mensaje en el fichero con la hora y el identificador
    //Bloquea el semaforo del fichero para que nadie pueda acceder a el.
    pthread_mutex_lock(&semaforoFichero);
    ficheroLogs = fopen("registroTiempos.log", "a");
    fprintf(ficheroLogs, "[%s] %s: %s\n", stnow, id, mensaje);
    pthread_mutex_unlock(&semaforoFichero);
}

//Estas funciones las realizaran los distintos thread
void nuevoClienteRed() {
    pthread_mutex_lock(&semaforoColaClientes);
    //Dentro del mutex solo tiene que estar el codigo de la zona critica.
    pthread_mutex_unlock(&semaforoColaClientes);
}

void accionesCliente() {
    
}

void *accionesTecnico(void *arg) {
    printf("%s\n", (char *)arg);
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