/* Standard includes. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* POSIX includes. */
#include <unistd.h>

/* Include Demo Config as the first non-system header. */
#include "demo_config.h"

/* MQTT API headers. */
#include "core_mqtt.h"
#include "core_mqtt_state.h"

/* OpenSSL sockets transport implementation. */
#include "network_transport.h"

/*Include backoff algorithm header for retry logic.*/
#include "backoff_algorithm.h"

/* Clock for timer. */
#include "clock.h"

#ifdef CONFIG_EXAMPLE_USE_ESP_SECURE_CERT_MGR
    #include "esp_secure_cert_read.h"    
#endif

#ifdef CONFIG_EXAMPLE_USE_ESP_SECURE_CERT_MGR
    #include "esp_secure_cert_read.h"    
#endif

#ifndef CLIENT_IDENTIFIER
    #error "Please define a unique client identifier, CLIENT_IDENTIFIER, in menuconfig"
#endif


/**
 * @brief The length of the MQTT metrics string expected by AWS IoT.
 */
#define METRICS_STRING_LENGTH               ( ( uint16_t ) ( sizeof( METRICS_STRING ) - 1 ) )

/**
 * @brief The length of the outgoing publish records array used by the coreMQTT
 * library to track QoS > 0 packet ACKS for outgoing publishes.
 */
#define OUTGOING_PUBLISH_RECORD_LEN    ( 10U )

/**
 * @brief The length of the incoming publish records array used by the coreMQTT
 * library to track QoS > 0 packet ACKS for incoming publishes.
 */
#define INCOMING_PUBLISH_RECORD_LEN    ( 10U )

/**
 * @brief Size of the network buffer for MQTT packets.
 */
#define NETWORK_BUFFER_SIZE       ( CONFIG_MQTT_NETWORK_BUFFER_SIZE )

/**
 * @brief The network buffer must remain valid for the lifetime of the MQTT context.
 */
static uint8_t buffer[ NETWORK_BUFFER_SIZE ];

/**
 * @brief Delay in seconds between two iterations of subscribePublishLoop().
 */
#define MQTT_SUBPUB_LOOP_DELAY_SECONDS      ( 5U )

/**
 * @brief Length of MQTT server host name.
 */
#define AWS_IOT_ENDPOINT_LENGTH         ( ( uint16_t ) ( sizeof( AWS_IOT_ENDPOINT ) - 1 ) )

/**
 * @brief The maximum number of retries for connecting to server.
 */
#define CONNECTION_RETRY_MAX_ATTEMPTS            ( 5U )

/**
 * @brief The maximum back-off delay (in milliseconds) for retrying connection to server.
 */
#define CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS    ( 5000U )

/**
 * @brief The base back-off delay (in milliseconds) to use for connection retry attempts.
 */
#define CONNECTION_RETRY_BACKOFF_BASE_MS         ( 500U )

/**
 * @brief Timeout for receiving CONNACK packet in milli seconds.
 */
#define CONNACK_RECV_TIMEOUT_MS                  ( 1000U )

/**
 * @brief Length of client MQTT topic.
 */
#define MQTT_EXAMPLE_TOPIC_LENGTH           ( ( uint16_t ) ( sizeof( MQTT_EXAMPLE_TOPIC ) - 1 ) )

/**
 * @brief The topic to subscribe and publish to in the example.
 *
 * The topic name starts with the client identifier to ensure that each demo
 * interacts with a unique topic name.
 */
#define MQTT_EXAMPLE_TOPIC                  CLIENT_IDENTIFIER "/example/topic"

/**
 * @brief Sends an MQTT CONNECT packet over the already connected TCP socket.
 *
 * @param[in] pMqttContext MQTT context pointer.
 * @param[in] createCleanSession Creates a new MQTT session if true.
 * If false, tries to establish the existing session if there was session
 * already present in broker.
 * @param[out] pSessionPresent Session was already present in the broker or not.
 * Session present response is obtained from the CONNACK from broker.
 *
 * @return EXIT_SUCCESS if an MQTT session is established;
 * EXIT_FAILURE otherwise.
 */
static int establishMqttSession( MQTTContext_t * pMqttContext,
                                 bool createCleanSession,
                                 bool * pSessionPresent );
/**
 * @brief Array to track the outgoing publish records for outgoing publishes
 * with QoS > 0.
 *
 * This is passed into #MQTT_InitStatefulQoS to allow for QoS > 0.
 *
 */
static MQTTPubAckInfo_t pOutgoingPublishRecords[ OUTGOING_PUBLISH_RECORD_LEN ];

/**
 * @brief Array to track the incoming publish records for incoming publishes
 * with QoS > 0.
 *
 * This is passed into #MQTT_InitStatefulQoS to allow for QoS > 0.
 *
 */
static MQTTPubAckInfo_t pIncomingPublishRecords[ INCOMING_PUBLISH_RECORD_LEN ];

/**
 * @brief The application callback function for getting the incoming publish
 * and incoming acks reported from MQTT library.
 *
 * @param[in] pMqttContext MQTT context pointer.
 * @param[in] pPacketInfo Packet Info pointer for the incoming packet.
 * @param[in] pDeserializedInfo Deserialized information from the incoming packet.
 */
static void eventCallback( MQTTContext_t * pMqttContext,
                           MQTTPacketInfo_t * pPacketInfo,
                           MQTTDeserializedInfo_t * pDeserializedInfo );


/**
 * @brief Close an MQTT session by sending MQTT DISCONNECT.
 *
 * @param[in] pMqttContext MQTT context pointer.
 *
 * @return EXIT_SUCCESS if DISCONNECT was successfully sent;
 * EXIT_FAILURE otherwise.
 */
static int disconnectMqttSession( MQTTContext_t * pMqttContext );

/**
 * @brief Initializes the MQTT library.
 *
 * @param[in] pMqttContext MQTT context pointer.
 * @param[in] pNetworkContext The network context pointer.
 *
 * @return EXIT_SUCCESS if the MQTT library is initialized;
 * EXIT_FAILURE otherwise.
 */
static int initializeMqtt( MQTTContext_t * pMqttContext,
                           NetworkContext_t * pNetworkContext );

/**
 * @brief Connect to MQTT broker with reconnection retries.
 *
 * If connection fails, retry is attempted after a timeout.
 * Timeout value will exponentially increase until maximum
 * timeout value is reached or the number of attempts are exhausted.
 *
 * @param[out] pNetworkContext The output parameter to return the created network context.
 * @param[out] pMqttContext The output to return the created MQTT context.
 * @param[in,out] pClientSessionPresent Pointer to flag indicating if an
 * MQTT session is present in the client.
 * @param[out] pBrokerSessionPresent Session was already present in the broker or not.
 * Session present response is obtained from the CONNACK from broker.
 *
 * @return EXIT_FAILURE on failure; EXIT_SUCCESS on successful connection.
 */
static int connectToServerWithBackoffRetries( NetworkContext_t * pNetworkContext,
                                              MQTTContext_t * pMqttContext,
                                              bool * pClientSessionPresent,
                                              bool * pBrokerSessionPresent );

/**
 * @brief Function to clean up all the outgoing publishes maintained in the
 * array.
 */
static void cleanupOutgoingPublishes( void );

/**
 * @brief Function to update variable globalSubAckStatus with status
 * information from Subscribe ACK. Called by eventCallback after processing
 * incoming subscribe echo.
 *
 * @param[in] Server response to the subscription request.
 */
static void updateSubAckStatus( MQTTPacketInfo_t * pPacketInfo );

/*-----------------------------------------------------------*/

static int establishMqttSession( MQTTContext_t * pMqttContext,
                                 bool createCleanSession,
                                 bool * pSessionPresent )
{
    int returnStatus = EXIT_SUCCESS;
    MQTTStatus_t mqttStatus;
    MQTTConnectInfo_t connectInfo = { 0 };

    assert( pMqttContext != NULL );
    assert( pSessionPresent != NULL );

    /* Establish MQTT session by sending a CONNECT packet. */

    /* If #createCleanSession is true, start with a clean session
     * i.e. direct the MQTT broker to discard any previous session data.
     * If #createCleanSession is false, directs the broker to attempt to
     * reestablish a session which was already present. */
    connectInfo.cleanSession = createCleanSession;

    /* The client identifier is used to uniquely identify this MQTT client to
     * the MQTT broker. In a production device the identifier can be something
     * unique, such as a device serial number. */
    connectInfo.pClientIdentifier = CLIENT_IDENTIFIER;
    connectInfo.clientIdentifierLength = CLIENT_IDENTIFIER_LENGTH;

    /* The maximum time interval in seconds which is allowed to elapse
     * between two Control Packets.
     * It is the responsibility of the Client to ensure that the interval between
     * Control Packets being sent does not exceed the this Keep Alive value. In the
     * absence of sending any other Control Packets, the Client MUST send a
     * PINGREQ Packet. */
    connectInfo.keepAliveSeconds = MQTT_KEEP_ALIVE_INTERVAL_SECONDS;

    /* Use the username and password for authentication, if they are defined.
     * Refer to the AWS IoT documentation below for details regarding client
     * authentication with a username and password.
     * https://docs.aws.amazon.com/iot/latest/developerguide/custom-authentication.html
     * An authorizer setup needs to be done, as mentioned in the above link, to use
     * username/password based client authentication.
     *
     * The username field is populated with voluntary metrics to AWS IoT.
     * The metrics collected by AWS IoT are the operating system, the operating
     * system's version, the hardware platform, and the MQTT Client library
     * information. These metrics help AWS IoT improve security and provide
     * better technical support.
     *
     * If client authentication is based on username/password in AWS IoT,
     * the metrics string is appended to the username to support both client
     * authentication and metrics collection. */
    #ifdef CLIENT_USERNAME
        connectInfo.pUserName = CLIENT_USERNAME_WITH_METRICS;
        connectInfo.userNameLength = strlen( CLIENT_USERNAME_WITH_METRICS );
        connectInfo.pPassword = CLIENT_PASSWORD;
        connectInfo.passwordLength = strlen( CLIENT_PASSWORD );
    #else
        connectInfo.pUserName = METRICS_STRING;
        connectInfo.userNameLength = METRICS_STRING_LENGTH;
        /* Password for authentication is not used. */
        connectInfo.pPassword = NULL;
        connectInfo.passwordLength = 0U;
    #endif /* ifdef CLIENT_USERNAME */

    /* Send MQTT CONNECT packet to broker. */
    mqttStatus = MQTT_Connect( pMqttContext, &connectInfo, NULL, CONNACK_RECV_TIMEOUT_MS, pSessionPresent );

    if( mqttStatus != MQTTSuccess )
    {
        returnStatus = EXIT_FAILURE;
        LogError( ( "Connection with MQTT broker failed with status %s.",
                    MQTT_Status_strerror( mqttStatus ) ) );
    }
    else
    {
        LogInfo( ( "MQTT connection successfully established with broker.\n\n" ) );
    }

    return returnStatus;
}


/*-----------------------------------------------------------*/

static int disconnectMqttSession( MQTTContext_t * pMqttContext )
{
    MQTTStatus_t mqttStatus = MQTTSuccess;
    int returnStatus = EXIT_SUCCESS;

    assert( pMqttContext != NULL );

    /* Send DISCONNECT. */
    mqttStatus = MQTT_Disconnect( pMqttContext );

    if( mqttStatus != MQTTSuccess )
    {
        LogError( ( "Sending MQTT DISCONNECT failed with status=%s.",
                    MQTT_Status_strerror( mqttStatus ) ) );
        returnStatus = EXIT_FAILURE;
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

static void handleIncomingPublish( MQTTPublishInfo_t * pPublishInfo,
                                   uint16_t packetIdentifier )
{
    assert( pPublishInfo != NULL );

    /* Process incoming Publish. */
    LogInfo( ( "Incoming QOS : %d.", pPublishInfo->qos ) );

    /* Verify the received publish is for the topic we have subscribed to. */
    if( ( pPublishInfo->topicNameLength == MQTT_EXAMPLE_TOPIC_LENGTH ) &&
        ( 0 == strncmp( MQTT_EXAMPLE_TOPIC,
                        pPublishInfo->pTopicName,
                        pPublishInfo->topicNameLength ) ) )
    {
        LogInfo( ( "Incoming Publish Topic Name: %.*s matches subscribed topic.\n"
                   "Incoming Publish message Packet Id is %u.\n"
                   "Incoming Publish Message : %.*s.\n\n",
                   pPublishInfo->topicNameLength,
                   pPublishInfo->pTopicName,
                   packetIdentifier,
                   ( int ) pPublishInfo->payloadLength,
                   ( const char * ) pPublishInfo->pPayload ) );
    }
    else
    {
        LogInfo( ( "Incoming Publish Topic Name: %.*s does not match subscribed topic.",
                   pPublishInfo->topicNameLength,
                   pPublishInfo->pTopicName ) );
    }
}

/*-----------------------------------------------------------*/

static void eventCallback( MQTTContext_t * pMqttContext,
                           MQTTPacketInfo_t * pPacketInfo,
                           MQTTDeserializedInfo_t * pDeserializedInfo )
{
    uint16_t packetIdentifier;

    assert( pMqttContext != NULL );
    assert( pPacketInfo != NULL );
    assert( pDeserializedInfo != NULL );

    /* Suppress unused parameter warning when asserts are disabled in build. */
    ( void ) pMqttContext;

    packetIdentifier = pDeserializedInfo->packetIdentifier;

    /* Handle incoming publish. The lower 4 bits of the publish packet
     * type is used for the dup, QoS, and retain flags. Hence masking
     * out the lower bits to check if the packet is publish. */
    if( ( pPacketInfo->type & 0xF0U ) == MQTT_PACKET_TYPE_PUBLISH )
    {
        assert( pDeserializedInfo->pPublishInfo != NULL );
        /* Handle incoming publish. */
        handleIncomingPublish( pDeserializedInfo->pPublishInfo, packetIdentifier );
    }
    else
    {
        /* Handle other packets. */
        switch( pPacketInfo->type )
        {
            case MQTT_PACKET_TYPE_SUBACK:

                /* A SUBACK from the broker, containing the server response to our subscription request, has been received.
                 * It contains the status code indicating server approval/rejection for the subscription to the single topic
                 * requested. The SUBACK will be parsed to obtain the status code, and this status code will be stored in global
                 * variable globalSubAckStatus. */
                updateSubAckStatus( pPacketInfo );

                /* Check status of the subscription request. If globalSubAckStatus does not indicate
                 * server refusal of the request (MQTTSubAckFailure), it contains the QoS level granted
                 * by the server, indicating a successful subscription attempt. */
                if( globalSubAckStatus != MQTTSubAckFailure )
                {
                    LogInfo( ( "Subscribed to the topic %.*s. with maximum QoS %u.\n\n",
                               MQTT_EXAMPLE_TOPIC_LENGTH,
                               MQTT_EXAMPLE_TOPIC,
                               globalSubAckStatus ) );
                }

                /* Make sure ACK packet identifier matches with Request packet identifier. */
                assert( globalSubscribePacketIdentifier == packetIdentifier );

                /* Update the global ACK packet identifier. */
                globalAckPacketIdentifier = packetIdentifier;
                break;

            case MQTT_PACKET_TYPE_UNSUBACK:
                LogInfo( ( "Unsubscribed from the topic %.*s.\n\n",
                           MQTT_EXAMPLE_TOPIC_LENGTH,
                           MQTT_EXAMPLE_TOPIC ) );
                /* Make sure ACK packet identifier matches with Request packet identifier. */
                assert( globalUnsubscribePacketIdentifier == packetIdentifier );

                /* Update the global ACK packet identifier. */
                globalAckPacketIdentifier = packetIdentifier;
                break;

            case MQTT_PACKET_TYPE_PINGRESP:

                /* Nothing to be done from application as library handles
                 * PINGRESP. */
                LogWarn( ( "PINGRESP should not be handled by the application "
                           "callback when using MQTT_ProcessLoop.\n\n" ) );
                break;

            case MQTT_PACKET_TYPE_PUBACK:
                LogInfo( ( "PUBACK received for packet id %u.\n\n",
                           packetIdentifier ) );
                /* Cleanup publish packet when a PUBACK is received. */
                cleanupOutgoingPublishWithPacketID( packetIdentifier );

                /* Update the global ACK packet identifier. */
                globalAckPacketIdentifier = packetIdentifier;
                break;

            /* Any other packet type is invalid. */
            default:
                LogError( ( "Unknown packet type received:(%02x).\n\n",
                            pPacketInfo->type ) );
        }
    }
}

/*-----------------------------------------------------------*/

static void updateSubAckStatus( MQTTPacketInfo_t * pPacketInfo )
{
    uint8_t * pPayload = NULL;
    size_t pSize = 0;

    MQTTStatus_t mqttStatus = MQTT_GetSubAckStatusCodes( pPacketInfo, &pPayload, &pSize );

    /* MQTT_GetSubAckStatusCodes always returns success if called with packet info
     * from the event callback and non-NULL parameters. */
    assert( mqttStatus == MQTTSuccess );

    /* Suppress unused variable warning when asserts are disabled in build. */
    ( void ) mqttStatus;

    /* Demo only subscribes to one topic, so only one status code is returned. */
    globalSubAckStatus = ( MQTTSubAckStatus_t ) pPayload[ 0 ];
}

/*-----------------------------------------------------------*/

static int connectToServerWithBackoffRetries( NetworkContext_t * pNetworkContext,
                                              MQTTContext_t * pMqttContext,
                                              bool * pClientSessionPresent,
                                              bool * pBrokerSessionPresent )
{
    int returnStatus = EXIT_SUCCESS;
    BackoffAlgorithmStatus_t backoffAlgStatus = BackoffAlgorithmSuccess;
    TlsTransportStatus_t tlsStatus = TLS_TRANSPORT_SUCCESS;
    BackoffAlgorithmContext_t reconnectParams;
    bool createCleanSession;

    pNetworkContext->pcHostname = AWS_IOT_ENDPOINT;
    pNetworkContext->xPort = AWS_MQTT_PORT;
    pNetworkContext->pxTls = NULL;
    pNetworkContext->xTlsContextSemaphore = xSemaphoreCreateMutexStatic(&xTlsContextSemaphoreBuffer);

    pNetworkContext->disableSni = 0;
    uint16_t nextRetryBackOff;

    /* Initialize credentials for establishing TLS session. */
    pNetworkContext->pcServerRootCA = root_cert_auth_start;
    pNetworkContext->pcServerRootCASize = root_cert_auth_end - root_cert_auth_start;

    /* If #CLIENT_USERNAME is defined, username/password is used for authenticating
     * the client. */
#ifdef CONFIG_EXAMPLE_USE_SECURE_ELEMENT
    pNetworkContext->use_secure_element = true;

#elif defined(CONFIG_EXAMPLE_USE_ESP_SECURE_CERT_MGR)
    if (esp_secure_cert_get_device_cert(&pNetworkContext->pcClientCert, &pNetworkContext->pcClientCertSize) != ESP_OK) {
        LogError( ( "Failed to obtain flash address of device cert") );
        return EXIT_FAILURE;
    }
#ifdef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
    pNetworkContext->ds_data = esp_secure_cert_get_ds_ctx();
    if (pNetworkContext->ds_data == NULL) {
        LogError( ( "Failed to obtain the ds context") );
        return EXIT_FAILURE;
    }
#else /* !CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */
    if (esp_secure_cert_get_priv_key(&pNetworkContext->pcClientKey, &pNetworkContext->pcClientKeySize) != ESP_OK) {
        LogError( ( "Failed to obtain flash address of private_key") );
        return EXIT_FAILURE;
    }
#endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */

#else /* !CONFIG_EXAMPLE_USE_SECURE_ELEMENT && !CONFIG_EXAMPLE_USE_ESP_SECURE_CERT_MGR  */
    #ifndef CLIENT_USERNAME
        pNetworkContext->pcClientCert = client_cert_start;
        pNetworkContext->pcClientCertSize = client_cert_end - client_cert_start;
        pNetworkContext->pcClientKey = client_key_start;
        pNetworkContext->pcClientKeySize = client_key_end - client_key_start;
    #endif
#endif
    /* AWS IoT requires devices to send the Server Name Indication (SNI)
     * extension to the Transport Layer Security (TLS) protocol and provide
     * the complete endpoint address in the host_name field. Details about
     * SNI for AWS IoT can be found in the link below.
     * https://docs.aws.amazon.com/iot/latest/developerguide/transport-security.html */

    if( AWS_MQTT_PORT == 443 )
    {
        /* Pass the ALPN protocol name depending on the port being used.
         * Please see more details about the ALPN protocol for the AWS IoT MQTT
         * endpoint in the link below.
         * https://aws.amazon.com/blogs/iot/mqtt-with-tls-client-authentication-on-port-443-why-it-is-useful-and-how-it-works/
         *
         * For username and password based authentication in AWS IoT,
         * #AWS_IOT_PASSWORD_ALPN is used. More details can be found in the
         * link below.
         * https://docs.aws.amazon.com/iot/latest/developerguide/custom-authentication.html
         */

        static const char * pcAlpnProtocols[] = { NULL, NULL };

        #ifdef CLIENT_USERNAME
            pcAlpnProtocols[0] = AWS_IOT_PASSWORD_ALPN;
        #else
            pcAlpnProtocols[0] = AWS_IOT_MQTT_ALPN;
        #endif

        pNetworkContext->pAlpnProtos = pcAlpnProtocols;
    } else {
        pNetworkContext->pAlpnProtos = NULL;
    }

    /* Initialize reconnect attempts and interval */
    BackoffAlgorithm_InitializeParams( &reconnectParams,
                                       CONNECTION_RETRY_BACKOFF_BASE_MS,
                                       CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS,
                                       CONNECTION_RETRY_MAX_ATTEMPTS );

    /* Attempt to connect to MQTT broker. If connection fails, retry after
     * a timeout. Timeout value will exponentially increase until maximum
     * attempts are reached.
     */
    do
    {
        /* Establish a TLS session with the MQTT broker. This example connects
         * to the MQTT broker as specified in AWS_IOT_ENDPOINT and AWS_MQTT_PORT
         * at the demo config header. */
        LogInfo( ( "Establishing a TLS session to %.*s:%d.",
                   AWS_IOT_ENDPOINT_LENGTH,
                   AWS_IOT_ENDPOINT,
                   AWS_MQTT_PORT ) );
        tlsStatus = xTlsConnect ( pNetworkContext );

        if( tlsStatus == TLS_TRANSPORT_SUCCESS )
        {
            /* A clean MQTT session needs to be created, if there is no session saved
             * in this MQTT client. */
            createCleanSession = ( *pClientSessionPresent == true ) ? false : true;

            /* Sends an MQTT Connect packet using the established TLS session,
             * then waits for connection acknowledgment (CONNACK) packet. */
            returnStatus = establishMqttSession( pMqttContext, createCleanSession, pBrokerSessionPresent );

            if( returnStatus == EXIT_FAILURE )
            {
                /* End TLS session, then close TCP connection. */
                cleanupESPSecureMgrCerts( pNetworkContext );
                ( void ) xTlsDisconnect( pNetworkContext );
            }
        }

        if( returnStatus == EXIT_FAILURE )
        {
            /* Generate a random number and get back-off value (in milliseconds) for the next connection retry. */
            backoffAlgStatus = BackoffAlgorithm_GetNextBackoff( &reconnectParams, generateRandomNumber(), &nextRetryBackOff );

            if( backoffAlgStatus == BackoffAlgorithmRetriesExhausted )
            {
                LogError( ( "Connection to the broker failed, all attempts exhausted." ) );
                returnStatus = EXIT_FAILURE;
            }
            else if( backoffAlgStatus == BackoffAlgorithmSuccess )
            {
                LogWarn( ( "Connection to the broker failed. Retrying connection "
                           "after %hu ms backoff.",
                           ( unsigned short ) nextRetryBackOff ) );
                Clock_SleepMs( nextRetryBackOff );
            }
        }
    } while( ( returnStatus == EXIT_FAILURE ) && ( backoffAlgStatus == BackoffAlgorithmSuccess ) );

    return returnStatus;
}

/*-----------------------------------------------------------*/

static void cleanupOutgoingPublishes( void )
{
    assert( outgoingPublishPackets != NULL );

    /* Clean up all the outgoing publish packets. */
    ( void ) memset( outgoingPublishPackets, 0x00, sizeof( outgoingPublishPackets ) );
}
/*-----------------------------------------------------------*/

static int initializeMqtt( MQTTContext_t * pMqttContext,
                           NetworkContext_t * pNetworkContext )
{
    int returnStatus = EXIT_SUCCESS;
    MQTTStatus_t mqttStatus;
    MQTTFixedBuffer_t networkBuffer;
    TransportInterface_t transport = { 0 };

    assert( pMqttContext != NULL );
    assert( pNetworkContext != NULL );

    /* Fill in TransportInterface send and receive function pointers.
     * For this demo, TCP sockets are used to send and receive data
     * from network. Network context is SSL context for OpenSSL.*/
    transport.pNetworkContext = pNetworkContext;
    transport.send = espTlsTransportSend;
    transport.recv = espTlsTransportRecv;
    transport.writev = NULL;

    /* Fill the values for network buffer. */
    networkBuffer.pBuffer = buffer;
    networkBuffer.size = NETWORK_BUFFER_SIZE;

    /* Initialize MQTT library. */
    mqttStatus = MQTT_Init( pMqttContext,
                            &transport,
                            Clock_GetTimeMs,
                            eventCallback,
                            &networkBuffer );

    if( mqttStatus != MQTTSuccess )
    {
        returnStatus = EXIT_FAILURE;
        LogError( ( "MQTT_Init failed: Status = %s.", MQTT_Status_strerror( mqttStatus ) ) );
    }
    else
    {
        mqttStatus = MQTT_InitStatefulQoS( pMqttContext,
                                           pOutgoingPublishRecords,
                                           OUTGOING_PUBLISH_RECORD_LEN,
                                           pIncomingPublishRecords,
                                           INCOMING_PUBLISH_RECORD_LEN );

        if( mqttStatus != MQTTSuccess )
        {
            returnStatus = EXIT_FAILURE;
            LogError( ( "MQTT_InitStatefulQoS failed: Status = %s.", MQTT_Status_strerror( mqttStatus ) ) );
        }
    }

    return returnStatus;
}


int start_mqtt_connection(int argc, char ** argv) {
    MQTTContext_t mqttContext = { 0 };
    NetworkContext_t xNetworkContext = { 0 }; // TODO: Make it global??  to close conection later ??
    bool clientSessionPresent = false, brokerSessionPresent = false;

    /* Initialize MQTT library. Initialization of the MQTT library needs to be
     * done only once in this demo. */
    returnStatus = initializeMqtt( &mqttContext, &xNetworkContext );

    if( returnStatus == EXIT_SUCCESS )
    {
        for( ; ; )
        {
            /* Attempt to connect to the MQTT broker. If connection fails, retry after
             * a timeout. Timeout value will be exponentially increased till the maximum
             * attempts are reached or maximum timeout value is reached. The function
             * returns EXIT_FAILURE if the TCP connection cannot be established to
             * broker after configured number of attempts. */
            returnStatus = connectToServerWithBackoffRetries( &xNetworkContext, &mqttContext, &clientSessionPresent, &brokerSessionPresent );

            if( returnStatus == EXIT_FAILURE )
            {
                /* Log error to indicate connection failure after all
                 * reconnect attempts are over. */
                LogError( ( "Failed to connect to MQTT broker %.*s.",
                            AWS_IOT_ENDPOINT_LENGTH,
                            AWS_IOT_ENDPOINT ) );
            }
            else
            {
                /* Update the flag to indicate that an MQTT client session is saved.
                 * Once this flag is set, MQTT connect in the following iterations of
                 * this demo will be attempted without requesting for a clean session. */
                clientSessionPresent = true;

                /* Check if session is present and if there are any outgoing publishes
                 * that need to resend. This is only valid if the broker is
                 * re-establishing a session which was already present. */
                if( brokerSessionPresent == true )
                {
                    LogInfo( ( "An MQTT session with broker is re-established. "
                               "Resending unacked publishes." ) );

                    /* Handle all the resend of publish messages. */ // TODO: CHECK THIS
                    //returnStatus = handlePublishResend( &mqttContext );
                }
                else
                {
                    LogInfo( ( "A clean MQTT connection is established."
                               " Cleaning up all the stored outgoing publishes.\n\n" ) );

                    /* Clean up the outgoing publishes waiting for ack as this new
                     * connection doesn't re-establish an existing session. */
                    cleanupOutgoingPublishes();
                }

                /* End TLS session, then close TCP connection. */
                // cleanupESPSecureMgrCerts( &xNetworkContext ); // TODO: Dont close
                // ( void ) xTlsDisconnect( &xNetworkContext ); // TODO: Dont close
            }

            // if( returnStatus == EXIT_SUCCESS )
            // {
            //     /* Log message indicating an iteration completed successfully. */
            //     LogInfo( ( "Demo completed successfully." ) );
            // }

            LogInfo( ( "Short delay before starting the next iteration....\n" ) );
            sleep( MQTT_SUBPUB_LOOP_DELAY_SECONDS );
        }
    }
    return returnStatus;
}