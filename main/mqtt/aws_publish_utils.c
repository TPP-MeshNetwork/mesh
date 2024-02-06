#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* POSIX includes. */
#include <unistd.h>

/* MQTT API headers. */
#include "core_mqtt.h"
#include "core_mqtt_state.h"


/**
 * @brief Delay between MQTT publishes in seconds.
 */
#define DELAY_BETWEEN_PUBLISHES_SECONDS     ( 1U )
/**
 * @brief Timeout for MQTT_ProcessLoop function in milliseconds.
 */
#define MQTT_PROCESS_LOOP_TIMEOUT_MS        ( 5000U )
/**
 * @brief Number of PUBLISH messages sent per iteration.
 */
#define MQTT_PUBLISH_COUNT_PER_LOOP         ( 5U )
/**
 * @brief Packet Identifier generated when Unsubscribe request was sent to the broker;
 * it is used to match received Unsubscribe ACK to the transmitted unsubscribe
 * request.
 */
static uint16_t globalUnsubscribePacketIdentifier = 0U;
/**
 * @brief Status of latest Subscribe ACK;
 * it is updated every time the callback function processes a Subscribe ACK
 * and accounts for subscription to a single topic.
 */
static MQTTSubAckStatus_t globalSubAckStatus = MQTTSubAckFailure;

static int publishToTopic( MQTTContext_t * pMqttContext, char * message )
{
    int returnStatus = EXIT_SUCCESS;
    MQTTStatus_t mqttStatus = MQTTSuccess;
    uint8_t publishIndex = MAX_OUTGOING_PUBLISHES;

    assert( pMqttContext != NULL );

    /* Get the next free index for the outgoing publish. All QoS1 outgoing
     * publishes are stored until a PUBACK is received. These messages are
     * stored for supporting a resend if a network connection is broken before
     * receiving a PUBACK. */
    returnStatus = getNextFreeIndexForOutgoingPublishes( &publishIndex );

    if( returnStatus == EXIT_FAILURE )
    {
        LogError( ( "Unable to find a free spot for outgoing PUBLISH message.\n\n" ) );
    }
    else
    {
        /* This example publishes to only one topic and uses QOS1. */
        outgoingPublishPackets[ publishIndex ].pubInfo.qos = MQTTQoS1;
        outgoingPublishPackets[ publishIndex ].pubInfo.pTopicName = MQTT_EXAMPLE_TOPIC;
        outgoingPublishPackets[ publishIndex ].pubInfo.topicNameLength = MQTT_EXAMPLE_TOPIC_LENGTH;
        outgoingPublishPackets[ publishIndex ].pubInfo.pPayload = message;
        outgoingPublishPackets[ publishIndex ].pubInfo.payloadLength = ( uint16_t ) (sizeof(message)*(sizeof(char *)) - 1);

        /* Get a new packet id. */
        outgoingPublishPackets[ publishIndex ].packetId = MQTT_GetPacketId( pMqttContext );

        /* Send PUBLISH packet. */
        mqttStatus = MQTT_Publish( pMqttContext,
                                   &outgoingPublishPackets[ publishIndex ].pubInfo,
                                   outgoingPublishPackets[ publishIndex ].packetId );

        if( mqttStatus != MQTTSuccess )
        {
            LogError( ( "Failed to send PUBLISH packet to broker with error = %s.",
                        MQTT_Status_strerror( mqttStatus ) ) );
            cleanupOutgoingPublishAt( publishIndex );
            returnStatus = EXIT_FAILURE;
        }
        else
        {
            LogInfo( ( "PUBLISH sent for topic %.*s to broker with packet ID %u.\n\n",
                       MQTT_EXAMPLE_TOPIC_LENGTH,
                       MQTT_EXAMPLE_TOPIC,
                       outgoingPublishPackets[ publishIndex ].packetId ) );
        }
    }

    return returnStatus;
}


static int publishLoop( MQTTContext_t * pMqttContext, char * message)
{
    int returnStatus = EXIT_SUCCESS;
    MQTTStatus_t mqttStatus = MQTTSuccess;
    uint32_t publishCount = 0;
    const uint32_t maxPublishCount = MQTT_PUBLISH_COUNT_PER_LOOP;

    assert( pMqttContext != NULL );

    if( returnStatus == EXIT_SUCCESS )
    {
        /* Publish messages with QOS1, receive incoming messages and
         * send keep alive messages. */
        for( publishCount = 0; publishCount < maxPublishCount; publishCount++ )
        {
            LogInfo( ( "Sending Publish to the MQTT topic %.*s.",
                       MQTT_EXAMPLE_TOPIC_LENGTH,
                       MQTT_EXAMPLE_TOPIC ) );
            returnStatus = publishToTopic( pMqttContext, message );

            /* Calling MQTT_ProcessLoop to process incoming publish echo, since
             * application subscribed to the same topic the broker will send
             * publish message back to the application. This function also
             * sends ping request to broker if MQTT_KEEP_ALIVE_INTERVAL_SECONDS
             * has expired since the last MQTT packet sent and receive
             * ping responses. */
            mqttStatus = processLoopWithTimeout( pMqttContext, MQTT_PROCESS_LOOP_TIMEOUT_MS );

            /* For any error in #MQTT_ProcessLoop, exit the loop and disconnect
             * from the broker. */
            if( ( mqttStatus != MQTTSuccess ) && ( mqttStatus != MQTTNeedMoreBytes ) )
            {
                LogError( ( "MQTT_ProcessLoop returned with status = %s.",
                            MQTT_Status_strerror( mqttStatus ) ) );
                returnStatus = EXIT_FAILURE;
                break;
            }

            LogInfo( ( "Delay before continuing to next iteration.\n\n" ) );

            /* Leave connection idle for some time. */
            sleep( DELAY_BETWEEN_PUBLISHES_SECONDS );
        }
    }

    if( returnStatus == EXIT_SUCCESS )
    {
        /* Process Incoming UNSUBACK packet from the broker. */
        returnStatus = waitForPacketAck( pMqttContext,
                                         globalUnsubscribePacketIdentifier,
                                         MQTT_PROCESS_LOOP_TIMEOUT_MS );
    }

    /* Send an MQTT Disconnect packet over the already connected TCP socket.
     * There is no corresponding response for the disconnect packet. After sending
     * disconnect, client must close the network connection. */
    // LogInfo( ( "Disconnecting the MQTT connection with %.*s.",
    //            AWS_IOT_ENDPOINT_LENGTH,
    //            AWS_IOT_ENDPOINT ) );
    // if( returnStatus == EXIT_FAILURE )
    // {
    //     /* Returned status is not used to update the local status as there
    //      * were failures in demo execution. */
    //     ( void ) disconnectMqttSession( pMqttContext );
    // }
    // else
    // {
    //     returnStatus = disconnectMqttSession( pMqttContext );
    // }

    /* Reset global SUBACK status variable after completion of subscription request cycle. */
    globalSubAckStatus = MQTTSubAckFailure;

    return returnStatus;
}


