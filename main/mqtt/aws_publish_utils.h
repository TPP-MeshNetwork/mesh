/* Standard includes. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* MQTT API headers. */
#include "core_mqtt.h"
#include "core_mqtt_state.h"

static int publishLoop( MQTTContext_t * pMqttContext, char * message);
