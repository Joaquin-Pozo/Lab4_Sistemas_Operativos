#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

/* ----------------- Definición de variables globales ----------------- */

#define TAMANO_BUFFER 5
#define NUM_RECEPTORES 5
#define NUM_TRABAJADORES 3
#define TOTAL_TAREAS 1000

// Contador global de tareas producidas
int tareasProducidas = 0;
// Contador global de tareas consumidas
int tareasConsumidas = 0;

// Indices para recorrer el buffer circular
int indiceProductor = 0;
int indiceConsumidor = 0;

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

/* ----------------- Semáforos utilizados ----------------- */

// Semáforo que cuenta los espacios libres en el buffer (empty)
sem_t semEmpty;
// Semáforo que cuenta las tareas disponibles en el buffer (full)
sem_t semFull;
// Mutex que protege el acceso al buffer compartido y a los contadores
pthread_mutex_t mutexTareas;

/* ----------------- Funciones para operar en el buffer circular ----------------- */

// Función que agrega una nueva tarea y avanza hacia la siguiente posición del buffer circular
void insertarTarea (int *bufferTareas, int idTarea) {
    bufferTareas[indiceProductor] = idTarea;
    // Mueve el indice hacia la siguiente posición (si se encuentra al final del arreglo, vuelve al inicio)
    indiceProductor = (indiceProductor + 1) % TAMANO_BUFFER;
}

// Función que consume una tarea y avanza hacia la siguiente posición del buffer circular. Retorna el id de la tarea consumida
int eliminarTarea (int *bufferTareas) {
    int idTarea = bufferTareas[indiceConsumidor];
    // Mueve el indice hacia la siguiente posición (si se encuentra al final del arreglo, vuelve al inicio)
    indiceConsumidor = (indiceConsumidor + 1) % TAMANO_BUFFER;
    return idTarea;
}

/* ----------------- Threads Productores ----------------- */

// Cada Thread Productor produce tareas hasta alcanzar el total de tareas
void *producirTarea (void *arg) {
    ThreadDataProductor *threadData = (ThreadDataProductor *)arg;

    while (1) {
        // Espera a que se libere un espacio en el buffer
        sem_wait(&semEmpty);

        // Activa el mutex antes de ingresar a la Sección Crítica (S.C.)
        pthread_mutex_lock(&mutexTareas);

        /* ----------------- Inicio de S.C. ----------------- */

        // Verifica si ya se produjo el total de tareas. Si ya se alcanzó el total, entonces avisa a los consumidores para que finalicen
        if (tareasProducidas >= TOTAL_TAREAS) {
            pthread_mutex_unlock(&mutexTareas);
            break;
        }
        // Asigna el id a la nueva tarea
        int idTarea = tareasProducidas + 1;
        // Se produce la nueva tarea en el buffer
        insertarTarea(threadData->bufferTareas, idTarea);
        // Aumenta el contador
        tareasProducidas++;
        // Procesamiento del thread
        printf("Receptor %d produjo la tarea %d\n", threadData->id, idTarea);
        // Avisa que se produjo una tarea nueva
        sem_post(&semFull);
        pthread_mutex_unlock(&mutexTareas);

        /* ----------------- Fin de S.C. ----------------- */ 
    }
    printf("\nFinalizó el thread productor %d\n\n", threadData->id);
    pthread_exit(NULL);
}

// El Thread Comodín avisa a los Threads Consumidores que deben finalizar su ejecución
void *avisarFinConsumidores(void *arg) {
    ThreadDataProductor *threadData = (ThreadDataProductor *)arg;
    for (int i = 0; i < NUM_TRABAJADORES; i++) {
        pthread_mutex_lock(&mutexTareas);
        sem_post(&semFull);
        pthread_mutex_unlock(&mutexTareas);
    }
}

/* ----------------- Thread Consumidor ----------------- */

// Cada Thread Consumidor consume tareas hasta alcanzar el total de tareas
void *consumirTarea (void *arg) {
    ThreadDataConsumidor *threadData = (ThreadDataConsumidor *)arg;

    while (1) {
        // Espera a que existan tareas disponibles en el buffer
        sem_wait(&semFull);

        // Activa el mutex antes de ingresar a la Sección Crítica (S.C.)
        pthread_mutex_lock(&mutexTareas);

        /* ----------------- Inicio de S.C. ----------------- */
        
        // Verifica si ya se consumió el total de tareas. Si ya se alcanzó el total, entonces desbloquea el mutex y retorna
        if (tareasConsumidas >= TOTAL_TAREAS) {
            pthread_mutex_unlock(&mutexTareas);
            break;
        }
        
        // Consume la tarea del buffer
        int idTarea = eliminarTarea(threadData->bufferTareas);
        // Aumenta el contador
        tareasConsumidas++;
        // Procesamiento del thread
        printf("Trabajador %d procesó la tarea %d\n", threadData->id, idTarea);
        // Avisa que se liberó un espacio en el buffer
        sem_post(&semEmpty);
        pthread_mutex_unlock(&mutexTareas);

        /* ----------------- Fin de S.C. ----------------- */
    }
    printf("\nFinalizó el thread consumidor %d\n\n", threadData->id);
    pthread_exit(NULL);
}

/* ----------------- Ejecución del programa ----------------- */

int main () {
    // Buffer para almacenar las tareas
    int bufferTareas[TAMANO_BUFFER];
    int i;
    // Inicializa el semáforo full en 0 para que lleve registro del número de tareas en el buffer (incrementa su valor por cada tarea producida)
    sem_init(&semFull, 0, 0);
    // Inicializa el semáforo empty con el tamaño máximo de tareas en el buffer (decrementa su valor por cada tarea consumida)
    sem_init(&semEmpty, 0, TAMANO_BUFFER);
    // Inicializa el semáforo binario
    pthread_mutex_init(&mutexTareas, NULL);

    // Creación de los threads productores (crea un thread adicional como comodín para avisar el fin de ejecución a los threads consumidores)
    pthread_t *threadsProductores = malloc((NUM_RECEPTORES * sizeof(pthread_t)) + sizeof(pthread_t));
    ThreadDataProductor *threadDataProductor = malloc((NUM_RECEPTORES * sizeof(ThreadDataProductor)) + sizeof(ThreadDataProductor));
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

    // Espera a que finalicen todos los threads productores
    for (i = 0; i < NUM_RECEPTORES; i++) {
        pthread_join(threadsProductores[i], NULL);
    }
    // Crea el thread productor comodín que da aviso a los threads consumidores para que finalicen su ejecución
    threadDataProductor[NUM_RECEPTORES].id = NUM_RECEPTORES;
    threadDataProductor[NUM_RECEPTORES].bufferTareas = bufferTareas;
    pthread_create(&threadsProductores[NUM_RECEPTORES], NULL, avisarFinConsumidores, &threadDataProductor[NUM_RECEPTORES]);

    // Espera a que finalice el thread comodin
    pthread_join(threadsProductores[NUM_RECEPTORES], NULL);

    // Espera a que finalicen todos los threads consumidores
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