/**
 * @file wifi_mqtt.cpp
 * @author Bernd Giesecke (bernd@giesecke.tk)
 * @brief WiFi and MQTT functions
 * @version 0.1
 * @date 2024-08-17
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "main.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <esp_wifi.h>
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient/archive/master.zip

/** Multi WiFi */
extern WiFiMulti wifi_multi;

//* ********************************************************* */
//* Requires WiFi credentials setup through WisBlock Toolbox  */
//* or through AT commands                                    */
//* Until AT commands implemented, set them here manually     */
//* ********************************************************* */
/** Primary SSID of local WiFi network */
String ssid_prim = "YOUR WIFI AP #1";
/** Secondary SSID of local WiFi network */
String ssid_sec = "YOUR WIFI AP #2";
/** Password for primary local WiFi network */
String pw_prim = "YOUR WIFI AP #1 PASSWORD";
/** Password for secondary local WiFi network */
String pw_sec = "YOUR WIFI AP #2 PASSWORD";

// Replace it with your MQTT Broker IP address or domain
const char *mqtt_server = "YOUR BROKER URL/IP ADDRESS";

// Define an ID to indicate the device,
// If it is the same as other devices which connect the same mqtt server,
// it will lead to the failure to connect to the mqtt server
const char *mqttClientId = "esp32";

// if need username and password to connect mqtt server, they cannot be NULL.
const char *mqttUsername = "YOUR BROKER USER NAME";
const char *mqttPassword = "YOUR BROKER PASSWORD";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

/**
 * @brief Setup WiFi and MQTT connections
 *
 */
void setup_wifi(void)
{
	delay(10);
	// We start by connecting to a WiFi network
	MYLOG("WiFi", "Connecting to %s or %s", ssid_prim, ssid_sec);

	// Setup mqtt broker
	mqttClient.setServer(mqtt_server, 1883);
	mqttClient.setBufferSize(1024);
	mqttClient.setKeepAlive(g_lorawan_settings.send_repeat_time / 1000 * 2);
	mqttClient.setSocketTimeout(g_lorawan_settings.send_repeat_time / 1000 * 2);

	//* ********************************************************* */
	//* Requires WiFi credentials setup through WisBlock Toolbox  */
	//* or through AT commands                                    */
	//* Until AT commands implemented, set them here manually     */
	//* ********************************************************* */
	Preferences preferences;
	preferences.begin("WiFiCred", false);
	preferences.putString("g_ssid_prim", ssid_prim);
	preferences.putString("g_ssid_sec", ssid_sec);
	preferences.putString("g_pw_prim", pw_prim);
	preferences.putString("g_pw_sec", pw_sec);
	preferences.putBool("valid", true);

	// Init Wifi with WisBlock-API-V2
	init_wifi();

	time_t start_wait = millis();
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		if ((millis() - start_wait) > 30000)
		{
			MYLOG("WiFi", "No connection in 30 seconds");
			break;
		}
	}

	if (WiFi.status() == WL_CONNECTED)
	{
		String ips = WiFi.localIP().toString();
		MYLOG("WiFi", "WiFi connected, IP address: %s", ips.c_str());

		// Connect to MQTT server
		if (mqttClient.connect(mqttClientId, mqttUsername, mqttPassword, "P2P_GW", 1, true, "Connected"))
		{
			MYLOG("MQTT", "MQTT connected");
		}
		else
		{
			MYLOG("MQTT", "MQTT failed code %d", mqttClient.state());
		}
	}
}

/**
 * @brief Check WiFi connection and re-connect to MQTT broker
 *
 */
void reconnect_wifi(void)
{
	if (WiFi.status() != WL_CONNECTED)
	{
		// To be checked, the WiFi functions in the WisBlock-API-V2 should automatically try to reconnect
		// WiFi.begin(ssid, password);

		// Try to just run wifi_multi.run();
		wifi_multi.run();

		time_t start_wait = millis();
		while (WiFi.status() != WL_CONNECTED)
		{
			delay(500);
			if ((millis() - start_wait) > 30000)
			{
				MYLOG("WiFi", "No connection in 30 seconds");
				break;
			}
		}
	}

	if ((WiFi.status() == WL_CONNECTED) && !mqttClient.connected())
	{
		// Connect to MQTT server
		if (mqttClient.connect(mqttClientId, mqttUsername, mqttPassword, "P2P_GW", 1, true, "Connected"))
		{
			MYLOG("MQTT", "MQTT connected");
		}
		else
		{
			MYLOG("MQTT", "MQTT failed code %d", mqttClient.state());
		}
	}
}

/**
 * @brief Publish a topic to the MQTT broker
 *
 * @param topic char array with the topic
 * @param payload char array with the payload (we use JSON here)
 * @return true Publish successful
 * @return false Publish failed (MQTT or WiFi connection problem)
 */
bool publish_mqtt(char *topic, char *payload)
{
	check_mqtt();
	if ((WiFi.status() != WL_CONNECTED) || !mqttClient.connected())
	{
		MYLOG("MQTT", "No connection");
		reconnect_wifi();
	}
	if ((WiFi.status() == WL_CONNECTED) && mqttClient.connected())
	{
		MYLOG("MQTT", "Try to send");
		if (mqttClient.publish(topic, payload))
		{
			MYLOG("MQTT", "Publish returned OK");
			return true;
		}
		else
		{
			MYLOG("MQTT", "Publish returned FAIL");
			return false;
		}
	}
	return false;
}

/**
 * @brief Check connection to MQTT broker
 * 		If disconnected, try to reconnect
 * 		Run the mqttClient loop should not be required
 * 		as we do not subscribe to anything
 *
 */
void check_mqtt(void)
{
	reconnect_wifi();
	mqttClient.loop();
}