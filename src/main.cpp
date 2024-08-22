#include <Arduino.h>
#include "nvs.h"
#include "nvs_flash.h"
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
//#include <RTClib.h>
#include <Adafruit_NeoPixel.h>


#define NO 0
#define SI 1

// Colores del texto en tablero
#define RED   1
#define GREEN 2
#define BLUE  3

          
// variables de control para pausa
ulong previous_time, current_time, previous_time_solicitud , actual_time_solicitud;
ulong tiempo_inicial, tiempo_final;


// variables para envio de tiro a DIana
#define LIBRE 0
#define OCUPADO 1


// varables para envio de datos a diana
#define INICIALIZA_TRANSMISION 1
#define SELECCIONA_DIANA_LIBRE 2
#define MENU_SELECCIONA_COLOR 3
#define PREPARA_PAQUETE_ENVIO  4
#define ENVIA_PAQUETE          5
#define CONFIRMA_RECEPCION_DEL_PAQUETE 6
#define ESPERA_20MSEG_3INTENTOS 7
#define ENVIO_EXITOSO 8
#define LIBERA_BUZON_Y_LIMPIA_DATOS_REMITENTE 9
int8_t menu_transmision = INICIALIZA_TRANSMISION;
int intentos_envio=0;

// direcciones mac de receptores

uint8_t   broadcastAddress1[]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
uint8_t   broadcastAddress2[]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
uint8_t   broadcastAddress3[]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
uint8_t   broadcastAddress4[]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; //Monitor Central para regreso de paquetes


// estructura de envio de datos al compañero(peer)
typedef struct structura_mensaje
{
int t; //test
int d; //diana
int c; //color
int p; //propietario del
float tiempo;
} structura_mensaje;

structura_mensaje datos_enviados;
structura_mensaje datos_recibidos;
structura_mensaje datos_locales_diana;  // para usar en diana


#define TEST 0
#define TIRO_ACTIVO 1

// variable tipo esp_now_info_t para almacenar informacion del compañero
esp_now_peer_info_t peerInfo;


/* -------------------PASO 0 INICIO DE ESP-NOW ------------------*/
void Inicia_ESP_NOW()
{
if (esp_now_init() != ESP_OK)
    {
      pixels.setPixelColor(1,pixels.Color(10,0,0));
      pixels.setPixelColor(2,pixels.Color(10,0,0));
      pixels.setPixelColor(3,pixels.Color(10,0,0));
      Serial.println("Error initializing ESP-NOW");
      return;
    }
}

/* --------------------PASO 1 LECTURA DE LA MAC -------------------------------------------------*/
// obtiene la direccion mac de  ESTE EQUIPO ESP-32
void readMacAddress()
{
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) 
    {
      Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                    baseMac[0], baseMac[1], baseMac[2],
                    baseMac[3], baseMac[4], baseMac[5]);
    } 
  else 
    {
      Serial.println("Failed to read MAC address");
      pixels.setPixelColor(1,pixels.Color(10,0,0));
      pixels.setPixelColor(2,pixels.Color(10,0,0));
      pixels.setPixelColor(3,pixels.Color(10,0,0));
      pixels.setPixelColor(4,pixels.Color(10,0,0));
    }
}
  
#define VACIO 0
#define CON_MENSAJE 1


/* ----------------PASO 2 ADD PEER-----------------------------------------------------*/
// firma peer de
void Peer_Monitor_En_Linea()
{
  // register peer1
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  memcpy(peerInfo.peer_addr, broadcastAddress4, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
     {
      pixels.setPixelColor(1,pixels.Color(10,0,0));
      Serial.println("Failed to add peer");
        return;
      }
   else
      { 
        pixels.setPixelColor(1,pixels.Color(10,0,0));
        Serial.println("Add Peer Monitor OK");

      }
}

/* ------------------PASO 3 PRUEBA DE CONEXION ---------------------------------------------------*/
void Test_Conexion_Diana()
{ 
  // DATOS A ENVIAR  
    datos_enviados.t=TEST;
    datos_enviados.c=1;   //SE MANDA ROJO PARA TEST
    datos_enviados.p=0; // JUGADOR 0 NO ACTIVO
    datos_enviados.tiempo=0; // para regresar el tiempo
    
  // monitor
    esp_err_t result4 = esp_now_send(broadcastAddress4, (uint8_t *) &datos_enviados, sizeof(structura_mensaje));
    if (result4 == ESP_OK) 
     {
      pixels.setPixelColor(1,pixels.Color(0,10,0));
      pixels.setPixelColor(2,pixels.Color(0,10,0));
      Serial.println("Sent with success");
     }
    else 
     {
      pixels.setPixelColor(1,pixels.Color(10,0,0));
      pixels.setPixelColor(2,pixels.Color(10,0,0));
      Serial.println("Error sending the data");
     }
}

 /* -------------VARIABLES DE RECEPCION DE DATOS---------------------------------*/

int8_t rojo=0;
int8_t verde=0;
int8_t azul=0;
#define NULO 0
int color_diana = NULO;
#define TIEMPO_DE_RETARDO_DE_RESPUESTA 500 // 500 mseg entre termino de disparo y envio a monitor

//estado de diana
#define APAGADO 0
#define ENCENDIDO 1
int estado_diana =APAGADO;

#define EN_ESPERA 0
#define INICIA_CONTADOR_TIEMPO 1
#define INICIA_PROCESO_SOLICITUD 2
#define ENCIENDE_SOLICITUD 3
#define ESPERA_LEDS_SOLICITUD 4
#define INICIA_PROCESO_APUNTANDO 5
#define ENCIENDE_APUNTANDO 6
#define ESPERA_LEDS_APUNTA 7
#define INICIA_PROCESO_DISPARO 8
#define ENCIENDE_DISPARO 9
#define ESPERA_LEDS_DISPARO 10
#define ESPERA_ENVIO_RESULTADOS 11
#define ENVIA_RESULTADOS 12
#define DATOS_ENVIADOS_OK 13
int menu_encendido_diana = EN_ESPERA;


#define SIN_SEÑAL 0
#define APUNTANDO 1
#define DISPARO 2
int frecuencia_detectada =0;

/* -------------------- RECEPCION DE DATOS -----------------------*/
// Callback when data is received
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) 
{
  memcpy(&datos_recibidos, incomingData, sizeof(datos_recibidos));
  //informacion de longuitud de paquete
  Serial.print("Bytes received: ");
  Serial.println(len);
 
  char macStr[18];
  Serial.print("Packet received from: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
  

  if (datos_recibidos.t==TIRO_ACTIVO)
    { 
      estado_diana=ENCENDIDO;
      menu_encendido_diana=INICIA_CONTADOR_TIEMPO;
      datos_locales_diana.t =datos_recibidos.t;
      datos_locales_diana.c =datos_recibidos.c;
      datos_locales_diana.d =datos_recibidos.d;
      datos_locales_diana.p =datos_recibidos.p;
      datos_locales_diana.tiempo=0.00;
      switch (datos_locales_diana.c)
      {
      case GREEN:
        rojo=0;
        verde=1;
        azul=0;
        frecuencia_apunta=frecuencia_apunta_verde;
        frecuencia_disparo=frecuencia_disparo_verde;
        break;
      case BLUE:
        rojo=0;
        verde=0;
        azul=1;
        frecuencia_apunta=frecuencia_apunta_azul;
        frecuencia_disparo=frecuencia_disparo_azul;
        break;
      default:
        break;
      }
    }
}




//variables para deteccion de disparo en analisis de frecuencia
#define SIN_DETECCION 0
#define LASER_APUNTANDO 2
#define DISPARO_DETECTADO 3
int8_t estado_detector = SIN_DETECCION;

// definicion de io-esp32
const int pin_leds = 2;
uint8_t led_inicio=0;
int orden_led_[40]={0,8,9,22,23,31,32,45,1,7,10,21,24,30,33,44,2,6,11,20,33,43,3,5,12,19,34,42,4,4,13,18,35,41};
#define NUMPIXELS 46
Adafruit_NeoPixel pixels(NUMPIXELS,pin_leds,NEO_RGB+NEO_KHZ800);
int prende_leds=SI;
int8_t destello =1;


/* variables para analisis de frecuencia */
uint16_t frecuencia_central=0;
uint16_t frecuencia_apunta=0;
uint16_t frecuencia_disparo=0;
uint16_t frecuencia_apunta_verde=1200;
uint16_t frecuencia_apunta_azul= 3000;

uint16_t frecuencia_disparo_verde=2400;
uint16_t frecuencia_disparo_azul =4800;

int primer_encendido_apunta = SI;
uint8_t led_apunta=5;


/* -----------------------INICIO    S E T U P --------------------------------*/

void setup() 
{
  // inicia puerto de salida de leds
  pinMode (pin_leds,OUTPUT);

  // inicia Neopixel strip
  pixels.begin();
  pixels.clear(); // inicializa todos en off

  // inicia comunicacion serial
  Serial.begin(115200);

  // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);

  // escribe direccion mac
  Serial.print("[DEFAULT] ESP32 Board MAC Address: ");
  readMacAddress();

  // Init ESP-NOW
  Inicia_ESP_NOW();
  

  // sincronizacin de dianas (peer)
  Peer_Monitor_En_Linea();

  // test en linea MOnitor
  Test_Conexion_Diana();
} // fin setup

/* ----------------------  F I N    S E T U P -------------------------*/


void loop() 
{
  switch (estado_diana)
    {
      case APAGADO:
          /* code */
          /*se mantiene aqui, hasta qeu se inicie el proceso en el void
          de recepcion de solicitud, ahi se asigna los datos de color,*/
          break;

      case ENCENDIDO:
          switch (menu_encendido_diana)
            {
              case INICIA_CONTADOR_TIEMPO:
                   tiempo_inicial=millis();
                   menu_encendido_diana=INICIA_PROCESO_SOLICITUD;
                   break;

              case INICIA_PROCESO_SOLICITUD:
                  /*  contador de tiempo*/
                  previous_time=millis();
                  previous_time_solicitud=millis();
                  menu_encendido_diana=ENCIENDE_SOLICITUD;
                  led_inicio=0;
                  prende_leds=SI;
                  break;

              case ENCIENDE_SOLICITUD:
                  if (prende_leds==SI)
                    {
                      //int orden_led_[40]={0,8,9,22,23,31,32,45,1,7,10,21,24,30,33,44,2,6,11,20,33,43,3,5,12,19,34,42,4,4,13,18,35,41};
                      pixels.setPixelColor(orden_led_ [led_inicio+1],pixels.Color(rojo,verde*10,azul*10));
                      pixels.setPixelColor(orden_led_ [led_inicio+2],pixels.Color(rojo,verde*10,azul*10));
                      pixels.setPixelColor(orden_led_ [led_inicio+3],pixels.Color(rojo,verde*10,azul*10));
                      pixels.setPixelColor(orden_led_ [led_inicio+4],pixels.Color(rojo,verde*10,azul*10));
                      pixels.setPixelColor(orden_led_ [led_inicio+5],pixels.Color(rojo,verde*10,azul*10));
                      pixels.setPixelColor(orden_led_ [led_inicio+6],pixels.Color(rojo,verde*10,azul*10));
                      pixels.setPixelColor(orden_led_ [led_inicio+7],pixels.Color(rojo,verde*10,azul*10));
                      pixels.setPixelColor(orden_led_ [led_inicio+8],pixels.Color(rojo,verde*10,azul*10));
                      pixels.show();
                      prende_leds=NO;
                    }
                  menu_encendido_diana=ESPERA_LEDS_SOLICITUD;
                  break;
        
              case ESPERA_LEDS_SOLICITUD:
                  frecuencia_detectada =Analisis_De_Frecuencia(); 
                  // frecuencia_detectada = 0 SIN DETECCION / 1 LASER_APUNTANDO/ 2 DISPARO_DETECTADO
                  switch (frecuencia_detectada)
                    {
                      case SIN_DETECCION:
                        /* continua con la solicitud de disparo*/
                        actual_time_solicitud=millis();
                        if (actual_time_solicitud > (previous_time_solicitud+300))
                          {
                            pixels.clear();
                            led_inicio=led_inicio+8;
                            if (led_inicio>40)
                              {
                                led_inicio=0;
                              }
                            prende_leds=SI;
                            previous_time_solicitud=millis();
                            menu_encendido_diana=ENCIENDE_SOLICITUD;
                          }                        
                        break;

                      case LASER_APUNTANDO:
                        menu_encendido_diana=INICIA_PROCESO_APUNTANDO;
                        prende_leds=SI;
                        break;
                      
                      case DISPARO_DETECTADO:
                        menu_encendido_diana=INICIA_PROCESO_DISPARO;
                        prende_leds=SI;
                        led_apunta=5;
                        break;

                    } //fin de switch frecuencia_detectada
              case INICIA_PROCESO_APUNTANDO:
                 /*  contador de tiempo*/
                  previous_time=millis();
                  previous_time_solicitud=millis();
                  menu_encendido_diana=ENCIENDE_APUNTANDO;
                  prende_leds=SI;
                  led_apunta=5;
                  break;    

              case ENCIENDE_APUNTANDO:
                  /* CODGIDO*/
                  if (prende_leds==SI)
                    {
                      pixels.setPixelColor(led_apunta-5,pixels.Color(0,0,0));
                      pixels.setPixelColor(led_apunta-4,pixels.Color(rojo,verde*5,azul*5));
                      pixels.setPixelColor(led_apunta-3,pixels.Color(rojo,verde*10,azul*10));
                      pixels.setPixelColor(led_apunta-2,pixels.Color(rojo,verde*15,azul*15));
                      pixels.setPixelColor(led_apunta-1,pixels.Color(rojo,verde*20,azul*20));
                      pixels.setPixelColor(led_apunta,pixels.Color(rojo,verde*25,azul*25));
                      pixels.show();
                      prende_leds=NO;
                    }
                  menu_encendido_diana=ESPERA_LEDS_APUNTA;
                  break;

              case ESPERA_LEDS_APUNTA:
                  frecuencia_detectada =Analisis_De_Frecuencia(); 
                  // frecuencia_detectada = 0 SIN DETECCION / 1 LASER_APUNTANDO/ 2 DISPARO_DETECTADO
                  switch (frecuencia_detectada)
                    {
                      case SIN_DETECCION:
                        /* regresa a solicitud*/
                           menu_encendido_diana=INICIA_PROCESO_SOLICITUD;                   
                        break;

                      case LASER_APUNTANDO:
                        actual_time_solicitud=millis();
                        if (actual_time_solicitud > (previous_time_solicitud+50))
                            {
                              led_apunta++;
                              previous_time_solicitud=millis();
                              if (led_apunta>50)
                                {
                                  led_apunta=5;
                                }
                              menu_encendido_diana=ENCIENDE_APUNTANDO;
                              prende_leds=SI;
                            }       
                        break;
                      
                      case DISPARO_DETECTADO:
                        menu_encendido_diana=INICIA_PROCESO_DISPARO;
                        prende_leds=SI;
                        led_apunta=1;
                        previous_time=millis();
                        tiempo_final=millis();
                        break;

                    } //fin de switch frecuencia_detectada               
                  break;
              case INICIA_PROCESO_DISPARO:
                  previous_time=millis();
                  previous_time_solicitud=millis();
                  menu_encendido_diana=ENCIENDE_DISPARO;
                  prende_leds=SI;
                  destello=1;
                  break;


              case ENCIENDE_DISPARO:
                  if (prende_leds==SI)
                    {
                      pixels.setPixelColor(rand()%45,pixels.Color((rand()%5)*5,(rand()%5)*5,(rand()%5)*5));
                      pixels.setPixelColor(rand()%45,pixels.Color((rand()%5)*5,(rand()%5)*5,(rand()%5)*5));
                      pixels.setPixelColor(rand()%45,pixels.Color((rand()%5)*5,(rand()%5)*5,(rand()%5)*5));
                      pixels.setPixelColor(rand()%45,pixels.Color((rand()%5)*5,(rand()%5)*5,(rand()%5)*5));
                      pixels.setPixelColor(rand()%45,pixels.Color((rand()%5)*5,(rand()%5)*5,(rand()%5)*5));
                      pixels.setPixelColor(rand()%45,pixels.Color((rand()%5)*5,(rand()%5)*5,(rand()%5)*5));
                      pixels.setPixelColor(rand()%45,pixels.Color((rand()%5)*5,(rand()%5)*5,(rand()%5)*5));
                      pixels.setPixelColor(rand()%45,pixels.Color((rand()%5)*5,(rand()%5)*5,(rand()%5)*5));
                      pixels.show();
                      prende_leds=NO;
                    }
                    menu_encendido_diana=ESPERA_LEDS_DISPARO;
                  break;

              case ESPERA_LEDS_DISPARO:
                        actual_time_solicitud=millis();
                        if (actual_time_solicitud > (previous_time_solicitud+50))
                          {
                            destello++;
                            if (destello>100)
                              {
                                previous_time_solicitud=millis();
                                menu_encendido_diana=ESPERA_ENVIO_RESULTADOS;
                                pixels.clear();

                              }
                            menu_encendido_diana=ENCIENDE_DISPARO;
                            prende_leds=SI;
                          }       
                        break;

              case ESPERA_ENVIO_RESULTADOS:
                         actual_time_solicitud=millis();
                        if (actual_time_solicitud > (previous_time_solicitud+500))
                          {
                           menu_encendido_diana=ENVIA_RESULTADOS;
                           menu_transmision=INICIALIZA_TRANSMISION;
                          }   
                        break;  
              case ENVIA_RESULTADOS:
                  switch (menu_transmision)
                    {
                      case INICIALIZA_TRANSMISION:
                        /* code */
                        menu_transmision=PREPARA_PAQUETE_ENVIO;
                        break;

                      case PREPARA_PAQUETE_ENVIO:
                        datos_enviados.t=TIRO_ACTIVO; // TEST SI O TEST NO->VALIDO
                        datos_enviados.d=datos_locales_diana.d; //diana
                        datos_enviados.c=datos_locales_diana.c;   // color disparo
                        datos_enviados.p=datos_locales_diana.p;
                        datos_enviados.tiempo=tiempo_final-tiempo_inicial; // para regresar el tiempo
                        menu_transmision=ENVIA_PAQUETE;
                        intentos_envio=1;
                        break;

                      case ENVIA_PAQUETE:
                          esp_err_t result1 = esp_now_send(broadcastAddress4,(uint8_t *) &datos_enviados,sizeof(structura_mensaje));         
                          if (result1 == ESP_OK) 
                            {
                              Serial.println("Sent with success");
                              menu_encendido_diana=DATOS_ENVIADOS_OK;
                              menu_transmision=INICIALIZA_TRANSMISION;
                            }
                          else 
                            {
                              Serial.println("Error sending the data");
                              intentos_envio++;
                              menu_transmision=ESPERA_20MSEG_3INTENTOS;
                              previous_time=millis();
                            }
                          break;

                      case  ESPERA_20MSEG_3INTENTOS:
                        current_time=millis();
                        if (current_time>(previous_time+200))
                          {
                            if (intentos_envio<4)
                              {
                                menu_transmision=ENVIA_PAQUETE;
                                intentos_envio=intentos_envio+1;
                              }
                            else // manda mensaje de error
                              {
                                pixels.clear();
                                for (int i=2;i<=6;i++)
                                  {
                                    pixels.setPixelColor(i,pixels.Color(10,0,0));
                                  }
                                pixels.show();
                                menu_transmision=INICIALIZA_TRANSMISION;
                                estado_diana=APAGADO;
                              }
                          }
                          break;

                    } // fin switch menu_transmision


                case DATOS_ENVIADOS_OK:
                     estado_diana=APAGADO;
                     pixels.clear();
                     break;

            } // fin switch menu_encendido_diana
          break;

    }  // fin switch estado_diana

} // FIN  loop ---------------------------------------------------


int Analisis_De_Frecuencia()
{ uint16_t frecuencia_detectada=0;

  // SIN DETECCION 
  if (frecuencia_detectada<2000)  // no hace nada
    { 
      return SIN_DETECCION;
    }
  if ((frecuencia_detectada>=2000)&(frecuencia_detectada<=3000))
    {
      return LASER_APUNTANDO;
    }
  if ((frecuencia_detectada>=4000)&(frecuencia_detectada<=4500))
    {
      return DISPARO_DETECTADO;
    }
}


/*---------------------------------------------------------------------*/








/* ---------------------------------------------------------------------*/



/* ---------------------------------------------------------------------*/

/* ---------------------------------------------------------------------*/


/* ---------------------------------------------------------------------*/


/*
Programa principal Monitor
-  Funcion de escribir a monitor
- conexon de sistema de radio para botones



Software de Diana
- Estructura navegacion
- Oficina de recepcion
- Oficina de envio
- Funcion de leds
- Funcion de recepcion de disparo




Hardware tablero
- conexon de sistema de radio para botones
- conexion de esp con tablero y fuente
- conexion de led en tablero





Hardware Pistola


MENSAJES DIANA (ENCIENDE LED EN LINEA SUPERIOR DE DIANA)
VERDE =OK ROJO= FALLO 
*   fallo de add peer 
**   fall de prueba de envio a Monitor
***  FALLO DE INICIALIZACIO DE ESP-NOW
***  FALLO DE LECTURA DE LA MAC
****** fall la transmision al Monitor con el resultado del tiempo de disparo
*/



