#include "mqtt_queue.h"
#include "esp_log.h"


int queueSize = 10;
int suscriberQueueSize = 3;
SuscriptionTopicsHash_t *suscription_topics = NULL;

// create mutex for suscription_topics
SemaphoreHandle_t xHashMutex = NULL;

void init_suscriber_hash() {
    suscription_topics = NULL;
    xHashMutex = xSemaphoreCreateMutex();
}


/* suscriber_add_topic
*  Description: Adds a topic to the hash table
*  Note: This function runs only once several times and does not need a mutex
*/
void suscriber_add_topic(char *topic,void (*event_handler)(char* topic, char *message)) {
    SuscriptionTopicsHash_t *s;
    xSemaphoreTake(xHashMutex, portMAX_DELAY);
    HASH_FIND_STR(suscription_topics, topic, s);  /* id already in the hash? */
    if (s == NULL) {
        s = (SuscriptionTopicsHash_t *) malloc(sizeof(SuscriptionTopicsHash_t));
        strcpy(s->topic, topic);
        s->queue = xQueueCreate(suscriberQueueSize, sizeof(mqtt_message_t));
        s->event_handler = event_handler;
        HASH_ADD_STR(suscription_topics, topic, s);

        HASH_FIND_STR(suscription_topics, topic, s);
        if (s) ESP_LOGI("SUSCRIBER", "topic %s succesfully added to hash", s->topic);
    } else {
        ESP_LOGI("SUSCRIBER", "topic %s already exists in hash", s->topic);
    }
    xSemaphoreGive(xHashMutex);
}

/* suscriber_find_topic
*  Description: Finds a topic in the hash table
*/
SuscriptionTopicsHash_t * suscriber_find_topic(const char * topic) {
    xSemaphoreTake(xHashMutex, portMAX_DELAY);
    SuscriptionTopicsHash_t *s = NULL;
    HASH_FIND_STR(suscription_topics, topic, s);  /* s: output pointer */
    xSemaphoreGive(xHashMutex);
    return s;
}

/* suscriber_get_message
*  Description: Gets a message from the queue of a topic
*/
char * suscriber_get_message(const char *topic) {
    char * message = NULL;
    SuscriptionTopicsHash_t *s = suscriber_find_topic(topic);
    xSemaphoreTake(xHashMutex, portMAX_DELAY); // Important! need to lock the hash so that no two processes can access the same topic
    if (s != NULL) {
        mqtt_message_t s_message;
        size_t available_messages = uxQueueMessagesWaiting(s->queue);
        if (s->queue != NULL && available_messages > 0) {
            if ( xQueueReceive(s->queue, &s_message, 10) == pdPASS ) {
                // ESP_LOG_BUFFER_HEXDUMP("SUSCRIBER", &s_message, sizeof(s_message), ESP_LOG_INFO);
                ESP_LOGI("[suscriber_get_message]", "Message: %s", s_message.message);
                ESP_LOGI("[suscriber_get_message]", "Topic: %s", s_message.topic);
                message = (char *) calloc((sizeof(char)) * MAX_MESSAGE_LENGTH, 1);
                strcpy(message, s_message.message);
            } else {
                ESP_LOGI("SUSCRIBER", "Failed to receive message from queue");
            }
        }
    }
    xSemaphoreGive(xHashMutex);
    return message;
}

/* suscriber_add_message
*  Description: Adds a message to the queue of a topic
*/
void suscriber_add_message(const char *topic, const char *message) {
    ESP_LOGI("SUSCRIBER", "Adding message to topic %s", topic);
    SuscriptionTopicsHash_t *s = suscriber_find_topic(topic);

    // making a static message so that the queue copies the struct and not the pointer
    mqtt_message_t s_message;
    // copy message by message_length in msg.message
    const size_t message_length = strlen(message);
    strncpy(s_message.message, message, message_length);
    s_message.message[message_length] = '\0';
    // copy topic by topic_length in msg.topic
    const size_t topic_length = strlen(topic);
    strncpy(s_message.topic, topic, topic_length);
    s_message.topic[topic_length] = '\0';

    xSemaphoreTake(xHashMutex, portMAX_DELAY);
    if(s==NULL) ESP_LOGD("[suscriber_add_message]", "s is null\n");
    if (s != NULL) {
        ESP_LOGD("[suscriber_add_message]", "--->>>>> pointer message: %p", &message);
        // Note: do not change xTicksToWait because it is a blocking call
        if ( xQueueGenericSend(s->queue, ( void * ) &s_message, 0, queueSEND_TO_BACK) != pdPASS ) {
            ESP_LOGI("SUSCRIBER", "Failed to send message to queue");
        } else {
            ESP_LOGI("SUSCRIBER", "Message sent to queue");
        }
    }
    xSemaphoreGive(xHashMutex);
}

/* suscriber_delete_topic
*  Description: Deletes a topic from the hash table
*/
void suscriber_delete_topic(SuscriptionTopicsHash_t *s) {
    xSemaphoreTake(xHashMutex, portMAX_DELAY);
    if (s) {
        HASH_DEL(suscription_topics, s);  /* user: pointer to deletee */
        free(s);
    }
    xSemaphoreGive(xHashMutex);
}

/* get_topics_list
* Description: Returns a list of topics in the hash table
*/
char ** get_topics_list() {
    SuscriptionTopicsHash_t *s, *tmp;
    char **topics = NULL;
    int i = 0;

    xSemaphoreTake(xHashMutex, portMAX_DELAY);
    HASH_ITER(hh, suscription_topics, s, tmp) {
        topics = realloc(topics, (i + 1) * sizeof(char *));
        topics[i] = s->topic;
        i++;
    }
    // end with NULL
    topics = realloc(topics, (i + 1) * sizeof(char *));
    topics[i] = NULL;

    xSemaphoreGive(xHashMutex);
    return topics;
}