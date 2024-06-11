#include "core_mqtt.h"
#include "network_transport.h"

int start_mqtt_connection(MQTTContext_t * mqttContext, NetworkContext_t * xNetworkContext, char * clientIdentifier, char ** topics);

/**
 * @brief Close an MQTT session by sending MQTT DISCONNECT.
 *
 * @param[in] pMqttContext MQTT context pointer.
 *
 * @return EXIT_SUCCESS if DISCONNECT was successfully sent;
 * EXIT_FAILURE otherwise.
 */
int disconnectMqttSession( MQTTContext_t * pMqttContext );

/**
 * @brief Sends an MQTT PUBLISH to #MQTT_EXAMPLE_TOPIC defined at
 * the top of the file.
 *
 * @param[in] pMqttContext MQTT context pointer.
 *
 * @return EXIT_SUCCESS if PUBLISH was successfully sent;
 * EXIT_FAILURE otherwise.
 */
int publishToTopic( MQTTContext_t * pMqttContext, char * message, char *topic, MQTTQoS_t qos );

int publishLoop( MQTTContext_t * pMqttContext, char * message, char *topic);

MQTTStatus_t processLoopWithTimeout( MQTTContext_t * pMqttContext,
                                            uint32_t ulTimeoutMs );

int disconnectMqttSession( MQTTContext_t * pMqttContext );
