#include <WiFi.h>
#include <PubSubClient.h>
#include "SPI.h"
#include <Wire.h>   // ???
#include <VL53L0X.h>  // Pololu 1.0.2
#include <esp_wifi_types.h>
#include <esp_wifi_internal.h>
#include <esp_wifi.h>
#include <esp_now.h>

VL53L0X SensorToF;
//--------- Definiciones ESPNOW ---------------//
#define PAYLOAD_SIZE    32                  // Tamano en Bytes de los paquetes a enviar por ESP-NOW
#define DATARATE        WIFI_PHY_RATE_6M   // Para cambiar el bitrate de ESP-NOW a 24Mbps
#define CHANNEL         1                   // Canal WiFi

//Adafruit_VL53L0X lox = Adafruit_VL53L0X();  //Constructor para sensor distancia
//VL53L0X_RangingMeasurementData_t measure;
  
// Cambio variable yy1 -> count_cam

struct MD               // Estructura para las mediciones de mouse_cam
{
    byte motion;
    char dx, dy;
    byte squal;
    word shutter;
    byte max_pix;
    int over;
};

int yy=0;               // coordenadas obtenidas por sensor ADNS3080 (dead reckoning)
double temp_cal;        // medicion temporal distancia [cm]
double scale;           // escalamiento de cuentas ADNS a [cm] 
double medi = 10;
double xx = 10;

///////////////////////////////////////
//     Configuración MQTT   ///////////
///////////////////////////////////////
//const char* ssid = "ac3e";  // Nombre WiFi a conectarse
//const char* password = "rac3e/07"; // Contraseña WiFi
//onst char* ssid = "stringUTEM";
//const char* password = "stringstable";
//const char* ssid = "trenesAC3E";
//const char* password = "stringstable";
const char* ssid = "fvp";
const char* password = "nomeacuerdo";
//const char* ssid = "VTR-6351300";
//const char* password= "zkd2bxhcHqHm";

//const char* mqtt_server = "10.1.28.117";  // IP de la Raspberry
//const char* mqtt_server = "192.168.137.1";
const char* mqtt_server = "192.168.1.100";  // IP fvp
//const char* mqtt_server = "192.168.0.24";
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;  
char msg[50];  

////////////////////////////////////////////
//      Configuracion ESP-NOW       ////////
////////////////////////////////////////////
uint8_t mac_leader[] = {0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F};		// custom MAC
uint8_t mac_addr_broadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct {
    uint32_t timestamp;
    double position;
    double velocity;
} ESPNOW_payload;

esp_now_peer_info_t peerInfo;   // Estructura para anadir peers a espnow


////////////////////////////////////////////////////
//          Configuración motor DC      ////////////
////////////////////////////////////////////////////
const int Control_fwd = 25;                //  Pin AIN1  [Control del sentido de rotaciÃ³n +]
const int Control_back = 26;            //  Pin AIN2   [Control del sentido de rotaciÃ³n -]
const int Control_v = 12;                 //  Pin PWMA    [Control de la velocidad de rotaciÃ³n]
int MotorSpeed = 0;                   // Velocidad del motor  0..1024
int MotorDirection = 1;               // Avanzar (1) o Retroceder (0)

////////////////////////////////////////////////
//         Variables para Sensor Distancia //////
////////////////////////////////////////////////
double distancia = 25;
double old_d = 0;                         
double old = 0;  
double t_distancia = 0;
double t_old;
double t1; // para temporizar la medición de la velocidad
double t2;

////////////////////////////////////////////
//      Varaibles para código general //////
////////////////////////////////////////////
int dead = 1;     // flag buscar zona muerta
int deadband = 300;     //valor de deadband inicial en caso de no querer buscar deadband
int tiempo_inicial = 0;
bool flag = true; //flag para sincronizar
bool start = false; // variable para iniciar la prueba
bool run_test = false;

////////////////////////////////////////////
// Variables y parametros PID //////////////
////////////////////////////////////////////
double v_lider = 0, mierror, v_medida, v_min = 1000;
double u = 1;       // Actuacion (?)

//////////////////////////////////////////////////////

void loop() {

    //Verificación de conexion MQTT
    /*if (!client.connected()) 
    {
    	esp_now_deinit();
        reconnect();
        esp_now_init();
    }*/
    client.loop();
    if(run_test){
    	client.disconnect();
    	start = true;
    }
    if(flag==false){   // Envia desfase para sincronizar datos de la prueba
        String delta = String((millis()-tiempo_inicial)*0.001);
        delta.toCharArray(msg, delta.length() + 1);                                                                           
        client.publish("trenes/carroA/desfase", msg);
        flag = true;
        //start = true;
        Serial.println("Sync!");
    }  // Da comienzo a la prueba
	
    else if (start){
        // Mediciones  
        medi=0;
        int count_cam=0;
        old=millis();
        MD md;
        //int val = mousecam_read_reg(ADNS3080_PIXEL_SUM);
        //delayMicroseconds(3500);
        for (int i = 0; i < 20 ; i++) {
            if (i % 5 == 1) {    // solo algunas mediciones de ToF. No se actualiza muy seguido. Pero ya no hay tiempo perdido entre mediciones.
                uint16_t range = SensorToF.readReg16Bit(SensorToF.RESULT_RANGE_STATUS + 10);
                medi=medi+range;
            }
            mousecam_read_motion(&md);
            count_cam += (int8_t) md.dy;   //(int8_t) md.dy;    // medición de distancia entre old y la última medición
            delayMicroseconds(800);  // retardo arbitrario. No cambia mucho la medición del adns3080. Si se lee el registro de movimiento muy seguido entrega 0s pq no ha detectado movimiento.
        }
  
        v_medida=((scale*1000)*count_cam)/(millis()-old);   // Problema: Bajé el foco del lento para aumentar la resolución
                                                            // Funciona pero hay que revisar cada lente para mejorarla. 
        medi=medi/40; // promediamos 4 mediciones del sensor de distancia. No se si sirve de algo
        Serial.println(v_medida);                                              
        // Agregar delay? para enviar datos en forma sincronizada. Setup actual: millis()- old = 44 o 45. Quedan entre 6 y 5 ms

        /* Rutina para publicar en MQTT */
        /*
        if ((millis()-t_old) > 50){
            String v_lider = String(v_medida, 2);
            v_lider.toCharArray(msg, v_lider.length() + 1);                    // Datos enviados para analizar controlador
            client.publish("trenes/carroL/v_lider", msg);
            String envio = String(v_medida, 2)+ ", " + String(millis()-tiempo_inicial);
            envio.toCharArray(msg, envio.length() + 1);                    // Datos enviados para analizar controlador
            client.publish("trenes/carroL/test", msg);
            t_old = millis();
        }
        else{
            delay(47-(millis()-t_old)); //49
            String v_lider = String(v_medida, 2);
            v_lider.toCharArray(msg, v_lider.length() + 1);                    // Datos enviados para analizar controlador
            client.publish("trenes/carroL/v_lider", msg);
            String envio = String(v_medida, 2)+ ", " + String(millis()-tiempo_inicial);
            envio.toCharArray(msg, envio.length() + 1);                    // Datos enviados para analizar controlador
            client.publish("trenes/carroL/test", msg);
            t_old = millis();
        }*/

        /* Envio por ESP-NOW */
        delay(7);
        uint32_t time_now = millis() - t_old;
        if( (time_now > 50) ){
            //Serial.println(medi);
            ESPNOW_payload.timestamp = time_now;
            ESPNOW_payload.velocity = v_medida;
            ESPNOW_payload.position = medi;
            esp_now_send(mac_addr_broadcast, (uint8_t*) &ESPNOW_payload, sizeof(ESPNOW_payload));         // 'True' broadcast, no hay ACK del receptor
            //esp_now_send(NULL, (uint8_t*) &v_lider, sizeof(v_lider));                     // Destino NULL para iteracion a todos los receptos. Sí hay ACK del receptor.
            t_old = millis();
        }
        /*else{
            if(!resend)
                delay( 47 - (millis()-t_old) );
            v_lider = v_medida;
            esp_now_send(mac_addr_broadcast, (uint8_t*) &v_lider, sizeof(v_lider));
            //esp_now_send(NULL, (uint8_t &v_lider, sizeof(v_lider)));
            t_old = millis(); 
            resend = false;
        }*/
        ////////////////////////////////////////////////////////
        //        Rutina del Carro                 /////////////
        ////////////////////////////////////////////////////////
  
        if (abs(u) != 0 && medi > 12.0)
        {
            if (u >= 0){
                MotorDirection = 1;
                MotorSpeed = u;
            }
            else{
                MotorDirection = 0;
                MotorSpeed = -u;
            } 
        }
        else  
        {
            if (MotorSpeed > 20)
                MotorSpeed =  MotorSpeed - 20; 
            else
            {
                //start = false;
                MotorSpeed = 0;
                u = 1;                            // Si queda en cero, al momento de reiniciar la prueba ingresara directamente a este bucle
                run_test = false;
                //reconnect();
            } 
        }
        SetMotorControl();
    }
}

///////////////////////////////////////////////////////

void SetMotorControl()
{
    if (MotorDirection == 1)            //Avanzar
    {
        digitalWrite(Control_fwd, LOW);
        digitalWrite(Control_back, HIGH);
    }
    else                                //Retroceder
    {
        digitalWrite(Control_fwd, HIGH);
        digitalWrite(Control_back, LOW);
    }

    ledcWrite(Control_v, MotorSpeed); // esp32 (PIN, duty cycle PWM)
}



void callback(char* topic, byte* payload, unsigned int length) {
    String mensaje;
    for (int i = 0; i < length; i++) {
        char nuevo = (char)payload[i];
        mensaje.concat(nuevo);
    }
    Serial.println("Mensaje Recibido por MQTT");
    ////////////////////////////////
    // QUE HACE AL RECIBIR DATOS ///
    ////////////////////////////////
    if (String(topic) == "trenes/sync") {
        if (String(mensaje)=="True") {
            tiempo_inicial = millis();
            Serial.println("Sync Recibido");
            flag = false;
        }
        else if (String(mensaje)=="False"){
            Serial.println("Detener");
            flag = true;
            start = false;
            MotorSpeed = 0;
            SetMotorControl();      
        }
    }
    if (String(topic) == "trenes/start"){
    	if(String(mensaje) == "True"){
    		run_test = true;
    		Serial.println("Starting Test");
    	}
    }
/*  if (strcmp(topic , "trenes/carro4/p") == 0) {
    Kp = mensaje.toFloat();
    Serial.println(Kp);
  }
  if (strcmp(topic , "trenes/carro4/i") == 0) {
    Ki = mensaje.toFloat();
    Serial.println(Ki);
  }
  if (strcmp(topic , "trenes/carro4/d") == 0) {
    Kd = mensaje.toFloat();
    Serial.println(Kd);
  }*/
    if (strcmp(topic , "trenes/carrol/u") == 0) {
        u = mensaje.toFloat();
        Serial.println(u);
    }
  
}

/////////////////////////////////////////////////////

/* ESPNOW Sent Callback Function */
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    /*if ( status == ESP_NOW_SEND_SUCCESS ) {
        //Serial.println("Delivery Success");
        resend = false;
    }
    else {
        //Serial.println("Delivery Fail");
        resend = true;
    }*/
}

/* ESPNOW Receive Callback Function */
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    // Definir que hacer al recibir
}
