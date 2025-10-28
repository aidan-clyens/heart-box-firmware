#include "aws_iot.h"

#include "core_mqtt.h"
#include "core_mqtt_state.h"
#include "network_transport.h"
#include "backoff_algorithm.h"

#include "esp_log.h"

static const char *TAG_MQTT_CLIENT = "AWS IoT";

void aws_iot_connect()
{
  int rc = EXIT_SUCCESS;

  MQTTContext_t mqtt_context = {0};
  NetworkContext_t network_context = {0};

  rc = aws_iot_init(mqttContext, networkContext) if (rc != EXIT_SUCCESS)
  {
    ESP_LOGE(TAG_MQTT_CLIENT, "Failed to connect to MQTT broker");
    return;
  }

  // connectToServerWithBackoffRetries(networkContext, mqttContext, clientSessionPresent)

  // subscribePublishLoop()

  // End TLS session and close TCP connection
}

void aws_iot_disconnect()
{
  
}

static int aws_iot_init(MQTTContext_t *p_mqtt_context, NetworkContext_t *p_network_context)
{
  TransportInterface_t transport = {0};

  transport.pNetworkContext = p_network_context;
}
