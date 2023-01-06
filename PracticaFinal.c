#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h> 
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

//Variables globales



//Definicion de las funciones
int calculaAleatorio(int inicio, int fin);

//Estructuras
struct Clientes{
    int id;
    
    //Estado del cliente: 0 = no atendido, 1 = en proceso de atencion y 2 = terminado de atender.
    int atendido;

    //Su valor va desde 1 hasta 10(ambos incluidos), siendo 1 la prioridad mas baja y 10 la más alta.
    int prioridad;

    // a = clientes con problemas en la app y r = clientes con problemas de red.
    char tipo;
}

struct Tecnico{

    //Identificador para los tecnicos, como en este caso solo se disponen de 2 tecnicos, su valor sera 1 o 2.
    int id;

    //Contador de clientes  atendidos, en el momento que llegue a 5, se tomará un descanso de 5 segundos
    int count;
}

struct ResponsableReps{

    //Identificador que funciona igual que el del tecnico, su valor sera 1 o 2 puesto que solo se disponen de 2 responsables.
    int id;

    //Contador de clientes  atendidos, en el momento que llegue a 6, se tomará un descanso de 6 segundos
    int count;
}





int calculaAleatorio(int inicio, int fin){
    int generado;
    srand(time(NULL));
    generado = rand() % (inicio+1) + (fin-inicio);
    return generado;
}
