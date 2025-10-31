#ifndef __AWS_IOT_TASK_H__
#define __AWS_IOT_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Initialize and start the AWS IoT task */
void aws_iot_task_init(void);

/** @brief Request a connection to the configured AWS IoT broker */
void aws_iot_connect(void);

/** @brief Start listening for incoming MQTT messages from AWS IoT */
void aws_iot_start_listening(void);

#ifdef __cplusplus
}
#endif

#endif // __AWS_IOT_TASK_H__