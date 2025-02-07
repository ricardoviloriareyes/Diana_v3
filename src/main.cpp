#include <Arduino.h>
#include "nvs.h"
#include "nvs_flash.h"
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
//#include <RTClib.h>
#include <Adafruit_NeoPixel.h>
# include <Adafruit_BusIO_Register.h>
# include <Adafruit_I2CDevice.h>
# include <Adafruit_I2CRegister.h>
# include <Adafruit_SPIDevice.h>

/*

CD003 5 febrero 
Se debe revisar que llegue la figura seleccionada en monitor y el numero dedisparo

CD-001 2 febrero 2025
Integracion de  figuras

ED-890-OK 
ERROR DIANA 890 -22ene2025
- se adiciona el numero de paquete al paquete enviado
- se pone luz roja de disponible en equipo en envio exitoso
- se corren pruebas de velocidad y se comporta estable

*/
//24:62:AB:DC:AC:F4


// direcciones mac de ESTE RECEPTOR (PARA SER CAPTURADA EN EL  MONITOR)
uint8_t   broadcastAddress1[]={0x24,0x62,0xAB,0xDC,0xAC,0xF4};


//4c:eb:d6:75:48:30

//Monitor Central para regreso de resultados
uint8_t   broadcastAddressMonitor[]={0x4C,0xEB,0xD6,0x75,0x48,0x30}; 


// informacion para definicion de colores solamente,
/*
const uint16_t colors_global[] = 
  { 
     matrix_arriba.Color(150,150,150),

     matrix_arriba.Color(255,50,  0 ), 
     matrix_arriba.Color( 0, 255,  0 ),
     matrix_arriba.Color( 0,   0, 255),
     matrix_arriba.Color( 186, 126,  66),
     matrix_arriba.Color(255,92,0),
 
  };

// Colores del texto en tablero
#define WHITE 0
#define RED   1
#define GREEN 2
#define BLUE  3
#define BLUE_LIGHT 4
#define ORANGE 5


entra en tiro, se revisa si que tiene que mostrar
si muestra numeros, entonces se inicia figuras=numeros
ciclo=1, se selecciona el numero
cada numero debe tener diferentes colores para cada ciclo
al terminar un ciclo debe regresar a uno hasta detectar el apunta
en apunta la reglon 5 parpadea aleatorio y mucho mas rapido y puede ser el ciclo doble de rapido
CUANDO LLEGA SE SELECCIONA LA FIGURA A MOSTRAR
NUMERO DE CICLOS A MOSTRAR SEGUN LA FIGURA


AL LLEGAR EL TIRO RECIBE EL COLOR, LA FIGURA Y EL NUMERO DE TIRO

AL ENTRAR EL DESPLIEGUE SE REVISA:

LA FIGURA
    FIGURA SPACEINVADER

    FIGURA NUMERO
           SE ASIGNAN MATRICES PARA CICLOS
              1,2,3,4,5
           CASE CICLO
                1. SE MUESTRA MATRIZ AGINADA
                   FOR  DE 0 A 40
                   {
                   se asgina  color para led
                        {
                          black l1=0, l2=0,l3=0;
                          white  led1=255 led2=255 led3255;
                        }
  set.pixelcolor (pixel,led1,led2,led3);
                      
                   SE ASIGNA COLOR}
                   
                2: MATRIZ ASIGNADA
                3: MATRIZ ASIGNADA
                4. MATRIZ ASIGNADA
                5: MATRIZ ASIGNADA




do case figura
   
   numero
          do case numero_mostrado
             UNO
             MATRIZMOSTRAR=MATRIZSELECCIONADA A LA RECEPCION


for pixel=0 a 39
{
  do case color
    {
            black l1=0, l2=0,l3=0;
            white  led1=255 led2=255 led3255;
    }
  set.pixelcolor (pixel,led1,led2,led3);


}


*/


#define NO 0
#define SI 1

// Colores del texto en tablero
#define RED   1
#define GREEN 2
#define BLUE  3

          
// variables de control de tiempo
volatile unsigned long previous_time_reenvio,current_time_reenvio;

// varables para envio de datos a diana
#define STANDBYE 0
#define PREPARA_PAQUETE_ENVIO  1
#define ENVIA_PAQUETE          2
#define PAUSA_REENVIO 3
int intentos_envio=0;
int8_t flujo_de_envio = STANDBYE;

// logica del proceso del disparo

#define INICIA_STANDBYE 0
#define INICIALIZA_TIEMPO 1
#define MONITOREA_DISPARO 2
#define ENVIA_RESULTADOS 3
#define FINALIZA_REINICIA_VARIBLES 4
int8_t case_proceso_encendido = INICIA_STANDBYE;

// variables de tipo de figura CD001
#define BICHOS 1
#define CIRCULOS 2
#define NUMEROS 3

//ESPERA
#define UNO 1
#define DOS 2
#define TRES 3
#define CUATRO 4
#define CINCO 5
#define SEIS 6
#define SIETE 7

//APUNTA
#define OCHO 8
#define NUEVE 9


//NUMEROS
#define TREINTA 30
#define PARPADEA1 50
#define PARPADEA2 51
int8_t  numero_calculado_de_tiro=UNO;

int case_estado_espera_circulos = UNO;
int case_estado_apunta_circulos = OCHO;

int case_estado_espera_bichos= UNO;
int case_estado_apunta_bichos= OCHO;

int case_estado_espera_numeros = TREINTA;
int case_estado_apunta_numeros =OCHO;


// Valores posibles de variables de matriz led, no se utilizan solo referencia para asignacion
#define ATRAS 0
#define TINTA 1
#define BASE 2
#define RESALTA 3

//
uint8_t color_atras_1;
uint8_t color_atras_2;
uint8_t color_atras_3;
uint8_t color_tinta_1;
uint8_t color_tinta_2; 
uint8_t color_tinta_3;
uint8_t color_base_1;
uint8_t color_base_2;
uint8_t color_base_3;
uint8_t color_resalta_1;
uint8_t color_resalta_2;
uint8_t color_resalta_3;

// Matriz global de asignacion 
uint8_t Vector_Matriz_Led[40] = {0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9};

//MATRIZ CIRCULOS 
/*apunta*/
//uint8_t Vector_Matriz_Led[40] = {0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9};
  uint8_t Vector_Circulo_9 [40] = {1,1,1,1,1,1,1,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,3,3,1,0,1};
  uint8_t Vector_Circulo_8 [40] = {1,1,1,1,1,1,1,1,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,3,3,0,1,0};
//espera
  uint8_t Vector_Circulo_7 [40] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,3,1,1,1};
  uint8_t Vector_Circulo_6 [40] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,1,1,1};
  uint8_t Vector_Circulo_5 [40] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,1,1,1,1,2,1,1,2,1,1};
  uint8_t Vector_Circulo_4 [40] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,1,1,1,1,2,1,1,2,1,1};
  uint8_t Vector_Circulo_3 [40] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,1,1,2,1,1,1,1,2,1,1,2,1,1,1,1,2,1};
  uint8_t Vector_Circulo_2 [40] = {1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,1,1,1,1,1,1,2,2,1,1,1,1,1,1,2,2,1,1,1,1,1,1,2};
  uint8_t Vector_Circulo_1 [40] = {2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};


// MATRIZ BICHOS
/*apunta*/
//uint8_t Vector_Matriz_Led[40] = {0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9};
  uint8_t Vector_Bichos_9[40]   = {0,0,1,1,1,1,1,0,0,1,2,1,2,1,0,0,0,0,1,1,1,1,1,0,0,1,0,1,0,1,0,0,0,0,2,3,3,2,0,0};
  uint8_t Vector_Bichos_8[40]   = {0,0,1,1,1,1,1,0,0,1,2,1,2,1,0,0,0,0,1,1,1,1,1,0,0,1,0,1,0,1,0,0,0,0,2,3,3,2,0,0};
/* espera*/
  uint8_t Vector_Bichos_6[40]   = {0,0,1,1,1,1,1,0,0,1,2,1,2,1,0,0,0,0,1,1,1,1,1,0,0,1,0,1,0,1,0,0,0,0,2,3,3,2,0,0};
  uint8_t Vector_Bichos_5[40]   = {0,1,1,1,1,1,0,0,0,0,1,2,1,2,1,0,0,1,1,1,1,1,0,0,0,0,1,0,1,0,1,0,0,0,0,3,3,0,0,0};
  uint8_t Vector_Bichos_4[40]   = {0,1,1,1,1,1,0,0,0,0,1,2,1,2,1,0,0,1,1,1,1,1,0,0,0,0,1,0,1,0,1,0,0,0,2,3,3,2,0,0};
  uint8_t Vector_Bichos_3[40]   = {0,1,1,1,1,1,0,0,0,0,1,2,1,2,1,0,0,1,1,1,1,1,0,0,0,0,1,0,1,0,1,0,0,0,0,3,3,0,0,0};
  uint8_t Vector_Bichos_2[40]   = {0,0,1,1,1,1,1,0,0,1,2,1,2,1,0,0,0,0,1,1,1,1,1,0,0,1,0,1,0,1,0,0,0,0,2,3,3,2,0,0};
  uint8_t Vector_Bichos_1[40]   = {0,0,0,1,1,1,1,1,1,2,1,2,1,0,0,0,0,0,0,1,1,1,1,1,1,0,1,0,1,0,0,0,0,0,0,3,3,0,0,0};

// MATRIZ NUMEROS
//uint8_t Vector_Matriz_Led[40]         = {0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9};
  uint8_t Vector_Numeros_1[40]          = {0,0,0,1,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,1,1,0,0};
  uint8_t Vector_Numeros_2[40]          = {0,0,1,1,1,1,0,0,0,0,1,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0};
  uint8_t Vector_Numeros_3[40]          = {0,0,1,1,1,1,0,0,0,0,1,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,1,0,0,0,0,0,0,0,1,1,1,1,0,0};
  uint8_t Vector_Numeros_4[40]          = {0,0,1,0,0,1,0,0,0,0,1,0,0,1,0,0,0,0,1,1,1,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0};
  uint8_t Vector_Numeros_5[40]          = {0,0,1,1,1,1,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,0,1,0,0,0,0,0,0,0,1,1,1,1,0,0};
  uint8_t Vector_Numeros_6[40]          = {0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,0,1,0,0,1,0,0,0,0,1,1,1,1,0,0};
  uint8_t Vector_Numeros_7[40]          = {0,0,1,1,1,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0};
  uint8_t Vector_Numeros_8[40]          = {0,0,1,1,1,1,0,0,0,0,1,0,0,1,0,0,0,0,1,1,1,1,0,0,0,0,1,0,0,1,0,0,0,0,1,1,1,1,0,0};
  uint8_t Vector_Numeros_9[40]          = {0,0,1,1,1,1,0,0,0,0,1,0,0,1,0,0,0,0,1,1,1,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0};
  uint8_t Vector_Numeros_10[40]         = {1,1,0,0,0,1,1,1,1,0,1,0,0,0,1,0,0,1,0,0,0,1,0,1,1,0,1,0,0,0,1,0,1,1,1,0,0,1,1,1};
  uint8_t Vector_Numeros_11[40]         = {1,1,0,0,0,1,1,1,1,0,0,0,0,0,1,0,0,1,0,0,0,1,1,1,0,0,1,0,0,0,1,0,1,1,1,0,0,1,1,1};
  uint8_t Vector_Numeros_12[40]         = {1,1,0,0,0,1,1,1,1,0,0,0,0,0,1,0,0,1,0,0,0,1,1,1,0,0,1,0,0,0,1,0,1,1,1,0,0,1,1,1};
  uint8_t Vector_Numeros_13[40]         = {1,1,0,0,0,1,1,1,1,0,0,0,0,0,1,0,0,1,0,0,0,1,1,1,1,0,0,0,0,0,1,0,1,1,1,0,0,1,1,1};
  uint8_t Vector_Numeros_14[40]         = {1,1,0,0,0,1,1,1,0,0,1,0,0,0,1,0,0,1,0,0,0,1,1,1,1,0,0,0,0,0,1,0,1,1,1,0,0,1,1,1};
  uint8_t Vector_Numeros_15[40]         = {1,1,0,0,0,1,1,1,0,0,1,0,0,0,1,0,0,1,0,0,0,1,1,1,1,0,0,0,0,0,1,0,1,1,1,0,0,1,1,1};
  uint8_t Vector_Numeros_16[40]         = {1,1,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,1,0,0,0,1,1,1,1,0,1,0,0,0,1,0,1,1,1,0,0,1,1,1};
  uint8_t Vector_Numeros_17[40]         = {1,1,0,0,0,1,1,1,1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,1,1,0,0,0,0,0,1,0,1,1,1,0,0,0,0,1};
  uint8_t Vector_Numeros_18[40]         = {1,1,0,0,0,1,1,1,1,0,1,0,0,0,1,0,0,1,0,0,0,1,1,1,1,0,1,0,0,0,1,0,1,1,1,0,0,1,1,1};
  uint8_t Vector_Numeros_19[40]         = {1,1,0,0,0,1,1,1,1,0,1,0,0,0,1,0,0,1,0,0,0,1,1,1,1,0,0,0,0,0,1,0,1,1,1,0,0,0,0,1};
  uint8_t Vector_Numeros_20[40]         = {1,1,1,0,0,1,1,1,1,0,1,0,0,1,0,0,1,1,1,0,0,1,0,1,1,0,1,0,0,0,0,1,1,1,1,0,0,1,1,1};
  uint8_t Vector_Numeros_21[40]         = {1,1,1,0,0,0,1,0,0,1,0,0,0,1,0,0,1,1,1,0,0,0,1,0,0,1,0,0,0,0,0,1,1,1,1,0,0,0,1,0};
  uint8_t Vector_Numeros_22[40]         = {1,1,1,0,0,1,1,1,1,0,0,0,0,1,0,0,1,1,1,0,0,1,1,1,0,0,1,0,0,0,0,1,1,1,1,0,0,1,1,1};
  uint8_t Vector_Numeros_23[40]         = {1,1,1,0,0,1,1,1,0,0,0,0,0,1,0,0,1,1,1,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0,0,1,1,1};
  uint8_t Vector_Numeros_24[40]         = {1,1,1,0,0,1,0,1,1,0,1,0,0,1,0,0,1,1,1,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0,0,1};
  uint8_t Vector_Numeros_25[40]         = {1,1,1,0,0,1,1,1,0,0,1,0,0,1,0,0,1,1,1,0,0,1,1,1,1,0,1,0,0,0,0,1,1,1,1,0,0,1,1,1};
  uint8_t Vector_Numeros_26[40]         = {1,1,1,0,0,1,0,0,0,0,1,0,0,1,0,0,1,1,1,0,0,1,1,1,1,0,1,0,0,0,0,1,1,1,1,0,0,1,1,1};
  uint8_t Vector_Numeros_27[40]         = {1,1,1,0,0,1,1,1,1,0,0,0,0,1,0,0,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0,0,1};
  uint8_t Vector_Numeros_28[40]         = {1,1,1,0,0,1,1,1,1,0,1,0,0,1,0,0,1,1,1,0,0,1,1,1,1,0,1,0,0,0,0,1,1,1,1,0,0,1,1,1};
  uint8_t Vector_Numeros_29[40]         = {1,1,1,0,0,1,1,1,1,0,1,0,0,1,0,0,1,1,1,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0,0,1};
  uint8_t Vector_Numeros_30[40]         = {1,1,1,0,0,1,1,1,1,0,1,0,0,1,0,0,1,1,1,0,0,1,0,1,1,0,1,0,0,1,0,0,1,1,1,0,0,1,1,1};
  uint8_t Vector_Numeros_PARPADEA1[40]  = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,0,0,0,0,3,2,2,3,0,0};
  uint8_t Vector_Numeros_PARPADEA2[40]  = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,0,0,0,0,2,3,3,2,0,0};




#define APAGADO   0  //CD001
#define ENCENDIDO 1 

#define SIN_CAMBIO_ESTADO_RELOJ 0
#define DETECTA_CAMBIO_ESTADO_RELOJ 1
int reloj_lento =ENCENDIDO;
int reloj_rapido=ENCENDIDO;
int actualiza_display_lento =SI;
int actualiza_display_rapido =SI;


// estructura de envio de datos al compañero(peer)
typedef struct structura_mensaje
{
int n; //Paquete
int t; //tipo de tiro TEST 0,TIRO_ACTIVO 1, READY_JUGADOR 2
int d; //diana
int c; //color
int p; //propietario del
float tiempo;
int f; // Grupo de figuras a mostrar CD001
int s; //no de tiro, sirve para las figuras numeros CD001
} structura_mensaje;

structura_mensaje datos_enviados;
structura_mensaje datos_recibidos;
structura_mensaje datos_locales_diana;  // para usar en diana


#define TEST 0
#define TIRO_ACTIVO 1
#define READY_JUGADOR 2
int tipo_tiro_recibido= TIRO_ACTIVO;
int no_paquete=2000;

// variable tipo esp_now_info_t para almacenar informacion del compañero
esp_now_peer_info_t peerInfo;


// definicion de io-esp32
const int pin_pulsos = 2;  // recepcion de pulsos par calculo de frecuencia
const int pin_leds = 16; // pin asignado de pcb del  diagrama de electronica diana 3.1
const int button_wakeup =33;

// variable para guardar el pulso de wakeup
int buttonstate=0;

// definicion de tira led
uint8_t led_inicio=0;
#define NUMPIXELS 31
Adafruit_NeoPixel strip(NUMPIXELS,pin_leds,NEO_RGB+NEO_KHZ800);
int prende_leds=SI;


/* -------------------PASO 0 INICIO DE ESP-NOW ------------------*/
void Inicia_ESP_NOW()
{
  Serial.println("Inicia ESPNOW");
  if (esp_now_init() != ESP_OK)
    {
      Serial.println("Error initializing ESP-NOW");
      strip.setPixelColor(0,strip.Color(250,0,0));
      strip.show();
      return;
    }
  else
  {
      Serial.println("initializing ESP-NOW  OK");
      strip.setPixelColor(0,strip.Color(0,0,250));
      strip.show();
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
    }
}



/* ----------------PASO 2 ADD PEER-----------------------------------------------------*/
// firma peer de
void Peer_Monitor_En_Linea()
{
  // register peer1
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  memcpy(peerInfo.peer_addr, broadcastAddressMonitor, 6);
  delay(500);
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
     {
        strip.setPixelColor(1,strip.Color(250,0,0));
        strip.show();
        Serial.println("Failed to add peer Monitor");
        return;
     }
   else
      { 
        strip.setPixelColor(1,strip.Color(0,0,250));
        strip.show();
        Serial.println("Add Peer Monitor OK");

      }
}


/* ------------------PASO 3 PRUEBA DE CONEXION ---------------------------------------------------*/
void Test_Conexion_Diana()
{ 
  // DATOS A ENVIAR  
    if (no_paquete>=2101)
      {no_paquete=2000;}

    datos_enviados.n=no_paquete;
    datos_enviados.t=TEST;
    datos_enviados.c=1;   //SE MANDA led_1 PARA TEST
    datos_enviados.p=0; // JUGADOR 0 NO ACTIVO
    datos_enviados.tiempo=0; // para regresar el tiempo
    datos_enviados.f=0; // FIGURA - solo por mandar dato en el campo
    datos_enviados.s=0;  // SHOT - NUMERO DE TIRO, SOLO DE RELLENO

  // monitor
    esp_err_t result4 = esp_now_send(broadcastAddressMonitor, (uint8_t *) &datos_enviados, sizeof(structura_mensaje));
    delay(500);
    if (result4 == ESP_OK) 
     {
      strip.setPixelColor(2,strip.Color(0,0,250));
      strip.show();
      Serial.println("TEST-Envio a Monitor OK");
     }

    else 
     {
      strip.setPixelColor(2,strip.Color(250,0,0));
      strip.show();
      Serial.println("TEST-Error enviando a Monitor");
     }
}

 /* -------------VARIABLES DE RECEPCION DE DATOS---------------------------------*/

int8_t led_2=0;
int8_t led_1=0;
int8_t led_3=0;


//estado de diana
#define APAGADO 0
#define ENCENDIDO 1
int case_estado_diana =APAGADO;



/* variables para analisis de frecuencia */
ulong frecuencia_disparo=10000;
ulong frecuencia_apunta=5000;

ulong frecuencia_disparo_tiro_verde=1300 ; //2500;
ulong frecuencia_apunta_tiro_verde= 800 ; // 2000;

ulong frecuencia_disparo_tiro_azul =1300;
ulong frecuencia_apunta_tiro_azul= 800;


/* ----------------- ENVIO DE DATOS -----------------------------*/
int enviar_datos=NO;
String success;


// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) 
{
  delay(250);
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status ==0)
  {
    success = "Delivery Success :)";
    // reinicia el equipo para esperar un nueva solicitud
    //flujo_de_envio=STANDBYE;
    //case_proceso_encendido = INICIA_STANDBYE;
    //case_estado_diana=APAGADO;
    //captura_tiempo_inicial=SI;
  }
  else
  {
    success = "Delivery Fail :(";
  }
}


// tiempo de duracion del tiro
volatile unsigned long tiempo_inicia_tiro=0;
volatile unsigned long tiempo_termina_tiro=0;
int activa_envio_de_resultados=NO;


//variables para deteccion de disparo en analisis de frecuencia
#define SIN_DETECCION 0
#define LASER_APUNTANDO 1
#define DISPARO_DETECTADO 2
//int8_t estado_detector = SIN_DETECCION;


//----
volatile unsigned long tiempo_inicial_muestreo=0;
volatile unsigned long tiempo_actual_muestreo =0;
volatile unsigned long tiempo_inicial_leds=0;
volatile unsigned long tiempo_actual_leds=0;
volatile unsigned long periodo_muestreo_mseg =10;
volatile unsigned long pulsos=0;
volatile unsigned long muestra[10]={0,0,0,0,0,0,0,0,0,0};
volatile unsigned long promedio_muestreo=0;
volatile unsigned long nueva_frecuencia=0;
volatile unsigned long acumulado=0;
int8_t rango_actual=0;
int8_t cambio_la_frecuencia =NO;
int8_t posicion_nueva       =SIN_DETECCION;
int captura_tiempo_inicial=SI;


// contadores para relojes de encendido y apagado tiras con figuras

volatile unsigned long tiempo_inicial_reloj_lento=0;
volatile unsigned long tiempo_actual_reloj_lento=0;
volatile unsigned long tiempo_inicial_reloj_rapido=0;
volatile unsigned long tiempo_actual_reloj_rapido=0;



/* -------------------- RECEPCION DE DATOS -----------------------*/
// Callback when data is received
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) 
{
  memcpy(&datos_recibidos, incomingData, sizeof(datos_recibidos));
  //informacion de longuitud de paquete
  Serial.println("No. de paquete recibido :"+String(no_paquete));
  Serial.print("Bytes received: ");
  Serial.println(len);
 
  char macStr[18];
  Serial.print("Packet received from: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
  
  Serial.println("Tiro activo = "+String(datos_recibidos.t));

  // se guada tipo de tiro recibido para validar el rady de loc jugadores de contadores descendentes
  tipo_tiro_recibido=datos_recibidos.t;

  // SE REGRESA JUGADOR READY PARA EVITAR ACUMULAR TIROS EN JUGADOR

  if (datos_recibidos.t>=TIRO_ACTIVO)  // JUGADOR READY =2 TIRO ACTIVO =1
    { 
      Serial.println("RECEPCION PAQUETE"+String(datos_recibidos.n));

      //Serial.println("datos_recibidos.no_paquete ="+String(datos_recibidos.n));
      Serial.println("datos_recibidos.tipo_tiro ="+String(datos_recibidos.t));
      //Serial.println("datos_recibidos.color ="+String(datos_recibidos.c));
      //Serial.println("datos_recibidos.diana ="+String(datos_recibidos.d));
      //Serial.println("datos_recibidos.propietario ="+String(datos_recibidos.p));
      //Serial.println("datos_recibidos.tiempo ="+String(datos_recibidos.tiempo));      
      //Serial.println("datos_recibidos.figura ="+String(datos_recibidos.f));
      //Serial.println("datos_recibidos.shot ="+String(datos_recibidos.s));

      datos_locales_diana.n= datos_recibidos.n;
      datos_locales_diana.t =datos_recibidos.t;
      datos_locales_diana.c =datos_recibidos.c;
      datos_locales_diana.d =datos_recibidos.d;
      datos_locales_diana.p =datos_recibidos.p;
      datos_locales_diana.tiempo=0.00;
      datos_locales_diana.f =datos_recibidos.f; // figura CD001
      datos_locales_diana.s =datos_recibidos.s; // no de disparo CD001
      numero_calculado_de_tiro=31-datos_locales_diana.s;

      no_paquete++;
      pulsos=0;
      switch (datos_locales_diana.c)
        {
          case GREEN:
            //Serial.println("Programa color verde");
            led_1=250;
            led_2=0;
            led_3=0;
            frecuencia_apunta=frecuencia_apunta_tiro_verde;
            frecuencia_disparo=frecuencia_disparo_tiro_verde;
            break;

          case BLUE:
            //Serial.println("Programa color azul");
            led_1=0;
            led_2=0;
            led_3=250;
            frecuencia_apunta=frecuencia_apunta_tiro_azul;
            frecuencia_disparo=frecuencia_disparo_tiro_azul;
            break;

        }
      case_estado_diana=ENCENDIDO;
      Serial.println("Estado-diana= ENCENDIDO");


    }
}












//declaracion de funciones
void Analisis_De_Frecuencia2();
void Califica_La_Frecuencia();
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len);
void Test_Conexion_Diana();
void readMacAddress();
void Inicia_ESP_NOW();
void Peer_Monitor_En_Linea();
void Enciende_Tira();
void Espera_Tira_Leds();
void Apunta_Tira_Leds();
void Disparo_Tira_Leds();
void Envia_Resultados_Al_Monitor();

void Enciende_Tira_Figuras();
  void Espera_Tira_Leds_Figuras();
    int Reloj_Lento();
    void Asigna_Leds_Espera_Bichos();
    void Asigna_Leds_Espera_Circulos();
    void Asigna_Leds_Espera_Numeros();



  void Apunta_Tira_Leds_Figuras();
    int Reloj_Rapido();
    void Asigna_Leds_Apunta_Bichos();
    void Asigna_Leds_Apunta_Circulos();
    void Asigna_Leds_Apunta_Numeros();


  void Disparo_Tira_Leds_Figuras();

//Comun
    void  Enciende_Matriz_Led();






int i;


void Cuenta_Pulso()
{
 pulsos++;
}


/* -----------------------INICIO    S E T U P --------------------------------*/

void setup() 
{
  Serial.println("DIANA");
  // definicion en puerto IO16 (pin_pulsos) la lectura de interrupción para calculo de frecuencia 
  pinMode(pin_pulsos,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin_pulsos),Cuenta_Pulso,RISING);

  // inicia puerto de salida de leds
  pinMode (pin_leds,OUTPUT);
  digitalWrite(pin_leds,LOW);
  pinMode (button_wakeup,INPUT);

  /* 
  checar condicion de automatica de sleepmode
  cuando pasen 3 minutos sin comunicacion
  debe de ponerse en modo dormir
  el wakeup se dara por la señal de pin button_wakeup
  1Dic2024 no esta implementado en software para la versión 3.1 aun que la electronica ya esta lista 
  */

  // inicia Neopixel strip
  strip.begin();
  strip.show();
  strip.setBrightness(100);

  // para iniciar el flasheo de la tira
  tiempo_inicial_leds=millis();


  // inicia comunicacion serial
  Serial.begin(115200);

  // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);

  // escribe direccion mac
  Serial.print("[DEFAULT] ESP32 Board MAC Address: ");
  readMacAddress();
  Serial.println("MAC: "+ WiFi.macAddress());


  // Init ESP-NOW
  Inicia_ESP_NOW();
  

  // sincronizacin de dianas (peer)
  Peer_Monitor_En_Linea();

  // test en linea MOnitor
  Test_Conexion_Diana();

  // incio de medidor de tiempo muestreo
  tiempo_inicial_muestreo=millis();
  //Serial.println(tiempo_inicial_muestreo);

  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  esp_now_register_send_cb(OnDataSent);



} // fin setup

/* ----------------------  F I N    S E T U P -------------------------*/

/* ------------------------inicio loop ----------------------------*/
void loop() 
{ 
  // para switchear los parpadeos de las figuras 
  tiempo_actual_reloj_lento=millis();
  tiempo_actual_reloj_rapido=millis();

  switch (case_estado_diana)
  {
    case APAGADO:
      //Serial.print(".");
      /* code */
      break;

    case ENCENDIDO:
      //valida di hay datos a enviar
      if (enviar_datos==SI) // se habilita en Envia_resultados_al_monitor/ENVIA_PAQUETE 
        {
          Serial.println("ENVIO PAQUETE :"+String(datos_enviados.n));
          esp_err_t result = esp_now_send(broadcastAddressMonitor,(uint8_t *) &datos_enviados,sizeof(structura_mensaje)); 
          delay(250);        
          if (result == ESP_OK) 
            {
              case_proceso_encendido=FINALIZA_REINICIA_VARIBLES;
              Serial.println("Envio Paquete OK, intento : "+String(intentos_envio));
              Serial.println("Tiempo disparo (seg)   : "+String(datos_enviados.tiempo));
              enviar_datos=NO;
              strip.clear();
              strip.show();
              for (int i=0; i<=3;++i)
                {strip.setPixelColor(i,strip.Color(0,250,0));
                }
              strip.show();
            }
          else 
            {
              Serial.println("Error envio : "+String(intentos_envio));
              intentos_envio++;
              flujo_de_envio=PAUSA_REENVIO;
              strip.clear();
              strip.show();
              strip.setPixelColor(1,strip.Color(0,250,0));
              strip.setPixelColor(1,strip.Color(1,250,0));
              strip.setPixelColor(1,strip.Color(2,250,0));
              strip.show();
              previous_time_reenvio=millis();
            }
          Serial.println("");
          Serial.println("");
        }

      // proceso encendido
      switch (case_proceso_encendido)
        {
            case INICIA_STANDBYE:          
                  Serial.println("reinicia frecuencias");
                  for (int i=0; i<=9;++i)
                  {
                    muestra[i]=0;
                  }
                 case_proceso_encendido=INICIALIZA_TIEMPO;
                 break;

            case INICIALIZA_TIEMPO:
                  tiempo_inicia_tiro=millis(); // incia tiempo del tiro
                  tiempo_inicial_leds=millis();
                  strip.clear();
                  Serial.println("Inicializa tiempo (mseg): "+String(tiempo_inicia_tiro));
                  case_proceso_encendido=MONITOREA_DISPARO;
                  break;

            case MONITOREA_DISPARO:
                  Analisis_De_Frecuencia2();
                  Califica_La_Frecuencia();
                  Enciende_Tira_Figuras (); //ED001 SE CAMBIO Enciende_Tira();
                  if (posicion_nueva==DISPARO_DETECTADO) //posicion nueva es modificado en califica frecuencia
                  {
                    Serial.println("-->posicion_nueva=DISPARO_DETECTADO");
                    case_proceso_encendido=ENVIA_RESULTADOS;
                    activa_envio_de_resultados=SI;
                    flujo_de_envio=PREPARA_PAQUETE_ENVIO;
                    posicion_nueva=SIN_DETECCION;
                  }
                  break;

            case ENVIA_RESULTADOS:
                  Envia_Resultados_Al_Monitor();
                  break;

            case FINALIZA_REINICIA_VARIBLES:
                  // finaliza envio
                  Serial.println("APAGA DIANA");
                  case_estado_diana=APAGADO;
                  pulsos=0;
                  captura_tiempo_inicial=SI;;
                  flujo_de_envio=STANDBYE;

                  // reinicia varibles de menus
                  case_proceso_encendido = INICIA_STANDBYE;
                  prende_leds=SI;
                  led_inicio=0;

                  // tiempo de duracion del tiro
                  tiempo_inicia_tiro=0;
                  tiempo_termina_tiro=0;
                  activa_envio_de_resultados=NO;

                  //inicializa variables de freceuncia
                  rango_actual=0;
                  cambio_la_frecuencia =NO;
                  posicion_nueva       =SIN_DETECCION;
                  captura_tiempo_inicial=SI;
                  promedio_muestreo=0;
                  nueva_frecuencia=0;
                  acumulado=0;

                  // reinicia el acumulado y rangos
                  frecuencia_disparo=10000;
                  frecuencia_apunta=5000;
                  break;

        } //fin case case_proceso_encendido
      break;
  } // fin de switch case_estado_diana
}
// FIN  loop ---------------------------------------------------



/*-----------------------------------------------*/
void 
Envia_Resultados_Al_Monitor()
{ Serial.println("Envia resultados al monitor") ;              
  switch (flujo_de_envio)
    {
      case STANDBYE:
        /* solo espera */
        break;
      case PREPARA_PAQUETE_ENVIO:
        datos_enviados.n =datos_locales_diana.n;
        datos_enviados.t=TIRO_ACTIVO; // TEST SI O TEST NO->VALIDO
        // regresa el tipo del tiro recibido
        datos_enviados.t=tipo_tiro_recibido;
        datos_enviados.d=datos_locales_diana.d; //diana
        datos_enviados.c=datos_locales_diana.c;   // color disparo
        datos_enviados.p=datos_locales_diana.p;
        // se envian segundos
        datos_enviados.tiempo=(tiempo_termina_tiro-tiempo_inicia_tiro)/1000.0; // para regresar el tiempo
        intentos_envio=1;
        flujo_de_envio=ENVIA_PAQUETE;
        break;
      case ENVIA_PAQUETE: // se pone a primer nivel por deficion del la función
        enviar_datos=SI;
        break;
      case PAUSA_REENVIO:
        current_time_reenvio=millis();
        if ((current_time_reenvio-previous_time_reenvio)>250)
          {
            if (intentos_envio<4)
              {
                flujo_de_envio=ENVIA_PAQUETE;
              }
            else // manda mensaje de error
              {
                strip.clear();
                for (int i=2;i<=32;i++)
                  {
                    strip.setPixelColor(i,strip.Color(0,250,0));
                  }
                strip.show();
                Serial.println("APAGA DIANA en PAUSA REENVIO");
                case_estado_diana=APAGADO;
                captura_tiempo_inicial=NO;
              }
          }
          break;
      } //fin switch flujo_de_envio
}





/* ----------------------------------------------------------*/
void Enciende_Tira ()
{ 
  tiempo_actual_leds=millis();
  switch (posicion_nueva)
  {
    case SIN_DETECCION:   // SIN_DETECCION
      if ((tiempo_actual_leds-tiempo_inicial_leds)>100)
        { // cambia posicion de leds
          led_inicio=led_inicio+1;
          //Serial.println("cambio de led :"+String(led_inicio));
          prende_leds=SI;
          if (led_inicio>8) { led_inicio=0;}
          // reinicia periodo
          tiempo_inicial_leds=millis();
          // enciende
          Espera_Tira_Leds(); //CD001 DEBE SACARSE DEL IF YA QUE LA FUNCIONLED INICIO YA NO SIRVE
        }
      break;
    case LASER_APUNTANDO:   // LASER APUNTANDO
      if ((tiempo_actual_leds-tiempo_inicial_leds)>30)
        { // cambia posicion de leds
          led_inicio=led_inicio+1;
          //Serial.println("cambio de led apuntando :"+String(led_inicio));
          prende_leds=SI;
          if (led_inicio>8)
              { led_inicio=0;
              }
          // reinicia periodo
          tiempo_inicial_leds=millis();
          // enciende
          Apunta_Tira_Leds();
        }
        break;
    case DISPARO_DETECTADO:   // DISPARO_DETECTADO
        Serial.print("DISPARO DETECTADO / frecuencia : ");
        Serial.println(nueva_frecuencia);
        Serial.println("Contador final (mseg) : "+String(tiempo_termina_tiro));
        for (int i=1;i<=4;i++)
          {
          Disparo_Tira_Leds();
          }
        break;
  }


  /* nota: se quito la opcion de validar que disparo viniera
   de apuntar, ya que a veces  no lo detecta si el gatillo se oprime
   muy rapido y perderia el tiro, asi que pueden hacer trampa
   dejando el gatillo oprimido
   */
}


/* --------------------------------------------------------*/
void Analisis_De_Frecuencia2()
{
  tiempo_actual_muestreo=millis();
  // define el rango de tiempo de cada rango de muestreo
  if ((tiempo_actual_muestreo-tiempo_inicial_muestreo)>periodo_muestreo_mseg) //espacio para obtener la muestra
  {
    /*
    La frecuencia se obtiene con:
    10 muestra de 10 msseg cada una
    El promedio nos da la frecuencia de 10 mseg
    se tiene que multiplicar por 100 para sacar la muestra 
    en frecuencia () por segundo, pero el circuito
    tiene un divisior de decadas por lo que se tiene que multipicar
    por 10 adicional, por eso se escribe pulsos*mil
    */
    

    muestra[rango_actual]=pulsos*1000; 
    //Serial.print(" Pulsos rango ["+String(rango_actual)+"] : ");
    //Serial.println(pulsos*10); 
    if (rango_actual<9)
      { // incrementa el NO de muestra y reinicia periodo de muestreo
        rango_actual++;  
        tiempo_inicial_muestreo=millis();
        pulsos=0;
      }
    else // igual a 9 
      { 
        //calcula el promedio del muestreo
        for (int i=0;i<=9;i++)  { acumulado=acumulado+muestra[i];}
        promedio_muestreo=(int)acumulado/10;
        //Serial.println("Nueva frecuencia : "+String(promedio_muestreo));
        // reasigna la frecuencia
        nueva_frecuencia=promedio_muestreo;
        // reinicia el acumulado y rangos
        acumulado=0;
        rango_actual=0;
        // habilita revision del estado de la frecuencia
        cambio_la_frecuencia=SI;
      }
  }
}
      
/* ----------------------------------------------------- */
void Califica_La_Frecuencia()
{        
  if (cambio_la_frecuencia==SI)
    { 
      //Serial.println("Entro a Califica Frecuencia");
      // entre freceuncia de disparo y disparo+500
      if ((nueva_frecuencia>frecuencia_disparo)&&(nueva_frecuencia<frecuencia_disparo+500)) // led_1 1300
       {
        posicion_nueva=DISPARO_DETECTADO;
        Serial.println("Disparo detectado");

        // prepara variables para envio de datos a Monitor
        activa_envio_de_resultados=SI;
        flujo_de_envio=PREPARA_PAQUETE_ENVIO;
        case_proceso_encendido=ENVIA_RESULTADOS;

        // marca el tiempo final del tiro
        tiempo_termina_tiro=millis();

       }
      else
       {
        if ((nueva_frecuencia>frecuencia_apunta)&&(nueva_frecuencia>frecuencia_apunta)) // led_1 700
         {
          posicion_nueva=LASER_APUNTANDO;
          Serial.println("Apuntando");
         }
        else
         {
          //Serial.println("Sin deteccion");
          posicion_nueva=SIN_DETECCION;
         }
       }
       //Se deshabilita la calificacion
      cambio_la_frecuencia=NO;
    }
}



/*--------------------------------------------------------*/
void Espera_Tira_Leds()
{ 
  if (prende_leds==SI)
    { 
      strip.clear();
      strip.setPixelColor(8-led_inicio,strip.Color(led_1,led_2,led_3));
      strip.setPixelColor(16-led_inicio,strip.Color(led_1,led_2,led_3));
      strip.setPixelColor(24-led_inicio,strip.Color(led_1,led_2,led_3));
      strip.setPixelColor(32-led_inicio,strip.Color(led_1,led_2,led_3));
      strip.show();
      prende_leds=NO;
    }
}

/*------------------------------------------------------------*/
void Apunta_Tira_Leds()
{ 
  if (prende_leds==SI)
    {
      strip.clear();
      strip.setPixelColor(led_inicio,strip.Color(led_1,led_2,led_3));
      strip.setPixelColor(led_inicio+1,strip.Color(led_1,led_2,led_3));
      strip.setPixelColor(led_inicio+2,strip.Color(led_1,led_2,led_3));
      strip.setPixelColor(led_inicio+8,strip.Color(led_1,led_2,led_3));
      strip.setPixelColor(led_inicio+9,strip.Color(led_1,led_2,led_3));
      strip.setPixelColor(led_inicio+10,strip.Color(led_1,led_2,led_3));
      strip.setPixelColor(led_inicio+16,strip.Color(led_1,led_2,led_3));
      strip.setPixelColor(led_inicio+17,strip.Color(led_1,led_2,led_3));
      strip.setPixelColor(led_inicio+18,strip.Color(led_1,led_2,led_3));
      strip.setPixelColor(led_inicio+24,strip.Color(led_1,led_2,led_3));
      strip.setPixelColor(led_inicio+25,strip.Color(led_1,led_2,led_3));
      strip.setPixelColor(led_inicio+26,strip.Color(led_1,led_2,led_3));
      strip.show();
      prende_leds=NO;
    }
}

/* ---------------------------------------------------------------------*/
void Disparo_Tira_Leds()
{     int i;
      strip.setBrightness(100);
      for(i=0;i<=39;i++)
      {
      strip.setPixelColor(i,strip.Color(250,250,250));
      }
      strip.show();
      delay(20);
      strip.setBrightness(100);
      strip.clear();
      strip.show();
     

}


/*

int8_t tiros_de_jugador_por_turno[5]={0,0,0,0,0};
int8_t color_tablero[40]={1,0,}


nuevas fucniones 2 de febrero 2025
*/

/*
// variables de tipo de figura
#define BICHOS 1
#define CIRCULOS 2
#define NUMEROS 3




*/
/*--------------------------------------------------------*/
void Espera_Tira_Leds_Figuras()
{ 
  if (Reloj_Lento()==DETECTA_CAMBIO_ESTADO_RELOJ)
    {
      switch (datos_locales_diana.f)
          {
            case BICHOS:
                /*CODE*/
                Asigna_Leds_Espera_Bichos();
                break;

            case CIRCULOS:
                /*CODE*/
                Asigna_Leds_Espera_Circulos();
                break;

            case NUMEROS:
                /*CODE*/
                Asigna_Leds_Espera_Numeros();
                break;
          }
    }
}

/*--------------------------------------------------------*/
void Apunta_Tira_Leds_Figuras()
{ 
  if (Reloj_Rapido()==DETECTA_CAMBIO_ESTADO_RELOJ)
    {
      switch (datos_locales_diana.f)
          {
            case BICHOS:
                /*CODE*/
                Asigna_Leds_Apunta_Bichos();
                break;

            case CIRCULOS:
                /*CODE*/
                Asigna_Leds_Apunta_Circulos();
                break;

            case NUMEROS:
                /*CODE*/
                Asigna_Leds_Apunta_Numeros(); 
                break;
          }
    }
}


/*----------------------------------------------------------------------*/
void Asigna_Leds_Espera_Circulos()
{     
  /*asigna color del matriz*/
  switch (case_estado_espera_circulos)
    {
      case UNO:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Circulo_1[i];
          }
        case_estado_espera_circulos=DOS;
        break;

      case DOS:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Circulo_2[i];
          }
        case_estado_espera_circulos=TRES;
        break;      
      case TRES:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Circulo_3[i];
          }
        case_estado_espera_circulos=CUATRO;
        break;    
      case CUATRO:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Circulo_4[i];
          }
        case_estado_espera_circulos=CINCO;
        break;
      case CINCO:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Circulo_5[i];
          }
        case_estado_espera_circulos=SEIS;
        break;
      case SEIS:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Circulo_6[i];
          }
        case_estado_espera_circulos=SIETE;
        break;

      case SIETE:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Circulo_7[i];
          }
        case_estado_espera_circulos=UNO;
        break;
    }
  /* solicita encender matriz*/
  Enciende_Matriz_Led();

  // el bajar el estado lo realiza el reloj lento ya que al regresar el valor ya regres 
  // SIN CAMBIO DE ESTADO y ya no entra a escribir el siguiente
}

/*----------------------------------------------------------------------*/
void Asigna_Leds_Espera_Bichos()
{     
  /*asigna color del matriz*/
  switch (case_estado_espera_bichos)
    {
      case UNO:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Bichos_1[i];
          }
        case_estado_espera_bichos=DOS;
        break;

      case DOS:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Bichos_2[i];
          }
        case_estado_espera_bichos=TRES;
        break;      
      case TRES:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Bichos_3[i];
          }
        case_estado_espera_bichos=CUATRO;
        break;    
      case CUATRO:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Bichos_4[i];
          }
        case_estado_espera_bichos=CINCO;
        break;
      case CINCO:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Bichos_5[i];
          }
        case_estado_espera_bichos=SEIS;
        break;
      case SEIS:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Bichos_6[i];
          }
        case_estado_espera_bichos=UNO;
        break;
    }
  /* solicita encender matriz*/
  Enciende_Matriz_Led();

  /* 
  Nota de navegacion:
  el cambio del valor a "bajar el estado" lo realiza el reloj lento ya que al calcular el valor regresa
  SIN CAMBIO DE ESTADO y ya no entra a escribir el siguiente hasta que el reloj cumpla la condicion
  */


}


/*----------------------------------------------------------------------*/
void Asigna_Leds_Espera_Numeros()
{  
  
  /*asigna color del matriz*/
  //el numero_calculado_de_tiro = 31-datos_locales_diana.s  y se asigna cuando se recibe el paquete y es fijo
  switch (case_estado_espera_numeros) // selecciona la figura del tiro
    {
      case TREINTA:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_30[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;


      case PARPADEA1:
        /* code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_PARPADEA1[i];
          }
        case_estado_espera_numeros=PARPADEA2;               
        break;

      case PARPADEA2:
        /* code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_PARPADEA1[i];
          }
        case_estado_espera_numeros=numero_calculado_de_tiro;               
        break;

      case 29:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_29[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 28:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_28[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 27:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_27[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 26:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_26[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 25:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_25[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 24:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_24[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 23:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_23[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 22:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_22[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 21:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_21[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 20:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_20[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 19:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_19[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 18:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_18[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 17:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_17[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 16:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_16[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 15:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_15[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 14:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_14[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 13:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_13[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 12:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_12[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 11:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_11[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 10:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_10[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 9:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_9[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 8:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_8[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 7:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_7[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 6:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_6[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 5:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_5[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 4:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_4[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 3:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_3[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;

      case 2:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_2[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;
      case 1:
        /*code*/
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Numeros_1[i];
          }
        case_estado_espera_numeros=PARPADEA1;        
        break;
      default:
        /*code*/
        Serial.println("Error: Se paso el contador :"+String(case_estado_espera_numeros));
        case_estado_espera_numeros=PARPADEA1;        
        break;


    } // fin swtich case_estado_espera_numero
  /* solicita encender matriz*/
  Enciende_Matriz_Led();

  // el bajar el estado lo realiza el reloj lento ya que al regresar el valor ya regres 
  // SIN CAMBIO DE ESTADO y ya no entra a escribir el siguiente
}



/*----------------------------------------------------------------------*/
void Asigna_Leds_Apunta_Bichos()
{     
  /*asigna color del matriz*/
  switch (case_estado_apunta_circulos)
    {
      case OCHO:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Bichos_8[i];
          }
        case_estado_espera_bichos=NUEVE;
        break;

      case NUEVE:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Bichos_9[i];
          }
        case_estado_espera_bichos=OCHO;
        break;      
    }
  /* solicita encender matriz*/
  Enciende_Matriz_Led();

  // el bajar el estado lo realiza el reloj lento ya que al regresar el valor ya regres 
  // SIN CAMBIO DE ESTADO y ya no entra a escribir el siguiente
}

/*----------------------------------------------------------------------*/
void Asigna_Leds_Apunta_Circulos()
{     
  /*asigna color del matriz*/
  switch (case_estado_apunta_circulos)
    {
      case OCHO:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Circulo_8[i];
          }
        case_estado_apunta_circulos=NUEVE;
        break;

      case NUEVE:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Circulo_9[i];
          }
        case_estado_apunta_circulos=OCHO;
        break;      
    }
  /* solicita encender matriz*/
  Enciende_Matriz_Led();

  // el bajar el estado lo realiza el reloj lento ya que al regresar el valor ya regres 
  // SIN CAMBIO DE ESTADO y ya no entra a escribir el siguiente
}

/*----------------------------------------------------------------------*/
void Asigna_Leds_Apunta_Numeros()
{     
  /*asigna color del matriz*/
  switch (case_estado_apunta_numeros)
    {
      case OCHO:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Circulo_8[i]; // se utiliza tambien para apuntar en nuemreos
          }
        case_estado_apunta_numeros=NUEVE;
        break;

      case NUEVE:
        for(int i=0;i<=39;i++)
          {
            Vector_Matriz_Led[i]=Vector_Circulo_9[i]; // se utiliza tambien para apuntar en nuemreos
          }
        case_estado_apunta_numeros=OCHO;
        break;      
    }
  /* solicita encender matriz*/
  Enciende_Matriz_Led();
                      // el bajar el estado lo realiza el reloj lento ya que al regresar el valor ya regres 
                      // SIN CAMBIO DE ESTADO y ya no entra a escribir el siguiente
}


/*
// valores posibles de Vector_Matriz_led
#define ATRAS 0
#define TINTA 1
#define BASE 2
#define RESALTA 3
*/
void  Enciende_Matriz_Led()
{
  //defincion de pantone de colores con base al color solicitado por monitor
  switch (datos_locales_diana.c) 
    {
    case GREEN: 
      /* code */
        //color atras = NEGRO
        color_atras_1=0; color_atras_2=0; color_atras_3=0;

        //color tinta = VERDE
        color_tinta_1=255; color_tinta_2=0;  color_tinta_3=0;

        // color Base =AMARILLO
        color_base_1=255; color_base_2=255; color_base_3=0;

        //color Resalta = ROJO
        color_resalta_1=0; color_resalta_2=255; color_resalta_3=0;   
      break;

    case BLUE:
      /*code*/
        //color atras = NEGRO
        color_atras_1=0; color_atras_2=0; color_atras_3=0;

        //color tinta = AZUL
        color_tinta_1=0; color_tinta_2=0;  color_tinta_3=255;

        // color Base =AMARILLO
        color_base_1=255; color_base_2=255; color_base_3=0;

        //color Resalta = ROJO
        color_resalta_1=0; color_resalta_2=255; color_resalta_3=0;
      break;
    }

  //carga color a todos los leds
  for(int numero_led=0;numero_led<=39;numero_led++)
    {
      switch (Vector_Matriz_Led[numero_led])
        {
          case ATRAS: // NEGRO
            strip.setPixelColor(numero_led,strip.Color(color_atras_1,color_atras_2,color_atras_3));
            break;

          case TINTA: // VERDE O AZUL
            /* code */
            strip.setPixelColor(numero_led,strip.Color(color_tinta_1,color_tinta_2,color_tinta_3));
            break;

          case BASE: 
            /*CODE*/
            strip.setPixelColor(numero_led,strip.Color(color_base_1,color_base_2,color_base_3));            
            break;

          case RESALTA:
            strip.setPixelColor(numero_led,strip.Color(color_resalta_1,color_resalta_2,color_resalta_3));            
            break;
        }
    }
  strip.show(); 
}

/* --------------------------------------------------------*/


int Reloj_Lento() //CD001
{
  tiempo_actual_reloj_lento=millis();
  if ((tiempo_inicial_reloj_lento+1000)<tiempo_actual_reloj_lento)
    {
        tiempo_inicial_reloj_lento=millis();
        actualiza_display_lento=SI;
        return DETECTA_CAMBIO_ESTADO_RELOJ;
    }
  else
    {
      actualiza_display_lento=NO;
      return SIN_CAMBIO_ESTADO_RELOJ;
    }
}

/* -------------------------------------------------------*/
int Reloj_Rapido() //CD001
{
  tiempo_actual_reloj_rapido=millis();
  if ((tiempo_inicial_reloj_rapido+500)<tiempo_actual_reloj_rapido)
    {
        tiempo_inicial_reloj_rapido=millis();
        actualiza_display_rapido=SI;
        return DETECTA_CAMBIO_ESTADO_RELOJ;      
    }
  else
    {
      actualiza_display_rapido=NO;
      return SIN_CAMBIO_ESTADO_RELOJ;
    }
}






/* ---------------------------------------------------------------------*/
void Disparo_Tira_Leds_Figuras()
{    
      strip.setBrightness(100);
      strip.clear();
      for(int i=0;i<=16;i++)
      {
      strip.setPixelColor(i,strip.Color(250,250,250));
      }
      strip.show();
      delay(10);
      strip.setBrightness(100);
      strip.clear();
      strip.show();
     

}

/* ----------------------------------------------------------*/
void Enciende_Tira_Figuras()
{ 
  tiempo_actual_leds=millis();
  switch (posicion_nueva)
  {
    case SIN_DETECCION:   // SIN_DETECCION
        Espera_Tira_Leds_Figuras(); //CD001 se SACA DEL IF YA QUE LA FUNCIONLED INICIO YA NO SIRVE
        break;
    case LASER_APUNTANDO:   // LASER APUNTANDO
        Apunta_Tira_Leds_Figuras();
        break;
    case DISPARO_DETECTADO:   // DISPARO_DETECTADO
        Disparo_Tira_Leds_Figuras();
        break;
  }
}