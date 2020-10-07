/*
 Name:		testESP.ino
 Created:	10/2/2020 6:52:51 PM
 Author:	Felipe Villenas
*/

#include <math.h>
#include <esp_now.h>
#include <esp_wifi_types.h>
#include <esp_wifi_internal.h>
#include <esp_wifi.h>
#include <WiFi.h>

#define TOTAL_PACKETS	500
#define	T_MS			20
#define PAYLOAD_SIZE	8
#define CHANNEL			1
#define DATA_RATE		WIFI_PHY_RATE_6M
#define CUSTOM_WIFI_CFG true
#define SET_ACTION(action, name) if(action == ESP_OK) { Serial.println(String(name) + " OK!"); } else{ Serial.println("Error with: "+String(name)); }

struct ESPNOW_payload {
	uint32_t timestamp;
	float data;
} packet;

uint8_t mac_broadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

uint32_t t0, t1;
int n = 0;
uint8_t k = 0;
int N_packets = 0;

esp_now_peer_info_t peer_info;

void setup_custom_wifi();
void setup_espnow();
void add_peer(uint8_t* mac, int channel, bool encrypt);
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) { /* nothing for now */ };
void OnDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) { /* nothing for now */ };
float square_wave(int);
float exp_cos(int);

// the setup function runs once when you press reset or power the board
void setup() {

	Serial.begin(115200);
	WiFi.mode(WIFI_STA);

	if (CUSTOM_WIFI_CFG) {
		WiFi.disconnect();
		setup_custom_wifi();
		//esp_wifi_set_ps(WIFI_PS_NONE); power saving mode none
	}

	setup_espnow();
	add_peer(mac_broadcast, CHANNEL, false);

	packet.timestamp = 0;
	packet.data = 0.f;

	Serial.println("Setup Completado!");
}

// the loop function runs over and over again until power down or reset
void loop() {

	if (N_packets < TOTAL_PACKETS) {

		t1 = millis();
		if (t1 - t0 >= T_MS) {
			packet.timestamp = millis();
			//float h = square_wave(n);
			float h = exp_cos(n);
			packet.data = h;
			esp_now_send(peer_info.peer_addr, (uint8_t*)&packet, sizeof(packet));
			n++;
			N_packets++;
			t0 = millis();
		}

	}
}

float square_wave(int n) {

	float h[] = { 1.0 , -1.0 };
	if (n % 10 == 0) {
		k++;
		k = k & 0x01;
	}

	return h[k];
}

float exp_cos(int n) {

	float h = 1 - (float)exp(-0.016 * n) * (float)cos(0.1 * n);
	return h;
}
//////////////////////////////////////////////////////////////////////////

/* Custom WiFi Settings for 'better' ESPNOW */
void setup_custom_wifi() {

	SET_ACTION(esp_wifi_stop(), "Stop WiFi");
	SET_ACTION(esp_wifi_deinit(), "De-init WiFi");

	wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
	wifi_cfg.ampdu_tx_enable = 0;
	SET_ACTION(esp_wifi_init(&wifi_cfg), "Setting custom config");
	SET_ACTION(esp_wifi_start(), "Starting WiFi");
	SET_ACTION(esp_wifi_set_promiscuous(true), "Setting promiscuous mode");
	SET_ACTION(esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE), "Setting channel");
	SET_ACTION(esp_wifi_internal_set_fix_rate(ESP_IF_WIFI_STA, true, DATA_RATE), "Setting datarate");

}

void setup_espnow() {

	SET_ACTION(esp_now_init(), "Initializing ESPNOW");

	esp_now_register_send_cb(OnDataSent);
	esp_now_register_recv_cb(OnDataRecv);
}

void add_peer(uint8_t* mac, int channel, bool encrypt) {

	memcpy(peer_info.peer_addr, mac, 6);
	peer_info.channel = channel;
	peer_info.encrypt = encrypt;

	if (esp_now_add_peer(&peer_info) != ESP_OK)
		Serial.println("Failed to add peer");
	else
		Serial.println("Peer added successfully");
}
