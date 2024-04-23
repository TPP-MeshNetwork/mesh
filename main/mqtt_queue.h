// Building mqtt queues for inter task communication

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "utils/uthash.h" // https://troydhanson.github.io/uthash/userguide.html

#ifndef MQTT_QUEUE_H
#define MQTT_QUEUE_H

extern int queueSize;
extern int suscriberQueueSize;
extern struct SuscriptionTopicsHash *suscription_topics;


struct SuscriptionTopicsHash {
    char topic[255];                    /* key topic */
    QueueHandle_t queue;       /* value */
    UT_hash_handle hh;         /* makes this structure hashable */
};

typedef struct {
    QueueHandle_t mqttPublisherQueue;
    struct SuscriptionTopicsHash *mqttSuscriberHash;
} mqtt_queues_t;

typedef struct {
    char message[255];
    char topic[255];
} mqtt_message_t;


char ** get_topics_list();
void suscriber_add_topic(char *topic);
struct SuscriptionTopicsHash * suscriber_find_topic(char * topic);
void suscriber_delete_topic(struct SuscriptionTopicsHash *s);

#endif