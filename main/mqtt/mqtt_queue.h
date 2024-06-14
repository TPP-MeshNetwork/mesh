// Building mqtt queues for inter task communication

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "utils/uthash.h" // https://troydhanson.github.io/uthash/userguide.html

#ifndef MQTT_QUEUE_H
#define MQTT_QUEUE_H

extern int queueSize;
extern int suscriberQueueSize;
#define MAX_TOPIC_LENGTH 255
#define MAX_MESSAGE_LENGTH 1024

typedef struct {
    char topic[MAX_TOPIC_LENGTH];                    /* key topic */
    QueueHandle_t queue;       /* value */
    void (*event_handler)(char*topic, char *message); /* event handler function pointer */
    UT_hash_handle hh;         /* makes this structure hashable */
} SuscriptionTopicsHash_t;

extern SuscriptionTopicsHash_t *suscription_topics;


typedef struct {
    QueueHandle_t mqttPublisherQueue;
    SuscriptionTopicsHash_t *mqttSuscriberHash;
} mqtt_queues_t;

typedef struct {
    char message[MAX_MESSAGE_LENGTH];
    char topic[MAX_TOPIC_LENGTH];
} mqtt_message_t;

void init_suscriber_hash();
char ** get_topics_list();
// add event handler function pointer
void suscriber_add_topic(char *topic, void (*event_handler)(char* topic, char* message));
SuscriptionTopicsHash_t * suscriber_find_topic(const char* topic);
void suscriber_delete_topic(SuscriptionTopicsHash_t *s);
void suscriber_add_message(const char* topic, const char* message);
char * suscriber_get_message(const char* topic);

#endif