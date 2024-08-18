/**
 * @file main.h
 * @author Bernd Giesecke (bernd@giesecke.tk)
 * @brief Includes, defines and globals
 * @version 0.1
 * @date 2024-08-17
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef _MAIN_H_
#define _MAIN_H_

#include <Arduino.h>
#include <WisBlock-API-V2.h>
#include "RAK1906_env.h"

// Debug output set to 0 to disable app debug output
#ifndef MY_DEBUG
#define MY_DEBUG 1
#endif

#if MY_DEBUG > 0
#define MYLOG(tag, ...)                                                 \
	if (tag)                                                            \
		Serial.printf("[%s] ", tag);                                    \
	Serial.printf(__VA_ARGS__);                                         \
	Serial.printf("\n");                                                \
	if (g_ble_uart_is_connected)                                        \
	{                                                                   \
		char buff[255];                                                 \
		int len = sprintf(buff, __VA_ARGS__);                           \
		uart_tx_characteristic->setValue((uint8_t *)buff, (size_t)len); \
		uart_tx_characteristic->notify(true);                           \
		delay(50);                                                      \
	}
#else
#define MYLOG(...)
#endif

/** Define the version of your SW */
#ifndef SW_VERSION_1
#define SW_VERSION_1 1 // major version increase on API change / not backwards compatible
#endif
#ifndef SW_VERSION_2
#define SW_VERSION_2 0 // minor version increase on API change / backward compatible
#endif
#ifndef SW_VERSION_3
#define SW_VERSION_3 0 // patch version increase on bugfix, no affect on API
#endif

/** Application function definitions */
void setup_app(void);
bool init_app(void);
void app_event_handler(void);
void ble_data_handler(void) __attribute__((weak));
void lora_data_handler(void);

// Wakeup flags
#define PARSE 0b1000000000000000
#define N_PARSE 0b0111111111111111

// Cayenne LPP Channel numbers per sensor value
#define LPP_CHANNEL_BATT 1 // Base Board

// Globals
extern WisCayenne g_solution_data;

// WiFi and POST stuff
void setup_wifi(void);
void reconnect_wifi(void);
bool post_request(char *payload, size_t len);

// Parser
bool parse_send(uint8_t *data, uint16_t data_len);

// OLED
#include <nRF_SSD1306Wire.h>
bool init_rak1921(void);
void rak1921_add_line(char *line);
void rak1921_show(void);
void rak1921_write_header(char *header_line);
void rak1921_clear(void);
void rak1921_write_line(int16_t line, int16_t y_pos, String text);
void rak1921_display(void);
extern char line_str[];
extern bool has_rak1921;

#endif // _MAIN_H_