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


//24:62:AB:DC:AC:F4

// direcciones mac de ESTE RECEPTOR (PARA SER CAPTURADA EN EL  MONITOR)
uint8_t   broadcastAddress1[]={0x24,0x62,0xAB,0xDC,0xAC,0xF4};


//4c:eb:d6:75:48:30

//Monitor Central para regreso de resultados
uint8_t   broadcastAddress4[]={0x4C,0xEB,0xD6,0x75,0x48,0x30}; 

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
      strip.setPixelColor(1,strip.Color(100,0,0));
      strip.setPixelColor(2,strip.Color(100,0,0));
      strip.setPixelColor(3,strip.Color(100,0,0));
      strip.show();
      Serial.println("Error initializing ESP-NOW");
      return;
    }
  else
  {
      strip.setPixelColor(1,strip.Color(0,100,0));
      strip.setPixelColor(2,strip.Color(0,100,0));
      strip.setPixelColor(3,strip.Color(1,100,0));
      strip.show();
      Serial.println("initializing ESP-NOW  OK");
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
      strip.setPixelColor(1,strip.Color(0,0,0));
      strip.setPixelColor(2,strip.Color(20,0,0));
      strip.setPixelColor(3,strip.Color(20,0,0));
      strip.setPixelColor(4,strip.Color(20,0,0));
      strip.show();
    }
}



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
        strip.setPixelColor(1,strip.Color(100,0,0));
        strip.show();
        Serial.println("Failed to add peer Monitor");
        return;
     }
   else
      { 
        strip.setPixelColor(1,strip.Color(0,150,0));
        strip.show();
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
      strip.setPixelColor(1,strip.Color(0,100,0));
      strip.setPixelColor(2,strip.Color(0,100,0));
      strip.show();
      Serial.println("TEST-Envio a Monitor OK");
     }

    else 
     {
      strip.setPixelColor(1,strip.Color(100,0,0));
      strip.setPixelColor(2,strip.Color(100,0,0));
      strip.show();
      Serial.println("TEST-Error enviando a Monitor");
     }
}

 /* -------------VARIABLES DE RECEPCION DE DATOS---------------------------------*/

int8_t rojo=0;
int8_t verde=0;
int8_t azul=1;


//estado de diana
#define APAGADO 0
#define ENCENDIDO 1
int estado_diana =APAGADO;



/* variables para analisis de frecuencia */
ulong frecuencia_disparo=1300;
ulong frecuencia_apunta=800;

ulong frecuencia_disparo_verde=2500;
ulong frecuencia_apunta_verde=2000;

ulong frecuencia_disparo_azul =1300;
ulong frecuencia_apunta_azul= 800;


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


// tiempo de duracion del tiro
volatile unsigned long tiempo_inicia_tiro=0;
volatile unsigned long tiempo_termina_tiro=0;
int activa_envio_de_resultados=NO;



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
  
  Serial.println("Tiro activo = "+String(datos_recibidos.t));

  if (datos_recibidos.t==TIRO_ACTIVO)
    { 
      Serial.println("Estado-diana= ENCENDIDO");
      estado_diana=ENCENDIDO;
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
int8_t Cambio_La_Frecuencia =NO;
int8_t posicion_nueva       =SIN_DETECCION;
int captura_tiempo_inicial=SI;

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
  //Serial.println(tiempo_inicial_muestreo);

  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));



  // variables a fijar para pruebas 
  // SE DEBEN QUITAR CUANDO INICIE SINCRONIZACIN CON MONITOR
 // estado_diana=ENCENDIDO;

} // fin setup

/* ----------------------  F I N    S E T U P -------------------------*/

/* ------------------------inicio loop ----------------------------*/
void loop() 
{ 
  switch (estado_diana)
  {
    case APAGADO:
      /* code */
      break;
    case ENCENDIDO:  // se habiita al recibir solicitud
      // inicia el tiempo del  disparo
      if (captura_tiempo_inicial==SI)
        {
          tiempo_inicia_tiro=millis(); // incia tiempo del tiro
          Serial.println("Contador inicial (mseg): "+String(tiempo_inicia_tiro));
          captura_tiempo_inicial=NO;
        }
      // proceso durante el disparo
      if (activa_envio_de_resultados==NO)
        {
          Analisis_De_Frecuencia2();
          Califica_La_Frecuencia();
          Enciende_Tira ();
        }
      else
        {
         Envia_Resultados_Al_Monitor();
        }
      // Reliza el envio de resultados,se deja a este nivel por requerimiento de ESPNOW
      if (enviar_datos==SI) // se habilita en Envia_resultados_al_monitor/ENVIA_PAQUETE 
        {
          esp_err_t result = esp_now_send(broadcastAddress4,(uint8_t *) &datos_enviados,sizeof(structura_mensaje));         
          if (result == ESP_OK) 
            {
              Serial.println("Envio OK, intento : "+String(intentos_envio));
              Serial.println("Tiempo disparo (seg)   : "+String(datos_enviados.tiempo));
              enviar_datos=NO;
              estado_diana=APAGADO;
              captura_tiempo_inicial=NO;
              strip.clear();
              strip.show();
            }
          else 
            {
              Serial.println("Error envio : "+String(intentos_envio));
              intentos_envio++;
              flujo_de_envio=PAUSA_REENVIO;
              previous_time_reenvio=millis();
            }
        }
      break;
  } // fin de switch estado_diana
}
// FIN  loop ---------------------------------------------------



/*-----------------------------------------------*/
void Envia_Resultados_Al_Monitor()
{                 
  switch (flujo_de_envio)
    {
      case STANDBYE:
        /* solo espera */
        break;
      case PREPARA_PAQUETE_ENVIO:
        datos_enviados.t=TIRO_ACTIVO; // TEST SI O TEST NO->VALIDO
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
        if ((current_time_reenvio-previous_time_reenvio)>200)
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
                    strip.setPixelColor(i,strip.Color(50,0,0));
                  }
                strip.show();
                estado_diana=APAGADO;
                captura_tiempo_inicial=NO;
                Serial.println("NO SE PUDO MANDAR INFORMACION !");
                delay(3000);
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
        Serial.print("DISPARO DETECTADO / frecuencia : ");
        Serial.println(nueva_frecuencia);
        Serial.println("Contador final (mseg) : "+String(tiempo_termina_tiro));
        for (int i=1;i<=4;i++)
          {
          Disparo_Tira_Leds();
          }
        posicion_nueva=SIN_DETECCION;
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
        Cambio_La_Frecuencia=SI;
      }
  }
}
      
/* ----------------------------------------------------- */
void Califica_La_Frecuencia()
{        
  if (Cambio_La_Frecuencia==SI)
    { 
      //Serial.println("Entro a Califica Frecuencia");
      // entre freceuncia de disparo y disparo+500
      if ((nueva_frecuencia>frecuencia_disparo)&&(nueva_frecuencia<frecuencia_disparo+500)) // rojo 1300
       {
        posicion_nueva=DISPARO_DETECTADO;
        //Serial.println("Disparo detectado");

        // prepara variables para envio de datos a Monitor
        activa_envio_de_resultados=SI;
        flujo_de_envio=PREPARA_PAQUETE_ENVIO;

        // marca el tiempo final del tiro
        tiempo_termina_tiro=millis();

       }
      else
       {
        if ((nueva_frecuencia>frecuencia_apunta)&&(nueva_frecuencia>frecuencia_apunta)) // rojo 700
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
       //Se deshabilita la calificacion
      Cambio_La_Frecuencia=NO;
    }
}



/*--------------------------------------------------------*/
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

/*------------------------------------------------------------*/
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

/* ---------------------------------------------------------------------*/
void Disparo_Tira_Leds()
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



