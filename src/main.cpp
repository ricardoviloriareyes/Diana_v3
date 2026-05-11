// DIANA_V4
/*
BITACORA
VERSION 4
6MAYO 2026
Se tiene problema con los intervalos de lectura de la frecuencia se cambia de paralelo a secuencial
Se cambia estructura de lectura paralela con procesamiento lineal lectura y luego procesamiento
Se cambia estructura a secuencial de lectura freceuncia->analisis->impresion->envio

*/

#include <Arduino.h>
// #include "nvs.h"
// #include "nvs_flash.h"
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
// #include <RTClib.h>
#include <Adafruit_NeoPixel.h>
// # include <Adafruit_BusIO_Register.h>
// # include <Adafruit_I2CDevice.h>
// # include <Adafruit_I2CRegister.h>
// # include <Adafruit_SPIDevice.h>


// direcciones mac de ESTE RECEPTOR (PARA SER CAPTURADA EN EL  MONITOR)
uint8_t broadcastAddress1[] = {0x94, 0x54, 0xC5, 0x73, 0xD7, 0x70};

// Monitor Central para regreso de resultados
uint8_t broadcastAddressMonitor[] = {0x4C, 0xEB, 0xD6, 0x75, 0x48, 0x30};
// 4c:eb:d6:75:48:30


#define NO 0
#define SI 1
#define RECHAZADO 0
#define VALIDO 1

// Colores del disparo
#define COLOR_SIN_ASIGNAR 0
#define GREEN 2
#define BLUE 3
#define RED 1
#define ARCOIRIS 4
uint8_t color_de_disparo = COLOR_SIN_ASIGNAR;

// variables de control de tiempo
volatile unsigned long previous_time_reenvio, current_time_reenvio;
volatile unsigned long espera_tiempo_inicial, espera_tiempo_actual;

// varables para envio de datos a diana
#define STANDBYE 0
#define PREPARA_PAQUETE_ENVIO 1
#define ENVIA_PAQUETE 2
#define PAUSA_REENVIO 3
int intentos_envio = 0;
int8_t case_proceso_envio = STANDBYE;

// logica del proceso del disparo
#define INICIA_STANDBYE 0
#define MONITOREA_DISPARO 2
int8_t case_posicion_del_proceso = INICIA_STANDBYE;

// variables de tipo de figura CD001
#define BICHOS 1
#define CIRCULOS 2
#define NUMEROS 3

// JUGADORES
#define JUGADOR_NO_ACTIVO 0
#define JUGADOR1 1
#define JUGADOR2 2
#define JUGADOR3 3
#define JUGADOR4 4

// ESPERA
#define CERO 0
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

#define APAGADO 0 // CD001
#define ENCENDIDO 1

#define SIN_CAMBIO_ESTADO_RELOJ 0
#define DETECTA_CAMBIO_ESTADO_RELOJ 1
int reloj_1Seg = ENCENDIDO;
int reloj_10mseg =ENCENDIDO;
int reloj_500mseg = ENCENDIDO;
int reloj_250mseg = ENCENDIDO;
int relon_100mseg = ENCENDIDO;
int actualiza_display_1seg = SI;
int actualiza_display_500mseg = SI;
int actualiza_display_200mseg = SI;
int actualiza_display_150mseg = SI;
int actualiza_display_100mseg = SI;
int actualiza_display_10mseg=SI;
uint8_t salir_por_timeout = NO;
uint16_t tiempo_espera_salida = 500; // milisegundos

// Tipos de juegos
#define JUEGO_CLASICO 1
#define JUEGO_TORNEO 2
#define JUEGO_EQUIPOS 3
#define JUEGO_VELOCIDAD 4

// varibles para condiciones de manejo del tiro
#define PRIVADO 0
#define PUBLICO 1
uint8_t habilita_figura =SI;



// estructura de envio de datos al compañero(peer)
typedef struct structura_mensaje
{
  // control
  int n; // Id del paquete

  // propiedades del Juego
  uint8_t ju; // numero del juego 1 JUEGO_CLASICO, 2 JUEGO TORNEO, 3 JUEGO EQUIPOS , 4 JUEGO VELOCIDAD
  uint8_t jr; // numero del round corresponde al round que esta corriendo en cada juego

  // Propiedades del Paquete
  uint8_t t; // test
  uint8_t d; // diana
  uint8_t c; // color GREEN,BLUE,RED,
  uint8_t p; // No. de jugador

  // propiedades del tiro
  uint8_t s; // numero asignado de tiro, sirve para las figuras numeros CD001

  // Condiciones de manejo del tiro
  uint8_t pp; // "0" privado (solo jugador original ) o "1" publico (cualquiera de los dos)
  uint8_t pa; // No. de jugador alternativo en caso de ser tiro publico

  // propiedades de figura
  uint16_t vf; // velocidad de cambio de la figura valores netos como 1500, 2000 o 3000 milisegundos
  uint8_t tf;  // tipo de la figura TEST=0, NORMAL=1, BONO=2, CASTIGO=3
  uint8_t vt;  // se contabiliza el tiro como , NORMAL= 1 tiro, BONO =1 tiro, CASTIGO=0
  uint8_t xf;  // iniciar con figura

  // resultado OBTENIDO del disparo
  uint8_t jg; // No. Jugador Impacto
  int16_t po; // puntuacion obtenida del disparo
  uint8_t fi; // figura impactada para marcador en monitor

} structura_mensaje;

/* fin de importacion*/

structura_mensaje datos_enviados;
structura_mensaje datos_recibidos;


//Almacenamiento de analisis del dito
typedef struct estructura_datos_tiro
{
  //Analiza_Muestras();
  int resultado_de_analisis; //VALIDO, RECHAZADO
  int media_obtenida;
  float porcentaje_desviacion; //0.00 ->sin limite
  //Calcular_Datos_Del_Tiro()
  int valor_del_tiro;   // NO_DETECTADO, APUNTA_VERDE,APUNTA AZUL, DISPARO_VERDE,DISPARO_AZUL,REPETIR_ANALISIS_DE_FRECUENCIA. EN_PROCESO_DE_ENVIO
  int color_del_tiro;   //COLOR_SIN_ASIGNAR, GREEN, BLUE
  int color_del_tirador; //COLOR_SIN_ASIGNAR, GREEN, BLUE 
  //Evalua_Tirador_Correcto()
  int tirador_correcto;  //SI,NO
 // analisis de figura 
  uint8_t tipo_de_figura; // NORMAL, BONO, CATIGO, CALIBRA, TEST
  uint8_t figura_impactada; //OSO, RANA, BOTELLA, CORAZON ... ETC
  int puntos_figura; //-1000 ... +1000
} estructura_datos_tiro;

estructura_datos_tiro datos_del_tiro;






// structura_mensaje datos_locales_diana;  // para usar en diana




#define TEST 0
#define TIRO_ACTIVO 1
#define TIRO_CALIBRA 2


/* 3MAYO2025 SE ANEXA*/

#define NORMAL 1
#define BONO 2
#define CASTIGO 3
#define CALIBRA 4

// valores para Tipos de figuras  a enviar

#define TIPO_TEST 0
#define TIPO_NORMAL 1
#define TIPO_BONO 2
#define TIPO_CASTIGO 3
#define TIPO_CALIBRA 4
uint8_t tipo_de_figura = TIPO_NORMAL;

// valores para tipos de figuras a enviar



// Normal el valor inicial se genera en diana
// el valor se genera de forma local, debe ser diferente a castigo y bonos
//  se genera aqui y el monitor recibe, debe ser el mismo para mostrar la misma



//  figura en el monitor

#define NORMAL_SIN_IMPACTO 13

#define NORMAL_OSO        11
#define NORMAL_ZORRA      10
#define NORMAL_MARIPOSA   9
#define NORMAL_PINGUINO   8
#define NORMAL_ARANA      7
#define NORMAL_LAGARTIJA  6
#define NORMAL_RANA       5

// Bono el valor inicial se genera en monitor y se recibe
// debe ser el mismo en monitor
#define BONO_CORAZON    4
#define BONO_PINGUINO  3
#define BONO_RANA      2
#define BONO_CORAZON_200 1 // el bono corazon 100 se genera de forma local y se envia a monitor

// Castigo el valor inicial se genera en monitor y se recibe
//  debe ser el mismo en monitor
#define CASTIGO_UNICORNIO 16
#define CASTIGO_BOTELLA 15
#define CASTIGO_COPA 14
//Figura calibra
#define CALIBRA_TEST 18


#define FIGURA_SIN_DEFINIR 0

#define PUNTOS_FIGURA_SIN_DEFINIR 0

#define PUNTOS_NORMAL_OSO 50
#define PUNTOS_NORMAL_ZORRA 40
#define PUNTOS_NORMAL_MARIPOSA 45
#define PUNTOS_NORMAL_PIGUINO 75
#define PUNTOS_NORMAL_ARANA 35
#define PUNTOS_NORMAL_LAGARTIJA 30
#define PUNTOS_NORMAL_RANA 50
// Bono
#define PUNTOS_BONO_CORAZON 300
#define PUNTOS_BONO_PINGUINO 200
#define PUNTOS_BONO_RANA 150
#define PUNTOS_BONO_CORAZON_200 100
#define PUNTOS_CALIBRA_0 0
// Castigo
#define PUNTOS_CASTIGO_UNICORNIO -200
#define PUNTOS_CASTIGO_BOTELLA -100
#define PUNTOS_CASTIGO_COPA -70
#define PUNTOS_CASTIGO_SIN_IMPACTO 0
#define PUNTOS_CALIBRA_TEST 0
int posicion_led_apunta=1;

// Calibra
#define CALIBRA_OK 20

// Movimiento de figuras
#define IZQUIERDA 0 // Sentido del movimiento
#define DERECHA 1
uint8_t lado = IZQUIERDA;
uint8_t contador_vueltas = 1;

/*FIN DE ANEXO 3MAYO2025*/

uint8_t tipo_tiro_recibido = TIRO_ACTIVO;
uint16_t no_paquete_test = 2000;
int valor_puntuacion_figura = 0;
// se usa para identificar la figura y mostrar el valor del tiro, se quito de "Evalua_Figura_NOrmal()"
uint8_t figura_impactada = FIGURA_SIN_DEFINIR;

// variable tipo esp_now_info_t para almacenar informacion del compañero
esp_now_peer_info_t peerInfo;

// definicion de io-esp32
const int pin_pulsos = 2; // recepcion de pulsos par calculo de frecuencia
const int pin_leds = 16;  // pin asignado de pcb del  diagrama de electronica diana 3.1
const int pin_led_blanco = 33;

// variable para guardar el pulso de wakeup
int buttonstate = 0;

// definicion de tira led
uint8_t led_inicio = 0;
#define NUMPIXELS 256
#define NUMPIXELS1 262

Adafruit_NeoPixel tira(NUMPIXELS1, pin_leds, NEO_RGB + NEO_KHZ800);
int prende_leds = SI;

/*---------------------------*/
void Espera(int local_millisegundos_de_espera) //Ver 4.0
{
  espera_tiempo_inicial = millis();
  uint8_t local_espera = SI;
  while (local_espera == SI)
  {
    espera_tiempo_actual = millis();
    if ((espera_tiempo_actual - espera_tiempo_inicial) > local_millisegundos_de_espera)
    {
      local_espera = NO;
    }
  }
}
/* -------------------PASO 0 INICIO DE ESP-NOW ------------------*/
void Inicia_ESP_NOW()
{
  Serial.println("Inicia ESPNOW");
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    tira.setPixelColor(0, tira.Color(250, 0, 0));
    tira.show();
    return;
  }
  else
  {
    Serial.println("initializing ESP-NOW  OK");
    tira.setPixelColor(0, tira.Color(0, 0, 250));
    tira.show();
  }
}

/* --------------------PASO 1 LECTURA DE LA MAC -------------------------------------------------*/
// obtiene la direccion mac de  ESTE EQUIPO ESP-32

void readMacAddress() //Ver 4.0
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
  Espera(500);
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    tira.setPixelColor(1, tira.Color(250, 0, 0));
    tira.show();
    Serial.println("Failed to add peer Monitor");
    return;
  }
  else
  {
    tira.setPixelColor(1, tira.Color(0, 0, 250));
    tira.show();
    Serial.println("Add Peer Monitor OK");
  }
}

/* ------------------PASO 3 PRUEBA DE CONEXION ---------------------------------------------------*/
void Test_Conexion_Diana()
{
  // DATOS A ENVIAR
  if (no_paquete_test >= 2101)
  {
    no_paquete_test = 2000;
  }

  datos_enviados.n = no_paquete_test;
  datos_enviados.t = TEST;
  datos_enviados.c = 1;                 // SE MANDA pixel_rojo PARA TEST
  datos_enviados.p = JUGADOR_NO_ACTIVO; // JUGADOR 0 NO ACTIVO

  // monitor
  esp_err_t result4 = esp_now_send(broadcastAddressMonitor, (uint8_t *)&datos_enviados, sizeof(structura_mensaje));
  Espera(500);
  if (result4 == ESP_OK)
  {
    tira.setPixelColor(2, tira.Color(0, 0, 250));
    tira.show();
    Serial.println("TEST-Envio a Monitor OK");
    no_paquete_test++;
  }

  else
  {
    tira.setPixelColor(2, tira.Color(250, 0, 0));
    tira.show();
    Serial.println("TEST-Error enviando a Monitor");
  }
}

/* -------------VARIABLES DE RECEPCION DE DATOS---------------------------------*/

int8_t pixel_verde = 0;
int8_t pixel_rojo = 0;
int8_t pixel_azul = 0;
uint8_t Posicion_Aro = 1;

// estado de diana
#define APAGADO 0
#define ENCENDIDO 1
int case_estado_diana = APAGADO;

// revisar la frecuencia con osciloscopio del disparo
/* variables para analisis de frecuencia */
ulong frecuencia_disparo = 10000;
ulong frecuencia_apunta = 5000;

int8_t activa_captura_de_muestras = NO;


// los datos de las frecuencias se dividen entre 100 para sacar la media de la  muestra
//ejemplo disparo verde 2200 si se muestrea en 10 mseg el resultado es 22 pulsos
int frecuencia_apunta_tiro_verde = 12;
int frecuencia_disparo_tiro_verde = 22;
int frecuencia_apunta_tiro_azul = 32;
int frecuencia_disparo_tiro_azul = 42;
int ajuste_frecuencia_arriba = 3; // es el ajuste hacia arriba y hacia abajo del valor generado de la media
int ajuste_frecuencia_abajo=1;

#define NO_DETECTADO 0
#define APUNTA_VERDE 1
#define DISPARO_VERDE 2
#define APUNTA_AZUL 3
#define DISPARO_AZUL 4
#define EN_PROCESO_DE_ENVIO 5
#define REPETIR_ANALISIS_DE_FRECUENCIA 6


uint8_t case_valor_del_laser = NO_DETECTADO;
uint8_t figura_actual = FIGURA_SIN_DEFINIR;

/* ----------------- ENVIO DE DATOS -----------------------------*/
int habilita_enviar_paquete = NO;
int paquete_enviado_ok = NO;
String success;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Espera(250);
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status == 0)
  {
    success = "Delivery Success :)";
    // reinicia el equipo para esperar un nueva solicitud
    // case_proceso_envio=STANDBYE;
    // case_estado_diana=APAGADO;
    // captura_tiempo_inicial=SI;
  }
  else
  {
    success = "Delivery Fail :(";
  }
}

// tiempo de duracion del tiro
volatile unsigned long tiempo_inicia_tiro = 0;
volatile unsigned long tiempo_termina_tiro = 0;
int activa_envio_de_resultados = NO;

// variables para deteccion de disparo en analisis de frecuencia
#define NO_DETECTADO 0

//----
volatile unsigned long pulsos = 0;
float arreglo_muestras[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int captura_nueva_muestra =SI;

float global_porcentaje_desviacion=1;
int captura_tiempo_inicial = SI;

// contadores para relojes de encendido y apagado tiras con figuras

volatile unsigned long tiempo_inicial_reloj_1seg = millis();
volatile unsigned long tiempo_actual_reloj_1seg = 0;

volatile unsigned long tiempo_inicial_reloj_500mseg = millis();
volatile unsigned long tiempo_actual_reloj_500mseg = 0;

volatile unsigned long tiempo_inicial_reloj_200mseg = millis();
volatile unsigned long tiempo_actual_reloj_200mseg = 0;

volatile unsigned long tiempo_inicial_reloj_150mseg = millis();
volatile unsigned long tiempo_actual_reloj_150mseg = 0;

volatile unsigned long tiempo_inicial_reloj_100mseg = millis();
volatile unsigned long tiempo_actual_reloj_100mseg = 0;

volatile unsigned long tiempo_inicial_reloj_10mseg = micros();
volatile unsigned long tiempo_actual_reloj_10mseg = 0;


// publicidad

void Muestra_Figura_Publicidad(int local_figura_publicidad);
void Muestra_Valor_Publicidad(int local_figura_publicidad);
void Ejecuta_Publicidad();
void Matriz_Ilumina_Mira_Aleatorio();
void Matriz_Apuntando_Fijo();
void Matriz_Apuntando_Intermitente();
void Marco_Intermitente1();
void Marco_Intermitente2();
void Matriz_Calibra_Publicidad();
void Dibuja_Marco_Intermitente();


// Paso 1 captura frecuencias
void Paso_Uno_Analiza_Tiro_Obten_Valor();
void Inicia_Datos_Estructura_Tiro();
void Analiza_Muestras();
void Captura_20_muestras();
void Evalua_Tirador_Correcto();
void Calcular_Datos_Del_Tiro();

//Paso 2 Dibuja
void Paso_2_Evalua_Resultado_Disparo();
uint8_t Evalua_y_Dibuja_Figura_Recibida();
uint8_t Evalua_Figura_Normal();
uint8_t Evalua_Figura_Bono();
uint8_t Evalua_Figura_Castigo();
uint8_t Evalua_Figura_Calibra();

void Asigna_Color_Basandose_En_Color_Recibido_De_Monitor();
void Dibuja_Valor_Disparo();
void Aro_Apuntando_Secuencia();
void Matriz_Aro_1_Apuntado();
void Matriz_Aro_2_Apuntado();
void Matriz_Aro_3_Apuntado();
void Matriz_Aro_4_Apuntado();

//Paso 3 Envio informacion
void Arma_Paquete_De_Envio();




//Comunicacion

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
void Asigna_Color_Inicial(uint8_t local_color);


void Test_Conexion_Diana();
void readMacAddress();
void Inicia_ESP_NOW();
void Peer_Monitor_En_Linea();

//Relojes
int Reloj_1_Seg();
int creloj_500mseg();
int reloj_200mseg();
int reloj_150mseg();
int creloj_100mseg();
int creloj_10mseg();

//Control
void Reinicializa_Variables_Antes_De_Apagar();
void Inicia_Relojes();
void Switchea_Izquierda_Por_Derecha();
void Espera(int local_millisegundos_de_espera);
void Despliega_Muestras();



// figuras

void Matriz_Oso();
void Matriz_Zorra();
void Matriz_Mariposa();
void Matriz_Pinguino();
void Matriz_Arana();
void Matriz_Lagartija();
void Matriz_Rana();
void Matriz_Unicornio();
void Matriz_Botella();
void Matriz_Copa();
void Matriz_Corazon();
void Matriz_Ok();
void Matriz_Calibra();

void Matriz_Limpia_Secuencias();
void No_Wifi();

// figuras de valores del tiro
void Matriz_Green();
void Matriz_Blue();
void Valor_50();
void Valor_40();
void Valor_45();
void Valor_75();
void Valor_35();
void Valor_30();
void Valor_300();
void Valor_200();
void Valor_150();
void Valor_100();
void Valor_00();
void Valor_Menos_200();
void Valor_Menos_100();
void Valor_Menos_70();

// Auxiliares

void Despliega_datos_recibidos(); 
void  Paquete_datos_enviados();
void Muestra_Datos_Programador();
void Despliega_Estatus_Del_Tiro();


// Contadores

int contador_muestra=0;
#define tolerancia_desviacion_std 0.35


/* -------------------- RECEPCION DE DATOS -----------------------*/
// Callback when data is received
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
  memcpy(&datos_recibidos, incomingData, sizeof(datos_recibidos));
  // informacion de longuitud de paquete
  Serial.println("---------Paquete recibido---------------");
  // Control
  Serial.println(".n = No. Paquete : " + String(datos_recibidos.n));
  Serial.print("Bytes received: ");  Serial.println(len);
  char macStr[18];
  Serial.print("Packet  from: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
  Despliega_datos_recibidos();

  // se guada tipo de tiro recibido para validar el rady de loc jugadores de contadores descendentes
  // SE REGRESA JUGADOR READY PARA EVITAR ACUMULAR TIROS EN JUGADOR

  if ((datos_recibidos.t == TIRO_ACTIVO) || (datos_recibidos.t == TIRO_CALIBRA)) 
  {
    // inicializa los pulsos para PASO 1 analisis de frecuencia
    activa_captura_de_muestras = SI;
    contador_muestra=0;
    captura_nueva_muestra=SI;
    tiempo_inicia_tiro=millis();
    pulsos = 0;
    habilita_enviar_paquete=NO;
    habilita_figura=SI;

    // Asigna "color original" de tirador privado -> rojo,verde o azul
    Asigna_Color_Inicial(datos_recibidos.c);
    case_estado_diana = ENCENDIDO;

    // ->se pone para limpiar matriz para limpiar la publicidad antes de entrar
    tira.clear();
    tira.show();
    // Serial.println("case_estado_diana= ENCENDIDO");
  }
  else
  {
    // test de funcionamiento, entra al proceso solo checa el retorno de la señal
  }
}



/*-----------------------------------------------------------------------------*/
// Contador para AttachInterrupt para proceso de identificacion de freceuncia recibida por laser

void Cuenta_Pulso() //Ver 4.0
{
  pulsos++;
}

// Pantalla fantasma para matriz 16x16
// Se pone una sola linea ya que concide con la tira gracias a la hoja de excel que ya da el valor
int Pantalla[262];

void Asigna_Pantalla()
{
  for (int posicion_led = 0; posicion_led <= 261; posicion_led++)
  {
    Pantalla[posicion_led] = posicion_led;
  }
}

void Inicia_Datos_Estructura_Tiro()
{
datos_del_tiro.resultado_de_analisis=VALIDO;
datos_del_tiro.media_obtenida=0;
datos_del_tiro.porcentaje_desviacion=0.00;
datos_del_tiro.valor_del_tiro=NO_DETECTADO;
datos_del_tiro.color_del_tiro=COLOR_SIN_ASIGNAR;
datos_del_tiro.color_del_tirador=COLOR_SIN_ASIGNAR;
datos_del_tiro.tirador_correcto=SI;
datos_del_tiro.tipo_de_figura=NORMAL;
datos_del_tiro.figura_impactada=NORMAL_OSO;
datos_del_tiro.puntos_figura=PUNTOS_NORMAL_OSO;
}

int contador = 0;
int temporizador_publicidad = 0;
#define ENCIENDE_PUBLICIDAD 180 // espera 3 minutos antes de entrar publicidad
#define CAMBIA_A_MIRA       182
#define CAMBIA_A_DISPARO    184
#define APAGA_PUBLICIDAD    186
#define SEGUNDOS_APAGADO    3


int cambia_figura_publicidad = SI;
int figura_publicidad = 1;
int ilumina_mira=NO;

/* -----------------------INICIO    S E T U P --------------------------------*/

void setup()
{
  Asigna_Pantalla();
  attachInterrupt(digitalPinToInterrupt(pin_pulsos), Cuenta_Pulso, FALLING);

  // inicia Neopixel tira
  tira.begin();
  tira.setBrightness(20);
  tira.show();

  // inicia comunicacion serial
  Serial.begin(115200);

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // escribe direccion mac
  Serial.print("[DEFAULT] ESP32 Board MAC Address: ");
  readMacAddress();
  Serial.println("MAC: " + WiFi.macAddress());

  // Init ESP-NOW
  Inicia_ESP_NOW();

  // sincronizacin de dianas (peer)
  Peer_Monitor_En_Linea();

  // test en linea MOnitor
  Test_Conexion_Diana();


  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  esp_now_register_send_cb(OnDataSent);

  Inicia_Relojes(); 

  Inicia_Datos_Estructura_Tiro();

} // fin setup

/* ----------------------  F I N    S E T U P -------------------------*/

void loop() //Ver 4.0
{
  switch (case_estado_diana)
  {
  case APAGADO:
    Ejecuta_Publicidad();
    break;
  case ENCENDIDO:
    temporizador_publicidad = 0;
    Paso_Uno_Analiza_Tiro_Obten_Valor();
    Despliega_Estatus_Del_Tiro(); // Informacion para desarrollador, estado del tiro en esta parte
    if (habilita_figura == SI)
      {
     figura_actual = Evalua_y_Dibuja_Figura_Recibida(); //  Dibuja figuraSolicitada->(figura-vs-tiempo)
      }  
    if (Reloj_1_Seg() == DETECTA_CAMBIO_ESTADO_RELOJ)
    {
      Serial.println("Numero de figura : " + String(figura_actual));
    }

    Paso_2_Evalua_Resultado_Disparo();

    // PASO TRES - Envio de los datos al monitor, se pone en raiz porque es requerimiento, intentara 3 veces
    if (habilita_enviar_paquete == SI)
    {
      Serial.println("ENVIO PAQUETE :" + String(datos_enviados.n));
      esp_err_t result = esp_now_send(broadcastAddressMonitor, (uint8_t *)&datos_enviados, sizeof(structura_mensaje));
      Espera(100);
      if (result == ESP_OK)
      {
        Serial.println("Envio Paquete OK, intento : " + String(intentos_envio));
        tira.clear();
        tira.show();
        // nueva version
        Reinicializa_Variables_Antes_De_Apagar();
        case_estado_diana = APAGADO;
        // pendiente --> reincializa contador de publicidad para espera
      }
      else
      { // incrementa contador y vuelve a intentar
        Serial.println("Error envio : " + String(intentos_envio));
        intentos_envio++;
        datos_del_tiro.valor_del_tiro = EN_PROCESO_DE_ENVIO; // para que no cambie nada el paso 2
        if (intentos_envio > 3)
        {
          Reinicializa_Variables_Antes_De_Apagar();
          case_estado_diana = APAGADO;
          // pendiente reinicia contador de publicidad y a que puede mezclar con dian en lo que espera
        }
        tira.clear();
        tira.show();
        tira.setPixelColor(1, tira.Color(0, 250, 0));
        tira.setPixelColor(1, tira.Color(1, 250, 0));
        tira.setPixelColor(1, tira.Color(2, 250, 0));
        tira.show();
        previous_time_reenvio = millis();
      }
    } // fin if habilita_enviar_paquete
    break; //ENCENDIDO
  } // case_estado_diana

} //Fin de loop


void Paso_Uno_Analiza_Tiro_Obten_Valor()
{   
    // Serial.println("activa_captura_de_muestras ="+String(activa_captura_de_muestras));
    if (activa_captura_de_muestras == SI)
    {
      Captura_20_muestras(); // captura las 20 muestras
      Analiza_Muestras();    // analiza 19 muestras ignorando la muestra[0] que es no es confiable
      if (datos_del_tiro.resultado_de_analisis == VALIDO)
      {
        Calcular_Datos_Del_Tiro(); // obtiene de la frecuencias el valor->color del tiro y valor
        Evalua_Tirador_Correcto(); // Si PRIVADO-> valida corresponda al color del tiro solicitado excepto NO_DETECTADO
        if (datos_del_tiro.tirador_correcto == NO)
        {
          datos_del_tiro.valor_del_tiro = REPETIR_ANALISIS_DE_FRECUENCIA;
          datos_del_tiro.color_del_tiro = COLOR_SIN_ASIGNAR;
          datos_del_tiro.color_del_tirador = COLOR_SIN_ASIGNAR;
        }
        // Switchea_Izquierda_Por_Derecha; // para dar movimiento a las figuras
      }
      else
      {
        datos_del_tiro.valor_del_tiro = REPETIR_ANALISIS_DE_FRECUENCIA;
      }
    }
}


void Paso_2_Evalua_Resultado_Disparo()
{   
    // Serial.print ("valor_del_tiro : "+String(datos_del_tiro.valor_del_tiro));
    Switchea_Izquierda_Por_Derecha();
    switch (datos_del_tiro.valor_del_tiro)
    {
    case NO_DETECTADO:
      break;
    case APUNTA_VERDE:
      // Aro_Apuntando_Secuencia(); // dibuja aro con base : datos_del_tiro.color_del_tiro
      Dibuja_Marco_Intermitente();
      Matriz_Apuntando_Intermitente();
      activa_captura_de_muestras = SI;
      habilita_enviar_paquete == NO;
      break;
    case DISPARO_VERDE:
      habilita_figura=NO;
      Dibuja_Valor_Disparo(); // Con base : datos_del_tirador.puntos_figura
      activa_captura_de_muestras = NO;
      tiempo_termina_tiro = millis();
      Arma_Paquete_De_Envio();
      datos_del_tiro.valor_del_tiro = EN_PROCESO_DE_ENVIO;
      habilita_enviar_paquete == SI;
      intentos_envio = 1;
      break;
    case APUNTA_AZUL:
      // Aro_Apuntando_Secuencia(); // dibuja aro con base : datos_del_tiro.color_del_tiro
      Dibuja_Marco_Intermitente();
      Matriz_Apuntando_Intermitente();
      activa_captura_de_muestras = SI;
      habilita_enviar_paquete == NO;
      break;
    case DISPARO_AZUL:
      habilita_figura=NO;
      Dibuja_Valor_Disparo(); // Con base : datos_del_tirador.puntos_figura
      activa_captura_de_muestras = NO;
      habilita_enviar_paquete == SI;
      tiempo_termina_tiro = millis();
      Arma_Paquete_De_Envio();
      datos_del_tiro.valor_del_tiro = EN_PROCESO_DE_ENVIO; // para segunda vuelta ya no haga nada y se vaya envio de nuevo
      intentos_envio = 1;
      break;
    case REPETIR_ANALISIS_DE_FRECUENCIA:
      activa_captura_de_muestras = SI;
      Serial.println(" <-- Repetir muestra  ");
      habilita_enviar_paquete == NO;
      break;
    case EN_PROCESO_DE_ENVIO:
      habilita_enviar_paquete =SI;
      Serial.print("--> ");
      // no hace nada para pasar a Paso 3 y intento de envio de resultados al monitor
      break;
    }
}




void Despliega_Estatus_Del_Tiro()
{
  if (Reloj_1_Seg()==DETECTA_CAMBIO_ESTADO_RELOJ)
  {
  switch (datos_del_tiro.valor_del_tiro)
  {
  case NO_DETECTADO:
    Serial.print("ND ");
    Serial.println(datos_del_tiro.media_obtenida);
    break;
  case APUNTA_VERDE:
    Serial.println("APUNTA VERDE ");
    break;
  case DISPARO_VERDE:
    Serial.println("DISPARO VERDE ");
  case APUNTA_AZUL:
    Serial.println("APUNTA AZUL ");
    break;
  case DISPARO_AZUL:
    Serial.println("DISPARO AZUL ");
    break;
  case REPETIR_ANALISIS_DE_FRECUENCIA:
    Serial.println("REPETIR ANALISIS ");
    break;
  case EN_PROCESO_DE_ENVIO:
    Serial.println("EN PROCESO ENVIO ");
    // no hace nada para pasar a Paso 3 y intento de envio de resultados al monitor
    break;
  }
  }
  // Espera(2000);
}

void Calcular_Datos_Del_Tiro() //Ver 4.0
{
  // Serial.println("datos_del_tiro.media_obtenida"+String(datos_del_tiro.media_obtenida));
  if (datos_del_tiro.media_obtenida <=8)
  {
    datos_del_tiro.valor_del_tiro = NO_DETECTADO;
    datos_del_tiro.color_del_tiro = COLOR_SIN_ASIGNAR;
    datos_del_tiro.color_del_tirador = COLOR_SIN_ASIGNAR;
  }
  else
  {
    if (datos_del_tiro.media_obtenida <= 13)
    {
      datos_del_tiro.valor_del_tiro = APUNTA_VERDE;
      datos_del_tiro.color_del_tiro = GREEN;
      datos_del_tiro.color_del_tirador = GREEN;
    }
    else
    {
      if (datos_del_tiro.media_obtenida <= 17)
      {
      datos_del_tiro.valor_del_tiro = DISPARO_VERDE;
      datos_del_tiro.color_del_tiro = GREEN;
      datos_del_tiro.color_del_tirador = GREEN; 
      }
      else
      {
        if (datos_del_tiro.media_obtenida <= 24) 
        {
        datos_del_tiro.valor_del_tiro = APUNTA_AZUL;
        datos_del_tiro.color_del_tiro = BLUE;
        datos_del_tiro.color_del_tirador = BLUE;
        }
        else
        {
          if (datos_del_tiro.media_obtenida <= 31) 
          {
          datos_del_tiro.valor_del_tiro = NO_DETECTADO;
          datos_del_tiro.color_del_tiro = COLOR_SIN_ASIGNAR;
          datos_del_tiro.color_del_tirador = COLOR_SIN_ASIGNAR; 
          }
          else
          {
            if (datos_del_tiro.media_obtenida <= 42) 
            {
            datos_del_tiro.valor_del_tiro = DISPARO_AZUL;
            datos_del_tiro.color_del_tiro = BLUE;
            datos_del_tiro.color_del_tirador = BLUE; 
            }
            else
            {
            datos_del_tiro.valor_del_tiro = NO_DETECTADO;
            datos_del_tiro.color_del_tiro = COLOR_SIN_ASIGNAR;
            datos_del_tiro.color_del_tirador = COLOR_SIN_ASIGNAR;           
            }
          }
        }
      }
    }
  }
}

void Evalua_Tirador_Correcto() // Ver 4.0
{
  if (datos_del_tiro.valor_del_tiro > NO_DETECTADO) //quita valuacion en no tiro
  {
    datos_del_tiro.tirador_correcto = SI;

    if (datos_recibidos.pp == PRIVADO)
    {
      if (datos_del_tiro.color_del_tiro == datos_recibidos.c)
      {
        datos_del_tiro.tirador_correcto = SI;
      }
      else
      {
        datos_del_tiro.tirador_correcto = NO;
      }
    }
    if (datos_recibidos.pp == PUBLICO)
    {
      datos_del_tiro.tirador_correcto = SI;
    }
  }
}

void Reinicializa_Variables_Antes_De_Apagar() // Ver 4.0
{
  // De Paso 0 apagado para esperar 60 segundos despues de apagado para entrar publicidad
  temporizador_publicidad=0; // para que inicie cuenta antes de meter publicidad
  habilita_figura=SI;
  // De Paso 1
  activa_captura_de_muestras=SI;
  datos_del_tiro.valor_del_tiro=NO_DETECTADO;
  // De Paso 2
  habilita_enviar_paquete=NO;
  // De Paso 3
    intentos_envio=1;

}

void Captura_20_muestras() // Ver 4.0
{
  int local_ciclo = SI;
  while (local_ciclo == SI)
  {
    if (creloj_10mseg() == DETECTA_CAMBIO_ESTADO_RELOJ)
    {
      // Serial.println(contador_muestra);
      arreglo_muestras[contador_muestra] = pulsos;
      // Serial.println(arreglo_muestras[contador_muestra]);
      pulsos = 0;
      contador_muestra++;
      if (contador_muestra == 20) // se paso a la 20 por orden anterior,se regresa al origen
      {
        contador_muestra = 0; // regresa a casilla[0]
        local_ciclo = NO;
      }
    }
  }
  // Despliega_Muestras();
}

void Despliega_Muestras() // Ver 4.0
{
  for (int i=0;i<=19;i++)
  {
    Serial.println("arreglo_muestras ["+String(i)+"] = "+String( arreglo_muestras[i]));
  }
  Espera(6000);
}

void Analiza_Muestras() //Ver 4.0 
{
  // Serial.print("Analiza muestras");
  float local_primera_media = 0;
  float local_segunda_media = 0;
  float local_nivel_inferior = 0;
  float local_nivel_superior = 0;
  int muestras_validas = 0;
  float local_suma_cuadrados = 0;
  float local_varianza_muestral = 0;
  float local_distribucion_estandar = 1;
  float local_porcentaje_desviacion = 1;
  float local_resta_muestra[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  float local_cuadrados_resta_muestra[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  int   local_muestra_valida[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  // Saca Media todas las muestra[0] para eliminar maximos y minimos
  for (int i = 1; i <= 19; i++)
  {
    local_primera_media = local_primera_media + arreglo_muestras[i];
  }
  local_primera_media = local_primera_media / 19; // son # casillas de muestreo
  // Serial.println("local_primera_media : "+String(local_primera_media));
  if (local_primera_media > 8.00 )  // 9 pulsos por cada 10 msegundos = 900 hhz
  { // solo del la muestra[1]--muestra[19] la 0 no se utilizara
    // Se elimina las muestras que estan mas alla de la media+-.5media
    local_nivel_inferior = local_primera_media - (local_primera_media / 3);
    local_nivel_superior = local_primera_media + (local_primera_media / 3);
    for (int i = 1; i <= 19; i++)
    {
      if ((arreglo_muestras[i] > local_nivel_inferior) && (arreglo_muestras[i] < local_nivel_superior))
      {
        local_muestra_valida[i] = VALIDO;
        muestras_validas++;
      }
    }
    // Saca segunda media con muestras recortadas
    for (int i = 1; i <= 19; i++)
    {
      if (local_muestra_valida[i] == VALIDO)
      {
        local_segunda_media = local_segunda_media + arreglo_muestras[i];
      }
    }
    local_segunda_media = local_segunda_media / (muestras_validas);
    // Serial.println("Muestras validas : "+String(muestras_validas));
    // Serial.println("local_segunda_media : "+String(local_primera_media));

    // Resta Media de casillas, se hace para todas, al final solo se escogen las validas solamente
    for (int i = 1; i <= 19; i++)
    {
      local_resta_muestra[i] = arreglo_muestras[i] - int(local_segunda_media);
    }
    // Obtiene Cuadrados de casillas de resta para quitar los negativos se esogen la validas al final
    for (int i = 1; i <= 19; i++)
    {
      local_cuadrados_resta_muestra[i] = local_resta_muestra[i] * local_resta_muestra[i];
    }
    // Suma Cuadrados los validos solamente
    for (int i = 1; i <= 19; i++)
    {
      if (local_muestra_valida[i] == VALIDO)
      {
        local_suma_cuadrados = local_suma_cuadrados + local_cuadrados_resta_muestra[i];
      }
    }
    // Varianza Muestral (n-1) n=peridos valids
    local_varianza_muestral = local_suma_cuadrados / (muestras_validas - 1);

    // Desviacion estandar
    local_distribucion_estandar = sqrt(local_varianza_muestral);
    local_porcentaje_desviacion = local_distribucion_estandar / local_segunda_media;
    // guarda resultados
    datos_del_tiro.media_obtenida = int(local_segunda_media);
    datos_del_tiro.porcentaje_desviacion = local_porcentaje_desviacion;
    // Serial.println("Porcentaje desviacion : "+String(local_porcentaje_desviacion));
    // Espera(2000);

    if (local_porcentaje_desviacion <= tolerancia_desviacion_std)
    {
      datos_del_tiro.resultado_de_analisis = VALIDO;
    }
    else
    {
      datos_del_tiro.resultado_de_analisis = RECHAZADO;
    }
  } // 800
  else
  { // Ruido de celda, se da como un SIN DETECCION
    // SIN DETECCION
    datos_del_tiro.resultado_de_analisis = VALIDO;
    datos_del_tiro.media_obtenida =local_primera_media;
    datos_del_tiro.porcentaje_desviacion = 0.1;
    // Serial.println("DETECCION <900 HZ -> CERO -> SIN DETECCION");
    // Espera(1000);
  }
}



void Ejecuta_Publicidad() //ver 4.0
{
  // aumenta contador de tiempo de espera
  if (Reloj_1_Seg() == DETECTA_CAMBIO_ESTADO_RELOJ)
  {
    temporizador_publicidad++;
    Serial.print("."+String(temporizador_publicidad));
    // define la figura a mostrar en el periodo
    if (temporizador_publicidad == ENCIENDE_PUBLICIDAD)
    {
      figura_publicidad = int(rand() % 11) + 1;
    }
  }
  // Inicia mostrado de figura
  if ((temporizador_publicidad >= ENCIENDE_PUBLICIDAD) && (temporizador_publicidad < CAMBIA_A_MIRA))
  {
    Muestra_Figura_Publicidad(figura_publicidad);
  }

  // Apaga figura para esperar el cambio
  if ((temporizador_publicidad >= CAMBIA_A_MIRA) && (temporizador_publicidad < CAMBIA_A_DISPARO))
  {
    Muestra_Figura_Publicidad(figura_publicidad);
    Matriz_Ilumina_Mira_Aleatorio();
    tira.show();
  }
  // Inicia mostrado de valor de figura
  if((temporizador_publicidad >= CAMBIA_A_DISPARO) && (temporizador_publicidad<APAGA_PUBLICIDAD))
  {
    Muestra_Valor_Publicidad(figura_publicidad);
  }

  // detecta cambio y regresa 1 segundo antes de encender para seleccionar una nueva figura
  if (temporizador_publicidad >= APAGA_PUBLICIDAD)
  {
    temporizador_publicidad = ENCIENDE_PUBLICIDAD -SEGUNDOS_APAGADO;
    ilumina_mira=NO;
    tira.clear();
    tira.show();
  }
}

void Muestra_Figura_Publicidad(int local_figura_publicidad) //Ver 4.0
{
  if (reloj_200mseg() == DETECTA_CAMBIO_ESTADO_RELOJ)
  {
    Switchea_Izquierda_Por_Derecha();
    // Serial.print("x");
    if(local_figura_publicidad>0){
      tira.clear();
    }
    // 
    switch (local_figura_publicidad)
    {
    case 1:
      Matriz_Oso();
      break;
    case 2:
      Matriz_Mariposa();
      break;
    case 3:
      Matriz_Zorra();
      break;
    case 4:      
      Matriz_Arana();
      break;
    case 5:
      Matriz_Lagartija();
      break;
    case 6:
      Matriz_Corazon();
      break;
    case 7:
      Matriz_Pinguino();
      break;
    case 8:
      Matriz_Rana();
      break;
    case 9:
      Matriz_Unicornio();
      break;
    case 10:
      Matriz_Botella();
      break;
    case 11:
      Matriz_Copa();
      break;
    default:
      Matriz_Oso();
      break;
    }
    tira.show();
}
}

void Muestra_Valor_Publicidad(int local_figura_publicidad) //Ver 4.0
{
  if (reloj_200mseg() == DETECTA_CAMBIO_ESTADO_RELOJ)
  {
    // Serial.print("x");
    Switchea_Izquierda_Por_Derecha();
    if(local_figura_publicidad>0){
      tira.clear();
    }
    // 
    switch (local_figura_publicidad)
    {
    case 1:
      Valor_50();
      break;
    case 2:
      Valor_45();
      break;
    case 3:
      Valor_40();
      break;
    case 4:      
      Valor_35();
      break;
    case 5:
      Valor_30();
      break;
    case 6:
      Valor_300();
      break;
    case 7:
      Valor_200();
      break;
    case 8:
      Valor_150();
      break;
    case 9:
      Valor_Menos_200();
      break;
    case 10:
      Valor_Menos_100();
      break;
    case 11:
      Valor_Menos_70();
      break;
    default:
      Valor_50();
      break;
    }
    tira.show();
  }
}
/*-----------------------------------------------------------------*/

void Inicia_Relojes() //Ver 4.0
{
  // para switchear los parpadeos de las figuras
  tiempo_actual_reloj_1seg = millis();
  tiempo_actual_reloj_500mseg = millis();
  tiempo_actual_reloj_200mseg = millis();
  tiempo_actual_reloj_150mseg =millis();
  tiempo_actual_reloj_100mseg = millis();
  tiempo_actual_reloj_10mseg=millis();
}


/*------------------------------------------------------------*/

void  Paquete_datos_enviados() //Ver 4.0
{

  // datos obtenidos del disparo
  Serial.println();
  Serial.println("---------DATOS DE PAQUETE A ENVIAR -------------------");
  Serial.println("No. jugador que impacto : " + String(datos_enviados.jg));
  Serial.println("Puntuacion del disparo  : " + String(datos_enviados.po));
  Serial.println("figura que impacto      : " + String(datos_enviados.fi));

  //Propiedades del Paquete
  switch (datos_enviados.t)
  {
  case TEST:
    Serial.println(".t = 0= TEST DE FUNCIONAMIENTO");
    break;
  case TIRO_ACTIVO:
    Serial.println(".t = 1= SOLICITUD DE TIRO ACTIVO");
    break;
  case TIRO_CALIBRA:
    Serial.println(".t = 2= SOLICITUD DE JUGADOR READY");
    break;
  default:
    Serial.println(".t = SIN DEFINIR -> " + String(datos_enviados.t));
    break;
  }
  Serial.println(".d = No. diana : " + String(datos_enviados.d));
  switch (datos_enviados.c)
  {
  case GREEN:
    Serial.println(".c = 2 = COLOR GREEN");
    break;
  case BLUE:
    Serial.println(".c = 3 = COLOR  BLUE");
    break;
  case RED:
    Serial.println(".c = 1 = COLOR RED");
    break;
  default:
    Serial.println(".c = COLOR SIN DEFINIR -> " + String(datos_recibidos.c));
    break;
  }
  Serial.println(".p = No. Jugador : " + String(datos_enviados.p));
  Serial.println(".s = No. de tiro : " + String(datos_enviados.s));
  // Condiciones de manejo de tiro
  switch (datos_enviados.pp)
  {
  case PRIVADO:
    Serial.println(".pp = 0 = PRIVADO ");
    break;
  case PUBLICO:
    Serial.println(".pp = 1 = PUBLICO ");
    break;
  default:
    Serial.println(".pp = SIN DEFINIR  --> " + String(datos_enviados.pp));
    break;
  }
  Serial.println(".pa = No. de Jugador alterno : " + String(datos_enviados.pa));
  Serial.println(".vf = VELOCIDAD DE CAMBIO RECIBIDA : " + String(datos_enviados.vf));
  switch (datos_enviados.tf)
  {
  case FIGURA_SIN_DEFINIR:
    Serial.println("tf =0 = TIPO_TEST ");
    break;
  case TIPO_NORMAL:
    Serial.println("tf =1 : TIPO_NORMAL ");
    break;
  case TIPO_BONO:
    Serial.println("tf =2 : TIPO_BONO ");
    break;
  case TIPO_CASTIGO:
    Serial.println("tf =3 : TIPO_CASTIGO ");
    break;
  case TIPO_CALIBRA:
    Serial.println("tf = 4 : TIPO_CALIBRA ");
    break;
  default:
    Serial.println(".tf = *NO VALIDO*   --> " + String(datos_enviados.tf));
    break;
  }
  Serial.println(".vt = INCREMENTO DE NO. TIROS EN CUENTA  : " + String(datos_enviados.vt));
  Serial.println(".xf = NO DE FIGURA CON QUE INICIAR : " + String(datos_enviados.vt));

  // valores posibles de .ju
  switch (datos_enviados.ju)
  {
  case JUEGO_CLASICO:
    Serial.println(".ju = 1= JUEGO CLASICO");
    break;
  case JUEGO_TORNEO:
    Serial.println(".ju = 2= JUEGO TORNEO");
    break;
  case JUEGO_EQUIPOS:
    Serial.println(".ju = 3= JUEGO EQUIPOS");
    break;
  case JUEGO_VELOCIDAD:
    Serial.println(".ju = 4= JUEGO VELOCIDAD");
    break;
  default:
    Serial.println(".ju = SIN DEFINIR -> " + String(datos_enviados.ju));
    break;
  }

  Serial.println(".jr = No. de Round = " + String(datos_enviados.jr));

  Serial.println("---------FIN DE PAQUETE  ----------------");
  Serial.println();
}

/* ----------------------------------------------------------*/

void Despliega_datos_recibidos()  // Ver 4.0
{
  Serial.println("---------PAQUETE RECIBIDO -------------------");
  // Propiedades del Paquete
  switch (datos_recibidos.t)
  {
  case TEST:
    Serial.println(".t = 0= TEST DE FUNCIONAMIENTO");
    break;
  case TIRO_ACTIVO:
    Serial.println(".t = 1= SOLICITUD DE TIRO ACTIVO");
    break;
  case TIRO_CALIBRA:
    Serial.println(".t = 2= SOLICITUD DE JUGADOR READY");
    break;
  default:
    Serial.println(".t = SIN DEFINIR -> " + String(datos_recibidos.t));
    break;
  }

  Serial.println(".d = No. diana : " + String(datos_recibidos.d));

  switch (datos_recibidos.c)
  {
  case GREEN:
    Serial.println(".c = 2 = COLOR GREEN");
    break;
  case BLUE:
    Serial.println(".c = 3 = COLOR  BLUE");
    break;
  case RED:
    Serial.println(".c = 1 = COLOR RED");
    break;
  default:
    Serial.println(".c = COLOR SIN DEFINIR -> " + String(datos_recibidos.c));
    break;
  }

  Serial.println(".p = No. Jugador : " + String(datos_recibidos.p));
  Serial.println(".s = No. de tiro : " + String(datos_recibidos.s));

  // Condiciones de manejo de tiro
  switch (datos_recibidos.pp)
  {
  case PRIVADO:
    Serial.println(".pp = 0 = PRIVADO ");
    break;
  case PUBLICO:
    Serial.println(".pp = 1 = PUBLICO ");
    break;
  default:
    Serial.println(".pp = SIN DEFINIR  --> " + String(datos_recibidos.pp));
    break;
  }
  Serial.println(".pa = No. de Jugador alterno : " + String(datos_recibidos.pa));
  Serial.println(".vf = VELOCIDAD DE CAMBIO RECIBIDA : " + String(datos_recibidos.vf));
  switch (datos_recibidos.tf)
  {
  case FIGURA_SIN_DEFINIR:
    Serial.println("tf =0 = TIPO_TEST ");
    break;
  case TIPO_NORMAL:
    Serial.println("tf =1 = TIPO_NORMAL ");
    break;
  case TIPO_BONO:
    Serial.println("tf =2 = TIPO_BONO ");
    break;
  case TIPO_CASTIGO:
    Serial.println("tf =3 = TIPO_CASTIGO ");
    break;
  case TIPO_CALIBRA:
    Serial.println("tf =4 = TIPO_CALIBRA ");
    break;
  default:
    Serial.println(".tf = no encontrado el valor   --> " + String(datos_recibidos.tf));
    break;
  }
  Serial.println(".vt = INCREMENTO DE NO. TIROS EN CUENTA  : " + String(datos_recibidos.vt));
  Serial.println(".xf = NO DE FIGURA CON QUE INICIAR : " + String(datos_recibidos.vt));

  // valores posibles de .ju
  switch (datos_recibidos.ju)
  {
  case JUEGO_CLASICO:
    Serial.println(".ju = 1= JUEGO CLASICO");
    break;
  case JUEGO_TORNEO:
    Serial.println(".ju = 2= JUEGO TORNEO");
    break;
  case JUEGO_EQUIPOS:
    Serial.println(".ju = 3= JUEGO EQUIPOS");
    break;
  case JUEGO_VELOCIDAD:
    Serial.println(".ju = 4= JUEGO VELOCIDAD");
    break;
  default:
    Serial.println(".ju = SIN DEFINIR -> " + String(datos_recibidos.ju));
    break;
  }

  Serial.println(".jr = No. de Round = " + String(datos_recibidos.jr));

  Serial.println("---------FIN DE PAQUETE RECIBIDO ----------------");
}





/*--------------------------------------------------------*/

int Reloj_1_Seg() // Ver 4.0
{
  tiempo_actual_reloj_1seg = millis();
  if ((tiempo_inicial_reloj_1seg + 1000) < tiempo_actual_reloj_1seg)
  {
    tiempo_inicial_reloj_1seg = millis();
    actualiza_display_1seg = SI;
    return DETECTA_CAMBIO_ESTADO_RELOJ;
  }
  else
  {
    actualiza_display_1seg = NO;
    return SIN_CAMBIO_ESTADO_RELOJ;
  }
}

/* -------------------------------------------------------*/
int creloj_500mseg() // CD001
{
  tiempo_actual_reloj_500mseg = millis();
  if ((tiempo_inicial_reloj_500mseg + 500) < tiempo_actual_reloj_500mseg)
  {
    tiempo_inicial_reloj_500mseg = millis();
    actualiza_display_500mseg = SI;
    return DETECTA_CAMBIO_ESTADO_RELOJ;
  }
  else
  {
    actualiza_display_500mseg = NO;
    return SIN_CAMBIO_ESTADO_RELOJ;
  }
}

int reloj_200mseg() // Ver 4.0
{
  tiempo_actual_reloj_200mseg = millis();
  if ((tiempo_inicial_reloj_200mseg + 200) < tiempo_actual_reloj_200mseg)
  {
    tiempo_inicial_reloj_200mseg = millis();
    actualiza_display_200mseg = SI;
    return DETECTA_CAMBIO_ESTADO_RELOJ;
  }
  else
  {
    actualiza_display_200mseg = NO;
    return SIN_CAMBIO_ESTADO_RELOJ;
  }
}

int reloj_150mseg(){
  tiempo_actual_reloj_150mseg = millis();
  if ((tiempo_inicial_reloj_150mseg + 150) < tiempo_actual_reloj_150mseg)
  {
    tiempo_inicial_reloj_150mseg = millis();
    actualiza_display_150mseg = SI;
    return DETECTA_CAMBIO_ESTADO_RELOJ;
  }
  else
  {
    actualiza_display_150mseg = NO;
    return SIN_CAMBIO_ESTADO_RELOJ;
  } 
}

int creloj_100mseg() // CD001
{
  tiempo_actual_reloj_100mseg = millis();
  if ((tiempo_inicial_reloj_100mseg + 100) < tiempo_actual_reloj_100mseg)
  {
    tiempo_inicial_reloj_100mseg = millis();
    actualiza_display_100mseg = SI;
    return DETECTA_CAMBIO_ESTADO_RELOJ;
  }
  else
  {
    actualiza_display_100mseg = NO;
    return SIN_CAMBIO_ESTADO_RELOJ;
  }
}

int creloj_10mseg() // Ver 4.0
{
  tiempo_actual_reloj_10mseg = micros();
  if ((tiempo_inicial_reloj_10mseg + 10000) < tiempo_actual_reloj_10mseg)
  {
    tiempo_inicial_reloj_10mseg = micros();
    return DETECTA_CAMBIO_ESTADO_RELOJ;
  }
  else
  {
    return SIN_CAMBIO_ESTADO_RELOJ;
  }
}



/* ---------------------------------------------------------------------*/

void Asigna_Color_Inicial(uint8_t local_color) //Ver 4.0
{
  switch (datos_recibidos.c)
  {
  case RED:
    /*code*/
    // Serial.println("Programa color rojo para castigos");
    pixel_rojo = 0;
    pixel_verde = 250;
    pixel_azul = 0;
    break;

  case GREEN:
    // Serial.println("Programa color verde");
    pixel_rojo = 250;
    pixel_verde = 0;
    pixel_azul = 0;
    break;

  case BLUE:
    // Serial.println("Programa color azul");
    pixel_rojo = 0;
    pixel_verde = 0;
    pixel_azul = 250;
    break;
  }
}




uint8_t Evalua_y_Dibuja_Figura_Recibida() //Ver 4.0
{
  // Serial.println("tipo de datos.recibidos.tf "+String(datos_recibidos.tf));
  uint8_t resultado = FIGURA_SIN_DEFINIR;
  tira.clear();
  switch (datos_recibidos.tf) // tipo de figura
  {
  case TIPO_NORMAL:
    /*case*/
    resultado = Evalua_Figura_Normal();
    break;
  case TIPO_BONO:
    /*case*/
    resultado = Evalua_Figura_Bono();
    break;
  case TIPO_CASTIGO:
    /*code*/
    resultado = Evalua_Figura_Castigo();
    break;
  case TIPO_CALIBRA:
    /*code*/
    resultado = Evalua_Figura_Calibra();
    break;
  case TIPO_TEST:
    /*code*/
    // Se supone que nunca va a habilitarse ya que test no prende la diana
    resultado = 0;
    Serial.println("ERROR EL TEST NUNCA PRENDE LA DIANA");
    break;
  }
  Matriz_Apuntando_Fijo();
  tira.show();
  return resultado;
}

/*------------------------------------------------------------------------*/

uint8_t Evalua_Figura_Normal() //Ver 4.0
{
  uint8_t local_figura_impactada = FIGURA_SIN_DEFINIR;
  volatile unsigned long rango = millis() - tiempo_inicia_tiro;

  // clasifica la figura dependiendo del tiempo df (velocidad de cambio figura)

  if (rango < datos_recibidos.vf * 2)
  {
    local_figura_impactada = NORMAL_OSO;
    valor_puntuacion_figura = PUNTOS_NORMAL_OSO;
    Matriz_Oso();
  }
  else
  {
    if (rango < (datos_recibidos.vf * 4))
    {
      local_figura_impactada = NORMAL_MARIPOSA;
      valor_puntuacion_figura = PUNTOS_NORMAL_MARIPOSA;
      Matriz_Mariposa();
    }
    else
    {
      if (rango < (datos_recibidos.vf * 6))
      {
        local_figura_impactada = NORMAL_ZORRA;
        valor_puntuacion_figura = PUNTOS_NORMAL_ZORRA;
        Matriz_Zorra();
      }
      else
      {
        if (rango < (datos_recibidos.vf * 8))
        {
          local_figura_impactada = NORMAL_ARANA;
          valor_puntuacion_figura = PUNTOS_NORMAL_ARANA;
          Matriz_Arana();
        }
        else
        {
          local_figura_impactada = NORMAL_LAGARTIJA;
          valor_puntuacion_figura = PUNTOS_NORMAL_LAGARTIJA;
          Matriz_Lagartija();
        }
      } // end if vf*3
    } // end if vf*2
  } // end if vf
  Matriz_Ilumina_Mira_Aleatorio();
  datos_del_tiro.tipo_de_figura=NORMAL;
  datos_del_tiro.figura_impactada=local_figura_impactada;
  datos_del_tiro.puntos_figura=valor_puntuacion_figura;
  return local_figura_impactada;
}


/*-----------------------------------------------------------------------*/
uint8_t Evalua_Figura_Bono() //Ver 4.0
{
  // Mostrara la figura por dos periodos, en caso de no impacto manda el valor a cero
  uint8_t local_figura_impactada = FIGURA_SIN_DEFINIR;
  if (datos_recibidos.xf==1){
    datos_recibidos.xf=BONO_CORAZON; // en caso que no llegue una figura valida
  }
  volatile unsigned long rango = millis() - tiempo_inicia_tiro;
  if (rango < (datos_recibidos.vf * 4))
  {
    // Serial.println("Bono- valor de datos_recibidos.xf : " + String(datos_recibidos.xf));
    switch (datos_recibidos.xf)
    {
    case BONO_CORAZON:
      local_figura_impactada = datos_recibidos.xf;
      valor_puntuacion_figura = PUNTOS_BONO_CORAZON;
      Matriz_Corazon();
      break;

    case BONO_PINGUINO:
      local_figura_impactada  = datos_recibidos.xf;
      valor_puntuacion_figura = PUNTOS_BONO_PINGUINO;
      Matriz_Pinguino();
      break;

    case BONO_RANA:
      local_figura_impactada = datos_recibidos.xf;
      valor_puntuacion_figura = PUNTOS_BONO_RANA;
      Matriz_Rana();
      break;
    }
  }
  else
  {
    local_figura_impactada  = BONO_PINGUINO;
    valor_puntuacion_figura = PUNTOS_BONO_PINGUINO;
    Matriz_Pinguino();
    tira.show();
    // salir_por_timeout=SI;
  }
  tira.show();
  datos_del_tiro.tipo_de_figura=BONO;
  datos_del_tiro.figura_impactada=local_figura_impactada;
  datos_del_tiro.puntos_figura=valor_puntuacion_figura;  
  return local_figura_impactada ;
}
/*-----------------------------------------------------------------------*/

uint8_t Evalua_Figura_Castigo() //Ver 4.0
{
  uint8_t local_figura_impactada = FIGURA_SIN_DEFINIR; 
  volatile unsigned long rango = millis() - tiempo_inicia_tiro;
    if (datos_recibidos.xf==1){
    datos_recibidos.xf=CASTIGO_UNICORNIO; // en caso que no llegue una figura castigo no valida
    } 
  if (rango < (datos_recibidos.vf * 4))
  {
    datos_del_tiro.tipo_de_figura=CASTIGO;
    // Serial.println("Castigo- valor de datos_recibidos.xf : " + String(datos_recibidos.xf));
    switch (datos_recibidos.xf)
    {
    case CASTIGO_UNICORNIO:
      local_figura_impactada = CASTIGO_UNICORNIO;
      valor_puntuacion_figura = PUNTOS_CASTIGO_UNICORNIO;
      Matriz_Unicornio();
      break;

    case CASTIGO_BOTELLA:
      local_figura_impactada = CASTIGO_BOTELLA;
      valor_puntuacion_figura = PUNTOS_CASTIGO_BOTELLA;
      Matriz_Botella();
      break;

    case CASTIGO_COPA:
      local_figura_impactada = CASTIGO_COPA;
      valor_puntuacion_figura = PUNTOS_CASTIGO_COPA;
      Matriz_Copa();
      break;
    }
  }
  else
  {
    if (rango < (datos_recibidos.vf * 7)) // si no dispara al castigo, se convierte en BONO
    {
      datos_del_tiro.tipo_de_figura=BONO;
      // Serial.println("CastigoBono- valor de datos_recibidos.xf : " + String(datos_recibidos.xf));
      switch (datos_recibidos.xf)
      {
      case CASTIGO_UNICORNIO: // se premia con mayor punturacion  por no disparar a unicornio
        local_figura_impactada = BONO_CORAZON;
        valor_puntuacion_figura = PUNTOS_BONO_CORAZON;
        Matriz_Corazon();
        break;

      case CASTIGO_BOTELLA:
        local_figura_impactada = BONO_PINGUINO;
        valor_puntuacion_figura = PUNTOS_BONO_PINGUINO;
        Matriz_Pinguino();
        break;

      case CASTIGO_COPA:
        local_figura_impactada = BONO_RANA;
        valor_puntuacion_figura = PUNTOS_BONO_RANA;
        Matriz_Rana();
        break;
      }
    }
    else
    {
      datos_del_tiro.tipo_de_figura=BONO;
      local_figura_impactada = BONO_RANA; // mayor de 4 periodos de esprea
      valor_puntuacion_figura = PUNTOS_BONO_RANA;
      Matriz_Rana();
    }
  }
  tira.show();
  datos_del_tiro.figura_impactada = local_figura_impactada;
  datos_del_tiro.puntos_figura = valor_puntuacion_figura;
  return local_figura_impactada;
}
/*------------------------------------------------------------*/

uint8_t Evalua_Figura_Calibra() //Ver 4.0
{
  uint8_t local_figura_impactada = FIGURA_SIN_DEFINIR;

  local_figura_impactada =CALIBRA_TEST;
  valor_puntuacion_figura = PUNTOS_CALIBRA_TEST;
  datos_del_tiro.tipo_de_figura=CALIBRA;
  datos_del_tiro.figura_impactada = local_figura_impactada;
  datos_del_tiro.puntos_figura = valor_puntuacion_figura;
  tira.clear();
  Asigna_Color_Basandose_En_Color_Recibido_De_Monitor();
  Matriz_Calibra();
  Matriz_Apuntando_Fijo();
  //tira.show();
  return local_figura_impactada;
}
/*----------------------------------------------------------------------------*/

void Switchea_Izquierda_Por_Derecha() //ver 4.0
{
  if (lado == IZQUIERDA)
  {
    lado = DERECHA;
  }
  else
  {
    lado = IZQUIERDA;
  }
}

/*----------------------------------------------------------------------------*/

void Matriz_Zorra() //Ver 4.0
{
  tira.setBrightness(30);
  if (lado == IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[13], 105, 210, 30);
    tira.setPixelColor(Pantalla[5], 105, 210, 30);
    tira.setPixelColor(Pantalla[18], 105, 210, 30);
    tira.setPixelColor(Pantalla[26], 105, 210, 30);
    tira.setPixelColor(Pantalla[45], 105, 210, 30);
    tira.setPixelColor(Pantalla[44], 105, 210, 30);
    tira.setPixelColor(Pantalla[38], 105, 210, 30);
    tira.setPixelColor(Pantalla[37], 105, 210, 30);
    tira.setPixelColor(Pantalla[50], 105, 210, 30);
    tira.setPixelColor(Pantalla[51], 127, 255, 80);
    tira.setPixelColor(Pantalla[52], 105, 210, 30);
    tira.setPixelColor(Pantalla[56], 105, 210, 30);
    tira.setPixelColor(Pantalla[57], 127, 255, 80);
    tira.setPixelColor(Pantalla[58], 105, 210, 30);
    tira.setPixelColor(Pantalla[77], 105, 210, 30);
    tira.setPixelColor(Pantalla[76], 127, 255, 80);
    tira.setPixelColor(Pantalla[75], 105, 210, 30);
    tira.setPixelColor(Pantalla[74], 105, 210, 30);
    tira.setPixelColor(Pantalla[73], 105, 210, 30);
    tira.setPixelColor(Pantalla[72], 105, 210, 30);
    tira.setPixelColor(Pantalla[71], 105, 210, 30);
    tira.setPixelColor(Pantalla[70], 127, 255, 80);
    tira.setPixelColor(Pantalla[69], 105, 210, 30);
    tira.setPixelColor(Pantalla[82], 105, 210, 30);
    tira.setPixelColor(Pantalla[83], 105, 210, 30);
    tira.setPixelColor(Pantalla[84], 105, 210, 30);
    tira.setPixelColor(Pantalla[85], 105, 210, 30);
    tira.setPixelColor(Pantalla[86], 105, 210, 30);
    tira.setPixelColor(Pantalla[87], 105, 210, 30);
    tira.setPixelColor(Pantalla[88], 105, 210, 30);
    tira.setPixelColor(Pantalla[89], 105, 210, 30);
    tira.setPixelColor(Pantalla[90], 105, 210, 30);
    tira.setPixelColor(Pantalla[92], 0, 0, 0);
    tira.setPixelColor(Pantalla[93], 0, 0, 0);
    tira.setPixelColor(Pantalla[94], 0, 0, 0);
    tira.setPixelColor(Pantalla[109], 105, 210, 30);
    tira.setPixelColor(Pantalla[108], 105, 210, 30);
    tira.setPixelColor(Pantalla[107], 0, 0, 255);
    tira.setPixelColor(Pantalla[106], 105, 210, 30);
    tira.setPixelColor(Pantalla[105], 105, 210, 30);
    tira.setPixelColor(Pantalla[104], 105, 210, 30);
    tira.setPixelColor(Pantalla[103], 0, 0, 255);
    tira.setPixelColor(Pantalla[102], 105, 210, 30);
    tira.setPixelColor(Pantalla[101], 105, 210, 30);
    tira.setPixelColor(Pantalla[99], 0, 0, 0);
    tira.setPixelColor(Pantalla[98], 0, 0, 0);
    tira.setPixelColor(Pantalla[97], 0, 0, 0);
    tira.setPixelColor(Pantalla[114], 100, 100, 100);
    tira.setPixelColor(Pantalla[115], 100, 100, 100);
    tira.setPixelColor(Pantalla[116], 100, 100, 100);
    tira.setPixelColor(Pantalla[117], 100, 100, 100);
    tira.setPixelColor(Pantalla[119], 100, 100, 100);
    tira.setPixelColor(Pantalla[120], 100, 100, 100);
    tira.setPixelColor(Pantalla[121], 100, 100, 100);
    tira.setPixelColor(Pantalla[122], 100, 100, 100);
    tira.setPixelColor(Pantalla[124], 0, 0, 0);
    tira.setPixelColor(Pantalla[125], 0, 0, 0);
    tira.setPixelColor(Pantalla[126], 105, 210, 30);
    tira.setPixelColor(Pantalla[141], 100, 100, 100);
    tira.setPixelColor(Pantalla[140], 100, 100, 100);
    tira.setPixelColor(Pantalla[139], 100, 100, 100);
    tira.setPixelColor(Pantalla[138], 100, 100, 100);
    tira.setPixelColor(Pantalla[137], 100, 100, 100);
    tira.setPixelColor(Pantalla[136], 100, 100, 100);
    tira.setPixelColor(Pantalla[135], 100, 100, 100);
    tira.setPixelColor(Pantalla[134], 100, 100, 100);
    tira.setPixelColor(Pantalla[133], 100, 100, 100);
    tira.setPixelColor(Pantalla[131], 0, 0, 0);
    tira.setPixelColor(Pantalla[130], 105, 210, 30);
    tira.setPixelColor(Pantalla[129], 105, 210, 30);
    tira.setPixelColor(Pantalla[147], 100, 100, 100);
    tira.setPixelColor(Pantalla[148], 100, 100, 100);
    tira.setPixelColor(Pantalla[149], 100, 100, 100);
    tira.setPixelColor(Pantalla[150], 100, 100, 100);
    tira.setPixelColor(Pantalla[151], 100, 100, 100);
    tira.setPixelColor(Pantalla[152], 100, 100, 100);
    tira.setPixelColor(Pantalla[153], 100, 100, 100);
    tira.setPixelColor(Pantalla[156], 0, 0, 0);
    tira.setPixelColor(Pantalla[157], 105, 210, 30);
    tira.setPixelColor(Pantalla[158], 105, 210, 30);
    tira.setPixelColor(Pantalla[171], 100, 100, 100);
    tira.setPixelColor(Pantalla[170], 100, 100, 100);
    tira.setPixelColor(Pantalla[169], 100, 100, 100);
    tira.setPixelColor(Pantalla[168], 100, 100, 100);
    tira.setPixelColor(Pantalla[167], 100, 100, 100);
    tira.setPixelColor(Pantalla[163], 105, 210, 30);
    tira.setPixelColor(Pantalla[162], 105, 210, 30);
    tira.setPixelColor(Pantalla[179], 105, 210, 30);
    tira.setPixelColor(Pantalla[180], 105, 210, 30);
    tira.setPixelColor(Pantalla[181], 105, 210, 30);
    tira.setPixelColor(Pantalla[182], 105, 210, 30);
    tira.setPixelColor(Pantalla[183], 105, 210, 30);
    tira.setPixelColor(Pantalla[184], 105, 210, 30);
    tira.setPixelColor(Pantalla[185], 105, 210, 30);
    tira.setPixelColor(Pantalla[187], 105, 210, 30);
    tira.setPixelColor(Pantalla[188], 105, 210, 30);
    tira.setPixelColor(Pantalla[205], 105, 210, 30);
    tira.setPixelColor(Pantalla[204], 105, 210, 30);
    tira.setPixelColor(Pantalla[203], 0, 0, 0);
    tira.setPixelColor(Pantalla[202], 105, 210, 30);
    tira.setPixelColor(Pantalla[201], 100, 100, 100);
    tira.setPixelColor(Pantalla[200], 105, 210, 30);
    tira.setPixelColor(Pantalla[199], 0, 0, 0);
    tira.setPixelColor(Pantalla[198], 105, 210, 30);
    tira.setPixelColor(Pantalla[197], 105, 210, 30);
    tira.setPixelColor(Pantalla[196], 127, 255, 80);
    tira.setPixelColor(Pantalla[210], 105, 210, 30);
    tira.setPixelColor(Pantalla[211], 105, 210, 30);
    tira.setPixelColor(Pantalla[212], 0, 0, 0);
    tira.setPixelColor(Pantalla[213], 105, 210, 30);
    tira.setPixelColor(Pantalla[214], 100, 100, 100);
    tira.setPixelColor(Pantalla[215], 105, 210, 30);
    tira.setPixelColor(Pantalla[216], 0, 0, 0);
    tira.setPixelColor(Pantalla[217], 105, 210, 30);
    tira.setPixelColor(Pantalla[218], 105, 210, 30);

    tira.setPixelColor(Pantalla[244], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
  }
  else
  {
    tira.setPixelColor(Pantalla[13], 105, 210, 30);
    tira.setPixelColor(Pantalla[5], 105, 210, 30);
    tira.setPixelColor(Pantalla[18], 105, 210, 30);
    tira.setPixelColor(Pantalla[26], 105, 210, 30);
    tira.setPixelColor(Pantalla[45], 105, 210, 30);
    tira.setPixelColor(Pantalla[44], 105, 210, 30);
    tira.setPixelColor(Pantalla[38], 105, 210, 30);
    tira.setPixelColor(Pantalla[37], 105, 210, 30);
    tira.setPixelColor(Pantalla[50], 105, 210, 30);
    tira.setPixelColor(Pantalla[51], 127, 255, 80);
    tira.setPixelColor(Pantalla[52], 105, 210, 30);
    tira.setPixelColor(Pantalla[56], 105, 210, 30);
    tira.setPixelColor(Pantalla[57], 127, 255, 80);
    tira.setPixelColor(Pantalla[58], 105, 210, 30);
    tira.setPixelColor(Pantalla[77], 105, 210, 30);
    tira.setPixelColor(Pantalla[76], 127, 255, 80);
    tira.setPixelColor(Pantalla[75], 105, 210, 30);
    tira.setPixelColor(Pantalla[74], 105, 210, 30);
    tira.setPixelColor(Pantalla[73], 105, 210, 30);
    tira.setPixelColor(Pantalla[72], 105, 210, 30);
    tira.setPixelColor(Pantalla[71], 105, 210, 30);
    tira.setPixelColor(Pantalla[70], 127, 255, 80);
    tira.setPixelColor(Pantalla[69], 105, 210, 30);
    tira.setPixelColor(Pantalla[82], 105, 210, 30);
    tira.setPixelColor(Pantalla[83], 105, 210, 30);
    tira.setPixelColor(Pantalla[84], 105, 210, 30);
    tira.setPixelColor(Pantalla[85], 105, 210, 30);
    tira.setPixelColor(Pantalla[86], 105, 210, 30);
    tira.setPixelColor(Pantalla[87], 105, 210, 30);
    tira.setPixelColor(Pantalla[88], 105, 210, 30);
    tira.setPixelColor(Pantalla[89], 105, 210, 30);
    tira.setPixelColor(Pantalla[90], 105, 210, 30);
    tira.setPixelColor(Pantalla[92], 105, 210, 30);
    tira.setPixelColor(Pantalla[93], 0, 0, 0);
    tira.setPixelColor(Pantalla[94], 0, 0, 0);
    tira.setPixelColor(Pantalla[109], 105, 210, 30);
    tira.setPixelColor(Pantalla[108], 105, 210, 30);
    tira.setPixelColor(Pantalla[106], 105, 210, 30);
    tira.setPixelColor(Pantalla[105], 105, 210, 30);
    tira.setPixelColor(Pantalla[104], 105, 210, 30);
    tira.setPixelColor(Pantalla[102], 105, 210, 30);
    tira.setPixelColor(Pantalla[101], 105, 210, 30);
    tira.setPixelColor(Pantalla[99], 105, 210, 30);
    tira.setPixelColor(Pantalla[98], 0, 0, 0);
    tira.setPixelColor(Pantalla[97], 0, 0, 0);
    tira.setPixelColor(Pantalla[114], 100, 100, 100);
    tira.setPixelColor(Pantalla[115], 100, 100, 100);
    tira.setPixelColor(Pantalla[116], 100, 100, 100);
    tira.setPixelColor(Pantalla[117], 100, 100, 100);
    tira.setPixelColor(Pantalla[119], 100, 100, 100);
    tira.setPixelColor(Pantalla[120], 100, 100, 100);
    tira.setPixelColor(Pantalla[121], 100, 100, 100);
    tira.setPixelColor(Pantalla[122], 100, 100, 100);
    tira.setPixelColor(Pantalla[124], 105, 210, 30);
    tira.setPixelColor(Pantalla[125], 0, 0, 0);
    tira.setPixelColor(Pantalla[126], 0, 0, 0);
    tira.setPixelColor(Pantalla[141], 100, 100, 100);
    tira.setPixelColor(Pantalla[140], 100, 100, 100);
    tira.setPixelColor(Pantalla[139], 100, 100, 100);
    tira.setPixelColor(Pantalla[138], 100, 100, 100);
    tira.setPixelColor(Pantalla[137], 100, 100, 100);
    tira.setPixelColor(Pantalla[136], 100, 100, 100);
    tira.setPixelColor(Pantalla[135], 100, 100, 100);
    tira.setPixelColor(Pantalla[134], 100, 100, 100);
    tira.setPixelColor(Pantalla[133], 100, 100, 100);
    tira.setPixelColor(Pantalla[131], 105, 210, 30);
    tira.setPixelColor(Pantalla[130], 0, 0, 0);
    tira.setPixelColor(Pantalla[129], 0, 0, 0);
    tira.setPixelColor(Pantalla[147], 100, 100, 100);
    tira.setPixelColor(Pantalla[148], 100, 100, 100);
    tira.setPixelColor(Pantalla[149], 100, 100, 100);
    tira.setPixelColor(Pantalla[151], 100, 100, 100);
    tira.setPixelColor(Pantalla[152], 100, 100, 100);
    tira.setPixelColor(Pantalla[153], 100, 100, 100);
    tira.setPixelColor(Pantalla[156], 105, 210, 30);
    tira.setPixelColor(Pantalla[157], 0, 0, 0);
    tira.setPixelColor(Pantalla[158], 0, 0, 0);
    tira.setPixelColor(Pantalla[171], 100, 100, 100);
    tira.setPixelColor(Pantalla[170], 100, 100, 100);
    tira.setPixelColor(Pantalla[169], 100, 100, 100);
    tira.setPixelColor(Pantalla[168], 100, 100, 100);
    tira.setPixelColor(Pantalla[167], 100, 100, 100);
    tira.setPixelColor(Pantalla[163], 105, 210, 30);
    tira.setPixelColor(Pantalla[179], 105, 210, 30);
    tira.setPixelColor(Pantalla[180], 105, 210, 30);
    tira.setPixelColor(Pantalla[181], 105, 210, 30);
    tira.setPixelColor(Pantalla[182], 105, 210, 30);
    tira.setPixelColor(Pantalla[183], 105, 210, 30);
    tira.setPixelColor(Pantalla[184], 105, 210, 30);
    tira.setPixelColor(Pantalla[185], 105, 210, 30);
    tira.setPixelColor(Pantalla[187], 105, 210, 30);
    tira.setPixelColor(Pantalla[188], 105, 210, 30);
    tira.setPixelColor(Pantalla[205], 105, 210, 30);
    tira.setPixelColor(Pantalla[204], 105, 210, 30);
    tira.setPixelColor(Pantalla[203], 0, 0, 0);
    tira.setPixelColor(Pantalla[202], 105, 210, 30);
    tira.setPixelColor(Pantalla[201], 100, 100, 100);
    tira.setPixelColor(Pantalla[200], 105, 210, 30);
    tira.setPixelColor(Pantalla[199], 0, 0, 0);
    tira.setPixelColor(Pantalla[198], 105, 210, 30);
    tira.setPixelColor(Pantalla[197], 105, 210, 30);
    tira.setPixelColor(Pantalla[196], 127, 255, 80);
    tira.setPixelColor(Pantalla[210], 105, 210, 30);
    tira.setPixelColor(Pantalla[211], 105, 210, 30);
    tira.setPixelColor(Pantalla[212], 0, 0, 0);
    tira.setPixelColor(Pantalla[213], 105, 210, 30);
    tira.setPixelColor(Pantalla[214], 100, 100, 100);
    tira.setPixelColor(Pantalla[215], 105, 210, 30);
    tira.setPixelColor(Pantalla[216], 0, 0, 0);
    tira.setPixelColor(Pantalla[217], 105, 210, 30);
    tira.setPixelColor(Pantalla[218], 105, 210, 30);

    tira.setPixelColor(Pantalla[244], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
  }
}
/*----------------------------------------------------------------------------*/




void Matriz_Arana() //Ver 4.0
{
  if (lado == IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[8], 0, 0, 255);
    tira.setPixelColor(Pantalla[18], 0, 255, 0);
    tira.setPixelColor(Pantalla[23], 0, 0, 255);
    tira.setPixelColor(Pantalla[28], 0, 255, 0);
    tira.setPixelColor(Pantalla[46], 0, 255, 0);
    tira.setPixelColor(Pantalla[44], 0, 255, 0);
    tira.setPixelColor(Pantalla[42], 0, 255, 0);
    tira.setPixelColor(Pantalla[41], 0, 255, 0);
    tira.setPixelColor(Pantalla[40], 0, 255, 0);
    tira.setPixelColor(Pantalla[39], 0, 255, 0);
    tira.setPixelColor(Pantalla[38], 0, 255, 0);
    tira.setPixelColor(Pantalla[36], 0, 255, 0);
    tira.setPixelColor(Pantalla[34], 0, 255, 0);
    tira.setPixelColor(Pantalla[52], 0, 255, 0);
    tira.setPixelColor(Pantalla[53], 0, 255, 0);
    tira.setPixelColor(Pantalla[54], 0, 255, 0);
    tira.setPixelColor(Pantalla[55], 0, 255, 0);
    tira.setPixelColor(Pantalla[56], 0, 255, 0);
    tira.setPixelColor(Pantalla[57], 0, 255, 0);
    tira.setPixelColor(Pantalla[58], 0, 255, 0);
    tira.setPixelColor(Pantalla[78], 0, 255, 0);
    tira.setPixelColor(Pantalla[77], 0, 255, 0);
    tira.setPixelColor(Pantalla[76], 0, 255, 0);
    tira.setPixelColor(Pantalla[75], 0, 255, 0);
    tira.setPixelColor(Pantalla[74], 0, 255, 0);
    tira.setPixelColor(Pantalla[73], 0, 255, 0);
    tira.setPixelColor(Pantalla[72], 0, 255, 0);
    tira.setPixelColor(Pantalla[71], 0, 255, 0);
    tira.setPixelColor(Pantalla[70], 0, 255, 0);
    tira.setPixelColor(Pantalla[69], 0, 255, 0);
    tira.setPixelColor(Pantalla[68], 0, 255, 0);
    tira.setPixelColor(Pantalla[67], 0, 255, 0);
    tira.setPixelColor(Pantalla[66], 0, 255, 0);
    tira.setPixelColor(Pantalla[80], 0, 255, 0);
    tira.setPixelColor(Pantalla[84], 0, 255, 0);
    tira.setPixelColor(Pantalla[85], 0, 255, 0);
    tira.setPixelColor(Pantalla[86], 0, 255, 0);
    tira.setPixelColor(Pantalla[87], 0, 255, 0);
    tira.setPixelColor(Pantalla[88], 0, 255, 0);
    tira.setPixelColor(Pantalla[89], 0, 255, 0);
    tira.setPixelColor(Pantalla[90], 0, 255, 0);
    tira.setPixelColor(Pantalla[94], 0, 255, 0);
    tira.setPixelColor(Pantalla[109], 0, 255, 0);
    tira.setPixelColor(Pantalla[108], 0, 255, 0);
    tira.setPixelColor(Pantalla[107], 0, 255, 0);
    tira.setPixelColor(Pantalla[106], 0, 255, 0);
    tira.setPixelColor(Pantalla[105], 0, 255, 0);
    tira.setPixelColor(Pantalla[104], 0, 255, 0);
    tira.setPixelColor(Pantalla[103], 0, 255, 0);
    tira.setPixelColor(Pantalla[102], 0, 255, 0);
    tira.setPixelColor(Pantalla[101], 0, 255, 0);
    tira.setPixelColor(Pantalla[100], 0, 255, 0);
    tira.setPixelColor(Pantalla[99], 0, 255, 0);
    tira.setPixelColor(Pantalla[113], 0, 255, 0);
    tira.setPixelColor(Pantalla[116], 0, 255, 0);
    tira.setPixelColor(Pantalla[117], 255, 255, 0);
    tira.setPixelColor(Pantalla[118], 0, 255, 0);
    tira.setPixelColor(Pantalla[119], 0, 255, 0);
    tira.setPixelColor(Pantalla[120], 0, 255, 0);
    tira.setPixelColor(Pantalla[121], 255, 255, 0);
    tira.setPixelColor(Pantalla[122], 0, 255, 0);
    tira.setPixelColor(Pantalla[125], 0, 255, 0);
    tira.setPixelColor(Pantalla[143], 0, 255, 0);
    tira.setPixelColor(Pantalla[140], 0, 255, 0);
    tira.setPixelColor(Pantalla[138], 0, 255, 0);
    tira.setPixelColor(Pantalla[137], 0, 255, 0);
    tira.setPixelColor(Pantalla[136], 0, 255, 0);
    tira.setPixelColor(Pantalla[135], 0, 255, 0);
    tira.setPixelColor(Pantalla[134], 0, 255, 0);
    tira.setPixelColor(Pantalla[132], 0, 255, 0);
    tira.setPixelColor(Pantalla[129], 0, 255, 0);
    tira.setPixelColor(Pantalla[146], 0, 255, 0);
    tira.setPixelColor(Pantalla[149], 0, 255, 0);
    tira.setPixelColor(Pantalla[153], 0, 255, 0);
    tira.setPixelColor(Pantalla[156], 0, 255, 0);
    tira.setPixelColor(Pantalla[173], 0, 255, 0);
    tira.setPixelColor(Pantalla[169], 0, 255, 0);
    tira.setPixelColor(Pantalla[167], 0, 255, 0);
    tira.setPixelColor(Pantalla[163], 0, 255, 0);
    tira.setPixelColor(Pantalla[178], 0, 255, 0);
    tira.setPixelColor(Pantalla[188], 0, 255, 0);

    tira.setPixelColor(Pantalla[244], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
  }
  else
  {
    tira.setPixelColor(Pantalla[14], 0, 255, 0);
    tira.setPixelColor(Pantalla[8], 0, 0, 255);
    tira.setPixelColor(Pantalla[2], 0, 255, 0);
    tira.setPixelColor(Pantalla[18], 0, 255, 0);
    tira.setPixelColor(Pantalla[23], 0, 0, 255);
    tira.setPixelColor(Pantalla[28], 0, 255, 0);
    tira.setPixelColor(Pantalla[44], 0, 255, 0);
    tira.setPixelColor(Pantalla[42], 0, 255, 0);
    tira.setPixelColor(Pantalla[41], 0, 255, 0);
    tira.setPixelColor(Pantalla[40], 0, 255, 0);
    tira.setPixelColor(Pantalla[39], 0, 255, 0);
    tira.setPixelColor(Pantalla[38], 0, 255, 0);
    tira.setPixelColor(Pantalla[36], 0, 255, 0);
    tira.setPixelColor(Pantalla[48], 0, 255, 0);
    tira.setPixelColor(Pantalla[49], 0, 255, 0);
    tira.setPixelColor(Pantalla[52], 0, 255, 0);
    tira.setPixelColor(Pantalla[53], 0, 255, 0);
    tira.setPixelColor(Pantalla[54], 0, 255, 0);
    tira.setPixelColor(Pantalla[55], 0, 255, 0);
    tira.setPixelColor(Pantalla[56], 0, 255, 0);
    tira.setPixelColor(Pantalla[57], 0, 255, 0);
    tira.setPixelColor(Pantalla[58], 0, 255, 0);
    tira.setPixelColor(Pantalla[61], 0, 255, 0);
    tira.setPixelColor(Pantalla[62], 0, 255, 0);
    tira.setPixelColor(Pantalla[77], 0, 255, 0);
    tira.setPixelColor(Pantalla[76], 0, 255, 0);
    tira.setPixelColor(Pantalla[75], 0, 255, 0);
    tira.setPixelColor(Pantalla[74], 0, 255, 0);
    tira.setPixelColor(Pantalla[73], 0, 255, 0);
    tira.setPixelColor(Pantalla[72], 0, 255, 0);
    tira.setPixelColor(Pantalla[71], 0, 255, 0);
    tira.setPixelColor(Pantalla[70], 0, 255, 0);
    tira.setPixelColor(Pantalla[69], 0, 255, 0);
    tira.setPixelColor(Pantalla[68], 0, 255, 0);
    tira.setPixelColor(Pantalla[67], 0, 255, 0);
    tira.setPixelColor(Pantalla[84], 0, 255, 0);
    tira.setPixelColor(Pantalla[85], 0, 255, 0);
    tira.setPixelColor(Pantalla[86], 0, 255, 0);
    tira.setPixelColor(Pantalla[87], 0, 255, 0);
    tira.setPixelColor(Pantalla[88], 0, 255, 0);
    tira.setPixelColor(Pantalla[89], 0, 255, 0);
    tira.setPixelColor(Pantalla[90], 0, 255, 0);
    tira.setPixelColor(Pantalla[109], 0, 255, 0);
    tira.setPixelColor(Pantalla[108], 0, 255, 0);
    tira.setPixelColor(Pantalla[107], 0, 255, 0);
    tira.setPixelColor(Pantalla[106], 0, 255, 0);
    tira.setPixelColor(Pantalla[105], 0, 255, 0);
    tira.setPixelColor(Pantalla[104], 0, 255, 0);
    tira.setPixelColor(Pantalla[103], 0, 255, 0);
    tira.setPixelColor(Pantalla[102], 0, 255, 0);
    tira.setPixelColor(Pantalla[101], 0, 255, 0);
    tira.setPixelColor(Pantalla[100], 0, 255, 0);
    tira.setPixelColor(Pantalla[99], 0, 255, 0);
    tira.setPixelColor(Pantalla[113], 0, 255, 0);
    tira.setPixelColor(Pantalla[116], 0, 255, 0);
    tira.setPixelColor(Pantalla[117], 255, 255, 0);
    tira.setPixelColor(Pantalla[118], 0, 255, 0);
    tira.setPixelColor(Pantalla[119], 0, 255, 0);
    tira.setPixelColor(Pantalla[120], 0, 255, 0);
    tira.setPixelColor(Pantalla[121], 255, 255, 0);
    tira.setPixelColor(Pantalla[122], 0, 255, 0);
    tira.setPixelColor(Pantalla[125], 0, 255, 0);
    tira.setPixelColor(Pantalla[143], 0, 255, 0);
    tira.setPixelColor(Pantalla[140], 0, 255, 0);
    tira.setPixelColor(Pantalla[138], 0, 255, 0);
    tira.setPixelColor(Pantalla[137], 0, 255, 0);
    tira.setPixelColor(Pantalla[136], 0, 255, 0);
    tira.setPixelColor(Pantalla[135], 0, 255, 0);
    tira.setPixelColor(Pantalla[134], 0, 255, 0);
    tira.setPixelColor(Pantalla[132], 0, 255, 0);
    tira.setPixelColor(Pantalla[129], 0, 255, 0);
    tira.setPixelColor(Pantalla[146], 0, 255, 0);
    tira.setPixelColor(Pantalla[149], 0, 255, 0);
    tira.setPixelColor(Pantalla[153], 0, 255, 0);
    tira.setPixelColor(Pantalla[156], 0, 255, 0);
    tira.setPixelColor(Pantalla[173], 0, 255, 0);
    tira.setPixelColor(Pantalla[170], 0, 255, 0);
    tira.setPixelColor(Pantalla[166], 0, 255, 0);
    tira.setPixelColor(Pantalla[163], 0, 255, 0);
    tira.setPixelColor(Pantalla[179], 0, 255, 0);
    tira.setPixelColor(Pantalla[187], 0, 255, 0);

    tira.setPixelColor(Pantalla[244], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
  }
}
/*----------------------------------------------------------------------------*/

void Matriz_Lagartija() //Ver 4.0
{
  if (lado == IZQUIERDA)
  {

    tira.setPixelColor(Pantalla[8], 255, 0, 0);
    tira.setPixelColor(Pantalla[7], 255, 0, 0);
    tira.setPixelColor(Pantalla[6], 255, 0, 0);
    tira.setPixelColor(Pantalla[5], 255, 0, 0);
    tira.setPixelColor(Pantalla[23], 255, 0, 0);
    tira.setPixelColor(Pantalla[44], 0, 0, 255);
    tira.setPixelColor(Pantalla[41], 0, 0, 255);
    tira.setPixelColor(Pantalla[40], 255, 0, 0);
    tira.setPixelColor(Pantalla[39], 255, 0, 255);
    tira.setPixelColor(Pantalla[50], 0, 0, 255);
    tira.setPixelColor(Pantalla[51], 0, 0, 255);
    tira.setPixelColor(Pantalla[52], 0, 0, 255);
    tira.setPixelColor(Pantalla[53], 0, 0, 255);
    tira.setPixelColor(Pantalla[54], 0, 0, 255);
    tira.setPixelColor(Pantalla[55], 255, 0, 0);
    tira.setPixelColor(Pantalla[56], 255, 0, 255);
    tira.setPixelColor(Pantalla[57], 255, 0, 255);
    tira.setPixelColor(Pantalla[58], 255, 0, 255);
    tira.setPixelColor(Pantalla[59], 255, 0, 255);
    tira.setPixelColor(Pantalla[76], 0, 0, 255);
    tira.setPixelColor(Pantalla[73], 0, 0, 255);
    tira.setPixelColor(Pantalla[72], 255, 0, 0);
    tira.setPixelColor(Pantalla[71], 255, 0, 255);
    tira.setPixelColor(Pantalla[68], 255, 0, 255);
    tira.setPixelColor(Pantalla[86], 0, 0, 255);
    tira.setPixelColor(Pantalla[87], 255, 0, 0);
    tira.setPixelColor(Pantalla[88], 255, 0, 255);
    tira.setPixelColor(Pantalla[90], 255, 0, 255);
    tira.setPixelColor(Pantalla[91], 255, 0, 255);
    tira.setPixelColor(Pantalla[92], 255, 0, 255);
    tira.setPixelColor(Pantalla[105], 0, 0, 255);
    tira.setPixelColor(Pantalla[104], 255, 0, 0);
    tira.setPixelColor(Pantalla[103], 255, 0, 255);
    tira.setPixelColor(Pantalla[100], 255, 0, 255);
    tira.setPixelColor(Pantalla[118], 0, 0, 255);
    tira.setPixelColor(Pantalla[119], 255, 0, 0);
    tira.setPixelColor(Pantalla[120], 255, 0, 255);
    tira.setPixelColor(Pantalla[137], 0, 0, 255);
    tira.setPixelColor(Pantalla[136], 255, 0, 0);
    tira.setPixelColor(Pantalla[135], 255, 0, 255);
    tira.setPixelColor(Pantalla[132], 255, 0, 255);
    tira.setPixelColor(Pantalla[147], 0, 0, 255);
    tira.setPixelColor(Pantalla[148], 0, 0, 255);
    tira.setPixelColor(Pantalla[149], 0, 0, 255);
    tira.setPixelColor(Pantalla[150], 0, 0, 255);
    tira.setPixelColor(Pantalla[151], 255, 0, 0);
    tira.setPixelColor(Pantalla[152], 255, 0, 255);
    tira.setPixelColor(Pantalla[153], 255, 0, 255);
    tira.setPixelColor(Pantalla[154], 255, 0, 255);
    tira.setPixelColor(Pantalla[155], 255, 0, 255);
    tira.setPixelColor(Pantalla[156], 255, 0, 255);
    tira.setPixelColor(Pantalla[172], 0, 0, 255);
    tira.setPixelColor(Pantalla[168], 255, 0, 0);
    tira.setPixelColor(Pantalla[164], 255, 0, 255);
    tira.setPixelColor(Pantalla[178], 0, 0, 255);
    tira.setPixelColor(Pantalla[179], 0, 0, 255);
    tira.setPixelColor(Pantalla[180], 0, 0, 255);
    tira.setPixelColor(Pantalla[182], 255, 0, 0);
    tira.setPixelColor(Pantalla[183], 255, 0, 0);
    tira.setPixelColor(Pantalla[184], 255, 0, 0);
    tira.setPixelColor(Pantalla[204], 0, 0, 255);
    tira.setPixelColor(Pantalla[201], 0, 255, 0);
    tira.setPixelColor(Pantalla[200], 255, 0, 0);
    tira.setPixelColor(Pantalla[199], 0, 255, 0);
    tira.setPixelColor(Pantalla[215], 255, 0, 0);

    tira.setPixelColor(Pantalla[244], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
  }
  else
  {

    tira.setPixelColor(Pantalla[8], 255, 0, 0);
    tira.setPixelColor(Pantalla[7], 255, 0, 0);
    tira.setPixelColor(Pantalla[6], 255, 0, 0);
    tira.setPixelColor(Pantalla[5], 255, 0, 0);
    tira.setPixelColor(Pantalla[23], 255, 0, 0);
    tira.setPixelColor(Pantalla[41], 0, 0, 255);
    tira.setPixelColor(Pantalla[40], 255, 0, 0);
    tira.setPixelColor(Pantalla[39], 255, 0, 255);
    tira.setPixelColor(Pantalla[36], 255, 0, 255);
    tira.setPixelColor(Pantalla[51], 0, 0, 255);
    tira.setPixelColor(Pantalla[52], 0, 0, 255);
    tira.setPixelColor(Pantalla[53], 0, 0, 255);
    tira.setPixelColor(Pantalla[54], 0, 0, 255);
    tira.setPixelColor(Pantalla[55], 255, 0, 0);
    tira.setPixelColor(Pantalla[56], 255, 0, 255);
    tira.setPixelColor(Pantalla[57], 255, 0, 255);
    tira.setPixelColor(Pantalla[58], 255, 0, 255);
    tira.setPixelColor(Pantalla[59], 255, 0, 255);
    tira.setPixelColor(Pantalla[60], 255, 0, 255);
    tira.setPixelColor(Pantalla[76], 0, 0, 255);
    tira.setPixelColor(Pantalla[73], 0, 0, 255);
    tira.setPixelColor(Pantalla[72], 255, 0, 0);
    tira.setPixelColor(Pantalla[71], 255, 0, 255);
    tira.setPixelColor(Pantalla[68], 255, 0, 255);
    tira.setPixelColor(Pantalla[82], 0, 0, 255);
    tira.setPixelColor(Pantalla[83], 0, 0, 255);
    tira.setPixelColor(Pantalla[84], 0, 0, 255);
    tira.setPixelColor(Pantalla[86], 0, 0, 255);
    tira.setPixelColor(Pantalla[87], 255, 0, 0);
    tira.setPixelColor(Pantalla[88], 255, 0, 255);
    tira.setPixelColor(Pantalla[108], 0, 0, 255);
    tira.setPixelColor(Pantalla[105], 0, 0, 255);
    tira.setPixelColor(Pantalla[104], 255, 0, 0);
    tira.setPixelColor(Pantalla[103], 255, 0, 255);
    tira.setPixelColor(Pantalla[118], 0, 0, 255);
    tira.setPixelColor(Pantalla[119], 255, 0, 0);
    tira.setPixelColor(Pantalla[120], 255, 0, 255);
    tira.setPixelColor(Pantalla[140], 0, 0, 255);
    tira.setPixelColor(Pantalla[137], 0, 0, 255);
    tira.setPixelColor(Pantalla[136], 255, 0, 0);
    tira.setPixelColor(Pantalla[135], 255, 0, 255);
    tira.setPixelColor(Pantalla[146], 0, 0, 255);
    tira.setPixelColor(Pantalla[147], 0, 0, 255);
    tira.setPixelColor(Pantalla[148], 0, 0, 255);
    tira.setPixelColor(Pantalla[149], 0, 0, 255);
    tira.setPixelColor(Pantalla[150], 0, 0, 255);
    tira.setPixelColor(Pantalla[151], 255, 0, 0);
    tira.setPixelColor(Pantalla[152], 255, 0, 255);
    tira.setPixelColor(Pantalla[153], 255, 0, 255);
    tira.setPixelColor(Pantalla[154], 255, 0, 255);
    tira.setPixelColor(Pantalla[155], 255, 0, 255);
    tira.setPixelColor(Pantalla[172], 0, 0, 255);
    tira.setPixelColor(Pantalla[168], 255, 0, 0);
    tira.setPixelColor(Pantalla[164], 255, 0, 255);
    tira.setPixelColor(Pantalla[182], 255, 0, 0);
    tira.setPixelColor(Pantalla[183], 255, 0, 0);
    tira.setPixelColor(Pantalla[184], 255, 0, 0);
    tira.setPixelColor(Pantalla[186], 255, 0, 255);
    tira.setPixelColor(Pantalla[187], 255, 0, 255);
    tira.setPixelColor(Pantalla[188], 255, 0, 255);
    tira.setPixelColor(Pantalla[201], 100, 100, 100);
    tira.setPixelColor(Pantalla[200], 255, 0, 0);
    tira.setPixelColor(Pantalla[199], 100, 100, 100);
    tira.setPixelColor(Pantalla[196], 255, 0, 255);
    tira.setPixelColor(Pantalla[215], 255, 0, 0);

    tira.setPixelColor(Pantalla[244], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
  }
}
/*----------------------------------------------------------------------------*/

void Matriz_Mariposa() //Ver 4.0
{
  if (lado == IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[13], 0, 255, 255);
    tira.setPixelColor(Pantalla[12], 0, 255, 255);
    tira.setPixelColor(Pantalla[11], 0, 255, 255);
    tira.setPixelColor(Pantalla[4], 0, 255, 255);
    tira.setPixelColor(Pantalla[3], 0, 255, 255);
    tira.setPixelColor(Pantalla[2], 0, 255, 255);
    tira.setPixelColor(Pantalla[17], 0, 255, 255);
    tira.setPixelColor(Pantalla[18], 0, 255, 255);
    tira.setPixelColor(Pantalla[19], 0, 255, 255);
    tira.setPixelColor(Pantalla[20], 0, 255, 255);
    tira.setPixelColor(Pantalla[21], 0, 255, 255);
    tira.setPixelColor(Pantalla[26], 0, 255, 255);
    tira.setPixelColor(Pantalla[27], 0, 255, 255);
    tira.setPixelColor(Pantalla[28], 0, 255, 255);
    tira.setPixelColor(Pantalla[29], 0, 255, 255);
    tira.setPixelColor(Pantalla[30], 0, 255, 255);
    tira.setPixelColor(Pantalla[47], 0, 255, 255);
    tira.setPixelColor(Pantalla[46], 0, 255, 255);
    tira.setPixelColor(Pantalla[45], 0, 255, 255);
    tira.setPixelColor(Pantalla[44], 0, 255, 0);
    tira.setPixelColor(Pantalla[43], 0, 255, 255);
    tira.setPixelColor(Pantalla[42], 0, 255, 255);
    tira.setPixelColor(Pantalla[41], 0, 255, 255);
    tira.setPixelColor(Pantalla[38], 0, 255, 255);
    tira.setPixelColor(Pantalla[37], 0, 255, 255);
    tira.setPixelColor(Pantalla[36], 0, 255, 255);
    tira.setPixelColor(Pantalla[35], 0, 255, 255);
    tira.setPixelColor(Pantalla[34], 0, 255, 255);
    tira.setPixelColor(Pantalla[33], 0, 255, 255);
    tira.setPixelColor(Pantalla[32], 0, 255, 255);
    tira.setPixelColor(Pantalla[48], 0, 255, 255);
    tira.setPixelColor(Pantalla[49], 0, 255, 255);
    tira.setPixelColor(Pantalla[50], 0, 255, 255);
    tira.setPixelColor(Pantalla[51], 0, 255, 0);
    tira.setPixelColor(Pantalla[52], 0, 255, 255);
    tira.setPixelColor(Pantalla[53], 0, 255, 255);
    tira.setPixelColor(Pantalla[54], 0, 255, 255);
    tira.setPixelColor(Pantalla[55], 255, 0, 0);
    tira.setPixelColor(Pantalla[56], 255, 0, 0);
    tira.setPixelColor(Pantalla[57], 0, 255, 255);
    tira.setPixelColor(Pantalla[58], 0, 255, 255);
    tira.setPixelColor(Pantalla[59], 0, 255, 255);
    tira.setPixelColor(Pantalla[60], 0, 255, 0);
    tira.setPixelColor(Pantalla[61], 0, 255, 255);
    tira.setPixelColor(Pantalla[62], 0, 255, 255);
    tira.setPixelColor(Pantalla[63], 0, 255, 255);
    tira.setPixelColor(Pantalla[79], 0, 255, 255);
    tira.setPixelColor(Pantalla[78], 0, 255, 255);
    tira.setPixelColor(Pantalla[77], 0, 255, 0);
    tira.setPixelColor(Pantalla[76], 0, 255, 0);
    tira.setPixelColor(Pantalla[75], 0, 255, 0);
    tira.setPixelColor(Pantalla[74], 0, 255, 255);
    tira.setPixelColor(Pantalla[73], 0, 255, 255);
    tira.setPixelColor(Pantalla[72], 255, 0, 0);
    tira.setPixelColor(Pantalla[71], 255, 0, 0);
    tira.setPixelColor(Pantalla[70], 0, 255, 255);
    tira.setPixelColor(Pantalla[69], 0, 255, 255);
    tira.setPixelColor(Pantalla[68], 0, 255, 0);
    tira.setPixelColor(Pantalla[67], 0, 255, 0);
    tira.setPixelColor(Pantalla[66], 0, 255, 0);
    tira.setPixelColor(Pantalla[65], 0, 255, 255);
    tira.setPixelColor(Pantalla[64], 0, 255, 255);
    tira.setPixelColor(Pantalla[80], 0, 255, 255);
    tira.setPixelColor(Pantalla[81], 0, 255, 0);
    tira.setPixelColor(Pantalla[82], 0, 255, 0);
    tira.setPixelColor(Pantalla[83], 0, 255, 0);
    tira.setPixelColor(Pantalla[84], 0, 255, 0);
    tira.setPixelColor(Pantalla[85], 0, 255, 0);
    tira.setPixelColor(Pantalla[86], 0, 255, 255);
    tira.setPixelColor(Pantalla[87], 255, 0, 0);
    tira.setPixelColor(Pantalla[88], 255, 0, 0);
    tira.setPixelColor(Pantalla[89], 0, 255, 255);
    tira.setPixelColor(Pantalla[90], 0, 255, 0);
    tira.setPixelColor(Pantalla[91], 0, 255, 0);
    tira.setPixelColor(Pantalla[92], 0, 255, 0);
    tira.setPixelColor(Pantalla[93], 0, 255, 0);
    tira.setPixelColor(Pantalla[94], 0, 255, 0);
    tira.setPixelColor(Pantalla[95], 0, 255, 255);
    tira.setPixelColor(Pantalla[111], 0, 255, 255);
    tira.setPixelColor(Pantalla[110], 0, 255, 255);
    tira.setPixelColor(Pantalla[109], 0, 255, 0);
    tira.setPixelColor(Pantalla[108], 0, 255, 0);
    tira.setPixelColor(Pantalla[107], 0, 255, 0);
    tira.setPixelColor(Pantalla[106], 0, 255, 255);
    tira.setPixelColor(Pantalla[105], 0, 255, 255);
    tira.setPixelColor(Pantalla[104], 255, 0, 0);
    tira.setPixelColor(Pantalla[103], 255, 0, 0);
    tira.setPixelColor(Pantalla[102], 0, 255, 255);
    tira.setPixelColor(Pantalla[101], 0, 255, 255);
    tira.setPixelColor(Pantalla[100], 0, 255, 0);
    tira.setPixelColor(Pantalla[99], 0, 255, 0);
    tira.setPixelColor(Pantalla[98], 0, 255, 0);
    tira.setPixelColor(Pantalla[97], 0, 255, 255);
    tira.setPixelColor(Pantalla[96], 0, 255, 255);
    tira.setPixelColor(Pantalla[112], 0, 255, 255);
    tira.setPixelColor(Pantalla[113], 0, 255, 255);
    tira.setPixelColor(Pantalla[114], 0, 255, 255);
    tira.setPixelColor(Pantalla[115], 0, 255, 0);
    tira.setPixelColor(Pantalla[116], 0, 255, 255);
    tira.setPixelColor(Pantalla[117], 0, 255, 255);
    tira.setPixelColor(Pantalla[118], 0, 255, 255);
    tira.setPixelColor(Pantalla[119], 255, 0, 0);
    tira.setPixelColor(Pantalla[120], 255, 0, 0);
    tira.setPixelColor(Pantalla[121], 0, 255, 255);
    tira.setPixelColor(Pantalla[122], 0, 255, 255);
    tira.setPixelColor(Pantalla[123], 0, 255, 255);
    tira.setPixelColor(Pantalla[124], 0, 255, 0);
    tira.setPixelColor(Pantalla[125], 0, 255, 255);
    tira.setPixelColor(Pantalla[126], 0, 255, 255);
    tira.setPixelColor(Pantalla[127], 0, 255, 255);
    tira.setPixelColor(Pantalla[143], 0, 255, 255);
    tira.setPixelColor(Pantalla[142], 0, 255, 255);
    tira.setPixelColor(Pantalla[141], 0, 255, 255);
    tira.setPixelColor(Pantalla[140], 0, 255, 0);
    tira.setPixelColor(Pantalla[139], 0, 255, 255);
    tira.setPixelColor(Pantalla[138], 0, 255, 255);
    tira.setPixelColor(Pantalla[137], 0, 255, 255);
    tira.setPixelColor(Pantalla[136], 255, 0, 0);
    tira.setPixelColor(Pantalla[135], 255, 0, 0);
    tira.setPixelColor(Pantalla[134], 0, 255, 255);
    tira.setPixelColor(Pantalla[133], 0, 255, 255);
    tira.setPixelColor(Pantalla[132], 0, 255, 255);
    tira.setPixelColor(Pantalla[131], 0, 255, 0);
    tira.setPixelColor(Pantalla[130], 0, 255, 255);
    tira.setPixelColor(Pantalla[129], 0, 255, 255);
    tira.setPixelColor(Pantalla[128], 0, 255, 255);
    tira.setPixelColor(Pantalla[145], 0, 255, 255);
    tira.setPixelColor(Pantalla[146], 0, 255, 255);
    tira.setPixelColor(Pantalla[147], 0, 255, 0);
    tira.setPixelColor(Pantalla[148], 0, 255, 255);
    tira.setPixelColor(Pantalla[149], 0, 255, 255);
    tira.setPixelColor(Pantalla[150], 0, 255, 255);
    tira.setPixelColor(Pantalla[151], 255, 0, 0);
    tira.setPixelColor(Pantalla[152], 255, 0, 0);
    tira.setPixelColor(Pantalla[153], 0, 255, 255);
    tira.setPixelColor(Pantalla[154], 0, 255, 255);
    tira.setPixelColor(Pantalla[155], 0, 255, 255);
    tira.setPixelColor(Pantalla[156], 0, 255, 0);
    tira.setPixelColor(Pantalla[157], 0, 255, 255);
    tira.setPixelColor(Pantalla[158], 0, 255, 255);
    tira.setPixelColor(Pantalla[173], 0, 255, 255);
    tira.setPixelColor(Pantalla[172], 0, 255, 255);
    tira.setPixelColor(Pantalla[171], 0, 255, 255);
    tira.setPixelColor(Pantalla[170], 0, 255, 255);
    tira.setPixelColor(Pantalla[168], 255, 0, 0);
    tira.setPixelColor(Pantalla[167], 255, 0, 0);
    tira.setPixelColor(Pantalla[165], 0, 255, 255);
    tira.setPixelColor(Pantalla[164], 0, 255, 255);
    tira.setPixelColor(Pantalla[163], 0, 255, 255);
    tira.setPixelColor(Pantalla[162], 0, 255, 255);
    tira.setPixelColor(Pantalla[178], 0, 255, 255);
    tira.setPixelColor(Pantalla[179], 0, 255, 255);
    tira.setPixelColor(Pantalla[183], 255, 0, 0);
    tira.setPixelColor(Pantalla[184], 255, 0, 0);
    tira.setPixelColor(Pantalla[188], 0, 255, 255);
    tira.setPixelColor(Pantalla[189], 0, 255, 255);
    tira.setPixelColor(Pantalla[204], 0, 255, 255);
    tira.setPixelColor(Pantalla[200], 255, 0, 0);
    tira.setPixelColor(Pantalla[199], 255, 0, 0);
    tira.setPixelColor(Pantalla[195], 0, 255, 255);

    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
  }
  else
  {
    tira.setPixelColor(Pantalla[20], 0, 255, 255);
    tira.setPixelColor(Pantalla[27], 0, 255, 255);
    tira.setPixelColor(Pantalla[43], 0, 255, 255);
    tira.setPixelColor(Pantalla[42], 0, 255, 255);
    tira.setPixelColor(Pantalla[40], 255, 0, 0);
    tira.setPixelColor(Pantalla[39], 255, 0, 0);
    tira.setPixelColor(Pantalla[37], 0, 255, 255);
    tira.setPixelColor(Pantalla[36], 0, 255, 255);
    tira.setPixelColor(Pantalla[52], 0, 255, 255);
    tira.setPixelColor(Pantalla[53], 0, 255, 255);
    tira.setPixelColor(Pantalla[54], 0, 255, 255);
    tira.setPixelColor(Pantalla[55], 255, 0, 0);
    tira.setPixelColor(Pantalla[56], 255, 0, 0);
    tira.setPixelColor(Pantalla[57], 0, 255, 255);
    tira.setPixelColor(Pantalla[58], 0, 255, 255);
    tira.setPixelColor(Pantalla[59], 0, 255, 255);
    tira.setPixelColor(Pantalla[75], 0, 255, 0);
    tira.setPixelColor(Pantalla[74], 0, 255, 255);
    tira.setPixelColor(Pantalla[73], 0, 255, 255);
    tira.setPixelColor(Pantalla[72], 255, 0, 0);
    tira.setPixelColor(Pantalla[71], 255, 0, 0);
    tira.setPixelColor(Pantalla[70], 0, 255, 255);
    tira.setPixelColor(Pantalla[69], 0, 255, 255);
    tira.setPixelColor(Pantalla[68], 0, 255, 0);
    tira.setPixelColor(Pantalla[84], 0, 255, 0);
    tira.setPixelColor(Pantalla[85], 0, 255, 0);
    tira.setPixelColor(Pantalla[86], 0, 255, 255);
    tira.setPixelColor(Pantalla[87], 255, 0, 0);
    tira.setPixelColor(Pantalla[88], 255, 0, 0);
    tira.setPixelColor(Pantalla[89], 0, 255, 255);
    tira.setPixelColor(Pantalla[90], 0, 255, 0);
    tira.setPixelColor(Pantalla[91], 0, 255, 0);
    tira.setPixelColor(Pantalla[107], 0, 255, 0);
    tira.setPixelColor(Pantalla[106], 0, 255, 255);
    tira.setPixelColor(Pantalla[105], 0, 255, 255);
    tira.setPixelColor(Pantalla[104], 255, 0, 0);
    tira.setPixelColor(Pantalla[103], 255, 0, 0);
    tira.setPixelColor(Pantalla[102], 0, 255, 255);
    tira.setPixelColor(Pantalla[101], 0, 255, 255);
    tira.setPixelColor(Pantalla[100], 0, 255, 0);
    tira.setPixelColor(Pantalla[116], 0, 255, 255);
    tira.setPixelColor(Pantalla[117], 0, 255, 255);
    tira.setPixelColor(Pantalla[118], 0, 255, 255);
    tira.setPixelColor(Pantalla[119], 255, 0, 0);
    tira.setPixelColor(Pantalla[120], 255, 0, 0);
    tira.setPixelColor(Pantalla[121], 0, 255, 255);
    tira.setPixelColor(Pantalla[122], 0, 255, 255);
    tira.setPixelColor(Pantalla[123], 0, 255, 255);
    tira.setPixelColor(Pantalla[139], 0, 255, 255);
    tira.setPixelColor(Pantalla[138], 0, 255, 255);
    tira.setPixelColor(Pantalla[137], 0, 255, 255);
    tira.setPixelColor(Pantalla[136], 255, 0, 0);
    tira.setPixelColor(Pantalla[135], 255, 0, 0);
    tira.setPixelColor(Pantalla[134], 0, 255, 255);
    tira.setPixelColor(Pantalla[133], 0, 255, 255);
    tira.setPixelColor(Pantalla[132], 0, 255, 255);
    tira.setPixelColor(Pantalla[148], 0, 255, 255);
    tira.setPixelColor(Pantalla[149], 0, 255, 255);
    tira.setPixelColor(Pantalla[150], 0, 255, 255);
    tira.setPixelColor(Pantalla[151], 255, 0, 0);
    tira.setPixelColor(Pantalla[152], 255, 0, 0);
    tira.setPixelColor(Pantalla[153], 0, 255, 255);
    tira.setPixelColor(Pantalla[154], 0, 255, 255);
    tira.setPixelColor(Pantalla[155], 0, 255, 255);
    tira.setPixelColor(Pantalla[171], 0, 255, 255);
    tira.setPixelColor(Pantalla[170], 0, 255, 255);
    tira.setPixelColor(Pantalla[169], 0, 255, 255);
    tira.setPixelColor(Pantalla[168], 255, 0, 0);
    tira.setPixelColor(Pantalla[167], 255, 0, 0);
    tira.setPixelColor(Pantalla[166], 0, 255, 255);
    tira.setPixelColor(Pantalla[165], 0, 255, 255);
    tira.setPixelColor(Pantalla[164], 0, 255, 255);
    tira.setPixelColor(Pantalla[180], 0, 255, 255);
    tira.setPixelColor(Pantalla[181], 0, 255, 255);
    tira.setPixelColor(Pantalla[183], 255, 0, 0);
    tira.setPixelColor(Pantalla[184], 255, 0, 0);
    tira.setPixelColor(Pantalla[186], 0, 255, 255);
    tira.setPixelColor(Pantalla[187], 0, 255, 255);
    tira.setPixelColor(Pantalla[203], 0, 255, 255);
    tira.setPixelColor(Pantalla[202], 0, 255, 255);
    tira.setPixelColor(Pantalla[197], 0, 255, 255);
    tira.setPixelColor(Pantalla[196], 0, 255, 255);
    tira.setPixelColor(Pantalla[212], 0, 255, 255);
    tira.setPixelColor(Pantalla[219], 0, 255, 255);
    tira.setPixelColor(Pantalla[235], 0, 255, 255);
    tira.setPixelColor(Pantalla[228], 0, 255, 255);
    tira.setPixelColor(Pantalla[244], 0, 255, 255);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[251], 0, 255, 255);
  }
}
/*----------------------------------------------------------------------------*/

void Matriz_Corazon() //Ver 4.0
{
  tira.setPixelColor(Pantalla[3], 0, 255, 0);
  tira.setPixelColor(Pantalla[4], 0, 255, 0);
  tira.setPixelColor(Pantalla[5], 0, 255, 0);
  tira.setPixelColor(Pantalla[10], 0, 255, 0);
  tira.setPixelColor(Pantalla[11], 0, 255, 0);
  tira.setPixelColor(Pantalla[12], 0, 255, 0);
  tira.setPixelColor(Pantalla[29], 0, 255, 0);
  tira.setPixelColor(Pantalla[28], 0, 255, 0);
  tira.setPixelColor(Pantalla[27], 0, 255, 0);
  tira.setPixelColor(Pantalla[26], 0, 255, 0);
  tira.setPixelColor(Pantalla[25], 0, 255, 0);
  tira.setPixelColor(Pantalla[22], 0, 255, 0);
  tira.setPixelColor(Pantalla[21], 0, 255, 0);
  tira.setPixelColor(Pantalla[20], 0, 255, 0);
  tira.setPixelColor(Pantalla[19], 0, 255, 0);
  tira.setPixelColor(Pantalla[18], 0, 255, 0);
  tira.setPixelColor(Pantalla[33], 0, 255, 0);
  tira.setPixelColor(Pantalla[34], 0, 255, 0);
  tira.setPixelColor(Pantalla[35], 0, 255, 0);
  tira.setPixelColor(Pantalla[36], 0, 255, 0);
  tira.setPixelColor(Pantalla[37], 0, 255, 0);
  tira.setPixelColor(Pantalla[38], 0, 255, 0);
  tira.setPixelColor(Pantalla[39], 0, 255, 0);
  tira.setPixelColor(Pantalla[40], 0, 255, 0);
  tira.setPixelColor(Pantalla[41], 0, 255, 0);
  tira.setPixelColor(Pantalla[42], 0, 255, 0);
  tira.setPixelColor(Pantalla[43], 0, 255, 0);
  tira.setPixelColor(Pantalla[44], 0, 255, 0);
  tira.setPixelColor(Pantalla[45], 0, 255, 0);
  tira.setPixelColor(Pantalla[46], 0, 255, 0);
  tira.setPixelColor(Pantalla[62], 0, 255, 0);
  tira.setPixelColor(Pantalla[61], 0, 255, 0);
  tira.setPixelColor(Pantalla[60], 0, 255, 0);
  tira.setPixelColor(Pantalla[59], 0, 255, 0);
  tira.setPixelColor(Pantalla[58], 0, 255, 0);
  tira.setPixelColor(Pantalla[57], 0, 255, 0);
  tira.setPixelColor(Pantalla[56], 0, 255, 0);
  tira.setPixelColor(Pantalla[55], 0, 255, 0);
  tira.setPixelColor(Pantalla[54], 0, 255, 0);
  tira.setPixelColor(Pantalla[53], 0, 255, 0);
  tira.setPixelColor(Pantalla[52], 0, 255, 0);
  tira.setPixelColor(Pantalla[51], 0, 255, 0);
  tira.setPixelColor(Pantalla[50], 0, 255, 0);
  tira.setPixelColor(Pantalla[49], 0, 255, 0);
  tira.setPixelColor(Pantalla[65], 0, 255, 0);
  tira.setPixelColor(Pantalla[66], 0, 255, 0);
  tira.setPixelColor(Pantalla[67], 0, 255, 0);
  tira.setPixelColor(Pantalla[68], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[69], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[70], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[71], 0, 255, 0);
  tira.setPixelColor(Pantalla[72], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[73], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[74], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[75], 0, 255, 0);
  tira.setPixelColor(Pantalla[76], 0, 255, 0);
  tira.setPixelColor(Pantalla[77], 0, 255, 0);
  tira.setPixelColor(Pantalla[78], 0, 255, 0);
  tira.setPixelColor(Pantalla[94], 0, 255, 0);
  tira.setPixelColor(Pantalla[93], 0, 255, 0);
  tira.setPixelColor(Pantalla[92], 0, 255, 0);
  tira.setPixelColor(Pantalla[91], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[90], 0, 255, 0);
  tira.setPixelColor(Pantalla[89], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[88], 0, 255, 0);
  tira.setPixelColor(Pantalla[87], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[86], 0, 255, 0);
  tira.setPixelColor(Pantalla[85], 0, 255, 0);
  tira.setPixelColor(Pantalla[84], 0, 255, 0);
  tira.setPixelColor(Pantalla[83], 0, 255, 0);
  tira.setPixelColor(Pantalla[82], 0, 255, 0);
  tira.setPixelColor(Pantalla[81], 0, 255, 0);
  tira.setPixelColor(Pantalla[97], 0, 255, 0);
  tira.setPixelColor(Pantalla[98], 0, 255, 0);
  tira.setPixelColor(Pantalla[99], 0, 255, 0);
  tira.setPixelColor(Pantalla[100], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[101], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[102], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[103], 0, 255, 0);
  tira.setPixelColor(Pantalla[104], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[105], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[106], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[107], 0, 255, 0);
  tira.setPixelColor(Pantalla[108], 0, 255, 0);
  tira.setPixelColor(Pantalla[109], 0, 255, 0);
  tira.setPixelColor(Pantalla[110], 0, 255, 0);
  tira.setPixelColor(Pantalla[125], 0, 255, 0);
  tira.setPixelColor(Pantalla[124], 0, 255, 0);
  tira.setPixelColor(Pantalla[123], 0, 255, 0);
  tira.setPixelColor(Pantalla[122], 0, 255, 0);
  tira.setPixelColor(Pantalla[121], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[120], 0, 255, 0);
  tira.setPixelColor(Pantalla[119], 0, 255, 0);
  tira.setPixelColor(Pantalla[118], 0, 255, 0);
  tira.setPixelColor(Pantalla[117], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[116], 0, 255, 0);
  tira.setPixelColor(Pantalla[115], 0, 255, 0);
  tira.setPixelColor(Pantalla[114], 0, 255, 0);
  tira.setPixelColor(Pantalla[130], 0, 255, 0);
  tira.setPixelColor(Pantalla[131], 0, 255, 0);
  tira.setPixelColor(Pantalla[132], 0, 255, 0);
  tira.setPixelColor(Pantalla[133], 0, 255, 0);
  tira.setPixelColor(Pantalla[134], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[135], 0, 255, 0);
  tira.setPixelColor(Pantalla[136], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[137], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[138], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[139], 0, 255, 0);
  tira.setPixelColor(Pantalla[140], 0, 255, 0);
  tira.setPixelColor(Pantalla[141], 0, 255, 0);
  tira.setPixelColor(Pantalla[156], 0, 255, 0);
  tira.setPixelColor(Pantalla[155], 0, 255, 0);
  tira.setPixelColor(Pantalla[154], 0, 255, 0);
  tira.setPixelColor(Pantalla[153], 0, 255, 0);
  tira.setPixelColor(Pantalla[152], 0, 255, 0);
  tira.setPixelColor(Pantalla[151], 0, 255, 0);
  tira.setPixelColor(Pantalla[150], 0, 255, 0);
  tira.setPixelColor(Pantalla[149], 0, 255, 0);
  tira.setPixelColor(Pantalla[148], 0, 255, 0);
  tira.setPixelColor(Pantalla[147], 0, 255, 0);
  tira.setPixelColor(Pantalla[164], 0, 255, 0);
  tira.setPixelColor(Pantalla[165], 0, 255, 0);
  tira.setPixelColor(Pantalla[166], 0, 255, 0);
  tira.setPixelColor(Pantalla[167], 0, 255, 0);
  tira.setPixelColor(Pantalla[168], 0, 255, 0);
  tira.setPixelColor(Pantalla[169], 0, 255, 0);
  tira.setPixelColor(Pantalla[170], 0, 255, 0);
  tira.setPixelColor(Pantalla[171], 0, 255, 0);
  tira.setPixelColor(Pantalla[186], 0, 255, 0);
  tira.setPixelColor(Pantalla[185], 0, 255, 0);
  tira.setPixelColor(Pantalla[184], 0, 255, 0);
  tira.setPixelColor(Pantalla[183], 0, 255, 0);
  tira.setPixelColor(Pantalla[182], 0, 255, 0);
  tira.setPixelColor(Pantalla[181], 0, 255, 0);
  tira.setPixelColor(Pantalla[198], 0, 255, 0);
  tira.setPixelColor(Pantalla[199], 0, 255, 0);
  tira.setPixelColor(Pantalla[200], 0, 255, 0);
  tira.setPixelColor(Pantalla[201], 0, 255, 0);
  tira.setPixelColor(Pantalla[216], 0, 255, 0);
  tira.setPixelColor(Pantalla[215], 0, 255, 0);
}
/*----------------------------------------------------------------------------*/

void Matriz_Ilumina_Mira_Aleatorio() //Ver 4.0
{
  if (lado== IZQUIERDA)
  {
  tira.setPixelColor(Pantalla[256], 255, 0, 0);
  tira.setPixelColor(Pantalla[257], 255, 0, 0);
  tira.setPixelColor(Pantalla[258], 0, 0, 255);
  tira.setPixelColor(Pantalla[259], 0,0,255);
  tira.setPixelColor(Pantalla[260], 255, 0, 0);
  tira.setPixelColor(Pantalla[261],255, 0, 0); 
  }
  else{
  tira.setPixelColor(Pantalla[256], 0, 0, 255);
  tira.setPixelColor(Pantalla[257], 0, 0, 255);
  tira.setPixelColor(Pantalla[258], 255, 0, 0);
  tira.setPixelColor(Pantalla[259], 255,0,0);
  tira.setPixelColor(Pantalla[260], 0, 0,255);
  tira.setPixelColor(Pantalla[261], 0, 0, 255);  
  }

  // tira.setPixelColor(Pantalla[256], (rand() % 245), (rand() % 245), (rand() % 245));
  // tira.setPixelColor(Pantalla[257], (rand() % 245), (rand() % 245), (rand() % 245));
  // tira.setPixelColor(Pantalla[258], (rand() % 245), (rand() % 245), (rand() % 245));
  // tira.setPixelColor(Pantalla[259], (rand() % 245), (rand() % 245), ( rand() % 245));
  // tira.setPixelColor(Pantalla[260], (rand() % 245), ( rand() % 245), (rand() % 245));
  // tira.setPixelColor(Pantalla[261], (rand() % 245), (rand() % 245), (rand() % 245));

}
/*----------------------------------------------------------------------------*/

void Matriz_Oso() //Ver 4.0
{

  if (lado == IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[17], 100, 100, 100);
    tira.setPixelColor(Pantalla[18], 100, 100, 100);
    tira.setPixelColor(Pantalla[19], 100, 100, 100);
    tira.setPixelColor(Pantalla[20], 100, 100, 100);
    tira.setPixelColor(Pantalla[27], 100, 100, 100);
    tira.setPixelColor(Pantalla[28], 100, 100, 100);
    tira.setPixelColor(Pantalla[29], 100, 100, 100);
    tira.setPixelColor(Pantalla[30], 100, 100, 100);
    tira.setPixelColor(Pantalla[46], 100, 100, 100);
    tira.setPixelColor(Pantalla[43], 100, 100, 100);
    tira.setPixelColor(Pantalla[37], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[36], 100, 100, 100);
    tira.setPixelColor(Pantalla[33], 100, 100, 100);
    tira.setPixelColor(Pantalla[49], 100, 100, 100);
    tira.setPixelColor(Pantalla[52], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], 105, 210, 30);
    tira.setPixelColor(Pantalla[54], 100, 100, 100);
    tira.setPixelColor(Pantalla[55], 100, 100, 100);
    tira.setPixelColor(Pantalla[56], 100, 100, 100);
    tira.setPixelColor(Pantalla[57], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[58], 105, 210, 30);
    tira.setPixelColor(Pantalla[59], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[62], 100, 100, 100);
    tira.setPixelColor(Pantalla[78], 100, 100, 100);
    tira.setPixelColor(Pantalla[77], 100, 100, 100);
    tira.setPixelColor(Pantalla[76], 100, 100, 100);
    tira.setPixelColor(Pantalla[75], 105, 210, 30);
    tira.setPixelColor(Pantalla[74], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], 100, 100, 100);
    tira.setPixelColor(Pantalla[72], 100, 100, 100);
    tira.setPixelColor(Pantalla[71], 100, 100, 100);
    tira.setPixelColor(Pantalla[70], 100, 100, 100);
    tira.setPixelColor(Pantalla[69], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[68], 105, 210, 30);
    tira.setPixelColor(Pantalla[67], 100, 100, 100);
    tira.setPixelColor(Pantalla[66], 100, 100, 100);
    tira.setPixelColor(Pantalla[65], 100, 100, 100);
    tira.setPixelColor(Pantalla[83], 105, 210, 30);
    tira.setPixelColor(Pantalla[84], 100, 100, 100);
    tira.setPixelColor(Pantalla[85], 100, 100, 100);
    tira.setPixelColor(Pantalla[86], 100, 100, 100);
    tira.setPixelColor(Pantalla[87], 100, 100, 100);
    tira.setPixelColor(Pantalla[88], 100, 100, 100);
    tira.setPixelColor(Pantalla[89], 100, 100, 100);
    tira.setPixelColor(Pantalla[90], 100, 100, 100);
    tira.setPixelColor(Pantalla[91], 100, 100, 100);
    tira.setPixelColor(Pantalla[92], 105, 210, 30);
    tira.setPixelColor(Pantalla[108], 100, 100, 100);
    tira.setPixelColor(Pantalla[107], 100, 100, 100);
    tira.setPixelColor(Pantalla[106], 100, 100, 100);
    tira.setPixelColor(Pantalla[105], 100, 100, 100);
    tira.setPixelColor(Pantalla[104], 100, 100, 100);
    tira.setPixelColor(Pantalla[103], 100, 100, 100);
    tira.setPixelColor(Pantalla[102], 100, 100, 100);
    tira.setPixelColor(Pantalla[101], 100, 100, 100);
    tira.setPixelColor(Pantalla[100], 100, 100, 100);
    tira.setPixelColor(Pantalla[99], 100, 100, 100);
    tira.setPixelColor(Pantalla[115], 100, 100, 100);
    tira.setPixelColor(Pantalla[116], 100, 100, 100);
    tira.setPixelColor(Pantalla[118], 100, 100, 100);
    tira.setPixelColor(Pantalla[119], 100, 100, 100);
    tira.setPixelColor(Pantalla[120], 100, 100, 100);
    tira.setPixelColor(Pantalla[121], 100, 100, 100);
    tira.setPixelColor(Pantalla[123], 100, 100, 100);
    tira.setPixelColor(Pantalla[124], 100, 100, 100);
    tira.setPixelColor(Pantalla[140], 100, 100, 100);
    tira.setPixelColor(Pantalla[139], 100, 100, 100);
    tira.setPixelColor(Pantalla[138], 100, 100, 100);
    tira.setPixelColor(Pantalla[137], 100, 100, 100);
    tira.setPixelColor(Pantalla[136], 100, 100, 100);
    tira.setPixelColor(Pantalla[135], 100, 100, 100);
    tira.setPixelColor(Pantalla[134], 100, 100, 100);
    tira.setPixelColor(Pantalla[133], 100, 100, 100);
    tira.setPixelColor(Pantalla[132], 100, 100, 100);
    tira.setPixelColor(Pantalla[131], 100, 100, 100);
    tira.setPixelColor(Pantalla[147], 100, 100, 100);
    tira.setPixelColor(Pantalla[148], 100, 100, 100);
    tira.setPixelColor(Pantalla[149], 100, 100, 100);
    tira.setPixelColor(Pantalla[150], 100, 100, 100);
    tira.setPixelColor(Pantalla[153], 100, 100, 100);
    tira.setPixelColor(Pantalla[154], 100, 100, 100);
    tira.setPixelColor(Pantalla[155], 100, 100, 100);
    tira.setPixelColor(Pantalla[156], 100, 100, 100);
    tira.setPixelColor(Pantalla[172], 100, 100, 100);
    tira.setPixelColor(Pantalla[171], 100, 100, 100);
    tira.setPixelColor(Pantalla[170], 100, 100, 100);
    tira.setPixelColor(Pantalla[169], 100, 100, 100);
    tira.setPixelColor(Pantalla[168], 100, 100, 100);
    tira.setPixelColor(Pantalla[167], 100, 100, 100);
    tira.setPixelColor(Pantalla[166], 100, 100, 100);
    tira.setPixelColor(Pantalla[165], 100, 100, 100);
    tira.setPixelColor(Pantalla[164], 100, 100, 100);
    tira.setPixelColor(Pantalla[163], 100, 100, 100);
    tira.setPixelColor(Pantalla[180], 100, 100, 100);
    tira.setPixelColor(Pantalla[181], 100, 100, 100);
    tira.setPixelColor(Pantalla[182], 100, 100, 100);
    tira.setPixelColor(Pantalla[183], 100, 100, 100);
    tira.setPixelColor(Pantalla[184], 100, 100, 100);
    tira.setPixelColor(Pantalla[185], 100, 100, 100);
    tira.setPixelColor(Pantalla[186], 100, 100, 100);
    tira.setPixelColor(Pantalla[187], 100, 100, 100);
    tira.setPixelColor(Pantalla[202], 100, 100, 100);
    tira.setPixelColor(Pantalla[201], 100, 100, 100);
    tira.setPixelColor(Pantalla[200], 100, 100, 100);
    tira.setPixelColor(Pantalla[199], 100, 100, 100);
    tira.setPixelColor(Pantalla[198], 100, 100, 100);
    tira.setPixelColor(Pantalla[197], 100, 100, 100);
    tira.setPixelColor(Pantalla[214], 105, 210, 30);
    tira.setPixelColor(Pantalla[215], 105, 210, 30);
    tira.setPixelColor(Pantalla[216], 105, 210, 30);
    tira.setPixelColor(Pantalla[217], 105, 210, 30);

    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
  }

  else
  {
    tira.setPixelColor(Pantalla[17], 100, 100, 100);
    tira.setPixelColor(Pantalla[18], 100, 100, 100);
    tira.setPixelColor(Pantalla[19], 100, 100, 100);
    tira.setPixelColor(Pantalla[20], 100, 100, 100);
    tira.setPixelColor(Pantalla[27], 100, 100, 100);
    tira.setPixelColor(Pantalla[28], 100, 100, 100);
    tira.setPixelColor(Pantalla[29], 100, 100, 100);
    tira.setPixelColor(Pantalla[30], 100, 100, 100);
    tira.setPixelColor(Pantalla[46], 100, 100, 100);
    tira.setPixelColor(Pantalla[43], 100, 100, 100);
    tira.setPixelColor(Pantalla[37], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[36], 100, 100, 100);
    tira.setPixelColor(Pantalla[33], 100, 100, 100);
    tira.setPixelColor(Pantalla[49], 100, 100, 100);
    tira.setPixelColor(Pantalla[52], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], 105, 210, 30);
    tira.setPixelColor(Pantalla[54], 100, 100, 100);
    tira.setPixelColor(Pantalla[55], 100, 100, 100);
    tira.setPixelColor(Pantalla[56], 100, 100, 100);
    tira.setPixelColor(Pantalla[57], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[58], 105, 210, 30);
    tira.setPixelColor(Pantalla[59], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[62], 100, 100, 100);
    tira.setPixelColor(Pantalla[78], 100, 100, 100);
    tira.setPixelColor(Pantalla[77], 100, 100, 100);
    tira.setPixelColor(Pantalla[76], 100, 100, 100);
    tira.setPixelColor(Pantalla[75], 105, 210, 30);
    tira.setPixelColor(Pantalla[74], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], 100, 100, 100);
    tira.setPixelColor(Pantalla[72], 100, 100, 100);
    tira.setPixelColor(Pantalla[71], 100, 100, 100);
    tira.setPixelColor(Pantalla[70], 100, 100, 100);
    tira.setPixelColor(Pantalla[69], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[68], 105, 210, 30);
    tira.setPixelColor(Pantalla[67], 100, 100, 100);
    tira.setPixelColor(Pantalla[66], 100, 100, 100);
    tira.setPixelColor(Pantalla[65], 100, 100, 100);
    tira.setPixelColor(Pantalla[83], 105, 210, 30);
    tira.setPixelColor(Pantalla[84], 100, 100, 100);
    tira.setPixelColor(Pantalla[85], 100, 100, 100);
    tira.setPixelColor(Pantalla[86], 100, 100, 100);
    tira.setPixelColor(Pantalla[87], 100, 100, 100);
    tira.setPixelColor(Pantalla[88], 100, 100, 100);
    tira.setPixelColor(Pantalla[89], 100, 100, 100);
    tira.setPixelColor(Pantalla[90], 100, 100, 100);
    tira.setPixelColor(Pantalla[91], 100, 100, 100);
    tira.setPixelColor(Pantalla[92], 105, 210, 30);
    tira.setPixelColor(Pantalla[108], 100, 100, 100);
    tira.setPixelColor(Pantalla[107], 100, 100, 100);
    tira.setPixelColor(Pantalla[106], 100, 100, 100);
    tira.setPixelColor(Pantalla[105], 100, 100, 100);
    tira.setPixelColor(Pantalla[104], 100, 100, 100);
    tira.setPixelColor(Pantalla[103], 100, 100, 100);
    tira.setPixelColor(Pantalla[102], 100, 100, 100);
    tira.setPixelColor(Pantalla[101], 100, 100, 100);
    tira.setPixelColor(Pantalla[100], 100, 100, 100);
    tira.setPixelColor(Pantalla[99], 100, 100, 100);
    tira.setPixelColor(Pantalla[115], 100, 100, 100);
    tira.setPixelColor(Pantalla[116], 100, 100, 100);
    tira.setPixelColor(Pantalla[117], 0, 255, 0);
    tira.setPixelColor(Pantalla[118], 100, 100, 100);
    tira.setPixelColor(Pantalla[119], 100, 100, 100);
    tira.setPixelColor(Pantalla[120], 100, 100, 100);
    tira.setPixelColor(Pantalla[121], 100, 100, 100);
    tira.setPixelColor(Pantalla[122], 0, 255, 0);
    tira.setPixelColor(Pantalla[123], 100, 100, 100);
    tira.setPixelColor(Pantalla[124], 100, 100, 100);
    tira.setPixelColor(Pantalla[140], 100, 100, 100);
    tira.setPixelColor(Pantalla[139], 100, 100, 100);
    tira.setPixelColor(Pantalla[138], 100, 100, 100);
    tira.setPixelColor(Pantalla[137], 100, 100, 100);
    tira.setPixelColor(Pantalla[136], 100, 100, 100);
    tira.setPixelColor(Pantalla[135], 100, 100, 100);
    tira.setPixelColor(Pantalla[134], 100, 100, 100);
    tira.setPixelColor(Pantalla[133], 100, 100, 100);
    tira.setPixelColor(Pantalla[132], 100, 100, 100);
    tira.setPixelColor(Pantalla[131], 100, 100, 100);
    tira.setPixelColor(Pantalla[147], 100, 100, 100);
    tira.setPixelColor(Pantalla[148], 100, 100, 100);
    tira.setPixelColor(Pantalla[149], 100, 100, 100);
    tira.setPixelColor(Pantalla[150], 100, 100, 100);
    tira.setPixelColor(Pantalla[153], 100, 100, 100);
    tira.setPixelColor(Pantalla[154], 100, 100, 100);
    tira.setPixelColor(Pantalla[155], 100, 100, 100);
    tira.setPixelColor(Pantalla[156], 100, 100, 100);
    tira.setPixelColor(Pantalla[172], 100, 100, 100);
    tira.setPixelColor(Pantalla[171], 100, 100, 100);
    tira.setPixelColor(Pantalla[170], 100, 100, 100);
    tira.setPixelColor(Pantalla[169], 100, 100, 100);
    tira.setPixelColor(Pantalla[168], 100, 100, 100);
    tira.setPixelColor(Pantalla[167], 100, 100, 100);
    tira.setPixelColor(Pantalla[166], 100, 100, 100);
    tira.setPixelColor(Pantalla[165], 100, 100, 100);
    tira.setPixelColor(Pantalla[164], 100, 100, 100);
    tira.setPixelColor(Pantalla[163], 100, 100, 100);
    tira.setPixelColor(Pantalla[180], 100, 100, 100);
    tira.setPixelColor(Pantalla[181], 100, 100, 100);
    tira.setPixelColor(Pantalla[182], 100, 100, 100);
    tira.setPixelColor(Pantalla[183], 0, 255, 0);
    tira.setPixelColor(Pantalla[184], 0, 255, 0);
    tira.setPixelColor(Pantalla[185], 100, 100, 100);
    tira.setPixelColor(Pantalla[186], 100, 100, 100);
    tira.setPixelColor(Pantalla[187], 100, 100, 100);
    tira.setPixelColor(Pantalla[202], 100, 100, 100);
    tira.setPixelColor(Pantalla[201], 100, 100, 100);
    tira.setPixelColor(Pantalla[200], 100, 100, 100);
    tira.setPixelColor(Pantalla[199], 100, 100, 100);
    tira.setPixelColor(Pantalla[198], 100, 100, 100);
    tira.setPixelColor(Pantalla[197], 100, 100, 100);
    tira.setPixelColor(Pantalla[214], 105, 210, 30);
    tira.setPixelColor(Pantalla[215], 105, 210, 30);
    tira.setPixelColor(Pantalla[216], 105, 210, 30);
    tira.setPixelColor(Pantalla[217], 105, 210, 30);

    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
  }
}
/*----------------------------------------------------------------------------*/

void Matriz_Rana(){
  if (lado==IZQUIERDA){
tira.setPixelColor(Pantalla[ 28],100,100,100);tira.setPixelColor(Pantalla[ 27],100,100,100);tira.setPixelColor(Pantalla[ 26],100,100,100);tira.setPixelColor(Pantalla[ 25],0,0,255);tira.setPixelColor(Pantalla[ 22],0,0,255);tira.setPixelColor(Pantalla[ 21],100,100,100);tira.setPixelColor(Pantalla[ 20],100,100,100);tira.setPixelColor(Pantalla[ 19],100,100,100);
tira.setPixelColor(Pantalla[ 35],100,100,100);tira.setPixelColor(Pantalla[ 36],0,0,0);tira.setPixelColor(Pantalla[ 37],100,100,100);tira.setPixelColor(Pantalla[ 38],0,0,255);tira.setPixelColor(Pantalla[ 39],255,0,0);tira.setPixelColor(Pantalla[ 40],255,0,0);tira.setPixelColor(Pantalla[ 41],0,0,255);tira.setPixelColor(Pantalla[ 42],100,100,100);tira.setPixelColor(Pantalla[ 43],0,0,0);tira.setPixelColor(Pantalla[ 44],100,100,100);
tira.setPixelColor(Pantalla[ 60],100,100,100);tira.setPixelColor(Pantalla[ 59],100,100,100);tira.setPixelColor(Pantalla[ 58],100,100,100);tira.setPixelColor(Pantalla[ 57],0,0,255);tira.setPixelColor(Pantalla[ 56],255,0,0);tira.setPixelColor(Pantalla[ 55],255,0,0);tira.setPixelColor(Pantalla[ 54],0,0,255);tira.setPixelColor(Pantalla[ 53],100,100,100);tira.setPixelColor(Pantalla[ 52],100,100,100);tira.setPixelColor(Pantalla[ 51],100,100,100);
tira.setPixelColor(Pantalla[ 67],0,0,255);tira.setPixelColor(Pantalla[ 68],0,0,255);tira.setPixelColor(Pantalla[ 69],0,0,255);tira.setPixelColor(Pantalla[ 70],0,0,255);tira.setPixelColor(Pantalla[ 71],255,0,0);tira.setPixelColor(Pantalla[ 72],255,0,0);tira.setPixelColor(Pantalla[ 73],0,0,255);tira.setPixelColor(Pantalla[ 74],0,0,255);tira.setPixelColor(Pantalla[ 75],0,0,255);tira.setPixelColor(Pantalla[ 76],0,0,255);
tira.setPixelColor(Pantalla[ 93],255,0,0);tira.setPixelColor(Pantalla[ 92],255,0,0);tira.setPixelColor(Pantalla[ 91],255,0,0);tira.setPixelColor(Pantalla[ 90],255,0,0);tira.setPixelColor(Pantalla[ 89],255,0,0);tira.setPixelColor(Pantalla[ 88],255,0,0);tira.setPixelColor(Pantalla[ 87],255,0,0);tira.setPixelColor(Pantalla[ 86],255,0,0);tira.setPixelColor(Pantalla[ 85],255,0,0);tira.setPixelColor(Pantalla[ 84],255,0,0);tira.setPixelColor(Pantalla[ 83],255,0,0);tira.setPixelColor(Pantalla[ 82],255,0,0);
tira.setPixelColor(Pantalla[ 97],255,0,0);tira.setPixelColor(Pantalla[ 98],255,0,0);tira.setPixelColor(Pantalla[ 99],255,0,0);tira.setPixelColor(Pantalla[ 100],255,0,0);tira.setPixelColor(Pantalla[ 101],255,0,0);tira.setPixelColor(Pantalla[ 102],255,0,0);tira.setPixelColor(Pantalla[ 103],255,0,0);tira.setPixelColor(Pantalla[ 104],255,0,0);tira.setPixelColor(Pantalla[ 105],255,0,0);tira.setPixelColor(Pantalla[ 106],255,0,0);tira.setPixelColor(Pantalla[ 107],255,0,0);tira.setPixelColor(Pantalla[ 108],255,0,0);tira.setPixelColor(Pantalla[ 109],255,0,0);tira.setPixelColor(Pantalla[ 110],255,0,0);
tira.setPixelColor(Pantalla[ 126],255,0,0);tira.setPixelColor(Pantalla[ 125],255,0,0);tira.setPixelColor(Pantalla[ 124],255,0,0);tira.setPixelColor(Pantalla[ 123],255,0,0);tira.setPixelColor(Pantalla[ 122],255,0,0);tira.setPixelColor(Pantalla[ 121],255,0,0);tira.setPixelColor(Pantalla[ 120],0,0,0);tira.setPixelColor(Pantalla[ 119],0,0,0);tira.setPixelColor(Pantalla[ 118],255,0,0);tira.setPixelColor(Pantalla[ 117],255,0,0);tira.setPixelColor(Pantalla[ 116],255,0,0);tira.setPixelColor(Pantalla[ 115],255,0,0);tira.setPixelColor(Pantalla[ 114],255,0,0);tira.setPixelColor(Pantalla[ 113],255,0,0);
tira.setPixelColor(Pantalla[ 130],255,0,0);tira.setPixelColor(Pantalla[ 131],255,0,0);tira.setPixelColor(Pantalla[ 132],255,0,0);tira.setPixelColor(Pantalla[ 133],255,0,0);tira.setPixelColor(Pantalla[ 134],255,0,0);tira.setPixelColor(Pantalla[ 135],255,0,0);tira.setPixelColor(Pantalla[ 136],255,0,0);tira.setPixelColor(Pantalla[ 137],255,0,0);tira.setPixelColor(Pantalla[ 138],255,0,0);tira.setPixelColor(Pantalla[ 139],255,0,0);tira.setPixelColor(Pantalla[ 140],255,0,0);tira.setPixelColor(Pantalla[ 141],255,0,0);
tira.setPixelColor(Pantalla[ 154],255,0,0);tira.setPixelColor(Pantalla[ 153],255,0,0);tira.setPixelColor(Pantalla[ 152],255,0,0);tira.setPixelColor(Pantalla[ 151],255,0,0);tira.setPixelColor(Pantalla[ 150],255,0,0);tira.setPixelColor(Pantalla[ 149],255,0,0);
tira.setPixelColor(Pantalla[ 164],255,0,0);tira.setPixelColor(Pantalla[ 167],255,0,0);tira.setPixelColor(Pantalla[ 168],255,0,0);tira.setPixelColor(Pantalla[ 171],255,0,0);
tira.setPixelColor(Pantalla[ 188],255,0,0);tira.setPixelColor(Pantalla[ 187],255,0,0);tira.setPixelColor(Pantalla[ 186],255,0,0);tira.setPixelColor(Pantalla[ 185],255,0,0);tira.setPixelColor(Pantalla[ 184],100,100,100);tira.setPixelColor(Pantalla[ 183],100,100,100);tira.setPixelColor(Pantalla[ 182],255,0,0);tira.setPixelColor(Pantalla[ 181],255,0,0);tira.setPixelColor(Pantalla[ 180],255,0,0);tira.setPixelColor(Pantalla[ 179],255,0,0);
tira.setPixelColor(Pantalla[ 193],255,0,0);tira.setPixelColor(Pantalla[ 195],255,0,0);tira.setPixelColor(Pantalla[ 196],255,0,0);tira.setPixelColor(Pantalla[ 197],255,0,0);tira.setPixelColor(Pantalla[ 198],255,0,0);tira.setPixelColor(Pantalla[ 199],100,100,100);tira.setPixelColor(Pantalla[ 200],100,100,100);tira.setPixelColor(Pantalla[ 201],255,0,0);tira.setPixelColor(Pantalla[ 202],255,0,0);tira.setPixelColor(Pantalla[ 203],255,0,0);tira.setPixelColor(Pantalla[ 204],255,0,0);tira.setPixelColor(Pantalla[ 206],255,0,0);
tira.setPixelColor(Pantalla[ 222],255,0,0);tira.setPixelColor(Pantalla[ 221],255,0,0);tira.setPixelColor(Pantalla[ 220],255,0,0);tira.setPixelColor(Pantalla[ 218],255,0,0);tira.setPixelColor(Pantalla[ 216],100,100,100);tira.setPixelColor(Pantalla[ 215],100,100,100);tira.setPixelColor(Pantalla[ 213],255,0,0);tira.setPixelColor(Pantalla[ 211],255,0,0);tira.setPixelColor(Pantalla[ 210],255,0,0);tira.setPixelColor(Pantalla[ 209],255,0,0);

tira.setPixelColor(Pantalla[ 250],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 249],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 248],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 247],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 246],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 245],pixel_rojo,pixel_verde,pixel_azul);

  }
  else{
tira.setPixelColor(Pantalla[ 28],100,100,100);tira.setPixelColor(Pantalla[ 27],100,100,100);tira.setPixelColor(Pantalla[ 26],100,100,100);tira.setPixelColor(Pantalla[ 25],0,0,255);tira.setPixelColor(Pantalla[ 22],0,0,255);tira.setPixelColor(Pantalla[ 21],100,100,100);tira.setPixelColor(Pantalla[ 20],100,100,100);tira.setPixelColor(Pantalla[ 19],100,100,100);
tira.setPixelColor(Pantalla[ 35],100,100,100);tira.setPixelColor(Pantalla[ 36],0,255,0);tira.setPixelColor(Pantalla[ 37],100,100,100);tira.setPixelColor(Pantalla[ 38],0,0,255);tira.setPixelColor(Pantalla[ 39],255,0,0);tira.setPixelColor(Pantalla[ 40],255,0,0);tira.setPixelColor(Pantalla[ 41],0,0,255);tira.setPixelColor(Pantalla[ 42],100,100,100);tira.setPixelColor(Pantalla[ 43],0,255,0);tira.setPixelColor(Pantalla[ 44],100,100,100);
tira.setPixelColor(Pantalla[ 60],100,100,100);tira.setPixelColor(Pantalla[ 59],100,100,100);tira.setPixelColor(Pantalla[ 58],100,100,100);tira.setPixelColor(Pantalla[ 57],0,0,255);tira.setPixelColor(Pantalla[ 56],255,0,0);tira.setPixelColor(Pantalla[ 55],255,0,0);tira.setPixelColor(Pantalla[ 54],0,0,255);tira.setPixelColor(Pantalla[ 53],100,100,100);tira.setPixelColor(Pantalla[ 52],100,100,100);tira.setPixelColor(Pantalla[ 51],100,100,100);
tira.setPixelColor(Pantalla[ 67],0,0,255);tira.setPixelColor(Pantalla[ 68],0,0,255);tira.setPixelColor(Pantalla[ 69],0,0,255);tira.setPixelColor(Pantalla[ 70],0,0,255);tira.setPixelColor(Pantalla[ 71],255,0,0);tira.setPixelColor(Pantalla[ 72],255,0,0);tira.setPixelColor(Pantalla[ 73],0,0,255);tira.setPixelColor(Pantalla[ 74],0,0,255);tira.setPixelColor(Pantalla[ 75],0,0,255);tira.setPixelColor(Pantalla[ 76],0,0,255);
tira.setPixelColor(Pantalla[ 93],255,0,0);tira.setPixelColor(Pantalla[ 92],255,0,0);tira.setPixelColor(Pantalla[ 91],255,0,0);tira.setPixelColor(Pantalla[ 90],255,0,0);tira.setPixelColor(Pantalla[ 89],255,0,0);tira.setPixelColor(Pantalla[ 88],255,0,0);tira.setPixelColor(Pantalla[ 87],255,0,0);tira.setPixelColor(Pantalla[ 86],255,0,0);tira.setPixelColor(Pantalla[ 85],255,0,0);tira.setPixelColor(Pantalla[ 84],255,0,0);tira.setPixelColor(Pantalla[ 83],255,0,0);tira.setPixelColor(Pantalla[ 82],255,0,0);
tira.setPixelColor(Pantalla[ 97],255,0,0);tira.setPixelColor(Pantalla[ 98],255,0,0);tira.setPixelColor(Pantalla[ 99],0,0,0);tira.setPixelColor(Pantalla[ 100],255,0,0);tira.setPixelColor(Pantalla[ 101],255,0,0);tira.setPixelColor(Pantalla[ 102],255,0,0);tira.setPixelColor(Pantalla[ 103],255,0,0);tira.setPixelColor(Pantalla[ 104],255,0,0);tira.setPixelColor(Pantalla[ 105],255,0,0);tira.setPixelColor(Pantalla[ 106],255,0,0);tira.setPixelColor(Pantalla[ 107],255,0,0);tira.setPixelColor(Pantalla[ 108],0,0,0);tira.setPixelColor(Pantalla[ 109],255,0,0);tira.setPixelColor(Pantalla[ 110],255,0,0);
tira.setPixelColor(Pantalla[ 126],255,0,0);tira.setPixelColor(Pantalla[ 125],255,0,0);tira.setPixelColor(Pantalla[ 124],255,0,0);tira.setPixelColor(Pantalla[ 123],0,0,0);tira.setPixelColor(Pantalla[ 122],0,0,0);tira.setPixelColor(Pantalla[ 121],0,0,0);tira.setPixelColor(Pantalla[ 120],0,0,0);tira.setPixelColor(Pantalla[ 119],0,0,0);tira.setPixelColor(Pantalla[ 118],0,0,0);tira.setPixelColor(Pantalla[ 117],0,0,0);tira.setPixelColor(Pantalla[ 116],0,0,0);tira.setPixelColor(Pantalla[ 115],255,0,0);tira.setPixelColor(Pantalla[ 114],255,0,0);tira.setPixelColor(Pantalla[ 113],255,0,0);
tira.setPixelColor(Pantalla[ 130],255,0,0);tira.setPixelColor(Pantalla[ 131],255,0,0);tira.setPixelColor(Pantalla[ 132],255,0,0);tira.setPixelColor(Pantalla[ 133],255,0,0);tira.setPixelColor(Pantalla[ 134],255,0,0);tira.setPixelColor(Pantalla[ 135],255,0,0);tira.setPixelColor(Pantalla[ 136],255,0,0);tira.setPixelColor(Pantalla[ 137],255,0,0);tira.setPixelColor(Pantalla[ 138],255,0,0);tira.setPixelColor(Pantalla[ 139],255,0,0);tira.setPixelColor(Pantalla[ 140],255,0,0);tira.setPixelColor(Pantalla[ 141],255,0,0);
tira.setPixelColor(Pantalla[ 154],255,0,0);tira.setPixelColor(Pantalla[ 153],255,0,0);tira.setPixelColor(Pantalla[ 152],255,0,0);tira.setPixelColor(Pantalla[ 151],255,0,0);tira.setPixelColor(Pantalla[ 150],255,0,0);tira.setPixelColor(Pantalla[ 149],255,0,0);
tira.setPixelColor(Pantalla[ 164],255,0,0);tira.setPixelColor(Pantalla[ 167],255,0,0);tira.setPixelColor(Pantalla[ 168],255,0,0);tira.setPixelColor(Pantalla[ 171],255,0,0);
tira.setPixelColor(Pantalla[ 188],255,0,0);tira.setPixelColor(Pantalla[ 187],255,0,0);tira.setPixelColor(Pantalla[ 186],255,0,0);tira.setPixelColor(Pantalla[ 185],255,0,0);tira.setPixelColor(Pantalla[ 184],100,100,100);tira.setPixelColor(Pantalla[ 183],100,100,100);tira.setPixelColor(Pantalla[ 182],255,0,0);tira.setPixelColor(Pantalla[ 181],255,0,0);tira.setPixelColor(Pantalla[ 180],255,0,0);tira.setPixelColor(Pantalla[ 179],255,0,0);
tira.setPixelColor(Pantalla[ 193],255,0,0);tira.setPixelColor(Pantalla[ 195],255,0,0);tira.setPixelColor(Pantalla[ 196],255,0,0);tira.setPixelColor(Pantalla[ 197],255,0,0);tira.setPixelColor(Pantalla[ 198],255,0,0);tira.setPixelColor(Pantalla[ 199],100,100,100);tira.setPixelColor(Pantalla[ 200],100,100,100);tira.setPixelColor(Pantalla[ 201],255,0,0);tira.setPixelColor(Pantalla[ 202],255,0,0);tira.setPixelColor(Pantalla[ 203],255,0,0);tira.setPixelColor(Pantalla[ 204],255,0,0);tira.setPixelColor(Pantalla[ 206],255,0,0);
tira.setPixelColor(Pantalla[ 222],255,0,0);tira.setPixelColor(Pantalla[ 221],255,0,0);tira.setPixelColor(Pantalla[ 220],255,0,0);tira.setPixelColor(Pantalla[ 218],255,0,0);tira.setPixelColor(Pantalla[ 216],100,100,100);tira.setPixelColor(Pantalla[ 215],100,100,100);tira.setPixelColor(Pantalla[ 213],255,0,0);tira.setPixelColor(Pantalla[ 211],255,0,0);tira.setPixelColor(Pantalla[ 210],255,0,0);tira.setPixelColor(Pantalla[ 209],255,0,0);

tira.setPixelColor(Pantalla[ 250],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 249],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 248],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 247],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 246],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 245],pixel_rojo,pixel_verde,pixel_azul);

  }
}

void Matriz_Rana_res() // Ver 4.0
{

  if (lado == IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[2], 100, 100, 100);
    tira.setPixelColor(Pantalla[3], 100, 100, 100);
    tira.setPixelColor(Pantalla[4], 100, 100, 100);
    tira.setPixelColor(Pantalla[5], 100, 100, 100);
    tira.setPixelColor(Pantalla[10], 100, 100, 100);
    tira.setPixelColor(Pantalla[11], 100, 100, 100);
    tira.setPixelColor(Pantalla[12], 100, 100, 100);
    tira.setPixelColor(Pantalla[13], 100, 100, 100);
    tira.setPixelColor(Pantalla[29], 100, 100, 100);
    tira.setPixelColor(Pantalla[26], 100, 100, 100);
    tira.setPixelColor(Pantalla[25], 0, 0, 255);
    tira.setPixelColor(Pantalla[22], 0, 0, 255);
    tira.setPixelColor(Pantalla[21], 100, 100, 100);
    tira.setPixelColor(Pantalla[18], 100, 100, 100);
    tira.setPixelColor(Pantalla[34], 100, 100, 100);
    tira.setPixelColor(Pantalla[37], 100, 100, 100);
    tira.setPixelColor(Pantalla[38], 0, 0, 255);
    tira.setPixelColor(Pantalla[39], 255, 0, 0);
    tira.setPixelColor(Pantalla[40], 255, 0, 0);
    tira.setPixelColor(Pantalla[41], 0, 0, 255);
    tira.setPixelColor(Pantalla[42], 100, 100, 100);
    tira.setPixelColor(Pantalla[45], 100, 100, 100);
    tira.setPixelColor(Pantalla[61], 100, 100, 100);
    tira.setPixelColor(Pantalla[60], 100, 100, 100);
    tira.setPixelColor(Pantalla[59], 100, 100, 100);
    tira.setPixelColor(Pantalla[58], 100, 100, 100);
    tira.setPixelColor(Pantalla[57], 0, 0, 255);
    tira.setPixelColor(Pantalla[56], 255, 0, 0);
    tira.setPixelColor(Pantalla[55], 255, 0, 0);
    tira.setPixelColor(Pantalla[54], 0, 0, 255);
    tira.setPixelColor(Pantalla[53], 100, 100, 100);
    tira.setPixelColor(Pantalla[52], 100, 100, 100);
    tira.setPixelColor(Pantalla[51], 100, 100, 100);
    tira.setPixelColor(Pantalla[50], 100, 100, 100);
    tira.setPixelColor(Pantalla[67], 0, 0, 255);
    tira.setPixelColor(Pantalla[68], 0, 0, 255);
    tira.setPixelColor(Pantalla[69], 0, 0, 255);
    tira.setPixelColor(Pantalla[70], 0, 0, 255);
    tira.setPixelColor(Pantalla[71], 255, 0, 0);
    tira.setPixelColor(Pantalla[72], 255, 0, 0);
    tira.setPixelColor(Pantalla[73], 0, 0, 255);
    tira.setPixelColor(Pantalla[74], 0, 0, 255);
    tira.setPixelColor(Pantalla[75], 0, 0, 255);
    tira.setPixelColor(Pantalla[76], 0, 0, 255);
    tira.setPixelColor(Pantalla[93], 255, 0, 0);
    tira.setPixelColor(Pantalla[92], 255, 0, 0);
    tira.setPixelColor(Pantalla[91], 255, 0, 0);
    tira.setPixelColor(Pantalla[90], 255, 0, 0);
    tira.setPixelColor(Pantalla[89], 255, 0, 0);
    tira.setPixelColor(Pantalla[88], 255, 0, 0);
    tira.setPixelColor(Pantalla[87], 255, 0, 0);
    tira.setPixelColor(Pantalla[86], 255, 0, 0);
    tira.setPixelColor(Pantalla[85], 255, 0, 0);
    tira.setPixelColor(Pantalla[84], 255, 0, 0);
    tira.setPixelColor(Pantalla[83], 255, 0, 0);
    tira.setPixelColor(Pantalla[82], 255, 0, 0);
    tira.setPixelColor(Pantalla[97], 255, 0, 0);
    tira.setPixelColor(Pantalla[98], 255, 0, 0);
    tira.setPixelColor(Pantalla[99], 255, 0, 0);
    tira.setPixelColor(Pantalla[100], 255, 0, 0);
    tira.setPixelColor(Pantalla[101], 255, 0, 0);
    tira.setPixelColor(Pantalla[102], 255, 0, 0);
    tira.setPixelColor(Pantalla[103], 255, 0, 0);
    tira.setPixelColor(Pantalla[104], 255, 0, 0);
    tira.setPixelColor(Pantalla[105], 255, 0, 0);
    tira.setPixelColor(Pantalla[106], 255, 0, 0);
    tira.setPixelColor(Pantalla[107], 255, 0, 0);
    tira.setPixelColor(Pantalla[108], 255, 0, 0);
    tira.setPixelColor(Pantalla[109], 255, 0, 0);
    tira.setPixelColor(Pantalla[110], 255, 0, 0);
    tira.setPixelColor(Pantalla[126], 255, 0, 0);
    tira.setPixelColor(Pantalla[125], 255, 0, 0);
    tira.setPixelColor(Pantalla[124], 255, 0, 0);
    tira.setPixelColor(Pantalla[123], 255, 0, 0);
    tira.setPixelColor(Pantalla[122], 255, 0, 0);
    tira.setPixelColor(Pantalla[121], 255, 0, 0);
    tira.setPixelColor(Pantalla[118], 255, 0, 0);
    tira.setPixelColor(Pantalla[117], 255, 0, 0);
    tira.setPixelColor(Pantalla[116], 255, 0, 0);
    tira.setPixelColor(Pantalla[115], 255, 0, 0);
    tira.setPixelColor(Pantalla[114], 255, 0, 0);
    tira.setPixelColor(Pantalla[113], 255, 0, 0);
    tira.setPixelColor(Pantalla[130], 255, 0, 0);
    tira.setPixelColor(Pantalla[131], 255, 0, 0);
    tira.setPixelColor(Pantalla[132], 255, 0, 0);
    tira.setPixelColor(Pantalla[133], 255, 0, 0);
    tira.setPixelColor(Pantalla[134], 255, 0, 0);
    tira.setPixelColor(Pantalla[135], 255, 0, 0);
    tira.setPixelColor(Pantalla[136], 255, 0, 0);
    tira.setPixelColor(Pantalla[137], 255, 0, 0);
    tira.setPixelColor(Pantalla[138], 255, 0, 0);
    tira.setPixelColor(Pantalla[139], 255, 0, 0);
    tira.setPixelColor(Pantalla[140], 255, 0, 0);
    tira.setPixelColor(Pantalla[141], 255, 0, 0);
    tira.setPixelColor(Pantalla[154], 255, 0, 0);
    tira.setPixelColor(Pantalla[153], 255, 0, 0);
    tira.setPixelColor(Pantalla[152], 255, 0, 0);
    tira.setPixelColor(Pantalla[151], 255, 0, 0);
    tira.setPixelColor(Pantalla[150], 255, 0, 0);
    tira.setPixelColor(Pantalla[149], 255, 0, 0);
    tira.setPixelColor(Pantalla[164], 255, 0, 0);
    tira.setPixelColor(Pantalla[167], 255, 0, 0);
    tira.setPixelColor(Pantalla[168], 255, 0, 0);
    tira.setPixelColor(Pantalla[171], 255, 0, 0);
    tira.setPixelColor(Pantalla[188], 255, 0, 0);
    tira.setPixelColor(Pantalla[187], 255, 0, 0);
    tira.setPixelColor(Pantalla[186], 255, 0, 0);
    tira.setPixelColor(Pantalla[185], 255, 0, 0);
    tira.setPixelColor(Pantalla[184], 100, 100, 100);
    tira.setPixelColor(Pantalla[183], 100, 100, 100);
    tira.setPixelColor(Pantalla[182], 255, 0, 0);
    tira.setPixelColor(Pantalla[181], 255, 0, 0);
    tira.setPixelColor(Pantalla[180], 255, 0, 0);
    tira.setPixelColor(Pantalla[179], 255, 0, 0);
    tira.setPixelColor(Pantalla[193], 255, 0, 0);
    tira.setPixelColor(Pantalla[195], 255, 0, 0);
    tira.setPixelColor(Pantalla[196], 255, 0, 0);
    tira.setPixelColor(Pantalla[197], 255, 0, 0);
    tira.setPixelColor(Pantalla[198], 255, 0, 0);
    tira.setPixelColor(Pantalla[199], 100, 100, 100);
    tira.setPixelColor(Pantalla[200], 100, 100, 100);
    tira.setPixelColor(Pantalla[201], 255, 0, 0);
    tira.setPixelColor(Pantalla[202], 255, 0, 0);
    tira.setPixelColor(Pantalla[203], 255, 0, 0);
    tira.setPixelColor(Pantalla[204], 255, 0, 0);
    tira.setPixelColor(Pantalla[206], 255, 0, 0);
    tira.setPixelColor(Pantalla[222], 255, 0, 0);
    tira.setPixelColor(Pantalla[221], 255, 0, 0);
    tira.setPixelColor(Pantalla[220], 255, 0, 0);
    tira.setPixelColor(Pantalla[218], 255, 0, 0);
    tira.setPixelColor(Pantalla[216], 100, 100, 100);
    tira.setPixelColor(Pantalla[215], 100, 100, 100);
    tira.setPixelColor(Pantalla[213], 255, 0, 0);
    tira.setPixelColor(Pantalla[211], 255, 0, 0);
    tira.setPixelColor(Pantalla[210], 255, 0, 0);
    tira.setPixelColor(Pantalla[209], 255, 0, 0);

    tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
  }
  else
  {
    tira.setPixelColor(Pantalla[2], 100, 100, 100);
    tira.setPixelColor(Pantalla[3], 100, 100, 100);
    tira.setPixelColor(Pantalla[4], 100, 100, 100);
    tira.setPixelColor(Pantalla[5], 100, 100, 100);
    tira.setPixelColor(Pantalla[10], 100, 100, 100);
    tira.setPixelColor(Pantalla[11], 100, 100, 100);
    tira.setPixelColor(Pantalla[12], 100, 100, 100);
    tira.setPixelColor(Pantalla[13], 100, 100, 100);
    tira.setPixelColor(Pantalla[29], 100, 100, 100);
    tira.setPixelColor(Pantalla[28], 0, 255, 0);
    tira.setPixelColor(Pantalla[27], 0, 255, 0);
    tira.setPixelColor(Pantalla[26], 100, 100, 100);
    tira.setPixelColor(Pantalla[25], 0, 0, 255);
    tira.setPixelColor(Pantalla[22], 0, 0, 255);
    tira.setPixelColor(Pantalla[21], 100, 100, 100);
    tira.setPixelColor(Pantalla[20], 0, 255, 0);
    tira.setPixelColor(Pantalla[19], 0, 255, 0);
    tira.setPixelColor(Pantalla[18], 100, 100, 100);
    tira.setPixelColor(Pantalla[34], 100, 100, 100);
    tira.setPixelColor(Pantalla[35], 0, 255, 0);
    tira.setPixelColor(Pantalla[36], 0, 255, 0);
    tira.setPixelColor(Pantalla[37], 100, 100, 100);
    tira.setPixelColor(Pantalla[38], 0, 0, 255);
    tira.setPixelColor(Pantalla[39], 255, 0, 0);
    tira.setPixelColor(Pantalla[40], 255, 0, 0);
    tira.setPixelColor(Pantalla[41], 0, 0, 255);
    tira.setPixelColor(Pantalla[42], 100, 100, 100);
    tira.setPixelColor(Pantalla[43], 0, 255, 0);
    tira.setPixelColor(Pantalla[44], 0, 255, 0);
    tira.setPixelColor(Pantalla[45], 100, 100, 100);
    tira.setPixelColor(Pantalla[61], 100, 100, 100);
    tira.setPixelColor(Pantalla[60], 100, 100, 100);
    tira.setPixelColor(Pantalla[59], 100, 100, 100);
    tira.setPixelColor(Pantalla[58], 100, 100, 100);
    tira.setPixelColor(Pantalla[57], 0, 0, 255);
    tira.setPixelColor(Pantalla[56], 255, 0, 0);
    tira.setPixelColor(Pantalla[55], 255, 0, 0);
    tira.setPixelColor(Pantalla[54], 0, 0, 255);
    tira.setPixelColor(Pantalla[53], 100, 100, 100);
    tira.setPixelColor(Pantalla[52], 100, 100, 100);
    tira.setPixelColor(Pantalla[51], 100, 100, 100);
    tira.setPixelColor(Pantalla[50], 100, 100, 100);
    tira.setPixelColor(Pantalla[67], 0, 0, 255);
    tira.setPixelColor(Pantalla[68], 0, 0, 255);
    tira.setPixelColor(Pantalla[69], 0, 0, 255);
    tira.setPixelColor(Pantalla[70], 0, 0, 255);
    tira.setPixelColor(Pantalla[71], 255, 0, 0);
    tira.setPixelColor(Pantalla[72], 255, 0, 0);
    tira.setPixelColor(Pantalla[73], 0, 0, 255);
    tira.setPixelColor(Pantalla[74], 0, 0, 255);
    tira.setPixelColor(Pantalla[75], 0, 0, 255);
    tira.setPixelColor(Pantalla[76], 0, 0, 255);
    tira.setPixelColor(Pantalla[93], 255, 0, 0);
    tira.setPixelColor(Pantalla[92], 255, 0, 0);
    tira.setPixelColor(Pantalla[91], 255, 0, 0);
    tira.setPixelColor(Pantalla[90], 255, 0, 0);
    tira.setPixelColor(Pantalla[89], 255, 0, 0);
    tira.setPixelColor(Pantalla[88], 255, 0, 0);
    tira.setPixelColor(Pantalla[87], 255, 0, 0);
    tira.setPixelColor(Pantalla[86], 255, 0, 0);
    tira.setPixelColor(Pantalla[85], 255, 0, 0);
    tira.setPixelColor(Pantalla[84], 255, 0, 0);
    tira.setPixelColor(Pantalla[83], 255, 0, 0);
    tira.setPixelColor(Pantalla[82], 255, 0, 0);
    tira.setPixelColor(Pantalla[97], 255, 0, 0);
    tira.setPixelColor(Pantalla[98], 255, 0, 0);
    tira.setPixelColor(Pantalla[100], 255, 0, 0);
    tira.setPixelColor(Pantalla[101], 255, 0, 0);
    tira.setPixelColor(Pantalla[102], 255, 0, 0);
    tira.setPixelColor(Pantalla[103], 255, 0, 0);
    tira.setPixelColor(Pantalla[104], 255, 0, 0);
    tira.setPixelColor(Pantalla[105], 255, 0, 0);
    tira.setPixelColor(Pantalla[106], 255, 0, 0);
    tira.setPixelColor(Pantalla[107], 255, 0, 0);
    tira.setPixelColor(Pantalla[109], 255, 0, 0);
    tira.setPixelColor(Pantalla[110], 255, 0, 0);
    tira.setPixelColor(Pantalla[126], 255, 0, 0);
    tira.setPixelColor(Pantalla[125], 255, 0, 0);
    tira.setPixelColor(Pantalla[124], 255, 0, 0);
    tira.setPixelColor(Pantalla[115], 255, 0, 0);
    tira.setPixelColor(Pantalla[114], 255, 0, 0);
    tira.setPixelColor(Pantalla[113], 255, 0, 0);
    tira.setPixelColor(Pantalla[130], 255, 0, 0);
    tira.setPixelColor(Pantalla[131], 255, 0, 0);
    tira.setPixelColor(Pantalla[132], 255, 0, 0);
    tira.setPixelColor(Pantalla[133], 255, 0, 0);
    tira.setPixelColor(Pantalla[134], 255, 0, 0);
    tira.setPixelColor(Pantalla[135], 255, 0, 0);
    tira.setPixelColor(Pantalla[136], 255, 0, 0);
    tira.setPixelColor(Pantalla[137], 255, 0, 0);
    tira.setPixelColor(Pantalla[138], 255, 0, 0);
    tira.setPixelColor(Pantalla[139], 255, 0, 0);
    tira.setPixelColor(Pantalla[140], 255, 0, 0);
    tira.setPixelColor(Pantalla[141], 255, 0, 0);
    tira.setPixelColor(Pantalla[154], 255, 0, 0);
    tira.setPixelColor(Pantalla[153], 255, 0, 0);
    tira.setPixelColor(Pantalla[152], 255, 0, 0);
    tira.setPixelColor(Pantalla[151], 255, 0, 0);
    tira.setPixelColor(Pantalla[150], 255, 0, 0);
    tira.setPixelColor(Pantalla[149], 255, 0, 0);
    tira.setPixelColor(Pantalla[164], 255, 0, 0);
    tira.setPixelColor(Pantalla[167], 255, 0, 0);
    tira.setPixelColor(Pantalla[168], 255, 0, 0);
    tira.setPixelColor(Pantalla[171], 255, 0, 0);
    tira.setPixelColor(Pantalla[188], 255, 0, 0);
    tira.setPixelColor(Pantalla[187], 255, 0, 0);
    tira.setPixelColor(Pantalla[186], 255, 0, 0);
    tira.setPixelColor(Pantalla[185], 255, 0, 0);
    tira.setPixelColor(Pantalla[184], 100, 100, 100);
    tira.setPixelColor(Pantalla[183], 100, 100, 100);
    tira.setPixelColor(Pantalla[182], 255, 0, 0);
    tira.setPixelColor(Pantalla[181], 255, 0, 0);
    tira.setPixelColor(Pantalla[180], 255, 0, 0);
    tira.setPixelColor(Pantalla[179], 255, 0, 0);
    tira.setPixelColor(Pantalla[193], 255, 0, 0);
    tira.setPixelColor(Pantalla[195], 255, 0, 0);
    tira.setPixelColor(Pantalla[196], 255, 0, 0);
    tira.setPixelColor(Pantalla[197], 255, 0, 0);
    tira.setPixelColor(Pantalla[198], 255, 0, 0);
    tira.setPixelColor(Pantalla[199], 100, 100, 100);
    tira.setPixelColor(Pantalla[200], 100, 100, 100);
    tira.setPixelColor(Pantalla[201], 255, 0, 0);
    tira.setPixelColor(Pantalla[202], 255, 0, 0);
    tira.setPixelColor(Pantalla[203], 255, 0, 0);
    tira.setPixelColor(Pantalla[204], 255, 0, 0);
    tira.setPixelColor(Pantalla[206], 255, 0, 0);
    tira.setPixelColor(Pantalla[222], 255, 0, 0);
    tira.setPixelColor(Pantalla[221], 255, 0, 0);
    tira.setPixelColor(Pantalla[220], 255, 0, 0);
    tira.setPixelColor(Pantalla[218], 255, 0, 0);
    tira.setPixelColor(Pantalla[216], 100, 100, 100);
    tira.setPixelColor(Pantalla[215], 100, 100, 100);
    tira.setPixelColor(Pantalla[213], 255, 0, 0);
    tira.setPixelColor(Pantalla[211], 255, 0, 0);
    tira.setPixelColor(Pantalla[210], 255, 0, 0);
    tira.setPixelColor(Pantalla[209], 255, 0, 0);

    tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
  }
}
/*----------------------------------------------------------------------------*/

void Matriz_Unicornio() //Ver 4.0
{
if (lado==IZQUIERDA)
{
tira.setPixelColor(Pantalla[ 15],0,255,0);tira.setPixelColor(Pantalla[ 14],0,255,0);tira.setPixelColor(Pantalla[ 13],0,255,0);tira.setPixelColor(Pantalla[ 12],0,0,0);tira.setPixelColor(Pantalla[ 11],0,0,0);tira.setPixelColor(Pantalla[ 10],0,0,0);tira.setPixelColor(Pantalla[ 9],0,255,0);tira.setPixelColor(Pantalla[ 8],0,255,0);tira.setPixelColor(Pantalla[ 7],0,255,0);tira.setPixelColor(Pantalla[ 6],0,0,0);tira.setPixelColor(Pantalla[ 5],0,0,0);tira.setPixelColor(Pantalla[ 4],0,0,0);tira.setPixelColor(Pantalla[ 3],0,255,0);tira.setPixelColor(Pantalla[ 2],0,255,0);tira.setPixelColor(Pantalla[ 1],0,255,0);tira.setPixelColor(Pantalla[ 0],0,0,0);
tira.setPixelColor(Pantalla[ 16],0,0,0);tira.setPixelColor(Pantalla[ 20],255,255,0);tira.setPixelColor(Pantalla[ 31],0,0,0);
tira.setPixelColor(Pantalla[ 47],0,0,0);tira.setPixelColor(Pantalla[ 43],255,255,0);tira.setPixelColor(Pantalla[ 32],0,0,0);
tira.setPixelColor(Pantalla[ 48],0,0,0);tira.setPixelColor(Pantalla[ 51],0,0,255);tira.setPixelColor(Pantalla[ 52],255,255,0);tira.setPixelColor(Pantalla[ 53],0,0,255);tira.setPixelColor(Pantalla[ 60],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 61],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 63],0,255,0);
tira.setPixelColor(Pantalla[ 79],0,255,0);tira.setPixelColor(Pantalla[ 77],100,100,100);tira.setPixelColor(Pantalla[ 76],100,100,100);tira.setPixelColor(Pantalla[ 75],100,100,100);tira.setPixelColor(Pantalla[ 74],100,100,100);tira.setPixelColor(Pantalla[ 73],100,100,100);tira.setPixelColor(Pantalla[ 68],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 67],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 66],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 65],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 64],0,255,0);
tira.setPixelColor(Pantalla[ 80],0,255,0);tira.setPixelColor(Pantalla[ 82],100,100,100);tira.setPixelColor(Pantalla[ 84],100,100,100);tira.setPixelColor(Pantalla[ 85],100,100,100);tira.setPixelColor(Pantalla[ 86],100,100,100);tira.setPixelColor(Pantalla[ 90],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 91],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 92],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 93],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 94],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 95],0,255,0);
tira.setPixelColor(Pantalla[ 111],0,255,0);tira.setPixelColor(Pantalla[ 110],100,100,100);tira.setPixelColor(Pantalla[ 109],100,100,100);tira.setPixelColor(Pantalla[ 107],100,100,100);tira.setPixelColor(Pantalla[ 106],100,100,100);tira.setPixelColor(Pantalla[ 105],100,100,100);tira.setPixelColor(Pantalla[ 101],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 100],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 99],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 97],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 96],0,0,0);
tira.setPixelColor(Pantalla[ 112],0,0,0);tira.setPixelColor(Pantalla[ 113],100,100,100);tira.setPixelColor(Pantalla[ 114],100,100,100);tira.setPixelColor(Pantalla[ 115],100,100,100);tira.setPixelColor(Pantalla[ 116],100,100,100);tira.setPixelColor(Pantalla[ 117],100,100,100);tira.setPixelColor(Pantalla[ 118],0,0,255);tira.setPixelColor(Pantalla[ 122],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 123],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 124],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 125],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 126],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 127],0,0,0);
tira.setPixelColor(Pantalla[ 143],0,0,0);tira.setPixelColor(Pantalla[ 139],255,0,255);tira.setPixelColor(Pantalla[ 138],0,0,255);tira.setPixelColor(Pantalla[ 137],100,100,100);tira.setPixelColor(Pantalla[ 133],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 132],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 130],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 129],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 128],0,0,0);
tira.setPixelColor(Pantalla[ 144],0,0,0);tira.setPixelColor(Pantalla[ 148],0,0,255);tira.setPixelColor(Pantalla[ 149],100,100,100);tira.setPixelColor(Pantalla[ 150],100,100,100);tira.setPixelColor(Pantalla[ 151],100,100,100);tira.setPixelColor(Pantalla[ 152],100,100,100);tira.setPixelColor(Pantalla[ 153],100,100,100);tira.setPixelColor(Pantalla[ 154],105,210,30);tira.setPixelColor(Pantalla[ 158],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 159],0,255,0);
tira.setPixelColor(Pantalla[ 175],0,255,0);tira.setPixelColor(Pantalla[ 171],100,100,100);tira.setPixelColor(Pantalla[ 170],100,100,100);tira.setPixelColor(Pantalla[ 169],100,100,100);tira.setPixelColor(Pantalla[ 168],100,100,100);tira.setPixelColor(Pantalla[ 167],100,100,100);tira.setPixelColor(Pantalla[ 166],100,100,100);tira.setPixelColor(Pantalla[ 165],100,100,100);tira.setPixelColor(Pantalla[ 160],0,255,0);
tira.setPixelColor(Pantalla[ 176],0,255,0);tira.setPixelColor(Pantalla[ 180],100,100,100);tira.setPixelColor(Pantalla[ 181],100,100,100);tira.setPixelColor(Pantalla[ 182],127,255,80);tira.setPixelColor(Pantalla[ 183],127,255,80);tira.setPixelColor(Pantalla[ 184],127,255,80);tira.setPixelColor(Pantalla[ 185],100,100,100);tira.setPixelColor(Pantalla[ 186],100,100,100);tira.setPixelColor(Pantalla[ 191],0,255,0);
tira.setPixelColor(Pantalla[ 207],0,255,0);tira.setPixelColor(Pantalla[ 203],100,100,100);tira.setPixelColor(Pantalla[ 202],100,100,100);tira.setPixelColor(Pantalla[ 199],127,255,80);tira.setPixelColor(Pantalla[ 198],100,100,100);tira.setPixelColor(Pantalla[ 197],100,100,100);tira.setPixelColor(Pantalla[ 192],0,0,0);
tira.setPixelColor(Pantalla[ 208],0,0,0);tira.setPixelColor(Pantalla[ 212],100,100,100);tira.setPixelColor(Pantalla[ 213],100,100,100);tira.setPixelColor(Pantalla[ 217],100,100,100);tira.setPixelColor(Pantalla[ 218],100,100,100);tira.setPixelColor(Pantalla[ 223],0,0,0);
tira.setPixelColor(Pantalla[ 239],0,0,0);tira.setPixelColor(Pantalla[ 235],255,0,0);tira.setPixelColor(Pantalla[ 234],255,0,0);tira.setPixelColor(Pantalla[ 230],255,0,0);tira.setPixelColor(Pantalla[ 229],255,0,0);tira.setPixelColor(Pantalla[ 224],0,0,0);
tira.setPixelColor(Pantalla[ 240],0,0,0);tira.setPixelColor(Pantalla[ 241],0,255,0);tira.setPixelColor(Pantalla[ 242],0,255,0);tira.setPixelColor(Pantalla[ 243],0,255,0);tira.setPixelColor(Pantalla[ 244],0,0,0);tira.setPixelColor(Pantalla[ 245],0,0,0);tira.setPixelColor(Pantalla[ 246],0,0,0);tira.setPixelColor(Pantalla[ 247],0,255,0);tira.setPixelColor(Pantalla[ 248],0,255,0);tira.setPixelColor(Pantalla[ 249],0,255,0);tira.setPixelColor(Pantalla[ 250],0,0,0);tira.setPixelColor(Pantalla[ 251],0,0,0);tira.setPixelColor(Pantalla[ 252],0,0,0);tira.setPixelColor(Pantalla[ 253],0,255,0);tira.setPixelColor(Pantalla[ 254],0,255,0);tira.setPixelColor(Pantalla[ 255],0,255,0);

}
else
{
tira.setPixelColor(Pantalla[ 15],0,0,0);tira.setPixelColor(Pantalla[ 14],0,0,0);tira.setPixelColor(Pantalla[ 13],0,0,0);tira.setPixelColor(Pantalla[ 12],0,255,0);tira.setPixelColor(Pantalla[ 11],0,255,0);tira.setPixelColor(Pantalla[ 10],0,255,0);tira.setPixelColor(Pantalla[ 9],0,0,0);tira.setPixelColor(Pantalla[ 8],0,0,0);tira.setPixelColor(Pantalla[ 7],0,0,0);tira.setPixelColor(Pantalla[ 6],0,255,0);tira.setPixelColor(Pantalla[ 5],0,255,0);tira.setPixelColor(Pantalla[ 4],0,255,0);tira.setPixelColor(Pantalla[ 3],0,0,0);tira.setPixelColor(Pantalla[ 2],0,0,0);tira.setPixelColor(Pantalla[ 1],0,0,0);tira.setPixelColor(Pantalla[ 0],0,255,0);
tira.setPixelColor(Pantalla[ 16],0,255,0);tira.setPixelColor(Pantalla[ 20],255,255,0);tira.setPixelColor(Pantalla[ 31],0,255,0);
tira.setPixelColor(Pantalla[ 47],0,255,0);tira.setPixelColor(Pantalla[ 43],255,255,0);tira.setPixelColor(Pantalla[ 32],0,255,0);
tira.setPixelColor(Pantalla[ 48],0,255,0);tira.setPixelColor(Pantalla[ 51],0,0,255);tira.setPixelColor(Pantalla[ 52],255,255,0);tira.setPixelColor(Pantalla[ 53],0,0,255);tira.setPixelColor(Pantalla[ 60],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 61],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 63],0,0,0);
tira.setPixelColor(Pantalla[ 79],0,0,0);tira.setPixelColor(Pantalla[ 77],100,100,100);tira.setPixelColor(Pantalla[ 76],100,100,100);tira.setPixelColor(Pantalla[ 75],100,100,100);tira.setPixelColor(Pantalla[ 74],100,100,100);tira.setPixelColor(Pantalla[ 73],100,100,100);tira.setPixelColor(Pantalla[ 68],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 67],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 66],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 65],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 64],0,0,0);
tira.setPixelColor(Pantalla[ 80],0,0,0);tira.setPixelColor(Pantalla[ 82],100,100,100);tira.setPixelColor(Pantalla[ 84],100,100,100);tira.setPixelColor(Pantalla[ 85],100,100,100);tira.setPixelColor(Pantalla[ 86],100,100,100);tira.setPixelColor(Pantalla[ 90],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 91],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 92],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 93],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 94],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 95],0,0,0);
tira.setPixelColor(Pantalla[ 111],0,0,0);tira.setPixelColor(Pantalla[ 110],100,100,100);tira.setPixelColor(Pantalla[ 109],100,100,100);tira.setPixelColor(Pantalla[ 107],100,100,100);tira.setPixelColor(Pantalla[ 106],100,100,100);tira.setPixelColor(Pantalla[ 105],100,100,100);tira.setPixelColor(Pantalla[ 101],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 100],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 99],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 97],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 96],0,255,0);
tira.setPixelColor(Pantalla[ 112],0,255,0);tira.setPixelColor(Pantalla[ 113],100,100,100);tira.setPixelColor(Pantalla[ 114],100,100,100);tira.setPixelColor(Pantalla[ 115],100,100,100);tira.setPixelColor(Pantalla[ 116],100,100,100);tira.setPixelColor(Pantalla[ 117],100,100,100);tira.setPixelColor(Pantalla[ 118],0,0,255);tira.setPixelColor(Pantalla[ 122],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 123],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 124],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 125],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 126],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 127],0,255,0);
tira.setPixelColor(Pantalla[ 143],0,255,0);tira.setPixelColor(Pantalla[ 139],255,0,255);tira.setPixelColor(Pantalla[ 138],0,0,255);tira.setPixelColor(Pantalla[ 137],100,100,100);tira.setPixelColor(Pantalla[ 133],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 132],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 130],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 129],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 128],0,255,0);
tira.setPixelColor(Pantalla[ 144],0,255,0);tira.setPixelColor(Pantalla[ 148],0,0,255);tira.setPixelColor(Pantalla[ 149],100,100,100);tira.setPixelColor(Pantalla[ 150],100,100,100);tira.setPixelColor(Pantalla[ 151],100,100,100);tira.setPixelColor(Pantalla[ 152],100,100,100);tira.setPixelColor(Pantalla[ 153],100,100,100);tira.setPixelColor(Pantalla[ 154],105,210,30);tira.setPixelColor(Pantalla[ 158],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 159],0,0,0);
tira.setPixelColor(Pantalla[ 175],0,0,0);tira.setPixelColor(Pantalla[ 171],100,100,100);tira.setPixelColor(Pantalla[ 170],100,100,100);tira.setPixelColor(Pantalla[ 169],100,100,100);tira.setPixelColor(Pantalla[ 168],100,100,100);tira.setPixelColor(Pantalla[ 167],100,100,100);tira.setPixelColor(Pantalla[ 166],100,100,100);tira.setPixelColor(Pantalla[ 165],100,100,100);tira.setPixelColor(Pantalla[ 160],0,0,0);
tira.setPixelColor(Pantalla[ 176],0,0,0);tira.setPixelColor(Pantalla[ 180],100,100,100);tira.setPixelColor(Pantalla[ 181],100,100,100);tira.setPixelColor(Pantalla[ 182],127,255,80);tira.setPixelColor(Pantalla[ 183],127,255,80);tira.setPixelColor(Pantalla[ 184],127,255,80);tira.setPixelColor(Pantalla[ 185],100,100,100);tira.setPixelColor(Pantalla[ 186],100,100,100);tira.setPixelColor(Pantalla[ 191],0,0,0);
tira.setPixelColor(Pantalla[ 207],0,0,0);tira.setPixelColor(Pantalla[ 203],100,100,100);tira.setPixelColor(Pantalla[ 202],100,100,100);tira.setPixelColor(Pantalla[ 199],127,255,80);tira.setPixelColor(Pantalla[ 198],100,100,100);tira.setPixelColor(Pantalla[ 197],100,100,100);tira.setPixelColor(Pantalla[ 192],0,255,0);
tira.setPixelColor(Pantalla[ 208],0,255,0);tira.setPixelColor(Pantalla[ 212],100,100,100);tira.setPixelColor(Pantalla[ 213],100,100,100);tira.setPixelColor(Pantalla[ 217],100,100,100);tira.setPixelColor(Pantalla[ 218],100,100,100);tira.setPixelColor(Pantalla[ 223],0,255,0);
tira.setPixelColor(Pantalla[ 239],0,255,0);tira.setPixelColor(Pantalla[ 235],255,0,0);tira.setPixelColor(Pantalla[ 234],255,0,0);tira.setPixelColor(Pantalla[ 230],255,0,0);tira.setPixelColor(Pantalla[ 229],255,0,0);tira.setPixelColor(Pantalla[ 224],0,255,0);
tira.setPixelColor(Pantalla[ 240],0,255,0);tira.setPixelColor(Pantalla[ 241],0,0,0);tira.setPixelColor(Pantalla[ 242],0,0,0);tira.setPixelColor(Pantalla[ 243],0,0,0);tira.setPixelColor(Pantalla[ 244],0,255,0);tira.setPixelColor(Pantalla[ 245],0,255,0);tira.setPixelColor(Pantalla[ 246],0,255,0);tira.setPixelColor(Pantalla[ 247],0,0,0);tira.setPixelColor(Pantalla[ 248],0,0,0);tira.setPixelColor(Pantalla[ 249],0,0,0);tira.setPixelColor(Pantalla[ 250],0,255,0);tira.setPixelColor(Pantalla[ 251],0,255,0);tira.setPixelColor(Pantalla[ 252],0,255,0);tira.setPixelColor(Pantalla[ 253],0,0,0);tira.setPixelColor(Pantalla[ 254],0,0,0);tira.setPixelColor(Pantalla[ 255],0,0,0);

}
}
/*----------------------------------------------------------------------------*/

void Matriz_Botella() // Ver 4.0
{
  if (lado == IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[0], 0, 255, 0);
    tira.setPixelColor(Pantalla[1], 0, 255, 0);
    tira.setPixelColor(Pantalla[2], 0, 255, 0);
    tira.setPixelColor(Pantalla[3], 0, 0, 0);
    tira.setPixelColor(Pantalla[4], 0, 0, 0);
    tira.setPixelColor(Pantalla[5], 0, 0, 0);
    tira.setPixelColor(Pantalla[6], 0, 255, 0);
    tira.setPixelColor(Pantalla[7], 0, 255, 0);
    tira.setPixelColor(Pantalla[8], 0, 255, 0);
    tira.setPixelColor(Pantalla[9], 0, 0, 0);
    tira.setPixelColor(Pantalla[10], 0, 0, 0);
    tira.setPixelColor(Pantalla[11], 0, 0, 0);
    tira.setPixelColor(Pantalla[12], 0, 255, 0);
    tira.setPixelColor(Pantalla[13], 0, 255, 0);
    tira.setPixelColor(Pantalla[14], 0, 255, 0);
    tira.setPixelColor(Pantalla[15], 0, 0, 0);
    tira.setPixelColor(Pantalla[31], 0, 0, 0);
    tira.setPixelColor(Pantalla[30], 0, 0, 0);
    tira.setPixelColor(Pantalla[25], 255, 0, 0);
    tira.setPixelColor(Pantalla[24], 255, 0, 0);
    tira.setPixelColor(Pantalla[23], 255, 0, 0);
    tira.setPixelColor(Pantalla[22], 255, 0, 0);
    tira.setPixelColor(Pantalla[20], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[19], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[16], 0, 0, 0);
    tira.setPixelColor(Pantalla[32], 0, 0, 0);
    tira.setPixelColor(Pantalla[34], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[35], 0, 0, 0);
    tira.setPixelColor(Pantalla[38], 255, 0, 0);
    tira.setPixelColor(Pantalla[39], 255, 0, 0);
    tira.setPixelColor(Pantalla[40], 255, 0, 0);
    tira.setPixelColor(Pantalla[41], 255, 0, 0);
    tira.setPixelColor(Pantalla[47], 0, 0, 0);
    tira.setPixelColor(Pantalla[63], 0, 0, 0);
    tira.setPixelColor(Pantalla[56], 105, 210, 30);
    tira.setPixelColor(Pantalla[55], 105, 210, 30);
    tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[49], 0, 0, 0);
    tira.setPixelColor(Pantalla[48], 0, 255, 0);
    tira.setPixelColor(Pantalla[64], 0, 255, 0);
    tira.setPixelColor(Pantalla[71], 0, 0, 255);
    tira.setPixelColor(Pantalla[72], 0, 0, 255);
    tira.setPixelColor(Pantalla[75], 0, 0, 0);
    tira.setPixelColor(Pantalla[79], 0, 255, 0);
    tira.setPixelColor(Pantalla[95], 0, 255, 0);
    tira.setPixelColor(Pantalla[93], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[89], 0, 0, 255);
    tira.setPixelColor(Pantalla[88], 100, 100, 100);
    tira.setPixelColor(Pantalla[87], 100, 100, 100);
    tira.setPixelColor(Pantalla[86], 0, 0, 255);
    tira.setPixelColor(Pantalla[82], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[80], 0, 255, 0);
    tira.setPixelColor(Pantalla[96], 0, 255, 0);
    tira.setPixelColor(Pantalla[98], 0, 0, 0);
    tira.setPixelColor(Pantalla[99], 0, 0, 0);
    tira.setPixelColor(Pantalla[101], 0, 0, 255);
    tira.setPixelColor(Pantalla[102], 100, 100, 100);
    tira.setPixelColor(Pantalla[103], 100, 100, 100);
    tira.setPixelColor(Pantalla[104], 100, 100, 100);
    tira.setPixelColor(Pantalla[105], 100, 100, 100);
    tira.setPixelColor(Pantalla[106], 0, 0, 255);
    tira.setPixelColor(Pantalla[109], 0, 0, 0);
    tira.setPixelColor(Pantalla[111], 0, 0, 0);
    tira.setPixelColor(Pantalla[127], 0, 0, 0);
    tira.setPixelColor(Pantalla[124], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[122], 0, 0, 255);
    tira.setPixelColor(Pantalla[121], 0, 255, 0);
    tira.setPixelColor(Pantalla[120], 0, 255, 0);
    tira.setPixelColor(Pantalla[119], 0, 255, 0);
    tira.setPixelColor(Pantalla[118], 0, 255, 0);
    tira.setPixelColor(Pantalla[117], 0, 0, 255);
    tira.setPixelColor(Pantalla[112], 0, 0, 0);
    tira.setPixelColor(Pantalla[128], 0, 0, 0);
    tira.setPixelColor(Pantalla[129], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[133], 0, 0, 255);
    tira.setPixelColor(Pantalla[134], 0, 255, 0);
    tira.setPixelColor(Pantalla[135], 0, 255, 0);
    tira.setPixelColor(Pantalla[136], 0, 255, 0);
    tira.setPixelColor(Pantalla[137], 0, 255, 0);
    tira.setPixelColor(Pantalla[138], 0, 0, 255);
    tira.setPixelColor(Pantalla[141], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[142], 0, 0, 0);
    tira.setPixelColor(Pantalla[143], 0, 0, 0);
    tira.setPixelColor(Pantalla[159], 0, 0, 0);
    tira.setPixelColor(Pantalla[158], 0, 0, 0);
    tira.setPixelColor(Pantalla[154], 0, 0, 255);
    tira.setPixelColor(Pantalla[153], 0, 255, 0);
    tira.setPixelColor(Pantalla[152], 0, 255, 0);
    tira.setPixelColor(Pantalla[151], 0, 255, 0);
    tira.setPixelColor(Pantalla[150], 0, 255, 0);
    tira.setPixelColor(Pantalla[149], 0, 0, 255);
    tira.setPixelColor(Pantalla[148], 0, 0, 0);
    tira.setPixelColor(Pantalla[147], 0, 0, 0);
    tira.setPixelColor(Pantalla[144], 0, 255, 0);
    tira.setPixelColor(Pantalla[160], 0, 255, 0);
    tira.setPixelColor(Pantalla[162], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[165], 0, 0, 255);
    tira.setPixelColor(Pantalla[166], 0, 255, 0);
    tira.setPixelColor(Pantalla[167], 0, 255, 0);
    tira.setPixelColor(Pantalla[168], 0, 255, 0);
    tira.setPixelColor(Pantalla[169], 0, 255, 0);
    tira.setPixelColor(Pantalla[170], 0, 0, 255);
    tira.setPixelColor(Pantalla[175], 0, 255, 0);
    tira.setPixelColor(Pantalla[191], 0, 255, 0);
    tira.setPixelColor(Pantalla[190], 0, 0, 0);
    tira.setPixelColor(Pantalla[186], 0, 0, 255);
    tira.setPixelColor(Pantalla[185], 0, 255, 0);
    tira.setPixelColor(Pantalla[184], 0, 255, 0);
    tira.setPixelColor(Pantalla[183], 0, 255, 0);
    tira.setPixelColor(Pantalla[182], 0, 255, 0);
    tira.setPixelColor(Pantalla[181], 0, 0, 255);
    tira.setPixelColor(Pantalla[180], 0, 0, 0);
    tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[177], 0, 0, 0);
    tira.setPixelColor(Pantalla[176], 0, 255, 0);
    tira.setPixelColor(Pantalla[192], 0, 255, 0);
    tira.setPixelColor(Pantalla[197], 0, 0, 255);
    tira.setPixelColor(Pantalla[198], 0, 0, 255);
    tira.setPixelColor(Pantalla[199], 0, 0, 255);
    tira.setPixelColor(Pantalla[200], 0, 0, 255);
    tira.setPixelColor(Pantalla[201], 0, 0, 255);
    tira.setPixelColor(Pantalla[202], 0, 0, 255);
    tira.setPixelColor(Pantalla[203], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[207], 0, 0, 0);
    tira.setPixelColor(Pantalla[223], 0, 0, 0);
    tira.setPixelColor(Pantalla[222], 0, 255, 0);
    tira.setPixelColor(Pantalla[221], 0, 255, 0);
    tira.setPixelColor(Pantalla[220], 0, 255, 0);
    tira.setPixelColor(Pantalla[219], 0, 0, 0);
    tira.setPixelColor(Pantalla[218], 0, 0, 0);
    tira.setPixelColor(Pantalla[217], 0, 0, 0);
    tira.setPixelColor(Pantalla[216], 0, 255, 0);
    tira.setPixelColor(Pantalla[215], 0, 255, 0);
    tira.setPixelColor(Pantalla[214], 0, 255, 0);
    tira.setPixelColor(Pantalla[213], 0, 0, 0);
    tira.setPixelColor(Pantalla[212], 0, 0, 0);
    tira.setPixelColor(Pantalla[211], 0, 0, 0);
    tira.setPixelColor(Pantalla[210], 0, 255, 0);
    tira.setPixelColor(Pantalla[209], 0, 255, 0);
    tira.setPixelColor(Pantalla[208], 0, 255, 0);
  }
  else
  {
    tira.setPixelColor(Pantalla[0], 0, 0, 0);
    tira.setPixelColor(Pantalla[1], 0, 0, 0);
    tira.setPixelColor(Pantalla[2], 0, 0, 0);
    tira.setPixelColor(Pantalla[3], 0, 255, 0);
    tira.setPixelColor(Pantalla[4], 0, 255, 0);
    tira.setPixelColor(Pantalla[5], 0, 255, 0);
    tira.setPixelColor(Pantalla[6], 0, 0, 0);
    tira.setPixelColor(Pantalla[7], 0, 0, 0);
    tira.setPixelColor(Pantalla[8], 0, 0, 0);
    tira.setPixelColor(Pantalla[9], 0, 255, 0);
    tira.setPixelColor(Pantalla[10], 0, 255, 0);
    tira.setPixelColor(Pantalla[11], 0, 255, 0);
    tira.setPixelColor(Pantalla[12], 0, 0, 0);
    tira.setPixelColor(Pantalla[13], 0, 0, 0);
    tira.setPixelColor(Pantalla[14], 0, 0, 0);
    tira.setPixelColor(Pantalla[15], 0, 255, 0);
    tira.setPixelColor(Pantalla[31], 0, 255, 0);
    tira.setPixelColor(Pantalla[30], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[25], 255, 0, 0);
    tira.setPixelColor(Pantalla[24], 255, 0, 0);
    tira.setPixelColor(Pantalla[23], 255, 0, 0);
    tira.setPixelColor(Pantalla[22], 255, 0, 0);
    tira.setPixelColor(Pantalla[20], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[19], 0, 0, 0);
    tira.setPixelColor(Pantalla[16], 0, 255, 0);
    tira.setPixelColor(Pantalla[32], 0, 255, 0);
    tira.setPixelColor(Pantalla[34], 0, 0, 0);
    tira.setPixelColor(Pantalla[35], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[38], 255, 0, 0);
    tira.setPixelColor(Pantalla[39], 255, 0, 0);
    tira.setPixelColor(Pantalla[40], 255, 0, 0);
    tira.setPixelColor(Pantalla[41], 255, 0, 0);
    tira.setPixelColor(Pantalla[47], 0, 255, 0);
    tira.setPixelColor(Pantalla[63], 0, 255, 0);
    tira.setPixelColor(Pantalla[56], 105, 210, 30);
    tira.setPixelColor(Pantalla[55], 105, 210, 30);
    tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[50], 0, 0, 0);
    tira.setPixelColor(Pantalla[49], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[48], 0, 0, 0);
    tira.setPixelColor(Pantalla[64], 0, 0, 0);
    tira.setPixelColor(Pantalla[71], 0, 0, 255);
    tira.setPixelColor(Pantalla[72], 0, 0, 255);
    tira.setPixelColor(Pantalla[75], 0, 0, 0);
    tira.setPixelColor(Pantalla[79], 0, 0, 0);
    tira.setPixelColor(Pantalla[95], 0, 0, 0);
    tira.setPixelColor(Pantalla[93], 0, 0, 0);
    tira.setPixelColor(Pantalla[89], 0, 0, 255);
    tira.setPixelColor(Pantalla[88], 100, 100, 100);
    tira.setPixelColor(Pantalla[87], 100, 100, 100);
    tira.setPixelColor(Pantalla[86], 0, 0, 255);
    tira.setPixelColor(Pantalla[82], 0, 0, 0);
    tira.setPixelColor(Pantalla[80], 0, 0, 0);
    tira.setPixelColor(Pantalla[96], 0, 0, 0);
    tira.setPixelColor(Pantalla[98], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[99], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[101], 0, 0, 255);
    tira.setPixelColor(Pantalla[102], 100, 100, 100);
    tira.setPixelColor(Pantalla[103], 100, 100, 100);
    tira.setPixelColor(Pantalla[104], 100, 100, 100);
    tira.setPixelColor(Pantalla[105], 100, 100, 100);
    tira.setPixelColor(Pantalla[106], 0, 0, 255);
    tira.setPixelColor(Pantalla[109], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[111], 0, 255, 0);
    tira.setPixelColor(Pantalla[127], 0, 255, 0);
    tira.setPixelColor(Pantalla[124], 0, 0, 0);
    tira.setPixelColor(Pantalla[122], 0, 0, 255);
    tira.setPixelColor(Pantalla[121], 0, 255, 0);
    tira.setPixelColor(Pantalla[120], 0, 255, 0);
    tira.setPixelColor(Pantalla[119], 0, 255, 0);
    tira.setPixelColor(Pantalla[118], 0, 255, 0);
    tira.setPixelColor(Pantalla[117], 0, 0, 255);
    tira.setPixelColor(Pantalla[112], 0, 255, 0);
    tira.setPixelColor(Pantalla[128], 0, 255, 0);
    tira.setPixelColor(Pantalla[129], 0, 0, 0);
    tira.setPixelColor(Pantalla[133], 0, 0, 255);
    tira.setPixelColor(Pantalla[134], 0, 255, 0);
    tira.setPixelColor(Pantalla[135], 0, 255, 0);
    tira.setPixelColor(Pantalla[136], 0, 255, 0);
    tira.setPixelColor(Pantalla[137], 0, 255, 0);
    tira.setPixelColor(Pantalla[138], 0, 0, 255);
    tira.setPixelColor(Pantalla[141], 0, 0, 0);
    tira.setPixelColor(Pantalla[142], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[143], 0, 255, 0);
    tira.setPixelColor(Pantalla[159], 0, 255, 0);
    tira.setPixelColor(Pantalla[158], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[154], 0, 0, 255);
    tira.setPixelColor(Pantalla[153], 0, 255, 0);
    tira.setPixelColor(Pantalla[152], 0, 255, 0);
    tira.setPixelColor(Pantalla[151], 0, 255, 0);
    tira.setPixelColor(Pantalla[150], 0, 255, 0);
    tira.setPixelColor(Pantalla[149], 0, 0, 255);
    tira.setPixelColor(Pantalla[148], 0, 0, 0);
    tira.setPixelColor(Pantalla[147], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[144], 0, 0, 0);
    tira.setPixelColor(Pantalla[160], 0, 0, 0);
    tira.setPixelColor(Pantalla[162], 0, 0, 0);
    tira.setPixelColor(Pantalla[165], 0, 0, 255);
    tira.setPixelColor(Pantalla[166], 0, 255, 0);
    tira.setPixelColor(Pantalla[167], 0, 255, 0);
    tira.setPixelColor(Pantalla[168], 0, 255, 0);
    tira.setPixelColor(Pantalla[169], 0, 255, 0);
    tira.setPixelColor(Pantalla[170], 0, 0, 255);
    tira.setPixelColor(Pantalla[175], 0, 0, 0);
    tira.setPixelColor(Pantalla[191], 0, 0, 0);
    tira.setPixelColor(Pantalla[190], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[186], 0, 0, 255);
    tira.setPixelColor(Pantalla[185], 0, 255, 0);
    tira.setPixelColor(Pantalla[184], 0, 255, 0);
    tira.setPixelColor(Pantalla[183], 0, 255, 0);
    tira.setPixelColor(Pantalla[182], 0, 255, 0);
    tira.setPixelColor(Pantalla[181], 0, 0, 255);
    tira.setPixelColor(Pantalla[180], 0, 0, 0);
    tira.setPixelColor(Pantalla[178], 0, 0, 0);
    tira.setPixelColor(Pantalla[177], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[176], 0, 0, 0);
    tira.setPixelColor(Pantalla[192], 0, 0, 0);
    tira.setPixelColor(Pantalla[197], 0, 0, 255);
    tira.setPixelColor(Pantalla[198], 0, 255, 0);
    tira.setPixelColor(Pantalla[199], 0, 255, 0);
    tira.setPixelColor(Pantalla[200], 0, 255, 0);
    tira.setPixelColor(Pantalla[201], 0, 255, 0);
    tira.setPixelColor(Pantalla[202], 0, 0, 255);
    tira.setPixelColor(Pantalla[203], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[207], 0, 255, 0);
    tira.setPixelColor(Pantalla[223], 0, 255, 0);
    tira.setPixelColor(Pantalla[222], 0, 0, 0);
    tira.setPixelColor(Pantalla[221], 0, 0, 0);
    tira.setPixelColor(Pantalla[220], 0, 0, 0);
    tira.setPixelColor(Pantalla[219], 0, 255, 0);
    tira.setPixelColor(Pantalla[218], 0, 255, 0);
    tira.setPixelColor(Pantalla[217], 0, 255, 0);
    tira.setPixelColor(Pantalla[216], 0, 0, 0);
    tira.setPixelColor(Pantalla[215], 0, 0, 0);
    tira.setPixelColor(Pantalla[214], 0, 0, 0);
    tira.setPixelColor(Pantalla[213], 0, 255, 0);
    tira.setPixelColor(Pantalla[212], 0, 255, 0);
    tira.setPixelColor(Pantalla[211], 0, 255, 0);
    tira.setPixelColor(Pantalla[210], 0, 0, 0);
    tira.setPixelColor(Pantalla[209], 0, 0, 0);
    tira.setPixelColor(Pantalla[208], 0, 0, 0);
  }
}
/*----------------------------------------------------------------------------*/

void Matriz_Copa() // Ver 4.0
{

  if (lado == IZQUIERDA)
  {

    tira.setPixelColor(Pantalla[0], 0, 0, 0);
    tira.setPixelColor(Pantalla[1], 0, 0, 0);
    tira.setPixelColor(Pantalla[2], 0, 0, 0);
    tira.setPixelColor(Pantalla[3], 0, 255, 0);
    tira.setPixelColor(Pantalla[4], 0, 255, 0);
    tira.setPixelColor(Pantalla[5], 0, 255, 0);
    tira.setPixelColor(Pantalla[6], 0, 0, 0);
    tira.setPixelColor(Pantalla[7], 0, 0, 0);
    tira.setPixelColor(Pantalla[8], 0, 0, 0);
    tira.setPixelColor(Pantalla[9], 0, 255, 0);
    tira.setPixelColor(Pantalla[10], 0, 255, 0);
    tira.setPixelColor(Pantalla[11], 0, 255, 0);
    tira.setPixelColor(Pantalla[12], 0, 0, 0);
    tira.setPixelColor(Pantalla[13], 0, 0, 0);
    tira.setPixelColor(Pantalla[14], 0, 0, 0);
    tira.setPixelColor(Pantalla[15], 0, 255, 0);
    tira.setPixelColor(Pantalla[31], 0, 255, 0);
    tira.setPixelColor(Pantalla[30], 0, 0, 0);
    tira.setPixelColor(Pantalla[20], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[19], 0, 0, 0);
    tira.setPixelColor(Pantalla[16], 0, 255, 0);
    tira.setPixelColor(Pantalla[32], 0, 255, 0);
    tira.setPixelColor(Pantalla[34], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[35], 0, 0, 0);
    tira.setPixelColor(Pantalla[37], 0, 0, 255);
    tira.setPixelColor(Pantalla[38], 100, 100, 100);
    tira.setPixelColor(Pantalla[39], 100, 100, 100);
    tira.setPixelColor(Pantalla[40], 100, 100, 100);
    tira.setPixelColor(Pantalla[41], 100, 100, 100);
    tira.setPixelColor(Pantalla[42], 0, 0, 255);
    tira.setPixelColor(Pantalla[47], 0, 255, 0);
    tira.setPixelColor(Pantalla[63], 0, 255, 0);
    tira.setPixelColor(Pantalla[58], 0, 0, 255);
    tira.setPixelColor(Pantalla[57], 100, 100, 100);
    tira.setPixelColor(Pantalla[56], 100, 100, 100);
    tira.setPixelColor(Pantalla[55], 100, 100, 100);
    tira.setPixelColor(Pantalla[54], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], 0, 0, 255);
    tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[49], 0, 0, 0);
    tira.setPixelColor(Pantalla[48], 0, 0, 0);
    tira.setPixelColor(Pantalla[64], 0, 0, 0);
    tira.setPixelColor(Pantalla[69], 0, 0, 255);
    tira.setPixelColor(Pantalla[70], 100, 100, 100);
    tira.setPixelColor(Pantalla[71], 100, 100, 100);
    tira.setPixelColor(Pantalla[72], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], 100, 100, 100);
    tira.setPixelColor(Pantalla[74], 0, 0, 255);
    tira.setPixelColor(Pantalla[75], 0, 0, 0);
    tira.setPixelColor(Pantalla[79], 0, 0, 0);
    tira.setPixelColor(Pantalla[95], 0, 0, 0);
    tira.setPixelColor(Pantalla[93], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[90], 0, 0, 255);
    tira.setPixelColor(Pantalla[89], 0, 255, 0);
    tira.setPixelColor(Pantalla[88], 0, 255, 0);
    tira.setPixelColor(Pantalla[87], 0, 255, 0);
    tira.setPixelColor(Pantalla[86], 0, 255, 0);
    tira.setPixelColor(Pantalla[85], 0, 0, 255);
    tira.setPixelColor(Pantalla[82], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[80], 0, 0, 0);
    tira.setPixelColor(Pantalla[96], 0, 0, 0);
    tira.setPixelColor(Pantalla[98], 0, 0, 0);
    tira.setPixelColor(Pantalla[99], 0, 0, 0);
    tira.setPixelColor(Pantalla[101], 0, 0, 255);
    tira.setPixelColor(Pantalla[102], 0, 255, 0);
    tira.setPixelColor(Pantalla[103], 0, 255, 0);
    tira.setPixelColor(Pantalla[104], 0, 255, 0);
    tira.setPixelColor(Pantalla[105], 0, 255, 0);
    tira.setPixelColor(Pantalla[106], 0, 0, 255);
    tira.setPixelColor(Pantalla[109], 0, 0, 0);
    tira.setPixelColor(Pantalla[111], 0, 255, 0);
    tira.setPixelColor(Pantalla[127], 0, 255, 0);
    tira.setPixelColor(Pantalla[124], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[122], 0, 0, 255);
    tira.setPixelColor(Pantalla[121], 0, 255, 0);
    tira.setPixelColor(Pantalla[120], 0, 255, 0);
    tira.setPixelColor(Pantalla[119], 0, 255, 0);
    tira.setPixelColor(Pantalla[118], 0, 255, 0);
    tira.setPixelColor(Pantalla[117], 0, 0, 255);
    tira.setPixelColor(Pantalla[112], 0, 255, 0);
    tira.setPixelColor(Pantalla[128], 0, 255, 0);
    tira.setPixelColor(Pantalla[129], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[133], 0, 0, 255);
    tira.setPixelColor(Pantalla[134], 0, 255, 0);
    tira.setPixelColor(Pantalla[135], 0, 255, 0);
    tira.setPixelColor(Pantalla[136], 0, 255, 0);
    tira.setPixelColor(Pantalla[137], 0, 255, 0);
    tira.setPixelColor(Pantalla[138], 0, 0, 255);
    tira.setPixelColor(Pantalla[141], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[142], 0, 0, 0);
    tira.setPixelColor(Pantalla[143], 0, 255, 0);
    tira.setPixelColor(Pantalla[159], 0, 255, 0);
    tira.setPixelColor(Pantalla[158], 0, 0, 0);
    tira.setPixelColor(Pantalla[153], 0, 0, 255);
    tira.setPixelColor(Pantalla[152], 0, 0, 255);
    tira.setPixelColor(Pantalla[151], 0, 0, 255);
    tira.setPixelColor(Pantalla[150], 0, 0, 255);
    tira.setPixelColor(Pantalla[148], 0, 0, 0);
    tira.setPixelColor(Pantalla[147], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[144], 0, 0, 0);
    tira.setPixelColor(Pantalla[160], 0, 0, 0);
    tira.setPixelColor(Pantalla[162], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[167], 0, 0, 255);
    tira.setPixelColor(Pantalla[168], 0, 0, 255);
    tira.setPixelColor(Pantalla[175], 0, 0, 0);
    tira.setPixelColor(Pantalla[191], 0, 0, 0);
    tira.setPixelColor(Pantalla[190], 0, 0, 0);
    tira.setPixelColor(Pantalla[184], 0, 0, 255);
    tira.setPixelColor(Pantalla[183], 0, 0, 255);
    tira.setPixelColor(Pantalla[180], 0, 0, 0);
    tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[177], 0, 0, 0);
    tira.setPixelColor(Pantalla[176], 0, 0, 0);
    tira.setPixelColor(Pantalla[192], 0, 0, 0);
    tira.setPixelColor(Pantalla[199], 0, 0, 255);
    tira.setPixelColor(Pantalla[200], 0, 0, 255);
    tira.setPixelColor(Pantalla[203], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[207], 0, 255, 0);
    tira.setPixelColor(Pantalla[223], 0, 255, 0);
    tira.setPixelColor(Pantalla[222], 0, 0, 0);
    tira.setPixelColor(Pantalla[221], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[218], 0, 0, 255);
    tira.setPixelColor(Pantalla[217], 0, 0, 255);
    tira.setPixelColor(Pantalla[216], 0, 0, 255);
    tira.setPixelColor(Pantalla[215], 0, 0, 255);
    tira.setPixelColor(Pantalla[214], 0, 0, 255);
    tira.setPixelColor(Pantalla[213], 0, 0, 255);
    tira.setPixelColor(Pantalla[210], 0, 0, 0);
    tira.setPixelColor(Pantalla[209], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[208], 0, 255, 0);
    tira.setPixelColor(Pantalla[224], 0, 255, 0);
    tira.setPixelColor(Pantalla[239], 0, 255, 0);
    tira.setPixelColor(Pantalla[255], 0, 255, 0);
    tira.setPixelColor(Pantalla[254], 0, 0, 0);
    tira.setPixelColor(Pantalla[253], 0, 0, 0);
    tira.setPixelColor(Pantalla[252], 0, 0, 0);
    tira.setPixelColor(Pantalla[251], 0, 255, 0);
    tira.setPixelColor(Pantalla[250], 0, 255, 0);
    tira.setPixelColor(Pantalla[249], 0, 255, 0);
    tira.setPixelColor(Pantalla[248], 0, 0, 0);
    tira.setPixelColor(Pantalla[247], 0, 0, 0);
    tira.setPixelColor(Pantalla[246], 0, 0, 0);
    tira.setPixelColor(Pantalla[245], 0, 255, 0);
    tira.setPixelColor(Pantalla[244], 0, 255, 0);
    tira.setPixelColor(Pantalla[243], 0, 255, 0);
    tira.setPixelColor(Pantalla[242], 0, 0, 0);
    tira.setPixelColor(Pantalla[241], 0, 0, 0);
    tira.setPixelColor(Pantalla[240], 0, 0, 0);
  }
  else
  {
    tira.setPixelColor(Pantalla[0], 0, 255, 0);
    tira.setPixelColor(Pantalla[1], 0, 255, 0);
    tira.setPixelColor(Pantalla[2], 0, 255, 0);
    tira.setPixelColor(Pantalla[3], 0, 0, 0);
    tira.setPixelColor(Pantalla[4], 0, 0, 0);
    tira.setPixelColor(Pantalla[5], 0, 0, 0);
    tira.setPixelColor(Pantalla[6], 0, 255, 0);
    tira.setPixelColor(Pantalla[7], 0, 255, 0);
    tira.setPixelColor(Pantalla[8], 0, 255, 0);
    tira.setPixelColor(Pantalla[9], 0, 0, 0);
    tira.setPixelColor(Pantalla[10], 0, 0, 0);
    tira.setPixelColor(Pantalla[11], 0, 0, 0);
    tira.setPixelColor(Pantalla[12], 0, 255, 0);
    tira.setPixelColor(Pantalla[13], 0, 255, 0);
    tira.setPixelColor(Pantalla[14], 0, 255, 0);
    tira.setPixelColor(Pantalla[15], 0, 0, 0);
    tira.setPixelColor(Pantalla[31], 0, 255, 0);
    tira.setPixelColor(Pantalla[30], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[20], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[19], 0, 0, 0);
    tira.setPixelColor(Pantalla[16], 0, 0, 0);
    tira.setPixelColor(Pantalla[34], 0, 0, 0);
    tira.setPixelColor(Pantalla[35], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[37], 0, 0, 255);
    tira.setPixelColor(Pantalla[38], 100, 100, 100);
    tira.setPixelColor(Pantalla[39], 100, 100, 100);
    tira.setPixelColor(Pantalla[40], 100, 100, 100);
    tira.setPixelColor(Pantalla[41], 100, 100, 100);
    tira.setPixelColor(Pantalla[42], 0, 0, 255);
    tira.setPixelColor(Pantalla[47], 0, 0, 0);
    tira.setPixelColor(Pantalla[63], 0, 0, 0);
    tira.setPixelColor(Pantalla[58], 0, 0, 255);
    tira.setPixelColor(Pantalla[57], 100, 100, 100);
    tira.setPixelColor(Pantalla[56], 100, 100, 100);
    tira.setPixelColor(Pantalla[55], 100, 100, 100);
    tira.setPixelColor(Pantalla[54], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], 0, 0, 255);
    tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[50], 0, 0, 0);
    tira.setPixelColor(Pantalla[49], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[48], 0, 255, 0);
    tira.setPixelColor(Pantalla[64], 0, 0, 0);
    tira.setPixelColor(Pantalla[69], 0, 0, 255);
    tira.setPixelColor(Pantalla[70], 100, 100, 100);
    tira.setPixelColor(Pantalla[71], 100, 100, 100);
    tira.setPixelColor(Pantalla[72], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], 100, 100, 100);
    tira.setPixelColor(Pantalla[74], 0, 0, 255);
    tira.setPixelColor(Pantalla[75], 0, 0, 0);
    tira.setPixelColor(Pantalla[79], 0, 255, 0);
    tira.setPixelColor(Pantalla[95], 0, 0, 0);
    tira.setPixelColor(Pantalla[93], 0, 0, 0);
    tira.setPixelColor(Pantalla[90], 0, 0, 255);
    tira.setPixelColor(Pantalla[89], 0, 255, 0);
    tira.setPixelColor(Pantalla[88], 0, 255, 0);
    tira.setPixelColor(Pantalla[87], 0, 255, 0);
    tira.setPixelColor(Pantalla[86], 0, 255, 0);
    tira.setPixelColor(Pantalla[85], 0, 0, 255);
    tira.setPixelColor(Pantalla[82], 0, 0, 0);
    tira.setPixelColor(Pantalla[80], 0, 255, 0);
    tira.setPixelColor(Pantalla[96], 0, 255, 0);
    tira.setPixelColor(Pantalla[98], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[99], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[101], 0, 0, 255);
    tira.setPixelColor(Pantalla[102], 0, 255, 0);
    tira.setPixelColor(Pantalla[103], 0, 255, 0);
    tira.setPixelColor(Pantalla[104], 0, 255, 0);
    tira.setPixelColor(Pantalla[105], 0, 255, 0);
    tira.setPixelColor(Pantalla[106], 0, 0, 255);
    tira.setPixelColor(Pantalla[109], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[111], 0, 0, 0);
    tira.setPixelColor(Pantalla[127], 0, 255, 0);
    tira.setPixelColor(Pantalla[124], 0, 0, 0);
    tira.setPixelColor(Pantalla[122], 0, 0, 255);
    tira.setPixelColor(Pantalla[121], 0, 255, 0);
    tira.setPixelColor(Pantalla[120], 0, 255, 0);
    tira.setPixelColor(Pantalla[119], 0, 255, 0);
    tira.setPixelColor(Pantalla[118], 0, 255, 0);
    tira.setPixelColor(Pantalla[117], 0, 0, 255);
    tira.setPixelColor(Pantalla[112], 0, 0, 0);
    tira.setPixelColor(Pantalla[128], 0, 255, 0);
    tira.setPixelColor(Pantalla[129], 0, 0, 0);
    tira.setPixelColor(Pantalla[133], 0, 0, 255);
    tira.setPixelColor(Pantalla[134], 0, 255, 0);
    tira.setPixelColor(Pantalla[135], 0, 255, 0);
    tira.setPixelColor(Pantalla[136], 0, 255, 0);
    tira.setPixelColor(Pantalla[137], 0, 255, 0);
    tira.setPixelColor(Pantalla[138], 0, 0, 255);
    tira.setPixelColor(Pantalla[141], 0, 0, 0);
    tira.setPixelColor(Pantalla[142], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[143], 0, 0, 0);
    tira.setPixelColor(Pantalla[159], 0, 0, 0);
    tira.setPixelColor(Pantalla[158], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[153], 0, 0, 255);
    tira.setPixelColor(Pantalla[152], 0, 0, 255);
    tira.setPixelColor(Pantalla[151], 0, 0, 255);
    tira.setPixelColor(Pantalla[150], 0, 0, 255);
    tira.setPixelColor(Pantalla[148], 0, 0, 0);
    tira.setPixelColor(Pantalla[147], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[144], 0, 255, 0);
    tira.setPixelColor(Pantalla[160], 0, 0, 0);
    tira.setPixelColor(Pantalla[162], 0, 0, 0);
    tira.setPixelColor(Pantalla[167], 0, 0, 255);
    tira.setPixelColor(Pantalla[168], 0, 0, 255);
    tira.setPixelColor(Pantalla[175], 0, 255, 0);
    tira.setPixelColor(Pantalla[191], 0, 0, 0);
    tira.setPixelColor(Pantalla[190], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[184], 0, 0, 255);
    tira.setPixelColor(Pantalla[183], 0, 0, 255);
    tira.setPixelColor(Pantalla[180], 0, 0, 0);
    tira.setPixelColor(Pantalla[178], 0, 0, 0);
    tira.setPixelColor(Pantalla[177], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[176], 0, 255, 0);
    tira.setPixelColor(Pantalla[192], 0, 255, 0);
    tira.setPixelColor(Pantalla[197], 0, 0, 255);
    tira.setPixelColor(Pantalla[198], 0, 0, 255);
    tira.setPixelColor(Pantalla[199], 0, 0, 255);
    tira.setPixelColor(Pantalla[200], 0, 0, 255);
    tira.setPixelColor(Pantalla[201], 0, 0, 255);
    tira.setPixelColor(Pantalla[202], 0, 0, 255);
    tira.setPixelColor(Pantalla[203], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[207], 0, 0, 0);
    tira.setPixelColor(Pantalla[223], 0, 255, 0);
    tira.setPixelColor(Pantalla[222], 0, 255, 0);
    tira.setPixelColor(Pantalla[221], 0, 0, 0);
    tira.setPixelColor(Pantalla[220], 0, 0, 0);
    tira.setPixelColor(Pantalla[219], 0, 0, 0);
    tira.setPixelColor(Pantalla[218], 0, 255, 0);
    tira.setPixelColor(Pantalla[217], 0, 255, 0);
    tira.setPixelColor(Pantalla[216], 0, 255, 0);
    tira.setPixelColor(Pantalla[215], 0, 0, 0);
    tira.setPixelColor(Pantalla[214], 0, 0, 0);
    tira.setPixelColor(Pantalla[213], 0, 0, 0);
    tira.setPixelColor(Pantalla[212], 0, 255, 0);
    tira.setPixelColor(Pantalla[211], 0, 255, 0);
    tira.setPixelColor(Pantalla[210], 0, 255, 0);
    tira.setPixelColor(Pantalla[209], 0, 0, 0);
    tira.setPixelColor(Pantalla[208], 0, 0, 0);
  }
}
/*----------------------------------------------------------------------------*/

void No_Wifi()
{
  tira.setPixelColor(Pantalla[29], 0, 255, 0);
  tira.setPixelColor(Pantalla[25], 0, 255, 0);
  tira.setPixelColor(Pantalla[23], 0, 255, 0);
  tira.setPixelColor(Pantalla[22], 0, 255, 0);
  tira.setPixelColor(Pantalla[21], 0, 255, 0);
  tira.setPixelColor(Pantalla[20], 0, 255, 0);
  tira.setPixelColor(Pantalla[34], 0, 255, 0);
  tira.setPixelColor(Pantalla[35], 0, 255, 0);
  tira.setPixelColor(Pantalla[38], 0, 255, 0);
  tira.setPixelColor(Pantalla[40], 0, 255, 0);
  tira.setPixelColor(Pantalla[43], 0, 255, 0);
  tira.setPixelColor(Pantalla[61], 0, 255, 0);
  tira.setPixelColor(Pantalla[59], 0, 255, 0);
  tira.setPixelColor(Pantalla[57], 0, 255, 0);
  tira.setPixelColor(Pantalla[55], 0, 255, 0);
  tira.setPixelColor(Pantalla[52], 0, 255, 0);
  tira.setPixelColor(Pantalla[66], 0, 255, 0);
  tira.setPixelColor(Pantalla[69], 0, 255, 0);
  tira.setPixelColor(Pantalla[70], 0, 255, 0);
  tira.setPixelColor(Pantalla[72], 0, 255, 0);
  tira.setPixelColor(Pantalla[75], 0, 255, 0);
  tira.setPixelColor(Pantalla[93], 0, 255, 0);
  tira.setPixelColor(Pantalla[89], 0, 255, 0);
  tira.setPixelColor(Pantalla[87], 0, 255, 0);
  tira.setPixelColor(Pantalla[86], 0, 255, 0);
  tira.setPixelColor(Pantalla[85], 0, 255, 0);
  tira.setPixelColor(Pantalla[84], 0, 255, 0);

  tira.setPixelColor(Pantalla[129], 0, 255, 0);
  tira.setPixelColor(Pantalla[133], 0, 255, 0);
  tira.setPixelColor(Pantalla[135], 0, 255, 0);
  tira.setPixelColor(Pantalla[140], 0, 255, 0);
  tira.setPixelColor(Pantalla[141], 0, 255, 0);
  tira.setPixelColor(Pantalla[143], 0, 255, 0);
  tira.setPixelColor(Pantalla[158], 0, 255, 0);
  tira.setPixelColor(Pantalla[156], 0, 255, 0);
  tira.setPixelColor(Pantalla[154], 0, 255, 0);
  tira.setPixelColor(Pantalla[152], 0, 255, 0);
  tira.setPixelColor(Pantalla[147], 0, 255, 0);
  tira.setPixelColor(Pantalla[144], 0, 255, 0);
  tira.setPixelColor(Pantalla[161], 0, 255, 0);
  tira.setPixelColor(Pantalla[163], 0, 255, 0);
  tira.setPixelColor(Pantalla[165], 0, 255, 0);
  tira.setPixelColor(Pantalla[167], 0, 255, 0);
  tira.setPixelColor(Pantalla[169], 0, 255, 0);
  tira.setPixelColor(Pantalla[170], 0, 255, 0);
  tira.setPixelColor(Pantalla[172], 0, 255, 0);
  tira.setPixelColor(Pantalla[173], 0, 255, 0);
  tira.setPixelColor(Pantalla[175], 0, 255, 0);
  tira.setPixelColor(Pantalla[190], 0, 255, 0);
  tira.setPixelColor(Pantalla[188], 0, 255, 0);
  tira.setPixelColor(Pantalla[186], 0, 255, 0);
  tira.setPixelColor(Pantalla[184], 0, 255, 0);
  tira.setPixelColor(Pantalla[179], 0, 255, 0);
  tira.setPixelColor(Pantalla[176], 0, 255, 0);
  tira.setPixelColor(Pantalla[193], 0, 255, 0);
  tira.setPixelColor(Pantalla[194], 0, 255, 0);
  tira.setPixelColor(Pantalla[195], 0, 255, 0);
  tira.setPixelColor(Pantalla[196], 0, 255, 0);
  tira.setPixelColor(Pantalla[197], 0, 255, 0);
  tira.setPixelColor(Pantalla[199], 0, 255, 0);
  tira.setPixelColor(Pantalla[204], 0, 255, 0);
  tira.setPixelColor(Pantalla[207], 0, 255, 0);
}
/*----------------------------------------------------------------------------*/




/*--------------------------------------------------*/
void Asigna_Color_Basandose_En_Color_Recibido_De_Monitor() //Ver 4.0
{
  // define color del aro
  switch (datos_recibidos.c)
  {
  case GREEN:
    /*code*/
    pixel_rojo = 250;
    pixel_verde = 0;
    pixel_azul = 0;
    break;

  case BLUE:
    /*code*/
    // Serial.println("Programa color azul");
    pixel_rojo = 0;
    pixel_verde = 0;
    pixel_azul = 250;
    break;

  case RED:
    /*code*/
    // Serial.println("Programa color rojo para castigos");
    pixel_rojo = 0;
    pixel_verde = 250;
    pixel_azul = 0;
    break;

  case ARCOIRIS:
    /*code*/
    pixel_rojo = int(rand() % 255);
    pixel_verde = int(rand() % 255);
    pixel_azul = int(rand() % 255);
    break;
  }
}



/*----------------------------------*/

void Arma_Paquete_De_Envio() //Ver 4.0
{
  // LLENADO DE PAQUETE A ENVIAR
  // Control
  datos_enviados.n = datos_recibidos.n;
  // Propiedades de juego
  datos_enviados.ju = datos_recibidos.ju; //[Monitor]1 CLASICO-WESTERN / 2 TORNEO /3 PAREJAS/4 VELOCIDAD
  datos_enviados.jr = datos_recibidos.jr; // [Monitor]No de Round para definicion de velocidad, no aplica en diana
  // Propiedades del Paquete
  datos_enviados.t = datos_recibidos.t; //[Monitor]TEST=0, TIRO_ACTIVO=1, TIRO_CALIBRA=2 es es el recibido en el mensaje de llegada
  datos_enviados.d = datos_recibidos.d; //[MOnitor]No de Diana
  datos_enviados.c = datos_recibidos.c; //[Monitor]Color del tiro enviado a DIana
  datos_enviados.p = datos_recibidos.p; //[Monitor]Propietario Original del tiro

  // Propiedades del tiro
  datos_enviados.s = datos_recibidos.s; //[Monitor]Numero de tiro asignado
  // datos_enviados.tiempo=(tiempo_termina_tiro-tiempo_inicia_tiro)/1000.0; // para regresar el tiempo en segundos

  // Condiciones de manejo del tiro
  datos_enviados.pp = datos_recibidos.pp; //[Monitor] PRIVADO O PUBLICO el tipo de tiro
  datos_enviados.pa = datos_recibidos.pa; //[Monitor] Propietario Alternativo del tiro en caso PUBLICO

  // Propiedades de la figura
  datos_enviados.vf = datos_recibidos.vf; //[Monitor] Velocidad de cambio de figura, establece Monitor para cada tiro
  datos_enviados.tf = datos_recibidos.tf; //[Monitor]TEST=0, NORMAL=1, BONO=2, CASTIGO=3, CALIBRA=4
  datos_enviados.vt = datos_recibidos.vt; //[monitor] Cuanto vale el tiro NORMAL= 1 tiro, BONO =1 tiro, CASTIGO=0
  datos_enviados.xf = datos_recibidos.xf; //[Monitor] Figura a iniciar en DIana,solo para BOnos
                                          // ya que cstigo es definido y normal siempre empieza con el mayor

  // Resultados del disparo
  //  Analisis del propieatrio de los puntos generados por el disparo
  if (datos_recibidos.pp == PRIVADO)
  {
    datos_enviados.jg = datos_recibidos.p; //[Diana] siempre se asigna al propietario del tiro
  }

  if (datos_recibidos.pp == PUBLICO)
  {
    if (datos_del_tiro.color_del_tiro == datos_recibidos.c) //[Diana]color de jugador original
    {
      datos_enviados.jg = datos_recibidos.p;
    }
    else
    {
      datos_enviados.jg = datos_recibidos.pa;
    }
  }
  datos_enviados.po = datos_del_tiro.puntos_figura; //[Diana]puntuacion de la figura impactada
  datos_enviados.fi = datos_del_tiro.figura_impactada;           //[Diana]figura impactada en tiro
   Paquete_datos_enviados();
}

void Dibuja_Marco_Intermitente()
{
  // define color con base en el color de la pistola detectada
  switch (datos_del_tiro.color_del_tiro)
  { // rick
  case GREEN:
    /*code*/
    pixel_rojo = 250;
    pixel_verde = 0;
    pixel_azul = 0;
    break;

  case BLUE:
    /*code*/
    // Serial.println("Programa color azul");
    pixel_rojo = 0;
    pixel_verde = 0;
    pixel_azul = 250;
    break;

  default:
    pixel_rojo = 0;
    pixel_verde = 255;
    pixel_azul = 0;
    break;
  }

  // Define el aro a mostrar alrededor de la figura
  if (posicion_led_apunta > 2)
  {
    posicion_led_apunta = 1;
  }
  switch (posicion_led_apunta)
  { // rick
  case UNO:
    Marco_Intermitente1();
    Matriz_Apuntando_Fijo();
    posicion_led_apunta++;
    break;
  case DOS:
    Marco_Intermitente2();
    Matriz_Apuntando_Fijo();
    posicion_led_apunta++;
    break;
  }
  tira.show();
}

void Aro_Apuntando_Secuencia() //ver 4.0
{
  // define color con base en el color de la pistola detectada
  switch (datos_del_tiro.color_del_tiro)
  { // rick
  case GREEN:
    /*code*/
    pixel_rojo = 250;
    pixel_verde = 0;
    pixel_azul = 0;
    break;

  case BLUE:
    /*code*/
    // Serial.println("Programa color azul");
    pixel_rojo = 0;
    pixel_verde = 0;
    pixel_azul = 250;
    break;

  default:
    pixel_rojo = 0;
    pixel_verde =255;
    pixel_azul = 0;
    break;
  }

  // Define el aro a mostrar alrededor de la figura
  if (posicion_led_apunta > 4)
  {
    posicion_led_apunta = 1;
  }
  switch (posicion_led_apunta)
  { // rick
  case UNO:
    Matriz_Aro_1_Apuntado();
    Matriz_Apuntando_Fijo();
    posicion_led_apunta++;
    break;
  case DOS:
    Matriz_Aro_2_Apuntado();
    Matriz_Apuntando_Fijo();
    posicion_led_apunta++;
    break;
  case TRES:
    Matriz_Aro_3_Apuntado();
    Matriz_Apuntando_Fijo();
    posicion_led_apunta++;
    break;
  case CUATRO:
    Matriz_Aro_4_Apuntado();
    Matriz_Apuntando_Fijo();
    posicion_led_apunta++;
    break;
  }
  tira.show();
}


/*--------------------------------------------------------*/

void Matriz_Apuntando_Fijo() // Ver 4.0
{
  tira.setPixelColor(Pantalla[256], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[257], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[258], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[259], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[260], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[261], pixel_rojo, pixel_verde, pixel_azul); 
}

void Matriz_Apuntando_Intermitente() // Ver 4.0
{
  if (lado==IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[256], 0, 0, 0);
    tira.setPixelColor(Pantalla[257], 0, 0, 0);
    tira.setPixelColor(Pantalla[258], 0, 0, 0);
    tira.setPixelColor(Pantalla[259], 0, 0, 0);
    tira.setPixelColor(Pantalla[260], 0, 0, 0);
    tira.setPixelColor(Pantalla[261], 0, 0, 0);
  }
  else
  {
  tira.setPixelColor(Pantalla[256], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[257], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[258], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[259], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[260], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[261], pixel_rojo, pixel_verde, pixel_azul); 
  }
}





/*--------------------------------------------------------*/

void Dibuja_Valor_Disparo() // ver 4.0
{
  tira.clear();

  // PASO 1 Dibuja Valor figura
  Serial.println("datos_del_tiro.puntos_figura"+String(datos_del_tiro.puntos_figura));
  for (int i = 1; i <= 9; i++)
  {
    switch (datos_del_tiro.figura_impactada)
    {
      // Normal
    case NORMAL_OSO:
      /* code */
      Valor_50();
      break;
    case NORMAL_ZORRA:
      Valor_40();
      break;
    case NORMAL_MARIPOSA:
      Valor_45();
      break;
    case NORMAL_PINGUINO:
      Valor_75();
      break;
    case NORMAL_ARANA:
      Valor_35();
      break;
    case NORMAL_LAGARTIJA:
      Valor_30();
      break;
    case NORMAL_RANA:
      Valor_50();
      break;
    // Bonos
    case BONO_CORAZON:
      Valor_300();
      break;
    case BONO_PINGUINO:
      Valor_200();
      break;
    case BONO_RANA:
      Valor_150();
      break;
    case BONO_CORAZON_200:
      Valor_100();
      break;
    case CALIBRA_TEST:
      Matriz_Ok();
      break;
    // castigos
    case CASTIGO_UNICORNIO:
      Valor_Menos_200();
      break;
    case CASTIGO_BOTELLA:
      Valor_Menos_100();
      break;
    case CASTIGO_COPA:
      Valor_Menos_70();
      break;
    }
    tira.show();
    Espera(100);
  }
  tira.clear();
  tira.show();
}

void Valor_50(){
tira.setPixelColor(Pantalla[ 7],255,255,0);
tira.setPixelColor(Pantalla[ 24],255,255,0);
tira.setPixelColor(Pantalla[ 41],255,255,0);tira.setPixelColor(Pantalla[ 40],255,255,0);tira.setPixelColor(Pantalla[ 39],255,255,0);tira.setPixelColor(Pantalla[ 38],255,255,0);tira.setPixelColor(Pantalla[ 37],255,255,0);
tira.setPixelColor(Pantalla[ 56],255,255,0);
tira.setPixelColor(Pantalla[ 71],255,255,0);

tira.setPixelColor(Pantalla[ 109],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 108],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 107],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 106],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 105],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 102],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 101],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 100],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 99],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 114],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 121],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 125],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 141],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 134],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 130],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 146],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 153],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 157],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 173],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 172],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 171],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 170],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 169],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 166],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 162],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 182],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 185],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 189],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 201],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 198],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 194],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 214],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 217],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 221],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 237],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 236],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 235],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 234],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 233],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 230],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 229],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 228],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 227],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 226],(10+rand()%245),(10+rand()%245),(10+rand()%245));

}

void Valor_40(){
tira.setPixelColor(Pantalla[ 7],255,255,0);
tira.setPixelColor(Pantalla[ 24],255,255,0);
tira.setPixelColor(Pantalla[ 41],255,255,0);tira.setPixelColor(Pantalla[ 40],255,255,0);tira.setPixelColor(Pantalla[ 39],255,255,0);tira.setPixelColor(Pantalla[ 38],255,255,0);tira.setPixelColor(Pantalla[ 37],255,255,0);
tira.setPixelColor(Pantalla[ 56],255,255,0);
tira.setPixelColor(Pantalla[ 71],255,255,0);

tira.setPixelColor(Pantalla[ 109],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 105],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 102],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 101],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 100],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 99],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 114],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 118],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 121],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 125],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 141],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 137],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 134],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 130],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 146],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 150],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 153],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 157],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 173],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 172],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 171],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 170],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 169],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 166],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 162],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 182],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 185],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 189],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 201],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 198],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 194],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 214],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 217],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 221],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 233],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 230],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 229],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 228],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 227],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 226],(10+rand()%245),(10+rand()%245),(10+rand()%245));

}

void Valor_45(){
tira.setPixelColor(Pantalla[ 7],255,255,0);
tira.setPixelColor(Pantalla[ 24],255,255,0);
tira.setPixelColor(Pantalla[ 41],255,255,0);tira.setPixelColor(Pantalla[ 40],255,255,0);tira.setPixelColor(Pantalla[ 39],255,255,0);tira.setPixelColor(Pantalla[ 38],255,255,0);tira.setPixelColor(Pantalla[ 37],255,255,0);
tira.setPixelColor(Pantalla[ 56],255,255,0);
tira.setPixelColor(Pantalla[ 71],255,255,0);

tira.setPixelColor(Pantalla[ 109],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 105],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 102],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 101],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 100],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 99],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 114],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 118],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 121],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 141],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 137],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 134],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 146],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 150],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 153],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 173],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 172],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 171],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 170],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 169],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 166],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 165],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 164],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 163],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 162],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 182],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 189],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 201],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 194],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 214],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 221],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 233],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 230],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 229],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 228],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 227],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 226],(10+rand()%245),(10+rand()%245),(10+rand()%245));

}

void Valor_75(){
tira.setPixelColor(Pantalla[ 7],255,255,0);
tira.setPixelColor(Pantalla[ 24],255,255,0);
tira.setPixelColor(Pantalla[ 41],255,255,0);tira.setPixelColor(Pantalla[ 40],255,255,0);tira.setPixelColor(Pantalla[ 39],255,255,0);tira.setPixelColor(Pantalla[ 38],255,255,0);tira.setPixelColor(Pantalla[ 37],255,255,0);
tira.setPixelColor(Pantalla[ 56],255,255,0);
tira.setPixelColor(Pantalla[ 71],255,255,0);

tira.setPixelColor(Pantalla[ 109],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 108],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 107],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 106],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 105],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 102],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 101],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 100],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 99],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 118],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 121],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 137],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 134],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 150],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 153],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 169],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 166],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 165],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 164],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 163],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 162],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 182],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 189],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 201],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 194],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 214],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 221],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 233],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 230],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 229],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 228],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 227],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 226],(10+rand()%245),(10+rand()%245),(10+rand()%245));

}

void Valor_35()
{
tira.setPixelColor(Pantalla[ 7],255,255,0);
tira.setPixelColor(Pantalla[ 24],255,255,0);
tira.setPixelColor(Pantalla[ 41],255,255,0);tira.setPixelColor(Pantalla[ 40],255,255,0);tira.setPixelColor(Pantalla[ 39],255,255,0);tira.setPixelColor(Pantalla[ 38],255,255,0);tira.setPixelColor(Pantalla[ 37],255,255,0);
tira.setPixelColor(Pantalla[ 56],255,255,0);
tira.setPixelColor(Pantalla[ 71],255,255,0);

tira.setPixelColor(Pantalla[ 109],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 108],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 107],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 106],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 105],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 102],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 101],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 100],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 99],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 118],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 121],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 137],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 134],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 150],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 153],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 173],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 172],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 171],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 170],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 169],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 166],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 165],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 164],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 163],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 162],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 182],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 189],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 201],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 194],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 214],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 221],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 237],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 236],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 235],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 234],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 233],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 230],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 229],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 228],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 227],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 226],(10+rand()%245),(10+rand()%245),(10+rand()%245));

}

void Valor_30(){
tira.setPixelColor(Pantalla[ 7],255,255,0);
tira.setPixelColor(Pantalla[ 24],255,255,0);
tira.setPixelColor(Pantalla[ 41],255,255,0);tira.setPixelColor(Pantalla[ 40],255,255,0);tira.setPixelColor(Pantalla[ 39],255,255,0);tira.setPixelColor(Pantalla[ 38],255,255,0);tira.setPixelColor(Pantalla[ 37],255,255,0);
tira.setPixelColor(Pantalla[ 56],255,255,0);
tira.setPixelColor(Pantalla[ 71],255,255,0);

tira.setPixelColor(Pantalla[ 109],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 108],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 107],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 106],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 105],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 102],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 101],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 100],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 99],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 118],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 121],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 125],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 137],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 134],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 130],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 150],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 153],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 157],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 173],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 172],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 171],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 170],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 169],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 166],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 162],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 182],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 185],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 189],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 201],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 198],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 194],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 214],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 217],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 221],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 237],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 236],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 235],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 234],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 233],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 230],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 229],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 228],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 227],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 226],(10+rand()%245),(10+rand()%245),(10+rand()%245));

}

void Valor_300(){
tira.setPixelColor(Pantalla[ 7],255,255,0);
tira.setPixelColor(Pantalla[ 24],255,255,0);
tira.setPixelColor(Pantalla[ 41],255,255,0);tira.setPixelColor(Pantalla[ 40],255,255,0);tira.setPixelColor(Pantalla[ 39],255,255,0);tira.setPixelColor(Pantalla[ 38],255,255,0);tira.setPixelColor(Pantalla[ 37],255,255,0);
tira.setPixelColor(Pantalla[ 56],255,255,0);
tira.setPixelColor(Pantalla[ 71],255,255,0);

tira.setPixelColor(Pantalla[ 110],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 109],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 108],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 107],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 105],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 104],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 103],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 102],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 100],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 99],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 97],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 116],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 118],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 121],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 123],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 126],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 139],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 137],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 134],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 132],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 129],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 148],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 150],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 153],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 155],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 158],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 174],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 173],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 172],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 171],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 169],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 166],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 164],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 161],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 180],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 182],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 185],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 187],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 190],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 203],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 201],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 198],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 196],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 193],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 212],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 214],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 217],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 219],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 222],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 238],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 237],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 236],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 235],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 233],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 232],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 231],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 230],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 228],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 227],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 226],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 225],(10+rand()%245),(10+rand()%245),(10+rand()%245));

}

void Valor_200()
{
tira.setPixelColor(Pantalla[ 7],255,255,0);
tira.setPixelColor(Pantalla[ 24],255,255,0);
tira.setPixelColor(Pantalla[ 41],255,255,0);tira.setPixelColor(Pantalla[ 40],255,255,0);tira.setPixelColor(Pantalla[ 39],255,255,0);tira.setPixelColor(Pantalla[ 38],255,255,0);tira.setPixelColor(Pantalla[ 37],255,255,0);
tira.setPixelColor(Pantalla[ 56],255,255,0);
tira.setPixelColor(Pantalla[ 71],255,255,0);

tira.setPixelColor(Pantalla[ 110],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 109],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 108],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 107],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 105],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 104],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 103],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 102],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 100],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 99],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 97],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 116],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 118],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 121],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 123],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 126],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 139],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 137],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 134],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 132],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 129],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 148],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 150],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 153],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 155],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 158],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 174],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 173],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 172],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 171],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 169],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 166],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 164],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 161],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 177],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 182],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 185],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 187],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 190],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 206],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 201],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 198],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 196],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 193],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 209],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 214],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 217],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 219],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 222],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 238],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 237],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 236],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 235],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 233],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 232],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 231],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 230],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 228],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 227],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 226],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 225],(10+rand()%245),(10+rand()%245),(10+rand()%245));

}

void Valor_150(){
tira.setPixelColor(Pantalla[ 7],255,255,0);
tira.setPixelColor(Pantalla[ 24],255,255,0);
tira.setPixelColor(Pantalla[ 41],255,255,0);tira.setPixelColor(Pantalla[ 40],255,255,0);tira.setPixelColor(Pantalla[ 39],255,255,0);tira.setPixelColor(Pantalla[ 38],255,255,0);tira.setPixelColor(Pantalla[ 37],255,255,0);
tira.setPixelColor(Pantalla[ 56],255,255,0);
tira.setPixelColor(Pantalla[ 71],255,255,0);

tira.setPixelColor(Pantalla[ 108],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 105],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 104],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 103],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 102],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 100],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 99],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 97],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 115],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 118],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 123],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 126],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 140],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 137],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 132],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 129],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 147],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 150],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 155],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 158],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 172],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 169],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 168],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 167],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 166],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 164],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 161],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 179],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 185],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 187],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 190],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 204],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 198],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 196],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 193],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 211],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 217],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 219],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 222],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 236],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 233],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 232],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 231],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 230],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 228],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 227],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 226],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 225],(10+rand()%245),(10+rand()%245),(10+rand()%245));

}


void Valor_100()
{
tira.setPixelColor(Pantalla[ 7],255,255,0);
tira.setPixelColor(Pantalla[ 24],255,255,0);
tira.setPixelColor(Pantalla[ 41],255,255,0);tira.setPixelColor(Pantalla[ 40],255,255,0);tira.setPixelColor(Pantalla[ 39],255,255,0);tira.setPixelColor(Pantalla[ 38],255,255,0);tira.setPixelColor(Pantalla[ 37],255,255,0);
tira.setPixelColor(Pantalla[ 56],255,255,0);
tira.setPixelColor(Pantalla[ 71],255,255,0);

tira.setPixelColor(Pantalla[ 108],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 105],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 104],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 103],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 102],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 100],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 99],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 97],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 115],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 118],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 121],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 123],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 126],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 140],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 137],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 134],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 132],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 129],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 147],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 150],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 153],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 155],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 158],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 172],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 169],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 166],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 164],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 161],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 179],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 182],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 185],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 187],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 190],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 204],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 201],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 198],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 196],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 193],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 211],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 214],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 217],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 219],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 222],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 236],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 233],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 232],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 231],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 230],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 228],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 227],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 226],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 225],(10+rand()%245),(10+rand()%245),(10+rand()%245));

}

void Valor_20(){
tira.setPixelColor(Pantalla[ 7],255,255,0);
tira.setPixelColor(Pantalla[ 24],255,255,0);
tira.setPixelColor(Pantalla[ 41],255,255,0);tira.setPixelColor(Pantalla[ 40],255,255,0);tira.setPixelColor(Pantalla[ 39],255,255,0);tira.setPixelColor(Pantalla[ 38],255,255,0);tira.setPixelColor(Pantalla[ 37],255,255,0);
tira.setPixelColor(Pantalla[ 56],255,255,0);
tira.setPixelColor(Pantalla[ 71],255,255,0);

tira.setPixelColor(Pantalla[ 109],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 108],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 107],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 106],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 105],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 102],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 101],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 100],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 99],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 98],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 118],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 121],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 125],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 137],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 134],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 130],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 150],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 153],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 157],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 173],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 172],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 171],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 170],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 169],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 166],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 162],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 178],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 185],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 189],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 205],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 198],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 194],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 210],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 217],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 221],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 237],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 236],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 235],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 234],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 233],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 230],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 229],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 228],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 227],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 226],(10+rand()%245),(10+rand()%245),(10+rand()%245));

}

void Valor_Menos_200(){
tira.setPixelColor(Pantalla[ 41],0,255,0);tira.setPixelColor(Pantalla[ 40],0,255,0);tira.setPixelColor(Pantalla[ 39],0,255,0);tira.setPixelColor(Pantalla[ 38],0,255,0);tira.setPixelColor(Pantalla[ 37],0,255,0);
tira.setPixelColor(Pantalla[ 110],0,255,0);tira.setPixelColor(Pantalla[ 109],0,255,0);tira.setPixelColor(Pantalla[ 108],0,255,0);tira.setPixelColor(Pantalla[ 107],0,255,0);tira.setPixelColor(Pantalla[ 105],0,255,0);tira.setPixelColor(Pantalla[ 104],0,255,0);tira.setPixelColor(Pantalla[ 103],0,255,0);tira.setPixelColor(Pantalla[ 102],0,255,0);tira.setPixelColor(Pantalla[ 100],0,255,0);tira.setPixelColor(Pantalla[ 99],0,255,0);tira.setPixelColor(Pantalla[ 98],0,255,0);tira.setPixelColor(Pantalla[ 97],0,255,0);
tira.setPixelColor(Pantalla[ 116],0,255,0);tira.setPixelColor(Pantalla[ 118],0,255,0);tira.setPixelColor(Pantalla[ 121],0,255,0);tira.setPixelColor(Pantalla[ 123],0,255,0);tira.setPixelColor(Pantalla[ 126],0,255,0);
tira.setPixelColor(Pantalla[ 139],0,255,0);tira.setPixelColor(Pantalla[ 137],0,255,0);tira.setPixelColor(Pantalla[ 134],0,255,0);tira.setPixelColor(Pantalla[ 132],0,255,0);tira.setPixelColor(Pantalla[ 129],0,255,0);
tira.setPixelColor(Pantalla[ 148],0,255,0);tira.setPixelColor(Pantalla[ 150],0,255,0);tira.setPixelColor(Pantalla[ 153],0,255,0);tira.setPixelColor(Pantalla[ 155],0,255,0);tira.setPixelColor(Pantalla[ 158],0,255,0);
tira.setPixelColor(Pantalla[ 174],0,255,0);tira.setPixelColor(Pantalla[ 173],0,255,0);tira.setPixelColor(Pantalla[ 172],0,255,0);tira.setPixelColor(Pantalla[ 171],0,255,0);tira.setPixelColor(Pantalla[ 169],0,255,0);tira.setPixelColor(Pantalla[ 166],0,255,0);tira.setPixelColor(Pantalla[ 164],0,255,0);tira.setPixelColor(Pantalla[ 161],0,255,0);
tira.setPixelColor(Pantalla[ 177],0,255,0);tira.setPixelColor(Pantalla[ 182],0,255,0);tira.setPixelColor(Pantalla[ 185],0,255,0);tira.setPixelColor(Pantalla[ 187],0,255,0);tira.setPixelColor(Pantalla[ 190],0,255,0);
tira.setPixelColor(Pantalla[ 206],0,255,0);tira.setPixelColor(Pantalla[ 201],0,255,0);tira.setPixelColor(Pantalla[ 198],0,255,0);tira.setPixelColor(Pantalla[ 196],0,255,0);tira.setPixelColor(Pantalla[ 193],0,255,0);
tira.setPixelColor(Pantalla[ 209],0,255,0);tira.setPixelColor(Pantalla[ 214],0,255,0);tira.setPixelColor(Pantalla[ 217],0,255,0);tira.setPixelColor(Pantalla[ 219],0,255,0);tira.setPixelColor(Pantalla[ 222],0,255,0);
tira.setPixelColor(Pantalla[ 238],0,255,0);tira.setPixelColor(Pantalla[ 237],0,255,0);tira.setPixelColor(Pantalla[ 236],0,255,0);tira.setPixelColor(Pantalla[ 235],0,255,0);tira.setPixelColor(Pantalla[ 233],0,255,0);tira.setPixelColor(Pantalla[ 232],0,255,0);tira.setPixelColor(Pantalla[ 231],0,255,0);tira.setPixelColor(Pantalla[ 230],0,255,0);tira.setPixelColor(Pantalla[ 228],0,255,0);tira.setPixelColor(Pantalla[ 227],0,255,0);tira.setPixelColor(Pantalla[ 226],0,255,0);tira.setPixelColor(Pantalla[ 225],0,255,0);

}

void Valor_Menos_100(){
tira.setPixelColor(Pantalla[ 41],0,255,0);tira.setPixelColor(Pantalla[ 40],0,255,0);tira.setPixelColor(Pantalla[ 39],0,255,0);tira.setPixelColor(Pantalla[ 38],0,255,0);tira.setPixelColor(Pantalla[ 37],0,255,0);
tira.setPixelColor(Pantalla[ 108],0,255,0);tira.setPixelColor(Pantalla[ 105],0,255,0);tira.setPixelColor(Pantalla[ 104],0,255,0);tira.setPixelColor(Pantalla[ 103],0,255,0);tira.setPixelColor(Pantalla[ 102],0,255,0);tira.setPixelColor(Pantalla[ 100],0,255,0);tira.setPixelColor(Pantalla[ 99],0,255,0);tira.setPixelColor(Pantalla[ 98],0,255,0);tira.setPixelColor(Pantalla[ 97],0,255,0);
tira.setPixelColor(Pantalla[ 115],0,255,0);tira.setPixelColor(Pantalla[ 118],0,255,0);tira.setPixelColor(Pantalla[ 121],0,255,0);tira.setPixelColor(Pantalla[ 123],0,255,0);tira.setPixelColor(Pantalla[ 126],0,255,0);
tira.setPixelColor(Pantalla[ 140],0,255,0);tira.setPixelColor(Pantalla[ 137],0,255,0);tira.setPixelColor(Pantalla[ 134],0,255,0);tira.setPixelColor(Pantalla[ 132],0,255,0);tira.setPixelColor(Pantalla[ 129],0,255,0);
tira.setPixelColor(Pantalla[ 147],0,255,0);tira.setPixelColor(Pantalla[ 150],0,255,0);tira.setPixelColor(Pantalla[ 153],0,255,0);tira.setPixelColor(Pantalla[ 155],0,255,0);tira.setPixelColor(Pantalla[ 158],0,255,0);
tira.setPixelColor(Pantalla[ 172],0,255,0);tira.setPixelColor(Pantalla[ 169],0,255,0);tira.setPixelColor(Pantalla[ 166],0,255,0);tira.setPixelColor(Pantalla[ 164],0,255,0);tira.setPixelColor(Pantalla[ 161],0,255,0);
tira.setPixelColor(Pantalla[ 179],0,255,0);tira.setPixelColor(Pantalla[ 182],0,255,0);tira.setPixelColor(Pantalla[ 185],0,255,0);tira.setPixelColor(Pantalla[ 187],0,255,0);tira.setPixelColor(Pantalla[ 190],0,255,0);
tira.setPixelColor(Pantalla[ 204],0,255,0);tira.setPixelColor(Pantalla[ 201],0,255,0);tira.setPixelColor(Pantalla[ 198],0,255,0);tira.setPixelColor(Pantalla[ 196],0,255,0);tira.setPixelColor(Pantalla[ 193],0,255,0);
tira.setPixelColor(Pantalla[ 211],0,255,0);tira.setPixelColor(Pantalla[ 214],0,255,0);tira.setPixelColor(Pantalla[ 217],0,255,0);tira.setPixelColor(Pantalla[ 219],0,255,0);tira.setPixelColor(Pantalla[ 222],0,255,0);
tira.setPixelColor(Pantalla[ 236],0,255,0);tira.setPixelColor(Pantalla[ 233],0,255,0);tira.setPixelColor(Pantalla[ 232],0,255,0);tira.setPixelColor(Pantalla[ 231],0,255,0);tira.setPixelColor(Pantalla[ 230],0,255,0);tira.setPixelColor(Pantalla[ 228],0,255,0);tira.setPixelColor(Pantalla[ 227],0,255,0);tira.setPixelColor(Pantalla[ 226],0,255,0);tira.setPixelColor(Pantalla[ 225],0,255,0);

}

void Valor_Menos_70(){
tira.setPixelColor(Pantalla[ 41],0,255,0);tira.setPixelColor(Pantalla[ 40],0,255,0);tira.setPixelColor(Pantalla[ 39],0,255,0);tira.setPixelColor(Pantalla[ 38],0,255,0);tira.setPixelColor(Pantalla[ 37],0,255,0);
tira.setPixelColor(Pantalla[ 109],0,255,0);tira.setPixelColor(Pantalla[ 108],0,255,0);tira.setPixelColor(Pantalla[ 107],0,255,0);tira.setPixelColor(Pantalla[ 106],0,255,0);tira.setPixelColor(Pantalla[ 105],0,255,0);tira.setPixelColor(Pantalla[ 102],0,255,0);tira.setPixelColor(Pantalla[ 101],0,255,0);tira.setPixelColor(Pantalla[ 100],0,255,0);tira.setPixelColor(Pantalla[ 99],0,255,0);tira.setPixelColor(Pantalla[ 98],0,255,0);
tira.setPixelColor(Pantalla[ 118],0,255,0);tira.setPixelColor(Pantalla[ 121],0,255,0);tira.setPixelColor(Pantalla[ 125],0,255,0);
tira.setPixelColor(Pantalla[ 137],0,255,0);tira.setPixelColor(Pantalla[ 134],0,255,0);tira.setPixelColor(Pantalla[ 130],0,255,0);
tira.setPixelColor(Pantalla[ 150],0,255,0);tira.setPixelColor(Pantalla[ 153],0,255,0);tira.setPixelColor(Pantalla[ 157],0,255,0);
tira.setPixelColor(Pantalla[ 169],0,255,0);tira.setPixelColor(Pantalla[ 166],0,255,0);tira.setPixelColor(Pantalla[ 162],0,255,0);
tira.setPixelColor(Pantalla[ 182],0,255,0);tira.setPixelColor(Pantalla[ 185],0,255,0);tira.setPixelColor(Pantalla[ 189],0,255,0);
tira.setPixelColor(Pantalla[ 201],0,255,0);tira.setPixelColor(Pantalla[ 198],0,255,0);tira.setPixelColor(Pantalla[ 194],0,255,0);
tira.setPixelColor(Pantalla[ 214],0,255,0);tira.setPixelColor(Pantalla[ 217],0,255,0);tira.setPixelColor(Pantalla[ 221],0,255,0);
tira.setPixelColor(Pantalla[ 233],0,255,0);tira.setPixelColor(Pantalla[ 230],0,255,0);tira.setPixelColor(Pantalla[ 229],0,255,0);tira.setPixelColor(Pantalla[ 228],0,255,0);tira.setPixelColor(Pantalla[ 227],0,255,0);tira.setPixelColor(Pantalla[ 226],0,255,0);

}

void Valor_00(){
tira.setPixelColor(Pantalla[ 110],0,255,0);tira.setPixelColor(Pantalla[ 109],0,255,0);tira.setPixelColor(Pantalla[ 108],0,255,0);tira.setPixelColor(Pantalla[ 107],0,255,0);tira.setPixelColor(Pantalla[ 105],0,255,0);tira.setPixelColor(Pantalla[ 104],0,255,0);tira.setPixelColor(Pantalla[ 103],0,255,0);tira.setPixelColor(Pantalla[ 102],0,255,0);tira.setPixelColor(Pantalla[ 100],0,255,0);tira.setPixelColor(Pantalla[ 99],0,255,0);tira.setPixelColor(Pantalla[ 98],0,255,0);tira.setPixelColor(Pantalla[ 97],0,255,0);
tira.setPixelColor(Pantalla[ 113],0,255,0);tira.setPixelColor(Pantalla[ 116],0,255,0);tira.setPixelColor(Pantalla[ 118],0,255,0);tira.setPixelColor(Pantalla[ 121],0,255,0);tira.setPixelColor(Pantalla[ 123],0,255,0);tira.setPixelColor(Pantalla[ 126],0,255,0);
tira.setPixelColor(Pantalla[ 142],0,255,0);tira.setPixelColor(Pantalla[ 139],0,255,0);tira.setPixelColor(Pantalla[ 137],0,255,0);tira.setPixelColor(Pantalla[ 134],0,255,0);tira.setPixelColor(Pantalla[ 132],0,255,0);tira.setPixelColor(Pantalla[ 129],0,255,0);
tira.setPixelColor(Pantalla[ 145],0,255,0);tira.setPixelColor(Pantalla[ 148],0,255,0);tira.setPixelColor(Pantalla[ 150],0,255,0);tira.setPixelColor(Pantalla[ 153],0,255,0);tira.setPixelColor(Pantalla[ 155],0,255,0);tira.setPixelColor(Pantalla[ 158],0,255,0);
tira.setPixelColor(Pantalla[ 174],0,255,0);tira.setPixelColor(Pantalla[ 171],0,255,0);tira.setPixelColor(Pantalla[ 169],0,255,0);tira.setPixelColor(Pantalla[ 166],0,255,0);tira.setPixelColor(Pantalla[ 164],0,255,0);tira.setPixelColor(Pantalla[ 161],0,255,0);
tira.setPixelColor(Pantalla[ 177],0,255,0);tira.setPixelColor(Pantalla[ 180],0,255,0);tira.setPixelColor(Pantalla[ 182],0,255,0);tira.setPixelColor(Pantalla[ 185],0,255,0);tira.setPixelColor(Pantalla[ 187],0,255,0);tira.setPixelColor(Pantalla[ 190],0,255,0);
tira.setPixelColor(Pantalla[ 206],0,255,0);tira.setPixelColor(Pantalla[ 203],0,255,0);tira.setPixelColor(Pantalla[ 201],0,255,0);tira.setPixelColor(Pantalla[ 198],0,255,0);tira.setPixelColor(Pantalla[ 196],0,255,0);tira.setPixelColor(Pantalla[ 193],0,255,0);
tira.setPixelColor(Pantalla[ 209],0,255,0);tira.setPixelColor(Pantalla[ 212],0,255,0);tira.setPixelColor(Pantalla[ 214],0,255,0);tira.setPixelColor(Pantalla[ 217],0,255,0);tira.setPixelColor(Pantalla[ 219],0,255,0);tira.setPixelColor(Pantalla[ 222],0,255,0);
tira.setPixelColor(Pantalla[ 238],0,255,0);tira.setPixelColor(Pantalla[ 237],0,255,0);tira.setPixelColor(Pantalla[ 236],0,255,0);tira.setPixelColor(Pantalla[ 235],0,255,0);tira.setPixelColor(Pantalla[ 233],0,255,0);tira.setPixelColor(Pantalla[ 232],0,255,0);tira.setPixelColor(Pantalla[ 231],0,255,0);tira.setPixelColor(Pantalla[ 230],0,255,0);tira.setPixelColor(Pantalla[ 228],0,255,0);tira.setPixelColor(Pantalla[ 227],0,255,0);tira.setPixelColor(Pantalla[ 226],0,255,0);tira.setPixelColor(Pantalla[ 225],0,255,0);

}




void Matriz_Green()
{
  tira.setPixelColor(Pantalla[0], 255, 0, 0);
  tira.setPixelColor(Pantalla[1], 255, 0, 0);
  tira.setPixelColor(Pantalla[2], 255, 0, 0);
  tira.setPixelColor(Pantalla[3], 255, 0, 0);
  tira.setPixelColor(Pantalla[4], 255, 0, 0);
  tira.setPixelColor(Pantalla[5], 255, 0, 0);
  tira.setPixelColor(Pantalla[6], 255, 0, 0);
  tira.setPixelColor(Pantalla[7], 255, 0, 0);
  tira.setPixelColor(Pantalla[8], 255, 0, 0);
  tira.setPixelColor(Pantalla[9], 255, 0, 0);
  tira.setPixelColor(Pantalla[10], 255, 0, 0);
  tira.setPixelColor(Pantalla[11], 255, 0, 0);
  tira.setPixelColor(Pantalla[12], 255, 0, 0);
  tira.setPixelColor(Pantalla[13], 255, 0, 0);
  tira.setPixelColor(Pantalla[14], 255, 0, 0);
  tira.setPixelColor(Pantalla[15], 255, 0, 0);
  tira.setPixelColor(Pantalla[31], 255, 0, 0);
  tira.setPixelColor(Pantalla[30], 255, 0, 0);
  tira.setPixelColor(Pantalla[29], 255, 0, 0);
  tira.setPixelColor(Pantalla[28], 255, 0, 0);
  tira.setPixelColor(Pantalla[27], 255, 0, 0);
  tira.setPixelColor(Pantalla[26], 255, 0, 0);
  tira.setPixelColor(Pantalla[25], 255, 0, 0);
  tira.setPixelColor(Pantalla[24], 255, 0, 0);
  tira.setPixelColor(Pantalla[23], 255, 0, 0);
  tira.setPixelColor(Pantalla[22], 255, 0, 0);
  tira.setPixelColor(Pantalla[21], 255, 0, 0);
  tira.setPixelColor(Pantalla[20], 255, 0, 0);
  tira.setPixelColor(Pantalla[19], 255, 0, 0);
  tira.setPixelColor(Pantalla[18], 255, 0, 0);
  tira.setPixelColor(Pantalla[17], 255, 0, 0);
  tira.setPixelColor(Pantalla[16], 255, 0, 0);
  tira.setPixelColor(Pantalla[223], 255, 0, 0);
  tira.setPixelColor(Pantalla[222], 255, 0, 0);
  tira.setPixelColor(Pantalla[221], 255, 0, 0);
  tira.setPixelColor(Pantalla[220], 255, 0, 0);
  tira.setPixelColor(Pantalla[219], 255, 0, 0);
  tira.setPixelColor(Pantalla[218], 255, 0, 0);
  tira.setPixelColor(Pantalla[217], 255, 0, 0);
  tira.setPixelColor(Pantalla[216], 255, 0, 0);
  tira.setPixelColor(Pantalla[215], 255, 0, 0);
  tira.setPixelColor(Pantalla[214], 255, 0, 0);
  tira.setPixelColor(Pantalla[213], 255, 0, 0);
  tira.setPixelColor(Pantalla[212], 255, 0, 0);
  tira.setPixelColor(Pantalla[211], 255, 0, 0);
  tira.setPixelColor(Pantalla[210], 255, 0, 0);
  tira.setPixelColor(Pantalla[209], 255, 0, 0);
  tira.setPixelColor(Pantalla[208], 255, 0, 0);
  tira.setPixelColor(Pantalla[224], 255, 0, 0);
  tira.setPixelColor(Pantalla[225], 255, 0, 0);
  tira.setPixelColor(Pantalla[226], 255, 0, 0);
  tira.setPixelColor(Pantalla[227], 255, 0, 0);
  tira.setPixelColor(Pantalla[228], 255, 0, 0);
  tira.setPixelColor(Pantalla[229], 255, 0, 0);
  tira.setPixelColor(Pantalla[230], 255, 0, 0);
  tira.setPixelColor(Pantalla[231], 255, 0, 0);
  tira.setPixelColor(Pantalla[232], 255, 0, 0);
  tira.setPixelColor(Pantalla[233], 255, 0, 0);
  tira.setPixelColor(Pantalla[234], 255, 0, 0);
  tira.setPixelColor(Pantalla[235], 255, 0, 0);
  tira.setPixelColor(Pantalla[236], 255, 0, 0);
  tira.setPixelColor(Pantalla[237], 255, 0, 0);
  tira.setPixelColor(Pantalla[238], 255, 0, 0);
  tira.setPixelColor(Pantalla[239], 255, 0, 0);
  tira.setPixelColor(Pantalla[255], 255, 0, 0);
  tira.setPixelColor(Pantalla[254], 255, 0, 0);
  tira.setPixelColor(Pantalla[253], 255, 0, 0);
  tira.setPixelColor(Pantalla[252], 255, 0, 0);
  tira.setPixelColor(Pantalla[251], 255, 0, 0);
  tira.setPixelColor(Pantalla[250], 255, 0, 0);
  tira.setPixelColor(Pantalla[249], 255, 0, 0);
  tira.setPixelColor(Pantalla[248], 255, 0, 0);
  tira.setPixelColor(Pantalla[247], 255, 0, 0);
  tira.setPixelColor(Pantalla[246], 255, 0, 0);
  tira.setPixelColor(Pantalla[245], 255, 0, 0);
  tira.setPixelColor(Pantalla[244], 255, 0, 0);
  tira.setPixelColor(Pantalla[243], 255, 0, 0);
  tira.setPixelColor(Pantalla[242], 255, 0, 0);
  tira.setPixelColor(Pantalla[241], 255, 0, 0);
  tira.setPixelColor(Pantalla[240], 255, 0, 0);
}

void Matriz_Blue()
{
  tira.setPixelColor(Pantalla[0], 0, 0, 255);
  tira.setPixelColor(Pantalla[1], 0, 0, 255);
  tira.setPixelColor(Pantalla[2], 0, 0, 255);
  tira.setPixelColor(Pantalla[3], 0, 0, 255);
  tira.setPixelColor(Pantalla[4], 0, 0, 255);
  tira.setPixelColor(Pantalla[5], 0, 0, 255);
  tira.setPixelColor(Pantalla[6], 0, 0, 255);
  tira.setPixelColor(Pantalla[7], 0, 0, 255);
  tira.setPixelColor(Pantalla[8], 0, 0, 255);
  tira.setPixelColor(Pantalla[9], 0, 0, 255);
  tira.setPixelColor(Pantalla[10], 0, 0, 255);
  tira.setPixelColor(Pantalla[11], 0, 0, 255);
  tira.setPixelColor(Pantalla[12], 0, 0, 255);
  tira.setPixelColor(Pantalla[13], 0, 0, 255);
  tira.setPixelColor(Pantalla[14], 0, 0, 255);
  tira.setPixelColor(Pantalla[15], 0, 0, 255);
  tira.setPixelColor(Pantalla[31], 0, 0, 255);
  tira.setPixelColor(Pantalla[30], 0, 0, 255);
  tira.setPixelColor(Pantalla[29], 0, 0, 255);
  tira.setPixelColor(Pantalla[28], 0, 0, 255);
  tira.setPixelColor(Pantalla[27], 0, 0, 255);
  tira.setPixelColor(Pantalla[26], 0, 0, 255);
  tira.setPixelColor(Pantalla[25], 0, 0, 255);
  tira.setPixelColor(Pantalla[24], 0, 0, 255);
  tira.setPixelColor(Pantalla[23], 0, 0, 255);
  tira.setPixelColor(Pantalla[22], 0, 0, 255);
  tira.setPixelColor(Pantalla[21], 0, 0, 255);
  tira.setPixelColor(Pantalla[20], 0, 0, 255);
  tira.setPixelColor(Pantalla[19], 0, 0, 255);
  tira.setPixelColor(Pantalla[18], 0, 0, 255);
  tira.setPixelColor(Pantalla[17], 0, 0, 255);
  tira.setPixelColor(Pantalla[16], 0, 0, 255);
  tira.setPixelColor(Pantalla[223], 0, 0, 255);
  tira.setPixelColor(Pantalla[222], 0, 0, 255);
  tira.setPixelColor(Pantalla[221], 0, 0, 255);
  tira.setPixelColor(Pantalla[220], 0, 0, 255);
  tira.setPixelColor(Pantalla[219], 0, 0, 255);
  tira.setPixelColor(Pantalla[218], 0, 0, 255);
  tira.setPixelColor(Pantalla[217], 0, 0, 255);
  tira.setPixelColor(Pantalla[216], 0, 0, 255);
  tira.setPixelColor(Pantalla[215], 0, 0, 255);
  tira.setPixelColor(Pantalla[214], 0, 0, 255);
  tira.setPixelColor(Pantalla[213], 0, 0, 255);
  tira.setPixelColor(Pantalla[212], 0, 0, 255);
  tira.setPixelColor(Pantalla[211], 0, 0, 255);
  tira.setPixelColor(Pantalla[210], 0, 0, 255);
  tira.setPixelColor(Pantalla[209], 0, 0, 255);
  tira.setPixelColor(Pantalla[208], 0, 0, 255);
  tira.setPixelColor(Pantalla[224], 0, 0, 255);
  tira.setPixelColor(Pantalla[225], 0, 0, 255);
  tira.setPixelColor(Pantalla[226], 0, 0, 255);
  tira.setPixelColor(Pantalla[227], 0, 0, 255);
  tira.setPixelColor(Pantalla[228], 0, 0, 255);
  tira.setPixelColor(Pantalla[229], 0, 0, 255);
  tira.setPixelColor(Pantalla[230], 0, 0, 255);
  tira.setPixelColor(Pantalla[231], 0, 0, 255);
  tira.setPixelColor(Pantalla[232], 0, 0, 255);
  tira.setPixelColor(Pantalla[233], 0, 0, 255);
  tira.setPixelColor(Pantalla[234], 0, 0, 255);
  tira.setPixelColor(Pantalla[235], 0, 0, 255);
  tira.setPixelColor(Pantalla[236], 0, 0, 255);
  tira.setPixelColor(Pantalla[237], 0, 0, 255);
  tira.setPixelColor(Pantalla[238], 0, 0, 255);
  tira.setPixelColor(Pantalla[239], 0, 0, 255);
  tira.setPixelColor(Pantalla[255], 0, 0, 255);
  tira.setPixelColor(Pantalla[254], 0, 0, 255);
  tira.setPixelColor(Pantalla[253], 0, 0, 255);
  tira.setPixelColor(Pantalla[252], 0, 0, 255);
  tira.setPixelColor(Pantalla[251], 0, 0, 255);
  tira.setPixelColor(Pantalla[250], 0, 0, 255);
  tira.setPixelColor(Pantalla[249], 0, 0, 255);
  tira.setPixelColor(Pantalla[248], 0, 0, 255);
  tira.setPixelColor(Pantalla[247], 0, 0, 255);
  tira.setPixelColor(Pantalla[246], 0, 0, 255);
  tira.setPixelColor(Pantalla[245], 0, 0, 255);
  tira.setPixelColor(Pantalla[244], 0, 0, 255);
  tira.setPixelColor(Pantalla[243], 0, 0, 255);
  tira.setPixelColor(Pantalla[242], 0, 0, 255);
  tira.setPixelColor(Pantalla[241], 0, 0, 255);
  tira.setPixelColor(Pantalla[240], 0, 0, 255);
}



void Matriz_Limpia_Secuencias()
{
  tira.setPixelColor(Pantalla[224], 0, 0, 0);
  tira.setPixelColor(Pantalla[225], 0, 0, 0);
  tira.setPixelColor(Pantalla[226], 0, 0, 0);
  tira.setPixelColor(Pantalla[227], 0, 0, 0);
  tira.setPixelColor(Pantalla[228], 0, 0, 0);
  tira.setPixelColor(Pantalla[229], 0, 0, 0);
  tira.setPixelColor(Pantalla[231], 255, 255, 0);
  tira.setPixelColor(Pantalla[232], 255, 255, 0);
  tira.setPixelColor(Pantalla[234], 0, 0, 0);
  tira.setPixelColor(Pantalla[235], 0, 0, 0);
  tira.setPixelColor(Pantalla[236], 0, 0, 0);
  tira.setPixelColor(Pantalla[237], 0, 0, 0);
  tira.setPixelColor(Pantalla[238], 0, 0, 0);
  tira.setPixelColor(Pantalla[239], 0, 0, 0);
  tira.setPixelColor(Pantalla[255], 0, 0, 0);
  tira.setPixelColor(Pantalla[254], 0, 0, 0);
  tira.setPixelColor(Pantalla[253], 0, 0, 0);
  tira.setPixelColor(Pantalla[252], 0, 0, 0);
  tira.setPixelColor(Pantalla[251], 0, 0, 0);
  tira.setPixelColor(Pantalla[250], 0, 0, 0);
  tira.setPixelColor(Pantalla[248], 255, 255, 0);
  tira.setPixelColor(Pantalla[247], 255, 255, 0);
  tira.setPixelColor(Pantalla[245], 0, 0, 0);
  tira.setPixelColor(Pantalla[244], 0, 0, 0);
  tira.setPixelColor(Pantalla[243], 0, 0, 0);
  tira.setPixelColor(Pantalla[242], 0, 0, 0);
  tira.setPixelColor(Pantalla[241], 0, 0, 0);
  tira.setPixelColor(Pantalla[240], 0, 0, 0);
}

void Marco_Intermitente1(){
  tira.setPixelColor(Pantalla[ 15],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 14],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 13],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 12],0,0,0);tira.setPixelColor(Pantalla[ 11],0,0,0);tira.setPixelColor(Pantalla[ 10],0,0,0);tira.setPixelColor(Pantalla[ 9],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 8],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 7],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 6],0,0,0);tira.setPixelColor(Pantalla[ 5],0,0,0);tira.setPixelColor(Pantalla[ 4],0,0,0);tira.setPixelColor(Pantalla[ 3],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 2],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 1],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 0],0,0,0);
tira.setPixelColor(Pantalla[ 16],0,0,0);tira.setPixelColor(Pantalla[ 31],0,0,0);
tira.setPixelColor(Pantalla[ 47],0,0,0);tira.setPixelColor(Pantalla[ 32],0,0,0);
tira.setPixelColor(Pantalla[ 48],0,0,0);tira.setPixelColor(Pantalla[ 63],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 79],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 64],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 80],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 95],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 111],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 96],0,0,0);
tira.setPixelColor(Pantalla[ 112],0,0,0);tira.setPixelColor(Pantalla[ 127],0,0,0);
tira.setPixelColor(Pantalla[ 143],0,0,0);tira.setPixelColor(Pantalla[ 128],0,0,0);
tira.setPixelColor(Pantalla[ 144],0,0,0);tira.setPixelColor(Pantalla[ 159],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 175],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 160],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 176],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 191],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 207],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 192],0,0,0);
tira.setPixelColor(Pantalla[ 208],0,0,0);tira.setPixelColor(Pantalla[ 223],0,0,0);
tira.setPixelColor(Pantalla[ 239],0,0,0);tira.setPixelColor(Pantalla[ 224],0,0,0);
tira.setPixelColor(Pantalla[ 240],0,0,0);tira.setPixelColor(Pantalla[ 241],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 242],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 243],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 244],0,0,0);tira.setPixelColor(Pantalla[ 245],0,0,0);tira.setPixelColor(Pantalla[ 246],0,0,0);tira.setPixelColor(Pantalla[ 247],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 248],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 249],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 250],0,0,0);tira.setPixelColor(Pantalla[ 251],0,0,0);tira.setPixelColor(Pantalla[ 252],0,0,0);tira.setPixelColor(Pantalla[ 253],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 254],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 255],pixel_rojo,pixel_verde,pixel_azul);

}


void Marco_Intermitente2(){
  tira.setPixelColor(Pantalla[ 15],0,0,0);tira.setPixelColor(Pantalla[ 14],0,0,0);tira.setPixelColor(Pantalla[ 13],0,0,0);tira.setPixelColor(Pantalla[ 12],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 11],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 10],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 9],0,0,0);tira.setPixelColor(Pantalla[ 8],0,0,0);tira.setPixelColor(Pantalla[ 7],0,0,0);tira.setPixelColor(Pantalla[ 6],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 5],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 4],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 3],0,0,0);tira.setPixelColor(Pantalla[ 2],0,0,0);tira.setPixelColor(Pantalla[ 1],0,0,0);tira.setPixelColor(Pantalla[ 0],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 16],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 31],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 47],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 32],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 48],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 63],0,0,0);
tira.setPixelColor(Pantalla[ 79],0,0,0);tira.setPixelColor(Pantalla[ 64],0,0,0);
tira.setPixelColor(Pantalla[ 80],0,0,0);tira.setPixelColor(Pantalla[ 95],0,0,0);
tira.setPixelColor(Pantalla[ 111],0,0,0);tira.setPixelColor(Pantalla[ 96],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 112],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 127],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 143],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 128],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 144],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 159],0,0,0);
tira.setPixelColor(Pantalla[ 175],0,0,0);tira.setPixelColor(Pantalla[ 160],0,0,0);
tira.setPixelColor(Pantalla[ 176],0,0,0);tira.setPixelColor(Pantalla[ 191],0,0,0);
tira.setPixelColor(Pantalla[ 207],0,0,0);tira.setPixelColor(Pantalla[ 192],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 208],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 223],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 239],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 224],pixel_rojo,pixel_verde,pixel_azul);
tira.setPixelColor(Pantalla[ 240],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 241],0,0,0);tira.setPixelColor(Pantalla[ 242],0,0,0);tira.setPixelColor(Pantalla[ 243],0,0,0);tira.setPixelColor(Pantalla[ 244],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 245],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 246],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 247],0,0,0);tira.setPixelColor(Pantalla[ 248],0,0,0);tira.setPixelColor(Pantalla[ 249],0,0,0);tira.setPixelColor(Pantalla[ 250],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 251],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 252],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 253],0,0,0);tira.setPixelColor(Pantalla[ 254],0,0,0);tira.setPixelColor(Pantalla[ 255],0,0,0);

}


void Matriz_Aro_1_Apuntado() //Ver 4.0
{
tira.setPixelColor(Pantalla[ 240],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 241],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 242],0,0,0);tira.setPixelColor(Pantalla[ 243],0,0,0);tira.setPixelColor(Pantalla[ 244],0,0,0);tira.setPixelColor(Pantalla[ 245],0,0,0);tira.setPixelColor(Pantalla[ 246],0,0,0);tira.setPixelColor(Pantalla[ 247],0,0,0);tira.setPixelColor(Pantalla[ 248],0,0,0);tira.setPixelColor(Pantalla[ 249],0,0,0);tira.setPixelColor(Pantalla[ 250],0,0,0);tira.setPixelColor(Pantalla[ 251],0,0,0);tira.setPixelColor(Pantalla[ 252],0,0,0);tira.setPixelColor(Pantalla[ 253],0,0,0);tira.setPixelColor(Pantalla[ 254],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 255],pixel_rojo,pixel_verde,pixel_azul);

}

void Matriz_Aro_2_Apuntado() //Ver 4.0
{
tira.setPixelColor(Pantalla[ 240],0,0,0);tira.setPixelColor(Pantalla[ 241],0,0,0);tira.setPixelColor(Pantalla[ 242],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 243],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 244],0,0,0);tira.setPixelColor(Pantalla[ 245],0,0,0);tira.setPixelColor(Pantalla[ 246],0,0,0);tira.setPixelColor(Pantalla[ 247],0,0,0);tira.setPixelColor(Pantalla[ 248],0,0,0);tira.setPixelColor(Pantalla[ 249],0,0,0);tira.setPixelColor(Pantalla[ 250],0,0,0);tira.setPixelColor(Pantalla[ 251],0,0,0);tira.setPixelColor(Pantalla[ 252],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 253],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 254],0,0,0);tira.setPixelColor(Pantalla[ 255],0,0,0);

 }

void Matriz_Aro_3_Apuntado() //Ver 4.0
{
tira.setPixelColor(Pantalla[ 240],0,0,0);tira.setPixelColor(Pantalla[ 241],0,0,0);tira.setPixelColor(Pantalla[ 242],0,0,0);tira.setPixelColor(Pantalla[ 243],0,0,0);tira.setPixelColor(Pantalla[ 244],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 245],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 246],0,0,0);tira.setPixelColor(Pantalla[ 247],0,0,0);tira.setPixelColor(Pantalla[ 248],0,0,0);tira.setPixelColor(Pantalla[ 249],0,0,0);tira.setPixelColor(Pantalla[ 250],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 251],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 252],0,0,0);tira.setPixelColor(Pantalla[ 253],0,0,0);tira.setPixelColor(Pantalla[ 254],0,0,0);tira.setPixelColor(Pantalla[ 255],0,0,0);
 }

void Matriz_Aro_4_Apuntado() //Ver 4.0
{
  tira.setPixelColor(Pantalla[ 240],0,0,0);tira.setPixelColor(Pantalla[ 241],0,0,0);tira.setPixelColor(Pantalla[ 242],0,0,0);tira.setPixelColor(Pantalla[ 243],0,0,0);tira.setPixelColor(Pantalla[ 244],0,0,0);tira.setPixelColor(Pantalla[ 245],0,0,0);tira.setPixelColor(Pantalla[ 246],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 247],100,100,100);tira.setPixelColor(Pantalla[ 248],100,100,100);tira.setPixelColor(Pantalla[ 249],pixel_rojo,pixel_verde,pixel_azul);tira.setPixelColor(Pantalla[ 250],0,0,0);tira.setPixelColor(Pantalla[ 251],0,0,0);tira.setPixelColor(Pantalla[ 252],0,0,0);tira.setPixelColor(Pantalla[ 253],0,0,0);tira.setPixelColor(Pantalla[ 254],0,0,0);tira.setPixelColor(Pantalla[ 255],0,0,0);

 }

void Muestra_Datos_Programador() //Ver 4.0
{
  // Serial.println("datos_recibidos.no_paquete_test ="+String(datos_recibidos.n));
  // Serial.println("datos_recibidos.tipo_tiro =" + String(datos_recibidos.t));
  // Serial.println("datos_recibidos.color ="+String(datos_recibidos.c));
  // Serial.println("datos_recibidos.diana ="+String(datos_recibidos.d));
  // Serial.println("datos_recibidos.propietario ="+String(datos_recibidos.p));
  // Serial.println("datos_recibidos.tiempo ="+String(datos_recibidos.t));
  // Serial.println("datos_recibidos.figura ="+String(datos_recibidos.f));
  // Serial.println("datos_recibidos.shot ="+String(datos_recibidos.s));
}
/*------------------------------------------------------------------------------*/

void Matriz_Calibra_Publicidad()
{ // se pone para no afectar el valor de los pixeles de color
  // se definen como valores
  int pixel_rojo = 200;
  int pixel_verde = 0;
  int pixel_azul = 0;
  if (lado == IZQUIERDA)
  {

    tira.setPixelColor(Pantalla[16], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[17], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[18], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[20], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[21], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[22], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[24], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[25], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[26], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[28], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[29], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[30], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[46], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[43], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[39], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[34], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[49], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[52], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[53], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[54], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[56], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[57], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[58], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[61], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[78], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[75], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[69], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[66], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[81], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[84], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[85], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[86], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[88], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[89], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[90], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[93], pixel_rojo, pixel_verde, pixel_azul);

    tira.setPixelColor(Pantalla[118], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[137], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[150], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[169], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[184], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[202], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[201], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[200], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[214], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));

  }
  else

  {
    tira.setPixelColor(Pantalla[16], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[17], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[18], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[20], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[21], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[22], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[24], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[25], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[26], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[28], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[29], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[30], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[46], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[43], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[39], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[34], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[49], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[52], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[53], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[54], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[56], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[57], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[58], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[61], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[78], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[75], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[69], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[66], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[81], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[84], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[85], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[86], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[88], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[89], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[90], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[93], pixel_rojo, pixel_verde, pixel_azul);

    tira.setPixelColor(Pantalla[121], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[134], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[153], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[166], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[183], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[185], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[187], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[199], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[198], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[197], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[217], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);

  //   tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
  //   tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
  //   tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
  //   tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
  //   tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
 }
}
/*------------------------------------------------------------------------------*/

void Matriz_Calibra() //Ver 4.0
{

  if (lado == IZQUIERDA)
  {

    tira.setPixelColor(Pantalla[16], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[17], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[18], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[20], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[21], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[22], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[24], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[25], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[26], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[28], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[29], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[30], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[46], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[43], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[39], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[34], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[49], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[52], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[53], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[54], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[56], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[57], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[58], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[61], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[78], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[75], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[69], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[66], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[81], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[84], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[85], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[86], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[88], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[89], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[90], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[93], pixel_rojo, pixel_verde, pixel_azul);

    tira.setPixelColor(Pantalla[118], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[137], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[150], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[169], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[184], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[202], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[201], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[200], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[214], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));

    // tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
    // tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    // tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    // tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    // tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
    // tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
  }
  else

  {
    tira.setPixelColor(Pantalla[16], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[17], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[18], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[20], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[21], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[22], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[24], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[25], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[26], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[28], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[29], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[30], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[46], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[43], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[39], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[34], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[49], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[52], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[53], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[54], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[56], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[57], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[58], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[61], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[78], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[75], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[69], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[66], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[81], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[84], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[85], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[86], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[88], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[89], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[90], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[93], pixel_rojo, pixel_verde, pixel_azul);

    tira.setPixelColor(Pantalla[121], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[134], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[153], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[166], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[183], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[185], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[187], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[199], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[198], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[197], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[217], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    
    // tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
    // tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    // tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    // tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    // tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
    // tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
  }
}
/*------------------------------------------------------------------------------*/

void Matriz_Pinguino() //Ver 4.0
{
  if (lado == IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[3], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[19], 0, 0, 255);
    tira.setPixelColor(Pantalla[20], 0, 0, 255);
    tira.setPixelColor(Pantalla[21], 0, 0, 255);
    tira.setPixelColor(Pantalla[22], 0, 0, 255);
    tira.setPixelColor(Pantalla[23], 0, 0, 255);
    tira.setPixelColor(Pantalla[24], 0, 0, 255);
    tira.setPixelColor(Pantalla[25], 0, 0, 255);
    tira.setPixelColor(Pantalla[26], 0, 0, 255);
    tira.setPixelColor(Pantalla[27], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[28], 105, 210, 30);
    tira.setPixelColor(Pantalla[29], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[44], 0, 0, 255);
    tira.setPixelColor(Pantalla[43], 0, 0, 255);
    tira.setPixelColor(Pantalla[42], 100, 100, 100);
    tira.setPixelColor(Pantalla[41], 0, 0, 255);
    tira.setPixelColor(Pantalla[40], 0, 0, 255);
    tira.setPixelColor(Pantalla[39], 0, 0, 255);
    tira.setPixelColor(Pantalla[38], 0, 0, 255);
    tira.setPixelColor(Pantalla[37], 100, 100, 100);
    tira.setPixelColor(Pantalla[36], 0, 0, 255);
    tira.setPixelColor(Pantalla[35], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[51], 0, 0, 255);
    tira.setPixelColor(Pantalla[52], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], 100, 100, 100);
    tira.setPixelColor(Pantalla[54], 100, 100, 100);
    tira.setPixelColor(Pantalla[55], 0, 0, 255);
    tira.setPixelColor(Pantalla[56], 0, 0, 255);
    tira.setPixelColor(Pantalla[57], 100, 100, 100);
    tira.setPixelColor(Pantalla[58], 100, 100, 100);
    tira.setPixelColor(Pantalla[59], 100, 100, 100);
    tira.setPixelColor(Pantalla[60], 0, 0, 255);
    tira.setPixelColor(Pantalla[76], 0, 0, 255);
    tira.setPixelColor(Pantalla[75], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], 100, 100, 100);
    tira.setPixelColor(Pantalla[72], 0, 0, 255);
    tira.setPixelColor(Pantalla[71], 0, 0, 255);
    tira.setPixelColor(Pantalla[70], 100, 100, 100);
    tira.setPixelColor(Pantalla[68], 100, 100, 100);
    tira.setPixelColor(Pantalla[67], 0, 0, 255);
    tira.setPixelColor(Pantalla[83], 0, 0, 255);
    tira.setPixelColor(Pantalla[84], 100, 100, 100);
    tira.setPixelColor(Pantalla[86], 100, 100, 100);
    tira.setPixelColor(Pantalla[87], 0, 0, 255);
    tira.setPixelColor(Pantalla[88], 0, 0, 255);
    tira.setPixelColor(Pantalla[89], 100, 100, 100);
    tira.setPixelColor(Pantalla[91], 100, 100, 100);
    tira.setPixelColor(Pantalla[92], 0, 0, 255);
    tira.setPixelColor(Pantalla[108], 0, 0, 255);
    tira.setPixelColor(Pantalla[107], 100, 100, 100);
    tira.setPixelColor(Pantalla[106], 100, 100, 100);
    tira.setPixelColor(Pantalla[105], 0, 255, 0);
    tira.setPixelColor(Pantalla[104], 0, 255, 0);
    tira.setPixelColor(Pantalla[103], 0, 255, 0);
    tira.setPixelColor(Pantalla[102], 0, 255, 0);
    tira.setPixelColor(Pantalla[101], 100, 100, 100);
    tira.setPixelColor(Pantalla[100], 100, 100, 100);
    tira.setPixelColor(Pantalla[99], 0, 0, 255);
    tira.setPixelColor(Pantalla[116], 0, 0, 255);
    tira.setPixelColor(Pantalla[117], 100, 100, 100);
    tira.setPixelColor(Pantalla[118], 100, 100, 100);
    tira.setPixelColor(Pantalla[119], 0, 255, 0);
    tira.setPixelColor(Pantalla[120], 0, 255, 0);
    tira.setPixelColor(Pantalla[121], 100, 100, 100);
    tira.setPixelColor(Pantalla[122], 100, 100, 100);
    tira.setPixelColor(Pantalla[123], 0, 0, 255);
    tira.setPixelColor(Pantalla[140], 0, 0, 255);
    tira.setPixelColor(Pantalla[139], 0, 0, 255);
    tira.setPixelColor(Pantalla[138], 100, 100, 100);
    tira.setPixelColor(Pantalla[137], 100, 100, 100);
    tira.setPixelColor(Pantalla[136], 100, 100, 100);
    tira.setPixelColor(Pantalla[135], 100, 100, 100);
    tira.setPixelColor(Pantalla[134], 100, 100, 100);
    tira.setPixelColor(Pantalla[133], 100, 100, 100);
    tira.setPixelColor(Pantalla[132], 0, 0, 255);
    tira.setPixelColor(Pantalla[131], 0, 0, 255);
    tira.setPixelColor(Pantalla[147], 0, 0, 255);
    tira.setPixelColor(Pantalla[148], 0, 0, 255);
    tira.setPixelColor(Pantalla[149], 100, 100, 100);
    tira.setPixelColor(Pantalla[150], 100, 100, 100);
    tira.setPixelColor(Pantalla[151], 100, 100, 100);
    tira.setPixelColor(Pantalla[152], 100, 100, 100);
    tira.setPixelColor(Pantalla[153], 100, 100, 100);
    tira.setPixelColor(Pantalla[154], 100, 100, 100);
    tira.setPixelColor(Pantalla[155], 0, 0, 255);
    tira.setPixelColor(Pantalla[156], 0, 0, 255);
    tira.setPixelColor(Pantalla[172], 0, 0, 255);
    tira.setPixelColor(Pantalla[171], 0, 0, 255);
    tira.setPixelColor(Pantalla[170], 100, 100, 100);
    tira.setPixelColor(Pantalla[169], 100, 100, 100);
    tira.setPixelColor(Pantalla[168], 100, 100, 100);
    tira.setPixelColor(Pantalla[167], 100, 100, 100);
    tira.setPixelColor(Pantalla[166], 100, 100, 100);
    tira.setPixelColor(Pantalla[165], 100, 100, 100);
    tira.setPixelColor(Pantalla[164], 0, 0, 255);
    tira.setPixelColor(Pantalla[163], 0, 0, 255);
    tira.setPixelColor(Pantalla[179], 0, 0, 255);
    tira.setPixelColor(Pantalla[181], 0, 0, 255);
    tira.setPixelColor(Pantalla[182], 100, 100, 100);
    tira.setPixelColor(Pantalla[183], 100, 100, 100);
    tira.setPixelColor(Pantalla[184], 100, 100, 100);
    tira.setPixelColor(Pantalla[185], 100, 100, 100);
    tira.setPixelColor(Pantalla[186], 0, 0, 255);
    tira.setPixelColor(Pantalla[188], 0, 0, 255);
    tira.setPixelColor(Pantalla[202], 0, 0, 255);
    tira.setPixelColor(Pantalla[201], 100, 100, 100);
    tira.setPixelColor(Pantalla[200], 100, 100, 100);
    tira.setPixelColor(Pantalla[199], 100, 100, 100);
    tira.setPixelColor(Pantalla[198], 100, 100, 100);
    tira.setPixelColor(Pantalla[197], 0, 0, 255);
    tira.setPixelColor(Pantalla[213], 0, 255, 0);
    tira.setPixelColor(Pantalla[214], 0, 255, 0);
    tira.setPixelColor(Pantalla[215], 0, 0, 255);
    tira.setPixelColor(Pantalla[216], 0, 0, 255);
    tira.setPixelColor(Pantalla[217], 0, 255, 0);
    tira.setPixelColor(Pantalla[218], 0, 255, 0);

    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
  }
  else
  {
    tira.setPixelColor(Pantalla[3], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[19], 0, 0, 255);
    tira.setPixelColor(Pantalla[20], 0, 0, 255);
    tira.setPixelColor(Pantalla[21], 0, 0, 255);
    tira.setPixelColor(Pantalla[22], 0, 0, 255);
    tira.setPixelColor(Pantalla[23], 0, 0, 255);
    tira.setPixelColor(Pantalla[24], 0, 0, 255);
    tira.setPixelColor(Pantalla[25], 0, 0, 255);
    tira.setPixelColor(Pantalla[26], 0, 0, 255);
    tira.setPixelColor(Pantalla[27], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[28], 105, 210, 30);
    tira.setPixelColor(Pantalla[29], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[44], 0, 0, 255);
    tira.setPixelColor(Pantalla[43], 0, 0, 255);
    tira.setPixelColor(Pantalla[42], 100, 100, 100);
    tira.setPixelColor(Pantalla[41], 0, 0, 255);
    tira.setPixelColor(Pantalla[40], 0, 0, 255);
    tira.setPixelColor(Pantalla[39], 0, 0, 255);
    tira.setPixelColor(Pantalla[38], 0, 0, 255);
    tira.setPixelColor(Pantalla[37], 100, 100, 100);
    tira.setPixelColor(Pantalla[36], 0, 0, 255);
    tira.setPixelColor(Pantalla[35], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[51], 0, 0, 255);
    tira.setPixelColor(Pantalla[52], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], 100, 100, 100);
    tira.setPixelColor(Pantalla[54], 100, 100, 100);
    tira.setPixelColor(Pantalla[55], 0, 0, 255);
    tira.setPixelColor(Pantalla[56], 0, 0, 255);
    tira.setPixelColor(Pantalla[57], 100, 100, 100);
    tira.setPixelColor(Pantalla[58], 100, 100, 100);
    tira.setPixelColor(Pantalla[59], 100, 100, 100);
    tira.setPixelColor(Pantalla[60], 0, 0, 255);
    tira.setPixelColor(Pantalla[76], 0, 0, 255);
    tira.setPixelColor(Pantalla[75], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], 100, 100, 100);
    tira.setPixelColor(Pantalla[72], 0, 0, 255);
    tira.setPixelColor(Pantalla[71], 0, 0, 255);
    tira.setPixelColor(Pantalla[70], 100, 100, 100);
    tira.setPixelColor(Pantalla[68], 100, 100, 100);
    tira.setPixelColor(Pantalla[67], 0, 0, 255);
    tira.setPixelColor(Pantalla[83], 0, 0, 255);
    tira.setPixelColor(Pantalla[84], 100, 100, 100);
    tira.setPixelColor(Pantalla[86], 100, 100, 100);
    tira.setPixelColor(Pantalla[87], 0, 0, 255);
    tira.setPixelColor(Pantalla[88], 0, 0, 255);
    tira.setPixelColor(Pantalla[89], 100, 100, 100);
    tira.setPixelColor(Pantalla[91], 100, 100, 100);
    tira.setPixelColor(Pantalla[92], 0, 0, 255);
    tira.setPixelColor(Pantalla[108], 0, 0, 255);
    tira.setPixelColor(Pantalla[107], 100, 100, 100);
    tira.setPixelColor(Pantalla[106], 100, 100, 100);
    tira.setPixelColor(Pantalla[105], 0, 255, 0);
    tira.setPixelColor(Pantalla[104], 0, 255, 0);
    tira.setPixelColor(Pantalla[103], 0, 255, 0);
    tira.setPixelColor(Pantalla[102], 0, 255, 0);
    tira.setPixelColor(Pantalla[101], 100, 100, 100);
    tira.setPixelColor(Pantalla[100], 100, 100, 100);
    tira.setPixelColor(Pantalla[99], 0, 0, 255);
    tira.setPixelColor(Pantalla[116], 0, 0, 255);
    tira.setPixelColor(Pantalla[117], 100, 100, 100);
    tira.setPixelColor(Pantalla[118], 100, 100, 100);
    tira.setPixelColor(Pantalla[119], 0, 255, 0);
    tira.setPixelColor(Pantalla[120], 0, 255, 0);
    tira.setPixelColor(Pantalla[121], 100, 100, 100);
    tira.setPixelColor(Pantalla[122], 100, 100, 100);
    tira.setPixelColor(Pantalla[123], 0, 0, 255);
    tira.setPixelColor(Pantalla[141], 0, 0, 255);
    tira.setPixelColor(Pantalla[140], 0, 0, 255);
    tira.setPixelColor(Pantalla[139], 0, 0, 255);
    tira.setPixelColor(Pantalla[138], 100, 100, 100);
    tira.setPixelColor(Pantalla[137], 100, 100, 100);
    tira.setPixelColor(Pantalla[136], 100, 100, 100);
    tira.setPixelColor(Pantalla[135], 100, 100, 100);
    tira.setPixelColor(Pantalla[134], 100, 100, 100);
    tira.setPixelColor(Pantalla[133], 100, 100, 100);
    tira.setPixelColor(Pantalla[132], 0, 0, 255);
    tira.setPixelColor(Pantalla[131], 0, 0, 255);
    tira.setPixelColor(Pantalla[130], 0, 0, 255);
    tira.setPixelColor(Pantalla[146], 0, 0, 255);
    tira.setPixelColor(Pantalla[148], 0, 0, 255);
    tira.setPixelColor(Pantalla[149], 100, 100, 100);
    tira.setPixelColor(Pantalla[150], 100, 100, 100);
    tira.setPixelColor(Pantalla[151], 100, 100, 100);
    tira.setPixelColor(Pantalla[152], 100, 100, 100);
    tira.setPixelColor(Pantalla[153], 100, 100, 100);
    tira.setPixelColor(Pantalla[154], 100, 100, 100);
    tira.setPixelColor(Pantalla[155], 0, 0, 255);
    tira.setPixelColor(Pantalla[157], 0, 0, 255);
    tira.setPixelColor(Pantalla[173], 0, 0, 255);
    tira.setPixelColor(Pantalla[171], 0, 0, 255);
    tira.setPixelColor(Pantalla[170], 100, 100, 100);
    tira.setPixelColor(Pantalla[169], 100, 100, 100);
    tira.setPixelColor(Pantalla[168], 100, 100, 100);
    tira.setPixelColor(Pantalla[167], 100, 100, 100);
    tira.setPixelColor(Pantalla[166], 100, 100, 100);
    tira.setPixelColor(Pantalla[165], 100, 100, 100);
    tira.setPixelColor(Pantalla[164], 0, 0, 255);
    tira.setPixelColor(Pantalla[162], 0, 0, 255);
    tira.setPixelColor(Pantalla[181], 0, 0, 255);
    tira.setPixelColor(Pantalla[182], 100, 100, 100);
    tira.setPixelColor(Pantalla[183], 100, 100, 100);
    tira.setPixelColor(Pantalla[184], 100, 100, 100);
    tira.setPixelColor(Pantalla[185], 100, 100, 100);
    tira.setPixelColor(Pantalla[186], 0, 0, 255);
    tira.setPixelColor(Pantalla[202], 0, 0, 255);
    tira.setPixelColor(Pantalla[201], 100, 100, 100);
    tira.setPixelColor(Pantalla[200], 100, 100, 100);
    tira.setPixelColor(Pantalla[199], 100, 100, 100);
    tira.setPixelColor(Pantalla[198], 100, 100, 100);
    tira.setPixelColor(Pantalla[197], 0, 0, 255);
    tira.setPixelColor(Pantalla[213], 0, 255, 0);
    tira.setPixelColor(Pantalla[214], 0, 255, 0);
    tira.setPixelColor(Pantalla[215], 0, 0, 255);
    tira.setPixelColor(Pantalla[216], 0, 0, 255);
    tira.setPixelColor(Pantalla[217], 0, 255, 0);
    tira.setPixelColor(Pantalla[218], 0, 255, 0);

    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
  }
}
/*------------------------------------------------------------------------------*/



void Matriz_Ok()
{
tira.setPixelColor(Pantalla[ 76],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 75],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 74],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 73],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 70],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 67],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 83],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 86],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 89],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 92],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 108],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 105],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 102],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 100],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 115],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 118],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 121],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 122],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 140],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 137],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 134],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 132],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 147],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 150],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 153],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 156],(10+rand()%245),(10+rand()%245),(10+rand()%245));
tira.setPixelColor(Pantalla[ 172],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 171],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 170],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 169],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 166],(10+rand()%245),(10+rand()%245),(10+rand()%245));tira.setPixelColor(Pantalla[ 163],(10+rand()%245),(10+rand()%245),(10+rand()%245));

  }
/*-------------------------------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------------------------------*/