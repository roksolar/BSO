#include "string.h"
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "i2c.h"

#define PCF_ADDRESS	0x38
#define BUS_I2C		0
#define SCL 		14
#define SDA 		12

#define button1		0x20	// 0b ??0? ????
#define led1 		0xfe	// 0b ???? ???0

#define gpio_tx 	4		// D2 pin
#define gpio_rx 	5		// D1 pin

#define time_interval	500
