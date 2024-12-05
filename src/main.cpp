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
volatile unsigned long previous_time,           current_time;
volatile unsigned long previous_time_solicitud, actual_time_solicitud;
volatile unsigned long tiempo_inicial,          tiempo_final;
volatile unsigned long hora_de_inicio, hora_final;

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
      // strip.setPixelColor(1,strip.Color(10,0,0));
      // strip.setPixelColor(2,strip.Color(10,0,0));
      // strip.setPixelColor(3,strip.Color(10,0,0));
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
      //strip.setPixelColor(1,strip.Color(10,0,0));
      //strip.setPixelColor(2,strip.Color(10,0,0));
      //strip.setPixelColor(3,strip.Color(10,0,0));
      //strip.setPixelColor(4,strip.Color(10,0,0));
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
      //strip.setPixelColor(1,strip.Color(10,0,0));
      Serial.println("Failed to add peer");
        return;
      }
   else
      { 
        //strip.setPixelColor(1,strip.Color(10,0,0));
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
      //strip.setPixelColor(1,strip.Color(0,10,0));
      //strip.setPixelColor(2,strip.Color(0,10,0));
      Serial.println("Sent with success");
     }
    else 
     {
      //strip.setPixelColor(1,strip.Color(10,0,0));
      //strip.setPixelColor(2,strip.Color(10,0,0));
      Serial.println("Error sending the data");
     }
}

 /* -------------VARIABLES DE RECEPCION DE DATOS---------------------------------*/

int8_t rojo=0;
int8_t verde=0;
int8_t azul=1;
#define NULO 0
int color_diana = NULO;
#define TIEMPO_DE_RETARDO_DE_RESPUESTA 500 // 500 mseg entre termino de disparo y envio a monitor

//estado de diana
#define APAGADO 0
#define ENCENDIDO 1
int estado_diana =APAGADO;

#define EN_ESPERA 0
#define INICIA_CONTADOR_TIEMPO 1
#define INICIA_TIEMPO_SOLICITUD_DISPARO 2
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
ulong frecuencia_apunta=800;
ulong frecuencia_disparo=1300;
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
#define NUMPIXELS 31
Adafruit_NeoPixel strip(NUMPIXELS,pin_leds,NEO_RGB+NEO_KHZ800);
int prende_leds=SI;
int8_t destello =1;

int primer_encendido_apunta = SI;
uint8_t led_apunta=5;


//declaracion de funciones
void x_Espera_Leds_Disparo();
void Analisis_De_Frecuencia2();
void Califica_La_Frecuencia();
void x_inicia_proceso_solicitud();
void X_Inicia_Tiempo_Solicitud();
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
void Enciende_Tira();
void Enciende_Tira_Leds();
void Espera_Tira_Leds();
void Apunta_Tira_Leds();



volatile unsigned long tiempo_inicial_muestreo=0;
volatile unsigned long tiempo_actual_muestreo =0;
volatile unsigned long tiempo_inicial_leds=0;
volatile unsigned long tiempo_actual_leds=0;
volatile unsigned long periodo_muestreo_mseg =10;
volatile unsigned long pulsos=0;
volatile unsigned long muestra[10]={0,0,0,0,0,0,0,0,0,0};
volatile unsigned long promedio_muestreo=0;
volatile unsigned long nueva_frecuencia=0;
int8_t rango_actual=0;
volatile unsigned long acumulado=0;
volatile unsigned long rangototal=0;

int8_t se_genero_nueva_frecuencia=NO;
int8_t ultimo_resultado=0;

int8_t posicion_anterior=SIN_DETECCION;
int8_t posicion_nueva=SIN_DETECCION;
int8_t existe_cambio_de_posicion=SI;
int8_t estuvo_apuntando=NO;


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
  strip.begin();
  strip.show();
  strip.setBrightness(50);

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
  Serial.println(tiempo_inicial_muestreo);

  // variables a fijar para pruebas
  estado_diana=ENCENDIDO;

} // fin setup

/* ----------------------  F I N    S E T U P -------------------------*/


void loop() 
{
  // deteccion de frecuencia
  // frecuencia_detectada =Analisis_De_Frecuencia();
  Analisis_De_Frecuencia2();
  Califica_La_Frecuencia();
  Enciende_Tira ();


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
} // FIN  loop ---------------------------------------------------


void Enciende_Tira ()
{ 
  tiempo_actual_leds=millis();
  //Serial.print(tiempo_actual_leds-tiempo_inicial_leds);
  switch (posicion_nueva)
  {
    case SIN_DETECCION:   // SIN_DETECCION
      if ((tiempo_actual_leds-tiempo_inicial_leds)>200)
        { // cambia posicion de leds
          led_inicio=led_inicio+1;
          //Serial.println("cambio de led :"+String(led_inicio));
          prende_leds=SI;
          if (led_inicio>8) { led_inicio=0;}
          // reinicia periodo
          tiempo_inicial_leds=millis();
          // enciende
          Espera_Tira_Leds();
        }
      break;
    case LASER_APUNTANDO:   // LASER APUNTANDO
      if ((tiempo_actual_leds-tiempo_inicial_leds)>40)
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
        Serial.print("DISPARO DETECTADO : ");
        Serial.println(nueva_frecuencia);
        for (int i=1;i<=5;i++)
          {
          x_Enciende_Disparo_Destellos();
          }
        posicion_nueva=SIN_DETECCION;
        break;
  }
}
  /* nota: se quito la opcion de validar que disparo viniera
   de apuntar, ya que a veces  no lo detecta si el gatillo se oprime
   muy rapido y perderia el tiro, asi que pueden hacer trampa
   dejando el gatillo oprimido
   */



void Luces_Diana()
{
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
            case EN_ESPERA:
              // entra al iniciar proceso
              menu_proceso_diana=INICIA_CONTADOR_TIEMPO;
              break;            
            case INICIA_CONTADOR_TIEMPO:
              //Arranca el tiempo
              hora_de_inicio=millis();
              menu_proceso_diana=INICIA_TIEMPO_SOLICITUD_DISPARO;
              break;
            case INICIA_TIEMPO_SOLICITUD_DISPARO:
              // reinicia los contadores del tiempo total del disparo
              X_Inicia_Tiempo_Solicitud();
              menu_proceso_diana=ENCIENDE_SOLICITUD;
              break;
            case ENCIENDE_SOLICITUD:
              //Serial.print("-ES-");
              prende_leds=SI;
              x_Enciende_Solicitud();
              menu_proceso_diana=ESPERA_LEDS_SOLICITUD;
              break;
            case ESPERA_LEDS_SOLICITUD:
              //Serial.print("-SOL-");
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
              strip.clear();
              break;
          } // fin switch menu_proceso_diana
        break;

    }  // fin switch estado_diana
}




//------------------------------------------------------------
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
        se_genero_nueva_frecuencia=SI;
      }
  }
}
      
void Califica_La_Frecuencia()
{        
  if (se_genero_nueva_frecuencia==SI)
    { 
      //Serial.println("Entro a Califica Frecuencia");
      if (nueva_frecuencia>1200)
       {
        posicion_nueva=DISPARO_DETECTADO;
        //Serial.println("Disparo detectado");
       }
      else
       {
        if (nueva_frecuencia>700)
         {
          posicion_nueva=LASER_APUNTANDO;
         // Serial.println("Apuntando");
         }
        else
         {
          //Serial.println("Sin deteccion");
          posicion_nueva=SIN_DETECCION;
         }
       }
      // checa cambio de posicion de menu
      if (posicion_anterior==posicion_nueva)
       {
        existe_cambio_de_posicion=NO;
       }
      else
       {
        existe_cambio_de_posicion=SI;
        posicion_anterior=posicion_nueva;
       }
       se_genero_nueva_frecuencia=NO;
    }
}








/*-------------------------------------------------------------------------------*/
void x_Enciende_Solicitud()
{
  if (prende_leds==SI)
    {
      // Serial.print("led-ON");
      //int orden_led_[40]={0,8,9,22,23,31,32,45,1,7,10,21,24,30,33,44,2,6,11,20,33,43,3,5,12,19,34,42,4,4,13,18,35,41};
      strip.setPixelColor(orden_led_ [led_inicio+1],strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(orden_led_ [led_inicio+2],strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(orden_led_ [led_inicio+3],strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(orden_led_ [led_inicio+4],strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(orden_led_ [led_inicio+5],strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(orden_led_ [led_inicio+6],strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(orden_led_ [led_inicio+7],strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(orden_led_ [led_inicio+8],strip.Color(rojo,verde*50,azul*50));
      strip.show();
      prende_leds=NO;
    }
}

void Espera_Tira_Leds()
{ 
  if (prende_leds==SI)
    {
      strip.clear();
      strip.setPixelColor(8-led_inicio,strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(16-led_inicio,strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(24-led_inicio,strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(32-led_inicio,strip.Color(rojo,verde*50,azul*50));
      strip.show();
      prende_leds=NO;
    }
}

void Apunta_Tira_Leds()
{ 
  if (prende_leds==SI)
    {
      strip.clear();
      strip.setPixelColor(led_inicio,strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(led_inicio+1,strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(led_inicio+2,strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(led_inicio+8,strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(led_inicio+9,strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(led_inicio+10,strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(led_inicio+16,strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(led_inicio+17,strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(led_inicio+18,strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(led_inicio+24,strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(led_inicio+25,strip.Color(rojo,verde*50,azul*50));
      strip.setPixelColor(led_inicio+26,strip.Color(rojo,verde*50,azul*50));
      strip.show();
      prende_leds=NO;
    }
}
/*------------------------------------------------*/
void x_Esperando_en_Apunta()
{
  //frecuencia_detectada =Analisis_De_Frecuencia(); 
  switch (posicion_nueva)
    {
      case SIN_DETECCION:
        /* regresa a solicitud*/
        menu_proceso_diana=INICIA_TIEMPO_SOLICITUD_DISPARO;                   
        break;
      case LASER_APUNTANDO:
        actual_time_solicitud=millis();
        if ((actual_time_solicitud-previous_time_solicitud)>500)
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
    switch (posicion_nueva)
      {
        case SIN_DETECCION:
          /* continua con la solicitud de disparo*/
          actual_time_solicitud=millis();
          if ((actual_time_solicitud-previous_time_solicitud)>2000)
            {
              strip.clear();
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
                strip.clear();
                for (int i=2;i<=6;i++)
                  {
                    strip.setPixelColor(i,strip.Color(10,0,0));
                  }
                strip.show();
                menu_proceso_diana=DATOS_ENVIADOS_OK;
              }
          }
          break;
      } //fin switch menu_resultados
}

/*---------------------------------------------------------------------*/
void X_Inicia_Tiempo_Solicitud()
{
  /*  Inicia el contador de tiempo para el disparo*/
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
          strip.clear();
        }
      menu_proceso_diana=ENCIENDE_DISPARO;
      prende_leds=SI;
    } 
}
/* ---------------------------------------------------------------------*/
void x_Enciende_Disparo_Destellos()
{
      strip.clear();
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.setPixelColor(rand()%32,strip.Color((rand()%5)*50,(rand()%5)*50,(rand()%5)*50));
      strip.show();
      delay(50);
}
/* ---------------------------------------------------------------------*/

void x_Enciende_Apuntando()
{
  if (prende_leds==SI)
    {
      strip.setPixelColor(led_apunta-5,strip.Color(0,0,0));
      strip.setPixelColor(led_apunta-4,strip.Color(rojo,verde*5,azul*5));
      strip.setPixelColor(led_apunta-3,strip.Color(rojo,verde*10,azul*10));
      strip.setPixelColor(led_apunta-2,strip.Color(rojo,verde*15,azul*15));
      strip.setPixelColor(led_apunta-1,strip.Color(rojo,verde*20,azul*20));
      strip.setPixelColor(led_apunta,strip.Color(rojo,verde*25,azul*25));
      strip.show();
      prende_leds=NO;
    }
}

/*-------------------------------------------------------------*/




/*
switch (estado_diana)
    {
      case APAGADO:
        break;
      case ENCENDIDO:
        switch (menu_proceso_diana)
          {
            case EN_ESPERA:
              menu_proceso_diana=INICIA_CONTADOR_TIEMPO;
              //Serial.print("-IniciaEspera-");
              break;            
            case INICIA_CONTADOR_TIEMPO:
              //Serial.print("-IniciaContador-");
              tiempo_inicial=millis();
              menu_proceso_diana=INICIA_TIEMPO_SOLICITUD_DISPARO;
              break;
            case INICIA_TIEMPO_SOLICITUD_DISPARO:
              // reinicia los contadores del tiempo total del disparo
              X_Inicia_Tiempo_Solicitud();
              menu_proceso_diana=ENCIENDE_SOLICITUD;
              break;
            case ENCIENDE_SOLICITUD:
              //Serial.print("-ES-");
              prende_leds=SI;
              x_Enciende_Solicitud();
              menu_proceso_diana=ESPERA_LEDS_SOLICITUD;
              break;
            case ESPERA_LEDS_SOLICITUD:
              //Serial.print("-SOL-");
              x_Espera_Leds_Solicitud();
              break;
            case INICIA_PROCESO_APUNTANDO:
              //  contador de tiempo
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
              strip.clear();
              break;
          } // fin switch menu_proceso_diana
        break;

    }  // fin switch estado_diana
*/

