// Building mqtt queues for inter task communication
#include "freertos/semphr.h"

typedef struct {
    QueueHandle_t mqttPublisherQueue;
    QueueHandle_t mqttSuscriberQueue;    
} mqtt_queues_t;

typedef struct {
    char message[255];
    char topic[255];
} mqtt_message_t;


int queueSize = 100;