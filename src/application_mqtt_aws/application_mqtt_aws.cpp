#include "application_mqtt_aws.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
//#include <unistd.h> // for linux
#include <io.h> // for win
#include <limits.h>
#include <string.h>
#include <cstddef> // for std::nullptr_t

#include "mavlink.h"
#include <QUdpSocket>
/**
 * @brief This parameter will avoid infinite loop of publish and exit the program after certain number of publishes
 */
//static uint32_t publishCount = 0;
// TODO: path in format according to platform
static string rootCAPath = "../../qgroundcontrol/src/aws-iot-device-sdk-embedded-C/certs/RootCA1.pem";
static string clientCRTPath = "../../qgroundcontrol/src/aws-iot-device-sdk-embedded-C/certs/ThingCertificate.pem.crt";
static string clientKeyPath = "../../qgroundcontrol/src/aws-iot-device-sdk-embedded-C/certs/PrivateKey.pem.key";

static void aws_mqtt_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName,
                                                 uint16_t topicNameLen, IoT_Publish_Message_Params *params,
                                                 void *pData)         /* New message callback handler */
{
    IOT_UNUSED(pData);
    IOT_UNUSED(pClient);
    IOT_UNUSED(topicName);
    IOT_UNUSED(topicNameLen);
#ifdef AWS_DEBUG

    // Mavlink data
    mavlink_status_t status;
    mavlink_message_t r_message; // message/packet object
    int l_mqttRecLen = 0;
    char *pMQTTBuf = nullptr;// TODO: to use nullptr in this IDE?;
    uint8_t temp;

    l_mqttRecLen = static_cast<int>(params->payloadLen);
    pMQTTBuf = static_cast<char *>(params->payload);

    if(l_mqttRecLen > 0) {
            printf("Bytes Received: %d\nDatagram: ", static_cast<int>(l_mqttRecLen));
            for(int i = 0; i < l_mqttRecLen; ++i) {
              temp = pMQTTBuf[i];
              printf("%02x", static_cast<unsigned char>(temp));
              if(mavlink_parse_char(MAVLINK_COMM_0, pMQTTBuf[i], &r_message, &status)) {
                printf("\nReceived mavlink message with ID %d, sequence: %d from component %d of system %d\n", r_message.msgid, r_message.seq, r_message.compid, r_message.sysid);
                // To get the fields of the specific message in the packet you need to further decode the payload.
                // it can be performed from ID
                //MQTTProcessMessage(r_message);
              }
    }
    pMQTTBuf = nullptr;
    Sleep(1000);
    }
#endif
}


application_mqtt_aws::application_mqtt_aws(string host_url, unsigned int port, string root_ca_path, string client_crt_path,
                                           string client_key_path, iot_disconnect_handler disconnect_callback, bool auto_reconnect)
{
    this->host_url = host_url;
    this->port = port;
    this->root_ca = root_ca_path;
    this->client_crt = client_crt_path;
    this->client_key = client_key_path;
    this->auto_reconnect = auto_reconnect;
    this->mqttInitParams.disconnectHandler = disconnect_callback;


    mqttInitParams.enableAutoReconnect = this->auto_reconnect;
    mqttInitParams.pHostURL = (char*) (this->host_url).c_str();
    mqttInitParams.port = this->port;
    mqttInitParams.pRootCALocation = (char*) (this->root_ca).c_str();
    mqttInitParams.pDeviceCertLocation = (char*) (this->client_crt).c_str();
    mqttInitParams.pDevicePrivateKeyLocation = (char*) (this->client_key).c_str();
    mqttInitParams.mqttCommandTimeout_ms = 20000;
    mqttInitParams.tlsHandshakeTimeout_ms = 5000;
    mqttInitParams.isSSLHostnameVerify = true;
    mqttInitParams.disconnectHandler = disconnect_callback;
    mqttInitParams.disconnectHandlerData = nullptr;


#ifdef AWS_DEBUG
    char* messages_info =  new char[4096];
    sprintf(messages_info, "\r\nAWS IoT SDK Version %d.%d.%d-%s\r\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);
    cout << messages_info << endl;
    sprintf(messages_info, "Host url: %s, port: %d, auto reconnect = %d", this->mqttInitParams.pHostURL, this->port, this->auto_reconnect);
    cout << messages_info << endl;
    cout << "RootCA location: " << this->root_ca << endl;
    cout << "Client certificate location: " << this->client_crt << endl;
    cout << "Client key location: " << this->client_key << endl;
    cout << "mqttCommandTimeout_ms: " << 20000 << endl;
    cout << "tlsHandshakeTimeout_ms: " << 5000 << endl;
    cout << "isSSLHostnameVerify: true" << endl;
#endif
    this->aws_mqtt_init();
    delete[] messages_info;
}

void application_mqtt_aws::aws_mqtt_init()
{
    IoT_Error_t rc = FAILURE;
    rc = aws_iot_mqtt_init(& this->aws_client, &this->mqttInitParams);
    if(SUCCESS != rc) {
#ifdef AWS_DEBUG
        cout << "aws init error" << endl;
        return;
#endif
    }
    else {
#ifdef AWS_DEBUG
        cout << "aws init success" << endl;
#endif
        return;
    }
}

/* Connect to server */
bool application_mqtt_aws::aws_mqtt_connect(int keep_alive_second, string mqtt_client_id)
{
    this->aws_mqtt_client_id = mqtt_client_id;
    this->keepAliveIntervalInSec = keep_alive_second;
    this->connectParams.keepAliveIntervalInSec = this->keepAliveIntervalInSec;
    this->connectParams.isCleanSession = true;
    this->connectParams.MQTTVersion = MQTT_3_1_1;
    this->connectParams.pClientID = (char *)(this->aws_mqtt_client_id).c_str();
    this->connectParams.clientIDLen = (uint16_t) (this->aws_mqtt_client_id.length());
    this->connectParams.isWillMsgPresent = false;

#ifdef AWS_DEBUG
       cout << "Connecting..." << endl;
#endif

    IoT_Error_t rc = FAILURE;
    rc = aws_iot_mqtt_connect(&this->aws_client, &this->connectParams);
    if(SUCCESS != rc)
    {
        char* error = new char[1024];
        sprintf(error, "Error (%d) connecting to %s:%d", rc, mqttInitParams.pHostURL, mqttInitParams.port);
        cout << error << endl;
        delete[] error;
        return false;
    }
#ifdef AWS_DEBUG
    char* error = new char[1024];
    sprintf(error, "Connected to %s:%d", mqttInitParams.pHostURL, mqttInitParams.port);
    cout << error << endl;
    delete[] error;
#endif
    return true;
}

/* Subscribe topic */
bool application_mqtt_aws::aws_mqtt_subscribe(string topic, QoS qos)
{
    this->subscribe_topic =  topic;
#ifdef AWS_DEBUG
    cout << "Subscribing topic: " << topic << endl;
#endif
    IoT_Error_t rc = FAILURE;
    rc = aws_iot_mqtt_subscribe(&this->aws_client, (const char*)topic.c_str(), (uint16_t)this->subscribe_topic.length(),
                                qos, aws_mqtt_subscribe_callback_handler, nullptr);
    if(SUCCESS != rc)
    {
         char error[64];
         sprintf(error, "%d", rc);
         cout << "Error subscribing, return error code" << rc << endl;
         return false;
    }
    else  cout << "Subscribing return success" << endl;
    return true;
}

bool application_mqtt_aws::aws_mqtt_publish(string topic, string payload, QoS qos)
{

#ifdef AWS_DEBUG
    cout << "Publishing" << endl;
#endif
    IoT_Error_t rc;
    this->publish_topic = topic;
    this->paramsQOSx.qos = qos;
    this->paramsQOSx.payload = (void*) payload.c_str();
    this->paramsQOSx.payloadLen = payload.length();
    this->paramsQOSx.isRetained = 0;
    rc = aws_iot_mqtt_publish(&this->aws_client, (const char*) topic.c_str(), topic.length(), &this->paramsQOSx);
    if(SUCCESS != rc)
    {
#ifdef AWS_DEBUG
        cout << "Publish message error to topic " << topic << endl;
#endif
        return false;
    }
    else
    {
        cout << "Publish message to topic " << topic.c_str() << " successful" << endl;
        return true;
    }
}

/* Auto reconnect funtion */
void application_mqtt_aws::aws_mqtt_autoreconnect_enable()
{
    IoT_Error_t rc;
    rc = aws_iot_mqtt_autoreconnect_set_status(&this->aws_client, true);
    if(SUCCESS != rc)
    {
#ifdef AWS_DEBUG
    char error[128];
    sprintf(error, "Unable to set Auto Reconnect to true - %d", rc);
    cout << error << endl;
#endif
        return;
    }
    else
    {
#ifdef AWS_DEBUG
    cout << "Set auto reconnect successful with AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL = " << AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL;
    cout << ", AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL = " << AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL << endl;
    cout << "AWS_IOT_MIX[MAX]_RECONNECT_WAIT_INTERVAL define in file aws_iot_config.h" << endl;
#endif
    }
}

/* Disconnect callback handler */

void application_mqtt_aws::iot_disconnect_callback_handler(AWS_IoT_Client *pClient, void *data)
{
    cout << "MQTT Disconnect" << endl;
    IoT_Error_t rc = FAILURE;

    if(nullptr == pClient)
    {
        cout << "NULL pClient" << endl;
        return;
    }

    IOT_UNUSED(data);

    if(aws_iot_is_autoreconnect_enabled(pClient))
    {
        cout << "Auto Reconnect is enabled, Reconnecting attempt will start now";
    }
    else
    {
        cout << "Auto Reconnect not enabled. Starting manual reconnect..." << endl;
        rc = aws_iot_mqtt_attempt_reconnect(pClient);
        if(NETWORK_RECONNECTED == rc)
        {
            cout << "Manual Reconnect Successful" << endl;
        }
        else
        {
            char error[128];
            sprintf(error, "Manual Reconnect Failed - %d", rc);
            cout << error << endl;
        }
    }
}

/* Test AWS IoT in GUI */
bool AWSIoTTest()
{
    application_mqtt_aws connect(HostAddress, 8883, rootCAPath, clientCRTPath,
                                     clientKeyPath, nullptr, true);
    bool AWSSuccess = 0;

    AWSSuccess = connect.aws_mqtt_connect(600, AWS_IOT_MQTT_CLIENT_ID);


    AWSSuccess = connect.aws_mqtt_subscribe("DJI/pub", QOS0);
    connect.aws_mqtt_autoreconnect_enable();
    AWSSuccess = connect.aws_mqtt_publish("DJI/sub", "Client successfully connected to AWS server", QOS0);
    if(AWSSuccess) {
        return true;
    }
    return false;
}

