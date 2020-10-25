/*
 * Etienne Arlaud
 * Modificaciones Felipe Villenas - 09-29-2020 
 */

#include <stdio.h>
#include <iostream>
#include <chrono>
#include <sstream>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <thread>
#include <fstream>

#include "ESPNOW_manager.h"
#include "ESPNOW_types.h"

#define MAC_2_MSBytes(MAC)  MAC == NULL ? 0 : (MAC[0] << 8) | MAC[1]
#define MAC_4_LSBytes(MAC)  MAC == NULL ? 0 : (((((MAC[2] << 8) | MAC[3]) << 8) | MAC[4]) << 8) | MAC[5]

using namespace std;

static uint8_t my_mac[6] = {0x48, 0x89, 0xE7, 0xFA, 0x60, 0x7C};
static uint8_t dest_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};	// broadcast addr
/*static uint8_t ESP_macs[][6] = {	{ 0xC8, 0x2B, 0x96, 0xB4, 0xE6, 0xCC },
									{ 0xC8, 0x2B, 0x96, 0xB5, 0x78, 0x0C },
									{ 0xC4, 0x4F, 0x33, 0x15, 0xB0, 0x99 }
								};
static int number_of_ESPs = sizeof(ESP_macs)/6;

static uint8_t ESP_mac[6] = {0xC8, 0x2B, 0x96, 0xB4, 0xE6, 0xCC};
static uint8_t ESP_mac2[6] = { 0xC8, 0x2B, 0x96, 0xB5, 0x78, 0x0C };*/

typedef struct {
	uint64_t mac[10];
	int n_esp = 0;
	int get_esp_id(uint8_t* mac);
} ESP_MAC_list;

typedef struct {
	uint32_t sent_time;
	float esp_data;
} ESPNOW_payload;

bool first_packet = true;
uint32_t t0_esp;			// tiempo inicial de la ESP
uint32_t packet_counter = 0;
auto t0_pc = std::chrono::steady_clock::now();	// tiempo inicial pc
auto last_open_file = std::chrono::steady_clock::now();		// tiempo desde la ultima vez que se cerro el archivo

bool stop_flag = false;		// flag de control para terminar el programa
ESPNOW_payload rcv_data;	// struct para guardar los datos recibidos por la esp
std::ofstream *myFile;		// objeto para manejar el archivo de 'output'
ESP_MAC_list mac_list;		// struct para almacenar la lista de MACs

ESPNOW_manager *handler;

void wait4key() { std::cin.get(); stop_flag = true; }	// Funcion de control para termianar el programa, espera por una tecla presionada (Enter o 'char' + Enter)
void callback(uint8_t src_mac[6], uint8_t *data, int len);
void calcPacketLoss(uint32_t T_ms);


int main(int argc, char **argv) {

	assert(argc > 1);
	nice(-20);	// setea la prioridad del proceso -> rango es de [-20, 19], con -20 la mas alta prioridad
	
	std::remove("data.csv");
	myFile = new std::ofstream("data.csv");
	std::thread close_ctrl(wait4key);		// inicia un thread para manejar el cierre del programa	

	handler = new ESPNOW_manager(argv[1], DATARATE_6Mbps, CHANNEL_freq_8, my_mac, dest_mac, false);
	//handler->set_filter(ESP_macs[1], dest_mac);
	handler->set_filter();		// filtering only by ESP header
	handler->set_recv_callback(&callback);
	handler->start();
	
	while(!stop_flag) {
		std::this_thread::yield();
	}
	
	handler->end();
	close_ctrl.join();
	myFile->close();

	if (argc > 3 && !strcmp(argv[2],"-p")) {
		calcPacketLoss(atoi(argv[3]));
	}
	
	std::cout << "\nProgram terminated by user" << std::endl;
	return 0;
}

/* Funcion de callback al recibir datos */
void callback(uint8_t src_mac[6], uint8_t *data, int len) {
	
	packet_counter++;
	auto t_now = std::chrono::steady_clock::now(); 	// tiempo local del pc
	int id = mac_list.get_esp_id(src_mac);

	/* Copia los datos recibidos */
	memcpy(&rcv_data, data, sizeof(ESPNOW_payload));
	
	/* Normalizacion para que el primer timestamp sea 0 */
	if(first_packet){
		t0_esp = rcv_data.sent_time;
		t0_pc = t_now;
		first_packet = false;
	}

	std::chrono::duration<double, std::milli> dt_rcv = t_now - t0_pc;
	std::chrono::duration<double, std::milli> dt_file = t_now - last_open_file;

	std::cout << "Packets Received from ESPs : " << packet_counter << "\r";
	std::cout.flush();

	if (!myFile->is_open()) {
		myFile->open("data.csv", std::ofstream::out | std::ofstream::app);	// open file in output and append mode
	}
	/* Guarda en el archivo */
	*myFile << id << "," << dt_rcv.count() << "," << rcv_data.esp_data << "," << rcv_data.sent_time - t0_esp  << "\n";
	
	if (myFile->is_open() && dt_file.count() >= 40) {	// close the file if more than ~40 ms has passed since the last time it was closed
		myFile->close();								// this trick is only done to make the plot in "real time" look smooth
		last_open_file = std::chrono::steady_clock::now();
	}

	//handler->send();
}

void calcPacketLoss(uint32_t T_ms) {

	if(T_ms <= 0) {return;}

	std::ifstream file("data.csv");
	std::string line;
	uint32_t last = 0, current = 0;
	uint32_t delta = T_ms / 4;

	int loss = 0;

	while (std::getline(file, line)) {
		std::stringstream sstream(line);
		sstream >> current;

		if (current - last > T_ms + delta){
			loss += (current - last) / T_ms - 1;
		}
		last = current;
	}

	float percentage = (float)loss / (packet_counter + loss)  * 100;
	std::cout << "Packet Loss at " << T_ms << " [ms] : " << percentage << "%" << std::endl;
}

int ESP_MAC_list::get_esp_id(uint8_t* mac){

	int id = -1;
	uint64_t LSB = MAC_4_LSBytes(mac);
	uint64_t MSB = MAC_2_MSBytes(mac);
	uint64_t mac_tr = (MSB << 32) + LSB;
	for(int i=0; i < (int)(sizeof(mac_list.mac)/sizeof(uint64_t)); i++){
		if(mac_tr == mac_list.mac[i]){
			id = i;
		}
	}
	if(id == -1){
		mac_list.mac[mac_list.n_esp] = mac_tr;
		id = mac_list.n_esp;
		mac_list.n_esp++;
	}
	return id;
}
