/**
 * @file wifi_post.cpp
 * @author Bernd Giesecke (bernd@giesecke.tk)
 * @brief WiFi and HTTP POST functions
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
#include <HTTPClient.h>

/** Multi WiFi */
extern WiFiMulti wifi_multi;

/** WiFi Client */
WiFiClient client;
/** HTTP client */
HTTPClient http;

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

// Replace it with your HTTP POST API IP address or domain
const char *post_server = "http://YOUR_SERVER_URL";

/**
 * @brief Setup WiFi connections
 *
 */
void setup_wifi(void)
{
	delay(10);
	// We start by connecting to a WiFi network
	MYLOG("WiFi", "Connecting to %s or %s", ssid_prim, ssid_sec);

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
}

/**
 * @brief Check WiFi connection
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
}

/**
 * @brief Post the payload to HTTP POST API
 *
 * @param payload char array with the payload (we use JSON here)
 * @param len length of the payload
 * @return true Post successful
 * @return false Post failed (WiFi connection or URL problem)
 */
bool post_request(char *payload, size_t len)
{
	// Start HTTP client
	http.begin(client, post_server);

	// Specify content-type header
	http.addHeader("Content-Type", "application/json");

	// Send HTTP POST request
	int httpResponseCode = http.POST((uint8_t*) payload, len);

	http.end();

	if ((httpResponseCode != 200))
	{
		MYLOG("POST", "Response %d", httpResponseCode);
		return false;
	}
	return true;
}
