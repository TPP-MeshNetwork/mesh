#include "mqtt_queue.h"
#include "esp_log.h"


int queueSize = 10;
int suscriberQueueSize = 3;
struct SuscriptionTopicsHash *suscription_topics = NULL;

/* suscriber_add_topic
*  Description: Adds a topic to the hash table
*/
void suscriber_add_topic(char *topic) {
    struct SuscriptionTopicsHash *s;

    HASH_FIND_STR(suscription_topics, topic, s);  /* id already in the hash? */
    if (s == NULL) {
        s = (struct SuscriptionTopicsHash*)malloc(sizeof *s);
        strcpy(s->topic, topic);
        s->queue = xQueueCreate(queueSize, sizeof(mqtt_message_t) * suscriberQueueSize);
        HASH_ADD_STR(suscription_topics, topic, s);

        HASH_FIND_STR(suscription_topics, topic, s);
        if (s) ESP_LOGI("SUSCRIBER", "topic %s succesfully added to hash", s->topic);
    } else {
        ESP_LOGI("SUSCRIBER", "topic %s already exists in hash", s->topic);
    }
}

/* suscriber_find_topic
*  Description: Finds a topic in the hash table
*/
struct SuscriptionTopicsHash * suscriber_find_topic(char * topic) {
    struct SuscriptionTopicsHash *s;
    HASH_FIND_STR(suscription_topics, topic, s);  /* s: output pointer */
    return s;
}

/* suscriber_delete_topic
*  Description: Deletes a topic from the hash table
*/
void suscriber_delete_topic(struct SuscriptionTopicsHash *s) {
    if (s) {
        HASH_DEL(suscription_topics, s);  /* user: pointer to deletee */
        free(s);
    }
}

/* get_topics_list
* Description: Returns a list of topics in the hash table
*/
char ** get_topics_list() {
    struct SuscriptionTopicsHash *s, *tmp;
    char **topics = NULL;
    int i = 0;

    HASH_ITER(hh, suscription_topics, s, tmp) {
        topics = realloc(topics, (i + 1) * sizeof(char *));
        topics[i] = s->topic;
        i++;
    }
    // end with NULL
    topics = realloc(topics, (i + 1) * sizeof(char *));
    topics[i] = NULL;

    return topics;
}