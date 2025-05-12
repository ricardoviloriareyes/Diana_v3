#include <Arduino.h>
//#include "nvs.h"
//#include "nvs_flash.h"
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
//#include <RTClib.h>
# include <Adafruit_NeoPixel.h>
//# include <Adafruit_BusIO_Register.h>
//# include <Adafruit_I2CDevice.h>
//# include <Adafruit_I2CRegister.h>
//# include <Adafruit_SPIDevice.h>

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

// direcciones mac de ESTE RECEPTOR (PARA SER CAPTURADA EN EL  MONITOR)
uint8_t   broadcastAddress1[]={0x24,0x62,0xAB,0xDC,0xAC,0xF4};
//4c:eb:d6:75:48:30

//Monitor Central para regreso de resultados
uint8_t   broadcastAddressMonitor[]={0x4C,0xEB,0xD6,0x75,0x48,0x30}; 


#define NO 0
#define SI 1

// Colores del disparo
#define COLOR_SIN_ASIGNAR 0
#define RED   1
#define GREEN 2
#define BLUE  3
#define ARCOIRIS 4
uint8_t color_de_disparo=COLOR_SIN_ASIGNAR;

          
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

//JUGADORES
#define JUGADOR_NO_ACTIVO 0
#define JUGADOR1 1
#define JUGADOR2 2
#define JUGADOR3 3
#define JUGADOR4 4


//ESPERA
#define UNO 1
#define DOS 2
#define TRES 3
#define CUATRO 4
#define CINCO 5
#define SEIS 6
#define SIETE 7


// Valores posibles de variables de matriz led, no se utilizan solo referencia para asignacion
#define ATRAS 0
#define TINTA 1
#define BASE 2
#define RESALTA 3


#define APAGADO   0  //CD001
#define ENCENDIDO 1 

#define SIN_CAMBIO_ESTADO_RELOJ 0
#define DETECTA_CAMBIO_ESTADO_RELOJ 1
int reloj_1Seg =ENCENDIDO;
int reloj_500mseg=ENCENDIDO;
int reloj_250mseg =ENCENDIDO;
int relon_100mseg =ENCENDIDO;
int actualiza_display_1seg =SI;
int actualiza_display_500mseg =SI;
int actualiza_display_200mseg =SI;
int actualiza_display_100mseg=SI;
uint8_t salir_por_timeout=NO;
uint16_t tiempo_espera_salida=500;  // milisegundos


//Tipos de juegos
#define JUEGO_CLASICO 1
#define JUEGO_TORNEO 2
#define JUEGO_EQUIPOS 3
#define JUEGO_VELOCIDAD 4



// varibles para condiciones de manejo del tiro
#define PRIVADO 0
#define PUBLICO 1




// estructura de envio de datos al compañero(peer)
typedef struct structura_mensaje
{ 
  //control
  int n; // Id del paquete

  //propiedades del Juego
  uint8_t ju; //numero del juego 1 JUEGO_CLASICO, 2 JUEGO TORNEO, 3 JUEGO EQUIPOS , 4 JUEGO VELOCIDAD
  uint8_t jr; //numero del round corresponde al round que esta corriendo en cada juego 
  
  //Propiedades del Paquete
  uint8_t t; //test
  uint8_t d; //diana
  uint8_t c; //color
  uint8_t p; //No. de jugador
 
  //propiedades del tiro
  uint8_t s; //numero asignado de tiro, sirve para las figuras numeros CD001

  //Condiciones de manejo del tiro
  uint8_t pp; // tiro privado (solo jugador original ) o publico (cualquiera de los dos)
  uint8_t pa; // No. de jugador alternativo en caso de ser tiro publico

  // propiedades de figura
  uint16_t vf; //velocidad de cambio de la figura valores netos como 1500, 2000 o 3000 milisegundos
  uint8_t  tf;  //tipo de la figura SIN_DEFINIR=0, NORMAL=1, BONO=2, CASTIGO=3
  uint8_t  vt; // se contabiliza el tiro como , NORMAL= 1 tiro, BONO =1 tiro, CASTIGO=0 
  uint8_t  xf;  //iniciar con figura 

  // resultado OBTENIDO del disparo
  uint8_t  jg;     // No. Jugador Impacto
  int16_t  po;     // puntuacion obtenida del disparo
  uint8_t  fi;     // figura impactada para marcador en monitor


} structura_mensaje;

/* fin de importacion*/

structura_mensaje datos_enviados;
structura_mensaje datos_recibidos;
structura_mensaje datos_locales_diana;  // para usar en diana

#define TEST 0
#define TIRO_ACTIVO 1
#define JUGADOR_READY 2

/* 3MAYO2025 SE ANEXA*/



#define NORMAL  1
#define BONO    2
#define CASTIGO 3
#define CALIBRA 4

//valores para Tipos de figuras  a enviar

#define TIPO_TEST    0
#define TIPO_NORMAL  1
#define TIPO_BONO    2
#define TIPO_CASTIGO 3
#define TIPO_CALIBRA 4
uint8_t tipo_de_figura=TIPO_NORMAL;



//valores para tipos de figuras a enviar
//Normal
#define FIGURA_SIN_DEFINIR 0
#define NORMAL_CONEJO 1
#define NORMAL_ZORRA   2
#define NORMAL_ARANA   3
#define NORMAL_LAGARTIJA  4
#define NORMAL_SIN_IMPACTO 5

#define PUNTOS_FIGURA_SIN_DEFINIR 0
#define PUNTOS_NORMAL_CONEJO 45
#define PUNTOS_NORMAL_ZORRA  35
#define PUNTOS_NORMAL_ARANA  25
#define PUNTOS_NORMAL_LAGARTIJA 15
#define PUNTOS_NORMAL_SIN_IMPACTO 0

// Bono
#define BONO_CORAZON 1
#define BONO_OSO 2
#define BONO_RANA 3 
#define BONO_CONEJO 4
#define BONO_SIN_IMPACTO 5

#define PUNTOS_BONO_CORAZON 95
#define PUNTOS_BONO_OSO 70
#define PUNTOS_BONO_RANA 50
#define PUNTOS_BONO_CONEJO 45
#define PUNTOS_BONO_SIN_IMPACTO 0


//Castigo
#define CASTIGO_UNICORNIO 1
#define CASTIGO_BOTELLA 2
#define CASTIGO_COPA 3
#define CASTIGO_SIN_IMPACTO 5



#define PUNTOS_CASTIGO_UNICORNIO -300
#define PUNTOS_CASTIGO_BOTELLA -200
#define PUNTOS_CASTIGO_COPA -100
#define PUNTOS_CASTIGO_SIN_IMPACTO 0

//Calibra
#define CALIBRA_UNO  1

//Movimiento de figuras
#define IZQUIERDA 0
#define DERECHA   1
uint8_t lado =IZQUIERDA;
uint8_t contador_vueltas =1;


/*FIN DE ANEXO 3MAYO2025*/

uint8_t tipo_tiro_recibido= TIRO_ACTIVO;
uint16_t no_paquete_test=2000;
int16_t valor_puntuacion_figura=0;

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
#define NUMPIXELS 256
Adafruit_NeoPixel tira(NUMPIXELS,pin_leds,NEO_RGB+NEO_KHZ800);
int prende_leds=SI;


/* -------------------PASO 0 INICIO DE ESP-NOW ------------------*/
void Inicia_ESP_NOW()
{
  Serial.println("Inicia ESPNOW");
  if (esp_now_init() != ESP_OK)
    {
      Serial.println("Error initializing ESP-NOW");
      tira.setPixelColor(0,tira.Color(250,0,0));
      tira.show();
      return;
    }
  else
  {
      Serial.println("initializing ESP-NOW  OK");
      tira.setPixelColor(0,tira.Color(0,0,250));
      tira.show();
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
        tira.setPixelColor(1,tira.Color(250,0,0));
        tira.show();
        Serial.println("Failed to add peer Monitor");
        return;
     }
   else
      { 
        tira.setPixelColor(1,tira.Color(0,0,250));
        tira.show();
        Serial.println("Add Peer Monitor OK");

      }
}


/* ------------------PASO 3 PRUEBA DE CONEXION ---------------------------------------------------*/
void Test_Conexion_Diana()
{ 
  // DATOS A ENVIAR  
    if (no_paquete_test>=2101)
      {no_paquete_test=2000;}

    datos_enviados.n=no_paquete_test;
    datos_enviados.t=TEST;
    datos_enviados.c=1;   //SE MANDA pixel_rojo PARA TEST
    datos_enviados.p=JUGADOR_NO_ACTIVO; // JUGADOR 0 NO ACTIVO

  // monitor
    esp_err_t result4 = esp_now_send(broadcastAddressMonitor, (uint8_t *) &datos_enviados, sizeof(structura_mensaje));
    delay(500);
    if (result4 == ESP_OK) 
     {
      tira.setPixelColor(2,tira.Color(0,0,250));
      tira.show();
      Serial.println("TEST-Envio a Monitor OK");
      no_paquete_test++;
     }

    else 
     {
      tira.setPixelColor(2,tira.Color(250,0,0));
      tira.show();
      Serial.println("TEST-Error enviando a Monitor");
     }
}

 /* -------------VARIABLES DE RECEPCION DE DATOS---------------------------------*/

int8_t pixel_verde=0;
int8_t pixel_rojo=0;
int8_t pixel_azul=0;
uint8_t color_original=COLOR_SIN_ASIGNAR;
uint8_t Posicion_Aro =1;


//estado de diana
#define APAGADO 0
#define ENCENDIDO 1
int case_estado_diana =APAGADO;


// revisar la frecuencia con osciloscopio del disparo
/* variables para analisis de frecuencia */
ulong frecuencia_disparo=10000;
ulong frecuencia_apunta=5000;

ulong frecuencia_disparo_tiro_publico=0 ; 
ulong frecuencia_apunta_tiro_publico =0 ; 
 
ulong frecuencia_disparo_tiro_verde=1300 ; //1300;
ulong frecuencia_apunta_tiro_verde= 800 ; // 800;

ulong frecuencia_disparo_tiro_azul =1300; //1300
ulong frecuencia_apunta_tiro_azul= 800; //800

#define SIN_DETECCION 0
#define APUNTA_VERDE 1
#define DISPARO_VERDE 2
#define APUNTA_AZUL 3
#define DISPARO_AZUL 4
uint8_t valor_y_color_del_laser=SIN_DETECCION;
uint8_t figura_actual=FIGURA_SIN_DEFINIR;


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

volatile unsigned long tiempo_inicial_reloj_1seg=0;
volatile unsigned long tiempo_actual_reloj_1seg=0;

volatile unsigned long tiempo_inicial_reloj_500mseg=0;
volatile unsigned long tiempo_actual_reloj_500mseg=0;

volatile unsigned long tiempo_inicial_reloj_200mseg=0;
volatile unsigned long tiempo_actual_reloj_200mseg=0;

volatile unsigned long tiempo_inicial_reloj_100mseg=0;
volatile unsigned long tiempo_actual_reloj_100mseg=0;


uint8_t propietario_original_id=0;
uint8_t propietario_original_color=0;
uint8_t propietario_alternativo_id=0;
uint8_t propietario_alternativo_color=0;


/* -------------------- RECEPCION DE DATOS -----------------------*/
// Callback when data is received
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) 
{
  memcpy(&datos_recibidos, incomingData, sizeof(datos_recibidos));
  //informacion de longuitud de paquete

  Serial.println("No. de paquete recibido :"+String(datos_recibidos.n));
  Serial.print("Bytes received: ");  Serial.println(len);
 
  char macStr[18];
  Serial.print("Packet received from: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
  
  Serial.println("Tiro activo = "+String(datos_recibidos.t));

  // se guada tipo de tiro recibido para validar el rady de loc jugadores de contadores descendentes
  // SE REGRESA JUGADOR READY PARA EVITAR ACUMULAR TIROS EN JUGADOR

  if (datos_recibidos.t>=TIRO_ACTIVO)  // TIRO ACTIVO =1 &&  JUGADOR READY =2 
    { 
      //inicializa los pulsos para analisis de frecuencia
      pulsos=0;
      
      //Asigna "color original" de tirador privado -> rojo,verde o azul
      Asigna_Pixeles_Para_Definir_Color_Original(datos_recibidos.c);

      //Asigna frecuencia de tirador privado para "apunta" y "dispara"
      Asigna_Frecuencias_Base_Para_Tiro(datos_recibidos.c);

      case_estado_diana=ENCENDIDO;
      Serial.println("case_estado_diana= ENCENDIDO");
    }
  else
  {
    // tiro test
  }
}

//declaracion de funciones
void Ponderacion_Y_Obtencion_De_Frecuencia2();
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len);
void Test_Conexion_Diana();
void readMacAddress();
void Inicia_ESP_NOW();
void Peer_Monitor_En_Linea();
void Envia_Resultados_Al_Monitor();

int reloj_1seg();
int reloj_500mseg();
int reloj_200mseg();
int reloj_100mseg();


// figuras
void Matriz_Conejo();
void Matriz_Zorra();
void Matriz_Arana();
void Matriz_Lagartija();
void Matriz_TimeOut();
void Matriz_Corazon();
void Matriz_Oso();
void Matriz_Rana();
void Matriz_Unicornio();
void Matriz_Botella();
void Matriz_Copa();
void Matriz_Sin_Castigo();

//Contadores
int i;

/*-----------------------------------------------------------------------------*/
//Contador para AttachInterrupt para proceso de identificacion de freceuncia recibida por laser
void Cuenta_Pulso()
{
  pulsos++;
}

// Pantalla fantasma para matriz 16x16
//Se pone una sola linea ya que concide con la tira gracias a la hoja de excel que ya da el valor
int Pantalla[256];

void Asigna_Pantalla()
{
  for (int posicion_led=0;posicion_led<=255;posicion_led++) 
    { 
    Pantalla[posicion_led]=posicion_led;
    } 
}


/* -----------------------INICIO    S E T U P --------------------------------*/

void setup() 
{
  Asigna_Pantalla();
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

  // inicia Neopixel tira
  tira.begin();
  tira.show();
  tira.setBrightness(100);

  // para iniciar el flasheo de la tira


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

  Actualiza_Relojes();
    //Control Diana
  switch (case_estado_diana)
  {
    case APAGADO:
      //Serial.print(".");
      /* code */
      break;

    case ENCENDIDO:
      /* code */
     //PASO 1 proceso encendido
      switch (case_proceso_encendido)
        {
            case INICIA_STANDBYE:
                  //Reinicia matriz para analisis de frecuencia          
                  //Serial.println("reinicia frecuencias");
                  for (int i=0; i<=9;++i)
                  {
                    muestra[i]=0;
                  }
                 case_proceso_encendido=INICIALIZA_TIEMPO;
                 break;

            case INICIALIZA_TIEMPO:
                  // incia tiempo inicial del tiro
                  tiempo_inicia_tiro=millis(); 
                  tira.clear();
                  Serial.println("Inicializa tiempo (mseg): "+String(tiempo_inicia_tiro));
                  case_proceso_encendido=MONITOREA_DISPARO;
                  break;

            case MONITOREA_DISPARO:
                  //Checa y evalua la frecuencia del tiro laser detectado 
                  Ponderacion_Y_Obtencion_De_Frecuencia2();
                  //Encuentra a que color y valor que corresponde el laser detectado
                  valor_y_color_del_laser=Calcula_Color_Y_Nivel_Del_Disparo();
                  //el valor de "valor_y_color_laser"--> SIN_RESULTADO, APUNTA_VERDE, APUNTA_AZUL, DISPARO_VERDE, DISPARO_AZUL
 
                  // figura de fondo la imprime c/200seg par dar movimientos cn base al tiempo recibido 
                  if (reloj_200mseg()==DETECTA_CAMBIO_ESTADO_RELOJ)
                    {
                      //Dibuja figura y establece su valor en ese momento
                      Switchea_Izquierda_Por_Derecha(); //activa movimiento patas cada 300 milisegundos
                      figura_actual=Evalua_y_Dibuja_Figura(); //evalua y Dibuja figura cada 100 milisegundos
                      tira.show(); 
                    }

                   //En espera de un laser
                  if (valor_y_color_del_laser==SIN_DETECCION) //SIN_DETECCION=0
                        {
                          if (reloj_200mseg()==DETECTA_CAMBIO_ESTADO_RELOJ)
                          {
                          Aro_Sin_Deteccion();
                          tira.show();                            
                          }
                        }

                  //Detecta un laser apuntando
                  if ((valor_y_color_del_laser==APUNTA_VERDE) || (valor_y_color_del_laser==APUNTA_AZUL))
                          {
                            if (reloj_200mseg()==DETECTA_CAMBIO_ESTADO_RELOJ)
                            {
                              Aro_Apuntando();
                              tira.show();                              
                            }
                          }

                  // Detecta un laser disparando
                     if ((valor_y_color_del_laser==DISPARO_VERDE) || (valor_y_color_del_laser==DISPARO_AZUL))
                      {
                        Aro_Disparo(); //Se queda 250 milisegundos y sale con valor de posicion nueva DISPARO_DETECTADO
                        //no lleva tira.show() ya que se activa dentro de cada fase del Aro_Disparo
                      }                 
                    
                  //Deteccion de disparo o por timeout de espera que fue asignado en las figuras
                  if ((posicion_nueva==DISPARO_DETECTADO) || (salir_por_timeout==SI)) //posicion nueva es modificado en califica frecuencia
                    {
                       // marca el tiempo final del tiro
                      tiempo_termina_tiro=millis(); 

                      //Serial.println("-->posicion_nueva=DISPARO_DETECTADO");
                      //Sela del MONITOREO  y selecciona ENVIO DE RESULTADOS
                      case_proceso_encendido=ENVIA_RESULTADOS;
                      flujo_de_envio=PREPARA_PAQUETE_ENVIO;

                      //Activa el envio del resultado
                      activa_envio_de_resultados=SI;
                      //reinicia la deteccion del laser
                      posicion_nueva=SIN_DETECCION;
                    }
                  break;

            case ENVIA_RESULTADOS:
                  Envia_Resultados_Al_Monitor();
                  break;

            case FINALIZA_REINICIA_VARIBLES:
                  Reinicia_Todas_Las_Variables_Al_Termnar();
                  break;

        } //fin case case_proceso_encendido
      break;
  } // fin de switch case_estado_diana

  //PROCESO FINAL, ENVIA LOS DATOS AMONITOR Y APAGA DIANA, tiene que estar en el principal por requerimiento del esp_now_send
  if (enviar_datos==SI) 
      {
        Serial.println("ENVIO PAQUETE :"+String(datos_enviados.n));
        esp_err_t result = esp_now_send(broadcastAddressMonitor,(uint8_t *) &datos_enviados,sizeof(structura_mensaje)); 
        delay(250);        
        if (result == ESP_OK) 
            {
              case_proceso_encendido=FINALIZA_REINICIA_VARIBLES;
              Serial.println("Envio Paquete OK, intento : "+String(intentos_envio));
              enviar_datos=NO;
              tira.clear();
              tira.show();
              for (int i=0; i<=15;++i)
                {tira.setPixelColor(i,tira.Color(0,250,0));
                }
              tira.show();
            }
        else 
            {
              Serial.println("Error envio : "+String(intentos_envio));
              intentos_envio++;
              flujo_de_envio=PAUSA_REENVIO;
              tira.clear();
              tira.show();
              tira.setPixelColor(1,tira.Color(0,250,0));
              tira.setPixelColor(1,tira.Color(1,250,0));
              tira.setPixelColor(1,tira.Color(2,250,0));
              tira.show();
              previous_time_reenvio=millis();
            }
      } // fin if enviar_datos

}
// FIN  loop ---------------------------------------------------

void Reinicia_Todas_Las_Variables_Al_Termnar()
{
    // finaliza envio
    Serial.println("APAGA DIANA");
    case_estado_diana=APAGADO;
    pulsos=0;
    captura_tiempo_inicial=SI;;
    flujo_de_envio=STANDBYE;
    salir_por_timeout=NO;

    // reinicia varibles de menus
    case_proceso_encendido = INICIA_STANDBYE;
    prende_leds=SI;
    led_inicio=0;

    // tiempo de duracion del tiro
    tiempo_inicia_tiro=0;
    tiempo_termina_tiro=0;
    activa_envio_de_resultados=NO;
    salir_por_timeout=NO;

    //inicializa variables de freceuncia
    rango_actual=0;
    cambio_la_frecuencia =NO;
    posicion_nueva       =SIN_DETECCION;
    captura_tiempo_inicial=SI;
    promedio_muestreo=0;
    nueva_frecuencia=0;
    acumulado=0;

    // reinicia el acumulado y rangos
    frecuencia_disparo=0;
    frecuencia_apunta=0;
}


void Actualiza_Relojes()
{
  // para switchear los parpadeos de las figuras 
  tiempo_actual_reloj_1seg=millis();
  tiempo_actual_reloj_500mseg=millis();
  tiempo_actual_reloj_200mseg=millis();
  tiempo_actual_reloj_100mseg=millis();
}

/*-----------------------------------------------*/
void Envia_Resultados_Al_Monitor()
{ Serial.println("Envia resultados al monitor") ;              
  switch (flujo_de_envio)
    {
      case STANDBYE:
        /* solo espera */
        break;

      case PREPARA_PAQUETE_ENVIO:
        Arma_Paquete_De_Envio();
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
                tira.clear();
                for (int i=2;i<=32;i++)
                  {
                    tira.setPixelColor(i,tira.Color(0,250,0));
                  }
                tira.show();
                Serial.println("APAGA DIANA en PAUSA REENVIO");
                case_estado_diana=APAGADO;
                captura_tiempo_inicial=NO;
              }
          }
          break;
      } //fin switch flujo_de_envio
}





/* ----------------------------------------------------------*/


/* --------------------------------------------------------*/
void Ponderacion_Y_Obtencion_De_Frecuencia2()
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

/* ----------------------------------------------------- */
uint8_t  Calcula_Color_Y_Nivel_Del_Disparo()
{     
  uint8_t resultado=0;   
  if (cambio_la_frecuencia==SI)
    { 
      //evalua sin deteccion
      if ((nueva_frecuencia<(frecuencia_apunta_tiro_verde-500)))
        {
          resultado= SIN_DETECCION;
        }
      //evalua apunta verde
      if (nueva_frecuencia>(frecuencia_apunta_tiro_verde-500) && (nueva_frecuencia<(frecuencia_apunta_tiro_verde+500)))
        {
          resultado= APUNTA_VERDE;
          color_de_disparo=GREEN;
        }
      //evalua disparo_verd
      if (nueva_frecuencia>(frecuencia_disparo_tiro_verde-500) && (nueva_frecuencia<(frecuencia_disparo_tiro_verde+500)))
        {
          resultado= DISPARO_VERDE;
          color_de_disparo=GREEN;

        }
      //evalua apunta AZUL
      if (nueva_frecuencia>(frecuencia_apunta_tiro_azul-500) && (nueva_frecuencia<(frecuencia_apunta_tiro_azul+500)))
        {
          resultado= APUNTA_AZUL;
          color_de_disparo=BLUE;

        }
      //evalua disparo AZUL
      if (nueva_frecuencia>(frecuencia_disparo_tiro_azul-500) && (nueva_frecuencia<(frecuencia_disparo_tiro_azul+500)))
        {
          resultado= DISPARO_AZUL;
          color_de_disparo=BLUE;

        }
      cambio_la_frecuencia=NO; //para generar un perido para la re-evaluacion de la frecuencia
      return resultado;
    }
}





/*--------------------------------------------------------*/



int reloj_1seg() //CD001
{
  tiempo_actual_reloj_1seg=millis();
  if ((tiempo_inicial_reloj_1seg+1000)<tiempo_actual_reloj_1seg)
    {
        tiempo_inicial_reloj_1seg=millis();
        actualiza_display_1seg=SI;
        return DETECTA_CAMBIO_ESTADO_RELOJ;
    }
  else
    {
      actualiza_display_1seg=NO;
      return SIN_CAMBIO_ESTADO_RELOJ;
    }
}

/* -------------------------------------------------------*/
int reloj_500mseg() //CD001
{
  tiempo_actual_reloj_500mseg=millis();
  if ((tiempo_inicial_reloj_500mseg+500)<tiempo_actual_reloj_500mseg)
    {
        tiempo_inicial_reloj_500mseg=millis();
        actualiza_display_500mseg=SI;
        return DETECTA_CAMBIO_ESTADO_RELOJ;      
    }
  else
    {
      actualiza_display_500mseg=NO;
      return SIN_CAMBIO_ESTADO_RELOJ;
    }
}

int reloj_200mseg() //CD001
{
  tiempo_actual_reloj_200mseg=millis();
  if ((tiempo_inicial_reloj_200mseg+100)<tiempo_actual_reloj_200mseg)
    {
        tiempo_inicial_reloj_200mseg=millis();
        actualiza_display_200mseg=SI;
        return DETECTA_CAMBIO_ESTADO_RELOJ;      
    }
  else
    {
      actualiza_display_200mseg=NO;
      return SIN_CAMBIO_ESTADO_RELOJ;
    }
}


int reloj_100mseg() //CD001
{
  tiempo_actual_reloj_100mseg=millis();
  if ((tiempo_inicial_reloj_100mseg+100)<tiempo_actual_reloj_100mseg)
    {
        tiempo_inicial_reloj_100mseg=millis();
        actualiza_display_100mseg=SI;
        return DETECTA_CAMBIO_ESTADO_RELOJ;      
    }
  else
    {
      actualiza_display_100mseg=NO;
      return SIN_CAMBIO_ESTADO_RELOJ;
    }
}





/* ---------------------------------------------------------------------*/




void  Asigna_Pixeles_Para_Definir_Color_Original(uint8_t local_color)
{
  switch (datos_locales_diana.c)
  {
    case RED:
      /*code*/
      //Serial.println("Programa color rojo para castigos");
      pixel_rojo=0;
      pixel_verde=250;
      pixel_azul=0;
      color_original=RED;
      break;           

    case GREEN:
      //Serial.println("Programa color verde");
      pixel_rojo=250;
      pixel_verde=0;
      pixel_azul=0;
      color_original=GREEN;
      break;

    case BLUE:
      //Serial.println("Programa color azul");
      pixel_rojo=0;
      pixel_verde=0;
      pixel_azul=250;
      color_original=BLUE;
      break;

  } 
}


void Asigna_Frecuencias_Base_Para_Tiro(uint8_t local_color)
{

  switch (local_color)
      {
        case GREEN:
          /*code*/
          frecuencia_apunta=frecuencia_apunta_tiro_verde;
          frecuencia_disparo=frecuencia_disparo_tiro_verde;
          break;

        case BLUE:
          /*code*/
          frecuencia_apunta=frecuencia_apunta_tiro_azul;
          frecuencia_disparo=frecuencia_disparo_tiro_azul;
          break;

        case RED:
           /*code  solo de relleno, no debe de usarse esta opcion,solo es valida en nivel Publico
            y ahi se decide directamente una frecuencia OR la otra frecuencia*/
           frecuencia_apunta=frecuencia_apunta_tiro_verde;
           frecuencia_disparo=frecuencia_disparo_tiro_verde;
           break;
      }
}


uint8_t Evalua_y_Dibuja_Figura()
{
  uint8_t resultado=FIGURA_SIN_DEFINIR;
  switch (datos_recibidos.tf) // tipo de figura
  {
    case TIPO_NORMAL:
      /*case*/
      resultado=Evalua_Figura_Normal();
      break;
    case TIPO_BONO:
      /*case*/
      resultado=Evalua_Figura_Bono();
      break;
    case TIPO_CASTIGO:
      /*code*/
      resultado=Evalua_Figura_Castigo();
      break;
    case TIPO_CALIBRA:
      /*code*/
      resultado=Evalua_Figura_Calibra();
      break;
    case TIPO_TEST:
        /*code*/
        //Se supone que nunca va a habilitarse ya que test no prende la diana
        resultado=Evalua_Figura_Calibra();
        break;
  }
  return resultado;
}

/*------------------------------------------------------------------------*/
uint8_t Evalua_Figura_Normal()
{ 
  uint8_t resultado_figura=FIGURA_SIN_DEFINIR;
  int rango = millis()-tiempo_inicia_tiro; 

  //clasifica la figura dependiendo del tiempo df (velocidad de cambio figura)

  if (rango<datos_recibidos.vf)
    {
      resultado_figura=NORMAL_CONEJO;
      valor_puntuacion_figura=PUNTOS_NORMAL_CONEJO;
      Matriz_Conejo();
    }
  else
    {
      if (rango<(datos_recibidos.vf*2))
        {
          resultado_figura=NORMAL_ZORRA;
          valor_puntuacion_figura=PUNTOS_NORMAL_ZORRA;
          Matriz_Zorra();
        }
      else
        {
          if (rango<(datos_recibidos.vf*3))
            {
              resultado_figura=NORMAL_ARANA;
              valor_puntuacion_figura=PUNTOS_NORMAL_ARANA;
              Matriz_Arana();
            }
          else
          {
            if (rango<(datos_recibidos.vf*4))
              {
                resultado_figura=NORMAL_LAGARTIJA;
                valor_puntuacion_figura=PUNTOS_NORMAL_LAGARTIJA;
                Matriz_Lagartija();
              }
            else
              {
                resultado_figura=NORMAL_SIN_IMPACTO;
                valor_puntuacion_figura=PUNTOS_NORMAL_SIN_IMPACTO; 
                Matriz_TimeOut();
                tira.show();    
                delay(tiempo_espera_salida);
                salir_por_timeout=SI;           
              } //end if vf*4
          }//end if vf*3
        } //end if vf*2
    } //end if vf
    return resultado_figura;
}

/*-----------------------------------------------------------------------*/
uint8_t Evalua_Figura_Bono()
{
  //Mostrara la figura por dos periodos, en caso de no impacto manda el valor a cero
  uint8_t resultado=FIGURA_SIN_DEFINIR;
  int rango = millis()-tiempo_inicia_tiro; 
  if (rango<(datos_recibidos.vf*2))
    {
      switch (datos_recibidos.xf)
      {
        case BONO_CORAZON:
          resultado=datos_recibidos.xf;
          valor_puntuacion_figura=PUNTOS_BONO_CORAZON;  
          Matriz_Corazon(); 
          break;

        case BONO_OSO:
          resultado=datos_recibidos.xf;
          valor_puntuacion_figura=PUNTOS_BONO_OSO;  
          Matriz_Oso(); 
          break; 

        case BONO_RANA:
          resultado=datos_recibidos.xf;
          valor_puntuacion_figura=PUNTOS_BONO_RANA; 
          Matriz_Rana();  
          break;  

        case BONO_CONEJO:
          resultado=datos_recibidos.xf;
          valor_puntuacion_figura=PUNTOS_BONO_CONEJO;
          Matriz_Conejo();   
          break;         
      }
    }
  else
    {
      resultado=BONO_SIN_IMPACTO;
      valor_puntuacion_figura=PUNTOS_BONO_SIN_IMPACTO; 
      Matriz_TimeOut();
      tira.show();
      delay(tiempo_espera_salida);  
      salir_por_timeout=SI;
    }
  tira.show();
  return resultado;
}

uint8_t Evalua_Figura_Castigo()
{
  uint8_t resultado=FIGURA_SIN_DEFINIR;
  int rango = millis()-tiempo_inicia_tiro; 
  if (rango<(datos_recibidos.vf*2))
    {
      switch (datos_recibidos.xf)
      {
        case CASTIGO_UNICORNIO:
          resultado=datos_recibidos.xf;
          valor_puntuacion_figura=PUNTOS_CASTIGO_UNICORNIO;
          Matriz_Unicornio();   
          break;

        case CASTIGO_BOTELLA:
          resultado=datos_recibidos.xf;
          valor_puntuacion_figura=PUNTOS_CASTIGO_BOTELLA;
          Matriz_Botella();   
          break; 

        case CASTIGO_COPA:
          resultado=datos_recibidos.xf;

          valor_puntuacion_figura=PUNTOS_CASTIGO_COPA; 
          Matriz_Copa();  
          break;         
      }
    }
  else
    {
      resultado=CASTIGO_SIN_IMPACTO;
      valor_puntuacion_figura=PUNTOS_CASTIGO_SIN_IMPACTO;
      Matriz_Sin_Castigo(); 
      tira.show();
      delay(tiempo_espera_salida);
      salir_por_timeout=SI; 
    }
  tira.show();
  return resultado;
}


uint8_t Evalua_Figura_Calibra()
{
  uint8_t resultado=FIGURA_SIN_DEFINIR;
  int rango = millis()-tiempo_inicia_tiro; 
  if (rango<(datos_recibidos.vf*4))
    {
      resultado=CALIBRA_UNO;
      valor_puntuacion_figura=0;
      Matriz_Calibra();   
    }
  else
    {
      resultado=CALIBRA_UNO;
      valor_puntuacion_figura=0;
      Matriz_TimeOut(); 
      tira.show();
      delay(tiempo_espera_salida);
      salir_por_timeout=SI; 
    }
  tira.show();
  return resultado;
}



void Switchea_Izquierda_Por_Derecha()
{
  if (lado==IZQUIERDA)
        {
          lado=DERECHA;
        }
  else
        {
          lado=IZQUIERDA;
        }
 }

/*----------------*/
void Matriz_Conejo()
{
  if (lado==IZQUIERDA)
    {
      
      tira.setPixelColor(Pantalla[ 1],255,255,255);tira.setPixelColor(Pantalla[ 2],105,210,30);tira.setPixelColor(Pantalla[ 3],105,210,30);tira.setPixelColor(Pantalla[ 4],105,210,30);tira.setPixelColor(Pantalla[ 5],255,255,255);tira.setPixelColor(Pantalla[ 10],255,255,255);tira.setPixelColor(Pantalla[ 11],105,210,30);tira.setPixelColor(Pantalla[ 12],105,210,30);tira.setPixelColor(Pantalla[ 13],105,210,30);tira.setPixelColor(Pantalla[ 14],255,255,255);
      tira.setPixelColor(Pantalla[ 30],255,255,255);tira.setPixelColor(Pantalla[ 29],255,255,255);tira.setPixelColor(Pantalla[ 28],105,210,30);tira.setPixelColor(Pantalla[ 27],105,210,30);tira.setPixelColor(Pantalla[ 26],255,255,255);tira.setPixelColor(Pantalla[ 21],255,255,255);tira.setPixelColor(Pantalla[ 20],105,210,30);tira.setPixelColor(Pantalla[ 19],105,210,30);tira.setPixelColor(Pantalla[ 18],255,255,255);tira.setPixelColor(Pantalla[ 17],255,255,255);
      tira.setPixelColor(Pantalla[ 34],255,255,255);tira.setPixelColor(Pantalla[ 35],255,255,255);tira.setPixelColor(Pantalla[ 36],255,255,255);tira.setPixelColor(Pantalla[ 37],105,210,30);tira.setPixelColor(Pantalla[ 38],255,255,255);tira.setPixelColor(Pantalla[ 41],255,255,255);tira.setPixelColor(Pantalla[ 42],105,210,30);tira.setPixelColor(Pantalla[ 43],255,255,255);tira.setPixelColor(Pantalla[ 44],255,255,255);tira.setPixelColor(Pantalla[ 45],255,255,255);
      tira.setPixelColor(Pantalla[ 59],255,255,255);tira.setPixelColor(Pantalla[ 58],105,210,30);tira.setPixelColor(Pantalla[ 57],255,255,255);tira.setPixelColor(Pantalla[ 56],255,255,255);tira.setPixelColor(Pantalla[ 55],255,255,255);tira.setPixelColor(Pantalla[ 54],255,255,255);tira.setPixelColor(Pantalla[ 53],105,210,30);tira.setPixelColor(Pantalla[ 52],255,255,255);
      tira.setPixelColor(Pantalla[ 68],105,210,30);tira.setPixelColor(Pantalla[ 69],255,255,255);tira.setPixelColor(Pantalla[ 70],255,255,255);tira.setPixelColor(Pantalla[ 71],255,255,255);tira.setPixelColor(Pantalla[ 72],255,255,255);tira.setPixelColor(Pantalla[ 73],255,255,255);tira.setPixelColor(Pantalla[ 74],255,255,255);tira.setPixelColor(Pantalla[ 75],105,210,30);
      tira.setPixelColor(Pantalla[ 92],255,255,255);tira.setPixelColor(Pantalla[ 91],255,255,255);tira.setPixelColor(Pantalla[ 90],255,255,255);tira.setPixelColor(Pantalla[ 89],255,255,255);tira.setPixelColor(Pantalla[ 88],255,255,255);tira.setPixelColor(Pantalla[ 87],255,255,255);tira.setPixelColor(Pantalla[ 86],255,255,255);tira.setPixelColor(Pantalla[ 85],255,255,255);tira.setPixelColor(Pantalla[ 84],255,255,255);tira.setPixelColor(Pantalla[ 83],255,255,255);
      tira.setPixelColor(Pantalla[ 98],255,255,255);tira.setPixelColor(Pantalla[ 99],255,255,255);tira.setPixelColor(Pantalla[ 100],255,255,255);tira.setPixelColor(Pantalla[ 101],255,255,255);tira.setPixelColor(Pantalla[ 102],255,255,255);tira.setPixelColor(Pantalla[ 103],255,255,255);tira.setPixelColor(Pantalla[ 104],255,255,255);tira.setPixelColor(Pantalla[ 105],255,255,255);tira.setPixelColor(Pantalla[ 106],255,255,255);tira.setPixelColor(Pantalla[ 107],255,255,255);tira.setPixelColor(Pantalla[ 108],255,255,255);tira.setPixelColor(Pantalla[ 109],255,255,255);
      tira.setPixelColor(Pantalla[ 125],255,255,255);tira.setPixelColor(Pantalla[ 124],255,255,255);tira.setPixelColor(Pantalla[ 123],0,0,0);tira.setPixelColor(Pantalla[ 122],255,255,255);tira.setPixelColor(Pantalla[ 121],255,255,255);tira.setPixelColor(Pantalla[ 120],255,255,255);tira.setPixelColor(Pantalla[ 119],255,255,255);tira.setPixelColor(Pantalla[ 118],255,255,255);tira.setPixelColor(Pantalla[ 117],255,255,255);tira.setPixelColor(Pantalla[ 116],0,0,0);tira.setPixelColor(Pantalla[ 115],255,255,255);tira.setPixelColor(Pantalla[ 114],255,255,255);
      tira.setPixelColor(Pantalla[ 130],255,255,255);tira.setPixelColor(Pantalla[ 131],255,255,255);tira.setPixelColor(Pantalla[ 132],0,0,0);tira.setPixelColor(Pantalla[ 133],255,255,255);tira.setPixelColor(Pantalla[ 134],255,255,255);tira.setPixelColor(Pantalla[ 135],255,255,255);tira.setPixelColor(Pantalla[ 136],255,255,255);tira.setPixelColor(Pantalla[ 137],255,255,255);tira.setPixelColor(Pantalla[ 138],255,255,255);tira.setPixelColor(Pantalla[ 139],0,0,0);tira.setPixelColor(Pantalla[ 140],255,255,255);tira.setPixelColor(Pantalla[ 141],255,255,255);
      tira.setPixelColor(Pantalla[ 157],255,255,255);tira.setPixelColor(Pantalla[ 156],255,255,255);tira.setPixelColor(Pantalla[ 155],255,255,255);tira.setPixelColor(Pantalla[ 154],255,255,255);tira.setPixelColor(Pantalla[ 153],255,255,255);tira.setPixelColor(Pantalla[ 150],255,255,255);tira.setPixelColor(Pantalla[ 149],255,255,255);tira.setPixelColor(Pantalla[ 148],255,255,255);tira.setPixelColor(Pantalla[ 147],255,255,255);tira.setPixelColor(Pantalla[ 146],255,255,255);
      tira.setPixelColor(Pantalla[ 162],255,255,255);tira.setPixelColor(Pantalla[ 163],255,255,255);tira.setPixelColor(Pantalla[ 164],255,255,255);tira.setPixelColor(Pantalla[ 165],255,255,255);tira.setPixelColor(Pantalla[ 166],255,255,255);tira.setPixelColor(Pantalla[ 167],255,255,255);tira.setPixelColor(Pantalla[ 168],255,255,255);tira.setPixelColor(Pantalla[ 169],255,255,255);tira.setPixelColor(Pantalla[ 170],255,255,255);tira.setPixelColor(Pantalla[ 171],255,255,255);tira.setPixelColor(Pantalla[ 172],255,255,255);tira.setPixelColor(Pantalla[ 173],255,255,255);
      tira.setPixelColor(Pantalla[ 189],255,255,255);tira.setPixelColor(Pantalla[ 188],255,255,255);tira.setPixelColor(Pantalla[ 187],255,255,255);tira.setPixelColor(Pantalla[ 186],255,255,255);tira.setPixelColor(Pantalla[ 185],255,255,255);tira.setPixelColor(Pantalla[ 184],255,255,255);tira.setPixelColor(Pantalla[ 183],255,255,255);tira.setPixelColor(Pantalla[ 182],255,255,255);tira.setPixelColor(Pantalla[ 181],255,255,255);tira.setPixelColor(Pantalla[ 180],255,255,255);tira.setPixelColor(Pantalla[ 179],255,255,255);tira.setPixelColor(Pantalla[ 178],255,255,255);
      tira.setPixelColor(Pantalla[ 195],255,255,255);tira.setPixelColor(Pantalla[ 196],255,255,255);tira.setPixelColor(Pantalla[ 197],255,255,255);tira.setPixelColor(Pantalla[ 198],255,255,255);tira.setPixelColor(Pantalla[ 199],255,255,255);tira.setPixelColor(Pantalla[ 200],255,255,255);tira.setPixelColor(Pantalla[ 201],255,255,255);tira.setPixelColor(Pantalla[ 202],255,255,255);tira.setPixelColor(Pantalla[ 203],255,255,255);tira.setPixelColor(Pantalla[ 204],255,255,255);
      tira.setPixelColor(Pantalla[ 218],255,255,255);tira.setPixelColor(Pantalla[ 217],255,255,255);tira.setPixelColor(Pantalla[ 216],255,255,255);tira.setPixelColor(Pantalla[ 215],255,255,255);tira.setPixelColor(Pantalla[ 214],255,255,255);tira.setPixelColor(Pantalla[ 213],255,255,255);
      
    }
  else
    {
      tira.setPixelColor(Pantalla[ 125],255,255,255);tira.setPixelColor(Pantalla[ 124],255,255,255);tira.setPixelColor(Pantalla[ 123],0,255,0);tira.setPixelColor(Pantalla[ 122],0,255,0);tira.setPixelColor(Pantalla[ 121],255,255,255);tira.setPixelColor(Pantalla[ 120],255,255,255);tira.setPixelColor(Pantalla[ 119],255,255,255);tira.setPixelColor(Pantalla[ 118],255,255,255);tira.setPixelColor(Pantalla[ 117],0,255,0);tira.setPixelColor(Pantalla[ 116],0,255,0);tira.setPixelColor(Pantalla[ 115],255,255,255);tira.setPixelColor(Pantalla[ 114],255,255,255);
      tira.setPixelColor(Pantalla[ 130],255,255,255);tira.setPixelColor(Pantalla[ 131],255,255,255);tira.setPixelColor(Pantalla[ 132],0,255,0);tira.setPixelColor(Pantalla[ 133],255,255,0);tira.setPixelColor(Pantalla[ 134],255,255,255);tira.setPixelColor(Pantalla[ 135],255,255,255);tira.setPixelColor(Pantalla[ 136],255,255,255);tira.setPixelColor(Pantalla[ 137],255,255,255);tira.setPixelColor(Pantalla[ 138],255,255,0);tira.setPixelColor(Pantalla[ 139],0,255,0);tira.setPixelColor(Pantalla[ 140],255,255,255);tira.setPixelColor(Pantalla[ 141],255,255,255);
      tira.setPixelColor(Pantalla[ 157],255,255,255);tira.setPixelColor(Pantalla[ 156],255,255,255);tira.setPixelColor(Pantalla[ 155],255,255,255);tira.setPixelColor(Pantalla[ 154],255,255,255);tira.setPixelColor(Pantalla[ 153],255,255,255);tira.setPixelColor(Pantalla[ 150],255,255,255);tira.setPixelColor(Pantalla[ 149],255,255,255);tira.setPixelColor(Pantalla[ 148],255,255,255);tira.setPixelColor(Pantalla[ 147],255,255,255);tira.setPixelColor(Pantalla[ 146],255,255,255);
      tira.setPixelColor(Pantalla[ 162],255,255,255);tira.setPixelColor(Pantalla[ 163],255,255,255);tira.setPixelColor(Pantalla[ 164],255,255,255);tira.setPixelColor(Pantalla[ 165],255,255,255);tira.setPixelColor(Pantalla[ 166],255,255,255);tira.setPixelColor(Pantalla[ 167],255,255,255);tira.setPixelColor(Pantalla[ 168],255,255,255);tira.setPixelColor(Pantalla[ 169],255,255,255);tira.setPixelColor(Pantalla[ 170],255,255,255);tira.setPixelColor(Pantalla[ 171],255,255,255);tira.setPixelColor(Pantalla[ 172],255,255,255);tira.setPixelColor(Pantalla[ 173],255,255,255);
      tira.setPixelColor(Pantalla[ 189],255,255,255);tira.setPixelColor(Pantalla[ 188],255,255,255);tira.setPixelColor(Pantalla[ 187],255,255,255);tira.setPixelColor(Pantalla[ 186],255,255,255);tira.setPixelColor(Pantalla[ 185],0,255,0);tira.setPixelColor(Pantalla[ 184],0,0,0);tira.setPixelColor(Pantalla[ 183],0,0,0);tira.setPixelColor(Pantalla[ 182],0,255,0);tira.setPixelColor(Pantalla[ 181],255,255,255);tira.setPixelColor(Pantalla[ 180],255,255,255);tira.setPixelColor(Pantalla[ 179],255,255,255);tira.setPixelColor(Pantalla[ 178],255,255,255);
      tira.setPixelColor(Pantalla[ 195],255,255,255);tira.setPixelColor(Pantalla[ 196],255,255,255);tira.setPixelColor(Pantalla[ 197],255,255,255);tira.setPixelColor(Pantalla[ 198],0,0,0);tira.setPixelColor(Pantalla[ 199],0,0,0);tira.setPixelColor(Pantalla[ 200],0,0,0);tira.setPixelColor(Pantalla[ 201],0,0,0);tira.setPixelColor(Pantalla[ 202],255,255,255);tira.setPixelColor(Pantalla[ 203],255,255,255);tira.setPixelColor(Pantalla[ 204],255,255,255);
      tira.setPixelColor(Pantalla[ 218],255,255,255);tira.setPixelColor(Pantalla[ 217],255,255,255);tira.setPixelColor(Pantalla[ 216],255,255,255);tira.setPixelColor(Pantalla[ 215],255,255,255);tira.setPixelColor(Pantalla[ 214],255,255,255);tira.setPixelColor(Pantalla[ 213],255,255,255);

    }

}

void Matriz_Zorra()
{
  if (lado==IZQUIERDA)
    {
      
      tira.setPixelColor(Pantalla[ 1],105,210,30);tira.setPixelColor(Pantalla[ 10],105,210,30);
      tira.setPixelColor(Pantalla[ 30],105,210,30);tira.setPixelColor(Pantalla[ 29],105,210,30);tira.setPixelColor(Pantalla[ 22],105,210,30);tira.setPixelColor(Pantalla[ 21],105,210,30);
      tira.setPixelColor(Pantalla[ 33],105,210,30);tira.setPixelColor(Pantalla[ 34],127,255,80);tira.setPixelColor(Pantalla[ 35],105,210,30);tira.setPixelColor(Pantalla[ 40],105,210,30);tira.setPixelColor(Pantalla[ 41],127,255,80);tira.setPixelColor(Pantalla[ 42],105,210,30);
      tira.setPixelColor(Pantalla[ 62],105,210,30);tira.setPixelColor(Pantalla[ 61],127,255,80);tira.setPixelColor(Pantalla[ 60],105,210,30);tira.setPixelColor(Pantalla[ 59],105,210,30);tira.setPixelColor(Pantalla[ 58],105,210,30);tira.setPixelColor(Pantalla[ 57],105,210,30);tira.setPixelColor(Pantalla[ 56],105,210,30);tira.setPixelColor(Pantalla[ 55],105,210,30);tira.setPixelColor(Pantalla[ 54],127,255,80);tira.setPixelColor(Pantalla[ 53],105,210,30);
      tira.setPixelColor(Pantalla[ 65],105,210,30);tira.setPixelColor(Pantalla[ 66],105,210,30);tira.setPixelColor(Pantalla[ 67],105,210,30);tira.setPixelColor(Pantalla[ 68],105,210,30);tira.setPixelColor(Pantalla[ 69],105,210,30);tira.setPixelColor(Pantalla[ 70],105,210,30);tira.setPixelColor(Pantalla[ 71],105,210,30);tira.setPixelColor(Pantalla[ 72],105,210,30);tira.setPixelColor(Pantalla[ 73],105,210,30);tira.setPixelColor(Pantalla[ 74],105,210,30);tira.setPixelColor(Pantalla[ 76],105,210,30);tira.setPixelColor(Pantalla[ 77],255,255,255);
      tira.setPixelColor(Pantalla[ 94],105,210,30);tira.setPixelColor(Pantalla[ 93],105,210,30);tira.setPixelColor(Pantalla[ 92],0,0,0);tira.setPixelColor(Pantalla[ 91],105,210,30);tira.setPixelColor(Pantalla[ 90],105,210,30);tira.setPixelColor(Pantalla[ 89],105,210,30);tira.setPixelColor(Pantalla[ 88],105,210,30);tira.setPixelColor(Pantalla[ 87],0,0,0);tira.setPixelColor(Pantalla[ 86],105,210,30);tira.setPixelColor(Pantalla[ 85],105,210,30);tira.setPixelColor(Pantalla[ 83],105,210,30);tira.setPixelColor(Pantalla[ 82],255,255,255);tira.setPixelColor(Pantalla[ 81],255,255,255);
      tira.setPixelColor(Pantalla[ 97],105,210,30);tira.setPixelColor(Pantalla[ 98],105,210,30);tira.setPixelColor(Pantalla[ 99],0,0,0);tira.setPixelColor(Pantalla[ 100],105,210,30);tira.setPixelColor(Pantalla[ 101],105,210,30);tira.setPixelColor(Pantalla[ 102],105,210,30);tira.setPixelColor(Pantalla[ 103],105,210,30);tira.setPixelColor(Pantalla[ 104],0,0,0);tira.setPixelColor(Pantalla[ 105],105,210,30);tira.setPixelColor(Pantalla[ 106],105,210,30);tira.setPixelColor(Pantalla[ 108],105,210,30);tira.setPixelColor(Pantalla[ 109],255,255,255);tira.setPixelColor(Pantalla[ 110],255,255,255);tira.setPixelColor(Pantalla[ 111],255,255,255);
      tira.setPixelColor(Pantalla[ 126],255,255,255);tira.setPixelColor(Pantalla[ 125],255,255,255);tira.setPixelColor(Pantalla[ 124],255,255,255);tira.setPixelColor(Pantalla[ 123],255,255,255);tira.setPixelColor(Pantalla[ 120],255,255,255);tira.setPixelColor(Pantalla[ 119],255,255,255);tira.setPixelColor(Pantalla[ 118],255,255,255);tira.setPixelColor(Pantalla[ 117],255,255,255);tira.setPixelColor(Pantalla[ 114],105,210,30);tira.setPixelColor(Pantalla[ 113],255,255,255);tira.setPixelColor(Pantalla[ 112],255,255,255);
      tira.setPixelColor(Pantalla[ 128],255,255,255);tira.setPixelColor(Pantalla[ 129],255,255,255);tira.setPixelColor(Pantalla[ 130],255,255,255);tira.setPixelColor(Pantalla[ 131],255,255,255);tira.setPixelColor(Pantalla[ 132],255,255,255);tira.setPixelColor(Pantalla[ 133],255,255,255);tira.setPixelColor(Pantalla[ 134],255,255,255);tira.setPixelColor(Pantalla[ 135],255,255,255);tira.setPixelColor(Pantalla[ 136],255,255,255);tira.setPixelColor(Pantalla[ 137],255,255,255);tira.setPixelColor(Pantalla[ 138],255,255,255);tira.setPixelColor(Pantalla[ 139],255,255,255);tira.setPixelColor(Pantalla[ 141],105,210,30);tira.setPixelColor(Pantalla[ 142],105,210,30);tira.setPixelColor(Pantalla[ 143],255,255,255);
      tira.setPixelColor(Pantalla[ 158],255,255,255);tira.setPixelColor(Pantalla[ 157],255,255,255);tira.setPixelColor(Pantalla[ 156],255,255,255);tira.setPixelColor(Pantalla[ 155],255,255,255);tira.setPixelColor(Pantalla[ 154],255,255,255);tira.setPixelColor(Pantalla[ 153],255,255,255);tira.setPixelColor(Pantalla[ 152],255,255,255);tira.setPixelColor(Pantalla[ 151],255,255,255);tira.setPixelColor(Pantalla[ 150],255,255,255);tira.setPixelColor(Pantalla[ 149],255,255,255);tira.setPixelColor(Pantalla[ 146],105,210,30);tira.setPixelColor(Pantalla[ 145],105,210,30);tira.setPixelColor(Pantalla[ 144],255,255,255);
      tira.setPixelColor(Pantalla[ 163],255,255,255);tira.setPixelColor(Pantalla[ 164],255,255,255);tira.setPixelColor(Pantalla[ 165],255,255,255);tira.setPixelColor(Pantalla[ 166],255,255,255);tira.setPixelColor(Pantalla[ 167],255,255,255);tira.setPixelColor(Pantalla[ 168],255,255,255);tira.setPixelColor(Pantalla[ 172],105,210,30);tira.setPixelColor(Pantalla[ 173],105,210,30);tira.setPixelColor(Pantalla[ 174],255,255,255);
      tira.setPixelColor(Pantalla[ 189],105,210,30);tira.setPixelColor(Pantalla[ 188],105,210,30);tira.setPixelColor(Pantalla[ 187],105,210,30);tira.setPixelColor(Pantalla[ 186],105,210,30);tira.setPixelColor(Pantalla[ 185],105,210,30);tira.setPixelColor(Pantalla[ 184],105,210,30);tira.setPixelColor(Pantalla[ 183],105,210,30);tira.setPixelColor(Pantalla[ 182],105,210,30);tira.setPixelColor(Pantalla[ 180],105,210,30);tira.setPixelColor(Pantalla[ 179],105,210,30);tira.setPixelColor(Pantalla[ 178],255,255,255);
      tira.setPixelColor(Pantalla[ 193],105,210,30);tira.setPixelColor(Pantalla[ 194],105,210,30);tira.setPixelColor(Pantalla[ 195],0,0,0);tira.setPixelColor(Pantalla[ 196],105,210,30);tira.setPixelColor(Pantalla[ 197],255,255,255);tira.setPixelColor(Pantalla[ 198],255,255,255);tira.setPixelColor(Pantalla[ 199],105,210,30);tira.setPixelColor(Pantalla[ 200],0,0,0);tira.setPixelColor(Pantalla[ 201],105,210,30);tira.setPixelColor(Pantalla[ 202],105,210,30);tira.setPixelColor(Pantalla[ 203],127,255,80);tira.setPixelColor(Pantalla[ 204],255,255,255);
      tira.setPixelColor(Pantalla[ 222],105,210,30);tira.setPixelColor(Pantalla[ 221],105,210,30);tira.setPixelColor(Pantalla[ 220],0,0,0);tira.setPixelColor(Pantalla[ 219],105,210,30);tira.setPixelColor(Pantalla[ 218],255,255,255);tira.setPixelColor(Pantalla[ 217],255,255,255);tira.setPixelColor(Pantalla[ 216],105,210,30);tira.setPixelColor(Pantalla[ 215],0,0,0);tira.setPixelColor(Pantalla[ 214],105,210,30);tira.setPixelColor(Pantalla[ 213],105,210,30);
      

    }
  else
  {

    tira.setPixelColor(Pantalla[ 94],105,210,30);tira.setPixelColor(Pantalla[ 93],105,210,30);tira.setPixelColor(Pantalla[ 92],0,255,0);tira.setPixelColor(Pantalla[ 91],105,210,30);tira.setPixelColor(Pantalla[ 90],105,210,30);tira.setPixelColor(Pantalla[ 89],105,210,30);tira.setPixelColor(Pantalla[ 88],105,210,30);tira.setPixelColor(Pantalla[ 87],0,255,0);tira.setPixelColor(Pantalla[ 86],105,210,30);tira.setPixelColor(Pantalla[ 85],105,210,30);tira.setPixelColor(Pantalla[ 83],105,210,30);tira.setPixelColor(Pantalla[ 82],255,255,255);tira.setPixelColor(Pantalla[ 81],255,255,255);
    tira.setPixelColor(Pantalla[ 97],105,210,30);tira.setPixelColor(Pantalla[ 98],105,210,30);tira.setPixelColor(Pantalla[ 99],0,255,0);tira.setPixelColor(Pantalla[ 100],105,210,30);tira.setPixelColor(Pantalla[ 101],105,210,30);tira.setPixelColor(Pantalla[ 102],105,210,30);tira.setPixelColor(Pantalla[ 103],105,210,30);tira.setPixelColor(Pantalla[ 104],0,255,0);tira.setPixelColor(Pantalla[ 105],105,210,30);tira.setPixelColor(Pantalla[ 106],105,210,30);tira.setPixelColor(Pantalla[ 108],105,210,30);tira.setPixelColor(Pantalla[ 109],255,255,255);tira.setPixelColor(Pantalla[ 110],255,255,255);
    tira.setPixelColor(Pantalla[ 126],255,255,255);tira.setPixelColor(Pantalla[ 125],255,255,255);tira.setPixelColor(Pantalla[ 124],255,255,255);tira.setPixelColor(Pantalla[ 123],255,255,255);tira.setPixelColor(Pantalla[ 120],255,255,255);tira.setPixelColor(Pantalla[ 119],255,255,255);tira.setPixelColor(Pantalla[ 118],255,255,255);tira.setPixelColor(Pantalla[ 117],255,255,255);tira.setPixelColor(Pantalla[ 115],105,210,30);tira.setPixelColor(Pantalla[ 114],255,255,255);tira.setPixelColor(Pantalla[ 113],255,255,255);
    tira.setPixelColor(Pantalla[ 128],255,255,255);tira.setPixelColor(Pantalla[ 129],255,255,255);tira.setPixelColor(Pantalla[ 130],255,255,255);tira.setPixelColor(Pantalla[ 131],255,255,255);tira.setPixelColor(Pantalla[ 132],255,255,255);tira.setPixelColor(Pantalla[ 133],255,255,255);tira.setPixelColor(Pantalla[ 134],255,255,255);tira.setPixelColor(Pantalla[ 135],255,255,255);tira.setPixelColor(Pantalla[ 136],255,255,255);tira.setPixelColor(Pantalla[ 137],255,255,255);tira.setPixelColor(Pantalla[ 138],255,255,255);tira.setPixelColor(Pantalla[ 139],255,255,255);tira.setPixelColor(Pantalla[ 140],105,210,30);tira.setPixelColor(Pantalla[ 141],105,210,30);tira.setPixelColor(Pantalla[ 142],255,255,255);
    tira.setPixelColor(Pantalla[ 158],255,255,255);tira.setPixelColor(Pantalla[ 157],255,255,255);tira.setPixelColor(Pantalla[ 156],255,255,255);tira.setPixelColor(Pantalla[ 155],0,255,0);tira.setPixelColor(Pantalla[ 154],0,0,0);tira.setPixelColor(Pantalla[ 153],0,0,0);tira.setPixelColor(Pantalla[ 152],0,255,0);tira.setPixelColor(Pantalla[ 151],255,255,255);tira.setPixelColor(Pantalla[ 150],255,255,255);tira.setPixelColor(Pantalla[ 149],255,255,255);tira.setPixelColor(Pantalla[ 147],105,210,30);tira.setPixelColor(Pantalla[ 146],105,210,30);tira.setPixelColor(Pantalla[ 145],255,255,255);
    tira.setPixelColor(Pantalla[ 163],255,255,255);tira.setPixelColor(Pantalla[ 164],0,0,0);tira.setPixelColor(Pantalla[ 165],0,0,0);tira.setPixelColor(Pantalla[ 166],0,0,0);tira.setPixelColor(Pantalla[ 167],0,0,0);tira.setPixelColor(Pantalla[ 168],255,255,255);tira.setPixelColor(Pantalla[ 172],105,210,30);tira.setPixelColor(Pantalla[ 173],105,210,30);tira.setPixelColor(Pantalla[ 174],255,255,255);


  }




}

void Matriz_Arana()
{
  if (lado==IZQUIERDA)
    {
      tira.setPixelColor(Pantalla[ 0],0,0,0);tira.setPixelColor(Pantalla[ 1],0,0,0);tira.setPixelColor(Pantalla[ 8],0,0,255);tira.setPixelColor(Pantalla[ 15],0,0,0);
tira.setPixelColor(Pantalla[ 31],0,0,0);tira.setPixelColor(Pantalla[ 30],0,255,0);tira.setPixelColor(Pantalla[ 29],0,255,0);tira.setPixelColor(Pantalla[ 25],0,255,0);tira.setPixelColor(Pantalla[ 24],0,255,0);tira.setPixelColor(Pantalla[ 23],0,255,0);tira.setPixelColor(Pantalla[ 22],0,255,0);tira.setPixelColor(Pantalla[ 21],0,255,0);tira.setPixelColor(Pantalla[ 17],0,255,0);tira.setPixelColor(Pantalla[ 16],0,255,0);
tira.setPixelColor(Pantalla[ 32],0,255,0);tira.setPixelColor(Pantalla[ 33],0,0,0);tira.setPixelColor(Pantalla[ 34],0,0,0);tira.setPixelColor(Pantalla[ 35],0,255,0);tira.setPixelColor(Pantalla[ 37],0,255,0);tira.setPixelColor(Pantalla[ 38],0,255,0);tira.setPixelColor(Pantalla[ 39],0,255,0);tira.setPixelColor(Pantalla[ 40],255,255,0);tira.setPixelColor(Pantalla[ 41],0,255,0);tira.setPixelColor(Pantalla[ 42],0,255,0);tira.setPixelColor(Pantalla[ 43],0,255,0);tira.setPixelColor(Pantalla[ 45],0,255,0);tira.setPixelColor(Pantalla[ 46],0,0,0);tira.setPixelColor(Pantalla[ 47],0,0,0);
tira.setPixelColor(Pantalla[ 63],0,0,0);tira.setPixelColor(Pantalla[ 62],0,0,0);tira.setPixelColor(Pantalla[ 61],0,0,0);tira.setPixelColor(Pantalla[ 60],0,255,0);tira.setPixelColor(Pantalla[ 59],0,255,0);tira.setPixelColor(Pantalla[ 58],0,255,0);tira.setPixelColor(Pantalla[ 57],0,255,0);tira.setPixelColor(Pantalla[ 56],0,255,0);tira.setPixelColor(Pantalla[ 55],255,255,0);tira.setPixelColor(Pantalla[ 54],0,255,0);tira.setPixelColor(Pantalla[ 53],0,255,0);tira.setPixelColor(Pantalla[ 52],0,255,0);tira.setPixelColor(Pantalla[ 51],0,255,0);tira.setPixelColor(Pantalla[ 50],0,0,0);tira.setPixelColor(Pantalla[ 49],0,0,0);tira.setPixelColor(Pantalla[ 48],0,0,0);
tira.setPixelColor(Pantalla[ 64],0,255,0);tira.setPixelColor(Pantalla[ 65],0,255,0);tira.setPixelColor(Pantalla[ 68],0,255,0);tira.setPixelColor(Pantalla[ 69],0,255,0);tira.setPixelColor(Pantalla[ 70],0,255,0);tira.setPixelColor(Pantalla[ 71],255,255,0);tira.setPixelColor(Pantalla[ 72],255,255,0);tira.setPixelColor(Pantalla[ 73],255,255,0);tira.setPixelColor(Pantalla[ 74],0,255,0);tira.setPixelColor(Pantalla[ 75],0,255,0);tira.setPixelColor(Pantalla[ 76],0,255,0);tira.setPixelColor(Pantalla[ 79],0,255,0);
tira.setPixelColor(Pantalla[ 95],0,0,0);tira.setPixelColor(Pantalla[ 94],0,0,0);tira.setPixelColor(Pantalla[ 93],0,255,0);tira.setPixelColor(Pantalla[ 92],0,255,0);tira.setPixelColor(Pantalla[ 91],0,255,0);tira.setPixelColor(Pantalla[ 90],0,255,0);tira.setPixelColor(Pantalla[ 89],0,255,0);tira.setPixelColor(Pantalla[ 88],0,255,0);tira.setPixelColor(Pantalla[ 87],255,255,0);tira.setPixelColor(Pantalla[ 86],0,255,0);tira.setPixelColor(Pantalla[ 85],0,255,0);tira.setPixelColor(Pantalla[ 84],0,255,0);tira.setPixelColor(Pantalla[ 83],0,255,0);tira.setPixelColor(Pantalla[ 82],0,255,0);tira.setPixelColor(Pantalla[ 81],0,255,0);tira.setPixelColor(Pantalla[ 80],0,0,0);
tira.setPixelColor(Pantalla[ 96],0,0,0);tira.setPixelColor(Pantalla[ 97],0,0,0);tira.setPixelColor(Pantalla[ 98],0,0,0);tira.setPixelColor(Pantalla[ 100],0,255,0);tira.setPixelColor(Pantalla[ 101],0,255,0);tira.setPixelColor(Pantalla[ 102],0,255,0);tira.setPixelColor(Pantalla[ 103],0,255,0);tira.setPixelColor(Pantalla[ 104],0,255,0);tira.setPixelColor(Pantalla[ 105],0,255,0);tira.setPixelColor(Pantalla[ 106],0,255,0);tira.setPixelColor(Pantalla[ 107],0,255,0);tira.setPixelColor(Pantalla[ 108],0,255,0);tira.setPixelColor(Pantalla[ 110],0,0,0);tira.setPixelColor(Pantalla[ 111],0,0,0);
tira.setPixelColor(Pantalla[ 127],0,255,0);tira.setPixelColor(Pantalla[ 126],0,0,0);tira.setPixelColor(Pantalla[ 122],0,255,0);tira.setPixelColor(Pantalla[ 121],0,255,0);tira.setPixelColor(Pantalla[ 120],0,255,0);tira.setPixelColor(Pantalla[ 119],0,255,0);tira.setPixelColor(Pantalla[ 118],0,255,0);tira.setPixelColor(Pantalla[ 117],0,255,0);tira.setPixelColor(Pantalla[ 116],0,255,0);tira.setPixelColor(Pantalla[ 112],0,0,0);
tira.setPixelColor(Pantalla[ 128],0,255,0);tira.setPixelColor(Pantalla[ 129],0,255,0);tira.setPixelColor(Pantalla[ 130],0,0,0);tira.setPixelColor(Pantalla[ 131],0,255,0);tira.setPixelColor(Pantalla[ 132],0,255,0);tira.setPixelColor(Pantalla[ 133],0,255,0);tira.setPixelColor(Pantalla[ 134],255,255,0);tira.setPixelColor(Pantalla[ 135],0,255,0);tira.setPixelColor(Pantalla[ 136],0,255,0);tira.setPixelColor(Pantalla[ 137],0,255,0);tira.setPixelColor(Pantalla[ 138],255,255,0);tira.setPixelColor(Pantalla[ 139],0,255,0);tira.setPixelColor(Pantalla[ 140],0,255,0);tira.setPixelColor(Pantalla[ 141],0,255,0);tira.setPixelColor(Pantalla[ 142],0,0,0);tira.setPixelColor(Pantalla[ 143],0,255,0);
tira.setPixelColor(Pantalla[ 159],0,0,0);tira.setPixelColor(Pantalla[ 158],0,0,0);tira.setPixelColor(Pantalla[ 157],0,255,0);tira.setPixelColor(Pantalla[ 154],0,255,0);tira.setPixelColor(Pantalla[ 153],0,255,0);tira.setPixelColor(Pantalla[ 152],0,255,0);tira.setPixelColor(Pantalla[ 151],0,255,0);tira.setPixelColor(Pantalla[ 150],0,255,0);tira.setPixelColor(Pantalla[ 149],0,255,0);tira.setPixelColor(Pantalla[ 148],0,255,0);tira.setPixelColor(Pantalla[ 145],0,255,0);tira.setPixelColor(Pantalla[ 144],0,0,0);
tira.setPixelColor(Pantalla[ 160],0,0,0);tira.setPixelColor(Pantalla[ 161],0,0,0);tira.setPixelColor(Pantalla[ 164],0,255,0);tira.setPixelColor(Pantalla[ 166],0,255,0);tira.setPixelColor(Pantalla[ 170],0,255,0);tira.setPixelColor(Pantalla[ 172],0,255,0);tira.setPixelColor(Pantalla[ 175],0,0,0);
tira.setPixelColor(Pantalla[ 191],0,0,0);tira.setPixelColor(Pantalla[ 188],0,0,0);tira.setPixelColor(Pantalla[ 187],0,255,0);tira.setPixelColor(Pantalla[ 186],0,0,0);tira.setPixelColor(Pantalla[ 185],0,0,0);tira.setPixelColor(Pantalla[ 181],0,0,0);tira.setPixelColor(Pantalla[ 180],0,0,0);tira.setPixelColor(Pantalla[ 179],0,255,0);tira.setPixelColor(Pantalla[ 178],0,0,0);
tira.setPixelColor(Pantalla[ 195],0,0,0);tira.setPixelColor(Pantalla[ 197],0,255,0);tira.setPixelColor(Pantalla[ 198],0,0,0);tira.setPixelColor(Pantalla[ 202],0,0,0);tira.setPixelColor(Pantalla[ 203],0,255,0);tira.setPixelColor(Pantalla[ 204],0,0,0);tira.setPixelColor(Pantalla[ 205],0,0,0);
tira.setPixelColor(Pantalla[ 220],0,0,0);tira.setPixelColor(Pantalla[ 219],0,0,0);tira.setPixelColor(Pantalla[ 218],0,255,0);tira.setPixelColor(Pantalla[ 217],0,255,0);tira.setPixelColor(Pantalla[ 213],0,255,0);tira.setPixelColor(Pantalla[ 212],0,255,0);tira.setPixelColor(Pantalla[ 211],0,0,0);tira.setPixelColor(Pantalla[ 210],0,0,0);

    }
  else
    {
      tira.setPixelColor(Pantalla[ 0],0,255,0);tira.setPixelColor(Pantalla[ 1],0,255,0);tira.setPixelColor(Pantalla[ 8],0,0,255);tira.setPixelColor(Pantalla[ 15],0,255,0);
      tira.setPixelColor(Pantalla[ 31],0,0,0);tira.setPixelColor(Pantalla[ 30],0,0,0);tira.setPixelColor(Pantalla[ 29],0,255,0);tira.setPixelColor(Pantalla[ 25],0,255,0);tira.setPixelColor(Pantalla[ 24],0,255,0);tira.setPixelColor(Pantalla[ 23],0,255,0);tira.setPixelColor(Pantalla[ 22],0,255,0);tira.setPixelColor(Pantalla[ 21],0,255,0);tira.setPixelColor(Pantalla[ 17],0,255,0);tira.setPixelColor(Pantalla[ 16],0,0,0);
      tira.setPixelColor(Pantalla[ 32],0,0,0);tira.setPixelColor(Pantalla[ 33],0,0,0);tira.setPixelColor(Pantalla[ 34],0,0,0);tira.setPixelColor(Pantalla[ 35],0,255,0);tira.setPixelColor(Pantalla[ 37],0,255,0);tira.setPixelColor(Pantalla[ 38],0,255,0);tira.setPixelColor(Pantalla[ 39],0,255,0);tira.setPixelColor(Pantalla[ 40],255,255,0);tira.setPixelColor(Pantalla[ 41],0,255,0);tira.setPixelColor(Pantalla[ 42],0,255,0);tira.setPixelColor(Pantalla[ 43],0,255,0);tira.setPixelColor(Pantalla[ 45],0,255,0);tira.setPixelColor(Pantalla[ 46],0,0,0);tira.setPixelColor(Pantalla[ 47],0,0,0);
      tira.setPixelColor(Pantalla[ 63],0,0,0);tira.setPixelColor(Pantalla[ 62],0,0,0);tira.setPixelColor(Pantalla[ 61],0,0,0);tira.setPixelColor(Pantalla[ 60],0,255,0);tira.setPixelColor(Pantalla[ 59],0,255,0);tira.setPixelColor(Pantalla[ 58],0,255,0);tira.setPixelColor(Pantalla[ 57],0,255,0);tira.setPixelColor(Pantalla[ 56],0,255,0);tira.setPixelColor(Pantalla[ 55],255,255,0);tira.setPixelColor(Pantalla[ 54],0,255,0);tira.setPixelColor(Pantalla[ 53],0,255,0);tira.setPixelColor(Pantalla[ 52],0,255,0);tira.setPixelColor(Pantalla[ 51],0,255,0);tira.setPixelColor(Pantalla[ 50],0,0,0);tira.setPixelColor(Pantalla[ 49],0,0,0);tira.setPixelColor(Pantalla[ 48],0,0,0);
      tira.setPixelColor(Pantalla[ 64],0,0,0);tira.setPixelColor(Pantalla[ 65],0,0,0);tira.setPixelColor(Pantalla[ 68],0,255,0);tira.setPixelColor(Pantalla[ 69],0,255,0);tira.setPixelColor(Pantalla[ 70],0,255,0);tira.setPixelColor(Pantalla[ 71],255,255,0);tira.setPixelColor(Pantalla[ 72],255,255,0);tira.setPixelColor(Pantalla[ 73],255,255,0);tira.setPixelColor(Pantalla[ 74],0,255,0);tira.setPixelColor(Pantalla[ 75],0,255,0);tira.setPixelColor(Pantalla[ 76],0,255,0);tira.setPixelColor(Pantalla[ 79],0,0,0);
      tira.setPixelColor(Pantalla[ 95],0,0,0);tira.setPixelColor(Pantalla[ 94],0,0,0);tira.setPixelColor(Pantalla[ 93],0,255,0);tira.setPixelColor(Pantalla[ 92],0,255,0);tira.setPixelColor(Pantalla[ 91],0,255,0);tira.setPixelColor(Pantalla[ 90],0,255,0);tira.setPixelColor(Pantalla[ 89],0,255,0);tira.setPixelColor(Pantalla[ 88],0,255,0);tira.setPixelColor(Pantalla[ 87],255,255,0);tira.setPixelColor(Pantalla[ 86],0,255,0);tira.setPixelColor(Pantalla[ 85],0,255,0);tira.setPixelColor(Pantalla[ 84],0,255,0);tira.setPixelColor(Pantalla[ 83],0,255,0);tira.setPixelColor(Pantalla[ 82],0,255,0);tira.setPixelColor(Pantalla[ 81],0,255,0);tira.setPixelColor(Pantalla[ 80],0,0,0);
      tira.setPixelColor(Pantalla[ 96],0,255,0);tira.setPixelColor(Pantalla[ 97],0,255,0);tira.setPixelColor(Pantalla[ 98],0,0,0);tira.setPixelColor(Pantalla[ 100],0,255,0);tira.setPixelColor(Pantalla[ 101],0,255,0);tira.setPixelColor(Pantalla[ 102],0,255,0);tira.setPixelColor(Pantalla[ 103],0,255,0);tira.setPixelColor(Pantalla[ 104],0,255,0);tira.setPixelColor(Pantalla[ 105],0,255,0);tira.setPixelColor(Pantalla[ 106],0,255,0);tira.setPixelColor(Pantalla[ 107],0,255,0);tira.setPixelColor(Pantalla[ 108],0,255,0);tira.setPixelColor(Pantalla[ 110],0,0,0);tira.setPixelColor(Pantalla[ 111],0,255,0);
      tira.setPixelColor(Pantalla[ 127],0,255,0);tira.setPixelColor(Pantalla[ 126],0,0,0);tira.setPixelColor(Pantalla[ 122],0,255,0);tira.setPixelColor(Pantalla[ 121],0,255,0);tira.setPixelColor(Pantalla[ 120],0,255,0);tira.setPixelColor(Pantalla[ 119],0,255,0);tira.setPixelColor(Pantalla[ 118],0,255,0);tira.setPixelColor(Pantalla[ 117],0,255,0);tira.setPixelColor(Pantalla[ 116],0,255,0);tira.setPixelColor(Pantalla[ 112],0,0,0);
      tira.setPixelColor(Pantalla[ 128],0,0,0);tira.setPixelColor(Pantalla[ 129],0,0,0);tira.setPixelColor(Pantalla[ 130],0,0,0);tira.setPixelColor(Pantalla[ 131],0,255,0);tira.setPixelColor(Pantalla[ 132],0,255,0);tira.setPixelColor(Pantalla[ 133],0,255,0);tira.setPixelColor(Pantalla[ 134],255,255,0);tira.setPixelColor(Pantalla[ 135],0,255,0);tira.setPixelColor(Pantalla[ 136],0,255,0);tira.setPixelColor(Pantalla[ 137],0,255,0);tira.setPixelColor(Pantalla[ 138],255,255,0);tira.setPixelColor(Pantalla[ 139],0,255,0);tira.setPixelColor(Pantalla[ 140],0,255,0);tira.setPixelColor(Pantalla[ 141],0,255,0);tira.setPixelColor(Pantalla[ 142],0,0,0);tira.setPixelColor(Pantalla[ 143],0,0,0);
      tira.setPixelColor(Pantalla[ 159],0,0,0);tira.setPixelColor(Pantalla[ 158],0,0,0);tira.setPixelColor(Pantalla[ 157],0,255,0);tira.setPixelColor(Pantalla[ 154],0,255,0);tira.setPixelColor(Pantalla[ 153],0,255,0);tira.setPixelColor(Pantalla[ 152],0,255,0);tira.setPixelColor(Pantalla[ 151],0,255,0);tira.setPixelColor(Pantalla[ 150],0,255,0);tira.setPixelColor(Pantalla[ 149],0,255,0);tira.setPixelColor(Pantalla[ 148],0,255,0);tira.setPixelColor(Pantalla[ 145],0,255,0);tira.setPixelColor(Pantalla[ 144],0,0,0);
      tira.setPixelColor(Pantalla[ 160],0,255,0);tira.setPixelColor(Pantalla[ 161],0,255,0);tira.setPixelColor(Pantalla[ 164],0,255,0);tira.setPixelColor(Pantalla[ 166],0,255,0);tira.setPixelColor(Pantalla[ 170],0,255,0);tira.setPixelColor(Pantalla[ 172],0,255,0);tira.setPixelColor(Pantalla[ 175],0,255,0);
      tira.setPixelColor(Pantalla[ 191],0,255,0);tira.setPixelColor(Pantalla[ 188],0,255,0);tira.setPixelColor(Pantalla[ 187],0,0,0);tira.setPixelColor(Pantalla[ 186],0,0,0);tira.setPixelColor(Pantalla[ 185],0,0,0);tira.setPixelColor(Pantalla[ 181],0,0,0);tira.setPixelColor(Pantalla[ 180],0,0,0);tira.setPixelColor(Pantalla[ 179],0,0,0);tira.setPixelColor(Pantalla[ 178],0,255,0);
      tira.setPixelColor(Pantalla[ 195],0,255,0);tira.setPixelColor(Pantalla[ 196],0,0,0);tira.setPixelColor(Pantalla[ 197],0,0,0);tira.setPixelColor(Pantalla[ 198],0,0,0);tira.setPixelColor(Pantalla[ 202],0,0,0);tira.setPixelColor(Pantalla[ 203],0,0,0);tira.setPixelColor(Pantalla[ 204],0,0,0);tira.setPixelColor(Pantalla[ 205],0,255,0);
      tira.setPixelColor(Pantalla[ 220],0,255,0);tira.setPixelColor(Pantalla[ 219],0,255,0);tira.setPixelColor(Pantalla[ 218],0,0,0);tira.setPixelColor(Pantalla[ 217],0,0,0);tira.setPixelColor(Pantalla[ 213],0,0,0);tira.setPixelColor(Pantalla[ 212],0,0,0);tira.setPixelColor(Pantalla[ 211],0,255,0);tira.setPixelColor(Pantalla[ 210],0,255,0);
      
    }



}

void Matriz_Lagartija()
{
  if (lado==IZQUIERDA)
    {

      tira.setPixelColor(Pantalla[ 5],255,0,0);tira.setPixelColor(Pantalla[ 6],255,0,0);tira.setPixelColor(Pantalla[ 7],255,0,0);tira.setPixelColor(Pantalla[ 8],255,0,0);tira.setPixelColor(Pantalla[ 9],255,0,0);tira.setPixelColor(Pantalla[ 10],255,0,0);tira.setPixelColor(Pantalla[ 11],0,0,0);tira.setPixelColor(Pantalla[ 12],0,0,0);tira.setPixelColor(Pantalla[ 13],0,0,0);tira.setPixelColor(Pantalla[ 14],0,0,0);
      tira.setPixelColor(Pantalla[ 31],0,0,0);tira.setPixelColor(Pantalla[ 30],255,0,0);tira.setPixelColor(Pantalla[ 29],255,0,0);tira.setPixelColor(Pantalla[ 28],255,0,0);tira.setPixelColor(Pantalla[ 27],255,0,0);tira.setPixelColor(Pantalla[ 26],255,0,0);tira.setPixelColor(Pantalla[ 25],255,0,0);tira.setPixelColor(Pantalla[ 24],255,0,0);tira.setPixelColor(Pantalla[ 23],0,0,0);tira.setPixelColor(Pantalla[ 22],0,0,0);tira.setPixelColor(Pantalla[ 21],255,0,0);tira.setPixelColor(Pantalla[ 20],255,0,0);tira.setPixelColor(Pantalla[ 19],255,0,0);tira.setPixelColor(Pantalla[ 18],255,0,0);tira.setPixelColor(Pantalla[ 17],0,0,0);
      tira.setPixelColor(Pantalla[ 32],0,0,0);tira.setPixelColor(Pantalla[ 33],255,0,0);tira.setPixelColor(Pantalla[ 34],0,0,0);tira.setPixelColor(Pantalla[ 35],0,0,0);tira.setPixelColor(Pantalla[ 36],255,0,0);tira.setPixelColor(Pantalla[ 37],255,0,0);tira.setPixelColor(Pantalla[ 38],255,0,0);tira.setPixelColor(Pantalla[ 39],255,0,0);tira.setPixelColor(Pantalla[ 40],0,0,0);tira.setPixelColor(Pantalla[ 41],0,0,0);tira.setPixelColor(Pantalla[ 42],0,0,0);tira.setPixelColor(Pantalla[ 43],0,0,0);tira.setPixelColor(Pantalla[ 44],0,0,0);tira.setPixelColor(Pantalla[ 45],255,0,0);tira.setPixelColor(Pantalla[ 46],255,0,0);
      tira.setPixelColor(Pantalla[ 63],255,0,0);tira.setPixelColor(Pantalla[ 62],255,0,0);tira.setPixelColor(Pantalla[ 61],255,0,0);tira.setPixelColor(Pantalla[ 60],0,0,0);tira.setPixelColor(Pantalla[ 59],255,0,0);tira.setPixelColor(Pantalla[ 58],255,0,0);tira.setPixelColor(Pantalla[ 57],255,0,0);tira.setPixelColor(Pantalla[ 56],255,0,0);tira.setPixelColor(Pantalla[ 55],255,0,0);tira.setPixelColor(Pantalla[ 54],255,0,0);tira.setPixelColor(Pantalla[ 53],255,0,0);tira.setPixelColor(Pantalla[ 52],0,0,0);tira.setPixelColor(Pantalla[ 51],0,0,0);tira.setPixelColor(Pantalla[ 50],0,0,0);tira.setPixelColor(Pantalla[ 49],255,0,0);
      tira.setPixelColor(Pantalla[ 64],0,0,0);tira.setPixelColor(Pantalla[ 65],255,0,0);tira.setPixelColor(Pantalla[ 66],0,0,0);tira.setPixelColor(Pantalla[ 67],0,0,0);tira.setPixelColor(Pantalla[ 68],255,0,0);tira.setPixelColor(Pantalla[ 69],255,0,0);tira.setPixelColor(Pantalla[ 70],255,0,0);tira.setPixelColor(Pantalla[ 71],255,0,0);tira.setPixelColor(Pantalla[ 72],0,0,0);tira.setPixelColor(Pantalla[ 73],0,0,0);tira.setPixelColor(Pantalla[ 74],255,0,0);tira.setPixelColor(Pantalla[ 75],0,0,0);tira.setPixelColor(Pantalla[ 76],0,0,0);tira.setPixelColor(Pantalla[ 77],0,0,0);tira.setPixelColor(Pantalla[ 78],255,0,0);tira.setPixelColor(Pantalla[ 79],0,0,0);
      tira.setPixelColor(Pantalla[ 95],0,0,0);tira.setPixelColor(Pantalla[ 94],0,0,0);tira.setPixelColor(Pantalla[ 93],0,0,0);tira.setPixelColor(Pantalla[ 92],0,0,0);tira.setPixelColor(Pantalla[ 91],255,0,0);tira.setPixelColor(Pantalla[ 90],255,0,0);tira.setPixelColor(Pantalla[ 89],255,0,0);tira.setPixelColor(Pantalla[ 88],255,0,0);tira.setPixelColor(Pantalla[ 87],0,0,0);tira.setPixelColor(Pantalla[ 86],255,0,0);tira.setPixelColor(Pantalla[ 85],255,0,0);tira.setPixelColor(Pantalla[ 84],255,0,0);tira.setPixelColor(Pantalla[ 83],0,0,0);tira.setPixelColor(Pantalla[ 82],255,0,0);tira.setPixelColor(Pantalla[ 81],255,0,0);tira.setPixelColor(Pantalla[ 80],0,0,0);
      tira.setPixelColor(Pantalla[ 96],0,0,0);tira.setPixelColor(Pantalla[ 97],255,0,0);tira.setPixelColor(Pantalla[ 98],255,0,0);tira.setPixelColor(Pantalla[ 99],255,0,0);tira.setPixelColor(Pantalla[ 100],255,0,0);tira.setPixelColor(Pantalla[ 101],255,0,0);tira.setPixelColor(Pantalla[ 102],255,0,0);tira.setPixelColor(Pantalla[ 103],255,0,0);tira.setPixelColor(Pantalla[ 104],0,0,0);tira.setPixelColor(Pantalla[ 105],0,0,0);tira.setPixelColor(Pantalla[ 106],255,0,0);tira.setPixelColor(Pantalla[ 107],0,0,0);tira.setPixelColor(Pantalla[ 108],0,0,0);
      tira.setPixelColor(Pantalla[ 127],0,0,0);tira.setPixelColor(Pantalla[ 126],255,0,0);tira.setPixelColor(Pantalla[ 125],0,0,0);tira.setPixelColor(Pantalla[ 124],0,0,0);tira.setPixelColor(Pantalla[ 123],255,0,0);tira.setPixelColor(Pantalla[ 122],255,0,0);tira.setPixelColor(Pantalla[ 121],255,0,0);tira.setPixelColor(Pantalla[ 120],255,0,0);tira.setPixelColor(Pantalla[ 119],0,0,0);tira.setPixelColor(Pantalla[ 118],0,0,0);tira.setPixelColor(Pantalla[ 117],0,0,0);tira.setPixelColor(Pantalla[ 116],0,0,0);tira.setPixelColor(Pantalla[ 115],0,0,0);
      tira.setPixelColor(Pantalla[ 128],255,0,0);tira.setPixelColor(Pantalla[ 129],255,0,0);tira.setPixelColor(Pantalla[ 130],255,0,0);tira.setPixelColor(Pantalla[ 131],0,0,0);tira.setPixelColor(Pantalla[ 132],255,0,0);tira.setPixelColor(Pantalla[ 133],255,0,0);tira.setPixelColor(Pantalla[ 134],255,0,0);tira.setPixelColor(Pantalla[ 135],255,0,0);tira.setPixelColor(Pantalla[ 136],0,0,0);tira.setPixelColor(Pantalla[ 137],0,0,0);tira.setPixelColor(Pantalla[ 138],0,0,0);tira.setPixelColor(Pantalla[ 139],0,0,0);tira.setPixelColor(Pantalla[ 140],0,0,0);
      tira.setPixelColor(Pantalla[ 159],0,0,0);tira.setPixelColor(Pantalla[ 158],255,0,0);tira.setPixelColor(Pantalla[ 157],0,0,0);tira.setPixelColor(Pantalla[ 156],0,0,0);tira.setPixelColor(Pantalla[ 155],0,0,0);tira.setPixelColor(Pantalla[ 154],255,0,0);tira.setPixelColor(Pantalla[ 153],255,0,0);tira.setPixelColor(Pantalla[ 152],255,0,0);tira.setPixelColor(Pantalla[ 151],255,0,0);tira.setPixelColor(Pantalla[ 150],255,0,0);tira.setPixelColor(Pantalla[ 149],255,0,0);tira.setPixelColor(Pantalla[ 148],0,0,0);tira.setPixelColor(Pantalla[ 147],0,0,0);
      tira.setPixelColor(Pantalla[ 160],0,0,0);tira.setPixelColor(Pantalla[ 161],0,0,0);tira.setPixelColor(Pantalla[ 162],0,0,0);tira.setPixelColor(Pantalla[ 163],0,0,0);tira.setPixelColor(Pantalla[ 164],0,0,0);tira.setPixelColor(Pantalla[ 165],0,0,0);tira.setPixelColor(Pantalla[ 166],255,0,0);tira.setPixelColor(Pantalla[ 167],0,0,0);tira.setPixelColor(Pantalla[ 168],0,0,0);tira.setPixelColor(Pantalla[ 169],0,0,0);tira.setPixelColor(Pantalla[ 170],255,0,0);tira.setPixelColor(Pantalla[ 171],0,0,0);tira.setPixelColor(Pantalla[ 172],0,0,0);
      tira.setPixelColor(Pantalla[ 191],0,0,0);tira.setPixelColor(Pantalla[ 190],0,0,0);tira.setPixelColor(Pantalla[ 189],0,0,0);tira.setPixelColor(Pantalla[ 188],0,0,0);tira.setPixelColor(Pantalla[ 187],0,0,0);tira.setPixelColor(Pantalla[ 186],255,0,0);tira.setPixelColor(Pantalla[ 185],255,0,0);tira.setPixelColor(Pantalla[ 184],255,0,0);tira.setPixelColor(Pantalla[ 183],0,0,0);tira.setPixelColor(Pantalla[ 182],255,0,0);tira.setPixelColor(Pantalla[ 181],255,0,0);tira.setPixelColor(Pantalla[ 180],255,0,0);tira.setPixelColor(Pantalla[ 179],0,0,0);
      tira.setPixelColor(Pantalla[ 192],0,0,0);tira.setPixelColor(Pantalla[ 193],0,0,0);tira.setPixelColor(Pantalla[ 194],0,0,0);tira.setPixelColor(Pantalla[ 195],0,0,0);tira.setPixelColor(Pantalla[ 196],0,0,0);tira.setPixelColor(Pantalla[ 197],255,255,255);tira.setPixelColor(Pantalla[ 198],255,0,0);tira.setPixelColor(Pantalla[ 199],255,255,255);tira.setPixelColor(Pantalla[ 200],0,0,0);tira.setPixelColor(Pantalla[ 201],0,0,0);tira.setPixelColor(Pantalla[ 202],255,0,0);tira.setPixelColor(Pantalla[ 203],0,0,0);tira.setPixelColor(Pantalla[ 204],0,0,0);
      tira.setPixelColor(Pantalla[ 223],0,0,0);tira.setPixelColor(Pantalla[ 222],0,0,0);tira.setPixelColor(Pantalla[ 221],0,0,0);tira.setPixelColor(Pantalla[ 220],0,0,0);tira.setPixelColor(Pantalla[ 219],0,0,0);tira.setPixelColor(Pantalla[ 218],0,0,0);tira.setPixelColor(Pantalla[ 217],255,0,0);tira.setPixelColor(Pantalla[ 216],0,0,0);tira.setPixelColor(Pantalla[ 215],0,0,0);
      


    }
  else
    {

      tira.setPixelColor(Pantalla[ 5],255,0,0);tira.setPixelColor(Pantalla[ 6],255,0,0);tira.setPixelColor(Pantalla[ 7],255,0,0);tira.setPixelColor(Pantalla[ 8],255,0,0);tira.setPixelColor(Pantalla[ 9],255,0,0);tira.setPixelColor(Pantalla[ 10],255,0,0);tira.setPixelColor(Pantalla[ 11],255,0,0);tira.setPixelColor(Pantalla[ 12],255,0,0);tira.setPixelColor(Pantalla[ 13],0,0,0);tira.setPixelColor(Pantalla[ 14],0,0,0);
      tira.setPixelColor(Pantalla[ 31],0,0,0);tira.setPixelColor(Pantalla[ 30],0,0,0);tira.setPixelColor(Pantalla[ 29],0,0,0);tira.setPixelColor(Pantalla[ 28],0,0,0);tira.setPixelColor(Pantalla[ 27],255,0,0);tira.setPixelColor(Pantalla[ 26],255,0,0);tira.setPixelColor(Pantalla[ 25],255,0,0);tira.setPixelColor(Pantalla[ 24],255,0,0);tira.setPixelColor(Pantalla[ 23],0,0,0);tira.setPixelColor(Pantalla[ 22],0,0,0);tira.setPixelColor(Pantalla[ 21],0,0,0);tira.setPixelColor(Pantalla[ 20],0,0,0);tira.setPixelColor(Pantalla[ 19],255,0,0);tira.setPixelColor(Pantalla[ 18],255,0,0);tira.setPixelColor(Pantalla[ 17],0,0,0);
      tira.setPixelColor(Pantalla[ 32],0,0,0);tira.setPixelColor(Pantalla[ 33],0,0,0);tira.setPixelColor(Pantalla[ 34],0,0,0);tira.setPixelColor(Pantalla[ 35],0,0,0);tira.setPixelColor(Pantalla[ 36],255,0,0);tira.setPixelColor(Pantalla[ 37],255,0,0);tira.setPixelColor(Pantalla[ 38],255,0,0);tira.setPixelColor(Pantalla[ 39],255,0,0);tira.setPixelColor(Pantalla[ 40],255,0,0);tira.setPixelColor(Pantalla[ 41],255,0,0);tira.setPixelColor(Pantalla[ 42],255,0,0);tira.setPixelColor(Pantalla[ 43],0,0,0);tira.setPixelColor(Pantalla[ 44],0,0,0);tira.setPixelColor(Pantalla[ 45],255,0,0);tira.setPixelColor(Pantalla[ 46],0,0,0);
      tira.setPixelColor(Pantalla[ 63],0,0,0);tira.setPixelColor(Pantalla[ 62],255,0,0);tira.setPixelColor(Pantalla[ 61],255,0,0);tira.setPixelColor(Pantalla[ 60],255,0,0);tira.setPixelColor(Pantalla[ 59],255,0,0);tira.setPixelColor(Pantalla[ 58],255,0,0);tira.setPixelColor(Pantalla[ 57],255,0,0);tira.setPixelColor(Pantalla[ 56],255,0,0);tira.setPixelColor(Pantalla[ 55],0,0,0);tira.setPixelColor(Pantalla[ 54],0,0,0);tira.setPixelColor(Pantalla[ 53],255,0,0);tira.setPixelColor(Pantalla[ 52],0,0,0);tira.setPixelColor(Pantalla[ 51],0,0,0);tira.setPixelColor(Pantalla[ 50],255,0,0);tira.setPixelColor(Pantalla[ 49],0,0,0);
      tira.setPixelColor(Pantalla[ 64],0,0,0);tira.setPixelColor(Pantalla[ 65],255,0,0);tira.setPixelColor(Pantalla[ 66],0,0,0);tira.setPixelColor(Pantalla[ 67],0,0,0);tira.setPixelColor(Pantalla[ 68],255,0,0);tira.setPixelColor(Pantalla[ 69],255,0,0);tira.setPixelColor(Pantalla[ 70],255,0,0);tira.setPixelColor(Pantalla[ 71],255,0,0);tira.setPixelColor(Pantalla[ 72],0,0,0);tira.setPixelColor(Pantalla[ 73],255,0,0);tira.setPixelColor(Pantalla[ 74],255,0,0);tira.setPixelColor(Pantalla[ 75],255,0,0);tira.setPixelColor(Pantalla[ 76],0,0,0);tira.setPixelColor(Pantalla[ 77],255,0,0);tira.setPixelColor(Pantalla[ 78],255,0,0);tira.setPixelColor(Pantalla[ 79],255,0,0);
      tira.setPixelColor(Pantalla[ 95],255,0,0);tira.setPixelColor(Pantalla[ 94],255,0,0);tira.setPixelColor(Pantalla[ 93],255,0,0);tira.setPixelColor(Pantalla[ 92],0,0,0);tira.setPixelColor(Pantalla[ 91],255,0,0);tira.setPixelColor(Pantalla[ 90],255,0,0);tira.setPixelColor(Pantalla[ 89],255,0,0);tira.setPixelColor(Pantalla[ 88],255,0,0);tira.setPixelColor(Pantalla[ 87],0,0,0);tira.setPixelColor(Pantalla[ 86],0,0,0);tira.setPixelColor(Pantalla[ 85],255,0,0);tira.setPixelColor(Pantalla[ 84],0,0,0);tira.setPixelColor(Pantalla[ 83],0,0,0);tira.setPixelColor(Pantalla[ 82],0,0,0);tira.setPixelColor(Pantalla[ 81],0,0,0);tira.setPixelColor(Pantalla[ 80],0,0,0);
      tira.setPixelColor(Pantalla[ 96],0,0,0);tira.setPixelColor(Pantalla[ 97],255,0,0);tira.setPixelColor(Pantalla[ 98],0,0,0);tira.setPixelColor(Pantalla[ 99],0,0,0);tira.setPixelColor(Pantalla[ 100],255,0,0);tira.setPixelColor(Pantalla[ 101],255,0,0);tira.setPixelColor(Pantalla[ 102],255,0,0);tira.setPixelColor(Pantalla[ 103],255,0,0);tira.setPixelColor(Pantalla[ 104],0,0,0);tira.setPixelColor(Pantalla[ 105],0,0,0);tira.setPixelColor(Pantalla[ 106],0,0,0);tira.setPixelColor(Pantalla[ 107],0,0,0);tira.setPixelColor(Pantalla[ 108],0,0,0);
      tira.setPixelColor(Pantalla[ 127],0,0,0);tira.setPixelColor(Pantalla[ 126],0,0,0);tira.setPixelColor(Pantalla[ 125],0,0,0);tira.setPixelColor(Pantalla[ 124],0,0,0);tira.setPixelColor(Pantalla[ 123],255,0,0);tira.setPixelColor(Pantalla[ 122],255,0,0);tira.setPixelColor(Pantalla[ 121],255,0,0);tira.setPixelColor(Pantalla[ 120],255,0,0);tira.setPixelColor(Pantalla[ 119],255,0,0);tira.setPixelColor(Pantalla[ 118],255,0,0);tira.setPixelColor(Pantalla[ 117],255,0,0);tira.setPixelColor(Pantalla[ 116],0,0,0);tira.setPixelColor(Pantalla[ 115],0,0,0);
      tira.setPixelColor(Pantalla[ 128],0,0,0);tira.setPixelColor(Pantalla[ 129],0,0,0);tira.setPixelColor(Pantalla[ 130],0,0,0);tira.setPixelColor(Pantalla[ 131],0,0,0);tira.setPixelColor(Pantalla[ 132],255,0,0);tira.setPixelColor(Pantalla[ 133],255,0,0);tira.setPixelColor(Pantalla[ 134],255,0,0);tira.setPixelColor(Pantalla[ 135],255,0,0);tira.setPixelColor(Pantalla[ 136],0,0,0);tira.setPixelColor(Pantalla[ 137],0,0,0);tira.setPixelColor(Pantalla[ 138],255,0,0);tira.setPixelColor(Pantalla[ 139],0,0,0);tira.setPixelColor(Pantalla[ 140],0,0,0);
      tira.setPixelColor(Pantalla[ 159],0,0,0);tira.setPixelColor(Pantalla[ 158],255,0,0);tira.setPixelColor(Pantalla[ 157],255,0,0);tira.setPixelColor(Pantalla[ 156],255,0,0);tira.setPixelColor(Pantalla[ 155],255,0,0);tira.setPixelColor(Pantalla[ 154],255,0,0);tira.setPixelColor(Pantalla[ 153],255,0,0);tira.setPixelColor(Pantalla[ 152],0,0,0);tira.setPixelColor(Pantalla[ 151],0,0,0);tira.setPixelColor(Pantalla[ 150],255,0,0);tira.setPixelColor(Pantalla[ 149],255,0,0);tira.setPixelColor(Pantalla[ 148],255,0,0);tira.setPixelColor(Pantalla[ 147],0,0,0);
      tira.setPixelColor(Pantalla[ 160],0,0,0);tira.setPixelColor(Pantalla[ 161],255,0,0);tira.setPixelColor(Pantalla[ 162],0,0,0);tira.setPixelColor(Pantalla[ 163],0,0,0);tira.setPixelColor(Pantalla[ 164],0,0,0);tira.setPixelColor(Pantalla[ 165],255,0,0);tira.setPixelColor(Pantalla[ 166],0,0,0);tira.setPixelColor(Pantalla[ 167],0,0,0);tira.setPixelColor(Pantalla[ 168],0,0,0);tira.setPixelColor(Pantalla[ 169],0,0,0);tira.setPixelColor(Pantalla[ 170],255,0,0);tira.setPixelColor(Pantalla[ 171],0,0,0);tira.setPixelColor(Pantalla[ 172],0,0,0);
      tira.setPixelColor(Pantalla[ 191],255,0,0);tira.setPixelColor(Pantalla[ 190],255,0,0);tira.setPixelColor(Pantalla[ 189],255,0,0);tira.setPixelColor(Pantalla[ 188],0,0,0);tira.setPixelColor(Pantalla[ 187],255,0,0);tira.setPixelColor(Pantalla[ 186],255,0,0);tira.setPixelColor(Pantalla[ 185],255,0,0);tira.setPixelColor(Pantalla[ 184],0,0,0);tira.setPixelColor(Pantalla[ 183],0,0,0);tira.setPixelColor(Pantalla[ 182],0,0,0);tira.setPixelColor(Pantalla[ 181],0,0,0);tira.setPixelColor(Pantalla[ 180],0,0,0);tira.setPixelColor(Pantalla[ 179],0,0,0);
      tira.setPixelColor(Pantalla[ 192],0,0,0);tira.setPixelColor(Pantalla[ 193],255,0,0);tira.setPixelColor(Pantalla[ 194],0,0,0);tira.setPixelColor(Pantalla[ 195],0,0,0);tira.setPixelColor(Pantalla[ 196],255,255,255);tira.setPixelColor(Pantalla[ 197],255,0,0);tira.setPixelColor(Pantalla[ 198],255,255,255);tira.setPixelColor(Pantalla[ 199],0,0,0);tira.setPixelColor(Pantalla[ 200],0,0,0);tira.setPixelColor(Pantalla[ 201],0,0,0);tira.setPixelColor(Pantalla[ 202],0,0,0);tira.setPixelColor(Pantalla[ 203],0,0,0);tira.setPixelColor(Pantalla[ 204],0,0,0);
      tira.setPixelColor(Pantalla[ 223],0,0,0);tira.setPixelColor(Pantalla[ 222],0,0,0);tira.setPixelColor(Pantalla[ 221],0,0,0);tira.setPixelColor(Pantalla[ 220],0,0,0);tira.setPixelColor(Pantalla[ 219],0,0,0);tira.setPixelColor(Pantalla[ 218],255,0,0);tira.setPixelColor(Pantalla[ 217],0,0,0);tira.setPixelColor(Pantalla[ 216],0,0,0);tira.setPixelColor(Pantalla[ 215],0,0,0);
    }





}

void Matriz_TimeOut()
{
  tira.setPixelColor(Pantalla[ 29],0,255,0);tira.setPixelColor(Pantalla[ 18],0,255,0);
  tira.setPixelColor(Pantalla[ 35],0,255,0);tira.setPixelColor(Pantalla[ 44],0,255,0);
  tira.setPixelColor(Pantalla[ 59],0,255,0);tira.setPixelColor(Pantalla[ 52],0,255,0);
  tira.setPixelColor(Pantalla[ 69],0,255,0);tira.setPixelColor(Pantalla[ 74],0,255,0);
  tira.setPixelColor(Pantalla[ 89],0,255,0);tira.setPixelColor(Pantalla[ 86],0,255,0);
  tira.setPixelColor(Pantalla[ 103],0,255,0);tira.setPixelColor(Pantalla[ 104],0,255,0);
  tira.setPixelColor(Pantalla[ 120],0,255,0);tira.setPixelColor(Pantalla[ 119],0,255,0);
  tira.setPixelColor(Pantalla[ 134],0,255,0);tira.setPixelColor(Pantalla[ 137],0,255,0);
  tira.setPixelColor(Pantalla[ 154],0,255,0);tira.setPixelColor(Pantalla[ 149],0,255,0);
  tira.setPixelColor(Pantalla[ 164],0,255,0);tira.setPixelColor(Pantalla[ 171],0,255,0);
  tira.setPixelColor(Pantalla[ 188],0,255,0);tira.setPixelColor(Pantalla[ 179],0,255,0);
  tira.setPixelColor(Pantalla[ 194],0,255,0);tira.setPixelColor(Pantalla[ 205],0,255,0);
  
}

void Matriz_Corazon()
{
  tira.setPixelColor(Pantalla[ 3],0,255,0);tira.setPixelColor(Pantalla[ 4],0,255,0);tira.setPixelColor(Pantalla[ 5],0,255,0);tira.setPixelColor(Pantalla[ 10],0,255,0);tira.setPixelColor(Pantalla[ 11],0,255,0);tira.setPixelColor(Pantalla[ 12],0,255,0);
  tira.setPixelColor(Pantalla[ 29],0,255,0);tira.setPixelColor(Pantalla[ 28],0,255,0);tira.setPixelColor(Pantalla[ 27],0,255,0);tira.setPixelColor(Pantalla[ 26],0,255,0);tira.setPixelColor(Pantalla[ 25],0,255,0);tira.setPixelColor(Pantalla[ 22],0,255,0);tira.setPixelColor(Pantalla[ 21],0,255,0);tira.setPixelColor(Pantalla[ 20],0,255,0);tira.setPixelColor(Pantalla[ 19],0,255,0);tira.setPixelColor(Pantalla[ 18],0,255,0);
  tira.setPixelColor(Pantalla[ 33],0,255,0);tira.setPixelColor(Pantalla[ 34],0,255,0);tira.setPixelColor(Pantalla[ 35],0,255,0);tira.setPixelColor(Pantalla[ 36],0,255,0);tira.setPixelColor(Pantalla[ 37],0,255,0);tira.setPixelColor(Pantalla[ 38],0,255,0);tira.setPixelColor(Pantalla[ 39],0,255,0);tira.setPixelColor(Pantalla[ 40],0,255,0);tira.setPixelColor(Pantalla[ 41],0,255,0);tira.setPixelColor(Pantalla[ 42],0,255,0);tira.setPixelColor(Pantalla[ 43],0,255,0);tira.setPixelColor(Pantalla[ 44],0,255,0);tira.setPixelColor(Pantalla[ 45],0,255,0);tira.setPixelColor(Pantalla[ 46],0,255,0);
  tira.setPixelColor(Pantalla[ 62],0,255,0);tira.setPixelColor(Pantalla[ 61],0,255,0);tira.setPixelColor(Pantalla[ 60],0,255,0);tira.setPixelColor(Pantalla[ 59],0,255,0);tira.setPixelColor(Pantalla[ 58],0,255,0);tira.setPixelColor(Pantalla[ 57],0,255,0);tira.setPixelColor(Pantalla[ 56],0,255,0);tira.setPixelColor(Pantalla[ 55],0,255,0);tira.setPixelColor(Pantalla[ 54],0,255,0);tira.setPixelColor(Pantalla[ 53],0,255,0);tira.setPixelColor(Pantalla[ 52],0,255,0);tira.setPixelColor(Pantalla[ 51],0,255,0);tira.setPixelColor(Pantalla[ 50],0,255,0);tira.setPixelColor(Pantalla[ 49],0,255,0);
  tira.setPixelColor(Pantalla[ 65],0,255,0);tira.setPixelColor(Pantalla[ 66],0,255,0);tira.setPixelColor(Pantalla[ 67],0,255,0);tira.setPixelColor(Pantalla[ 68],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 69],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 70],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 71],0,255,0);tira.setPixelColor(Pantalla[ 72],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 73],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 74],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 75],0,255,0);tira.setPixelColor(Pantalla[ 76],0,255,0);tira.setPixelColor(Pantalla[ 77],0,255,0);tira.setPixelColor(Pantalla[ 78],0,255,0);
  tira.setPixelColor(Pantalla[ 94],0,255,0);tira.setPixelColor(Pantalla[ 93],0,255,0);tira.setPixelColor(Pantalla[ 92],0,255,0);tira.setPixelColor(Pantalla[ 91],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 90],0,255,0);tira.setPixelColor(Pantalla[ 89],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 88],0,255,0);tira.setPixelColor(Pantalla[ 87],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 86],0,255,0);tira.setPixelColor(Pantalla[ 85],0,255,0);tira.setPixelColor(Pantalla[ 84],0,255,0);tira.setPixelColor(Pantalla[ 83],0,255,0);tira.setPixelColor(Pantalla[ 82],0,255,0);tira.setPixelColor(Pantalla[ 81],0,255,0);
  tira.setPixelColor(Pantalla[ 97],0,255,0);tira.setPixelColor(Pantalla[ 98],0,255,0);tira.setPixelColor(Pantalla[ 99],0,255,0);tira.setPixelColor(Pantalla[ 100],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 101],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 102],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 103],0,255,0);tira.setPixelColor(Pantalla[ 104],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 105],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 106],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 107],0,255,0);tira.setPixelColor(Pantalla[ 108],0,255,0);tira.setPixelColor(Pantalla[ 109],0,255,0);tira.setPixelColor(Pantalla[ 110],0,255,0);
  tira.setPixelColor(Pantalla[ 125],0,255,0);tira.setPixelColor(Pantalla[ 124],0,255,0);tira.setPixelColor(Pantalla[ 123],0,255,0);tira.setPixelColor(Pantalla[ 122],0,255,0);tira.setPixelColor(Pantalla[ 121],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 120],0,255,0);tira.setPixelColor(Pantalla[ 119],0,255,0);tira.setPixelColor(Pantalla[ 118],0,255,0);tira.setPixelColor(Pantalla[ 117],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 116],0,255,0);tira.setPixelColor(Pantalla[ 115],0,255,0);tira.setPixelColor(Pantalla[ 114],0,255,0);
  tira.setPixelColor(Pantalla[ 130],0,255,0);tira.setPixelColor(Pantalla[ 131],0,255,0);tira.setPixelColor(Pantalla[ 132],0,255,0);tira.setPixelColor(Pantalla[ 133],0,255,0);tira.setPixelColor(Pantalla[ 134],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 135],0,255,0);tira.setPixelColor(Pantalla[ 136],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 137],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 138],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 139],0,255,0);tira.setPixelColor(Pantalla[ 140],0,255,0);tira.setPixelColor(Pantalla[ 141],0,255,0);
  tira.setPixelColor(Pantalla[ 156],0,255,0);tira.setPixelColor(Pantalla[ 155],0,255,0);tira.setPixelColor(Pantalla[ 154],0,255,0);tira.setPixelColor(Pantalla[ 153],0,255,0);tira.setPixelColor(Pantalla[ 152],0,255,0);tira.setPixelColor(Pantalla[ 151],0,255,0);tira.setPixelColor(Pantalla[ 150],0,255,0);tira.setPixelColor(Pantalla[ 149],0,255,0);tira.setPixelColor(Pantalla[ 148],0,255,0);tira.setPixelColor(Pantalla[ 147],0,255,0);
  tira.setPixelColor(Pantalla[ 164],0,255,0);tira.setPixelColor(Pantalla[ 165],0,255,0);tira.setPixelColor(Pantalla[ 166],0,255,0);tira.setPixelColor(Pantalla[ 167],0,255,0);tira.setPixelColor(Pantalla[ 168],0,255,0);tira.setPixelColor(Pantalla[ 169],0,255,0);tira.setPixelColor(Pantalla[ 170],0,255,0);tira.setPixelColor(Pantalla[ 171],0,255,0);
  tira.setPixelColor(Pantalla[ 186],0,255,0);tira.setPixelColor(Pantalla[ 185],0,255,0);tira.setPixelColor(Pantalla[ 184],0,255,0);tira.setPixelColor(Pantalla[ 183],0,255,0);tira.setPixelColor(Pantalla[ 182],0,255,0);tira.setPixelColor(Pantalla[ 181],0,255,0);
  tira.setPixelColor(Pantalla[ 198],0,255,0);tira.setPixelColor(Pantalla[ 199],0,255,0);tira.setPixelColor(Pantalla[ 200],0,255,0);tira.setPixelColor(Pantalla[ 201],0,255,0);
  tira.setPixelColor(Pantalla[ 216],0,255,0);tira.setPixelColor(Pantalla[ 215],0,255,0);
  
}

void Matriz_Oso()
{

  if (lado==IZQUIERDA)
    {
      tira.setPixelColor(Pantalla[ 0],100,100,100);tira.setPixelColor(Pantalla[ 1],100,100,100);tira.setPixelColor(Pantalla[ 2],100,100,100);tira.setPixelColor(Pantalla[ 12],100,100,100);tira.setPixelColor(Pantalla[ 13],100,100,100);tira.setPixelColor(Pantalla[ 14],100,100,100);
      tira.setPixelColor(Pantalla[ 31],100,100,100);tira.setPixelColor(Pantalla[ 30],127,255,80);tira.setPixelColor(Pantalla[ 29],100,100,100);tira.setPixelColor(Pantalla[ 28],100,100,100);tira.setPixelColor(Pantalla[ 27],100,100,100);tira.setPixelColor(Pantalla[ 26],100,100,100);tira.setPixelColor(Pantalla[ 25],100,100,100);tira.setPixelColor(Pantalla[ 24],100,100,100);tira.setPixelColor(Pantalla[ 23],100,100,100);tira.setPixelColor(Pantalla[ 22],100,100,100);tira.setPixelColor(Pantalla[ 21],100,100,100);tira.setPixelColor(Pantalla[ 20],100,100,100);tira.setPixelColor(Pantalla[ 19],100,100,100);tira.setPixelColor(Pantalla[ 18],127,255,80);tira.setPixelColor(Pantalla[ 17],100,100,100);
      tira.setPixelColor(Pantalla[ 32],100,100,100);tira.setPixelColor(Pantalla[ 33],100,100,100);tira.setPixelColor(Pantalla[ 34],100,100,100);tira.setPixelColor(Pantalla[ 35],100,100,100);tira.setPixelColor(Pantalla[ 36],100,100,100);tira.setPixelColor(Pantalla[ 37],100,100,100);tira.setPixelColor(Pantalla[ 38],100,100,100);tira.setPixelColor(Pantalla[ 39],100,100,100);tira.setPixelColor(Pantalla[ 40],100,100,100);tira.setPixelColor(Pantalla[ 41],100,100,100);tira.setPixelColor(Pantalla[ 42],100,100,100);tira.setPixelColor(Pantalla[ 43],100,100,100);tira.setPixelColor(Pantalla[ 44],100,100,100);tira.setPixelColor(Pantalla[ 45],100,100,100);tira.setPixelColor(Pantalla[ 46],100,100,100);
      tira.setPixelColor(Pantalla[ 61],100,100,100);tira.setPixelColor(Pantalla[ 60],100,100,100);tira.setPixelColor(Pantalla[ 59],100,100,100);tira.setPixelColor(Pantalla[ 58],100,100,100);tira.setPixelColor(Pantalla[ 57],100,100,100);tira.setPixelColor(Pantalla[ 56],100,100,100);tira.setPixelColor(Pantalla[ 55],100,100,100);tira.setPixelColor(Pantalla[ 54],100,100,100);tira.setPixelColor(Pantalla[ 53],100,100,100);tira.setPixelColor(Pantalla[ 52],100,100,100);tira.setPixelColor(Pantalla[ 51],100,100,100);
      tira.setPixelColor(Pantalla[ 66],100,100,100);tira.setPixelColor(Pantalla[ 67],100,100,100);tira.setPixelColor(Pantalla[ 68],0,0,0);tira.setPixelColor(Pantalla[ 69],100,100,100);tira.setPixelColor(Pantalla[ 70],100,100,100);tira.setPixelColor(Pantalla[ 71],100,100,100);tira.setPixelColor(Pantalla[ 72],100,100,100);tira.setPixelColor(Pantalla[ 73],100,100,100);tira.setPixelColor(Pantalla[ 74],0,0,0);tira.setPixelColor(Pantalla[ 75],100,100,100);tira.setPixelColor(Pantalla[ 76],100,100,100);
      tira.setPixelColor(Pantalla[ 93],100,100,100);tira.setPixelColor(Pantalla[ 92],100,100,100);tira.setPixelColor(Pantalla[ 91],100,100,100);tira.setPixelColor(Pantalla[ 90],100,100,100);tira.setPixelColor(Pantalla[ 89],100,100,100);tira.setPixelColor(Pantalla[ 88],100,100,100);tira.setPixelColor(Pantalla[ 87],100,100,100);tira.setPixelColor(Pantalla[ 86],100,100,100);tira.setPixelColor(Pantalla[ 85],100,100,100);tira.setPixelColor(Pantalla[ 84],100,100,100);tira.setPixelColor(Pantalla[ 83],100,100,100);
      tira.setPixelColor(Pantalla[ 98],100,100,100);tira.setPixelColor(Pantalla[ 99],100,100,100);tira.setPixelColor(Pantalla[ 100],100,100,100);tira.setPixelColor(Pantalla[ 101],100,100,100);tira.setPixelColor(Pantalla[ 102],127,255,80);tira.setPixelColor(Pantalla[ 103],127,255,80);tira.setPixelColor(Pantalla[ 104],127,255,80);tira.setPixelColor(Pantalla[ 105],100,100,100);tira.setPixelColor(Pantalla[ 106],100,100,100);tira.setPixelColor(Pantalla[ 107],100,100,100);tira.setPixelColor(Pantalla[ 108],100,100,100);
      tira.setPixelColor(Pantalla[ 125],100,100,100);tira.setPixelColor(Pantalla[ 124],100,100,100);tira.setPixelColor(Pantalla[ 123],100,100,100);tira.setPixelColor(Pantalla[ 122],100,100,100);tira.setPixelColor(Pantalla[ 121],100,100,100);tira.setPixelColor(Pantalla[ 120],127,255,80);tira.setPixelColor(Pantalla[ 119],100,100,100);tira.setPixelColor(Pantalla[ 118],100,100,100);tira.setPixelColor(Pantalla[ 117],100,100,100);tira.setPixelColor(Pantalla[ 116],100,100,100);tira.setPixelColor(Pantalla[ 115],100,100,100);
      tira.setPixelColor(Pantalla[ 130],100,100,100);tira.setPixelColor(Pantalla[ 131],100,100,100);tira.setPixelColor(Pantalla[ 132],100,100,100);tira.setPixelColor(Pantalla[ 133],100,100,100);tira.setPixelColor(Pantalla[ 134],100,100,100);tira.setPixelColor(Pantalla[ 135],100,100,100);tira.setPixelColor(Pantalla[ 136],100,100,100);tira.setPixelColor(Pantalla[ 137],100,100,100);tira.setPixelColor(Pantalla[ 138],100,100,100);tira.setPixelColor(Pantalla[ 139],100,100,100);tira.setPixelColor(Pantalla[ 140],100,100,100);
      tira.setPixelColor(Pantalla[ 156],100,100,100);tira.setPixelColor(Pantalla[ 155],100,100,100);tira.setPixelColor(Pantalla[ 154],100,100,100);tira.setPixelColor(Pantalla[ 153],100,100,100);tira.setPixelColor(Pantalla[ 152],100,100,100);tira.setPixelColor(Pantalla[ 151],100,100,100);tira.setPixelColor(Pantalla[ 150],100,100,100);tira.setPixelColor(Pantalla[ 149],100,100,100);tira.setPixelColor(Pantalla[ 148],100,100,100);
      tira.setPixelColor(Pantalla[ 164],100,100,100);tira.setPixelColor(Pantalla[ 165],100,100,100);tira.setPixelColor(Pantalla[ 166],100,100,100);tira.setPixelColor(Pantalla[ 167],100,100,100);tira.setPixelColor(Pantalla[ 168],100,100,100);tira.setPixelColor(Pantalla[ 169],100,100,100);tira.setPixelColor(Pantalla[ 170],100,100,100);
      tira.setPixelColor(Pantalla[ 187],105,210,30);tira.setPixelColor(Pantalla[ 186],105,210,30);tira.setPixelColor(Pantalla[ 185],105,210,30);tira.setPixelColor(Pantalla[ 184],105,210,30);tira.setPixelColor(Pantalla[ 183],105,210,30);tira.setPixelColor(Pantalla[ 182],105,210,30);tira.setPixelColor(Pantalla[ 181],105,210,30);
      tira.setPixelColor(Pantalla[ 195],100,100,100);tira.setPixelColor(Pantalla[ 196],100,100,100);tira.setPixelColor(Pantalla[ 197],100,100,100);tira.setPixelColor(Pantalla[ 198],255,0,0);tira.setPixelColor(Pantalla[ 199],255,0,0);tira.setPixelColor(Pantalla[ 200],255,0,0);tira.setPixelColor(Pantalla[ 201],100,100,100);tira.setPixelColor(Pantalla[ 202],100,100,100);tira.setPixelColor(Pantalla[ 203],100,100,100);
      tira.setPixelColor(Pantalla[ 221],100,100,100);tira.setPixelColor(Pantalla[ 220],100,100,100);tira.setPixelColor(Pantalla[ 219],100,100,100);tira.setPixelColor(Pantalla[ 218],100,100,100);tira.setPixelColor(Pantalla[ 217],255,0,0);tira.setPixelColor(Pantalla[ 216],255,0,0);tira.setPixelColor(Pantalla[ 215],255,0,0);tira.setPixelColor(Pantalla[ 214],100,100,100);tira.setPixelColor(Pantalla[ 213],100,100,100);tira.setPixelColor(Pantalla[ 212],100,100,100);tira.setPixelColor(Pantalla[ 211],100,100,100);



    }
  else
    {

      tira.setPixelColor(Pantalla[ 0],100,100,100);tira.setPixelColor(Pantalla[ 1],100,100,100);tira.setPixelColor(Pantalla[ 2],100,100,100);tira.setPixelColor(Pantalla[ 12],100,100,100);tira.setPixelColor(Pantalla[ 13],100,100,100);tira.setPixelColor(Pantalla[ 14],100,100,100);
      tira.setPixelColor(Pantalla[ 31],100,100,100);tira.setPixelColor(Pantalla[ 30],127,255,80);tira.setPixelColor(Pantalla[ 29],100,100,100);tira.setPixelColor(Pantalla[ 28],100,100,100);tira.setPixelColor(Pantalla[ 27],100,100,100);tira.setPixelColor(Pantalla[ 26],100,100,100);tira.setPixelColor(Pantalla[ 25],100,100,100);tira.setPixelColor(Pantalla[ 24],100,100,100);tira.setPixelColor(Pantalla[ 23],100,100,100);tira.setPixelColor(Pantalla[ 22],100,100,100);tira.setPixelColor(Pantalla[ 21],100,100,100);tira.setPixelColor(Pantalla[ 20],100,100,100);tira.setPixelColor(Pantalla[ 19],100,100,100);tira.setPixelColor(Pantalla[ 18],127,255,80);tira.setPixelColor(Pantalla[ 17],100,100,100);
      tira.setPixelColor(Pantalla[ 32],100,100,100);tira.setPixelColor(Pantalla[ 33],100,100,100);tira.setPixelColor(Pantalla[ 34],100,100,100);tira.setPixelColor(Pantalla[ 35],100,100,100);tira.setPixelColor(Pantalla[ 36],100,100,100);tira.setPixelColor(Pantalla[ 37],100,100,100);tira.setPixelColor(Pantalla[ 38],100,100,100);tira.setPixelColor(Pantalla[ 39],100,100,100);tira.setPixelColor(Pantalla[ 40],100,100,100);tira.setPixelColor(Pantalla[ 41],100,100,100);tira.setPixelColor(Pantalla[ 42],100,100,100);tira.setPixelColor(Pantalla[ 43],100,100,100);tira.setPixelColor(Pantalla[ 44],100,100,100);tira.setPixelColor(Pantalla[ 45],100,100,100);tira.setPixelColor(Pantalla[ 46],100,100,100);
      tira.setPixelColor(Pantalla[ 61],100,100,100);tira.setPixelColor(Pantalla[ 60],100,100,100);tira.setPixelColor(Pantalla[ 59],100,100,100);tira.setPixelColor(Pantalla[ 58],100,100,100);tira.setPixelColor(Pantalla[ 57],100,100,100);tira.setPixelColor(Pantalla[ 56],100,100,100);tira.setPixelColor(Pantalla[ 55],100,100,100);tira.setPixelColor(Pantalla[ 54],100,100,100);tira.setPixelColor(Pantalla[ 53],100,100,100);tira.setPixelColor(Pantalla[ 52],100,100,100);tira.setPixelColor(Pantalla[ 51],100,100,100);
      tira.setPixelColor(Pantalla[ 66],100,100,100);tira.setPixelColor(Pantalla[ 67],100,100,100);tira.setPixelColor(Pantalla[ 68],0,255,0);tira.setPixelColor(Pantalla[ 69],100,100,100);tira.setPixelColor(Pantalla[ 70],100,100,100);tira.setPixelColor(Pantalla[ 71],100,100,100);tira.setPixelColor(Pantalla[ 72],100,100,100);tira.setPixelColor(Pantalla[ 73],100,100,100);tira.setPixelColor(Pantalla[ 74],0,255,0);tira.setPixelColor(Pantalla[ 75],100,100,100);tira.setPixelColor(Pantalla[ 76],100,100,100);
      tira.setPixelColor(Pantalla[ 93],100,100,100);tira.setPixelColor(Pantalla[ 92],100,100,100);tira.setPixelColor(Pantalla[ 91],100,100,100);tira.setPixelColor(Pantalla[ 90],100,100,100);tira.setPixelColor(Pantalla[ 89],100,100,100);tira.setPixelColor(Pantalla[ 88],100,100,100);tira.setPixelColor(Pantalla[ 87],100,100,100);tira.setPixelColor(Pantalla[ 86],100,100,100);tira.setPixelColor(Pantalla[ 85],100,100,100);tira.setPixelColor(Pantalla[ 84],100,100,100);tira.setPixelColor(Pantalla[ 83],100,100,100);
      tira.setPixelColor(Pantalla[ 98],100,100,100);tira.setPixelColor(Pantalla[ 99],100,100,100);tira.setPixelColor(Pantalla[ 100],100,100,100);tira.setPixelColor(Pantalla[ 101],100,100,100);tira.setPixelColor(Pantalla[ 102],127,255,80);tira.setPixelColor(Pantalla[ 103],127,255,80);tira.setPixelColor(Pantalla[ 104],127,255,80);tira.setPixelColor(Pantalla[ 105],100,100,100);tira.setPixelColor(Pantalla[ 106],100,100,100);tira.setPixelColor(Pantalla[ 107],100,100,100);tira.setPixelColor(Pantalla[ 108],100,100,100);
      tira.setPixelColor(Pantalla[ 125],100,100,100);tira.setPixelColor(Pantalla[ 124],100,100,100);tira.setPixelColor(Pantalla[ 123],100,100,100);tira.setPixelColor(Pantalla[ 122],100,100,100);tira.setPixelColor(Pantalla[ 121],100,100,100);tira.setPixelColor(Pantalla[ 120],127,255,80);tira.setPixelColor(Pantalla[ 119],100,100,100);tira.setPixelColor(Pantalla[ 118],100,100,100);tira.setPixelColor(Pantalla[ 117],100,100,100);tira.setPixelColor(Pantalla[ 116],100,100,100);tira.setPixelColor(Pantalla[ 115],100,100,100);
      tira.setPixelColor(Pantalla[ 130],100,100,100);tira.setPixelColor(Pantalla[ 131],100,100,100);tira.setPixelColor(Pantalla[ 132],100,100,100);tira.setPixelColor(Pantalla[ 133],100,100,100);tira.setPixelColor(Pantalla[ 134],100,100,100);tira.setPixelColor(Pantalla[ 135],100,100,100);tira.setPixelColor(Pantalla[ 136],100,100,100);tira.setPixelColor(Pantalla[ 137],100,100,100);tira.setPixelColor(Pantalla[ 138],100,100,100);tira.setPixelColor(Pantalla[ 139],100,100,100);tira.setPixelColor(Pantalla[ 140],100,100,100);
      tira.setPixelColor(Pantalla[ 156],100,100,100);tira.setPixelColor(Pantalla[ 155],100,100,100);tira.setPixelColor(Pantalla[ 154],0,0,0);tira.setPixelColor(Pantalla[ 153],0,255,0);tira.setPixelColor(Pantalla[ 152],0,0,0);tira.setPixelColor(Pantalla[ 151],0,255,0);tira.setPixelColor(Pantalla[ 150],0,0,0);tira.setPixelColor(Pantalla[ 149],100,100,100);tira.setPixelColor(Pantalla[ 148],100,100,100);
      tira.setPixelColor(Pantalla[ 164],100,100,100);tira.setPixelColor(Pantalla[ 165],0,0,0);tira.setPixelColor(Pantalla[ 166],0,0,0);tira.setPixelColor(Pantalla[ 167],0,0,0);tira.setPixelColor(Pantalla[ 168],0,0,0);tira.setPixelColor(Pantalla[ 169],0,0,0);tira.setPixelColor(Pantalla[ 170],100,100,100);
      tira.setPixelColor(Pantalla[ 187],105,210,30);tira.setPixelColor(Pantalla[ 186],105,210,30);tira.setPixelColor(Pantalla[ 185],105,210,30);tira.setPixelColor(Pantalla[ 184],105,210,30);tira.setPixelColor(Pantalla[ 183],105,210,30);tira.setPixelColor(Pantalla[ 182],105,210,30);tira.setPixelColor(Pantalla[ 181],105,210,30);
      tira.setPixelColor(Pantalla[ 195],100,100,100);tira.setPixelColor(Pantalla[ 196],100,100,100);tira.setPixelColor(Pantalla[ 197],100,100,100);tira.setPixelColor(Pantalla[ 198],255,0,0);tira.setPixelColor(Pantalla[ 199],255,0,0);tira.setPixelColor(Pantalla[ 200],255,0,0);tira.setPixelColor(Pantalla[ 201],100,100,100);tira.setPixelColor(Pantalla[ 202],100,100,100);tira.setPixelColor(Pantalla[ 203],100,100,100);
      tira.setPixelColor(Pantalla[ 221],100,100,100);tira.setPixelColor(Pantalla[ 220],100,100,100);tira.setPixelColor(Pantalla[ 219],100,100,100);tira.setPixelColor(Pantalla[ 218],100,100,100);tira.setPixelColor(Pantalla[ 217],255,0,0);tira.setPixelColor(Pantalla[ 216],255,0,0);tira.setPixelColor(Pantalla[ 215],255,0,0);tira.setPixelColor(Pantalla[ 214],100,100,100);tira.setPixelColor(Pantalla[ 213],100,100,100);tira.setPixelColor(Pantalla[ 212],100,100,100);tira.setPixelColor(Pantalla[ 211],100,100,100);
      

    }





}

void Matriz_Rana()
{

  if (lado==IZQUIERDA)
    {
      tira.setPixelColor(Pantalla[ 2],100,100,100);tira.setPixelColor(Pantalla[ 3],100,100,100);tira.setPixelColor(Pantalla[ 4],100,100,100);tira.setPixelColor(Pantalla[ 5],100,100,100);tira.setPixelColor(Pantalla[ 10],100,100,100);tira.setPixelColor(Pantalla[ 11],100,100,100);tira.setPixelColor(Pantalla[ 12],100,100,100);tira.setPixelColor(Pantalla[ 13],100,100,100);
      tira.setPixelColor(Pantalla[ 29],100,100,100);tira.setPixelColor(Pantalla[ 26],100,100,100);tira.setPixelColor(Pantalla[ 25],0,0,255);tira.setPixelColor(Pantalla[ 22],0,0,255);tira.setPixelColor(Pantalla[ 21],100,100,100);tira.setPixelColor(Pantalla[ 18],100,100,100);
      tira.setPixelColor(Pantalla[ 34],100,100,100);tira.setPixelColor(Pantalla[ 37],100,100,100);tira.setPixelColor(Pantalla[ 38],0,0,255);tira.setPixelColor(Pantalla[ 39],255,0,0);tira.setPixelColor(Pantalla[ 40],255,0,0);tira.setPixelColor(Pantalla[ 41],0,0,255);tira.setPixelColor(Pantalla[ 42],100,100,100);tira.setPixelColor(Pantalla[ 45],100,100,100);
      tira.setPixelColor(Pantalla[ 61],100,100,100);tira.setPixelColor(Pantalla[ 60],100,100,100);tira.setPixelColor(Pantalla[ 59],100,100,100);tira.setPixelColor(Pantalla[ 58],100,100,100);tira.setPixelColor(Pantalla[ 57],0,0,255);tira.setPixelColor(Pantalla[ 56],255,0,0);tira.setPixelColor(Pantalla[ 55],255,0,0);tira.setPixelColor(Pantalla[ 54],0,0,255);tira.setPixelColor(Pantalla[ 53],100,100,100);tira.setPixelColor(Pantalla[ 52],100,100,100);tira.setPixelColor(Pantalla[ 51],100,100,100);tira.setPixelColor(Pantalla[ 50],100,100,100);
      tira.setPixelColor(Pantalla[ 67],0,0,255);tira.setPixelColor(Pantalla[ 68],0,0,255);tira.setPixelColor(Pantalla[ 69],0,0,255);tira.setPixelColor(Pantalla[ 70],0,0,255);tira.setPixelColor(Pantalla[ 71],255,0,0);tira.setPixelColor(Pantalla[ 72],255,0,0);tira.setPixelColor(Pantalla[ 73],0,0,255);tira.setPixelColor(Pantalla[ 74],0,0,255);tira.setPixelColor(Pantalla[ 75],0,0,255);tira.setPixelColor(Pantalla[ 76],0,0,255);
      tira.setPixelColor(Pantalla[ 93],255,0,0);tira.setPixelColor(Pantalla[ 92],255,0,0);tira.setPixelColor(Pantalla[ 91],255,0,0);tira.setPixelColor(Pantalla[ 90],255,0,0);tira.setPixelColor(Pantalla[ 89],255,0,0);tira.setPixelColor(Pantalla[ 88],255,0,0);tira.setPixelColor(Pantalla[ 87],255,0,0);tira.setPixelColor(Pantalla[ 86],255,0,0);tira.setPixelColor(Pantalla[ 85],255,0,0);tira.setPixelColor(Pantalla[ 84],255,0,0);tira.setPixelColor(Pantalla[ 83],255,0,0);tira.setPixelColor(Pantalla[ 82],255,0,0);
      tira.setPixelColor(Pantalla[ 97],255,0,0);tira.setPixelColor(Pantalla[ 98],255,0,0);tira.setPixelColor(Pantalla[ 99],255,0,0);tira.setPixelColor(Pantalla[ 100],0,0,0);tira.setPixelColor(Pantalla[ 101],255,0,0);tira.setPixelColor(Pantalla[ 102],255,0,0);tira.setPixelColor(Pantalla[ 103],255,0,0);tira.setPixelColor(Pantalla[ 104],255,0,0);tira.setPixelColor(Pantalla[ 105],255,0,0);tira.setPixelColor(Pantalla[ 106],255,0,0);tira.setPixelColor(Pantalla[ 107],0,0,0);tira.setPixelColor(Pantalla[ 108],255,0,0);tira.setPixelColor(Pantalla[ 109],255,0,0);tira.setPixelColor(Pantalla[ 110],255,0,0);
      tira.setPixelColor(Pantalla[ 126],255,0,0);tira.setPixelColor(Pantalla[ 125],255,0,0);tira.setPixelColor(Pantalla[ 124],255,0,0);tira.setPixelColor(Pantalla[ 123],255,0,0);tira.setPixelColor(Pantalla[ 122],0,0,0);tira.setPixelColor(Pantalla[ 121],0,0,0);tira.setPixelColor(Pantalla[ 120],0,0,0);tira.setPixelColor(Pantalla[ 119],0,0,0);tira.setPixelColor(Pantalla[ 118],0,0,0);tira.setPixelColor(Pantalla[ 117],0,0,0);tira.setPixelColor(Pantalla[ 116],255,0,0);tira.setPixelColor(Pantalla[ 115],255,0,0);tira.setPixelColor(Pantalla[ 114],255,0,0);tira.setPixelColor(Pantalla[ 113],255,0,0);
      tira.setPixelColor(Pantalla[ 130],255,0,0);tira.setPixelColor(Pantalla[ 131],255,0,0);tira.setPixelColor(Pantalla[ 132],255,0,0);tira.setPixelColor(Pantalla[ 133],255,0,0);tira.setPixelColor(Pantalla[ 134],255,0,0);tira.setPixelColor(Pantalla[ 135],255,0,0);tira.setPixelColor(Pantalla[ 136],255,0,0);tira.setPixelColor(Pantalla[ 137],255,0,0);tira.setPixelColor(Pantalla[ 138],255,0,0);tira.setPixelColor(Pantalla[ 139],255,0,0);tira.setPixelColor(Pantalla[ 140],255,0,0);tira.setPixelColor(Pantalla[ 141],255,0,0);
      tira.setPixelColor(Pantalla[ 154],255,0,0);tira.setPixelColor(Pantalla[ 153],255,0,0);tira.setPixelColor(Pantalla[ 152],255,0,0);tira.setPixelColor(Pantalla[ 151],255,0,0);tira.setPixelColor(Pantalla[ 150],255,0,0);tira.setPixelColor(Pantalla[ 149],255,0,0);
      tira.setPixelColor(Pantalla[ 164],255,0,0);tira.setPixelColor(Pantalla[ 167],255,0,0);tira.setPixelColor(Pantalla[ 168],255,0,0);tira.setPixelColor(Pantalla[ 171],255,0,0);
      tira.setPixelColor(Pantalla[ 188],255,0,0);tira.setPixelColor(Pantalla[ 187],255,0,0);tira.setPixelColor(Pantalla[ 186],255,0,0);tira.setPixelColor(Pantalla[ 185],255,0,0);tira.setPixelColor(Pantalla[ 184],100,100,100);tira.setPixelColor(Pantalla[ 183],100,100,100);tira.setPixelColor(Pantalla[ 182],255,0,0);tira.setPixelColor(Pantalla[ 181],255,0,0);tira.setPixelColor(Pantalla[ 180],255,0,0);tira.setPixelColor(Pantalla[ 179],255,0,0);
      tira.setPixelColor(Pantalla[ 193],255,0,0);tira.setPixelColor(Pantalla[ 195],255,0,0);tira.setPixelColor(Pantalla[ 196],255,0,0);tira.setPixelColor(Pantalla[ 197],255,0,0);tira.setPixelColor(Pantalla[ 198],255,0,0);tira.setPixelColor(Pantalla[ 199],100,100,100);tira.setPixelColor(Pantalla[ 200],100,100,100);tira.setPixelColor(Pantalla[ 201],255,0,0);tira.setPixelColor(Pantalla[ 202],255,0,0);tira.setPixelColor(Pantalla[ 203],255,0,0);tira.setPixelColor(Pantalla[ 204],255,0,0);tira.setPixelColor(Pantalla[ 206],255,0,0);
      tira.setPixelColor(Pantalla[ 222],255,0,0);tira.setPixelColor(Pantalla[ 221],255,0,0);tira.setPixelColor(Pantalla[ 220],255,0,0);tira.setPixelColor(Pantalla[ 218],255,0,0);tira.setPixelColor(Pantalla[ 216],100,100,100);tira.setPixelColor(Pantalla[ 215],100,100,100);tira.setPixelColor(Pantalla[ 213],255,0,0);tira.setPixelColor(Pantalla[ 211],255,0,0);tira.setPixelColor(Pantalla[ 210],255,0,0);tira.setPixelColor(Pantalla[ 209],255,0,0);
      
    }
  else
    {
      tira.setPixelColor(Pantalla[ 2],100,100,100);tira.setPixelColor(Pantalla[ 3],100,100,100);tira.setPixelColor(Pantalla[ 4],100,100,100);tira.setPixelColor(Pantalla[ 5],100,100,100);tira.setPixelColor(Pantalla[ 10],100,100,100);tira.setPixelColor(Pantalla[ 11],100,100,100);tira.setPixelColor(Pantalla[ 12],100,100,100);tira.setPixelColor(Pantalla[ 13],100,100,100);
      tira.setPixelColor(Pantalla[ 29],100,100,100);tira.setPixelColor(Pantalla[ 28],0,255,0);tira.setPixelColor(Pantalla[ 27],0,255,0);tira.setPixelColor(Pantalla[ 26],100,100,100);tira.setPixelColor(Pantalla[ 25],0,0,255);tira.setPixelColor(Pantalla[ 22],0,0,255);tira.setPixelColor(Pantalla[ 21],100,100,100);tira.setPixelColor(Pantalla[ 20],0,255,0);tira.setPixelColor(Pantalla[ 19],0,255,0);tira.setPixelColor(Pantalla[ 18],100,100,100);
      tira.setPixelColor(Pantalla[ 34],100,100,100);tira.setPixelColor(Pantalla[ 35],0,255,0);tira.setPixelColor(Pantalla[ 36],0,255,0);tira.setPixelColor(Pantalla[ 37],100,100,100);tira.setPixelColor(Pantalla[ 38],0,0,255);tira.setPixelColor(Pantalla[ 39],255,0,0);tira.setPixelColor(Pantalla[ 40],255,0,0);tira.setPixelColor(Pantalla[ 41],0,0,255);tira.setPixelColor(Pantalla[ 42],100,100,100);tira.setPixelColor(Pantalla[ 43],0,255,0);tira.setPixelColor(Pantalla[ 44],0,255,0);tira.setPixelColor(Pantalla[ 45],100,100,100);
      tira.setPixelColor(Pantalla[ 61],100,100,100);tira.setPixelColor(Pantalla[ 60],100,100,100);tira.setPixelColor(Pantalla[ 59],100,100,100);tira.setPixelColor(Pantalla[ 58],100,100,100);tira.setPixelColor(Pantalla[ 57],0,0,255);tira.setPixelColor(Pantalla[ 56],255,0,0);tira.setPixelColor(Pantalla[ 55],255,0,0);tira.setPixelColor(Pantalla[ 54],0,0,255);tira.setPixelColor(Pantalla[ 53],100,100,100);tira.setPixelColor(Pantalla[ 52],100,100,100);tira.setPixelColor(Pantalla[ 51],100,100,100);tira.setPixelColor(Pantalla[ 50],100,100,100);
      tira.setPixelColor(Pantalla[ 67],0,0,255);tira.setPixelColor(Pantalla[ 68],0,0,255);tira.setPixelColor(Pantalla[ 69],0,0,255);tira.setPixelColor(Pantalla[ 70],0,0,255);tira.setPixelColor(Pantalla[ 71],255,0,0);tira.setPixelColor(Pantalla[ 72],255,0,0);tira.setPixelColor(Pantalla[ 73],0,0,255);tira.setPixelColor(Pantalla[ 74],0,0,255);tira.setPixelColor(Pantalla[ 75],0,0,255);tira.setPixelColor(Pantalla[ 76],0,0,255);
      tira.setPixelColor(Pantalla[ 93],255,0,0);tira.setPixelColor(Pantalla[ 92],255,0,0);tira.setPixelColor(Pantalla[ 91],255,0,0);tira.setPixelColor(Pantalla[ 90],255,0,0);tira.setPixelColor(Pantalla[ 89],255,0,0);tira.setPixelColor(Pantalla[ 88],255,0,0);tira.setPixelColor(Pantalla[ 87],255,0,0);tira.setPixelColor(Pantalla[ 86],255,0,0);tira.setPixelColor(Pantalla[ 85],255,0,0);tira.setPixelColor(Pantalla[ 84],255,0,0);tira.setPixelColor(Pantalla[ 83],255,0,0);tira.setPixelColor(Pantalla[ 82],255,0,0);
      tira.setPixelColor(Pantalla[ 97],255,0,0);tira.setPixelColor(Pantalla[ 98],255,0,0);tira.setPixelColor(Pantalla[ 99],0,0,0);tira.setPixelColor(Pantalla[ 100],0,0,0);tira.setPixelColor(Pantalla[ 101],0,255,0);tira.setPixelColor(Pantalla[ 102],0,0,0);tira.setPixelColor(Pantalla[ 103],0,0,0);tira.setPixelColor(Pantalla[ 104],0,0,0);tira.setPixelColor(Pantalla[ 105],0,0,0);tira.setPixelColor(Pantalla[ 106],0,255,0);tira.setPixelColor(Pantalla[ 107],0,0,0);tira.setPixelColor(Pantalla[ 108],0,0,0);tira.setPixelColor(Pantalla[ 109],255,0,0);tira.setPixelColor(Pantalla[ 110],255,0,0);
      tira.setPixelColor(Pantalla[ 126],255,0,0);tira.setPixelColor(Pantalla[ 125],255,0,0);tira.setPixelColor(Pantalla[ 124],255,0,0);tira.setPixelColor(Pantalla[ 123],0,0,0);tira.setPixelColor(Pantalla[ 122],0,0,0);tira.setPixelColor(Pantalla[ 121],0,0,0);tira.setPixelColor(Pantalla[ 120],0,0,0);tira.setPixelColor(Pantalla[ 119],0,0,0);tira.setPixelColor(Pantalla[ 118],0,0,0);tira.setPixelColor(Pantalla[ 117],0,0,0);tira.setPixelColor(Pantalla[ 116],0,0,0);tira.setPixelColor(Pantalla[ 115],255,0,0);tira.setPixelColor(Pantalla[ 114],255,0,0);tira.setPixelColor(Pantalla[ 113],255,0,0);
      tira.setPixelColor(Pantalla[ 130],255,0,0);tira.setPixelColor(Pantalla[ 131],255,0,0);tira.setPixelColor(Pantalla[ 132],255,0,0);tira.setPixelColor(Pantalla[ 133],255,0,0);tira.setPixelColor(Pantalla[ 134],255,0,0);tira.setPixelColor(Pantalla[ 135],255,0,0);tira.setPixelColor(Pantalla[ 136],255,0,0);tira.setPixelColor(Pantalla[ 137],255,0,0);tira.setPixelColor(Pantalla[ 138],255,0,0);tira.setPixelColor(Pantalla[ 139],255,0,0);tira.setPixelColor(Pantalla[ 140],255,0,0);tira.setPixelColor(Pantalla[ 141],255,0,0);
      tira.setPixelColor(Pantalla[ 154],255,0,0);tira.setPixelColor(Pantalla[ 153],255,0,0);tira.setPixelColor(Pantalla[ 152],255,0,0);tira.setPixelColor(Pantalla[ 151],255,0,0);tira.setPixelColor(Pantalla[ 150],255,0,0);tira.setPixelColor(Pantalla[ 149],255,0,0);
      tira.setPixelColor(Pantalla[ 164],255,0,0);tira.setPixelColor(Pantalla[ 167],255,0,0);tira.setPixelColor(Pantalla[ 168],255,0,0);tira.setPixelColor(Pantalla[ 171],255,0,0);
      tira.setPixelColor(Pantalla[ 188],255,0,0);tira.setPixelColor(Pantalla[ 187],255,0,0);tira.setPixelColor(Pantalla[ 186],255,0,0);tira.setPixelColor(Pantalla[ 185],255,0,0);tira.setPixelColor(Pantalla[ 184],100,100,100);tira.setPixelColor(Pantalla[ 183],100,100,100);tira.setPixelColor(Pantalla[ 182],255,0,0);tira.setPixelColor(Pantalla[ 181],255,0,0);tira.setPixelColor(Pantalla[ 180],255,0,0);tira.setPixelColor(Pantalla[ 179],255,0,0);
      tira.setPixelColor(Pantalla[ 193],255,0,0);tira.setPixelColor(Pantalla[ 195],255,0,0);tira.setPixelColor(Pantalla[ 196],255,0,0);tira.setPixelColor(Pantalla[ 197],255,0,0);tira.setPixelColor(Pantalla[ 198],255,0,0);tira.setPixelColor(Pantalla[ 199],100,100,100);tira.setPixelColor(Pantalla[ 200],100,100,100);tira.setPixelColor(Pantalla[ 201],255,0,0);tira.setPixelColor(Pantalla[ 202],255,0,0);tira.setPixelColor(Pantalla[ 203],255,0,0);tira.setPixelColor(Pantalla[ 204],255,0,0);tira.setPixelColor(Pantalla[ 206],255,0,0);
      tira.setPixelColor(Pantalla[ 222],255,0,0);tira.setPixelColor(Pantalla[ 221],255,0,0);tira.setPixelColor(Pantalla[ 220],255,0,0);tira.setPixelColor(Pantalla[ 218],255,0,0);tira.setPixelColor(Pantalla[ 216],100,100,100);tira.setPixelColor(Pantalla[ 215],100,100,100);tira.setPixelColor(Pantalla[ 213],255,0,0);tira.setPixelColor(Pantalla[ 211],255,0,0);tira.setPixelColor(Pantalla[ 210],255,0,0);tira.setPixelColor(Pantalla[ 209],255,0,0);
      
    }


}

void Matriz_Unicornio()
{
  if (lado==IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[ 0],0,255,0);tira.setPixelColor(Pantalla[ 1],0,255,0);tira.setPixelColor(Pantalla[ 2],0,0,0);tira.setPixelColor(Pantalla[ 3],0,0,0);tira.setPixelColor(Pantalla[ 4],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 5],0,255,0);tira.setPixelColor(Pantalla[ 6],0,0,0);tira.setPixelColor(Pantalla[ 7],0,0,0);tira.setPixelColor(Pantalla[ 8],0,255,0);tira.setPixelColor(Pantalla[ 9],0,255,0);tira.setPixelColor(Pantalla[ 10],0,0,0);tira.setPixelColor(Pantalla[ 11],0,0,0);tira.setPixelColor(Pantalla[ 12],0,255,0);tira.setPixelColor(Pantalla[ 13],0,255,0);tira.setPixelColor(Pantalla[ 14],0,0,0);tira.setPixelColor(Pantalla[ 15],0,0,0);
    tira.setPixelColor(Pantalla[ 31],0,0,0);tira.setPixelColor(Pantalla[ 27],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 16],0,255,0);
    tira.setPixelColor(Pantalla[ 32],0,0,0);tira.setPixelColor(Pantalla[ 35],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 36],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 37],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 43],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 44],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 47],0,255,0);
    tira.setPixelColor(Pantalla[ 63],0,255,0);tira.setPixelColor(Pantalla[ 61],100,100,100);tira.setPixelColor(Pantalla[ 60],100,100,100);tira.setPixelColor(Pantalla[ 59],100,100,100);tira.setPixelColor(Pantalla[ 58],100,100,100);tira.setPixelColor(Pantalla[ 57],100,100,100);tira.setPixelColor(Pantalla[ 53],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 52],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 51],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 50],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 48],0,0,0);
    tira.setPixelColor(Pantalla[ 64],0,255,0);tira.setPixelColor(Pantalla[ 66],100,100,100);tira.setPixelColor(Pantalla[ 67],0,0,255);tira.setPixelColor(Pantalla[ 68],100,100,100);tira.setPixelColor(Pantalla[ 69],100,100,100);tira.setPixelColor(Pantalla[ 70],100,100,100);tira.setPixelColor(Pantalla[ 73],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 74],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 75],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 76],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 77],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 78],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 79],0,0,0);
    tira.setPixelColor(Pantalla[ 95],0,0,0);tira.setPixelColor(Pantalla[ 94],100,100,100);tira.setPixelColor(Pantalla[ 93],100,100,100);tira.setPixelColor(Pantalla[ 92],0,0,255);tira.setPixelColor(Pantalla[ 91],100,100,100);tira.setPixelColor(Pantalla[ 90],100,100,100);tira.setPixelColor(Pantalla[ 89],100,100,100);tira.setPixelColor(Pantalla[ 86],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 85],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 84],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 83],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 82],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 81],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 80],0,255,0);
    tira.setPixelColor(Pantalla[ 96],0,0,0);tira.setPixelColor(Pantalla[ 97],100,100,100);tira.setPixelColor(Pantalla[ 98],100,100,100);tira.setPixelColor(Pantalla[ 99],100,100,100);tira.setPixelColor(Pantalla[ 100],100,100,100);tira.setPixelColor(Pantalla[ 101],100,100,100);tira.setPixelColor(Pantalla[ 102],100,100,100);tira.setPixelColor(Pantalla[ 105],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 106],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 107],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 109],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 110],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 111],0,255,0);
    tira.setPixelColor(Pantalla[ 127],0,255,0);tira.setPixelColor(Pantalla[ 123],255,0,255);tira.setPixelColor(Pantalla[ 122],255,255,0);tira.setPixelColor(Pantalla[ 121],0,255,0);tira.setPixelColor(Pantalla[ 117],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 116],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 113],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 112],0,0,0);
    tira.setPixelColor(Pantalla[ 128],0,255,0);tira.setPixelColor(Pantalla[ 132],0,0,255);tira.setPixelColor(Pantalla[ 133],100,100,100);tira.setPixelColor(Pantalla[ 134],100,100,100);tira.setPixelColor(Pantalla[ 135],100,100,100);tira.setPixelColor(Pantalla[ 136],100,100,100);tira.setPixelColor(Pantalla[ 137],100,100,100);tira.setPixelColor(Pantalla[ 138],105,210,30);tira.setPixelColor(Pantalla[ 143],0,0,0);
    tira.setPixelColor(Pantalla[ 159],0,0,0);tira.setPixelColor(Pantalla[ 155],100,100,100);tira.setPixelColor(Pantalla[ 154],100,100,100);tira.setPixelColor(Pantalla[ 153],100,100,100);tira.setPixelColor(Pantalla[ 152],100,100,100);tira.setPixelColor(Pantalla[ 151],100,100,100);tira.setPixelColor(Pantalla[ 150],100,100,100);tira.setPixelColor(Pantalla[ 149],100,100,100);tira.setPixelColor(Pantalla[ 144],0,255,0);
    tira.setPixelColor(Pantalla[ 160],0,0,0);tira.setPixelColor(Pantalla[ 164],100,100,100);tira.setPixelColor(Pantalla[ 165],100,100,100);tira.setPixelColor(Pantalla[ 166],100,100,100);tira.setPixelColor(Pantalla[ 167],100,100,100);tira.setPixelColor(Pantalla[ 168],100,100,100);tira.setPixelColor(Pantalla[ 169],100,100,100);tira.setPixelColor(Pantalla[ 170],100,100,100);tira.setPixelColor(Pantalla[ 175],0,255,0);
    tira.setPixelColor(Pantalla[ 191],0,255,0);tira.setPixelColor(Pantalla[ 187],100,100,100);tira.setPixelColor(Pantalla[ 186],100,100,100);tira.setPixelColor(Pantalla[ 183],0,255,0);tira.setPixelColor(Pantalla[ 182],100,100,100);tira.setPixelColor(Pantalla[ 181],100,100,100);tira.setPixelColor(Pantalla[ 176],0,0,0);
    tira.setPixelColor(Pantalla[ 192],0,255,0);tira.setPixelColor(Pantalla[ 196],100,100,100);tira.setPixelColor(Pantalla[ 197],100,100,100);tira.setPixelColor(Pantalla[ 201],100,100,100);tira.setPixelColor(Pantalla[ 202],100,100,100);tira.setPixelColor(Pantalla[ 207],0,0,0);
    tira.setPixelColor(Pantalla[ 223],0,0,0);tira.setPixelColor(Pantalla[ 222],0,0,0);tira.setPixelColor(Pantalla[ 221],0,255,0);tira.setPixelColor(Pantalla[ 220],105,210,30);tira.setPixelColor(Pantalla[ 219],105,210,30);tira.setPixelColor(Pantalla[ 218],0,0,0);tira.setPixelColor(Pantalla[ 217],0,255,0);tira.setPixelColor(Pantalla[ 216],0,255,0);tira.setPixelColor(Pantalla[ 215],105,210,30);tira.setPixelColor(Pantalla[ 214],105,210,30);tira.setPixelColor(Pantalla[ 213],0,255,0);tira.setPixelColor(Pantalla[ 212],0,255,0);tira.setPixelColor(Pantalla[ 211],0,0,0);tira.setPixelColor(Pantalla[ 210],0,0,0);tira.setPixelColor(Pantalla[ 209],0,255,0);tira.setPixelColor(Pantalla[ 208],0,255,0);
    
  }
  else
  {
    tira.setPixelColor(Pantalla[ 0],0,0,0);tira.setPixelColor(Pantalla[ 1],0,0,0);tira.setPixelColor(Pantalla[ 2],0,255,0);tira.setPixelColor(Pantalla[ 3],0,255,0);tira.setPixelColor(Pantalla[ 4],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 5],0,0,0);tira.setPixelColor(Pantalla[ 6],0,255,0);tira.setPixelColor(Pantalla[ 7],0,255,0);tira.setPixelColor(Pantalla[ 8],0,0,0);tira.setPixelColor(Pantalla[ 9],0,0,0);tira.setPixelColor(Pantalla[ 10],0,255,0);tira.setPixelColor(Pantalla[ 11],0,255,0);tira.setPixelColor(Pantalla[ 12],0,0,0);tira.setPixelColor(Pantalla[ 13],0,0,0);tira.setPixelColor(Pantalla[ 14],0,255,0);tira.setPixelColor(Pantalla[ 15],0,255,0);
    tira.setPixelColor(Pantalla[ 31],0,255,0);tira.setPixelColor(Pantalla[ 27],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 16],0,0,0);
    tira.setPixelColor(Pantalla[ 32],0,255,0);tira.setPixelColor(Pantalla[ 35],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 36],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 37],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 43],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 44],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 47],0,0,0);
    tira.setPixelColor(Pantalla[ 63],0,0,0);tira.setPixelColor(Pantalla[ 61],100,100,100);tira.setPixelColor(Pantalla[ 60],100,100,100);tira.setPixelColor(Pantalla[ 59],100,100,100);tira.setPixelColor(Pantalla[ 58],100,100,100);tira.setPixelColor(Pantalla[ 57],100,100,100);tira.setPixelColor(Pantalla[ 53],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 52],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 51],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 50],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 48],0,255,0);
    tira.setPixelColor(Pantalla[ 64],0,0,0);tira.setPixelColor(Pantalla[ 66],100,100,100);tira.setPixelColor(Pantalla[ 67],0,0,255);tira.setPixelColor(Pantalla[ 68],100,100,100);tira.setPixelColor(Pantalla[ 69],100,100,100);tira.setPixelColor(Pantalla[ 70],100,100,100);tira.setPixelColor(Pantalla[ 73],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 74],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 75],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 76],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 77],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 78],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 79],0,255,0);
    tira.setPixelColor(Pantalla[ 95],0,255,0);tira.setPixelColor(Pantalla[ 94],100,100,100);tira.setPixelColor(Pantalla[ 93],100,100,100);tira.setPixelColor(Pantalla[ 92],0,0,255);tira.setPixelColor(Pantalla[ 91],100,100,100);tira.setPixelColor(Pantalla[ 90],100,100,100);tira.setPixelColor(Pantalla[ 89],100,100,100);tira.setPixelColor(Pantalla[ 86],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 85],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 84],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 83],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 82],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 81],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 80],0,0,0);
    tira.setPixelColor(Pantalla[ 96],0,255,0);tira.setPixelColor(Pantalla[ 97],100,100,100);tira.setPixelColor(Pantalla[ 98],100,100,100);tira.setPixelColor(Pantalla[ 99],100,100,100);tira.setPixelColor(Pantalla[ 100],100,100,100);tira.setPixelColor(Pantalla[ 101],100,100,100);tira.setPixelColor(Pantalla[ 102],100,100,100);tira.setPixelColor(Pantalla[ 105],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 106],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 107],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 109],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 110],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 111],0,0,0);
    tira.setPixelColor(Pantalla[ 127],0,0,0);tira.setPixelColor(Pantalla[ 123],255,0,255);tira.setPixelColor(Pantalla[ 122],255,255,0);tira.setPixelColor(Pantalla[ 121],0,255,0);tira.setPixelColor(Pantalla[ 117],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 116],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 113],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 112],0,255,0);
    tira.setPixelColor(Pantalla[ 128],0,0,0);tira.setPixelColor(Pantalla[ 132],0,0,255);tira.setPixelColor(Pantalla[ 133],100,100,100);tira.setPixelColor(Pantalla[ 134],100,100,100);tira.setPixelColor(Pantalla[ 135],100,100,100);tira.setPixelColor(Pantalla[ 136],100,100,100);tira.setPixelColor(Pantalla[ 137],100,100,100);tira.setPixelColor(Pantalla[ 138],105,210,30);tira.setPixelColor(Pantalla[ 143],0,255,0);
    tira.setPixelColor(Pantalla[ 159],0,255,0);tira.setPixelColor(Pantalla[ 155],100,100,100);tira.setPixelColor(Pantalla[ 154],100,100,100);tira.setPixelColor(Pantalla[ 153],100,100,100);tira.setPixelColor(Pantalla[ 152],100,100,100);tira.setPixelColor(Pantalla[ 151],100,100,100);tira.setPixelColor(Pantalla[ 150],100,100,100);tira.setPixelColor(Pantalla[ 149],100,100,100);tira.setPixelColor(Pantalla[ 144],0,0,0);
    tira.setPixelColor(Pantalla[ 160],0,255,0);tira.setPixelColor(Pantalla[ 164],100,100,100);tira.setPixelColor(Pantalla[ 165],100,100,100);tira.setPixelColor(Pantalla[ 166],100,100,100);tira.setPixelColor(Pantalla[ 167],100,100,100);tira.setPixelColor(Pantalla[ 168],100,100,100);tira.setPixelColor(Pantalla[ 169],100,100,100);tira.setPixelColor(Pantalla[ 170],100,100,100);tira.setPixelColor(Pantalla[ 175],0,0,0);
    tira.setPixelColor(Pantalla[ 191],0,0,0);tira.setPixelColor(Pantalla[ 187],100,100,100);tira.setPixelColor(Pantalla[ 186],100,100,100);tira.setPixelColor(Pantalla[ 183],0,255,0);tira.setPixelColor(Pantalla[ 182],100,100,100);tira.setPixelColor(Pantalla[ 181],100,100,100);tira.setPixelColor(Pantalla[ 176],0,255,0);
    tira.setPixelColor(Pantalla[ 192],0,0,0);tira.setPixelColor(Pantalla[ 196],100,100,100);tira.setPixelColor(Pantalla[ 197],100,100,100);tira.setPixelColor(Pantalla[ 201],100,100,100);tira.setPixelColor(Pantalla[ 202],100,100,100);tira.setPixelColor(Pantalla[ 207],0,255,0);
    tira.setPixelColor(Pantalla[ 223],0,255,0);tira.setPixelColor(Pantalla[ 222],0,255,0);tira.setPixelColor(Pantalla[ 221],0,0,0);tira.setPixelColor(Pantalla[ 220],105,210,30);tira.setPixelColor(Pantalla[ 219],105,210,30);tira.setPixelColor(Pantalla[ 218],0,255,0);tira.setPixelColor(Pantalla[ 217],0,0,0);tira.setPixelColor(Pantalla[ 216],0,0,0);tira.setPixelColor(Pantalla[ 215],105,210,30);tira.setPixelColor(Pantalla[ 214],105,210,30);tira.setPixelColor(Pantalla[ 213],0,0,0);tira.setPixelColor(Pantalla[ 212],0,0,0);tira.setPixelColor(Pantalla[ 211],0,255,0);tira.setPixelColor(Pantalla[ 210],0,255,0);tira.setPixelColor(Pantalla[ 209],0,0,0);tira.setPixelColor(Pantalla[ 208],0,0,0);
    
  }



}

void Matriz_Botella()
{
  if (lado==IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[ 0],0,255,0);tira.setPixelColor(Pantalla[ 1],0,255,0);tira.setPixelColor(Pantalla[ 2],0,255,0);tira.setPixelColor(Pantalla[ 3],0,0,0);tira.setPixelColor(Pantalla[ 4],0,0,0);tira.setPixelColor(Pantalla[ 5],0,0,0);tira.setPixelColor(Pantalla[ 6],0,255,0);tira.setPixelColor(Pantalla[ 7],0,255,0);tira.setPixelColor(Pantalla[ 8],0,255,0);tira.setPixelColor(Pantalla[ 9],0,0,0);tira.setPixelColor(Pantalla[ 10],0,0,0);tira.setPixelColor(Pantalla[ 11],0,0,0);tira.setPixelColor(Pantalla[ 12],0,255,0);tira.setPixelColor(Pantalla[ 13],0,255,0);tira.setPixelColor(Pantalla[ 14],0,255,0);tira.setPixelColor(Pantalla[ 15],0,0,0);
    tira.setPixelColor(Pantalla[ 31],0,0,0);tira.setPixelColor(Pantalla[ 30],0,0,0);tira.setPixelColor(Pantalla[ 25],255,0,0);tira.setPixelColor(Pantalla[ 24],255,0,0);tira.setPixelColor(Pantalla[ 23],255,0,0);tira.setPixelColor(Pantalla[ 22],255,0,0);tira.setPixelColor(Pantalla[ 20],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 19],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 16],0,0,0);
    tira.setPixelColor(Pantalla[ 32],0,0,0);tira.setPixelColor(Pantalla[ 34],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 35],0,0,0);tira.setPixelColor(Pantalla[ 38],255,0,0);tira.setPixelColor(Pantalla[ 39],255,0,0);tira.setPixelColor(Pantalla[ 40],255,0,0);tira.setPixelColor(Pantalla[ 41],255,0,0);tira.setPixelColor(Pantalla[ 47],0,0,0);
    tira.setPixelColor(Pantalla[ 63],0,0,0);tira.setPixelColor(Pantalla[ 56],105,210,30);tira.setPixelColor(Pantalla[ 55],105,210,30);tira.setPixelColor(Pantalla[ 52],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 50],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 49],0,0,0);tira.setPixelColor(Pantalla[ 48],0,255,0);
    tira.setPixelColor(Pantalla[ 64],0,255,0);tira.setPixelColor(Pantalla[ 71],0,0,255);tira.setPixelColor(Pantalla[ 72],0,0,255);tira.setPixelColor(Pantalla[ 75],0,0,0);tira.setPixelColor(Pantalla[ 79],0,255,0);
    tira.setPixelColor(Pantalla[ 95],0,255,0);tira.setPixelColor(Pantalla[ 93],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 89],0,0,255);tira.setPixelColor(Pantalla[ 88],100,100,100);tira.setPixelColor(Pantalla[ 87],100,100,100);tira.setPixelColor(Pantalla[ 86],0,0,255);tira.setPixelColor(Pantalla[ 82],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 80],0,255,0);
    tira.setPixelColor(Pantalla[ 96],0,255,0);tira.setPixelColor(Pantalla[ 98],0,0,0);tira.setPixelColor(Pantalla[ 99],0,0,0);tira.setPixelColor(Pantalla[ 101],0,0,255);tira.setPixelColor(Pantalla[ 102],100,100,100);tira.setPixelColor(Pantalla[ 103],100,100,100);tira.setPixelColor(Pantalla[ 104],100,100,100);tira.setPixelColor(Pantalla[ 105],100,100,100);tira.setPixelColor(Pantalla[ 106],0,0,255);tira.setPixelColor(Pantalla[ 109],0,0,0);tira.setPixelColor(Pantalla[ 111],0,0,0);
    tira.setPixelColor(Pantalla[ 127],0,0,0);tira.setPixelColor(Pantalla[ 124],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 122],0,0,255);tira.setPixelColor(Pantalla[ 121],0,255,0);tira.setPixelColor(Pantalla[ 120],0,255,0);tira.setPixelColor(Pantalla[ 119],0,255,0);tira.setPixelColor(Pantalla[ 118],0,255,0);tira.setPixelColor(Pantalla[ 117],0,0,255);tira.setPixelColor(Pantalla[ 112],0,0,0);
    tira.setPixelColor(Pantalla[ 128],0,0,0);tira.setPixelColor(Pantalla[ 129],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 133],0,0,255);tira.setPixelColor(Pantalla[ 134],0,255,0);tira.setPixelColor(Pantalla[ 135],0,255,0);tira.setPixelColor(Pantalla[ 136],0,255,0);tira.setPixelColor(Pantalla[ 137],0,255,0);tira.setPixelColor(Pantalla[ 138],0,0,255);tira.setPixelColor(Pantalla[ 141],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 142],0,0,0);tira.setPixelColor(Pantalla[ 143],0,0,0);
    tira.setPixelColor(Pantalla[ 159],0,0,0);tira.setPixelColor(Pantalla[ 158],0,0,0);tira.setPixelColor(Pantalla[ 154],0,0,255);tira.setPixelColor(Pantalla[ 153],0,255,0);tira.setPixelColor(Pantalla[ 152],0,255,0);tira.setPixelColor(Pantalla[ 151],0,255,0);tira.setPixelColor(Pantalla[ 150],0,255,0);tira.setPixelColor(Pantalla[ 149],0,0,255);tira.setPixelColor(Pantalla[ 148],0,0,0);tira.setPixelColor(Pantalla[ 147],0,0,0);tira.setPixelColor(Pantalla[ 144],0,255,0);
    tira.setPixelColor(Pantalla[ 160],0,255,0);tira.setPixelColor(Pantalla[ 162],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 165],0,0,255);tira.setPixelColor(Pantalla[ 166],0,255,0);tira.setPixelColor(Pantalla[ 167],0,255,0);tira.setPixelColor(Pantalla[ 168],0,255,0);tira.setPixelColor(Pantalla[ 169],0,255,0);tira.setPixelColor(Pantalla[ 170],0,0,255);tira.setPixelColor(Pantalla[ 175],0,255,0);
    tira.setPixelColor(Pantalla[ 191],0,255,0);tira.setPixelColor(Pantalla[ 190],0,0,0);tira.setPixelColor(Pantalla[ 186],0,0,255);tira.setPixelColor(Pantalla[ 185],0,255,0);tira.setPixelColor(Pantalla[ 184],0,255,0);tira.setPixelColor(Pantalla[ 183],0,255,0);tira.setPixelColor(Pantalla[ 182],0,255,0);tira.setPixelColor(Pantalla[ 181],0,0,255);tira.setPixelColor(Pantalla[ 180],0,0,0);tira.setPixelColor(Pantalla[ 178],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 177],0,0,0);tira.setPixelColor(Pantalla[ 176],0,255,0);
    tira.setPixelColor(Pantalla[ 192],0,255,0);tira.setPixelColor(Pantalla[ 197],0,0,255);tira.setPixelColor(Pantalla[ 198],0,0,255);tira.setPixelColor(Pantalla[ 199],0,0,255);tira.setPixelColor(Pantalla[ 200],0,0,255);tira.setPixelColor(Pantalla[ 201],0,0,255);tira.setPixelColor(Pantalla[ 202],0,0,255);tira.setPixelColor(Pantalla[ 203],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 207],0,0,0);
    tira.setPixelColor(Pantalla[ 223],0,0,0);tira.setPixelColor(Pantalla[ 222],0,255,0);tira.setPixelColor(Pantalla[ 221],0,255,0);tira.setPixelColor(Pantalla[ 220],0,255,0);tira.setPixelColor(Pantalla[ 219],0,0,0);tira.setPixelColor(Pantalla[ 218],0,0,0);tira.setPixelColor(Pantalla[ 217],0,0,0);tira.setPixelColor(Pantalla[ 216],0,255,0);tira.setPixelColor(Pantalla[ 215],0,255,0);tira.setPixelColor(Pantalla[ 214],0,255,0);tira.setPixelColor(Pantalla[ 213],0,0,0);tira.setPixelColor(Pantalla[ 212],0,0,0);tira.setPixelColor(Pantalla[ 211],0,0,0);tira.setPixelColor(Pantalla[ 210],0,255,0);tira.setPixelColor(Pantalla[ 209],0,255,0);tira.setPixelColor(Pantalla[ 208],0,255,0);
    

  }
  else
  {
    tira.setPixelColor(Pantalla[ 0],0,0,0);tira.setPixelColor(Pantalla[ 1],0,0,0);tira.setPixelColor(Pantalla[ 2],0,0,0);tira.setPixelColor(Pantalla[ 3],0,255,0);tira.setPixelColor(Pantalla[ 4],0,255,0);tira.setPixelColor(Pantalla[ 5],0,255,0);tira.setPixelColor(Pantalla[ 6],0,0,0);tira.setPixelColor(Pantalla[ 7],0,0,0);tira.setPixelColor(Pantalla[ 8],0,0,0);tira.setPixelColor(Pantalla[ 9],0,255,0);tira.setPixelColor(Pantalla[ 10],0,255,0);tira.setPixelColor(Pantalla[ 11],0,255,0);tira.setPixelColor(Pantalla[ 12],0,0,0);tira.setPixelColor(Pantalla[ 13],0,0,0);tira.setPixelColor(Pantalla[ 14],0,0,0);tira.setPixelColor(Pantalla[ 15],0,255,0);
    tira.setPixelColor(Pantalla[ 31],0,255,0);tira.setPixelColor(Pantalla[ 30],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 25],255,0,0);tira.setPixelColor(Pantalla[ 24],255,0,0);tira.setPixelColor(Pantalla[ 23],255,0,0);tira.setPixelColor(Pantalla[ 22],255,0,0);tira.setPixelColor(Pantalla[ 20],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 19],0,0,0);tira.setPixelColor(Pantalla[ 16],0,255,0);
    tira.setPixelColor(Pantalla[ 32],0,255,0);tira.setPixelColor(Pantalla[ 34],0,0,0);tira.setPixelColor(Pantalla[ 35],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 38],255,0,0);tira.setPixelColor(Pantalla[ 39],255,0,0);tira.setPixelColor(Pantalla[ 40],255,0,0);tira.setPixelColor(Pantalla[ 41],255,0,0);tira.setPixelColor(Pantalla[ 47],0,255,0);
    tira.setPixelColor(Pantalla[ 63],0,255,0);tira.setPixelColor(Pantalla[ 56],105,210,30);tira.setPixelColor(Pantalla[ 55],105,210,30);tira.setPixelColor(Pantalla[ 52],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 50],0,0,0);tira.setPixelColor(Pantalla[ 49],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 48],0,0,0);
    tira.setPixelColor(Pantalla[ 64],0,0,0);tira.setPixelColor(Pantalla[ 71],0,0,255);tira.setPixelColor(Pantalla[ 72],0,0,255);tira.setPixelColor(Pantalla[ 75],0,0,0);tira.setPixelColor(Pantalla[ 79],0,0,0);
    tira.setPixelColor(Pantalla[ 95],0,0,0);tira.setPixelColor(Pantalla[ 93],0,0,0);tira.setPixelColor(Pantalla[ 89],0,0,255);tira.setPixelColor(Pantalla[ 88],100,100,100);tira.setPixelColor(Pantalla[ 87],100,100,100);tira.setPixelColor(Pantalla[ 86],0,0,255);tira.setPixelColor(Pantalla[ 82],0,0,0);tira.setPixelColor(Pantalla[ 80],0,0,0);
    tira.setPixelColor(Pantalla[ 96],0,0,0);tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 99],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 101],0,0,255);tira.setPixelColor(Pantalla[ 102],100,100,100);tira.setPixelColor(Pantalla[ 103],100,100,100);tira.setPixelColor(Pantalla[ 104],100,100,100);tira.setPixelColor(Pantalla[ 105],100,100,100);tira.setPixelColor(Pantalla[ 106],0,0,255);tira.setPixelColor(Pantalla[ 109],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 111],0,255,0);
    tira.setPixelColor(Pantalla[ 127],0,255,0);tira.setPixelColor(Pantalla[ 124],0,0,0);tira.setPixelColor(Pantalla[ 122],0,0,255);tira.setPixelColor(Pantalla[ 121],0,255,0);tira.setPixelColor(Pantalla[ 120],0,255,0);tira.setPixelColor(Pantalla[ 119],0,255,0);tira.setPixelColor(Pantalla[ 118],0,255,0);tira.setPixelColor(Pantalla[ 117],0,0,255);tira.setPixelColor(Pantalla[ 112],0,255,0);
    tira.setPixelColor(Pantalla[ 128],0,255,0);tira.setPixelColor(Pantalla[ 129],0,0,0);tira.setPixelColor(Pantalla[ 133],0,0,255);tira.setPixelColor(Pantalla[ 134],0,255,0);tira.setPixelColor(Pantalla[ 135],0,255,0);tira.setPixelColor(Pantalla[ 136],0,255,0);tira.setPixelColor(Pantalla[ 137],0,255,0);tira.setPixelColor(Pantalla[ 138],0,0,255);tira.setPixelColor(Pantalla[ 141],0,0,0);tira.setPixelColor(Pantalla[ 142],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 143],0,255,0);
    tira.setPixelColor(Pantalla[ 159],0,255,0);tira.setPixelColor(Pantalla[ 158],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 154],0,0,255);tira.setPixelColor(Pantalla[ 153],0,255,0);tira.setPixelColor(Pantalla[ 152],0,255,0);tira.setPixelColor(Pantalla[ 151],0,255,0);tira.setPixelColor(Pantalla[ 150],0,255,0);tira.setPixelColor(Pantalla[ 149],0,0,255);tira.setPixelColor(Pantalla[ 148],0,0,0);tira.setPixelColor(Pantalla[ 147],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 144],0,0,0);
    tira.setPixelColor(Pantalla[ 160],0,0,0);tira.setPixelColor(Pantalla[ 162],0,0,0);tira.setPixelColor(Pantalla[ 165],0,0,255);tira.setPixelColor(Pantalla[ 166],0,255,0);tira.setPixelColor(Pantalla[ 167],0,255,0);tira.setPixelColor(Pantalla[ 168],0,255,0);tira.setPixelColor(Pantalla[ 169],0,255,0);tira.setPixelColor(Pantalla[ 170],0,0,255);tira.setPixelColor(Pantalla[ 175],0,0,0);
    tira.setPixelColor(Pantalla[ 191],0,0,0);tira.setPixelColor(Pantalla[ 190],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 186],0,0,255);tira.setPixelColor(Pantalla[ 185],0,255,0);tira.setPixelColor(Pantalla[ 184],0,255,0);tira.setPixelColor(Pantalla[ 183],0,255,0);tira.setPixelColor(Pantalla[ 182],0,255,0);tira.setPixelColor(Pantalla[ 181],0,0,255);tira.setPixelColor(Pantalla[ 180],0,0,0);tira.setPixelColor(Pantalla[ 178],0,0,0);tira.setPixelColor(Pantalla[ 177],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 176],0,0,0);
    tira.setPixelColor(Pantalla[ 192],0,0,0);tira.setPixelColor(Pantalla[ 197],0,0,255);tira.setPixelColor(Pantalla[ 198],0,255,0);tira.setPixelColor(Pantalla[ 199],0,255,0);tira.setPixelColor(Pantalla[ 200],0,255,0);tira.setPixelColor(Pantalla[ 201],0,255,0);tira.setPixelColor(Pantalla[ 202],0,0,255);tira.setPixelColor(Pantalla[ 203],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 207],0,255,0);
    tira.setPixelColor(Pantalla[ 223],0,255,0);tira.setPixelColor(Pantalla[ 222],0,0,0);tira.setPixelColor(Pantalla[ 221],0,0,0);tira.setPixelColor(Pantalla[ 220],0,0,0);tira.setPixelColor(Pantalla[ 219],0,255,0);tira.setPixelColor(Pantalla[ 218],0,255,0);tira.setPixelColor(Pantalla[ 217],0,255,0);tira.setPixelColor(Pantalla[ 216],0,0,0);tira.setPixelColor(Pantalla[ 215],0,0,0);tira.setPixelColor(Pantalla[ 214],0,0,0);tira.setPixelColor(Pantalla[ 213],0,255,0);tira.setPixelColor(Pantalla[ 212],0,255,0);tira.setPixelColor(Pantalla[ 211],0,255,0);tira.setPixelColor(Pantalla[ 210],0,0,0);tira.setPixelColor(Pantalla[ 209],0,0,0);tira.setPixelColor(Pantalla[ 208],0,0,0);
    

  }
}

void Matriz_Copa()
{

if (lado==IZQUIERDA)
{
  
  tira.setPixelColor(Pantalla[ 0],0,0,0);tira.setPixelColor(Pantalla[ 1],0,0,0);tira.setPixelColor(Pantalla[ 2],0,0,0);tira.setPixelColor(Pantalla[ 3],0,255,0);tira.setPixelColor(Pantalla[ 4],0,255,0);tira.setPixelColor(Pantalla[ 5],0,255,0);tira.setPixelColor(Pantalla[ 6],0,0,0);tira.setPixelColor(Pantalla[ 7],0,0,0);tira.setPixelColor(Pantalla[ 8],0,0,0);tira.setPixelColor(Pantalla[ 9],0,255,0);tira.setPixelColor(Pantalla[ 10],0,255,0);tira.setPixelColor(Pantalla[ 11],0,255,0);tira.setPixelColor(Pantalla[ 12],0,0,0);tira.setPixelColor(Pantalla[ 13],0,0,0);tira.setPixelColor(Pantalla[ 14],0,0,0);tira.setPixelColor(Pantalla[ 15],0,255,0);
  tira.setPixelColor(Pantalla[ 31],0,255,0);tira.setPixelColor(Pantalla[ 30],0,0,0);tira.setPixelColor(Pantalla[ 20],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 19],0,0,0);tira.setPixelColor(Pantalla[ 16],0,255,0);
  tira.setPixelColor(Pantalla[ 32],0,255,0);tira.setPixelColor(Pantalla[ 34],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 35],0,0,0);tira.setPixelColor(Pantalla[ 37],0,0,255);tira.setPixelColor(Pantalla[ 38],100,100,100);tira.setPixelColor(Pantalla[ 39],100,100,100);tira.setPixelColor(Pantalla[ 40],100,100,100);tira.setPixelColor(Pantalla[ 41],100,100,100);tira.setPixelColor(Pantalla[ 42],0,0,255);tira.setPixelColor(Pantalla[ 47],0,255,0);
  tira.setPixelColor(Pantalla[ 63],0,255,0);tira.setPixelColor(Pantalla[ 58],0,0,255);tira.setPixelColor(Pantalla[ 57],100,100,100);tira.setPixelColor(Pantalla[ 56],100,100,100);tira.setPixelColor(Pantalla[ 55],100,100,100);tira.setPixelColor(Pantalla[ 54],100,100,100);tira.setPixelColor(Pantalla[ 53],0,0,255);tira.setPixelColor(Pantalla[ 52],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 50],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 49],0,0,0);tira.setPixelColor(Pantalla[ 48],0,0,0);
  tira.setPixelColor(Pantalla[ 64],0,0,0);tira.setPixelColor(Pantalla[ 69],0,0,255);tira.setPixelColor(Pantalla[ 70],100,100,100);tira.setPixelColor(Pantalla[ 71],100,100,100);tira.setPixelColor(Pantalla[ 72],100,100,100);tira.setPixelColor(Pantalla[ 73],100,100,100);tira.setPixelColor(Pantalla[ 74],0,0,255);tira.setPixelColor(Pantalla[ 75],0,0,0);tira.setPixelColor(Pantalla[ 79],0,0,0);
  tira.setPixelColor(Pantalla[ 95],0,0,0);tira.setPixelColor(Pantalla[ 93],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 90],0,0,255);tira.setPixelColor(Pantalla[ 89],0,255,0);tira.setPixelColor(Pantalla[ 88],0,255,0);tira.setPixelColor(Pantalla[ 87],0,255,0);tira.setPixelColor(Pantalla[ 86],0,255,0);tira.setPixelColor(Pantalla[ 85],0,0,255);tira.setPixelColor(Pantalla[ 82],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 80],0,0,0);
  tira.setPixelColor(Pantalla[ 96],0,0,0);tira.setPixelColor(Pantalla[ 98],0,0,0);tira.setPixelColor(Pantalla[ 99],0,0,0);tira.setPixelColor(Pantalla[ 101],0,0,255);tira.setPixelColor(Pantalla[ 102],0,255,0);tira.setPixelColor(Pantalla[ 103],0,255,0);tira.setPixelColor(Pantalla[ 104],0,255,0);tira.setPixelColor(Pantalla[ 105],0,255,0);tira.setPixelColor(Pantalla[ 106],0,0,255);tira.setPixelColor(Pantalla[ 109],0,0,0);tira.setPixelColor(Pantalla[ 111],0,255,0);
  tira.setPixelColor(Pantalla[ 127],0,255,0);tira.setPixelColor(Pantalla[ 124],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 122],0,0,255);tira.setPixelColor(Pantalla[ 121],0,255,0);tira.setPixelColor(Pantalla[ 120],0,255,0);tira.setPixelColor(Pantalla[ 119],0,255,0);tira.setPixelColor(Pantalla[ 118],0,255,0);tira.setPixelColor(Pantalla[ 117],0,0,255);tira.setPixelColor(Pantalla[ 112],0,255,0);
  tira.setPixelColor(Pantalla[ 128],0,255,0);tira.setPixelColor(Pantalla[ 129],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 133],0,0,255);tira.setPixelColor(Pantalla[ 134],0,255,0);tira.setPixelColor(Pantalla[ 135],0,255,0);tira.setPixelColor(Pantalla[ 136],0,255,0);tira.setPixelColor(Pantalla[ 137],0,255,0);tira.setPixelColor(Pantalla[ 138],0,0,255);tira.setPixelColor(Pantalla[ 141],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 142],0,0,0);tira.setPixelColor(Pantalla[ 143],0,255,0);
  tira.setPixelColor(Pantalla[ 159],0,255,0);tira.setPixelColor(Pantalla[ 158],0,0,0);tira.setPixelColor(Pantalla[ 153],0,0,255);tira.setPixelColor(Pantalla[ 152],0,0,255);tira.setPixelColor(Pantalla[ 151],0,0,255);tira.setPixelColor(Pantalla[ 150],0,0,255);tira.setPixelColor(Pantalla[ 148],0,0,0);tira.setPixelColor(Pantalla[ 147],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 144],0,0,0);
  tira.setPixelColor(Pantalla[ 160],0,0,0);tira.setPixelColor(Pantalla[ 162],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 167],0,0,255);tira.setPixelColor(Pantalla[ 168],0,0,255);tira.setPixelColor(Pantalla[ 175],0,0,0);
  tira.setPixelColor(Pantalla[ 191],0,0,0);tira.setPixelColor(Pantalla[ 190],0,0,0);tira.setPixelColor(Pantalla[ 184],0,0,255);tira.setPixelColor(Pantalla[ 183],0,0,255);tira.setPixelColor(Pantalla[ 180],0,0,0);tira.setPixelColor(Pantalla[ 178],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 177],0,0,0);tira.setPixelColor(Pantalla[ 176],0,0,0);
  tira.setPixelColor(Pantalla[ 192],0,0,0);tira.setPixelColor(Pantalla[ 199],0,0,255);tira.setPixelColor(Pantalla[ 200],0,0,255);tira.setPixelColor(Pantalla[ 203],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 207],0,255,0);
  tira.setPixelColor(Pantalla[ 223],0,255,0);tira.setPixelColor(Pantalla[ 222],0,0,0);tira.setPixelColor(Pantalla[ 221],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 218],0,0,255);tira.setPixelColor(Pantalla[ 217],0,0,255);tira.setPixelColor(Pantalla[ 216],0,0,255);tira.setPixelColor(Pantalla[ 215],0,0,255);tira.setPixelColor(Pantalla[ 214],0,0,255);tira.setPixelColor(Pantalla[ 213],0,0,255);tira.setPixelColor(Pantalla[ 210],0,0,0);tira.setPixelColor(Pantalla[ 209],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 208],0,255,0);
  tira.setPixelColor(Pantalla[ 224],0,255,0);tira.setPixelColor(Pantalla[ 239],0,255,0);
  tira.setPixelColor(Pantalla[ 255],0,255,0);tira.setPixelColor(Pantalla[ 254],0,0,0);tira.setPixelColor(Pantalla[ 253],0,0,0);tira.setPixelColor(Pantalla[ 252],0,0,0);tira.setPixelColor(Pantalla[ 251],0,255,0);tira.setPixelColor(Pantalla[ 250],0,255,0);tira.setPixelColor(Pantalla[ 249],0,255,0);tira.setPixelColor(Pantalla[ 248],0,0,0);tira.setPixelColor(Pantalla[ 247],0,0,0);tira.setPixelColor(Pantalla[ 246],0,0,0);tira.setPixelColor(Pantalla[ 245],0,255,0);tira.setPixelColor(Pantalla[ 244],0,255,0);tira.setPixelColor(Pantalla[ 243],0,255,0);tira.setPixelColor(Pantalla[ 242],0,0,0);tira.setPixelColor(Pantalla[ 241],0,0,0);tira.setPixelColor(Pantalla[ 240],0,0,0);
  

}
else
{
  tira.setPixelColor(Pantalla[ 0],0,255,0);tira.setPixelColor(Pantalla[ 1],0,255,0);tira.setPixelColor(Pantalla[ 2],0,255,0);tira.setPixelColor(Pantalla[ 3],0,0,0);tira.setPixelColor(Pantalla[ 4],0,0,0);tira.setPixelColor(Pantalla[ 5],0,0,0);tira.setPixelColor(Pantalla[ 6],0,255,0);tira.setPixelColor(Pantalla[ 7],0,255,0);tira.setPixelColor(Pantalla[ 8],0,255,0);tira.setPixelColor(Pantalla[ 9],0,0,0);tira.setPixelColor(Pantalla[ 10],0,0,0);tira.setPixelColor(Pantalla[ 11],0,0,0);tira.setPixelColor(Pantalla[ 12],0,255,0);tira.setPixelColor(Pantalla[ 13],0,255,0);tira.setPixelColor(Pantalla[ 14],0,255,0);tira.setPixelColor(Pantalla[ 15],0,0,0);
  tira.setPixelColor(Pantalla[ 31],0,255,0);tira.setPixelColor(Pantalla[ 30],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 20],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 19],0,0,0);tira.setPixelColor(Pantalla[ 16],0,0,0);
  tira.setPixelColor(Pantalla[ 34],0,0,0);tira.setPixelColor(Pantalla[ 35],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 37],0,0,255);tira.setPixelColor(Pantalla[ 38],100,100,100);tira.setPixelColor(Pantalla[ 39],100,100,100);tira.setPixelColor(Pantalla[ 40],100,100,100);tira.setPixelColor(Pantalla[ 41],100,100,100);tira.setPixelColor(Pantalla[ 42],0,0,255);tira.setPixelColor(Pantalla[ 47],0,0,0);
  tira.setPixelColor(Pantalla[ 63],0,0,0);tira.setPixelColor(Pantalla[ 58],0,0,255);tira.setPixelColor(Pantalla[ 57],100,100,100);tira.setPixelColor(Pantalla[ 56],100,100,100);tira.setPixelColor(Pantalla[ 55],100,100,100);tira.setPixelColor(Pantalla[ 54],100,100,100);tira.setPixelColor(Pantalla[ 53],0,0,255);tira.setPixelColor(Pantalla[ 52],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 50],0,0,0);tira.setPixelColor(Pantalla[ 49],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 48],0,255,0);
  tira.setPixelColor(Pantalla[ 64],0,0,0);tira.setPixelColor(Pantalla[ 69],0,0,255);tira.setPixelColor(Pantalla[ 70],100,100,100);tira.setPixelColor(Pantalla[ 71],100,100,100);tira.setPixelColor(Pantalla[ 72],100,100,100);tira.setPixelColor(Pantalla[ 73],100,100,100);tira.setPixelColor(Pantalla[ 74],0,0,255);tira.setPixelColor(Pantalla[ 75],0,0,0);tira.setPixelColor(Pantalla[ 79],0,255,0);
  tira.setPixelColor(Pantalla[ 95],0,0,0);tira.setPixelColor(Pantalla[ 93],0,0,0);tira.setPixelColor(Pantalla[ 90],0,0,255);tira.setPixelColor(Pantalla[ 89],0,255,0);tira.setPixelColor(Pantalla[ 88],0,255,0);tira.setPixelColor(Pantalla[ 87],0,255,0);tira.setPixelColor(Pantalla[ 86],0,255,0);tira.setPixelColor(Pantalla[ 85],0,0,255);tira.setPixelColor(Pantalla[ 82],0,0,0);tira.setPixelColor(Pantalla[ 80],0,255,0);
  tira.setPixelColor(Pantalla[ 96],0,255,0);tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 99],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 101],0,0,255);tira.setPixelColor(Pantalla[ 102],0,255,0);tira.setPixelColor(Pantalla[ 103],0,255,0);tira.setPixelColor(Pantalla[ 104],0,255,0);tira.setPixelColor(Pantalla[ 105],0,255,0);tira.setPixelColor(Pantalla[ 106],0,0,255);tira.setPixelColor(Pantalla[ 109],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 111],0,0,0);
  tira.setPixelColor(Pantalla[ 127],0,255,0);tira.setPixelColor(Pantalla[ 124],0,0,0);tira.setPixelColor(Pantalla[ 122],0,0,255);tira.setPixelColor(Pantalla[ 121],0,255,0);tira.setPixelColor(Pantalla[ 120],0,255,0);tira.setPixelColor(Pantalla[ 119],0,255,0);tira.setPixelColor(Pantalla[ 118],0,255,0);tira.setPixelColor(Pantalla[ 117],0,0,255);tira.setPixelColor(Pantalla[ 112],0,0,0);
  tira.setPixelColor(Pantalla[ 128],0,255,0);tira.setPixelColor(Pantalla[ 129],0,0,0);tira.setPixelColor(Pantalla[ 133],0,0,255);tira.setPixelColor(Pantalla[ 134],0,255,0);tira.setPixelColor(Pantalla[ 135],0,255,0);tira.setPixelColor(Pantalla[ 136],0,255,0);tira.setPixelColor(Pantalla[ 137],0,255,0);tira.setPixelColor(Pantalla[ 138],0,0,255);tira.setPixelColor(Pantalla[ 141],0,0,0);tira.setPixelColor(Pantalla[ 142],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 143],0,0,0);
  tira.setPixelColor(Pantalla[ 159],0,0,0);tira.setPixelColor(Pantalla[ 158],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 153],0,0,255);tira.setPixelColor(Pantalla[ 152],0,0,255);tira.setPixelColor(Pantalla[ 151],0,0,255);tira.setPixelColor(Pantalla[ 150],0,0,255);tira.setPixelColor(Pantalla[ 148],0,0,0);tira.setPixelColor(Pantalla[ 147],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 144],0,255,0);
  tira.setPixelColor(Pantalla[ 160],0,0,0);tira.setPixelColor(Pantalla[ 162],0,0,0);tira.setPixelColor(Pantalla[ 167],0,0,255);tira.setPixelColor(Pantalla[ 168],0,0,255);tira.setPixelColor(Pantalla[ 175],0,255,0);
  tira.setPixelColor(Pantalla[ 191],0,0,0);tira.setPixelColor(Pantalla[ 190],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 184],0,0,255);tira.setPixelColor(Pantalla[ 183],0,0,255);tira.setPixelColor(Pantalla[ 180],0,0,0);tira.setPixelColor(Pantalla[ 178],0,0,0);tira.setPixelColor(Pantalla[ 177],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 176],0,255,0);
  tira.setPixelColor(Pantalla[ 192],0,255,0);tira.setPixelColor(Pantalla[ 197],0,0,255);tira.setPixelColor(Pantalla[ 198],0,0,255);tira.setPixelColor(Pantalla[ 199],0,0,255);tira.setPixelColor(Pantalla[ 200],0,0,255);tira.setPixelColor(Pantalla[ 201],0,0,255);tira.setPixelColor(Pantalla[ 202],0,0,255);tira.setPixelColor(Pantalla[ 203],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 207],0,0,0);
  tira.setPixelColor(Pantalla[ 223],0,255,0);tira.setPixelColor(Pantalla[ 222],0,255,0);tira.setPixelColor(Pantalla[ 221],0,0,0);tira.setPixelColor(Pantalla[ 220],0,0,0);tira.setPixelColor(Pantalla[ 219],0,0,0);tira.setPixelColor(Pantalla[ 218],0,255,0);tira.setPixelColor(Pantalla[ 217],0,255,0);tira.setPixelColor(Pantalla[ 216],0,255,0);tira.setPixelColor(Pantalla[ 215],0,0,0);tira.setPixelColor(Pantalla[ 214],0,0,0);tira.setPixelColor(Pantalla[ 213],0,0,0);tira.setPixelColor(Pantalla[ 212],0,255,0);tira.setPixelColor(Pantalla[ 211],0,255,0);tira.setPixelColor(Pantalla[ 210],0,255,0);tira.setPixelColor(Pantalla[ 209],0,0,0);tira.setPixelColor(Pantalla[ 208],0,0,0);
  

}


}

void Matriz_Sin_Castigo()
{
  tira.setPixelColor(Pantalla[ 61],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 60],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 59],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 58],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 55],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 52],(10+rand()%245),(10+rand()%245),(10+rand()%245));
  tira.setPixelColor(Pantalla[ 66],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 69],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 72],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 75],(10+rand()%245),(10+rand()%245),(10+rand()%245));
  tira.setPixelColor(Pantalla[ 93],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 90],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 87],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 85],(10+rand()%245),(10+rand()%245),(10+rand()%245));
  tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 101],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 104],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 106],(10+rand()%245),(10+rand()%245),(10+rand()%245));
  tira.setPixelColor(Pantalla[ 125],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 122],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 119],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 118],(10+rand()%245),(10+rand()%245),(10+rand()%245));
  tira.setPixelColor(Pantalla[ 130],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 133],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 136],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 138],(10+rand()%245),(10+rand()%245),(10+rand()%245));
  tira.setPixelColor(Pantalla[ 157],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 154],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 151],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 149],(10+rand()%245),(10+rand()%245),(10+rand()%245));
  tira.setPixelColor(Pantalla[ 162],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 165],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 168],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 171],(10+rand()%245),(10+rand()%245),(10+rand()%245));
  tira.setPixelColor(Pantalla[ 189],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 188],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 187],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 186],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 183],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 180],(10+rand()%245),(10+rand()%245),(10+rand()%245));
  
}


// SIN DETECCION
void Aro_Sin_Deteccion()
{
  //La logica de color se envia desde el monitor mediante el datos_recibidos.c
  Asigna_Color_Basandose_En_Color_Recibido_De_Monitor();
  switch (datos_recibidos.pp)
    {
      case PRIVADO:
          /*code*/
          //solo prende el aro si corresponde al color del tirador esperado
          if (datos_recibidos.c==color_de_disparo)  
              {
                Aro_Sin_Deteccion_Secuencia(); 
              }
          break;
      
      case PUBLICO:
          /*code*/
          // prende el apuntar sin importar el color de la pistola que este apuntando
          Aro_Sin_Deteccion_Secuencia();  
          break;
    }
}




void Aro_Sin_Deteccion_Secuencia()
{
  switch (Posicion_Aro)
  { 
    //rick
    case UNO:
        Matriz_Limpia_Secuencias();
        Matriz_Aro_1();
        Posicion_Aro=DOS;
        break;
    case DOS:
        Matriz_Aro_2();
        Posicion_Aro=TRES;
        break;
    case TRES:
        Matriz_Aro_3();
        Posicion_Aro=CUATRO;
        break;
    case CUATRO:
        Matriz_Aro_4();
        Posicion_Aro=CINCO;
        break;
    case CINCO:
        Matriz_Aro_5();
        Posicion_Aro=SEIS;
        break;
    case SEIS:
        Matriz_Aro_6();
        Posicion_Aro=SIETE;
        break;
    case SIETE:
        Matriz_Aro_7();
        Posicion_Aro=UNO;
        break;

  }
}


/*--------------------------------------------------*/
void Asigna_Color_Aro_Global()
{
    //define color del aro
    if (datos_recibidos.vt==CASTIGO)
      {
        //ROJO
        //Serial.println("Programa color rojo para castigos");
        pixel_rojo=0;
        pixel_verde=250;
        pixel_azul=0;
      }
    else 
    {
        /*code*/
        //AMARILLO
        pixel_rojo=250;
        pixel_verde=250;
        pixel_azul=0;
    }
}

/*--------------------------------------------------*/
void Asigna_Color_Basandose_En_Color_Recibido_De_Monitor()
{
    //define color del aro
    switch (datos_recibidos.c)
    {
      case GREEN:
        /*code*/
        pixel_rojo=250;
        pixel_verde=0;
        pixel_azul=0;
        break;

        case RED:
        /*code*/
        //Serial.println("Programa color rojo para castigos");
        pixel_rojo=0;
        pixel_verde=250;
        pixel_azul=0;
        break; 

        case BLUE:
        /*code*/
        //Serial.println("Programa color azul");
        pixel_rojo=0;
        pixel_verde=0;
        pixel_azul=250;
        break;

        case ARCOIRIS:
        /*code*/
        pixel_rojo = int(rand()%255);
        pixel_verde = int(rand()%255);
        pixel_azul = int(rand()%255);
        break;
    }
}

void Asigna_Color_Aro_Basandose_En_Tiro_Pistola()
{
    //define color con base en el color de la pistola detectada
    switch (color_de_disparo)
    {//rick
      case GREEN:
        /*code*/
        pixel_rojo=250;
        pixel_verde=0;
        pixel_azul=0;
        break;

      case BLUE:
        /*code*/
        //Serial.println("Programa color azul");
        pixel_rojo=0;
        pixel_verde=0;
        pixel_azul=250;
        break;
    }
}

/*---------------------------------*/
// DISPARO
void Aro_Disparo()
{
  switch (datos_recibidos.pp)
    {
      case PRIVADO:
        if (datos_recibidos.c==color_de_disparo)  //solo prende el aro si corresponde el color
          {
            tiempo_termina_tiro=millis();
            Aro_Disparo_Secuencia();
            posicion_nueva==DISPARO_DETECTADO;
          }
        break;

      case PUBLICO:
          tiempo_termina_tiro=millis();
          Aro_Apuntando_Secuencia();  // prende el apuntar sin importar el color que apunte
          posicion_nueva==DISPARO_DETECTADO;   
          break;       
    }

}


/*----------------------------------*/

void  Arma_Paquete_De_Envio()
{
  //LLENADO DE PAQUETE A ENVIAR
  //Control
  datos_enviados.n=datos_recibidos.n;
  //Propiedades de juego
  datos_enviados.ju=datos_recibidos.ju; //[Monitor]1 CLASICO-WESTERN / 2 TORNEO /3 PAREJAS/4 VELOCIDAD
  datos_enviados.jr=datos_recibidos.jr; // [Monitor]No de Round para definicion de velocidad, no aplica en diana
  //Propiedades del Paquete
  datos_enviados.t= datos_recibidos.t; //[Monitor]TEST=0, TIRO_ACTIVO=1, JUGADOR_READY=2 es es el recibido en el mensaje de llegada
  datos_enviados.d=datos_recibidos.d;  //[MOnitor]No de Diana
  datos_enviados.c=datos_recibidos.c;  //[Monitor]Color del tiro enviado a DIana
  datos_enviados.p=datos_recibidos.p;  //[Monitor]Propietario Original del tiro
  
  //Propiedades del tiro
  datos_enviados.s=datos_recibidos.s;  //[Monitor]Numero de tiro asignado
  //datos_enviados.tiempo=(tiempo_termina_tiro-tiempo_inicia_tiro)/1000.0; // para regresar el tiempo en segundos

  //Condiciones de manejo del tiro
  datos_enviados.pp=datos_recibidos.pp; //[Monitor] PRIVADO O PUBLICO el tipo de tiro
  datos_enviados.pa=datos_recibidos.pa; //[Monitor] Propietario Alternativo del tiro en caso PUBLICO
  //Propiedades de la figura
  datos_enviados.vf=datos_recibidos.vf; //[Monitor] Velocidad de cambio de figura, establece Monitor para cada tiro
  datos_enviados.tf=datos_recibidos.tf;  //[Monitor]SIN_DEFINIR=0, NORMAL=1, BONO=2, CASTIGO=3
  datos_enviados.vt=datos_recibidos.vt; //[monitor] Cuanto vale el tiro NORMAL= 1 tiro, BONO =1 tiro, CASTIGO=0
  datos_enviados.xf=datos_recibidos.xf; //[Monitor] Figura a iniciar en DIana,solo para BOnos 
                                        //ya que cstigo es definido y normal siempre empieza con el mayor
  //Resultados del disparo
  // Analisis del propieatrio de los puntos generados por el disparo
      if (datos_recibidos.pp==PRIVADO)
        {
          datos_enviados.jg=datos_recibidos.p; //[Diana] siempre se asigna al propietario del tiro

        }

      if (datos_recibidos.pp==PUBLICO)
        {
          if (color_de_disparo==datos_recibidos.c) //[Diana]color de jugador original
            {
              datos_enviados.jg=datos_recibidos.p;            
            }
          else
            {
              datos_enviados.jg=datos_recibidos.pa;            
            }
        }
  datos_enviados.po=valor_puntuacion_figura;   //[Diana]puntuacion de la figura impactada
  datos_enviados.fi=figura_actual;   //[Diana]figura impactada en tiro 
  
}


void Asigna_Disparo_Por_Color_De_Pistola()
{
  /* 1) Se selecciona el jugador del disparo
        si es el original de adigna No.jugaror
        si es el otro datos_enviados.jg=datos_recibidos.pa // jugador alternativo
     2) Se le asigna el puntaje del disparo ya sea normal, bono o castigo
     3) Se deja el numero de tiro al propietario original
     4) Se asigna al jugador original como datos_enviados.pg (jugador ganador deltiro)
  */
}


// APUNTADO
void Aro_Apuntando()
{
  switch (datos_recibidos.pp)
    {
      case PRIVADO:
        /*code*/
        //solo prende el aro si corresponde el color
        if (datos_recibidos.c==color_de_disparo)  
          {
            Asigna_Color_Basandose_En_Color_Recibido_De_Monitor();
            Aro_Apuntando_Secuencia(); 
          }        
        break;
      
        case PUBLICO:
        /*code*/
        // prende el apuntar verde o azul con base a la frecuencia dela pistola que detecto-->verde o azul   
        Asigna_Color_Aro_Basandose_En_Tiro_Pistola();
        Aro_Apuntando_Secuencia();       
        break;
    }

}

void Aro_Apuntando_Secuencia()
{
  switch (Posicion_Aro)
  { //rick
    case UNO:
        Matriz_Colorea_Apuntando();
        Matriz_Aro_1_Apuntado();
        Posicion_Aro=DOS;
        break;
    case DOS:
        Matriz_Colorea_Apuntando();
        Matriz_Aro_2_Apuntado();
        Posicion_Aro=TRES;
        break;
    case TRES:
        Matriz_Colorea_Apuntando();
        Matriz_Aro_3_Apuntado();
        Posicion_Aro=CUATRO;
        break;
    case CUATRO:
        Matriz_Colorea_Apuntando();
        Matriz_Aro_4_Apuntado();
        Posicion_Aro=CINCO;
        break;
    case CINCO:
        Matriz_Colorea_Apuntando();
        Matriz_Aro_5_Apuntado();
        Posicion_Aro=SEIS;
        break;
    case SEIS:
        Matriz_Colorea_Apuntando();
        Matriz_Aro_6_Apuntado();
        Posicion_Aro=SIETE;
        break;
    case SIETE:
        Matriz_Colorea_Apuntando();
        Matriz_Aro_7_Apuntado();
        Posicion_Aro=UNO;
        break;
  }
}

void Aro_Disparo_Secuencia()
{
        Matriz_Aro_1_Disparo();
        tira.show();
        delay(60);
        Matriz_Aro_2_Disparo();
        tira.show();
        delay(60);
        Matriz_Aro_3_Disparo();
        tira.show();
        delay(60);
        Matriz_Aro_4_Disparo();
        tira.show();
        delay(60);
        Matriz_Aro_5_Disparo();
        tira.show();
        delay(60); 
        Matriz_Aro_6_Disparo();
        tira.show();
        delay(60);
        Matriz_Aro_7_Disparo();
        tira.show();
        delay(120);
}






void Matriz_Aro_1()
{
  tira.setPixelColor(Pantalla[ 224],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 231],100,100,100);tira.setPixelColor(Pantalla[ 232],100,100,100);tira.setPixelColor(Pantalla[ 239],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 255],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 248],100,100,100);tira.setPixelColor(Pantalla[ 247],100,100,100);tira.setPixelColor(Pantalla[ 240],pixel_rojo,pixel_verde,pixel_azul);

}

void Matriz_Aro_2()
{
  tira.setPixelColor(Pantalla[ 225],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 231],100,100,100);tira.setPixelColor(Pantalla[ 232],100,100,100);tira.setPixelColor(Pantalla[ 238],pixel_rojo,pixel_verde,pixel_azul);
  tira.setPixelColor(Pantalla[ 254],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 248],100,100,100);tira.setPixelColor(Pantalla[ 247],100,100,100);tira.setPixelColor(Pantalla[ 241],pixel_rojo,pixel_verde,pixel_azul);
  

}

void Matriz_Aro_3()
{
  tira.setPixelColor(Pantalla[ 226],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 231],255,255,0);tira.setPixelColor(Pantalla[ 232],255,255,0);tira.setPixelColor(Pantalla[ 237],pixel_rojo,pixel_verde,pixel_azul);
  tira.setPixelColor(Pantalla[ 253],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 248],255,255,0);tira.setPixelColor(Pantalla[ 247],255,255,0);tira.setPixelColor(Pantalla[ 242],pixel_rojo,pixel_verde,pixel_azul);
  

}

void Matriz_Aro_4()
{
  tira.setPixelColor(Pantalla[ 227],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 231],255,255,0);tira.setPixelColor(Pantalla[ 232],255,255,0);tira.setPixelColor(Pantalla[ 236],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 252],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 248],255,255,0);tira.setPixelColor(Pantalla[ 247],255,255,0);tira.setPixelColor(Pantalla[ 243],pixel_rojo,pixel_verde,pixel_azul);

}

void Matriz_Aro_5()
{

  tira.setPixelColor(Pantalla[ 228],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 231],100,100,100);tira.setPixelColor(Pantalla[ 232],100,100,100);tira.setPixelColor(Pantalla[ 235],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 251],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 248],100,100,100);tira.setPixelColor(Pantalla[ 247],100,100,100);tira.setPixelColor(Pantalla[ 244],pixel_rojo,pixel_verde,pixel_azul);

}

void Matriz_Aro_6()
{
  tira.setPixelColor(Pantalla[ 229],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 231],100,100,100);tira.setPixelColor(Pantalla[ 232],100,100,100);tira.setPixelColor(Pantalla[ 234],pixel_rojo,pixel_verde,pixel_azul);
  tira.setPixelColor(Pantalla[ 250],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 248],100,100,100);tira.setPixelColor(Pantalla[ 247],100,100,100);tira.setPixelColor(Pantalla[ 245],pixel_rojo,pixel_verde,pixel_azul);
  

}

void Matriz_Aro_7()
{
tira.setPixelColor(Pantalla[ 217],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 216],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 215],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 214],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 230],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 231],255,255,0);tira.setPixelColor(Pantalla[ 232],255,255,0);tira.setPixelColor(Pantalla[ 233],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 249],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 248],255,255,0);tira.setPixelColor(Pantalla[ 247],255,255,0);tira.setPixelColor(Pantalla[ 246],pixel_rojo,pixel_verde,pixel_azul);

}

void Matriz_Limpia_Secuencias()
{
  tira.setPixelColor(Pantalla[ 224],0,0,0);tira.setPixelColor(Pantalla[ 225],0,0,0);tira.setPixelColor(Pantalla[ 226],0,0,0);tira.setPixelColor(Pantalla[ 227],0,0,0);tira.setPixelColor(Pantalla[ 228],0,0,0);tira.setPixelColor(Pantalla[ 229],0,0,0);tira.setPixelColor(Pantalla[ 231],255,255,0);tira.setPixelColor(Pantalla[ 232],255,255,0);tira.setPixelColor(Pantalla[ 234],0,0,0);tira.setPixelColor(Pantalla[ 235],0,0,0);tira.setPixelColor(Pantalla[ 236],0,0,0);tira.setPixelColor(Pantalla[ 237],0,0,0);tira.setPixelColor(Pantalla[ 238],0,0,0);tira.setPixelColor(Pantalla[ 239],0,0,0);
  tira.setPixelColor(Pantalla[ 255],0,0,0);tira.setPixelColor(Pantalla[ 254],0,0,0);tira.setPixelColor(Pantalla[ 253],0,0,0);tira.setPixelColor(Pantalla[ 252],0,0,0);tira.setPixelColor(Pantalla[ 251],0,0,0);tira.setPixelColor(Pantalla[ 250],0,0,0);tira.setPixelColor(Pantalla[ 248],255,255,0);tira.setPixelColor(Pantalla[ 247],255,255,0);tira.setPixelColor(Pantalla[ 245],0,0,0);tira.setPixelColor(Pantalla[ 244],0,0,0);tira.setPixelColor(Pantalla[ 243],0,0,0);tira.setPixelColor(Pantalla[ 242],0,0,0);tira.setPixelColor(Pantalla[ 241],0,0,0);tira.setPixelColor(Pantalla[ 240],0,0,0);
  
}

void Matriz_Colorea_Apuntando()
{
  
  tira.setPixelColor(Pantalla[ 127],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 126],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 125],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 124],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 123],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 122],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 121],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 120],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 119],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 118],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 117],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 116],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 115],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 114],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 113],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 112],pixel_rojo,pixel_verde,pixel_azul);
  tira.setPixelColor(Pantalla[ 128],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 129],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 130],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 131],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 132],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 133],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 134],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 135],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 136],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 137],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 138],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 139],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 140],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 141],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 142],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 143],pixel_rojo,pixel_verde,pixel_azul);
  tira.setPixelColor(Pantalla[ 159],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 158],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 157],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 156],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 155],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 154],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 153],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 152],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 151],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 150],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 149],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 148],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 147],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 146],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 145],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 144],pixel_rojo,pixel_verde,pixel_azul);
  tira.setPixelColor(Pantalla[ 160],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 161],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 162],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 163],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 164],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 165],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 166],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 167],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 168],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 169],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 170],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 171],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 172],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 173],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 174],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 175],pixel_rojo,pixel_verde,pixel_azul);
  tira.setPixelColor(Pantalla[ 191],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 190],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 189],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 188],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 187],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 186],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 185],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 184],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 183],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 182],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 181],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 180],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 179],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 178],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 177],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 176],pixel_rojo,pixel_verde,pixel_azul);
  tira.setPixelColor(Pantalla[ 192],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 193],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 194],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 195],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 196],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 197],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 198],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 199],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 200],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 201],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 202],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 203],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 204],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 205],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 206],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 207],pixel_rojo,pixel_verde,pixel_azul);
  tira.setPixelColor(Pantalla[ 223],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 222],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 221],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 220],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 219],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 218],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 217],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 216],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 215],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 214],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 213],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 212],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 211],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 210],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 209],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 208],pixel_rojo,pixel_verde,pixel_azul);
  tira.setPixelColor(Pantalla[ 224],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 225],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 226],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 227],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 228],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 229],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 230],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 231],100,100,100);tira.setPixelColor(Pantalla[ 232],100,100,100);tira.setPixelColor(Pantalla[ 233],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 234],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 235],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 236],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 237],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 238],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 239],pixel_rojo,pixel_verde,pixel_azul);
  tira.setPixelColor(Pantalla[ 255],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 254],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 253],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 252],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 251],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 250],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 249],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 248],100,100,100);tira.setPixelColor(Pantalla[ 247],100,100,100);tira.setPixelColor(Pantalla[ 246],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 245],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 244],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 243],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 242],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 241],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 240],pixel_rojo,pixel_verde,pixel_azul);
  


}

void Matriz_Aro_1_Apuntado()
{

  tira.setPixelColor(Pantalla[ 127],255,255,0);tira.setPixelColor(Pantalla[ 126],255,255,0);tira.setPixelColor(Pantalla[ 125],255,255,0);tira.setPixelColor(Pantalla[ 124],255,255,0);tira.setPixelColor(Pantalla[ 123],255,255,0);tira.setPixelColor(Pantalla[ 122],255,255,0);tira.setPixelColor(Pantalla[ 121],255,255,0);tira.setPixelColor(Pantalla[ 120],255,255,0);tira.setPixelColor(Pantalla[ 119],255,255,0);tira.setPixelColor(Pantalla[ 118],255,255,0);tira.setPixelColor(Pantalla[ 117],255,255,0);tira.setPixelColor(Pantalla[ 116],255,255,0);tira.setPixelColor(Pantalla[ 115],255,255,0);tira.setPixelColor(Pantalla[ 114],255,255,0);tira.setPixelColor(Pantalla[ 113],255,255,0);tira.setPixelColor(Pantalla[ 112],255,255,0);
  tira.setPixelColor(Pantalla[ 128],255,255,0);tira.setPixelColor(Pantalla[ 143],255,255,0);
  tira.setPixelColor(Pantalla[ 159],255,255,0);tira.setPixelColor(Pantalla[ 144],255,255,0);
  tira.setPixelColor(Pantalla[ 160],255,255,0);tira.setPixelColor(Pantalla[ 175],255,255,0);
  tira.setPixelColor(Pantalla[ 191],255,255,0);tira.setPixelColor(Pantalla[ 176],255,255,0);
  tira.setPixelColor(Pantalla[ 192],255,255,0);tira.setPixelColor(Pantalla[ 207],255,255,0);
  tira.setPixelColor(Pantalla[ 223],255,255,0);tira.setPixelColor(Pantalla[ 208],255,255,0);
  tira.setPixelColor(Pantalla[ 224],255,255,0);tira.setPixelColor(Pantalla[ 231],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 232],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 239],255,255,0);
  tira.setPixelColor(Pantalla[ 255],255,255,0);tira.setPixelColor(Pantalla[ 248],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 247],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 240],255,255,0);
  
}

void Matriz_Aro_2_Apuntado()
{
  tira.setPixelColor(Pantalla[ 129],255,255,0);tira.setPixelColor(Pantalla[ 130],255,255,0);tira.setPixelColor(Pantalla[ 131],255,255,0);tira.setPixelColor(Pantalla[ 132],255,255,0);tira.setPixelColor(Pantalla[ 133],255,255,0);tira.setPixelColor(Pantalla[ 134],255,255,0);tira.setPixelColor(Pantalla[ 135],255,255,0);tira.setPixelColor(Pantalla[ 136],255,255,0);tira.setPixelColor(Pantalla[ 137],255,255,0);tira.setPixelColor(Pantalla[ 138],255,255,0);tira.setPixelColor(Pantalla[ 139],255,255,0);tira.setPixelColor(Pantalla[ 140],255,255,0);tira.setPixelColor(Pantalla[ 141],255,255,0);tira.setPixelColor(Pantalla[ 142],255,255,0);
  tira.setPixelColor(Pantalla[ 158],255,255,0);tira.setPixelColor(Pantalla[ 145],255,255,0);
  tira.setPixelColor(Pantalla[ 161],255,255,0);tira.setPixelColor(Pantalla[ 174],255,255,0);
  tira.setPixelColor(Pantalla[ 190],255,255,0);tira.setPixelColor(Pantalla[ 177],255,255,0);
  tira.setPixelColor(Pantalla[ 193],255,255,0);tira.setPixelColor(Pantalla[ 206],255,255,0);
  tira.setPixelColor(Pantalla[ 222],255,255,0);tira.setPixelColor(Pantalla[ 209],255,255,0);
  tira.setPixelColor(Pantalla[ 225],255,255,0);tira.setPixelColor(Pantalla[ 231],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 232],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 238],255,255,0);
  tira.setPixelColor(Pantalla[ 254],255,255,0);tira.setPixelColor(Pantalla[ 248],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 247],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 241],255,255,0);
  

}

void Matriz_Aro_3_Apuntado()
{
  tira.setPixelColor(Pantalla[ 157],255,255,0);tira.setPixelColor(Pantalla[ 156],255,255,0);tira.setPixelColor(Pantalla[ 155],255,255,0);tira.setPixelColor(Pantalla[ 154],255,255,0);tira.setPixelColor(Pantalla[ 153],255,255,0);tira.setPixelColor(Pantalla[ 152],255,255,0);tira.setPixelColor(Pantalla[ 151],255,255,0);tira.setPixelColor(Pantalla[ 150],255,255,0);tira.setPixelColor(Pantalla[ 149],255,255,0);tira.setPixelColor(Pantalla[ 148],255,255,0);tira.setPixelColor(Pantalla[ 147],255,255,0);tira.setPixelColor(Pantalla[ 146],255,255,0);
  tira.setPixelColor(Pantalla[ 162],255,255,0);tira.setPixelColor(Pantalla[ 173],255,255,0);
  tira.setPixelColor(Pantalla[ 189],255,255,0);tira.setPixelColor(Pantalla[ 178],255,255,0);
  tira.setPixelColor(Pantalla[ 194],255,255,0);tira.setPixelColor(Pantalla[ 205],255,255,0);
  tira.setPixelColor(Pantalla[ 221],255,255,0);tira.setPixelColor(Pantalla[ 210],255,255,0);
  tira.setPixelColor(Pantalla[ 226],255,255,0);tira.setPixelColor(Pantalla[ 231],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 232],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 237],255,255,0);
  tira.setPixelColor(Pantalla[ 253],255,255,0);tira.setPixelColor(Pantalla[ 248],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 247],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 242],255,255,0);
  
}

void Matriz_Aro_4_Apuntado()
{
  tira.setPixelColor(Pantalla[ 163],255,255,0);tira.setPixelColor(Pantalla[ 164],255,255,0);tira.setPixelColor(Pantalla[ 165],255,255,0);tira.setPixelColor(Pantalla[ 166],255,255,0);tira.setPixelColor(Pantalla[ 167],255,255,0);tira.setPixelColor(Pantalla[ 168],255,255,0);tira.setPixelColor(Pantalla[ 169],255,255,0);tira.setPixelColor(Pantalla[ 170],255,255,0);tira.setPixelColor(Pantalla[ 171],255,255,0);tira.setPixelColor(Pantalla[ 172],255,255,0);
  tira.setPixelColor(Pantalla[ 188],255,255,0);tira.setPixelColor(Pantalla[ 179],255,255,0);
  tira.setPixelColor(Pantalla[ 195],255,255,0);tira.setPixelColor(Pantalla[ 204],255,255,0);
  tira.setPixelColor(Pantalla[ 220],255,255,0);tira.setPixelColor(Pantalla[ 211],255,255,0);
  tira.setPixelColor(Pantalla[ 227],255,255,0);tira.setPixelColor(Pantalla[ 231],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 232],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 236],255,255,0);
  tira.setPixelColor(Pantalla[ 252],255,255,0);tira.setPixelColor(Pantalla[ 248],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 247],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 243],255,255,0);
  

}

void Matriz_Aro_5_Apuntado()
{
  tira.setPixelColor(Pantalla[ 187],255,255,0);tira.setPixelColor(Pantalla[ 186],255,255,0);tira.setPixelColor(Pantalla[ 185],255,255,0);tira.setPixelColor(Pantalla[ 184],255,255,0);tira.setPixelColor(Pantalla[ 183],255,255,0);tira.setPixelColor(Pantalla[ 182],255,255,0);tira.setPixelColor(Pantalla[ 181],255,255,0);tira.setPixelColor(Pantalla[ 180],255,255,0);
  tira.setPixelColor(Pantalla[ 196],255,255,0);tira.setPixelColor(Pantalla[ 203],255,255,0);
  tira.setPixelColor(Pantalla[ 219],255,255,0);tira.setPixelColor(Pantalla[ 212],255,255,0);
  tira.setPixelColor(Pantalla[ 228],255,255,0);tira.setPixelColor(Pantalla[ 231],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 232],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 235],255,255,0);
  tira.setPixelColor(Pantalla[ 251],255,255,0);tira.setPixelColor(Pantalla[ 248],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 247],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 244],255,255,0);
  

}

void Matriz_Aro_6_Apuntado()
{
  tira.setPixelColor(Pantalla[ 197],255,255,0);tira.setPixelColor(Pantalla[ 198],255,255,0);tira.setPixelColor(Pantalla[ 199],255,255,0);tira.setPixelColor(Pantalla[ 200],255,255,0);tira.setPixelColor(Pantalla[ 201],255,255,0);tira.setPixelColor(Pantalla[ 202],255,255,0);
  tira.setPixelColor(Pantalla[ 218],255,255,0);tira.setPixelColor(Pantalla[ 213],255,255,0);
  tira.setPixelColor(Pantalla[ 229],255,255,0);tira.setPixelColor(Pantalla[ 231],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 232],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 234],255,255,0);
  tira.setPixelColor(Pantalla[ 250],255,255,0);tira.setPixelColor(Pantalla[ 248],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 247],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 245],255,255,0);
  

}

void Matriz_Aro_7_Apuntado()
{
  tira.setPixelColor(Pantalla[ 217],255,255,0);tira.setPixelColor(Pantalla[ 216],255,255,0);tira.setPixelColor(Pantalla[ 215],255,255,0);tira.setPixelColor(Pantalla[ 214],255,255,0);
  tira.setPixelColor(Pantalla[ 230],255,255,0);tira.setPixelColor(Pantalla[ 231],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 232],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 233],255,255,0);
  tira.setPixelColor(Pantalla[ 249],255,255,0);tira.setPixelColor(Pantalla[ 248],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 247],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 246],255,255,0);
  

}






void Muestra_Datos_Programador()
{
  //Serial.println("datos_recibidos.no_paquete_test ="+String(datos_recibidos.n));
  Serial.println("datos_recibidos.tipo_tiro ="+String(datos_recibidos.t));
  //Serial.println("datos_recibidos.color ="+String(datos_recibidos.c));
  //Serial.println("datos_recibidos.diana ="+String(datos_recibidos.d));
  //Serial.println("datos_recibidos.propietario ="+String(datos_recibidos.p));
  //Serial.println("datos_recibidos.tiempo ="+String(datos_recibidos.tiempo));      
  //Serial.println("datos_recibidos.figura ="+String(datos_recibidos.f));
  //Serial.println("datos_recibidos.shot ="+String(datos_recibidos.s));
}



void Matriz_Aro_1_Disparo()
{
  tira.setPixelColor(Pantalla[ 6],0,0,255);tira.setPixelColor(Pantalla[ 7],0,0,255);tira.setPixelColor(Pantalla[ 8],0,0,255);tira.setPixelColor(Pantalla[ 9],0,0,255);
  tira.setPixelColor(Pantalla[ 26],0,0,255);tira.setPixelColor(Pantalla[ 25],0,0,255);tira.setPixelColor(Pantalla[ 24],0,0,255);tira.setPixelColor(Pantalla[ 23],0,0,255);tira.setPixelColor(Pantalla[ 22],0,0,255);tira.setPixelColor(Pantalla[ 21],0,0,255);
  tira.setPixelColor(Pantalla[ 36],0,0,255);tira.setPixelColor(Pantalla[ 37],0,0,255);tira.setPixelColor(Pantalla[ 38],255,0,255);tira.setPixelColor(Pantalla[ 39],100,100,100);tira.setPixelColor(Pantalla[ 40],100,100,100);tira.setPixelColor(Pantalla[ 41],255,0,255);tira.setPixelColor(Pantalla[ 42],0,0,255);tira.setPixelColor(Pantalla[ 43],0,0,255);
  tira.setPixelColor(Pantalla[ 60],0,0,255);tira.setPixelColor(Pantalla[ 59],0,0,255);tira.setPixelColor(Pantalla[ 58],255,0,255);tira.setPixelColor(Pantalla[ 57],255,0,255);tira.setPixelColor(Pantalla[ 56],100,100,100);tira.setPixelColor(Pantalla[ 55],100,100,100);tira.setPixelColor(Pantalla[ 54],255,0,255);tira.setPixelColor(Pantalla[ 53],255,0,255);tira.setPixelColor(Pantalla[ 52],0,0,255);tira.setPixelColor(Pantalla[ 51],0,0,255);
  tira.setPixelColor(Pantalla[ 66],0,0,255);tira.setPixelColor(Pantalla[ 67],0,0,255);tira.setPixelColor(Pantalla[ 68],255,0,255);tira.setPixelColor(Pantalla[ 69],255,0,255);tira.setPixelColor(Pantalla[ 70],100,100,100);tira.setPixelColor(Pantalla[ 71],100,100,100);tira.setPixelColor(Pantalla[ 72],100,100,100);tira.setPixelColor(Pantalla[ 73],100,100,100);tira.setPixelColor(Pantalla[ 74],255,0,255);tira.setPixelColor(Pantalla[ 75],255,0,255);tira.setPixelColor(Pantalla[ 76],0,0,255);tira.setPixelColor(Pantalla[ 77],0,0,255);
  tira.setPixelColor(Pantalla[ 94],0,0,255);tira.setPixelColor(Pantalla[ 93],0,0,255);tira.setPixelColor(Pantalla[ 92],255,0,255);tira.setPixelColor(Pantalla[ 91],255,0,255);tira.setPixelColor(Pantalla[ 90],100,100,100);tira.setPixelColor(Pantalla[ 89],100,100,100);tira.setPixelColor(Pantalla[ 88],100,100,100);tira.setPixelColor(Pantalla[ 87],100,100,100);tira.setPixelColor(Pantalla[ 86],100,100,100);tira.setPixelColor(Pantalla[ 85],100,100,100);tira.setPixelColor(Pantalla[ 84],255,0,255);tira.setPixelColor(Pantalla[ 83],255,0,255);tira.setPixelColor(Pantalla[ 82],0,0,255);tira.setPixelColor(Pantalla[ 81],0,0,255);
  tira.setPixelColor(Pantalla[ 97],0,0,255);tira.setPixelColor(Pantalla[ 98],0,0,255);tira.setPixelColor(Pantalla[ 99],100,100,100);tira.setPixelColor(Pantalla[ 100],100,100,100);tira.setPixelColor(Pantalla[ 101],100,100,100);tira.setPixelColor(Pantalla[ 102],100,100,100);tira.setPixelColor(Pantalla[ 103],100,100,100);tira.setPixelColor(Pantalla[ 104],100,100,100);tira.setPixelColor(Pantalla[ 105],100,100,100);tira.setPixelColor(Pantalla[ 106],100,100,100);tira.setPixelColor(Pantalla[ 107],100,100,100);tira.setPixelColor(Pantalla[ 108],100,100,100);tira.setPixelColor(Pantalla[ 109],0,0,255);tira.setPixelColor(Pantalla[ 110],0,0,255);
  tira.setPixelColor(Pantalla[ 126],0,0,255);tira.setPixelColor(Pantalla[ 125],0,0,255);tira.setPixelColor(Pantalla[ 124],100,100,100);tira.setPixelColor(Pantalla[ 123],100,100,100);tira.setPixelColor(Pantalla[ 122],100,100,100);tira.setPixelColor(Pantalla[ 121],100,100,100);tira.setPixelColor(Pantalla[ 120],100,100,100);tira.setPixelColor(Pantalla[ 119],100,100,100);tira.setPixelColor(Pantalla[ 118],100,100,100);tira.setPixelColor(Pantalla[ 117],100,100,100);tira.setPixelColor(Pantalla[ 116],100,100,100);tira.setPixelColor(Pantalla[ 115],100,100,100);tira.setPixelColor(Pantalla[ 114],0,0,255);tira.setPixelColor(Pantalla[ 113],0,0,255);
  tira.setPixelColor(Pantalla[ 129],0,0,255);tira.setPixelColor(Pantalla[ 130],0,0,255);tira.setPixelColor(Pantalla[ 131],255,0,255);tira.setPixelColor(Pantalla[ 132],255,0,255);tira.setPixelColor(Pantalla[ 133],100,100,100);tira.setPixelColor(Pantalla[ 134],100,100,100);tira.setPixelColor(Pantalla[ 135],100,100,100);tira.setPixelColor(Pantalla[ 136],100,100,100);tira.setPixelColor(Pantalla[ 137],100,100,100);tira.setPixelColor(Pantalla[ 138],100,100,100);tira.setPixelColor(Pantalla[ 139],255,0,255);tira.setPixelColor(Pantalla[ 140],255,0,255);tira.setPixelColor(Pantalla[ 141],0,0,255);tira.setPixelColor(Pantalla[ 142],0,0,255);
  tira.setPixelColor(Pantalla[ 157],0,0,255);tira.setPixelColor(Pantalla[ 156],0,0,255);tira.setPixelColor(Pantalla[ 155],255,0,255);tira.setPixelColor(Pantalla[ 154],255,0,255);tira.setPixelColor(Pantalla[ 153],100,100,100);tira.setPixelColor(Pantalla[ 152],100,100,100);tira.setPixelColor(Pantalla[ 151],100,100,100);tira.setPixelColor(Pantalla[ 150],100,100,100);tira.setPixelColor(Pantalla[ 149],255,0,255);tira.setPixelColor(Pantalla[ 148],255,0,255);tira.setPixelColor(Pantalla[ 147],0,0,255);tira.setPixelColor(Pantalla[ 146],0,0,255);
  tira.setPixelColor(Pantalla[ 163],0,0,255);tira.setPixelColor(Pantalla[ 164],0,0,255);tira.setPixelColor(Pantalla[ 165],255,0,255);tira.setPixelColor(Pantalla[ 166],255,0,255);tira.setPixelColor(Pantalla[ 167],100,100,100);tira.setPixelColor(Pantalla[ 168],100,100,100);tira.setPixelColor(Pantalla[ 169],255,0,255);tira.setPixelColor(Pantalla[ 170],255,0,255);tira.setPixelColor(Pantalla[ 171],0,0,255);tira.setPixelColor(Pantalla[ 172],0,0,255);
  tira.setPixelColor(Pantalla[ 187],0,0,255);tira.setPixelColor(Pantalla[ 186],0,0,255);tira.setPixelColor(Pantalla[ 185],255,0,255);tira.setPixelColor(Pantalla[ 184],100,100,100);tira.setPixelColor(Pantalla[ 183],100,100,100);tira.setPixelColor(Pantalla[ 182],255,0,255);tira.setPixelColor(Pantalla[ 181],0,0,255);tira.setPixelColor(Pantalla[ 180],0,0,255);
  tira.setPixelColor(Pantalla[ 217],0,0,255);tira.setPixelColor(Pantalla[ 216],0,0,255);tira.setPixelColor(Pantalla[ 215],0,0,255);tira.setPixelColor(Pantalla[ 214],0,0,255);
 
  
}

void Matriz_Aro_2_Disparo()
{
  tira.setPixelColor(Pantalla[ 0],255,0,255);tira.setPixelColor(Pantalla[ 1],100,100,100);tira.setPixelColor(Pantalla[ 2],255,0,255);tira.setPixelColor(Pantalla[ 3],255,0,255);tira.setPixelColor(Pantalla[ 4],0,0,255);tira.setPixelColor(Pantalla[ 5],0,0,255);tira.setPixelColor(Pantalla[ 6],0,0,255);tira.setPixelColor(Pantalla[ 7],0,0,255);tira.setPixelColor(Pantalla[ 8],0,0,255);tira.setPixelColor(Pantalla[ 9],0,0,255);tira.setPixelColor(Pantalla[ 10],0,0,255);tira.setPixelColor(Pantalla[ 11],0,0,255);tira.setPixelColor(Pantalla[ 12],255,0,255);tira.setPixelColor(Pantalla[ 13],255,0,255);tira.setPixelColor(Pantalla[ 14],0,0,255);tira.setPixelColor(Pantalla[ 15],0,0,255);
  tira.setPixelColor(Pantalla[ 31],255,0,255);tira.setPixelColor(Pantalla[ 30],100,100,100);tira.setPixelColor(Pantalla[ 29],100,100,100);tira.setPixelColor(Pantalla[ 28],255,0,255);tira.setPixelColor(Pantalla[ 27],255,0,255);tira.setPixelColor(Pantalla[ 26],255,0,255);tira.setPixelColor(Pantalla[ 25],0,0,255);tira.setPixelColor(Pantalla[ 24],0,0,255);tira.setPixelColor(Pantalla[ 23],0,0,255);tira.setPixelColor(Pantalla[ 22],0,0,255);tira.setPixelColor(Pantalla[ 21],0,0,255);tira.setPixelColor(Pantalla[ 20],255,0,255);tira.setPixelColor(Pantalla[ 19],100,100,100);tira.setPixelColor(Pantalla[ 18],100,100,100);tira.setPixelColor(Pantalla[ 17],255,0,255);tira.setPixelColor(Pantalla[ 16],0,0,255);
  tira.setPixelColor(Pantalla[ 32],0,0,255);tira.setPixelColor(Pantalla[ 33],255,0,255);tira.setPixelColor(Pantalla[ 34],100,100,100);tira.setPixelColor(Pantalla[ 35],100,100,100);tira.setPixelColor(Pantalla[ 36],100,100,100);tira.setPixelColor(Pantalla[ 37],255,0,255);tira.setPixelColor(Pantalla[ 38],100,100,100);tira.setPixelColor(Pantalla[ 39],100,100,100);tira.setPixelColor(Pantalla[ 40],100,100,100);tira.setPixelColor(Pantalla[ 41],100,100,100);tira.setPixelColor(Pantalla[ 42],255,0,255);tira.setPixelColor(Pantalla[ 43],100,100,100);tira.setPixelColor(Pantalla[ 44],100,100,100);tira.setPixelColor(Pantalla[ 45],100,100,100);tira.setPixelColor(Pantalla[ 46],255,0,255);tira.setPixelColor(Pantalla[ 47],0,0,255);
  tira.setPixelColor(Pantalla[ 63],0,0,255);tira.setPixelColor(Pantalla[ 62],0,0,255);tira.setPixelColor(Pantalla[ 61],255,0,255);tira.setPixelColor(Pantalla[ 60],100,100,100);tira.setPixelColor(Pantalla[ 59],100,100,100);tira.setPixelColor(Pantalla[ 58],100,100,100);tira.setPixelColor(Pantalla[ 57],100,100,100);tira.setPixelColor(Pantalla[ 56],100,100,100);tira.setPixelColor(Pantalla[ 55],100,100,100);tira.setPixelColor(Pantalla[ 54],100,100,100);tira.setPixelColor(Pantalla[ 53],100,100,100);tira.setPixelColor(Pantalla[ 52],100,100,100);tira.setPixelColor(Pantalla[ 51],100,100,100);tira.setPixelColor(Pantalla[ 50],255,0,255);tira.setPixelColor(Pantalla[ 49],0,0,255);tira.setPixelColor(Pantalla[ 48],255,0,255);
  tira.setPixelColor(Pantalla[ 64],0,0,255);tira.setPixelColor(Pantalla[ 65],0,0,255);tira.setPixelColor(Pantalla[ 66],255,0,255);tira.setPixelColor(Pantalla[ 67],255,0,255);tira.setPixelColor(Pantalla[ 68],100,100,100);tira.setPixelColor(Pantalla[ 69],100,100,100);tira.setPixelColor(Pantalla[ 70],100,100,100);tira.setPixelColor(Pantalla[ 71],100,100,100);tira.setPixelColor(Pantalla[ 72],100,100,100);tira.setPixelColor(Pantalla[ 73],100,100,100);tira.setPixelColor(Pantalla[ 74],100,100,100);tira.setPixelColor(Pantalla[ 75],100,100,100);tira.setPixelColor(Pantalla[ 76],255,0,255);tira.setPixelColor(Pantalla[ 77],255,0,255);tira.setPixelColor(Pantalla[ 78],0,0,255);tira.setPixelColor(Pantalla[ 79],0,0,255);
  tira.setPixelColor(Pantalla[ 95],0,0,255);tira.setPixelColor(Pantalla[ 94],255,0,255);tira.setPixelColor(Pantalla[ 93],255,0,255);tira.setPixelColor(Pantalla[ 92],255,0,255);tira.setPixelColor(Pantalla[ 91],100,100,100);tira.setPixelColor(Pantalla[ 90],100,100,100);tira.setPixelColor(Pantalla[ 89],100,100,100);tira.setPixelColor(Pantalla[ 88],100,100,100);tira.setPixelColor(Pantalla[ 87],100,100,100);tira.setPixelColor(Pantalla[ 86],100,100,100);tira.setPixelColor(Pantalla[ 85],100,100,100);tira.setPixelColor(Pantalla[ 84],100,100,100);tira.setPixelColor(Pantalla[ 83],100,100,100);tira.setPixelColor(Pantalla[ 82],255,0,255);tira.setPixelColor(Pantalla[ 81],255,0,255);tira.setPixelColor(Pantalla[ 80],0,0,255);
  tira.setPixelColor(Pantalla[ 96],255,0,255);tira.setPixelColor(Pantalla[ 97],255,0,255);tira.setPixelColor(Pantalla[ 98],100,100,100);tira.setPixelColor(Pantalla[ 99],100,100,100);tira.setPixelColor(Pantalla[ 100],100,100,100);tira.setPixelColor(Pantalla[ 101],100,100,100);tira.setPixelColor(Pantalla[ 102],100,100,100);tira.setPixelColor(Pantalla[ 103],100,100,100);tira.setPixelColor(Pantalla[ 104],100,100,100);tira.setPixelColor(Pantalla[ 105],100,100,100);tira.setPixelColor(Pantalla[ 106],100,100,100);tira.setPixelColor(Pantalla[ 107],100,100,100);tira.setPixelColor(Pantalla[ 108],100,100,100);tira.setPixelColor(Pantalla[ 109],100,100,100);tira.setPixelColor(Pantalla[ 110],255,0,255);tira.setPixelColor(Pantalla[ 111],255,0,255);
  tira.setPixelColor(Pantalla[ 127],0,0,255);tira.setPixelColor(Pantalla[ 126],255,0,255);tira.setPixelColor(Pantalla[ 125],100,100,100);tira.setPixelColor(Pantalla[ 124],100,100,100);tira.setPixelColor(Pantalla[ 123],100,100,100);tira.setPixelColor(Pantalla[ 122],100,100,100);tira.setPixelColor(Pantalla[ 121],100,100,100);tira.setPixelColor(Pantalla[ 120],100,100,100);tira.setPixelColor(Pantalla[ 119],100,100,100);tira.setPixelColor(Pantalla[ 118],100,100,100);tira.setPixelColor(Pantalla[ 117],100,100,100);tira.setPixelColor(Pantalla[ 116],100,100,100);tira.setPixelColor(Pantalla[ 115],100,100,100);tira.setPixelColor(Pantalla[ 114],100,100,100);tira.setPixelColor(Pantalla[ 113],255,0,255);tira.setPixelColor(Pantalla[ 112],255,0,255);
  tira.setPixelColor(Pantalla[ 128],0,0,255);tira.setPixelColor(Pantalla[ 129],255,0,255);tira.setPixelColor(Pantalla[ 130],255,0,255);tira.setPixelColor(Pantalla[ 131],255,0,255);tira.setPixelColor(Pantalla[ 132],100,100,100);tira.setPixelColor(Pantalla[ 133],100,100,100);tira.setPixelColor(Pantalla[ 134],100,100,100);tira.setPixelColor(Pantalla[ 135],100,100,100);tira.setPixelColor(Pantalla[ 136],100,100,100);tira.setPixelColor(Pantalla[ 137],100,100,100);tira.setPixelColor(Pantalla[ 138],100,100,100);tira.setPixelColor(Pantalla[ 139],100,100,100);tira.setPixelColor(Pantalla[ 140],100,100,100);tira.setPixelColor(Pantalla[ 141],255,0,255);tira.setPixelColor(Pantalla[ 142],255,0,255);tira.setPixelColor(Pantalla[ 143],0,0,255);
  tira.setPixelColor(Pantalla[ 159],0,0,255);tira.setPixelColor(Pantalla[ 158],0,0,255);tira.setPixelColor(Pantalla[ 157],255,0,255);tira.setPixelColor(Pantalla[ 156],255,0,255);tira.setPixelColor(Pantalla[ 155],100,100,100);tira.setPixelColor(Pantalla[ 154],100,100,100);tira.setPixelColor(Pantalla[ 153],100,100,100);tira.setPixelColor(Pantalla[ 152],100,100,100);tira.setPixelColor(Pantalla[ 151],100,100,100);tira.setPixelColor(Pantalla[ 150],100,100,100);tira.setPixelColor(Pantalla[ 149],100,100,100);tira.setPixelColor(Pantalla[ 148],100,100,100);tira.setPixelColor(Pantalla[ 147],255,0,255);tira.setPixelColor(Pantalla[ 146],255,0,255);tira.setPixelColor(Pantalla[ 145],0,0,255);tira.setPixelColor(Pantalla[ 144],0,0,255);
  tira.setPixelColor(Pantalla[ 160],0,0,255);tira.setPixelColor(Pantalla[ 161],0,0,255);tira.setPixelColor(Pantalla[ 162],255,0,255);tira.setPixelColor(Pantalla[ 163],100,100,100);tira.setPixelColor(Pantalla[ 164],100,100,100);tira.setPixelColor(Pantalla[ 165],100,100,100);tira.setPixelColor(Pantalla[ 166],100,100,100);tira.setPixelColor(Pantalla[ 167],100,100,100);tira.setPixelColor(Pantalla[ 168],100,100,100);tira.setPixelColor(Pantalla[ 169],100,100,100);tira.setPixelColor(Pantalla[ 170],100,100,100);tira.setPixelColor(Pantalla[ 171],100,100,100);tira.setPixelColor(Pantalla[ 172],100,100,100);tira.setPixelColor(Pantalla[ 173],255,0,255);tira.setPixelColor(Pantalla[ 174],255,0,255);tira.setPixelColor(Pantalla[ 175],0,0,255);
  tira.setPixelColor(Pantalla[ 191],0,0,255);tira.setPixelColor(Pantalla[ 190],255,0,255);tira.setPixelColor(Pantalla[ 189],100,100,100);tira.setPixelColor(Pantalla[ 188],100,100,100);tira.setPixelColor(Pantalla[ 187],100,100,100);tira.setPixelColor(Pantalla[ 186],255,0,255);tira.setPixelColor(Pantalla[ 185],255,0,255);tira.setPixelColor(Pantalla[ 184],100,100,100);tira.setPixelColor(Pantalla[ 183],100,100,100);tira.setPixelColor(Pantalla[ 182],255,0,255);tira.setPixelColor(Pantalla[ 181],0,0,255);tira.setPixelColor(Pantalla[ 180],100,100,100);tira.setPixelColor(Pantalla[ 179],100,100,100);tira.setPixelColor(Pantalla[ 178],100,100,100);tira.setPixelColor(Pantalla[ 177],255,0,255);tira.setPixelColor(Pantalla[ 176],255,0,255);
  tira.setPixelColor(Pantalla[ 192],255,0,255);tira.setPixelColor(Pantalla[ 193],100,100,100);tira.setPixelColor(Pantalla[ 194],100,100,100);tira.setPixelColor(Pantalla[ 195],100,100,100);tira.setPixelColor(Pantalla[ 196],255,0,255);tira.setPixelColor(Pantalla[ 197],255,0,255);tira.setPixelColor(Pantalla[ 198],255,0,255);tira.setPixelColor(Pantalla[ 199],100,100,100);tira.setPixelColor(Pantalla[ 200],100,100,100);tira.setPixelColor(Pantalla[ 201],100,100,100);tira.setPixelColor(Pantalla[ 202],255,0,255);tira.setPixelColor(Pantalla[ 203],255,0,255);tira.setPixelColor(Pantalla[ 204],100,100,100);tira.setPixelColor(Pantalla[ 205],100,100,100);tira.setPixelColor(Pantalla[ 206],100,100,100);tira.setPixelColor(Pantalla[ 207],255,0,255);
  tira.setPixelColor(Pantalla[ 223],255,0,255);tira.setPixelColor(Pantalla[ 222],100,100,100);tira.setPixelColor(Pantalla[ 221],100,100,100);tira.setPixelColor(Pantalla[ 220],255,0,255);tira.setPixelColor(Pantalla[ 219],255,0,255);tira.setPixelColor(Pantalla[ 218],255,0,255);tira.setPixelColor(Pantalla[ 217],0,0,255);tira.setPixelColor(Pantalla[ 216],255,0,255);tira.setPixelColor(Pantalla[ 215],100,100,100);tira.setPixelColor(Pantalla[ 214],100,100,100);tira.setPixelColor(Pantalla[ 213],255,0,255);tira.setPixelColor(Pantalla[ 212],0,0,255);tira.setPixelColor(Pantalla[ 211],255,0,255);tira.setPixelColor(Pantalla[ 210],100,100,100);tira.setPixelColor(Pantalla[ 209],100,100,100);tira.setPixelColor(Pantalla[ 208],255,0,255);
  tira.setPixelColor(Pantalla[ 224],255,0,255);tira.setPixelColor(Pantalla[ 225],255,0,255);tira.setPixelColor(Pantalla[ 226],100,100,100);tira.setPixelColor(Pantalla[ 227],255,0,255);tira.setPixelColor(Pantalla[ 228],0,0,255);tira.setPixelColor(Pantalla[ 229],0,0,255);tira.setPixelColor(Pantalla[ 230],0,0,255);tira.setPixelColor(Pantalla[ 231],255,0,255);tira.setPixelColor(Pantalla[ 232],255,0,255);tira.setPixelColor(Pantalla[ 233],255,0,255);tira.setPixelColor(Pantalla[ 234],255,0,255);tira.setPixelColor(Pantalla[ 235],0,0,255);tira.setPixelColor(Pantalla[ 236],255,0,255);tira.setPixelColor(Pantalla[ 237],255,0,255);tira.setPixelColor(Pantalla[ 238],255,0,255);tira.setPixelColor(Pantalla[ 239],0,0,255);
  tira.setPixelColor(Pantalla[ 255],0,0,255);tira.setPixelColor(Pantalla[ 254],0,0,255);tira.setPixelColor(Pantalla[ 253],255,0,255);tira.setPixelColor(Pantalla[ 252],0,0,255);tira.setPixelColor(Pantalla[ 251],0,0,255);tira.setPixelColor(Pantalla[ 250],0,0,255);tira.setPixelColor(Pantalla[ 249],0,0,255);tira.setPixelColor(Pantalla[ 248],0,0,255);tira.setPixelColor(Pantalla[ 247],0,0,255);tira.setPixelColor(Pantalla[ 246],0,0,255);tira.setPixelColor(Pantalla[ 245],0,0,255);tira.setPixelColor(Pantalla[ 244],0,0,255);tira.setPixelColor(Pantalla[ 243],0,0,255);tira.setPixelColor(Pantalla[ 242],0,0,255);tira.setPixelColor(Pantalla[ 241],0,0,255);tira.setPixelColor(Pantalla[ 240],0,0,255);
   
}

void Matriz_Aro_3_Disparo()
{
  tira.setPixelColor(Pantalla[ 0],100,100,100);tira.setPixelColor(Pantalla[ 1],100,100,100);tira.setPixelColor(Pantalla[ 2],255,0,255);tira.setPixelColor(Pantalla[ 3],255,0,255);tira.setPixelColor(Pantalla[ 4],100,100,100);tira.setPixelColor(Pantalla[ 5],0,0,255);tira.setPixelColor(Pantalla[ 6],100,100,100);tira.setPixelColor(Pantalla[ 7],100,100,100);tira.setPixelColor(Pantalla[ 8],100,100,100);tira.setPixelColor(Pantalla[ 9],100,100,100);tira.setPixelColor(Pantalla[ 10],100,100,100);tira.setPixelColor(Pantalla[ 11],0,0,255);tira.setPixelColor(Pantalla[ 12],255,0,255);tira.setPixelColor(Pantalla[ 13],100,100,100);tira.setPixelColor(Pantalla[ 14],100,100,100);tira.setPixelColor(Pantalla[ 15],100,100,100);
  tira.setPixelColor(Pantalla[ 31],255,0,255);tira.setPixelColor(Pantalla[ 30],100,100,100);tira.setPixelColor(Pantalla[ 29],100,100,100);tira.setPixelColor(Pantalla[ 28],255,0,255);tira.setPixelColor(Pantalla[ 27],100,100,100);tira.setPixelColor(Pantalla[ 26],100,100,100);tira.setPixelColor(Pantalla[ 25],100,100,100);tira.setPixelColor(Pantalla[ 24],100,100,100);tira.setPixelColor(Pantalla[ 23],100,100,100);tira.setPixelColor(Pantalla[ 22],100,100,100);tira.setPixelColor(Pantalla[ 21],100,100,100);tira.setPixelColor(Pantalla[ 20],100,100,100);tira.setPixelColor(Pantalla[ 19],100,100,100);tira.setPixelColor(Pantalla[ 18],100,100,100);tira.setPixelColor(Pantalla[ 17],100,100,100);tira.setPixelColor(Pantalla[ 16],100,100,100);
  tira.setPixelColor(Pantalla[ 32],0,0,255);tira.setPixelColor(Pantalla[ 33],255,0,255);tira.setPixelColor(Pantalla[ 34],100,100,100);tira.setPixelColor(Pantalla[ 35],100,100,100);tira.setPixelColor(Pantalla[ 36],100,100,100);tira.setPixelColor(Pantalla[ 37],100,100,100);tira.setPixelColor(Pantalla[ 38],100,100,100);tira.setPixelColor(Pantalla[ 39],100,100,100);tira.setPixelColor(Pantalla[ 40],100,100,100);tira.setPixelColor(Pantalla[ 41],100,100,100);tira.setPixelColor(Pantalla[ 42],100,100,100);tira.setPixelColor(Pantalla[ 43],100,100,100);tira.setPixelColor(Pantalla[ 44],100,100,100);tira.setPixelColor(Pantalla[ 45],100,100,100);tira.setPixelColor(Pantalla[ 46],255,0,255);tira.setPixelColor(Pantalla[ 47],0,0,255);
  tira.setPixelColor(Pantalla[ 63],100,100,100);tira.setPixelColor(Pantalla[ 62],100,100,100);tira.setPixelColor(Pantalla[ 61],100,100,100);tira.setPixelColor(Pantalla[ 60],100,100,100);tira.setPixelColor(Pantalla[ 59],100,100,100);tira.setPixelColor(Pantalla[ 58],100,100,100);tira.setPixelColor(Pantalla[ 57],100,100,100);tira.setPixelColor(Pantalla[ 56],100,100,100);tira.setPixelColor(Pantalla[ 55],100,100,100);tira.setPixelColor(Pantalla[ 54],100,100,100);tira.setPixelColor(Pantalla[ 53],100,100,100);tira.setPixelColor(Pantalla[ 52],100,100,100);tira.setPixelColor(Pantalla[ 51],100,100,100);tira.setPixelColor(Pantalla[ 50],255,0,255);tira.setPixelColor(Pantalla[ 49],0,0,255);tira.setPixelColor(Pantalla[ 48],255,0,255);
  tira.setPixelColor(Pantalla[ 64],100,100,100);tira.setPixelColor(Pantalla[ 65],100,100,100);tira.setPixelColor(Pantalla[ 66],100,100,100);tira.setPixelColor(Pantalla[ 67],100,100,100);tira.setPixelColor(Pantalla[ 68],100,100,100);tira.setPixelColor(Pantalla[ 69],100,100,100);tira.setPixelColor(Pantalla[ 70],100,100,100);tira.setPixelColor(Pantalla[ 71],100,100,100);tira.setPixelColor(Pantalla[ 72],100,100,100);tira.setPixelColor(Pantalla[ 73],100,100,100);tira.setPixelColor(Pantalla[ 74],100,100,100);tira.setPixelColor(Pantalla[ 75],100,100,100);tira.setPixelColor(Pantalla[ 76],100,100,100);tira.setPixelColor(Pantalla[ 77],100,100,100);tira.setPixelColor(Pantalla[ 78],0,0,255);tira.setPixelColor(Pantalla[ 79],0,0,255);
  tira.setPixelColor(Pantalla[ 95],100,100,100);tira.setPixelColor(Pantalla[ 94],100,100,100);tira.setPixelColor(Pantalla[ 93],100,100,100);tira.setPixelColor(Pantalla[ 92],100,100,100);tira.setPixelColor(Pantalla[ 91],100,100,100);tira.setPixelColor(Pantalla[ 90],100,100,100);tira.setPixelColor(Pantalla[ 89],100,100,100);tira.setPixelColor(Pantalla[ 88],100,100,100);tira.setPixelColor(Pantalla[ 87],100,100,100);tira.setPixelColor(Pantalla[ 86],100,100,100);tira.setPixelColor(Pantalla[ 85],100,100,100);tira.setPixelColor(Pantalla[ 84],100,100,100);tira.setPixelColor(Pantalla[ 83],100,100,100);tira.setPixelColor(Pantalla[ 82],100,100,100);tira.setPixelColor(Pantalla[ 81],100,100,100);tira.setPixelColor(Pantalla[ 80],100,100,100);
  tira.setPixelColor(Pantalla[ 96],100,100,100);tira.setPixelColor(Pantalla[ 97],100,100,100);tira.setPixelColor(Pantalla[ 98],100,100,100);tira.setPixelColor(Pantalla[ 99],100,100,100);tira.setPixelColor(Pantalla[ 100],100,100,100);tira.setPixelColor(Pantalla[ 101],100,100,100);tira.setPixelColor(Pantalla[ 102],100,100,100);tira.setPixelColor(Pantalla[ 103],100,100,100);tira.setPixelColor(Pantalla[ 104],100,100,100);tira.setPixelColor(Pantalla[ 105],100,100,100);tira.setPixelColor(Pantalla[ 106],100,100,100);tira.setPixelColor(Pantalla[ 107],100,100,100);tira.setPixelColor(Pantalla[ 108],100,100,100);tira.setPixelColor(Pantalla[ 109],100,100,100);tira.setPixelColor(Pantalla[ 110],100,100,100);tira.setPixelColor(Pantalla[ 111],100,100,100);
  tira.setPixelColor(Pantalla[ 127],0,0,255);tira.setPixelColor(Pantalla[ 126],100,100,100);tira.setPixelColor(Pantalla[ 125],100,100,100);tira.setPixelColor(Pantalla[ 124],100,100,100);tira.setPixelColor(Pantalla[ 123],100,100,100);tira.setPixelColor(Pantalla[ 122],100,100,100);tira.setPixelColor(Pantalla[ 121],100,100,100);tira.setPixelColor(Pantalla[ 120],100,100,100);tira.setPixelColor(Pantalla[ 119],100,100,100);tira.setPixelColor(Pantalla[ 118],100,100,100);tira.setPixelColor(Pantalla[ 117],100,100,100);tira.setPixelColor(Pantalla[ 116],100,100,100);tira.setPixelColor(Pantalla[ 115],100,100,100);tira.setPixelColor(Pantalla[ 114],100,100,100);tira.setPixelColor(Pantalla[ 113],100,100,100);tira.setPixelColor(Pantalla[ 112],100,100,100);
  tira.setPixelColor(Pantalla[ 128],100,100,100);tira.setPixelColor(Pantalla[ 129],100,100,100);tira.setPixelColor(Pantalla[ 130],100,100,100);tira.setPixelColor(Pantalla[ 131],100,100,100);tira.setPixelColor(Pantalla[ 132],100,100,100);tira.setPixelColor(Pantalla[ 133],100,100,100);tira.setPixelColor(Pantalla[ 134],100,100,100);tira.setPixelColor(Pantalla[ 135],100,100,100);tira.setPixelColor(Pantalla[ 136],100,100,100);tira.setPixelColor(Pantalla[ 137],100,100,100);tira.setPixelColor(Pantalla[ 138],100,100,100);tira.setPixelColor(Pantalla[ 139],100,100,100);tira.setPixelColor(Pantalla[ 140],100,100,100);tira.setPixelColor(Pantalla[ 141],100,100,100);tira.setPixelColor(Pantalla[ 142],100,100,100);tira.setPixelColor(Pantalla[ 143],100,100,100);
  tira.setPixelColor(Pantalla[ 159],100,100,100);tira.setPixelColor(Pantalla[ 158],100,100,100);tira.setPixelColor(Pantalla[ 157],100,100,100);tira.setPixelColor(Pantalla[ 156],100,100,100);tira.setPixelColor(Pantalla[ 155],100,100,100);tira.setPixelColor(Pantalla[ 154],100,100,100);tira.setPixelColor(Pantalla[ 153],100,100,100);tira.setPixelColor(Pantalla[ 152],100,100,100);tira.setPixelColor(Pantalla[ 151],100,100,100);tira.setPixelColor(Pantalla[ 150],100,100,100);tira.setPixelColor(Pantalla[ 149],100,100,100);tira.setPixelColor(Pantalla[ 148],100,100,100);tira.setPixelColor(Pantalla[ 147],100,100,100);tira.setPixelColor(Pantalla[ 146],100,100,100);tira.setPixelColor(Pantalla[ 145],100,100,100);tira.setPixelColor(Pantalla[ 144],100,100,100);
  tira.setPixelColor(Pantalla[ 160],0,0,255);tira.setPixelColor(Pantalla[ 161],100,100,100);tira.setPixelColor(Pantalla[ 162],100,100,100);tira.setPixelColor(Pantalla[ 163],100,100,100);tira.setPixelColor(Pantalla[ 164],100,100,100);tira.setPixelColor(Pantalla[ 165],100,100,100);tira.setPixelColor(Pantalla[ 166],100,100,100);tira.setPixelColor(Pantalla[ 167],100,100,100);tira.setPixelColor(Pantalla[ 168],100,100,100);tira.setPixelColor(Pantalla[ 169],100,100,100);tira.setPixelColor(Pantalla[ 170],100,100,100);tira.setPixelColor(Pantalla[ 171],100,100,100);tira.setPixelColor(Pantalla[ 172],100,100,100);tira.setPixelColor(Pantalla[ 173],100,100,100);tira.setPixelColor(Pantalla[ 174],100,100,100);tira.setPixelColor(Pantalla[ 175],0,0,255);
  tira.setPixelColor(Pantalla[ 191],0,0,255);tira.setPixelColor(Pantalla[ 190],255,0,255);tira.setPixelColor(Pantalla[ 189],100,100,100);tira.setPixelColor(Pantalla[ 188],100,100,100);tira.setPixelColor(Pantalla[ 187],100,100,100);tira.setPixelColor(Pantalla[ 186],100,100,100);tira.setPixelColor(Pantalla[ 185],100,100,100);tira.setPixelColor(Pantalla[ 184],100,100,100);tira.setPixelColor(Pantalla[ 183],100,100,100);tira.setPixelColor(Pantalla[ 182],100,100,100);tira.setPixelColor(Pantalla[ 181],100,100,100);tira.setPixelColor(Pantalla[ 180],100,100,100);tira.setPixelColor(Pantalla[ 179],100,100,100);tira.setPixelColor(Pantalla[ 178],100,100,100);tira.setPixelColor(Pantalla[ 177],255,0,255);tira.setPixelColor(Pantalla[ 176],255,0,255);
  tira.setPixelColor(Pantalla[ 192],255,0,255);tira.setPixelColor(Pantalla[ 193],100,100,100);tira.setPixelColor(Pantalla[ 194],100,100,100);tira.setPixelColor(Pantalla[ 195],100,100,100);tira.setPixelColor(Pantalla[ 196],100,100,100);tira.setPixelColor(Pantalla[ 197],100,100,100);tira.setPixelColor(Pantalla[ 198],100,100,100);tira.setPixelColor(Pantalla[ 199],100,100,100);tira.setPixelColor(Pantalla[ 200],100,100,100);tira.setPixelColor(Pantalla[ 201],100,100,100);tira.setPixelColor(Pantalla[ 202],100,100,100);tira.setPixelColor(Pantalla[ 203],100,100,100);tira.setPixelColor(Pantalla[ 204],100,100,100);tira.setPixelColor(Pantalla[ 205],100,100,100);tira.setPixelColor(Pantalla[ 206],100,100,100);tira.setPixelColor(Pantalla[ 207],255,0,255);
  tira.setPixelColor(Pantalla[ 223],255,0,255);tira.setPixelColor(Pantalla[ 222],100,100,100);tira.setPixelColor(Pantalla[ 221],100,100,100);tira.setPixelColor(Pantalla[ 220],255,0,255);tira.setPixelColor(Pantalla[ 219],255,0,255);tira.setPixelColor(Pantalla[ 218],100,100,100);tira.setPixelColor(Pantalla[ 217],100,100,100);tira.setPixelColor(Pantalla[ 216],100,100,100);tira.setPixelColor(Pantalla[ 215],100,100,100);tira.setPixelColor(Pantalla[ 214],100,100,100);tira.setPixelColor(Pantalla[ 213],100,100,100);tira.setPixelColor(Pantalla[ 212],0,0,255);tira.setPixelColor(Pantalla[ 211],255,0,255);tira.setPixelColor(Pantalla[ 210],100,100,100);tira.setPixelColor(Pantalla[ 209],100,100,100);tira.setPixelColor(Pantalla[ 208],255,0,255);
  tira.setPixelColor(Pantalla[ 224],255,0,255);tira.setPixelColor(Pantalla[ 225],255,0,255);tira.setPixelColor(Pantalla[ 226],100,100,100);tira.setPixelColor(Pantalla[ 227],255,0,255);tira.setPixelColor(Pantalla[ 228],0,0,255);tira.setPixelColor(Pantalla[ 229],100,100,100);tira.setPixelColor(Pantalla[ 230],100,100,100);tira.setPixelColor(Pantalla[ 231],100,100,100);tira.setPixelColor(Pantalla[ 232],100,100,100);tira.setPixelColor(Pantalla[ 233],100,100,100);tira.setPixelColor(Pantalla[ 234],100,100,100);tira.setPixelColor(Pantalla[ 235],0,0,255);tira.setPixelColor(Pantalla[ 236],255,0,255);tira.setPixelColor(Pantalla[ 237],255,0,255);tira.setPixelColor(Pantalla[ 238],255,0,255);tira.setPixelColor(Pantalla[ 239],0,0,255);
  tira.setPixelColor(Pantalla[ 255],0,0,255);tira.setPixelColor(Pantalla[ 254],0,0,255);tira.setPixelColor(Pantalla[ 253],255,0,255);tira.setPixelColor(Pantalla[ 252],0,0,255);tira.setPixelColor(Pantalla[ 251],0,0,255);tira.setPixelColor(Pantalla[ 250],0,0,255);tira.setPixelColor(Pantalla[ 249],100,100,100);tira.setPixelColor(Pantalla[ 248],100,100,100);tira.setPixelColor(Pantalla[ 247],100,100,100);tira.setPixelColor(Pantalla[ 246],100,100,100);tira.setPixelColor(Pantalla[ 245],0,0,255);tira.setPixelColor(Pantalla[ 244],0,0,255);tira.setPixelColor(Pantalla[ 243],0,0,255);tira.setPixelColor(Pantalla[ 242],0,0,255);tira.setPixelColor(Pantalla[ 241],0,0,255);tira.setPixelColor(Pantalla[ 240],0,0,255);
   
}

void Matriz_Aro_4_Disparo()
{
tira.setPixelColor(Pantalla[ 0],100,100,100);tira.setPixelColor(Pantalla[ 1],100,100,100);tira.setPixelColor(Pantalla[ 2],255,0,255);tira.setPixelColor(Pantalla[ 3],255,255,0);tira.setPixelColor(Pantalla[ 4],105,210,30);tira.setPixelColor(Pantalla[ 5],0,0,255);tira.setPixelColor(Pantalla[ 6],105,210,30);tira.setPixelColor(Pantalla[ 7],105,210,30);tira.setPixelColor(Pantalla[ 8],0,0,255);tira.setPixelColor(Pantalla[ 9],255,255,0);tira.setPixelColor(Pantalla[ 10],105,210,30);tira.setPixelColor(Pantalla[ 11],0,0,255);tira.setPixelColor(Pantalla[ 12],255,0,255);tira.setPixelColor(Pantalla[ 13],100,100,100);tira.setPixelColor(Pantalla[ 14],100,100,100);tira.setPixelColor(Pantalla[ 15],100,100,100);
tira.setPixelColor(Pantalla[ 31],255,0,255);tira.setPixelColor(Pantalla[ 30],100,100,100);tira.setPixelColor(Pantalla[ 29],105,210,30);tira.setPixelColor(Pantalla[ 28],0,0,255);tira.setPixelColor(Pantalla[ 27],0,0,255);tira.setPixelColor(Pantalla[ 26],0,0,255);tira.setPixelColor(Pantalla[ 25],0,0,255);tira.setPixelColor(Pantalla[ 24],0,0,255);tira.setPixelColor(Pantalla[ 23],0,0,255);tira.setPixelColor(Pantalla[ 22],0,0,255);tira.setPixelColor(Pantalla[ 21],0,0,255);tira.setPixelColor(Pantalla[ 20],255,255,0);tira.setPixelColor(Pantalla[ 19],255,255,0);tira.setPixelColor(Pantalla[ 18],105,210,30);tira.setPixelColor(Pantalla[ 17],100,100,100);tira.setPixelColor(Pantalla[ 16],100,100,100);
tira.setPixelColor(Pantalla[ 32],0,0,255);tira.setPixelColor(Pantalla[ 33],255,0,255);tira.setPixelColor(Pantalla[ 34],255,255,0);tira.setPixelColor(Pantalla[ 35],0,0,255);tira.setPixelColor(Pantalla[ 36],100,100,100);tira.setPixelColor(Pantalla[ 37],100,100,100);tira.setPixelColor(Pantalla[ 38],100,100,100);tira.setPixelColor(Pantalla[ 39],100,100,100);tira.setPixelColor(Pantalla[ 40],100,100,100);tira.setPixelColor(Pantalla[ 41],100,100,100);tira.setPixelColor(Pantalla[ 42],100,100,100);tira.setPixelColor(Pantalla[ 43],0,0,255);tira.setPixelColor(Pantalla[ 44],0,0,255);tira.setPixelColor(Pantalla[ 45],255,255,0);tira.setPixelColor(Pantalla[ 46],105,210,30);tira.setPixelColor(Pantalla[ 47],0,0,255);
tira.setPixelColor(Pantalla[ 63],105,210,30);tira.setPixelColor(Pantalla[ 62],255,255,0);tira.setPixelColor(Pantalla[ 61],0,0,255);tira.setPixelColor(Pantalla[ 60],100,100,100);tira.setPixelColor(Pantalla[ 59],100,100,100);tira.setPixelColor(Pantalla[ 58],100,100,100);tira.setPixelColor(Pantalla[ 57],100,100,100);tira.setPixelColor(Pantalla[ 56],100,100,100);tira.setPixelColor(Pantalla[ 55],100,100,100);tira.setPixelColor(Pantalla[ 54],100,100,100);tira.setPixelColor(Pantalla[ 53],100,100,100);tira.setPixelColor(Pantalla[ 52],100,100,100);tira.setPixelColor(Pantalla[ 51],100,100,100);tira.setPixelColor(Pantalla[ 50],0,0,255);tira.setPixelColor(Pantalla[ 49],0,0,255);tira.setPixelColor(Pantalla[ 48],105,210,30);
tira.setPixelColor(Pantalla[ 64],255,255,0);tira.setPixelColor(Pantalla[ 65],0,0,255);tira.setPixelColor(Pantalla[ 66],100,100,100);tira.setPixelColor(Pantalla[ 67],100,100,100);tira.setPixelColor(Pantalla[ 68],100,100,100);tira.setPixelColor(Pantalla[ 69],100,100,100);tira.setPixelColor(Pantalla[ 70],100,100,100);tira.setPixelColor(Pantalla[ 71],100,100,100);tira.setPixelColor(Pantalla[ 72],100,100,100);tira.setPixelColor(Pantalla[ 73],100,100,100);tira.setPixelColor(Pantalla[ 74],100,100,100);tira.setPixelColor(Pantalla[ 75],100,100,100);tira.setPixelColor(Pantalla[ 76],100,100,100);tira.setPixelColor(Pantalla[ 77],100,100,100);tira.setPixelColor(Pantalla[ 78],0,0,255);tira.setPixelColor(Pantalla[ 79],105,210,30);
tira.setPixelColor(Pantalla[ 95],105,210,30);tira.setPixelColor(Pantalla[ 94],0,0,255);tira.setPixelColor(Pantalla[ 93],100,100,100);tira.setPixelColor(Pantalla[ 92],100,100,100);tira.setPixelColor(Pantalla[ 91],100,100,100);tira.setPixelColor(Pantalla[ 90],100,100,100);tira.setPixelColor(Pantalla[ 89],100,100,100);tira.setPixelColor(Pantalla[ 88],100,100,100);tira.setPixelColor(Pantalla[ 87],100,100,100);tira.setPixelColor(Pantalla[ 86],100,100,100);tira.setPixelColor(Pantalla[ 85],100,100,100);tira.setPixelColor(Pantalla[ 84],100,100,100);tira.setPixelColor(Pantalla[ 83],100,100,100);tira.setPixelColor(Pantalla[ 82],100,100,100);tira.setPixelColor(Pantalla[ 81],0,0,255);tira.setPixelColor(Pantalla[ 80],255,255,0);
tira.setPixelColor(Pantalla[ 96],255,255,0);tira.setPixelColor(Pantalla[ 97],0,0,255);tira.setPixelColor(Pantalla[ 98],100,100,100);tira.setPixelColor(Pantalla[ 99],100,100,100);tira.setPixelColor(Pantalla[ 100],100,100,100);tira.setPixelColor(Pantalla[ 101],100,100,100);tira.setPixelColor(Pantalla[ 102],100,100,100);tira.setPixelColor(Pantalla[ 103],100,100,100);tira.setPixelColor(Pantalla[ 104],100,100,100);tira.setPixelColor(Pantalla[ 105],100,100,100);tira.setPixelColor(Pantalla[ 106],100,100,100);tira.setPixelColor(Pantalla[ 107],100,100,100);tira.setPixelColor(Pantalla[ 108],100,100,100);tira.setPixelColor(Pantalla[ 109],100,100,100);tira.setPixelColor(Pantalla[ 110],0,0,255);tira.setPixelColor(Pantalla[ 111],255,255,0);
tira.setPixelColor(Pantalla[ 127],0,0,255);tira.setPixelColor(Pantalla[ 126],0,0,255);tira.setPixelColor(Pantalla[ 125],100,100,100);tira.setPixelColor(Pantalla[ 124],100,100,100);tira.setPixelColor(Pantalla[ 123],100,100,100);tira.setPixelColor(Pantalla[ 122],100,100,100);tira.setPixelColor(Pantalla[ 121],100,100,100);tira.setPixelColor(Pantalla[ 120],100,100,100);tira.setPixelColor(Pantalla[ 119],100,100,100);tira.setPixelColor(Pantalla[ 118],100,100,100);tira.setPixelColor(Pantalla[ 117],100,100,100);tira.setPixelColor(Pantalla[ 116],100,100,100);tira.setPixelColor(Pantalla[ 115],100,100,100);tira.setPixelColor(Pantalla[ 114],100,100,100);tira.setPixelColor(Pantalla[ 113],100,100,100);tira.setPixelColor(Pantalla[ 112],0,0,255);
tira.setPixelColor(Pantalla[ 128],255,255,0);tira.setPixelColor(Pantalla[ 129],0,0,255);tira.setPixelColor(Pantalla[ 130],100,100,100);tira.setPixelColor(Pantalla[ 131],100,100,100);tira.setPixelColor(Pantalla[ 132],100,100,100);tira.setPixelColor(Pantalla[ 133],100,100,100);tira.setPixelColor(Pantalla[ 134],100,100,100);tira.setPixelColor(Pantalla[ 135],100,100,100);tira.setPixelColor(Pantalla[ 136],100,100,100);tira.setPixelColor(Pantalla[ 137],100,100,100);tira.setPixelColor(Pantalla[ 138],100,100,100);tira.setPixelColor(Pantalla[ 139],100,100,100);tira.setPixelColor(Pantalla[ 140],100,100,100);tira.setPixelColor(Pantalla[ 141],100,100,100);tira.setPixelColor(Pantalla[ 142],100,100,100);tira.setPixelColor(Pantalla[ 143],105,210,30);
tira.setPixelColor(Pantalla[ 159],255,255,0);tira.setPixelColor(Pantalla[ 158],0,0,255);tira.setPixelColor(Pantalla[ 157],100,100,100);tira.setPixelColor(Pantalla[ 156],100,100,100);tira.setPixelColor(Pantalla[ 155],100,100,100);tira.setPixelColor(Pantalla[ 154],100,100,100);tira.setPixelColor(Pantalla[ 153],100,100,100);tira.setPixelColor(Pantalla[ 152],100,100,100);tira.setPixelColor(Pantalla[ 151],100,100,100);tira.setPixelColor(Pantalla[ 150],100,100,100);tira.setPixelColor(Pantalla[ 149],100,100,100);tira.setPixelColor(Pantalla[ 148],100,100,100);tira.setPixelColor(Pantalla[ 147],100,100,100);tira.setPixelColor(Pantalla[ 146],100,100,100);tira.setPixelColor(Pantalla[ 145],0,0,255);tira.setPixelColor(Pantalla[ 144],255,255,0);
tira.setPixelColor(Pantalla[ 160],0,0,255);tira.setPixelColor(Pantalla[ 161],0,0,255);tira.setPixelColor(Pantalla[ 162],100,100,100);tira.setPixelColor(Pantalla[ 163],100,100,100);tira.setPixelColor(Pantalla[ 164],100,100,100);tira.setPixelColor(Pantalla[ 165],100,100,100);tira.setPixelColor(Pantalla[ 166],100,100,100);tira.setPixelColor(Pantalla[ 167],100,100,100);tira.setPixelColor(Pantalla[ 168],100,100,100);tira.setPixelColor(Pantalla[ 169],100,100,100);tira.setPixelColor(Pantalla[ 170],100,100,100);tira.setPixelColor(Pantalla[ 171],100,100,100);tira.setPixelColor(Pantalla[ 172],100,100,100);tira.setPixelColor(Pantalla[ 173],0,0,255);tira.setPixelColor(Pantalla[ 174],255,255,0);tira.setPixelColor(Pantalla[ 175],105,210,30);
tira.setPixelColor(Pantalla[ 191],0,0,255);tira.setPixelColor(Pantalla[ 190],105,210,30);tira.setPixelColor(Pantalla[ 189],0,0,255);tira.setPixelColor(Pantalla[ 188],0,0,255);tira.setPixelColor(Pantalla[ 187],100,100,100);tira.setPixelColor(Pantalla[ 186],100,100,100);tira.setPixelColor(Pantalla[ 185],100,100,100);tira.setPixelColor(Pantalla[ 184],100,100,100);tira.setPixelColor(Pantalla[ 183],100,100,100);tira.setPixelColor(Pantalla[ 182],100,100,100);tira.setPixelColor(Pantalla[ 181],100,100,100);tira.setPixelColor(Pantalla[ 180],100,100,100);tira.setPixelColor(Pantalla[ 179],100,100,100);tira.setPixelColor(Pantalla[ 178],0,0,255);tira.setPixelColor(Pantalla[ 177],255,255,0);tira.setPixelColor(Pantalla[ 176],255,0,255);
tira.setPixelColor(Pantalla[ 192],255,0,255);tira.setPixelColor(Pantalla[ 193],100,100,100);tira.setPixelColor(Pantalla[ 194],105,210,30);tira.setPixelColor(Pantalla[ 195],255,255,0);tira.setPixelColor(Pantalla[ 196],0,0,255);tira.setPixelColor(Pantalla[ 197],0,0,255);tira.setPixelColor(Pantalla[ 198],100,100,100);tira.setPixelColor(Pantalla[ 199],100,100,100);tira.setPixelColor(Pantalla[ 200],100,100,100);tira.setPixelColor(Pantalla[ 201],100,100,100);tira.setPixelColor(Pantalla[ 202],100,100,100);tira.setPixelColor(Pantalla[ 203],0,0,255);tira.setPixelColor(Pantalla[ 204],0,0,255);tira.setPixelColor(Pantalla[ 205],255,255,0);tira.setPixelColor(Pantalla[ 206],255,255,0);tira.setPixelColor(Pantalla[ 207],255,0,255);
tira.setPixelColor(Pantalla[ 223],255,0,255);tira.setPixelColor(Pantalla[ 222],100,100,100);tira.setPixelColor(Pantalla[ 221],100,100,100);tira.setPixelColor(Pantalla[ 220],105,210,30);tira.setPixelColor(Pantalla[ 219],105,210,30);tira.setPixelColor(Pantalla[ 218],255,255,0);tira.setPixelColor(Pantalla[ 217],0,0,255);tira.setPixelColor(Pantalla[ 216],0,0,255);tira.setPixelColor(Pantalla[ 215],105,210,30);tira.setPixelColor(Pantalla[ 214],0,0,255);tira.setPixelColor(Pantalla[ 213],0,0,255);tira.setPixelColor(Pantalla[ 212],0,0,255);tira.setPixelColor(Pantalla[ 211],105,210,30);tira.setPixelColor(Pantalla[ 210],100,100,100);tira.setPixelColor(Pantalla[ 209],100,100,100);tira.setPixelColor(Pantalla[ 208],255,0,255);
tira.setPixelColor(Pantalla[ 224],255,0,255);tira.setPixelColor(Pantalla[ 225],255,0,255);tira.setPixelColor(Pantalla[ 226],100,100,100);tira.setPixelColor(Pantalla[ 227],255,0,255);tira.setPixelColor(Pantalla[ 228],0,0,255);tira.setPixelColor(Pantalla[ 229],100,100,100);tira.setPixelColor(Pantalla[ 230],105,210,30);tira.setPixelColor(Pantalla[ 231],255,255,0);tira.setPixelColor(Pantalla[ 232],0,0,255);tira.setPixelColor(Pantalla[ 233],105,210,30);tira.setPixelColor(Pantalla[ 234],255,255,0);tira.setPixelColor(Pantalla[ 235],0,0,255);tira.setPixelColor(Pantalla[ 236],105,210,30);tira.setPixelColor(Pantalla[ 237],255,0,255);tira.setPixelColor(Pantalla[ 238],255,0,255);tira.setPixelColor(Pantalla[ 239],0,0,255);
tira.setPixelColor(Pantalla[ 255],0,0,255);tira.setPixelColor(Pantalla[ 254],0,0,255);tira.setPixelColor(Pantalla[ 253],255,0,255);tira.setPixelColor(Pantalla[ 252],0,0,255);tira.setPixelColor(Pantalla[ 251],0,0,255);tira.setPixelColor(Pantalla[ 250],0,0,255);tira.setPixelColor(Pantalla[ 249],100,100,100);tira.setPixelColor(Pantalla[ 248],100,100,100);tira.setPixelColor(Pantalla[ 247],100,100,100);tira.setPixelColor(Pantalla[ 246],100,100,100);tira.setPixelColor(Pantalla[ 245],0,0,255);tira.setPixelColor(Pantalla[ 244],0,0,255);tira.setPixelColor(Pantalla[ 243],0,0,255);tira.setPixelColor(Pantalla[ 242],0,0,255);tira.setPixelColor(Pantalla[ 241],0,0,255);tira.setPixelColor(Pantalla[ 240],0,0,255);

}

void Matriz_Aro_5_Disparo()
{
  tira.setPixelColor(Pantalla[ 0],100,100,100);tira.setPixelColor(Pantalla[ 1],100,100,100);tira.setPixelColor(Pantalla[ 2],255,0,255);tira.setPixelColor(Pantalla[ 3],255,0,255);tira.setPixelColor(Pantalla[ 4],105,210,30);tira.setPixelColor(Pantalla[ 5],105,210,30);tira.setPixelColor(Pantalla[ 6],105,210,30);tira.setPixelColor(Pantalla[ 7],105,210,30);tira.setPixelColor(Pantalla[ 8],0,0,255);tira.setPixelColor(Pantalla[ 9],0,0,255);tira.setPixelColor(Pantalla[ 10],105,210,30);tira.setPixelColor(Pantalla[ 11],105,210,30);tira.setPixelColor(Pantalla[ 12],105,210,30);tira.setPixelColor(Pantalla[ 13],100,100,100);tira.setPixelColor(Pantalla[ 14],100,100,100);tira.setPixelColor(Pantalla[ 15],100,100,100);
  tira.setPixelColor(Pantalla[ 31],255,0,255);tira.setPixelColor(Pantalla[ 30],105,210,30);tira.setPixelColor(Pantalla[ 29],105,210,30);tira.setPixelColor(Pantalla[ 28],0,0,255);tira.setPixelColor(Pantalla[ 27],100,100,100);tira.setPixelColor(Pantalla[ 26],100,100,100);tira.setPixelColor(Pantalla[ 25],100,100,100);tira.setPixelColor(Pantalla[ 24],100,100,100);tira.setPixelColor(Pantalla[ 23],100,100,100);tira.setPixelColor(Pantalla[ 22],100,100,100);tira.setPixelColor(Pantalla[ 21],0,0,255);tira.setPixelColor(Pantalla[ 20],0,0,255);tira.setPixelColor(Pantalla[ 19],0,0,255);tira.setPixelColor(Pantalla[ 18],105,210,30);tira.setPixelColor(Pantalla[ 17],100,100,100);tira.setPixelColor(Pantalla[ 16],100,100,100);
  tira.setPixelColor(Pantalla[ 32],0,0,255);tira.setPixelColor(Pantalla[ 33],105,210,30);tira.setPixelColor(Pantalla[ 34],0,0,255);tira.setPixelColor(Pantalla[ 35],100,100,100);tira.setPixelColor(Pantalla[ 36],100,100,100);tira.setPixelColor(Pantalla[ 37],100,100,100);tira.setPixelColor(Pantalla[ 38],100,100,100);tira.setPixelColor(Pantalla[ 39],100,100,100);tira.setPixelColor(Pantalla[ 40],100,100,100);tira.setPixelColor(Pantalla[ 41],100,100,100);tira.setPixelColor(Pantalla[ 42],100,100,100);tira.setPixelColor(Pantalla[ 43],100,100,100);tira.setPixelColor(Pantalla[ 44],100,100,100);tira.setPixelColor(Pantalla[ 45],105,210,30);tira.setPixelColor(Pantalla[ 46],105,210,30);tira.setPixelColor(Pantalla[ 47],0,0,255);
  tira.setPixelColor(Pantalla[ 63],105,210,30);tira.setPixelColor(Pantalla[ 62],0,0,255);tira.setPixelColor(Pantalla[ 61],100,100,100);tira.setPixelColor(Pantalla[ 60],100,100,100);tira.setPixelColor(Pantalla[ 59],100,100,100);tira.setPixelColor(Pantalla[ 58],100,100,100);tira.setPixelColor(Pantalla[ 57],100,100,100);tira.setPixelColor(Pantalla[ 56],100,100,100);tira.setPixelColor(Pantalla[ 55],100,100,100);tira.setPixelColor(Pantalla[ 54],100,100,100);tira.setPixelColor(Pantalla[ 53],100,100,100);tira.setPixelColor(Pantalla[ 52],100,100,100);tira.setPixelColor(Pantalla[ 51],100,100,100);tira.setPixelColor(Pantalla[ 50],255,0,255);tira.setPixelColor(Pantalla[ 49],105,210,30);tira.setPixelColor(Pantalla[ 48],105,210,30);
  tira.setPixelColor(Pantalla[ 64],105,210,30);tira.setPixelColor(Pantalla[ 65],0,0,255);tira.setPixelColor(Pantalla[ 66],100,100,100);tira.setPixelColor(Pantalla[ 67],100,100,100);tira.setPixelColor(Pantalla[ 68],100,100,100);tira.setPixelColor(Pantalla[ 69],100,100,100);tira.setPixelColor(Pantalla[ 70],100,100,100);tira.setPixelColor(Pantalla[ 71],0,255,0);tira.setPixelColor(Pantalla[ 72],0,255,0);tira.setPixelColor(Pantalla[ 73],100,100,100);tira.setPixelColor(Pantalla[ 74],100,100,100);tira.setPixelColor(Pantalla[ 75],100,100,100);tira.setPixelColor(Pantalla[ 76],100,100,100);tira.setPixelColor(Pantalla[ 77],100,100,100);tira.setPixelColor(Pantalla[ 78],0,0,255);tira.setPixelColor(Pantalla[ 79],105,210,30);
  tira.setPixelColor(Pantalla[ 95],0,0,255);tira.setPixelColor(Pantalla[ 94],100,100,100);tira.setPixelColor(Pantalla[ 93],100,100,100);tira.setPixelColor(Pantalla[ 92],100,100,100);tira.setPixelColor(Pantalla[ 91],100,100,100);tira.setPixelColor(Pantalla[ 90],100,100,100);tira.setPixelColor(Pantalla[ 89],0,255,0);tira.setPixelColor(Pantalla[ 88],0,255,0);tira.setPixelColor(Pantalla[ 87],0,255,0);tira.setPixelColor(Pantalla[ 86],0,255,0);tira.setPixelColor(Pantalla[ 85],100,100,100);tira.setPixelColor(Pantalla[ 84],100,100,100);tira.setPixelColor(Pantalla[ 83],100,100,100);tira.setPixelColor(Pantalla[ 82],100,100,100);tira.setPixelColor(Pantalla[ 81],0,0,255);tira.setPixelColor(Pantalla[ 80],105,210,30);
  tira.setPixelColor(Pantalla[ 96],105,210,30);tira.setPixelColor(Pantalla[ 97],100,100,100);tira.setPixelColor(Pantalla[ 98],100,100,100);tira.setPixelColor(Pantalla[ 99],100,100,100);tira.setPixelColor(Pantalla[ 100],100,100,100);tira.setPixelColor(Pantalla[ 101],0,255,0);tira.setPixelColor(Pantalla[ 102],0,255,0);tira.setPixelColor(Pantalla[ 103],0,0,0);tira.setPixelColor(Pantalla[ 104],0,0,0);tira.setPixelColor(Pantalla[ 105],0,255,0);tira.setPixelColor(Pantalla[ 106],0,255,0);tira.setPixelColor(Pantalla[ 107],100,100,100);tira.setPixelColor(Pantalla[ 108],100,100,100);tira.setPixelColor(Pantalla[ 109],100,100,100);tira.setPixelColor(Pantalla[ 110],100,100,100);tira.setPixelColor(Pantalla[ 111],105,210,30);
  tira.setPixelColor(Pantalla[ 127],105,210,30);tira.setPixelColor(Pantalla[ 126],100,100,100);tira.setPixelColor(Pantalla[ 125],100,100,100);tira.setPixelColor(Pantalla[ 124],100,100,100);tira.setPixelColor(Pantalla[ 123],100,100,100);tira.setPixelColor(Pantalla[ 122],0,255,0);tira.setPixelColor(Pantalla[ 121],0,0,0);tira.setPixelColor(Pantalla[ 120],0,0,0);tira.setPixelColor(Pantalla[ 119],0,0,0);tira.setPixelColor(Pantalla[ 118],0,0,0);tira.setPixelColor(Pantalla[ 117],0,255,0);tira.setPixelColor(Pantalla[ 116],100,100,100);tira.setPixelColor(Pantalla[ 115],100,100,100);tira.setPixelColor(Pantalla[ 114],100,100,100);tira.setPixelColor(Pantalla[ 113],100,100,100);tira.setPixelColor(Pantalla[ 112],105,210,30);
  tira.setPixelColor(Pantalla[ 128],105,210,30);tira.setPixelColor(Pantalla[ 129],100,100,100);tira.setPixelColor(Pantalla[ 130],100,100,100);tira.setPixelColor(Pantalla[ 131],100,100,100);tira.setPixelColor(Pantalla[ 132],100,100,100);tira.setPixelColor(Pantalla[ 133],0,255,0);tira.setPixelColor(Pantalla[ 134],0,0,0);tira.setPixelColor(Pantalla[ 135],0,0,0);tira.setPixelColor(Pantalla[ 136],0,0,0);tira.setPixelColor(Pantalla[ 137],0,0,0);tira.setPixelColor(Pantalla[ 138],0,255,0);tira.setPixelColor(Pantalla[ 139],100,100,100);tira.setPixelColor(Pantalla[ 140],100,100,100);tira.setPixelColor(Pantalla[ 141],100,100,100);tira.setPixelColor(Pantalla[ 142],100,100,100);tira.setPixelColor(Pantalla[ 143],105,210,30);
  tira.setPixelColor(Pantalla[ 159],105,210,30);tira.setPixelColor(Pantalla[ 158],0,0,255);tira.setPixelColor(Pantalla[ 157],100,100,100);tira.setPixelColor(Pantalla[ 156],100,100,100);tira.setPixelColor(Pantalla[ 155],100,100,100);tira.setPixelColor(Pantalla[ 154],0,255,0);tira.setPixelColor(Pantalla[ 153],0,255,0);tira.setPixelColor(Pantalla[ 152],0,0,0);tira.setPixelColor(Pantalla[ 151],0,0,0);tira.setPixelColor(Pantalla[ 150],0,255,0);tira.setPixelColor(Pantalla[ 149],100,100,100);tira.setPixelColor(Pantalla[ 148],100,100,100);tira.setPixelColor(Pantalla[ 147],100,100,100);tira.setPixelColor(Pantalla[ 146],100,100,100);tira.setPixelColor(Pantalla[ 145],0,0,255);tira.setPixelColor(Pantalla[ 144],105,210,30);
  tira.setPixelColor(Pantalla[ 160],0,0,255);tira.setPixelColor(Pantalla[ 161],105,210,30);tira.setPixelColor(Pantalla[ 162],0,0,255);tira.setPixelColor(Pantalla[ 163],100,100,100);tira.setPixelColor(Pantalla[ 164],100,100,100);tira.setPixelColor(Pantalla[ 165],100,100,100);tira.setPixelColor(Pantalla[ 166],100,100,100);tira.setPixelColor(Pantalla[ 167],0,255,0);tira.setPixelColor(Pantalla[ 168],0,255,0);tira.setPixelColor(Pantalla[ 169],100,100,100);tira.setPixelColor(Pantalla[ 170],100,100,100);tira.setPixelColor(Pantalla[ 171],100,100,100);tira.setPixelColor(Pantalla[ 172],100,100,100);tira.setPixelColor(Pantalla[ 173],0,0,255);tira.setPixelColor(Pantalla[ 174],0,0,255);tira.setPixelColor(Pantalla[ 175],105,210,30);
  tira.setPixelColor(Pantalla[ 191],0,0,255);tira.setPixelColor(Pantalla[ 190],105,210,30);tira.setPixelColor(Pantalla[ 189],0,0,255);tira.setPixelColor(Pantalla[ 188],100,100,100);tira.setPixelColor(Pantalla[ 187],100,100,100);tira.setPixelColor(Pantalla[ 186],100,100,100);tira.setPixelColor(Pantalla[ 185],100,100,100);tira.setPixelColor(Pantalla[ 184],100,100,100);tira.setPixelColor(Pantalla[ 183],100,100,100);tira.setPixelColor(Pantalla[ 182],100,100,100);tira.setPixelColor(Pantalla[ 181],100,100,100);tira.setPixelColor(Pantalla[ 180],100,100,100);tira.setPixelColor(Pantalla[ 179],0,0,255);tira.setPixelColor(Pantalla[ 178],0,0,255);tira.setPixelColor(Pantalla[ 177],105,210,30);tira.setPixelColor(Pantalla[ 176],105,210,30);
  tira.setPixelColor(Pantalla[ 192],255,0,255);tira.setPixelColor(Pantalla[ 193],100,100,100);tira.setPixelColor(Pantalla[ 194],105,210,30);tira.setPixelColor(Pantalla[ 195],0,0,255);tira.setPixelColor(Pantalla[ 196],100,100,100);tira.setPixelColor(Pantalla[ 197],100,100,100);tira.setPixelColor(Pantalla[ 198],100,100,100);tira.setPixelColor(Pantalla[ 199],100,100,100);tira.setPixelColor(Pantalla[ 200],100,100,100);tira.setPixelColor(Pantalla[ 201],100,100,100);tira.setPixelColor(Pantalla[ 202],100,100,100);tira.setPixelColor(Pantalla[ 203],0,0,255);tira.setPixelColor(Pantalla[ 204],105,210,30);tira.setPixelColor(Pantalla[ 205],105,210,30);tira.setPixelColor(Pantalla[ 206],100,100,100);tira.setPixelColor(Pantalla[ 207],255,0,255);
  tira.setPixelColor(Pantalla[ 223],255,0,255);tira.setPixelColor(Pantalla[ 222],100,100,100);tira.setPixelColor(Pantalla[ 221],100,100,100);tira.setPixelColor(Pantalla[ 220],105,210,30);tira.setPixelColor(Pantalla[ 219],0,0,255);tira.setPixelColor(Pantalla[ 218],100,100,100);tira.setPixelColor(Pantalla[ 217],100,100,100);tira.setPixelColor(Pantalla[ 216],100,100,100);tira.setPixelColor(Pantalla[ 215],100,100,100);tira.setPixelColor(Pantalla[ 214],100,100,100);tira.setPixelColor(Pantalla[ 213],0,0,255);tira.setPixelColor(Pantalla[ 212],105,210,30);tira.setPixelColor(Pantalla[ 211],105,210,30);tira.setPixelColor(Pantalla[ 210],100,100,100);tira.setPixelColor(Pantalla[ 209],100,100,100);tira.setPixelColor(Pantalla[ 208],255,0,255);
  tira.setPixelColor(Pantalla[ 224],255,0,255);tira.setPixelColor(Pantalla[ 225],255,0,255);tira.setPixelColor(Pantalla[ 226],100,100,100);tira.setPixelColor(Pantalla[ 227],105,210,30);tira.setPixelColor(Pantalla[ 228],0,0,255);tira.setPixelColor(Pantalla[ 229],0,0,255);tira.setPixelColor(Pantalla[ 230],100,100,100);tira.setPixelColor(Pantalla[ 231],100,100,100);tira.setPixelColor(Pantalla[ 232],0,0,255);tira.setPixelColor(Pantalla[ 233],0,0,255);tira.setPixelColor(Pantalla[ 234],105,210,30);tira.setPixelColor(Pantalla[ 235],105,210,30);tira.setPixelColor(Pantalla[ 236],255,0,255);tira.setPixelColor(Pantalla[ 237],255,0,255);tira.setPixelColor(Pantalla[ 238],255,0,255);tira.setPixelColor(Pantalla[ 239],0,0,255);
  tira.setPixelColor(Pantalla[ 255],0,0,255);tira.setPixelColor(Pantalla[ 254],0,0,255);tira.setPixelColor(Pantalla[ 253],255,0,255);tira.setPixelColor(Pantalla[ 252],0,0,255);tira.setPixelColor(Pantalla[ 251],105,210,30);tira.setPixelColor(Pantalla[ 250],105,210,30);tira.setPixelColor(Pantalla[ 249],105,210,30);tira.setPixelColor(Pantalla[ 248],105,210,30);tira.setPixelColor(Pantalla[ 247],105,210,30);tira.setPixelColor(Pantalla[ 246],105,210,30);tira.setPixelColor(Pantalla[ 245],0,0,255);tira.setPixelColor(Pantalla[ 244],0,0,255);tira.setPixelColor(Pantalla[ 243],0,0,255);tira.setPixelColor(Pantalla[ 242],0,0,255);tira.setPixelColor(Pantalla[ 241],0,0,255);tira.setPixelColor(Pantalla[ 240],0,0,255);
   
}

void Matriz_Aro_6_Disparo()
{
  tira.setPixelColor(Pantalla[ 0],255,0,255);tira.setPixelColor(Pantalla[ 1],100,100,100);tira.setPixelColor(Pantalla[ 2],255,0,255);tira.setPixelColor(Pantalla[ 3],255,0,255);tira.setPixelColor(Pantalla[ 4],0,0,255);tira.setPixelColor(Pantalla[ 5],0,0,255);tira.setPixelColor(Pantalla[ 6],0,0,255);tira.setPixelColor(Pantalla[ 7],0,0,255);tira.setPixelColor(Pantalla[ 8],0,0,255);tira.setPixelColor(Pantalla[ 9],0,0,255);tira.setPixelColor(Pantalla[ 10],0,0,255);tira.setPixelColor(Pantalla[ 11],0,0,255);tira.setPixelColor(Pantalla[ 12],255,0,255);tira.setPixelColor(Pantalla[ 13],255,0,255);tira.setPixelColor(Pantalla[ 14],0,0,255);tira.setPixelColor(Pantalla[ 15],0,0,255);
tira.setPixelColor(Pantalla[ 31],255,0,255);tira.setPixelColor(Pantalla[ 30],100,100,100);tira.setPixelColor(Pantalla[ 29],100,100,100);tira.setPixelColor(Pantalla[ 28],255,0,255);tira.setPixelColor(Pantalla[ 27],255,0,255);tira.setPixelColor(Pantalla[ 26],255,0,255);tira.setPixelColor(Pantalla[ 25],0,0,255);tira.setPixelColor(Pantalla[ 24],0,255,0);tira.setPixelColor(Pantalla[ 23],0,255,0);tira.setPixelColor(Pantalla[ 22],0,0,255);tira.setPixelColor(Pantalla[ 21],0,0,255);tira.setPixelColor(Pantalla[ 20],255,0,255);tira.setPixelColor(Pantalla[ 19],100,100,100);tira.setPixelColor(Pantalla[ 18],100,100,100);tira.setPixelColor(Pantalla[ 17],255,0,255);tira.setPixelColor(Pantalla[ 16],0,0,255);
tira.setPixelColor(Pantalla[ 32],0,0,255);tira.setPixelColor(Pantalla[ 33],255,0,255);tira.setPixelColor(Pantalla[ 34],100,100,100);tira.setPixelColor(Pantalla[ 35],0,255,0);tira.setPixelColor(Pantalla[ 36],100,100,100);tira.setPixelColor(Pantalla[ 37],255,0,255);tira.setPixelColor(Pantalla[ 38],0,255,0);tira.setPixelColor(Pantalla[ 39],0,255,0);tira.setPixelColor(Pantalla[ 40],0,255,0);tira.setPixelColor(Pantalla[ 41],0,255,0);tira.setPixelColor(Pantalla[ 42],0,255,0);tira.setPixelColor(Pantalla[ 43],100,100,100);tira.setPixelColor(Pantalla[ 44],100,100,100);tira.setPixelColor(Pantalla[ 45],100,100,100);tira.setPixelColor(Pantalla[ 46],255,0,255);tira.setPixelColor(Pantalla[ 47],0,0,255);
tira.setPixelColor(Pantalla[ 63],0,0,255);tira.setPixelColor(Pantalla[ 62],0,255,0);tira.setPixelColor(Pantalla[ 61],255,0,255);tira.setPixelColor(Pantalla[ 60],100,100,100);tira.setPixelColor(Pantalla[ 59],100,100,100);tira.setPixelColor(Pantalla[ 58],0,255,0);tira.setPixelColor(Pantalla[ 57],0,255,0);tira.setPixelColor(Pantalla[ 56],0,255,0);tira.setPixelColor(Pantalla[ 55],0,255,0);tira.setPixelColor(Pantalla[ 54],0,255,0);tira.setPixelColor(Pantalla[ 53],0,255,0);tira.setPixelColor(Pantalla[ 52],0,255,0);tira.setPixelColor(Pantalla[ 51],100,100,100);tira.setPixelColor(Pantalla[ 50],0,255,0);tira.setPixelColor(Pantalla[ 49],0,0,255);tira.setPixelColor(Pantalla[ 48],255,0,255);
tira.setPixelColor(Pantalla[ 64],0,0,255);tira.setPixelColor(Pantalla[ 65],0,0,255);tira.setPixelColor(Pantalla[ 66],255,0,255);tira.setPixelColor(Pantalla[ 67],0,255,0);tira.setPixelColor(Pantalla[ 68],0,255,0);tira.setPixelColor(Pantalla[ 69],0,255,0);tira.setPixelColor(Pantalla[ 70],0,255,0);tira.setPixelColor(Pantalla[ 71],0,255,0);tira.setPixelColor(Pantalla[ 72],0,255,0);tira.setPixelColor(Pantalla[ 73],0,255,0);tira.setPixelColor(Pantalla[ 74],0,255,0);tira.setPixelColor(Pantalla[ 75],0,255,0);tira.setPixelColor(Pantalla[ 76],0,255,0);tira.setPixelColor(Pantalla[ 77],255,0,255);tira.setPixelColor(Pantalla[ 78],0,0,255);tira.setPixelColor(Pantalla[ 79],0,0,255);
tira.setPixelColor(Pantalla[ 95],0,0,255);tira.setPixelColor(Pantalla[ 94],255,0,255);tira.setPixelColor(Pantalla[ 93],255,0,255);tira.setPixelColor(Pantalla[ 92],0,255,0);tira.setPixelColor(Pantalla[ 91],0,255,0);tira.setPixelColor(Pantalla[ 90],0,255,0);tira.setPixelColor(Pantalla[ 89],0,255,0);tira.setPixelColor(Pantalla[ 88],0,0,0);tira.setPixelColor(Pantalla[ 87],0,0,0);tira.setPixelColor(Pantalla[ 86],0,0,0);tira.setPixelColor(Pantalla[ 85],0,255,0);tira.setPixelColor(Pantalla[ 84],0,255,0);tira.setPixelColor(Pantalla[ 83],0,255,0);tira.setPixelColor(Pantalla[ 82],0,255,0);tira.setPixelColor(Pantalla[ 81],255,0,255);tira.setPixelColor(Pantalla[ 80],0,0,255);
tira.setPixelColor(Pantalla[ 96],255,0,255);tira.setPixelColor(Pantalla[ 97],255,0,255);tira.setPixelColor(Pantalla[ 98],0,255,0);tira.setPixelColor(Pantalla[ 99],0,255,0);tira.setPixelColor(Pantalla[ 100],0,255,0);tira.setPixelColor(Pantalla[ 101],0,255,0);tira.setPixelColor(Pantalla[ 102],0,0,0);tira.setPixelColor(Pantalla[ 103],0,0,0);tira.setPixelColor(Pantalla[ 104],0,0,0);tira.setPixelColor(Pantalla[ 105],0,0,0);tira.setPixelColor(Pantalla[ 106],0,0,0);tira.setPixelColor(Pantalla[ 107],0,255,0);tira.setPixelColor(Pantalla[ 108],0,255,0);tira.setPixelColor(Pantalla[ 109],100,100,100);tira.setPixelColor(Pantalla[ 110],255,0,255);tira.setPixelColor(Pantalla[ 111],255,0,255);
tira.setPixelColor(Pantalla[ 127],0,0,255);tira.setPixelColor(Pantalla[ 126],0,255,0);tira.setPixelColor(Pantalla[ 125],100,100,100);tira.setPixelColor(Pantalla[ 124],100,100,100);tira.setPixelColor(Pantalla[ 123],0,255,0);tira.setPixelColor(Pantalla[ 122],0,255,0);tira.setPixelColor(Pantalla[ 121],0,0,0);tira.setPixelColor(Pantalla[ 120],0,0,0);tira.setPixelColor(Pantalla[ 119],0,0,0);tira.setPixelColor(Pantalla[ 118],0,0,0);tira.setPixelColor(Pantalla[ 117],0,0,0);tira.setPixelColor(Pantalla[ 116],0,0,0);tira.setPixelColor(Pantalla[ 115],0,255,0);tira.setPixelColor(Pantalla[ 114],0,255,0);tira.setPixelColor(Pantalla[ 113],255,0,255);tira.setPixelColor(Pantalla[ 112],255,0,255);
tira.setPixelColor(Pantalla[ 128],0,0,255);tira.setPixelColor(Pantalla[ 129],0,255,0);tira.setPixelColor(Pantalla[ 130],255,0,255);tira.setPixelColor(Pantalla[ 131],0,255,0);tira.setPixelColor(Pantalla[ 132],0,255,0);tira.setPixelColor(Pantalla[ 133],0,255,0);tira.setPixelColor(Pantalla[ 134],0,0,0);tira.setPixelColor(Pantalla[ 135],0,0,0);tira.setPixelColor(Pantalla[ 136],0,0,0);tira.setPixelColor(Pantalla[ 137],0,0,0);tira.setPixelColor(Pantalla[ 138],0,0,0);tira.setPixelColor(Pantalla[ 139],0,255,0);tira.setPixelColor(Pantalla[ 140],0,255,0);tira.setPixelColor(Pantalla[ 141],0,255,0);tira.setPixelColor(Pantalla[ 142],255,0,255);tira.setPixelColor(Pantalla[ 143],0,0,255);
tira.setPixelColor(Pantalla[ 159],0,0,255);tira.setPixelColor(Pantalla[ 158],0,0,255);tira.setPixelColor(Pantalla[ 157],0,255,0);tira.setPixelColor(Pantalla[ 156],255,0,255);tira.setPixelColor(Pantalla[ 155],0,255,0);tira.setPixelColor(Pantalla[ 154],0,255,0);tira.setPixelColor(Pantalla[ 153],0,0,0);tira.setPixelColor(Pantalla[ 152],0,0,0);tira.setPixelColor(Pantalla[ 151],0,0,0);tira.setPixelColor(Pantalla[ 150],0,0,0);tira.setPixelColor(Pantalla[ 149],0,255,0);tira.setPixelColor(Pantalla[ 148],0,255,0);tira.setPixelColor(Pantalla[ 147],255,0,255);tira.setPixelColor(Pantalla[ 146],255,0,255);tira.setPixelColor(Pantalla[ 145],0,255,0);tira.setPixelColor(Pantalla[ 144],0,0,255);
tira.setPixelColor(Pantalla[ 160],0,0,255);tira.setPixelColor(Pantalla[ 161],0,0,255);tira.setPixelColor(Pantalla[ 162],0,255,0);tira.setPixelColor(Pantalla[ 163],0,255,0);tira.setPixelColor(Pantalla[ 164],100,100,100);tira.setPixelColor(Pantalla[ 165],0,255,0);tira.setPixelColor(Pantalla[ 166],0,255,0);tira.setPixelColor(Pantalla[ 167],0,255,0);tira.setPixelColor(Pantalla[ 168],0,255,0);tira.setPixelColor(Pantalla[ 169],0,255,0);tira.setPixelColor(Pantalla[ 170],0,255,0);tira.setPixelColor(Pantalla[ 171],0,255,0);tira.setPixelColor(Pantalla[ 172],100,100,100);tira.setPixelColor(Pantalla[ 173],255,0,255);tira.setPixelColor(Pantalla[ 174],255,0,255);tira.setPixelColor(Pantalla[ 175],0,0,255);
tira.setPixelColor(Pantalla[ 191],0,0,255);tira.setPixelColor(Pantalla[ 190],255,0,255);tira.setPixelColor(Pantalla[ 189],100,100,100);tira.setPixelColor(Pantalla[ 188],100,100,100);tira.setPixelColor(Pantalla[ 187],100,100,100);tira.setPixelColor(Pantalla[ 186],0,255,0);tira.setPixelColor(Pantalla[ 185],255,0,255);tira.setPixelColor(Pantalla[ 184],0,255,0);tira.setPixelColor(Pantalla[ 183],0,255,0);tira.setPixelColor(Pantalla[ 182],0,255,0);tira.setPixelColor(Pantalla[ 181],0,255,0);tira.setPixelColor(Pantalla[ 180],0,255,0);tira.setPixelColor(Pantalla[ 179],0,255,0);tira.setPixelColor(Pantalla[ 178],0,255,0);tira.setPixelColor(Pantalla[ 177],0,255,0);tira.setPixelColor(Pantalla[ 176],255,0,255);
tira.setPixelColor(Pantalla[ 192],255,0,255);tira.setPixelColor(Pantalla[ 193],100,100,100);tira.setPixelColor(Pantalla[ 194],100,100,100);tira.setPixelColor(Pantalla[ 195],0,255,0);tira.setPixelColor(Pantalla[ 196],0,255,0);tira.setPixelColor(Pantalla[ 197],255,0,255);tira.setPixelColor(Pantalla[ 198],0,255,0);tira.setPixelColor(Pantalla[ 199],100,100,100);tira.setPixelColor(Pantalla[ 200],0,255,0);tira.setPixelColor(Pantalla[ 201],0,255,0);tira.setPixelColor(Pantalla[ 202],0,255,0);tira.setPixelColor(Pantalla[ 203],255,0,255);tira.setPixelColor(Pantalla[ 204],100,100,100);tira.setPixelColor(Pantalla[ 205],100,100,100);tira.setPixelColor(Pantalla[ 206],100,100,100);tira.setPixelColor(Pantalla[ 207],255,0,255);
tira.setPixelColor(Pantalla[ 223],255,0,255);tira.setPixelColor(Pantalla[ 222],100,100,100);tira.setPixelColor(Pantalla[ 221],0,255,0);tira.setPixelColor(Pantalla[ 220],0,255,0);tira.setPixelColor(Pantalla[ 219],0,255,0);tira.setPixelColor(Pantalla[ 218],255,0,255);tira.setPixelColor(Pantalla[ 217],0,0,255);tira.setPixelColor(Pantalla[ 216],255,0,255);tira.setPixelColor(Pantalla[ 215],0,255,0);tira.setPixelColor(Pantalla[ 214],100,100,100);tira.setPixelColor(Pantalla[ 213],255,0,255);tira.setPixelColor(Pantalla[ 212],0,0,255);tira.setPixelColor(Pantalla[ 211],255,0,255);tira.setPixelColor(Pantalla[ 210],0,255,0);tira.setPixelColor(Pantalla[ 209],100,100,100);tira.setPixelColor(Pantalla[ 208],255,0,255);
tira.setPixelColor(Pantalla[ 224],255,0,255);tira.setPixelColor(Pantalla[ 225],255,0,255);tira.setPixelColor(Pantalla[ 226],100,100,100);tira.setPixelColor(Pantalla[ 227],255,0,255);tira.setPixelColor(Pantalla[ 228],0,0,255);tira.setPixelColor(Pantalla[ 229],0,0,255);tira.setPixelColor(Pantalla[ 230],0,0,255);tira.setPixelColor(Pantalla[ 231],255,0,255);tira.setPixelColor(Pantalla[ 232],255,0,255);tira.setPixelColor(Pantalla[ 233],255,0,255);tira.setPixelColor(Pantalla[ 234],255,0,255);tira.setPixelColor(Pantalla[ 235],0,0,255);tira.setPixelColor(Pantalla[ 236],255,0,255);tira.setPixelColor(Pantalla[ 237],255,0,255);tira.setPixelColor(Pantalla[ 238],255,0,255);tira.setPixelColor(Pantalla[ 239],0,0,255);
tira.setPixelColor(Pantalla[ 255],0,0,255);tira.setPixelColor(Pantalla[ 254],0,0,255);tira.setPixelColor(Pantalla[ 253],255,0,255);tira.setPixelColor(Pantalla[ 252],0,0,255);tira.setPixelColor(Pantalla[ 251],0,0,255);tira.setPixelColor(Pantalla[ 250],0,0,255);tira.setPixelColor(Pantalla[ 249],0,0,255);tira.setPixelColor(Pantalla[ 248],0,0,255);tira.setPixelColor(Pantalla[ 247],0,0,255);tira.setPixelColor(Pantalla[ 246],0,0,255);tira.setPixelColor(Pantalla[ 245],0,0,255);tira.setPixelColor(Pantalla[ 244],0,0,255);tira.setPixelColor(Pantalla[ 243],0,0,255);tira.setPixelColor(Pantalla[ 242],0,0,255);tira.setPixelColor(Pantalla[ 241],0,0,255);tira.setPixelColor(Pantalla[ 240],0,0,255);

}

void Matriz_Aro_7_Disparo()
{
  tira.setPixelColor(Pantalla[ 0],0,0,0);tira.setPixelColor(Pantalla[ 1],0,0,0);tira.setPixelColor(Pantalla[ 2],0,0,0);tira.setPixelColor(Pantalla[ 3],0,0,0);tira.setPixelColor(Pantalla[ 4],0,0,0);tira.setPixelColor(Pantalla[ 5],0,255,0);tira.setPixelColor(Pantalla[ 6],0,255,0);tira.setPixelColor(Pantalla[ 7],0,255,0);tira.setPixelColor(Pantalla[ 8],0,255,0);tira.setPixelColor(Pantalla[ 9],0,255,0);tira.setPixelColor(Pantalla[ 10],0,255,0);tira.setPixelColor(Pantalla[ 11],0,0,0);tira.setPixelColor(Pantalla[ 12],0,0,0);tira.setPixelColor(Pantalla[ 13],0,0,0);tira.setPixelColor(Pantalla[ 14],0,255,0);tira.setPixelColor(Pantalla[ 15],0,0,0);
tira.setPixelColor(Pantalla[ 31],0,0,0);tira.setPixelColor(Pantalla[ 30],0,255,0);tira.setPixelColor(Pantalla[ 29],0,255,0);tira.setPixelColor(Pantalla[ 28],0,255,0);tira.setPixelColor(Pantalla[ 27],0,255,0);tira.setPixelColor(Pantalla[ 26],0,255,0);tira.setPixelColor(Pantalla[ 25],0,255,0);tira.setPixelColor(Pantalla[ 24],0,255,0);tira.setPixelColor(Pantalla[ 23],0,255,0);tira.setPixelColor(Pantalla[ 22],0,255,0);tira.setPixelColor(Pantalla[ 21],0,255,0);tira.setPixelColor(Pantalla[ 20],0,255,0);tira.setPixelColor(Pantalla[ 19],0,255,0);tira.setPixelColor(Pantalla[ 18],0,255,0);tira.setPixelColor(Pantalla[ 17],0,0,0);tira.setPixelColor(Pantalla[ 16],0,0,0);
tira.setPixelColor(Pantalla[ 32],0,0,0);tira.setPixelColor(Pantalla[ 33],0,0,0);tira.setPixelColor(Pantalla[ 34],0,255,0);tira.setPixelColor(Pantalla[ 35],0,255,0);tira.setPixelColor(Pantalla[ 36],0,255,0);tira.setPixelColor(Pantalla[ 37],0,255,0);tira.setPixelColor(Pantalla[ 38],0,255,0);tira.setPixelColor(Pantalla[ 39],0,255,0);tira.setPixelColor(Pantalla[ 40],0,255,0);tira.setPixelColor(Pantalla[ 41],0,255,0);tira.setPixelColor(Pantalla[ 42],0,255,0);tira.setPixelColor(Pantalla[ 43],0,255,0);tira.setPixelColor(Pantalla[ 44],0,255,0);tira.setPixelColor(Pantalla[ 45],0,255,0);tira.setPixelColor(Pantalla[ 46],0,0,0);tira.setPixelColor(Pantalla[ 47],0,0,0);
tira.setPixelColor(Pantalla[ 63],0,255,0);tira.setPixelColor(Pantalla[ 62],0,255,0);tira.setPixelColor(Pantalla[ 61],0,255,0);tira.setPixelColor(Pantalla[ 60],0,255,0);tira.setPixelColor(Pantalla[ 59],0,255,0);tira.setPixelColor(Pantalla[ 58],0,255,0);tira.setPixelColor(Pantalla[ 57],0,255,0);tira.setPixelColor(Pantalla[ 56],0,0,0);tira.setPixelColor(Pantalla[ 55],0,0,0);tira.setPixelColor(Pantalla[ 54],0,255,0);tira.setPixelColor(Pantalla[ 53],0,255,0);tira.setPixelColor(Pantalla[ 52],0,255,0);tira.setPixelColor(Pantalla[ 51],0,255,0);tira.setPixelColor(Pantalla[ 50],0,255,0);tira.setPixelColor(Pantalla[ 49],0,255,0);tira.setPixelColor(Pantalla[ 48],0,0,0);
tira.setPixelColor(Pantalla[ 64],0,255,0);tira.setPixelColor(Pantalla[ 65],0,255,0);tira.setPixelColor(Pantalla[ 66],0,255,0);tira.setPixelColor(Pantalla[ 67],0,255,0);tira.setPixelColor(Pantalla[ 68],0,255,0);tira.setPixelColor(Pantalla[ 69],0,0,0);tira.setPixelColor(Pantalla[ 70],0,0,0);tira.setPixelColor(Pantalla[ 71],0,0,0);tira.setPixelColor(Pantalla[ 72],0,0,0);tira.setPixelColor(Pantalla[ 73],0,0,0);tira.setPixelColor(Pantalla[ 74],0,0,0);tira.setPixelColor(Pantalla[ 75],0,255,0);tira.setPixelColor(Pantalla[ 76],0,255,0);tira.setPixelColor(Pantalla[ 77],0,255,0);tira.setPixelColor(Pantalla[ 78],0,255,0);tira.setPixelColor(Pantalla[ 79],0,0,0);
tira.setPixelColor(Pantalla[ 95],0,0,0);tira.setPixelColor(Pantalla[ 94],0,255,0);tira.setPixelColor(Pantalla[ 93],0,255,0);tira.setPixelColor(Pantalla[ 92],0,255,0);tira.setPixelColor(Pantalla[ 91],0,0,0);tira.setPixelColor(Pantalla[ 90],0,0,0);tira.setPixelColor(Pantalla[ 89],0,0,0);tira.setPixelColor(Pantalla[ 88],0,0,0);tira.setPixelColor(Pantalla[ 87],0,0,0);tira.setPixelColor(Pantalla[ 86],0,0,0);tira.setPixelColor(Pantalla[ 85],0,0,0);tira.setPixelColor(Pantalla[ 84],0,0,0);tira.setPixelColor(Pantalla[ 83],0,255,0);tira.setPixelColor(Pantalla[ 82],0,255,0);tira.setPixelColor(Pantalla[ 81],0,255,0);tira.setPixelColor(Pantalla[ 80],0,255,0);
tira.setPixelColor(Pantalla[ 96],0,255,0);tira.setPixelColor(Pantalla[ 97],0,255,0);tira.setPixelColor(Pantalla[ 98],0,255,0);tira.setPixelColor(Pantalla[ 99],0,255,0);tira.setPixelColor(Pantalla[ 100],0,0,0);tira.setPixelColor(Pantalla[ 101],0,0,0);tira.setPixelColor(Pantalla[ 102],0,0,0);tira.setPixelColor(Pantalla[ 103],0,0,0);tira.setPixelColor(Pantalla[ 104],0,0,0);tira.setPixelColor(Pantalla[ 105],0,0,0);tira.setPixelColor(Pantalla[ 106],0,0,0);tira.setPixelColor(Pantalla[ 107],0,0,0);tira.setPixelColor(Pantalla[ 108],0,255,0);tira.setPixelColor(Pantalla[ 109],0,255,0);tira.setPixelColor(Pantalla[ 110],0,255,0);tira.setPixelColor(Pantalla[ 111],0,255,0);
tira.setPixelColor(Pantalla[ 127],0,255,0);tira.setPixelColor(Pantalla[ 126],0,255,0);tira.setPixelColor(Pantalla[ 125],0,255,0);tira.setPixelColor(Pantalla[ 124],0,255,0);tira.setPixelColor(Pantalla[ 123],0,0,0);tira.setPixelColor(Pantalla[ 122],0,0,0);tira.setPixelColor(Pantalla[ 121],0,0,0);tira.setPixelColor(Pantalla[ 120],0,0,0);tira.setPixelColor(Pantalla[ 119],0,0,0);tira.setPixelColor(Pantalla[ 118],0,0,0);tira.setPixelColor(Pantalla[ 117],0,0,0);tira.setPixelColor(Pantalla[ 116],0,0,0);tira.setPixelColor(Pantalla[ 115],0,255,0);tira.setPixelColor(Pantalla[ 114],0,255,0);tira.setPixelColor(Pantalla[ 113],0,255,0);tira.setPixelColor(Pantalla[ 112],0,255,0);
tira.setPixelColor(Pantalla[ 128],0,0,0);tira.setPixelColor(Pantalla[ 129],0,255,0);tira.setPixelColor(Pantalla[ 130],0,255,0);tira.setPixelColor(Pantalla[ 131],0,255,0);tira.setPixelColor(Pantalla[ 132],0,0,0);tira.setPixelColor(Pantalla[ 133],0,0,0);tira.setPixelColor(Pantalla[ 134],0,0,0);tira.setPixelColor(Pantalla[ 135],0,0,0);tira.setPixelColor(Pantalla[ 136],0,0,0);tira.setPixelColor(Pantalla[ 137],0,0,0);tira.setPixelColor(Pantalla[ 138],0,0,0);tira.setPixelColor(Pantalla[ 139],0,0,0);tira.setPixelColor(Pantalla[ 140],0,255,0);tira.setPixelColor(Pantalla[ 141],0,255,0);tira.setPixelColor(Pantalla[ 142],0,255,0);tira.setPixelColor(Pantalla[ 143],0,0,0);
tira.setPixelColor(Pantalla[ 159],0,0,0);tira.setPixelColor(Pantalla[ 158],0,255,0);tira.setPixelColor(Pantalla[ 157],0,255,0);tira.setPixelColor(Pantalla[ 156],0,255,0);tira.setPixelColor(Pantalla[ 155],0,255,0);tira.setPixelColor(Pantalla[ 154],0,0,0);tira.setPixelColor(Pantalla[ 153],0,0,0);tira.setPixelColor(Pantalla[ 152],0,0,0);tira.setPixelColor(Pantalla[ 151],0,0,0);tira.setPixelColor(Pantalla[ 150],0,0,0);tira.setPixelColor(Pantalla[ 149],0,0,0);tira.setPixelColor(Pantalla[ 148],0,255,0);tira.setPixelColor(Pantalla[ 147],0,255,0);tira.setPixelColor(Pantalla[ 146],0,255,0);tira.setPixelColor(Pantalla[ 145],0,255,0);tira.setPixelColor(Pantalla[ 144],0,255,0);
tira.setPixelColor(Pantalla[ 160],0,0,0);tira.setPixelColor(Pantalla[ 161],0,255,0);tira.setPixelColor(Pantalla[ 162],0,255,0);tira.setPixelColor(Pantalla[ 163],0,255,0);tira.setPixelColor(Pantalla[ 164],0,255,0);tira.setPixelColor(Pantalla[ 165],0,255,0);tira.setPixelColor(Pantalla[ 166],0,255,0);tira.setPixelColor(Pantalla[ 167],0,0,0);tira.setPixelColor(Pantalla[ 168],0,0,0);tira.setPixelColor(Pantalla[ 169],0,0,0);tira.setPixelColor(Pantalla[ 170],0,255,0);tira.setPixelColor(Pantalla[ 171],0,255,0);tira.setPixelColor(Pantalla[ 172],0,255,0);tira.setPixelColor(Pantalla[ 173],0,255,0);tira.setPixelColor(Pantalla[ 174],0,0,0);tira.setPixelColor(Pantalla[ 175],0,0,0);
tira.setPixelColor(Pantalla[ 191],0,0,0);tira.setPixelColor(Pantalla[ 190],0,255,0);tira.setPixelColor(Pantalla[ 189],0,255,0);tira.setPixelColor(Pantalla[ 188],0,255,0);tira.setPixelColor(Pantalla[ 187],0,255,0);tira.setPixelColor(Pantalla[ 186],0,255,0);tira.setPixelColor(Pantalla[ 185],0,255,0);tira.setPixelColor(Pantalla[ 184],0,255,0);tira.setPixelColor(Pantalla[ 183],0,255,0);tira.setPixelColor(Pantalla[ 182],0,255,0);tira.setPixelColor(Pantalla[ 181],0,255,0);tira.setPixelColor(Pantalla[ 180],0,255,0);tira.setPixelColor(Pantalla[ 179],0,255,0);tira.setPixelColor(Pantalla[ 178],0,255,0);tira.setPixelColor(Pantalla[ 177],0,0,0);tira.setPixelColor(Pantalla[ 176],0,0,0);
tira.setPixelColor(Pantalla[ 192],0,0,0);tira.setPixelColor(Pantalla[ 193],0,255,0);tira.setPixelColor(Pantalla[ 194],0,255,0);tira.setPixelColor(Pantalla[ 195],0,255,0);tira.setPixelColor(Pantalla[ 196],0,255,0);tira.setPixelColor(Pantalla[ 197],0,255,0);tira.setPixelColor(Pantalla[ 198],0,255,0);tira.setPixelColor(Pantalla[ 199],0,255,0);tira.setPixelColor(Pantalla[ 200],0,255,0);tira.setPixelColor(Pantalla[ 201],0,255,0);tira.setPixelColor(Pantalla[ 202],0,255,0);tira.setPixelColor(Pantalla[ 203],0,255,0);tira.setPixelColor(Pantalla[ 204],0,255,0);tira.setPixelColor(Pantalla[ 205],0,255,0);tira.setPixelColor(Pantalla[ 206],0,0,0);tira.setPixelColor(Pantalla[ 207],0,0,0);
tira.setPixelColor(Pantalla[ 223],0,0,0);tira.setPixelColor(Pantalla[ 222],0,0,0);tira.setPixelColor(Pantalla[ 221],0,0,0);tira.setPixelColor(Pantalla[ 220],0,255,0);tira.setPixelColor(Pantalla[ 219],0,255,0);tira.setPixelColor(Pantalla[ 218],0,255,0);tira.setPixelColor(Pantalla[ 217],0,255,0);tira.setPixelColor(Pantalla[ 216],0,255,0);tira.setPixelColor(Pantalla[ 215],0,255,0);tira.setPixelColor(Pantalla[ 214],0,255,0);tira.setPixelColor(Pantalla[ 213],0,255,0);tira.setPixelColor(Pantalla[ 212],0,255,0);tira.setPixelColor(Pantalla[ 211],0,255,0);tira.setPixelColor(Pantalla[ 210],0,255,0);tira.setPixelColor(Pantalla[ 209],0,255,0);tira.setPixelColor(Pantalla[ 208],0,255,0);
tira.setPixelColor(Pantalla[ 224],0,0,0);tira.setPixelColor(Pantalla[ 225],0,255,0);tira.setPixelColor(Pantalla[ 226],0,255,0);tira.setPixelColor(Pantalla[ 227],0,255,0);tira.setPixelColor(Pantalla[ 228],0,255,0);tira.setPixelColor(Pantalla[ 229],0,0,0);tira.setPixelColor(Pantalla[ 230],0,255,0);tira.setPixelColor(Pantalla[ 231],0,255,0);tira.setPixelColor(Pantalla[ 232],0,255,0);tira.setPixelColor(Pantalla[ 233],0,255,0);tira.setPixelColor(Pantalla[ 234],0,0,0);tira.setPixelColor(Pantalla[ 235],0,255,0);tira.setPixelColor(Pantalla[ 236],0,0,0);tira.setPixelColor(Pantalla[ 237],0,255,0);tira.setPixelColor(Pantalla[ 238],0,255,0);tira.setPixelColor(Pantalla[ 239],0,0,0);
tira.setPixelColor(Pantalla[ 255],0,0,0);tira.setPixelColor(Pantalla[ 254],0,0,0);tira.setPixelColor(Pantalla[ 253],0,0,0);tira.setPixelColor(Pantalla[ 252],0,0,0);tira.setPixelColor(Pantalla[ 251],0,0,0);tira.setPixelColor(Pantalla[ 250],0,0,0);tira.setPixelColor(Pantalla[ 249],0,0,0);tira.setPixelColor(Pantalla[ 248],0,0,0);tira.setPixelColor(Pantalla[ 247],0,0,0);tira.setPixelColor(Pantalla[ 246],0,0,0);tira.setPixelColor(Pantalla[ 245],0,0,0);tira.setPixelColor(Pantalla[ 244],0,0,0);tira.setPixelColor(Pantalla[ 243],0,0,0);tira.setPixelColor(Pantalla[ 242],0,0,0);tira.setPixelColor(Pantalla[ 241],0,0,0);tira.setPixelColor(Pantalla[ 240],0,0,0);

}


void Matriz_Calibra()
{

tira.setPixelColor(Pantalla[ 71],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 88],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 103],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 120],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 135],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 152],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 164],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 167],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 170],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 186],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 184],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 182],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 198],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 199],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 200],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 216],(10+rand()%245),(10+rand()%245),(10+rand()%245));

}
