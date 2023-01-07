#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h> 
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

//Variables globales
int peticionesMax;
int countPeticiones;
int numTecnicos;
FILE *ficheroLogs;


//Definicion de las funciones
int calculaAleatorio(int inicio, int fin);
void escribirEnLog(char *id, char *mensaje);

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
}

struct Tecnico{

    //Identificador para los tecnicos, como en este caso solo se disponen de 2 tecnicos, su valor sera 1 o 2.
    int id;

    //Contador de clientes  atendidos, en el momento que llegue a 5, se tomará un descanso de 5 segundos
    int count;

    //Hilo que ejecutan los tecnicos
    pthread_t hiloTecnico;
}

struct ResponsableReps{

    //Identificador que funciona igual que el del tecnico, su valor sera 1 o 2 puesto que solo se disponen de 2 responsables.
    int id;

    //Contador de clientes  atendidos, en el momento que llegue a 6, se tomará un descanso de 6 segundos
    int count;

    //Hilo que ejecutan los responsables
    pthread_t hiloResponsable
}


int main(int argc, char *argv[]){
    //Asignar valores por defecto a algunas variables
    peticionesMax = 20;
    countPeticiones = 0;
    numTecnicos = 2;
    
    //Crear el archivo donde se almacenen los logs
    ficheroLogs = fopen("registroTiempos.log" , "w");
    if(ficheroLogs == NULL){
        perror("Error en la apertura del archivo de logs");
        exit(-1);
    }


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
    ficheroLogs = fopen("registroTiempos.log", "a");
    fprintf(ficheroLogs, "[%s] %s: %s\n", stnow, id, mensaje);
}
