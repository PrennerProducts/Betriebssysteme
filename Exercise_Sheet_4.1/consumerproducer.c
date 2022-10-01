
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

int threadRunFlag = 1;


/**
 * Type of the elements of the circular buffer
 * If you want to use a different element type, then change this typedef
 */
typedef int cb_element_t;

/**
 * Struct representing a circular buffer
 */
typedef struct {
    cb_element_t* buffer;
    int first;  // Index des ersten Elements
    int count;  // Anzahl der Elemente im Buffer
    int size;   // GrÃ¶ÃŸe des Buffers
    pthread_mutex_t mutex;
    pthread_cond_t condition_producer;
    pthread_cond_t condition_consumer;
    int finlalize_flag;
} circularbuffer_t;

/**
 * Initializes the circular buffer.
 *
 * \param buffer pointer to struct representing a circular buffer
 * \param size size of the circular buffer
 * \return a negative value in case of an error otherwise 0
 */
int cb_init(circularbuffer_t* buffer, int size) {
    if (size <= 0) {
        return -1;
    }
    buffer->buffer = (cb_element_t*)malloc(sizeof(cb_element_t)*size);
    if (buffer->buffer == NULL) {
        return -2;
    }
    buffer->first = 0;
    buffer->count = 0;
    buffer->size = size;
    buffer->finlalize_flag = 0;
    //INIT Mutex & ContitionVariable
    pthread_mutex_init(&buffer->mutex,NULL);
    pthread_cond_init(&buffer->condition_consumer, NULL);
    printf("Init Condition_consumer\n");
    pthread_cond_init(&buffer->condition_producer, NULL);
    printf("Init Condition_producer\n");
    return 0;
}

/**
 * Destroys the circular buffer and frees all ressources.
 *
 * \param buffer pointer to struct representing the circular buffer
 * \return a negative value in case of an error otherwise 0
 */
int cb_destroy(circularbuffer_t* buffer) {
    if (buffer->buffer) {
        free(buffer->buffer);
        buffer->buffer = NULL;
        //Destroy Mutex & ContitionVariable
        pthread_mutex_destroy(&buffer->mutex);
        pthread_cond_destroy(&buffer->condition_consumer);
        pthread_cond_destroy(&buffer->condition_producer);
        return 0;
    }

    return -1;
}

/**
 * Adds an Element to the circular buffer
 *
 * \param buffer pointer to struct representing a circular buffer
 * \param elem value to be added to the circular buffer
 * \return -1 when the buffer is full otherwise 0
 */
void cb_add(circularbuffer_t* buffer, cb_element_t elem, int id) {
    //LOCK and UNLOCK MUTEX
    pthread_mutex_lock(&buffer->mutex);
    while (!buffer->finlalize_flag && buffer->count >= buffer->size && threadRunFlag) {
        printf("Temperatursensor %d wurde schlafengelegt!\n", id);
        pthread_cond_wait(&buffer->condition_producer, &buffer->mutex);
        printf("Temperatursensor %d wurde aufgeweckt!\n", id);
    }
    if(threadRunFlag){
        int elem_index = (buffer->first + buffer->count) % buffer->size;
        buffer->buffer[elem_index] = elem;
        if(buffer->count <= 0){
            pthread_cond_signal(&buffer->condition_consumer);
        }
        buffer->count++;
}
    pthread_mutex_unlock(&buffer->mutex);}

/**
 * Removes an Element from the circular buffer. The removed element is returned via call-by-reference.
 *
 * \param buffer pointer to struct representing a circular buffer
 * \param elem pointer to where the removed value should be stored (call-by-reference)
 * \return -1 when the buffer is empty otherwise 0
 */
cb_element_t cb_remove(circularbuffer_t* buffer) {
    pthread_mutex_lock(&buffer->mutex);
    while (! buffer->finlalize_flag && buffer->count <= 0 && threadRunFlag) {
        printf("Logging Programm wurde schlafengelegt!\n");
        pthread_cond_wait(&buffer->condition_consumer, &buffer->mutex);
        printf("Logging Programm wurde aufgeweckt!\n");
    }

    cb_element_t element = buffer->buffer[buffer->first];
    buffer->first = (buffer->first + 1) % buffer->size;
    if(buffer->count >= buffer->size){
        pthread_cond_signal(&buffer->condition_producer);
    }
    buffer->count--;
    pthread_mutex_unlock(&buffer->mutex);
    return element;
}

/**
 * Returns the number of elements in the circular buffer
 *
 * \param buffer pointer to struct representing a circular buffer
 * \return number of elements in the circular buffer
 */
int cb_count(circularbuffer_t* buffer) {
    return buffer->count;
}

/**
 * Returns whether the circular buffer is full
 *
 * \param buffer pointer to struct representing a circular buffer
 * \return boolean value indicating that the buffer is full
 */
int cb_isFull(circularbuffer_t* buffer) {
    return buffer->count == buffer->size;
}

/**
 * Returns whether the circular buffer is empty
 *
 * \param buffer pointer to struct representing a circular buffer
 * \return boolean value indicating whether the buffer is empty
 */
int cb_isEmpty(circularbuffer_t* buffer) {
    return buffer->count == 0;
}

typedef struct {
    int id;
    circularbuffer_t* buffer;
}producer_data_t;

void* producerThreadFunk(void* arg){
    producer_data_t* data = (producer_data_t*)arg;
    while (threadRunFlag){
        int temperatur = 20+ rand() %61;
        printf("Eine Temperatur %d wurde vom Producer %d Hinzugefuegt!\n", temperatur, data->id);
        cb_add(data->buffer, temperatur, data->id);
    }
    printf("Exiting producer thread %d\n", data->id);
    return NULL;
}

typedef struct {
    circularbuffer_t* buffer;
}consumer_data_t;

void* consumerThreadFunk(void* arg){
    consumer_data_t* data = (consumer_data_t*)arg;
    int temp_count = 0;
    int temp_sum = 0;
    int temp_avarage = 0;
    while (threadRunFlag){
        cb_element_t temp = cb_remove(data->buffer);
        printf("Eine Temperatur %d wurde gelesen!\n", temp );
        temp_count ++;
        temp_sum += temp;
        temp_avarage = temp_sum / temp_count;
        if (temp_count >= 1000){
            threadRunFlag = 0;



        }
    }
    printf("AvarageTemp: %d\n",temp_avarage);
    printf("Exit Consumer_Thread!\n");
    return NULL;
}



int main() {

    // Creates a circular buffer with 7 elements
    circularbuffer_t myCB;
    cb_init(&myCB, 7);

    pthread_t producer[2];
    producer_data_t producerData[2];
    for (int i = 0; i < 2; i++) {
        producerData[i].id = i;
        producerData[i].buffer = &myCB;
        pthread_create(&producer[i],NULL, producerThreadFunk, (void*)&producerData[i]);
        printf("Thread_create ID: %d\n", producerData[i].id);
    }

    pthread_t consumer;
    consumer_data_t consumerData;
    consumerData.buffer = &myCB;
    pthread_create(&consumer, NULL, consumerThreadFunk, (void*)&consumerData);
    for (int i = 0; i < 2; ++i) {
        pthread_join(producer[i], NULL);
    }

    pthread_join(consumer, NULL);


    // Destroys the buffer und frees all ressources
    cb_destroy(&myCB);
}






