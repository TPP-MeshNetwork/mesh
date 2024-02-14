#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* POSIX includes. */
#include <unistd.h>

/* MQTT API headers. */
#include "core_mqtt.h"
#include "core_mqtt_state.h"

#include "mqtt/aws_variables.h"
#include "mqtt/aws_publish_utils.h"
#include "mqtt/aws_initialize_utils.h"

/*-----------------------------------------------------------*/

void cleanupOutgoingPublishAt( uint8_t index )
{
    assert( outgoingPublishPackets != NULL );
    assert( index < MAX_OUTGOING_PUBLISHES );

    /* Clear the outgoing publish packet. */
    ( void ) memset( &( outgoingPublishPackets[ index ] ),
                     0x00,
                     sizeof( outgoingPublishPackets[ index ] ) );
}
/*-----------------------------------------------------------*/

int getNextFreeIndexForOutgoingPublishes( uint8_t * pIndex )
{
    int returnStatus = EXIT_FAILURE;
    uint8_t index = 0;

    assert( outgoingPublishPackets != NULL );
    assert( pIndex != NULL );

    for( index = 0; index < MAX_OUTGOING_PUBLISHES; index++ )
    {
        /* A free index is marked by invalid packet id.
         * Check if the the index has a free slot. */
        if( outgoingPublishPackets[ index ].packetId == MQTT_PACKET_ID_INVALID )
        {
            returnStatus = EXIT_SUCCESS;
            break;
        }
    }

    /* Copy the available index into the output param. */
    *pIndex = index;

    return returnStatus;
}

/*-----------------------------------------------------------*/

MQTTStatus_t processLoopWithTimeout( MQTTContext_t * pMqttContext,
                                            uint32_t ulTimeoutMs )
{
    uint32_t ulMqttProcessLoopTimeoutTime;
    uint32_t ulCurrentTime;

    MQTTStatus_t eMqttStatus = MQTTSuccess;

    ulCurrentTime = pMqttContext->getTime();
    ulMqttProcessLoopTimeoutTime = ulCurrentTime + ulTimeoutMs;

    /* Call MQTT_ProcessLoop multiple times a timeout happens, or
     * MQTT_ProcessLoop fails. */
    while( ( ulCurrentTime < ulMqttProcessLoopTimeoutTime ) &&
           ( eMqttStatus == MQTTSuccess || eMqttStatus == MQTTNeedMoreBytes ) )
    {
        eMqttStatus = MQTT_ProcessLoop( pMqttContext );
        ulCurrentTime = pMqttContext->getTime();
    }

    return eMqttStatus;
}

/*-----------------------------------------------------------*/

int waitForPacketAck( MQTTContext_t * pMqttContext,
                             uint16_t usPacketIdentifier,
                             uint32_t ulTimeout )
{
    uint32_t ulMqttProcessLoopEntryTime;
    uint32_t ulMqttProcessLoopTimeoutTime;
    uint32_t ulCurrentTime;

    MQTTStatus_t eMqttStatus = MQTTSuccess;
    int returnStatus = EXIT_FAILURE;

    /* Reset the ACK packet identifier being received. */
    globalAckPacketIdentifier = 0U;

    ulCurrentTime = pMqttContext->getTime();
    ulMqttProcessLoopEntryTime = ulCurrentTime;
    ulMqttProcessLoopTimeoutTime = ulCurrentTime + ulTimeout;

    /* Call MQTT_ProcessLoop multiple times until the expected packet ACK
     * is received, a timeout happens, or MQTT_ProcessLoop fails. */
    while( ( globalAckPacketIdentifier != usPacketIdentifier ) &&
           ( ulCurrentTime < ulMqttProcessLoopTimeoutTime ) &&
           ( eMqttStatus == MQTTSuccess || eMqttStatus == MQTTNeedMoreBytes ) )
    {
        /* Event callback will set #globalAckPacketIdentifier when receiving
         * appropriate packet. */
        eMqttStatus = MQTT_ProcessLoop( pMqttContext );
        ulCurrentTime = pMqttContext->getTime();
    }

    if( ( ( eMqttStatus != MQTTSuccess ) && ( eMqttStatus != MQTTNeedMoreBytes ) ) ||
        ( globalAckPacketIdentifier != usPacketIdentifier ) )
    {
        LogError( ( "MQTT_ProcessLoop failed to receive ACK packet: Expected ACK Packet ID=%02"PRIx16", LoopDuration=%"PRIu32", Status=%s",
                    usPacketIdentifier,
                    ( ulCurrentTime - ulMqttProcessLoopEntryTime ),
                    MQTT_Status_strerror( eMqttStatus ) ) );
    }
    else
    {
        returnStatus = EXIT_SUCCESS;
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/


int publishToTopic( MQTTContext_t * pMqttContext, char * message, char * topicName)
{
    int returnStatus = EXIT_SUCCESS;
    MQTTStatus_t mqttStatus = MQTTSuccess;
    uint8_t publishIndex = MAX_OUTGOING_PUBLISHES;

    assert( pMqttContext != NULL );

    uint16_t lenTopic = strlen( topicName );

    LogInfo( ( "Sending Publish to the MQTT topic %.*s. Message %s",
                       lenTopic,
                       topicName,
                       message ) );

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
        outgoingPublishPackets[ publishIndex ].pubInfo.pTopicName = topicName;
        outgoingPublishPackets[ publishIndex ].pubInfo.topicNameLength = lenTopic;
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
                       lenTopic,
                       topicName,
                       outgoingPublishPackets[ publishIndex ].packetId ) );
        }
    }

    return returnStatus;
}


int publishLoop( MQTTContext_t * pMqttContext, char * message, char * topicName)
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
            returnStatus = publishToTopic( pMqttContext, message, topicName );

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
    LogInfo( ( "Disconnecting the MQTT connection with %.*s.",
               AWS_IOT_ENDPOINT_LENGTH,
               AWS_IOT_ENDPOINT ) );
    if( returnStatus == EXIT_FAILURE )
    {
        /* Returned status is not used to update the local status as there
         * were failures in demo execution. */
        ( void ) disconnectMqttSession( pMqttContext );
    }
    else
    {
        returnStatus = disconnectMqttSession( pMqttContext );
    }

    /* Reset global SUBACK status variable after completion of subscription request cycle. */
    globalSubAckStatus = MQTTSubAckFailure;

    return returnStatus;
}


