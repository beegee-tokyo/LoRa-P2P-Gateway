/**
 * @file main.cpp
 * @author Bernd Giesecke (bernd@giesecke.tk)
 * @brief P2P to HTTP POST gateway based on RAK11200, RAK13300 and WisBlock API V2
 * @version 0.1
 * @date 2024-08-17
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "main.h"

/** LoRaWAN packet */
WisCayenne g_solution_data(255);

/** Received package for parsing */
uint8_t rcvd_data[256];
/** Length of received package */
uint16_t rcvd_data_len = 0;

/** Send Fail counter **/
uint8_t send_fail = 0;

/** Set the device name, max length is 10 characters */
char g_ble_dev_name[10] = "RAK_GW";

/** Flag for RAK1906 sensor */
bool has_rak1906 = false;

/**
 * @brief Initial setup of the application (before LoRaWAN and BLE setup)
 *
 */
void setup_app(void)
{
	Serial.begin(115200);
	time_t serial_timeout = millis();
	// On nRF52840 the USB serial is not available immediately
	while (!Serial)
	{
		if ((millis() - serial_timeout) < 5000)
		{
			delay(100);
			digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
		}
		else
		{
			break;
		}
	}
	digitalWrite(LED_GREEN, LOW);

	// Set firmware version
	api_set_version(SW_VERSION_1, SW_VERSION_2, SW_VERSION_3);

	/************************************************************/
	/** This code works only in LoRa P2P mode.                  */
	/** Forcing here the usage of LoRa P2P, independant of      */
	/** saved settings.                                         */
	/************************************************************/
	// Read LoRaWAN settings from flash
	api_read_credentials();
	// Force LoRa P2P
	g_lorawan_settings.lorawan_enable = false;
	// Check if DevEUI was setup before or if it is "default" 0x56, 0x4D, 0xC1, 0xF3
	if ((g_lorawan_settings.node_device_eui[4] == 0x56) &&
		(g_lorawan_settings.node_device_eui[5] == 0x4D) &&
		(g_lorawan_settings.node_device_eui[6] == 0xC1) &&
		(g_lorawan_settings.node_device_eui[7] == 0xF3))
	{
		MYLOG("SETUP", "No valid DevEUI found, use BLE-MAC");

		// Variable to store the MAC address
		uint8_t baseMac[6];

		// Get the MAC address of the Bluetooth interface
		esp_read_mac(baseMac, ESP_MAC_BT);

		MYLOG("SETUP", "Device ID from BLE MAC %02X%02X%02X%02X", baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
		g_lorawan_settings.node_device_eui[0] = 0xAC;
		g_lorawan_settings.node_device_eui[1] = 0x1F;
		g_lorawan_settings.node_device_eui[2] = baseMac[0];
		g_lorawan_settings.node_device_eui[3] = baseMac[1];
		g_lorawan_settings.node_device_eui[4] = baseMac[2];
		g_lorawan_settings.node_device_eui[5] = baseMac[3];
		g_lorawan_settings.node_device_eui[6] = baseMac[4];
		g_lorawan_settings.node_device_eui[7] = baseMac[5];
	}
	else
	{
		MYLOG("SETUP", "Device ID from DEV EUI %02X%02X%02X%02X", g_lorawan_settings.node_device_eui[4], g_lorawan_settings.node_device_eui[5],
			  g_lorawan_settings.node_device_eui[6], g_lorawan_settings.node_device_eui[7]);
	}
	// Save LoRaWAN settings
	api_set_credentials();

	g_enable_ble = true;
}

/**
 * @brief Final setup of application  (after LoRaWAN and BLE setup)
 *
 * @return true
 * @return false
 */
bool init_app(void)
{
	MYLOG("APP", "init_app");

	uint32_t node_id_dec = g_lorawan_settings.node_device_eui[7];
	node_id_dec |= (uint32_t)g_lorawan_settings.node_device_eui[6] << 8;
	node_id_dec |= (uint32_t)g_lorawan_settings.node_device_eui[5] << 16;
	node_id_dec |= (uint32_t)g_lorawan_settings.node_device_eui[4] << 24;
	Serial.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
	Serial.println("WisBlock P2P HTTP POST gateway");
	Serial.printf("Device ID %02X%02X%02X%02X = %u\r\n", g_lorawan_settings.node_device_eui[4], g_lorawan_settings.node_device_eui[5],
				  g_lorawan_settings.node_device_eui[6], g_lorawan_settings.node_device_eui[7], node_id_dec);
	Serial.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

	// Initialize User AT commands \todo Add setup for WiFi, HTTP POST URL and node ID
	// init_user_at();

	has_rak1921 = init_rak1921();

	float batt = read_batt();
	for (int rd_lp = 0; rd_lp < 10; rd_lp++)
	{
		batt += read_batt();
		batt = batt / 2;
	}
	if (has_rak1921)
	{
		sprintf(line_str, "P2P GW B %.2fV", batt / 1000);
		rak1921_write_header(line_str);
	}
	else
	{
		MYLOG("APP", "No OLED found");
	}

	// Check if RAK1906 is available
	has_rak1906 = init_rak1906();
	if (has_rak1906)
	{
		AT_PRINTF("+EVT:RAK1906");
	}

	// Initialize WiFi connection
	setup_wifi();

	pinMode(WB_IO2, OUTPUT);
	digitalWrite(WB_IO2, LOW);

	// Enable P2P RX continuous
	g_lora_p2p_rx_mode = RX_MODE_RX;
	g_lora_p2p_rx_time = 0;
	g_rx_continuous = true;
	// Put Radio into continuous RX mode
	Radio.Rx(0);
	
	return true;
}

/**
 * @brief Handle events
 * 		Events can be
 * 		- timer (setup with AT+SENDINT=xxx)
 * 		- interrupt events
 * 		- wake-up signals from other tasks
 */
void app_event_handler(void)
{
	// Timer triggered event
	if ((g_task_event_type & STATUS) == STATUS)
	{
		g_task_event_type &= N_STATUS;
		MYLOG("APP", "Timer wakeup");

		if (g_lpwan_has_joined)
		{
			// Reset the packet
			g_solution_data.reset();

			// Get battery level
			float batt_level_f = read_batt();
			g_solution_data.addVoltage(LPP_CHANNEL_BATT, batt_level_f / 1000.0);

			// Read sensors and battery
			if (has_rak1906)
			{
				read_rak1906();
			}

			g_solution_data.addDevID(LPP_CHANNEL_DEVID, &g_lorawan_settings.node_device_eui[4]);

			uint8_t *packet = g_solution_data.getBuffer();
			MYLOG("APP", "Packet size %d Content:", g_solution_data.getSize());
#if MY_DEBUG > 0
			for (int idx = 0; idx < g_solution_data.getSize(); idx++)
			{
				Serial.printf("%02X ", packet[idx]);
			}
			Serial.println("");
#endif
			if (parse_send(packet, g_solution_data.getSize()))
			{
				MYLOG("APP", "GW POST sent");
				if (has_rak1921)
				{
					rak1921_add_line((char *)"GW POST sent");
				}
			}
			else
			{
				MYLOG("APP", "GW POST failed");
				if (has_rak1921)
				{
					rak1921_add_line((char *)"GW POST failed");
				}
			}

			// Reset the packet
			g_solution_data.reset();
		}
		else
		{
			MYLOG("APP", "Network not joined, skip sending");
		}
	}

	// Parse request event
	if ((g_task_event_type & PARSE) == PARSE)
	{
		g_task_event_type &= N_PARSE;

		if (parse_send(rcvd_data, rcvd_data_len))
		{
			MYLOG("APP", "Node POST sent");
			if (has_rak1921)
			{
				rak1921_add_line((char *)"Node POST sent");
			}
		}
		else
		{
			MYLOG("APP", "Node POST failed");
			if (has_rak1921)
			{
				rak1921_add_line((char *)"Node POST failed");
			}
		}
	}
}

/**
 * @brief Handle LoRa events
 *
 */
void lora_data_handler(void)
{
	// LoRa data handling
	if ((g_task_event_type & LORA_DATA) == LORA_DATA)
	{
		g_task_event_type &= N_LORA_DATA;
		MYLOG("APP", "Received package over LoRa");
		char log_buff[g_rx_data_len * 3] = {0};
		uint8_t log_idx = 0;
		for (int idx = 0; idx < g_rx_data_len; idx++)
		{
			sprintf(&log_buff[log_idx], "%02X ", g_rx_lora_data[idx]);
			log_idx += 3;
		}
		MYLOG("APP", "%s", log_buff);

#if MY_DEBUG > 0
		CayenneLPP lpp(g_rx_data_len - 8);
		memcpy(lpp.getBuffer(), &g_rx_lora_data[8], g_rx_data_len - 8);
		DynamicJsonDocument jsonBuffer(4096);
		JsonObject root = jsonBuffer.to<JsonObject>();
		lpp.decodeTTN(lpp.getBuffer(), g_rx_data_len - 8, root);
		serializeJsonPretty(root, Serial);
		Serial.println();
#endif
		memcpy(rcvd_data, g_rx_lora_data, g_rx_data_len);
		rcvd_data_len = g_rx_data_len;
		api_wake_loop(PARSE);
	}
}
