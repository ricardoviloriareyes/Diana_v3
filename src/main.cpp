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

// direcciones mac de ESTE RECEPTOR (PARA SER CAPTURADA EN EL  MONITOR)
uint8_t   broadcastAddress1[]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

//Monitor Central para regreso de resultados
uint8_t   broadcastAddress4[]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; 

#define NO 0
#define SI 1

// Colores del texto en tablero
#define RED   1
#define GREEN 2
#define BLUE  3

          
// variables de control de tiempo
ulong previous_time,           current_time;
ulong previous_time_solicitud, actual_time_solicitud;
ulong tiempo_inicial,          tiempo_final;

// varables para envio de datos a diana
#define INICIALIZA_TRANSMISION 1
#define PREPARA_PAQUETE_ENVIO  2
#define ENVIA_PAQUETE          3
#define CONFIRMA_RECEPCION 4 
#define PAUSA_VEINTE 5
#define ENVIO_EXITOSO 6
int8_t menu_transmision = INICIALIZA_TRANSMISION;
int intentos_envio=0;

#define INICIALIZA_TRANSMISION2 1
#define PREPARA_PAQUETE_ENVIO2  2
#define ENVIA_PAQUETE2          3
#define CONFIRMA_RECEPCION2 4 
#define PAUSA_VEINTE2 5
#define ENVIO_EXITOSO 6
int8_t menu_transmision2 = INICIALIZA_TRANSMISION;

#define P1_INICIA 0
#define P2_PREPARA 1
#define P3_ENVIA 2
#define P4_PAUSA 3
int8_t menu_resultados = P1_INICIA ;


// variables para envio de tiro a DIana
#define LIBRE 0
#define OCUPADO 1




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
      // pixels.setPixelColor(1,pixels.Color(10,0,0));
      // pixels.setPixelColor(2,pixels.Color(10,0,0));
      // pixels.setPixelColor(3,pixels.Color(10,0,0));
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
      //pixels.setPixelColor(1,pixels.Color(10,0,0));
      //pixels.setPixelColor(2,pixels.Color(10,0,0));
      //pixels.setPixelColor(3,pixels.Color(10,0,0));
      //pixels.setPixelColor(4,pixels.Color(10,0,0));
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
      //pixels.setPixelColor(1,pixels.Color(10,0,0));
      Serial.println("Failed to add peer");
        return;
      }
   else
      { 
        //pixels.setPixelColor(1,pixels.Color(10,0,0));
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
      //pixels.setPixelColor(1,pixels.Color(0,10,0));
      //pixels.setPixelColor(2,pixels.Color(0,10,0));
      Serial.println("Sent with success");
     }
    else 
     {
      //pixels.setPixelColor(1,pixels.Color(10,0,0));
      //pixels.setPixelColor(2,pixels.Color(10,0,0));
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
int menu_proceso_diana = EN_ESPERA;

int8_t frecuencia_detectada =0;

/* variables para analisis de frecuencia */
ulong frecuencia_central=0;
ulong frecuencia_apunta=0;
ulong frecuencia_disparo=0;
ulong frecuencia_apunta_verde=1200;
ulong frecuencia_apunta_azul= 3000;
ulong frecuencia_disparo_verde=2400;
ulong frecuencia_disparo_azul =4800;


/* ----------------- ENVIO DE DATOS -----------------------------*/
int enviar_datos=NO;
String success;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) 
{
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status ==0){
    success = "Delivery Success :)";
  }
  else{
    success = "Delivery Fail :(";
  }
}






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
      menu_proceso_diana=INICIA_CONTADOR_TIEMPO;
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

        }
    }
}




//variables para deteccion de disparo en analisis de frecuencia
#define SIN_DETECCION 0
#define LASER_APUNTANDO 1
#define DISPARO_DETECTADO 2
int8_t estado_detector = SIN_DETECCION;

// definicion de io-esp32
const int pin_pulsos = 2;  // recepcion de pulsos par calculo de frecuencia
const int pin_leds = 16; // pin asignado de pcb del  diagrama de electronica diana 3.1
const int button_wakeup =33;

// variable para guardar el pulso de wakeup
int buttonstate=0;


// definicion de tira led
uint8_t led_inicio=0;
int orden_led_[40]={0,8,9,22,23,31,32,45,1,7,10,21,24,30,33,44,2,6,11,20,33,43,3,5,12,19,34,42,4,4,13,18,35,41};
#define NUMPIXELS 46
Adafruit_NeoPixel pixels(NUMPIXELS,pin_leds,NEO_RGB+NEO_KHZ800);
int prende_leds=SI;
int8_t destello =1;

int primer_encendido_apunta = SI;
uint8_t led_apunta=5;


//declaracion de funciones
void x_Espera_Leds_Disparo();
int Analisis_De_Frecuencia();
void x_inicia_proceso_solicitud();
void x_Enciende_Apuntando();
void x_Enciende_Disparo_Destellos();
void x_Espera_20Mseg();
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len);
void Test_Conexion_Diana();
void readMacAddress();
void Inicia_ESP_NOW();
void Peer_Monitor_En_Linea();
void x_Espera_Leds_Solicitud();
void x_Enciende_Solicitud();
void x_Espera_Leds_Solicitud();
void x_Esperando_en_Apunta();
void x_Envia_Resultados();

volatile unsigned long tiempo_inicial_muestreo=0;
volatile unsigned long tiempo_actual_muestreo =0;
int8_t pulsos=0;
int8_t periodo_muestreo_mseg =10;
int8_t muestra[10]={0,0,0,0,0,0,0,0,0,0};
int8_t promedio_muestreo=0;
int8_t rango_actual=0;
int8_t acumulado=0;

int8_t ultimo_resultado=0;


void Cuenta_Pulso()
{
 pulsos++;
}


/* -----------------------INICIO    S E T U P --------------------------------*/

void setup() 
{
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
  pixels.begin();
  pixels.clear(); // inicializa todos en off

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
  Serial.println(tiempo_inicial_muestreo);

} // fin setup

/* ----------------------  F I N    S E T U P -------------------------*/


void loop() 
{
  // deteccion de frecuencia
   frecuencia_detectada =Analisis_De_Frecuencia();
   //Serial.print("Frecuencia detectada :");
   //Serial.println(frecuencia :);

  // envio de datos activado por  x_envia_resultados();
  if (enviar_datos==SI)
  {
    esp_err_t result = esp_now_send(broadcastAddress4,(uint8_t *) &datos_enviados,sizeof(structura_mensaje));         
    if (result == ESP_OK) 
      {
        Serial.println("Sent with success");
        menu_proceso_diana=DATOS_ENVIADOS_OK;
        menu_resultados=P1_INICIA;
        enviar_datos=NO;
      }
    else 
      {
        Serial.println("Error sending the data");
        intentos_envio++;
        menu_resultados=P4_PAUSA;
        previous_time=millis();
        enviar_datos=NO;
      }
  }


  switch (estado_diana)
    {
      case APAGADO:
        /* code */
        /*se mantiene aqui, hasta qeu se inicie el proceso en el void
        de recepcion de solicitud, ahi se asigna los datos de color,*/
        break;
      case ENCENDIDO:
        switch (menu_proceso_diana)
          {
            case INICIA_CONTADOR_TIEMPO:
              tiempo_inicial=millis();
              menu_proceso_diana=INICIA_PROCESO_SOLICITUD;
              break;
            case INICIA_PROCESO_SOLICITUD:
              x_inicia_proceso_solicitud();
              menu_proceso_diana=ENCIENDE_SOLICITUD;
              break;
            case ENCIENDE_SOLICITUD:
              x_Enciende_Solicitud();
              menu_proceso_diana=ESPERA_LEDS_SOLICITUD;
              break;
            case ESPERA_LEDS_SOLICITUD:
              x_Espera_Leds_Solicitud();
              break;
            case INICIA_PROCESO_APUNTANDO:
              /*  contador de tiempo*/
              previous_time=millis();
              previous_time_solicitud=millis();
              menu_proceso_diana=ENCIENDE_APUNTANDO;
              prende_leds=SI;
              led_apunta=5;
              break;    
            case ENCIENDE_APUNTANDO:
              x_Enciende_Apuntando();
              menu_proceso_diana=ESPERA_LEDS_APUNTA;
              break;
            case ESPERA_LEDS_APUNTA:
              x_Esperando_en_Apunta();
              break;
            case INICIA_PROCESO_DISPARO:
              previous_time=millis();
              previous_time_solicitud=millis();
              prende_leds=SI;
              destello=1;
              menu_proceso_diana=ENCIENDE_DISPARO;
              break;
            case ENCIENDE_DISPARO:
              x_Enciende_Disparo_Destellos();
              menu_proceso_diana=ESPERA_LEDS_DISPARO;
              break;
            case ESPERA_LEDS_DISPARO:
              x_Espera_Leds_Disparo();
              break;
            case ESPERA_ENVIO_RESULTADOS:
              actual_time_solicitud=millis();
              if ((actual_time_solicitud-previous_time_solicitud)>500)
                {
                  menu_proceso_diana=ENVIA_RESULTADOS;
                  menu_transmision=INICIALIZA_TRANSMISION;
                }   
              break;
            case ENVIA_RESULTADOS:
              x_Envia_Resultados();
              break;
            case DATOS_ENVIADOS_OK:
              estado_diana=APAGADO;
              pixels.clear();
              break;
          } // fin switch menu_proceso_diana
        break;

    }  // fin switch estado_diana
} // FIN  loop ---------------------------------------------------


/* ----------------------------------------------------------------*/
int Analisis_De_Frecuencia()
{
  tiempo_actual_muestreo=millis();
  //Serial.print(tiempo_actual_muestreo);
  //Serial.print("-");
 // Serial.print(tiempo_inicial_muestreo);
  //Serial.print("= ");
  //Serial.print(tiempo_actual_muestreo-tiempo_inicial_muestreo);
  //Serial.println(" ");

 if ((tiempo_actual_muestreo-tiempo_inicial_muestreo)>periodo_muestreo_mseg) //espacio para obtener la muestra
  {
    // asigna la cantidad de pulsos en muestra
    muestra[rango_actual]=pulsos; 

    /*Serial.print("Muestra [ ");
    Serial.print(rango_actual);
    Serial.print("] = ");
    Serial.println(pulsos);*/

    if (rango_actual==9)
      { 
        for (int i=0;i<=9;i++)
        {
          acumulado=acumulado+muestra[i];
        }
        promedio_muestreo=(int)acumulado/10;
        rango_actual=0;
        Serial.print("-");
        Serial.print(promedio_muestreo);

      }
    else
      {
        rango_actual++;  //cambia la muestra
        acumulado=0;
      }
    tiempo_inicial_muestreo=millis();
    pulsos=0;

    // califica la frecuencia  
    if (rango_actual==0) 
      { 
        ultimo_resultado=SIN_DETECCION;
        if  ( (promedio_muestreo>=(frecuencia_apunta-200)) && (promedio_muestreo <= (frecuencia_apunta+200) )) 
          {
            ultimo_resultado=LASER_APUNTANDO;
          }

        if  ( (promedio_muestreo>=(frecuencia_disparo-200)) && (promedio_muestreo <= (frecuencia_disparo+200) )) 
          {
            ultimo_resultado=DISPARO_DETECTADO;
          }
       // Serial.println("promedio_muestreo :"+String(promedio_muestreo));
      }

  }
  return ultimo_resultado;
}



/*-------------------------------------------------------------------------------*/
void x_Enciende_Solicitud()
{
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
}

/*------------------------------------------------*/
void x_Esperando_en_Apunta()
{
  //frecuencia_detectada =Analisis_De_Frecuencia(); 
  switch (frecuencia_detectada)
    {
      case SIN_DETECCION:
        /* regresa a solicitud*/
        menu_proceso_diana=INICIA_PROCESO_SOLICITUD;                   
        break;
      case LASER_APUNTANDO:
        actual_time_solicitud=millis();
        if ((actual_time_solicitud-previous_time_solicitud)>50)
          {
            led_apunta++;
            previous_time_solicitud=millis();
            if (led_apunta>50)
              {
                led_apunta=5;
              }
            menu_proceso_diana=ENCIENDE_APUNTANDO;
            prende_leds=SI;
          }       
        break;
      case DISPARO_DETECTADO:
        menu_proceso_diana=INICIA_PROCESO_DISPARO;
        prende_leds=SI;
        led_apunta=1;
        previous_time=millis();
        tiempo_final=millis();
        break;

    } //fin de switch frecuencia_detectada               
 }
/*--------------------------------------------------*/
void  x_Espera_Leds_Solicitud()
  {               
    //frecuencia_detectada =Analisis_De_Frecuencia(); 
    // frecuencia_detectada = 0 SIN DETECCION / 1 LASER_APUNTANDO/ 2 DISPARO_DETECTADO
    switch (frecuencia_detectada)
      {
        case SIN_DETECCION:
          /* continua con la solicitud de disparo*/
          actual_time_solicitud=millis();
          if ((actual_time_solicitud-previous_time_solicitud)>300)
            {
              pixels.clear();
              led_inicio=led_inicio+8;
              if (led_inicio>40)
                {
                  led_inicio=0;
                }
              prende_leds=SI;
              previous_time_solicitud=millis();
              menu_proceso_diana=ENCIENDE_SOLICITUD;
            }                        
          break;
        case LASER_APUNTANDO:
          menu_proceso_diana=INICIA_PROCESO_APUNTANDO;
          prende_leds=SI;
          break;             
        case DISPARO_DETECTADO:
          menu_proceso_diana=INICIA_PROCESO_DISPARO;
          prende_leds=SI;
          led_apunta=5;
          break;
      } //fin de switch frecuencia_detectada
  }


/*-----------------------------------------------*/
void x_Envia_Resultados()
{                 
  switch (menu_resultados)
    {
      case P1_INICIA:
        /* code */
        menu_resultados=P2_PREPARA;
        break;
      case P2_PREPARA:
        datos_enviados.t=TIRO_ACTIVO; // TEST SI O TEST NO->VALIDO
        datos_enviados.d=datos_locales_diana.d; //diana
        datos_enviados.c=datos_locales_diana.c;   // color disparo
        datos_enviados.p=datos_locales_diana.p;
        datos_enviados.tiempo=int((tiempo_final-tiempo_inicial)*100)/100.00; // para regresar el tiempo
        intentos_envio=1;
        menu_resultados=P3_ENVIA;
        break;
      case P3_ENVIA:
        enviar_datos=SI;
        break;
      case P4_PAUSA:
        current_time=millis();
        if ((current_time-previous_time)>200)
          {
            if (intentos_envio<4)
              {
                menu_resultados=P3_ENVIA;
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
                menu_proceso_diana=DATOS_ENVIADOS_OK;
              }
          }
          break;
      } //fin switch menu_resultados
}

/*---------------------------------------------------------------------*/
void x_inicia_proceso_solicitud()
{
  /*  contador de tiempo*/
  previous_time=millis();
  previous_time_solicitud=millis();
  led_inicio=0;
  prende_leds=SI;
}

/* ---------------------------------------------------------------------*/


/* ---------------------------------------------------------------------*/
void x_Espera_Leds_Disparo()
{                  
  actual_time_solicitud=millis();
  if ((actual_time_solicitud-previous_time_solicitud)>50)
    {
      destello++;
      if (destello>100)
        {
          previous_time_solicitud=millis();
          menu_proceso_diana=ESPERA_ENVIO_RESULTADOS;
          pixels.clear();
        }
      menu_proceso_diana=ENCIENDE_DISPARO;
      prende_leds=SI;
    } 
}
/* ---------------------------------------------------------------------*/
void x_Enciende_Disparo_Destellos()
{
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
}
/* ---------------------------------------------------------------------*/

void x_Enciende_Apuntando()
{
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
}

/*-------------------------------------------------------------*/


/*
Programa principal Monitor
-  Funcion de escribir a monitor
- conexon de sistema de radio para botones



Software de Diana
- analisis de frecuencia




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



