#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

/* ----------------- Definición de variables globales ----------------- */
#define TAMANO_BUFFER 20
#define NUM_RECEPTORES 3
#define NUM_TRABAJADORES 3
#define TOTAL_TAREAS 100

// Contador global de tareas producidas
int tareasProducidas = 0;
// Contador global de tareas consumidas
int tareasConsumidas = 0;

// Indices para recorrer el buffer circular
int idxIn = 0;
int idxOut = 0;

/* ----------------- Structs utilizados ----------------- */

// Struct compartido entre los threads productores
// Contiene su id y el buffer compartido
typedef struct {
    int id;
    int *bufferTareas;
} ThreadDataProductor;

// Struct compartido entre los threads consumidores
// Contiene su id y el buffer compartido
typedef struct {
    int id;
    int *bufferTareas;
} ThreadDataConsumidor;

// Semáforo que cuenta los espacios libres en el buffer (empty)
sem_t semEmpty;
// Semáforo que cuenta las tareas disponibles en el buffer (full)
sem_t semFull;
// Mutex que protege el acceso al buffer compartido y a los contadores
pthread_mutex_t mutexTareas;

/* ----------------- Funciones para producir y consumir tareas en el buffer ----------------- */

// Función que produce una nueva tarea en la posición idxIn del buffer cicular
void insertarTarea (int *bufferTareas, int idTarea) {
    bufferTareas[idxIn] = idTarea;
    // Mueve el indice hacia la siguiente posición
    idxIn = (idxIn + 1) % TAMANO_BUFFER;
}

// Función que consume una tarea desde la posición idxOut del buffer circular. Retorna el id de la tarea consumida 
int eliminarTarea (int *bufferTareas) {
    int idTarea = bufferTareas[idxOut];
    // Marcar el espacio como libre
    bufferTareas[idxOut] = -1;
    // Mueve el indice hacia la siguiente posición
    idxOut = (idxOut + 1) % TAMANO_BUFFER;
    return idTarea;
}

/* ----------------- Thread Productor ----------------- */

// Cada Thread Productor genera ids de tareas hasta alcanzar el total de tareas
void *producirTarea (void *arg) {
    ThreadDataProductor *threadData = (ThreadDataProductor *)arg;

    while (1) {
        // Espera hasta que se libere un espacio en el buffer
        sem_wait(&semEmpty);

        // Activa el mutex antes de ingresar a la Sección Crítica (S.C.)
        pthread_mutex_lock(&mutexTareas);

        /* ----------------- Inicio de S.C. ----------------- */

        // Verifica si ya se generó el total de tareas. Si ya se alcanzo el total, entonces desbloquea el mutex y retorna
        if (tareasProducidas >= TOTAL_TAREAS) {
            pthread_mutex_unlock(&mutexTareas);
            // Devuelve el espacio solicitado
            sem_post(&semEmpty);
            break;
        }

        int idTarea = tareasProducidas + 1;
        // Se produce la nueva tarea en el buffer
        insertarTarea(threadData->bufferTareas, idTarea);
        tareasProducidas++;
        printf("Receptor %d produjo la tarea %d\n", threadData->id, idTarea);
        pthread_mutex_unlock(&mutexTareas);

        /* ----------------- Fin de S.C. ----------------- */ 

        // Avisa que se produjo una tarea nueva
        sem_post(&semFull);
    }
    pthread_exit(NULL);
}

/* ----------------- Thread Consumidor ----------------- */

// Cada Thread Consumidor consume tareas hasta que se haya procesado el total de tareas
void *consumirTarea (void *arg) {
    ThreadDataConsumidor *threadData = (ThreadDataConsumidor *)arg;

    while (1) {
        // Espera que haya al menos una tarea disponible en el buffer
        sem_wait(&semFull);

        // Activa el mutex antes de ingresar a la Sección Crítica (S.C.)
        pthread_mutex_lock(&mutexTareas);

        /* ----------------- Inicio de S.C. ----------------- */

        // Verifica si ya se consumió el total de tareas. Si ya se alcanzo el total, entonces desbloquea el mutex y retorna
        if (tareasConsumidas >= TOTAL_TAREAS) {
            pthread_mutex_unlock(&mutexTareas);
            // Devuelve el espacio solicitado
            sem_post(&semFull);
            break;
        }
        // Consume la tarea del buffer
        int idTarea = eliminarTarea(threadData->bufferTareas);
        tareasConsumidas++;
        pthread_mutex_unlock(&mutexTareas);

        /* ----------------- Fin de S.C. ----------------- */

        // Avisa que se liberó un espacio en el buffer
        sem_post(&semFull);

        printf("Trabajador %d procesó la tarea %d\n", threadData->id, idTarea);     
    }
    pthread_exit(NULL);
}
/* ----------------- Ejecución del programa ----------------- */
int main () {
    // Arreglo de buffer para las tareas. Inicializa los ids de las tareas (-1 = espacio disponible)
    int bufferTareas[TAMANO_BUFFER];
    int i;
    for (i = 0; i < TAMANO_BUFFER; i++) {
        bufferTareas[i] = -1;
    }

    // Inicializa el semáforo full en 0 para que lleve registro del número de tareas en el buffer (incrementa su valor por cada tarea producida)
    sem_init(&semFull, 0, 0);
    // Inicializa el semáforo empty con el tamaño máximo de tareas en el buffer (decrementa su valor por cada tarea consumida)
    sem_init(&semEmpty, 0, TAMANO_BUFFER);
    // Inicializa el semáforo binario
    pthread_mutex_init(&mutexTareas, NULL);

    // Creación de los threads productores
    pthread_t *threadsProductores = malloc(NUM_RECEPTORES * sizeof(pthread_t));
    ThreadDataProductor *threadDataProductor = malloc(NUM_RECEPTORES * sizeof(ThreadDataProductor));
    for (i = 0; i < NUM_RECEPTORES; i++) {
        threadDataProductor[i].id = i;
        threadDataProductor[i].bufferTareas = bufferTareas;
        // crea la hebra
        pthread_create(&threadsProductores[i], NULL, producirTarea, &threadDataProductor[i]);
    }

    // Creación de los threads consumidores
    pthread_t *threadsConsumidores = malloc(NUM_TRABAJADORES * sizeof(pthread_t));
    ThreadDataConsumidor *threadDataConsumidor = malloc(NUM_TRABAJADORES * sizeof(ThreadDataConsumidor));
    for (i = 0; i < NUM_TRABAJADORES; i++) {
        threadDataConsumidor[i].id = i;
        threadDataConsumidor[i].bufferTareas = bufferTareas;
        // crea la hebra
        pthread_create(&threadsConsumidores[i], NULL, consumirTarea, &threadDataConsumidor[i]);
    }

    // Espera a que finalicen todas los threads productores
    for (i = 0; i < NUM_RECEPTORES; i++) {
        pthread_join(threadsProductores[i], NULL);
    }

    // Espera a que finalicen todas los threads consumidores
    for (i = 0; i < NUM_TRABAJADORES; i++) {
        pthread_join(threadsConsumidores[i], NULL);
    }

    // Mensaje final de resumen
    printf("Todas las %d tareas han sido procesadas.\n", TOTAL_TAREAS);
    printf("Finalizando el programa...\n");

    // Destruye los semáforos y el mutex después de su uso
    sem_destroy(&semFull);
    sem_destroy(&semEmpty);
    pthread_mutex_destroy(&mutexTareas);

    // Libera la memoria asignada con malloc
    free(threadsProductores);
    free(threadsConsumidores);
    free(threadDataConsumidor);
    free(threadDataProductor);

    return 0;
}