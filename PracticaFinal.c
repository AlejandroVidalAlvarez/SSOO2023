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

/*BORRAR ANTES DE ENTREGAR---> Para haceros la vida mas facil
    CONTROL + K y luego CONTROL + 0 --->PLEGAR TODOS LOS METODOS
    CONTROL + K y luego CONTROL + J --->DESPLEGAR TODOS LOS METODOS
*/

//[][][][] Estructuras [][][][]
struct Clientes{
    char id[20];
    //Estado del cliente: 0 = no atendido, 1 = en proceso de atencion y 2 = terminado de atender.
    int atendido;
    // a = clientes con problemas en la app y r = clientes con problemas de red.
    char tipo;
    //Su valor va desde 1 hasta 10(ambos incluidos), siendo 1 la prioridad mas baja y 10 la más alta.
    int prioridad;
    //Su valor es 0=no está solicitando visita, 1= solicita visita
    int solicitud;
    //Hilo que ejecutan los clientes
    pthread_t hiloCliente;
};
struct Tecnico{
    //Identificador para los tecnicos, como en este caso solo se disponen de 2 tecnicos, su valor sera 1 o 2.
    char id[20];
    //Contador de clientes  atendidos, en el momento que llegue a 5, se tomará un descanso de 5 segundos
    int count;
    //Hilo que ejecutan los tecnicos
    pthread_t hiloTecnico;
};
struct ResponsableReps{
    //Identificador que funciona igual que el del tecnico, su valor sera 1 o 2 puesto que solo se disponen de 2 responsables.
    char id[20];
    //Contador de clientes  atendidos, en el momento que llegue a 6, se tomará un descanso de 6 segundos
    int count;
    //Hilo que ejecutan los responsables
    pthread_t hiloResponsable;
};

//[][][][] Semaforos y variables condición [][][][]
pthread_mutex_t semaforoFichero;
pthread_mutex_t semaforoColaClientes;
pthread_mutex_t semaforoSolicitudesDomiciliarias;
int ignorarSolicitudes;
// Condicion para llamar al tecnico domiciliario cuando hay 4 solicitudes
pthread_cond_t condicionTecnicoDomiciliario = PTHREAD_COND_INITIALIZER;
// Variable condicion para que no entren mas solicitudes domiciliarias mientras trabaja el tecnico domiciliario
int ignorarSolicitudesDomiciliarias;



//[][][][]  Variables globales  [][][][]
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



//[][][][]  Definicion de las funciones  [][][][]
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
void ponerComoAtendido(char idBuscado[20]);
void compactarListaClientes(int pos);
int comprobarDescansoTecnico(void *arg);
void sumarContadorTecnico(void *arg);
void resetearContadorTecnico(void *arg);
int buscarClientePrioritario(char tipo);
void accionFinalTecnico(char idCliente[20]);
int comprobarDescansoResponsable(void *arg);
void sumarContadorResponsable(void *arg);
void resetearContadorResponsable(void *arg);
void accionFinalResponsable(char idCliente[20]);

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
    ignorarSolicitudes = 0;
    ignorarSolicitudesDomiciliarias = 0;
    
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
    if (pthread_mutex_init(&semaforoSolicitudesDomiciliarias, NULL) != 0) {
        perror("Error en la creacion del semaforo de solicitudes");
        exit (-1);
    }
    

    //Creacion de los hilos de los tecnicos.
    for(int i=0; i<numTecnicos; i++){
        sprintf(listaTecnicos[i].id,"tecnico_%d",(i+1));
        listaTecnicos[i].count=0;
        pthread_create(&listaTecnicos[i].hiloTecnico, NULL, accionesTecnico, (void *)(intptr_t)listaTecnicos[i].id);
    }

    //Creacion de los hilos de los responsables
    for(int i=0; i<numResponsables; i++){
        sprintf(listaResponsables[i].id,"resprep_%d",(i+1));
        listaResponsables[i].count=0;
        pthread_create(&listaResponsables[i].hiloResponsable, NULL, accionesresponsablesReparacion, (void *)(intptr_t)listaResponsables[i].id);
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

//[][][][]  Metodos Llegada Señales  [][][][]
void nuevoClienteRed(int signal) {
    if(ignorarSolicitudes==1){
        return;
    }
    pthread_mutex_lock(&semaforoColaClientes);
    printf("Nueva peticion cliente red, actualmente hay %d peticiones.\n", contadorPeticiones+1);  
    if(contadorPeticiones>=peticionesMax){
        printf("Peticion ignorada\n");
    }else{
        
        contadorClientesRed++;
        sprintf(listaClientes[contadorPeticiones].id,"clired_%d",contadorClientesRed);
        listaClientes[contadorPeticiones].atendido = 0;
        printf("Atributo atendido asignado\n");
        listaClientes[contadorPeticiones].tipo = 'r';
        listaClientes[contadorPeticiones].prioridad = calculaAleatorio(1,10);
        printf("Prioridad asignada\n");
        pthread_create(&listaClientes[contadorPeticiones].hiloCliente,NULL,accionesCliente,(void *)(intptr_t)contadorPeticiones);
        contadorPeticiones++;
        
    } 
    pthread_mutex_unlock(&semaforoColaClientes);
}
void nuevoClienteApp(int signal) {
    if(ignorarSolicitudes==1){
        return;
    }
    pthread_mutex_lock(&semaforoColaClientes);
    printf("Nueva peticion cliente app, actualmente hay %d peticiones.\n", contadorPeticiones+1);
    
    if(contadorPeticiones>=peticionesMax){
        printf("Peticion ignorada\n");
    }else{
        
        contadorClientesApp++;
        sprintf(listaClientes[contadorPeticiones].id,"cliapp_%d",contadorClientesApp);
        listaClientes[contadorPeticiones].atendido = 0;
        listaClientes[contadorPeticiones].tipo = 'a';
        listaClientes[contadorPeticiones].prioridad = calculaAleatorio(1,10);
        pthread_create(&listaClientes[contadorPeticiones].hiloCliente,NULL,accionesCliente,(void *)(intptr_t)contadorPeticiones);
        contadorPeticiones++;
        
    } 
    pthread_mutex_unlock(&semaforoColaClientes);
}
void manejadora_fin(int signal){
    printf("La llegada de solicitudes ha sido desactivada\n");
    ignorarSolicitudes = 1;
    escribirEnLog("FINAL","La llegada de solicitudes ha sido desactivada");
    while(1==1){
        pthread_mutex_lock(&semaforoColaClientes);
        if(contadorPeticiones==0){
            pthread_mutex_unlock(&semaforoColaClientes);
            printf("Saliendo del programa\n");
            escribirEnLog("TERMINADO","Saliendo del programa");
            exit(0);
        }else{
            pthread_mutex_unlock(&semaforoColaClientes);
            sleep(1);
        }
    }
}


//[][][][]  Metodos Tareas Principales  [][][][]
void *accionesCliente(void *arg) {
    intptr_t posicionArgumento1 = (intptr_t)arg;
    int posicionArgumento = (int)posicionArgumento1;

    char *texto;
    texto = malloc(sizeof(char) * 1024);
    sprintf(texto,"El cliente de tipo %c ha entrado al sistema y espera ser atendido",listaClientes[posicionArgumento].tipo);
    escribirEnLog(listaClientes[posicionArgumento].id,texto);
   
    free(texto);

    int contador;
    //mientras no esta siendo atendido, calculamos su comportamiento
    while(listaClientes[posicionArgumento].atendido==0){
        int aleat=calculaAleatorio(1, 100);
        if(contador%2==0){
           if(aleat <= 10){
            pthread_mutex_lock(&semaforoColaClientes);
            if(listaClientes[(intptr_t)arg].tipo=='a') {
                contadorClientesApp--;
            } else {
                contadorClientesRed--;
            }
            char *texto;
            texto = malloc(sizeof(char) * 1024);
            sprintf(texto,"El cliente ha abandonado el sistema por dificultad en la app");
            
            escribirEnLog(listaClientes[posicionArgumento].id,texto);
            
            free(texto);

            compactarListaClientes(posicionArgumento);
            pthread_mutex_unlock(&semaforoColaClientes);
            pthread_exit(NULL);
            }
        }
        if(contador%8==0){
           if(aleat > 10 && aleat <= 30){
            pthread_mutex_lock(&semaforoColaClientes);
            if(listaClientes[(intptr_t)arg].tipo=='a') {
                contadorClientesApp--;
            } else {
                contadorClientesRed--;
            }
            texto = malloc(sizeof(char) * 1024);
            sprintf(texto,"El cliente ha abandonado el sistema por cansarse de esperar");
            
            escribirEnLog(listaClientes[posicionArgumento].id,texto);
            
            free(texto);

            compactarListaClientes(posicionArgumento);
            pthread_mutex_unlock(&semaforoColaClientes);
            pthread_exit(NULL);
            }
        }
        if(contador%2==0){
           if(aleat > 30){
            if(calculaAleatorio(1,100)>95){
            pthread_mutex_lock(&semaforoColaClientes);
            if(listaClientes[(intptr_t)arg].tipo=='a') {
                contadorClientesApp--;
            } else {
                contadorClientesRed--;
            }
            texto = malloc(sizeof(char) * 1024);
            sprintf(texto,"El cliente ha abandonado el sistema por problemas de conexión");
            
            escribirEnLog(listaClientes[posicionArgumento].id,texto);
            
            free(texto);

            compactarListaClientes(posicionArgumento);
            pthread_mutex_unlock(&semaforoColaClientes);
            pthread_exit(NULL);  
            }
            }
        }
        if(contador%8==0){
            contador = 0;
        }
        contador++;
        sleep(1);
    }

   
    //mientras esta siendo atendido, no hace nada
    while(listaClientes[(intptr_t)arg].atendido==1){
        //printf("Estoy siendo atendido");
        sleep(2);
    }

    //en caso de ser un cliente de app, abandona la lista, si es de red podria solicitar atencion domiciliaria
    if(listaClientes[(intptr_t)arg].tipo=='a'){
        pthread_mutex_lock(&semaforoColaClientes);

        texto = malloc(sizeof(char) * 1024);
        sprintf(texto,"El cliente ha terminado de ser atendido y abandona el sistema");
        
        escribirEnLog(listaClientes[posicionArgumento].id,texto);
        
        free(texto);

        compactarListaClientes((intptr_t)arg);
        contadorClientesApp--;
        pthread_mutex_unlock(&semaforoColaClientes);
        pthread_exit(NULL);
    }else{
        if(calculaAleatorio(0,100)<30){ //caso de pedir atencion domiciliaria
           int terminar = 0; //
           while(terminar==0){
            pthread_mutex_lock(&semaforoSolicitudesDomiciliarias);
            if(nSolicitudesDomiciliarias<4){
                nSolicitudesDomiciliarias++;
                printf("Cliente esperando ser atendido en casa\n");
                if (nSolicitudesDomiciliarias == 4) {
                    pthread_cond_broadcast(&condicionTecnicoDomiciliario);
                }
                pthread_mutex_lock(&semaforoColaClientes);
                texto = malloc(sizeof(char) * 1024);
                sprintf(texto,"El cliente espera ha ser atendido en su domicilio");
                
                escribirEnLog(listaClientes[posicionArgumento].id,texto);
                
                free(texto);

                
                listaClientes[posicionArgumento].solicitud = 1;
                terminar = 1;
                pthread_mutex_unlock(&semaforoColaClientes);
                pthread_mutex_unlock(&semaforoSolicitudesDomiciliarias);
            }else{
                pthread_cond_wait(&condicionTecnicoDomiciliario, &semaforoSolicitudesDomiciliarias);
                pthread_mutex_unlock(&semaforoSolicitudesDomiciliarias);
                sleep(3);
            }
           } 
            terminar = 0;
            while(terminar==0){ //Espera a que el tecnico termine la visita a domicilio
                pthread_mutex_lock(&semaforoColaClientes);
                if(listaClientes[posicionArgumento].solicitud==0){
                    //la ha terminado
                    terminar = 1;
                }
                pthread_mutex_unlock(&semaforoColaClientes);
            }
            //el cliente comunica en el log su salida de la app tras la visita
                pthread_mutex_lock(&semaforoColaClientes);
                texto = malloc(sizeof(char) * 1024);
                sprintf(texto,"El cliente abandona el sistema tras terminar la visita a su domicilio");
                
                escribirEnLog(listaClientes[posicionArgumento].id,texto);
                
                free(texto);
                pthread_mutex_unlock(&semaforoColaClientes);
        

        }else{
            pthread_mutex_lock(&semaforoColaClientes);
            compactarListaClientes((intptr_t)arg);
            contadorClientesRed--;

            texto = malloc(sizeof(char) * 1024);
            sprintf(texto,"El cliente abandona el sistema tras ser atendido y no solicitar visita a domicilio");
            
            escribirEnLog(listaClientes[posicionArgumento].id,texto);
            
            free(texto);

            pthread_mutex_unlock(&semaforoColaClientes);
            pthread_exit(NULL);
        }
    }


}
void *accionesTecnico(void *arg) {
    
    printf("Creado %s\n", (char *)arg);
    while (1==1) {
        if(comprobarDescansoTecnico(arg)>=5){
            
            escribirEnLog((char *)arg, "Comienza el descanso");
            
            resetearContadorTecnico(arg);

            sleep(5);

            
            escribirEnLog((char *)arg, "Finaliza el descanso");
            
            
        }
        pthread_mutex_lock(&semaforoColaClientes);
        if (contadorClientesApp > 0) {
            
            int i = buscarClientePrioritario('a');
            if(!((i>=0)&&(i<20))){
                if(i==-1){
                    pthread_mutex_unlock(&semaforoColaClientes);
                    continue;
                }
                printf("error al buscar el cliente prioritario, realmente hay clientes?i-->%d\n",i);
                pthread_mutex_unlock(&semaforoColaClientes);
                sleep(3);//TODO aqui nunca deberia entrar por tanto eliminar antes de entregar 
                continue;//siguiente iteracion del while
            }
            char idCliente[20];

            char *text = malloc(sizeof(char) * 1024);
            sprintf(text,"Comienza la atencion al cliente llamado %s en la posicion %d",listaClientes[i].id,i);
            strcpy(idCliente, listaClientes[i].id);
            pthread_mutex_unlock(&semaforoColaClientes);
            escribirEnLog((char *)arg, text);
            free(text);

            int probabilidad = calculaAleatorio(1, 100);
            int dormir, tipoAtencion;
            char *texto = malloc(sizeof(char) * 1024);
            if (probabilidad <= 80) {
                sprintf(texto,"Finaliza la atencion al cliente, todo en regla");
                tipoAtencion = 0;
                dormir = calculaAleatorio(1, 4);
            } else if (probabilidad <= 90) {
                sprintf(texto,"Finaliza la atencion al cliente, cliente mal identificado");
                tipoAtencion = 1;
                dormir = calculaAleatorio(2, 6);
            } else {
                sprintf(texto,"Finaliza la atencion al cliente, compañía equivocada");
                tipoAtencion = 2;
                dormir = calculaAleatorio(1, 2);
            }

            
            
            escribirEnLog((char *)arg,texto);
            
            free(texto);

            pthread_mutex_lock(&semaforoColaClientes);
            accionFinalTecnico(idCliente);
            sumarContadorTecnico(arg);
            pthread_mutex_unlock(&semaforoColaClientes);
            sleep(dormir);

        } else {
            pthread_mutex_unlock(&semaforoColaClientes);
            sleep(3);//TODO es un sleep1, cambiar antes de entregar
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
    while (1==1) {
        pthread_mutex_lock(&semaforoSolicitudesDomiciliarias);
        while (nSolicitudesDomiciliarias < 4) {
            pthread_cond_wait(&condicionTecnicoDomiciliario, &semaforoSolicitudesDomiciliarias);
            printf("El numero de solicitudes domiciliarias es de %d\n", nSolicitudesDomiciliarias);
        }
        printf("Comienza la atencion domiciliaria\n");
        escribirEnLog("Tecnico domiciliario", "Comienza la atencion domiciliaria");
        pthread_mutex_lock(&semaforoSolicitudesDomiciliarias);
        ignorarSolicitudesDomiciliarias = 1;
        for (int i = 0; i<nSolicitudesDomiciliarias; i++) {
            pthread_mutex_lock(&semaforoColaClientes);
            for (int j = 0; j<contadorPeticiones; i++) {
                if (listaClientes[j].solicitud == 1) {
                    char* texto = malloc(sizeof(char) + 1024);
                    sprintf(texto, "Atendido el cliente %d", i);
                    listaClientes[i].solicitud = 0;
                    pthread_mutex_lock(&semaforoColaClientes);
                    sleep(1);
                    escribirEnLog("Tecnico domiciliario", texto);
                }
            }
        }
        nSolicitudesDomiciliarias = 0;
        printf("Acabo la atencion domiciliaria\n");
        escribirEnLog("Tecnico domiciliario", "Finaliza la atencion domiciliaria");
    }
    pthread_exit(NULL);
}
void *accionesresponsablesReparacion(void *arg) {
    printf("Creado %s\n", (char *)arg);
    while (1==1) {
        if(comprobarDescansoResponsable(arg)>=6){
            
            escribirEnLog((char *)arg, "Comienza el descanso");
            
            resetearContadorResponsable(arg);

            sleep(5);

            
            escribirEnLog((char *)arg, "Finaliza el descanso");
            
            
        }
        pthread_mutex_lock(&semaforoColaClientes);
        if (contadorClientesRed > 0) {
            
            int i = buscarClientePrioritario('r');
            if(!((i>=0)&&(i<20))){
                if(i==-1){
                    pthread_mutex_unlock(&semaforoColaClientes);
                    continue;
                }
                printf("error al buscar el cliente prioritario, realmente hay clientes?i-->%d\n",i);
                pthread_mutex_unlock(&semaforoColaClientes);
                sleep(3);//TODO aqui nunca deberia entrar por tanto eliminar antes de entregar 
                continue;//siguiente iteracion del while
            }
            char idCliente[20];

            char *text = malloc(sizeof(char) * 1024);
            sprintf(text,"Comienza la atencion al cliente llamado %s en la posicion %d",listaClientes[i].id,i);
            strcpy(idCliente, listaClientes[i].id);
            pthread_mutex_unlock(&semaforoColaClientes);
            escribirEnLog((char *)arg, text);
            free(text);

            int probabilidad = calculaAleatorio(1, 100);
            int dormir, tipoAtencion;
            char *texto = malloc(sizeof(char) * 1024);
            if (probabilidad <= 80) {
                sprintf(texto,"Finaliza la atencion al cliente, todo en regla");
                tipoAtencion = 0;
                dormir = calculaAleatorio(1, 4);
            } else if (probabilidad <= 90) {
                sprintf(texto,"Finaliza la atencion al cliente, cliente mal identificado");
                tipoAtencion = 1;
                dormir = calculaAleatorio(2, 6);
            } else {
                sprintf(texto,"Finaliza la atencion al cliente, compañía equivocada");
                tipoAtencion = 2;
                dormir = calculaAleatorio(1, 2);
            }

            
            
            escribirEnLog((char *)arg,texto);
            
            free(texto);

            pthread_mutex_lock(&semaforoColaClientes);
            accionFinalResponsable(idCliente);
            sumarContadorResponsable(arg);
            pthread_mutex_unlock(&semaforoColaClientes);
            sleep(dormir);

        } else {
            pthread_mutex_unlock(&semaforoColaClientes);
            sleep(3);//TODO es un sleep1, cambiar antes de entregar
        }
    }
    pthread_exit(NULL);
}


//[][][][]  Metodos auxiliares  [][][][]
int calculaAleatorio(int inicio, int fin){
    srand(time(NULL)); 
    return rand() % (fin-inicio+1) + inicio;
}

void ponerComoAtendido(char idBuscado[20]){

    for (int i = 0; i<contadorPeticiones; i++) {
        if (listaClientes[i].id == idBuscado ) {
            listaClientes[i].atendido=2;
            break;
        }
    }

}

int buscarPrioridad(char tipo) {

    int maximo = -1;
    for (int i = 0; i<contadorPeticiones; i++) {
        if (listaClientes[i].atendido==0 &&listaClientes[i].tipo == tipo && maximo < listaClientes[i].prioridad&&listaClientes[i].prioridad!=0) {
            maximo = listaClientes[i].prioridad;
        }
    }
    
    return maximo;
}
int comprobarDescansoTecnico(void *arg) {

    int num = 0;
    for (int i = 0; i<2; i++) {
        if (listaTecnicos[i].id == (char *)arg) {
            return listaTecnicos[i].count;
        }
    }
    printf("Error, no se encontró el tecnico");
    return num;
}
void resetearContadorTecnico(void *arg){
        
    for (int i = 0; i<2; i++) {
        if (listaTecnicos[i].id == (char *)arg) {
            listaTecnicos[i].count = 0;
            return;
        }
    }
    printf("Error, no se reseteó el contador del tecnico");
}
void sumarContadorTecnico(void *arg){
    for (int i = 0; i<2; i++) {
        if (listaTecnicos[i].id == (char *)arg) {
            listaTecnicos[i].count++;
            return;
        }
    }
    printf("Error al sumar");
    
}
void compactarListaClientes(int pos){ 
    //Metodo que va a compactar la lista de clientes cuando un cliente se marche, ya sea por haber sido atendido o porque abandona la cola por otros motivos
    if(contadorPeticiones==0){
        return;
    }else if(contadorPeticiones==1){
        contadorPeticiones=0;
            strncpy(listaClientes[0].id,"",20);
            listaClientes[0].atendido = 0;
            listaClientes[0].tipo =' ';
            listaClientes[0].solicitud = 0;
            listaClientes[0].prioridad=0;
        return;
    }else{
    for(int i=pos; i<contadorPeticiones-1; i++){
        //Para compactar, se mueven elementos de las posiciones siguientes a la pasada como parámetro una posicion a la izquierda
        listaClientes[i] = listaClientes[i+1];
    }
    //eliminamos el de la ultima posicion porque está duplicado en la pos anterior  Ejemplo: [1 2 3] ---eliminamos pos==0---> [2 3 3]
    strncpy(listaClientes[contadorPeticiones-1].id,"",20);
    listaClientes[contadorPeticiones-1].atendido = 0;
    listaClientes[contadorPeticiones-1].tipo =' ';
    listaClientes[contadorPeticiones-1].solicitud = 0;
    listaClientes[contadorPeticiones-1].prioridad=0;
    contadorPeticiones--;
    }
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

int buscarClientePrioritario(char tipo) {
    int prioridadMaxima = buscarPrioridad(tipo);
    for (int i = 0; i<contadorPeticiones; i++) {
        if (prioridadMaxima>0 &&listaClientes[i].prioridad == prioridadMaxima && listaClientes[i].tipo == tipo && listaClientes[i].atendido == 0) {
            listaClientes[i].atendido = 1;
            return i;
        } 
    }
    return prioridadMaxima;
}

void accionFinalTecnico(char idCliente[20]) {
    for (int i = 0; i<contadorPeticiones; i++) {
        if (strcmp(listaClientes[i].id,idCliente)==0&&listaClientes[i].atendido == 1) {
            listaClientes[i].atendido = 2;
            printf("Cliente %c encontrado, el contador de la app es: %d, y el contador total es: %d\n",*idCliente, contadorClientesApp, contadorPeticiones);
            return;
        }
    }
    
    printf("%s,%s",idCliente,listaClientes[0].id);
    printf("Cliente %c NO encontrado, el contador de la app es: %d, y el contador total es: %d\n",*idCliente, contadorClientesApp, contadorPeticiones);
}

int comprobarDescansoResponsable(void *arg) {

    int num = 0;
    for (int i = 0; i<2; i++) {
        if (listaResponsables[i].id == (char *)arg) {
            return listaResponsables[i].count;
        }
    }
    printf("Error, no se encontró el responsable\n");
    return num;
}
void resetearContadorResponsable(void *arg){
        
    for (int i = 0; i<2; i++) {
        if (listaResponsables[i].id == (char *)arg) {
            listaResponsables[i].count = 0;
            return;
        }
    }
    printf("Error, no se reseteó el contador del responsable\n");
}
void sumarContadorResponsable(void *arg){
    for (int i = 0; i<2; i++) {
        if (strcmp(listaResponsables[i].id, (char *)arg)) {
            listaResponsables[i].count++;
            return;
        }
    }
    printf("Error al sumar\n");
    
}
void accionFinalResponsable(char idCliente[20]) {
    for (int i = 0; i<contadorPeticiones; i++) {
        if (strcmp(listaClientes[i].id,idCliente)==0&&listaClientes[i].atendido == 1) {
            listaClientes[i].atendido = 2;
            printf("Cliente %c encontrado, el contador de red es: %d, y el contador total es: %d\n",*idCliente, contadorClientesRed, contadorPeticiones);
            return;
        }
    }
    
    printf("%s,%s",idCliente,listaClientes[0].id);
    printf("Cliente %c NO encontrado, el contador de red es: %d, y el contador total es: %d\n",*idCliente, contadorClientesRed, contadorPeticiones);
}