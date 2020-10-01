/*
 * Etienne Arlaud
 * Modificaciones Felipe Villenas - 09-29-2020 
 */

#include <stdio.h>
#include <iostream>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <thread>

#include <fstream>
//#include <sstream>

#include "ESPNOW_manager.h"
#include "ESPNOW_types.h"

using namespace std;

static uint8_t my_mac[6] = {0x48, 0x89, 0xE7, 0xFA, 0x60, 0x7C};
static uint8_t dest_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};	// broadcast addr
static uint8_t ESP_mac[6] = {0xC8,0x2B,0x96,0xB4,0xE6,0xCC};

bool stop_flag = false;		// flag de control para terminar el programa
ESPNOW_payload rcv_data;	// struct para guardar los datos recibidos por la esp
std::ofstream *myFile;	
	
ESPNOW_manager *handler;

uint8_t payload[127];

/* Funcion de control para terminar el programa */
void wait4key() { std::cin.get(); stop_flag = true; }

/* Funcion para imprimir el contenido del paquete */
void print_packet(uint8_t *data, int len)
{
    printf("----------------------------new packet-----------------------------------\n");
    int i;
    for (i = 0; i < len; i++)
    {
        if (i % 16 == 0)
            printf("\n");
        printf("0x%02x, ", data[i]);
    }
    printf("\n\n");
}

/* Funcion de callback al recibir datos */
void callback(uint8_t src_mac[6], uint8_t *data, int len) {
	handler->mypacket.wlan.actionframe.content.length = 127 + 5;
	memcpy(handler->mypacket.wlan.actionframe.content.payload, data, 6);
	
	/* Copia los datos recibidos */
	memcpy(&rcv_data, data, sizeof(ESPNOW_payload));
	
	std::cout << "--- Packets Received ---" << std::endl;
	std::cout << rcv_data.sent_time << ", " << rcv_data.esp_data << "\n" << std::endl;
	
	/* Guarda en el archivo */
	*myFile << rcv_data.sent_time << "," << rcv_data.esp_data << "\n";
	
	//print_packet(data, len);
	//handler->send();
}

int main(int argc, char **argv) {

	assert(argc > 1);
	nice(-20);	// setea la prioridad del proceso -> rango es de [-20, 19], con -20 la mas alta prioridad
	
	myFile = new std::ofstream("data.csv");
	std::thread close_ctrl(wait4key);	
	
	handler = new ESPNOW_manager(argv[1], DATARATE_6Mbps, CHANNEL_freq_9, my_mac, dest_mac, false);
	handler->set_filter(ESP_mac, dest_mac);
	handler->set_recv_callback(&callback);
	handler->start();

	while(!stop_flag) {
		std::this_thread::yield();
	}
	
	close_ctrl.join();
	myFile->close();
	handler->end();
	std::cout << "program terminated by user" << std::endl;

	return 0;
}
