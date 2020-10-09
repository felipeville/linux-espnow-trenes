/*
 Name:		testESP.ino
 Created:	10/2/2020 6:52:51 PM
 Author:	Felipe Villenas
*/

#include <Ticker.h>
#include <math.h>
#include <esp_now.h>
#include <esp_wifi_types.h>
#include <esp_wifi_internal.h>
#include <esp_wifi.h>
#include <PubSubClient.h>
#include <WiFi.h>

#define PAYLOAD_SIZE	8
#define CHANNEL			1
#define DATA_RATE		WIFI_PHY_RATE_6M
#define CUSTOM_WIFI_CFG true
#define SET_ACTION(action, name) if(action == ESP_OK) { Serial.println(String(name) + " OK!"); } else{ Serial.println("Error with: "+String(name)); }

struct ESPNOW_payload {
	uint32_t timestamp;
	float data;
} packet;

static uint8_t mac_broadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static uint8_t ESP_IDs[] = { 0xcc, 0x0c, 0x99 };		// esto es para asignar una ID a las ESP basado en el ultimo byte de su MAC

uint8_t myMac[6];
uint32_t T_MS = 20;
uint32_t TOTAL_PACKETS = 500;
uint32_t t0, t1;
int esp_id;
int N_packets = 0;
int n = 0;		// variable de tiempo como argumento para las funciones matematicas

esp_now_peer_info_t peer_info;

/* MQTT Settings */
const char* ssid = "VTR-6351300";
const char* password = "zkd2bxhcHqHm";
const char* mqtt_server = "192.168.0.5";	// laptop local IPv4
char mqtt_packet[64];
String topic[] = { "ESP_command", "ESP_ts", "ESP_tpackets" };
String esp_mqtt_id = "ESP_";
WiFiClient espClient;
PubSubClient client(espClient);
Ticker mqtt_refresh;

/* ESPNOW functions */
void setup_custom_wifi();
void setup_espnow();
void add_peer(uint8_t* mac, int channel, bool encrypt);
void OnDataSent(const uint8_t* mac, esp_now_send_status_t status) { /* nothing for now */ };
void OnDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) { /* nothing for now */ };

/* MQTT functions */
void setup_wifi_mqtt();
void mqtt_reconnect();
void mqtt_callback(char* ftopic, uint8_t* msg, uint32_t len);
void mqtt_refresh_loop() { client.loop(); }

/* MATH functions to simulate data*/
float square_wave(int, int);
float exp_cos(int);
float exp_only(int, int);
int get_esp_id(uint8_t mac_lsb);

// the setup function runs once when you press reset or power the board
void setup() {

	Serial.begin(115200);
	WiFi.mode(WIFI_STA);

	/* ---------- Setup ESP NOW ------------ */
	if (CUSTOM_WIFI_CFG) {
		WiFi.disconnect();
		setup_custom_wifi();
		esp_wifi_set_ps(WIFI_PS_NONE);	// power saving mode none
	}
	setup_espnow();
	add_peer(mac_broadcast, CHANNEL, false);
	/* ------------------------------------- */

	WiFi.macAddress(myMac);
	esp_id = get_esp_id(myMac[5]);
	esp_mqtt_id.concat(esp_id);
	Serial.println("ID asignada a la placa: " + esp_mqtt_id);

	/* ----------- Setup MQTT -------------- */
	setup_wifi_mqtt();
	client.setServer(mqtt_server, 1883);
	client.setCallback(mqtt_callback);
	if (!client.connected()) {
		mqtt_reconnect();
		Serial.println("Sub a los topicos: ");
		for (int i = 0; i < sizeof(topic) / sizeof(String); i++)
			Serial.println("> " + topic[i]);
	}
	mqtt_refresh.attach(1, mqtt_refresh_loop);		// llama a client.loop() cada 1 seg
	/* ------------------------------------- */

	packet.timestamp = 0;
	packet.data = 0.f;

	Serial.println("------ Setup Completado! ------");
}

// the loop function runs over and over again until power down or reset
void loop() {

	if (N_packets < TOTAL_PACKETS) {
		t1 = millis();
		if (t1 - t0 >= T_MS) {
			packet.timestamp = millis();
			float h;

			switch (esp_id) {
			case 0: { h = exp_cos(n); break; }
			case 1: { h = exp_only(n, 80); break; }
			case 2: { h = square_wave(n, 150); break; }
			default: h = exp_cos(n);
			}

			packet.data = h;
			esp_now_send(peer_info.peer_addr, (uint8_t*)&packet, sizeof(packet));
			n++;
			N_packets++;
			t0 = millis();
		}
	}
	else if (N_packets == TOTAL_PACKETS) {
		Serial.println("------ Finished Sending " + String(TOTAL_PACKETS) + " Packets -------"); 
		N_packets++; 
	}

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

//////////////////////////////////////////////////////////////////////////
/* MQTT Setup */
void setup_wifi_mqtt() {

	delay(50);
	Serial.println("Connecting to " + String(ssid));
	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) {
		delay(3000);
		Serial.print(".");
		WiFi.begin(ssid, password);
	}

	Serial.print("\nWi-Fi Connected!, IP Address: ");
	Serial.println(WiFi.localIP());
}

void mqtt_reconnect() {
	while (!client.connected()) {
		if (client.connect( esp_mqtt_id.c_str() )){
			for(int i = 0; i < sizeof(topic)/sizeof(String); i++)
				client.subscribe(topic[i].c_str());
		}
		else { delay(500); }
	}
}

void mqtt_callback(char* ftopic, uint8_t* msg, uint32_t len) {

	char* rcv_msg = (char*) malloc(len+1);
	memcpy(rcv_msg, msg, len);
	rcv_msg[len] = '\0';
	//Serial.printf("Msg received: %s", msg);
	if (!strcmp(rcv_msg, "resend")) {
		N_packets = 0;
		n = 0;
	}
	else if (!strcmp(ftopic, topic[0].c_str())) {
		esp_id = atoi(rcv_msg);
		if (esp_id > 2 || esp_id < 0) { esp_id = 0; }
		Serial.println("Output function changed to = " + String(esp_id));
	}
	else if ( !strcmp(ftopic, topic[1].c_str()) ) {
		T_MS = atoi(rcv_msg);
		Serial.println("T_MS changed to = " + String(T_MS));
	}
	else if ( !strcmp(ftopic, topic[2].c_str()) ) {
		TOTAL_PACKETS = atoi(rcv_msg);
		Serial.println("TOTAL_PACKETS changed to = " + String(TOTAL_PACKETS));
	}

	free(rcv_msg);
}

//////////////////////////////////////////////////////////////////////////
/* MATH functions */
float square_wave(int n, int T) {		// second order system square wave response
	int k = n % T;
	int j = n / T;
	float h = 0.5 * (1 - expf(-0.12 * k) * cosf(0.5 * k)) * powf(-1, j);
	return h;
}

float exp_cos(int n) {		// second order system step response
	float h = 1 - expf(-0.016 * n) * cosf(0.1 * n);
	return h;
}

float exp_only(int n, int T) {		// first order system step response
	float h = 0;
	int m = n - T;
	if (m >= 0) {
		h = 1.2 * (1 - expf(-0.016 * m) * cosf(0.02 * m));
	}
	return h;
}

int get_esp_id(uint8_t mac_lsb) {
	int id = -1;
	for (int i = 0; i < sizeof(ESP_IDs); i++) {
		if (mac_lsb == ESP_IDs[i]) {
			id = i;
			break;
		}
	}
	return id;
}
