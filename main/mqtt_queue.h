// Building mqtt queues for inter task communication

#include <freertos/queue.h>
typedef struct {
    QueueHandle_t mqttPublisherQueue;
    QueueHandle_t mqttSuscriberQueue; // TODO: change subscriber to be a hash: https://troydhanson.github.io/uthash/ so the keys are the topics and the values are the queues
} mqtt_queues_t;

typedef struct {
    char message[255];
    char topic[255];
} mqtt_message_t;


int queueSize = 10;