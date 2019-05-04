#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <i2c/i2c.h>
#include <bmp280/bmp280.h>
#include <string.h>
#define button1		0x20
#define PCF_ADDRESS	0x38
#define button2		0x10
#include <FreeRTOS.h>
#include <task.h>
#include <ssid_config.h>


#include <espressif/esp_sta.h>
#include <espressif/esp_wifi.h>

#include <paho_mqtt_c/MQTTESP8266.h>
#include <paho_mqtt_c/MQTTClient.h>

#include <semphr.h>

#include <stdio.h>

#include <espressif/esp_common.h>
#include <esp/uart.h>

#include <FreeRTOS.h>
#include <task.h>

#define MQTT_HOST ("192.168.1.2")
#define MQTT_PORT 1883

#define MQTT_USER "test"
#define MQTT_PASS "bso"

static TaskHandle_t task1_handle;
const uint8_t i2c_bus = 0;
const uint8_t scl_pin = 14;
const uint8_t sda_pin = 12;

SemaphoreHandle_t wifi_alive;
QueueHandle_t publish_queue;
#define PUB_MSG_LEN 16

bmp280_t bmp280_dev;

bool i2c_init_bmp(){
	return(i2c_init(i2c_bus, scl_pin, sda_pin, I2C_FREQ_400K));
}
bool i2c_init_button(){
	return (i2c_init(i2c_bus, scl_pin, sda_pin, I2C_FREQ_100K));
}

static bool init_bmp(){
	
	while(!i2c_init_bmp()){}
	bmp280_params_t params;
    	bmp280_init_default_params(&params);
	bmp280_dev.i2c_dev.bus = i2c_bus;
	bmp280_dev.i2c_dev.addr = BMP280_I2C_ADDRESS_0;
	return (bmp280_init(&bmp280_dev, &params));
}

static float get_pressure()
{
	float pressure, temperature, humidity;
	if (!bmp280_read_float(&bmp280_dev, &temperature, &pressure, &humidity)) {
		printf("Temperature reading failed\n");
		return -1.0;
	}
	return pressure;
}




static void  beat_task(void *pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();
	char msg[PUB_MSG_LEN];

	float pressure;

	int index = 0;
	float sum = 0;
	while (1) {
		vTaskDelayUntil(&xLastWakeTime, 1000 / portTICK_PERIOD_MS);
		pressure = get_pressure(bmp280_dev);

		if (pressure == -1.0){
			break;
		}
		sum = sum + pressure;
		index = index + 1;
		if(index == 10){
			printf("Average pressure: %.2f Pa  \n\r",(sum/index));
			snprintf(msg, PUB_MSG_LEN, "%f\r\n\0", (sum/index));
			if (xQueueSend(publish_queue, (void *)msg, 0) == pdFALSE) {
				printf("Publish queue overflow.\r\n");
			}
			  
			sum = 0;
			index = 0;   
		}
	}
}

static const char *  get_my_id(void)
{
    // Use MAC address for Station as unique ID
    static char my_id[13];
    static bool my_id_done = false;
    int8_t i;
    uint8_t x;
    if (my_id_done)
        return my_id;
    if (!sdk_wifi_get_macaddr(STATION_IF, (uint8_t *)my_id))
        return NULL;
    for (i = 5; i >= 0; --i)
    {
        x = my_id[i] & 0x0F;
        if (x > 9) x += 7;
        my_id[i * 2 + 1] = x + '0';
        x = my_id[i] >> 4;
        if (x > 9) x += 7;
        my_id[i * 2] = x + '0';
    }
    my_id[12] = '\0';
    my_id_done = true;
    return my_id;
}
// check for pressed button
void button(void *pvParameters) {
	uint8_t pcf_byte;
	char msg[PUB_MSG_LEN];
	float pressure;
	bool run = true;
	while (1) {

		i2c_slave_read(i2c_bus, PCF_ADDRESS, NULL, &pcf_byte, 1);
		
		if ((pcf_byte & button1) == 0){
			printf("Button 1 is pressed\n\r");
			pressure = get_pressure(bmp280_dev);
			snprintf(msg, PUB_MSG_LEN, "%f\r\n\0", pressure);
			if (xQueueSend(publish_queue, (void *)msg, 0) == pdFALSE) {
				printf("Publish queue overflow.\r\n");
			}
		
		}
		if ((pcf_byte & button2) == 0){
			if(run){
				printf("Button 2 is pressed.. pausing\n\r");	
				vTaskSuspend(task1_handle);
				run = false;
			}else{
				printf("Button 2 is pressed.. resuming\n\r");
				xQueueReset(publish_queue);
				vTaskResume(task1_handle);
				run = true;
			}
		
		}
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}



static void  mqtt_task(void *pvParameters)
{
    int ret         = 0;
    struct mqtt_network network;
    mqtt_client_t client   = mqtt_client_default;
    char mqtt_client_id[20];
    uint8_t mqtt_buf[100];
    uint8_t mqtt_readbuf[100];
    mqtt_packet_connect_data_t data = mqtt_packet_connect_data_initializer;

    mqtt_network_new( &network );
    memset(mqtt_client_id, 0, sizeof(mqtt_client_id));
    strcpy(mqtt_client_id, "ESP-");
    strcat(mqtt_client_id, get_my_id());

    while(1) {
        xSemaphoreTake(wifi_alive, portMAX_DELAY);
        printf("%s: started\n\r", __func__);
        printf("%s: (Re)connecting to MQTT server %s ... ",__func__,
               MQTT_HOST);
        ret = mqtt_network_connect(&network, MQTT_HOST, MQTT_PORT);
        if( ret ){
            printf("error: %d\n\r", ret);
            taskYIELD();
            continue;
        }
        printf("done\n\r");
        mqtt_client_new(&client, &network, 5000, mqtt_buf, 100,
                      mqtt_readbuf, 100);

        data.willFlag       = 0;
        data.MQTTVersion    = 3;
        data.clientID.cstring   = mqtt_client_id;
        data.username.cstring   = MQTT_USER;
        data.password.cstring   = MQTT_PASS;
        data.keepAliveInterval  = 10;
        data.cleansession   = 0;
        printf("Send MQTT connect ... ");
        ret = mqtt_connect(&client, &data);
        if(ret){
            printf("error: %d\n\r", ret);
            mqtt_network_disconnect(&network);
            taskYIELD();
            continue;
        }
        printf("done\r\n");
        xQueueReset(publish_queue);

        while(1){
            char msg[PUB_MSG_LEN - 1] = "\0";
	    // Wait for message
            while(xQueueReceive(publish_queue, (void *)msg, 0) ==
                  pdTRUE){
                printf("got message to publish\r\n");
                mqtt_message_t message;
                message.payload = msg;
                message.payloadlen = PUB_MSG_LEN;
                message.dup = 0;
                message.qos = MQTT_QOS1;
                message.retained = 0;
		// Publish message
                ret = mqtt_publish(&client, "/BSO-1", &message);
                if (ret != MQTT_SUCCESS ){
                    printf("error while publishing message: %d\n", ret );
                    break;
                }
            }

            ret = mqtt_yield(&client, 1000);
            if (ret == MQTT_DISCONNECTED)
                break;
        }
        printf("Connection dropped, request restart\n\r");
        mqtt_network_disconnect(&network);
        taskYIELD();
    }
}

static void  wifi_task(void *pvParameters)
{
    uint8_t status  = 0;
    uint8_t retries = 30;
    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    printf("WiFi: connecting to WiFi\n\r");
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

    while(1)
    {
        while ((status != STATION_GOT_IP) && (retries)){
            status = sdk_wifi_station_get_connect_status();
            printf("%s: status = %d\n\r", __func__, status );
            if( status == STATION_WRONG_PASSWORD ){
                printf("WiFi: wrong password\n\r");
                break;
            } else if( status == STATION_NO_AP_FOUND ) {
                printf("WiFi: AP not found\n\r");
                break;
            } else if( status == STATION_CONNECT_FAIL ) {
                printf("WiFi: connection failed\r\n");
                break;
            }
            vTaskDelay( 1000 / portTICK_PERIOD_MS );
            --retries;
        }
        if (status == STATION_GOT_IP) {
            printf("WiFi: Connected\n\r");
            xSemaphoreGive( wifi_alive );
            taskYIELD();
        }

        while ((status = sdk_wifi_station_get_connect_status()) == STATION_GOT_IP) {
            xSemaphoreGive( wifi_alive );
            taskYIELD();
        }
        printf("WiFi: disconnected\n\r");
        sdk_wifi_station_disconnect();
        vTaskDelay( 1000 / portTICK_PERIOD_MS );
    }
}

void user_init(void)
{

    while(!init_bmp()){
		printf("BMP280 initialization failed\n");
		vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    task1_handle = NULL;
    uart_set_baud(0, 115200);
    vSemaphoreCreateBinary(wifi_alive);
    publish_queue = xQueueCreate(3, PUB_MSG_LEN);
    xTaskCreate(&wifi_task, "wifi_task",  256, NULL, 2, NULL);
    xTaskCreate(beat_task, "beat_task", 1024, NULL, 3, &task1_handle);
    xTaskCreate(&mqtt_task, "mqtt_task", 1024, NULL, 4, NULL);
    xTaskCreate(&button, "button", 1024, NULL, 5, NULL);
}
