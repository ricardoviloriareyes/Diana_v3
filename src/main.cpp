// DIANA_V3
/*
BITACORA
30 Abril 2026
Se cambia la rutina de leer freceuncia y se reemplaza por un lectura de la media y desviacion estandar
Si la desviacion estandar representa menos del 30% se considera una lectura aceptable
Se controla mediante 
int tolerancia_desviacion_std=0.35
Se muestrea sobre 20 segmentos de 10mseg cada uno por ello se checa los pulsos en ese segmento que
son pulsos=frecuencia/100, ejemplo 1khz en 100 mseg son 100 pulsos en 10 mseg son 10 pulsos
Se ilumina la seccion de tiro con base a 
Matriz_Ilumina_Mira_Color_Jugador_Apuntado();

28 de ABRIL 2026
Se modifica para recibir las freceuncias nuevas de la pistola generados por un esp32-c3
Se reduce el tiempo de deepsleep de la pistola a 8 segundos
Se realiza cambio de apunta y dispara y se deja el estado de espera con la misma frecuencia que apunta
esto elimina un paso pero es mucho mas sencillo la operacion, ya que el usuario puede ver cuando apunta
al blanco y como cambia al color de su pistola para que ejecute el tiro
Se modifica que al no recibir ningun habilitacion del monitor por mas de 1 minuto, empiece a mostrar
los diferentes blancos con el objetivo de hacer publicidad al juego e invitar a los parroquianos a jugar
se realiza con la opcion     Ejecuta_Publicidad();



18 DE AGOSTO DE 2025
Se instala en otra maquina y se configura, ya no manda errores
al parecer actualizo a la ultima versión
Se modificara para soportar el cambio de sentido en la matriz del display
ya que se tuvo que modificar las posiciones porque la matriz no es en zifzag, sino
en lineas (matriz comprada en amazon)

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
uint8_t broadcastAddress1[] = {0x24, 0x62, 0xAB, 0xDC, 0xAC, 0xF4};
// 4c:eb:d6:75:48:30

// Monitor Central para regreso de resultados
uint8_t broadcastAddressMonitor[] = {0x4C, 0xEB, 0xD6, 0x75, 0x48, 0x30};

#define NO 0
#define SI 1

// Colores del disparo
#define COLOR_SIN_ASIGNAR 0
#define RED 1
#define GREEN 2
#define BLUE 3
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
#define INICIALIZA_TIEMPO 1
#define MONITOREA_DISPARO 2
#define ENVIA_RESULTADOS 3
#define FINALIZA_REINICIA_VARIBLES 4
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
  uint8_t c; // color
  uint8_t p; // No. de jugador

  // propiedades del tiro
  uint8_t s; // numero asignado de tiro, sirve para las figuras numeros CD001

  // Condiciones de manejo del tiro
  uint8_t pp; // tiro privado (solo jugador original ) o publico (cualquiera de los dos)
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
structura_mensaje
    datos_recibidos;
// structura_mensaje datos_locales_diana;  // para usar en diana

#define TEST 0
#define TIRO_ACTIVO 1
#define JUGADOR_READY 2

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

// Castigo el valor inicial se genera en monitor y se recibe
//  debe ser el mismo en monitor
#define CASTIGO_UNICORNIO 16
#define CASTIGO_BOTELLA 15
#define CASTIGO_COPA 14

// Normal el valor inicial se genera en diana
// el valor se genera de forma local, debe ser diferente a castigo y bonos
//  se genera aqui y el monitor recibe, debe ser el mismo para mostrar la misma
//  figura en el monitor
#define NORMAL_SIN_IMPACTO 13
#define NORMAL_CONEJO 12
#define NORMAL_OSO 11
#define NORMAL_ZORRA 10
#define NORMAL_MARIPOSA 9
#define NORMAL_PINGUINO 8
#define NORMAL_ARANA 7
#define NORMAL_LAGARTIJA 6
#define NORMAL_RANA 5

// Bono el valor inicial se genera en monitor y se recibe
// debe ser el mismo en monitor
#define BONO_CORAZON 4
#define BONO_PINGUINO 3
#define BONO_RANA 2
#define BONO_CORAZON_200 1 // el bono corazon 100 se genera de forma local y se envia a monitor

#define FIGURA_SIN_DEFINIR 0

#define PUNTOS_FIGURA_SIN_DEFINIR 0

#define PUNTOS_NORMAL_OSO 50
#define PUNTOS_NORMAL_ZORRA 40
#define PUNTOS_NORMAL_MARIPOSA 45
#define PUNTOS_BONO_PIGUINO 75
#define PUNTOS_NORMAL_ARANA 35
#define PUNTOS_NORMAL_LAGARTIJA 30
#define PUNTOS_NORMAL_RANA 50
// Bono
#define PUNTOS_BONO_CORAZON 300
#define PUNTOS_BONO_PINGUINO 200
#define PUNTOS_BONO_RANA 150
#define PUNTOS_BONO_CORAZON_200 100
#define PUNTOS_NORMAL_CONEJO 30
#define PUNTOS_NORMAL_PERRA 20
#define PUNTOS_CALIBRA_0 0
// Castigo
#define PUNTOS_CASTIGO_UNICORNIO -200
#define PUNTOS_CASTIGO_BOTELLA -100
#define PUNTOS_CASTIGO_COPA -70
#define PUNTOS_CASTIGO_SIN_IMPACTO 0

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
int16_t valor_puntuacion_figura = 0;
// se usa para identificar la figura y mostrar el valor del tiro, se quito de "Evalua_Figura_NOrmal()"
uint8_t resultado_figura = FIGURA_SIN_DEFINIR;

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
void Espera(int local_millisegundos_de_espera)
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
void readMacAddress()
{
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK)
  {
    Serial.print("Direccion de esta Diana :");
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
uint8_t color_original = COLOR_SIN_ASIGNAR;
uint8_t Posicion_Aro = 1;

// estado de diana
#define APAGADO 0
#define ENCENDIDO 1
int case_estado_diana = APAGADO;

// revisar la frecuencia con osciloscopio del disparo
/* variables para analisis de frecuencia */
ulong frecuencia_disparo = 10000;
ulong frecuencia_apunta = 5000;

ulong frecuencia_disparo_tiro_publico = 0;
ulong frecuencia_apunta_tiro_publico = 0;

// los datos de las frecuencias se dividen entre 100 para sacar la media de la  muestra
//ejemplo disparo verde 2200 si se muestrea en 10 mseg el resultado es 22 pulsos
int frecuencia_apunta_neutro = 5;
int frecuencia_apunta_tiro_verde = 12;
int frecuencia_disparo_tiro_verde = 22;
int frecuencia_apunta_tiro_azul = 32;
int frecuencia_disparo_tiro_azul = 42;
int ajuste_frecuencia_arriba = 3; // es el ajuste hacia arriba y hacia abajo del valor generado de la media
int ajuste_frecuencia_abajo=1;

#define SIN_DETECCION 0
#define DISPARO_VERDE 1
#define APUNTA_VERDE 2
#define APUNTA_ANONIMO 3
#define APUNTA_AZUL 4
#define DISPARO_AZUL 5

uint8_t case_valor_del_laser = SIN_DETECCION;
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
    // case_posicion_del_proceso = INICIA_STANDBYE;
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
#define SIN_DETECCION 0
#define LASER_APUNTANDO 1
#define DISPARO_DETECTADO 2
// int8_t estado_detector = SIN_DETECCION;

//----
volatile unsigned long tiempo_inicial_muestreo = 0;
volatile unsigned long tiempo_actual_muestreo = 0;
volatile unsigned long tiempo_actual_leds = 0;
volatile unsigned long periodo_muestreo_mseg = 1000;
volatile unsigned long pulsos = 0;
volatile unsigned long muestra[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


volatile unsigned long promedio_muestreo = 0;
float global_porcentaje_desviacion=1;
int nueva_frecuencia = 0;
int media_estadistica_obtenida =0;
volatile unsigned long acumulado = 0;
int8_t rango_actual = 0;
int8_t evalua_frecuencia = NO;
int8_t posicion_nueva = SIN_DETECCION;
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

volatile unsigned long tiempo_inicial_reloj_10mseg = millis();
volatile unsigned long tiempo_actual_reloj_10mseg = 0;


uint8_t propietario_original_id = 0;
uint8_t propietario_original_color = 0;
uint8_t propietario_alternativo_id = 0;
uint8_t propietario_alternativo_color = 0;

// declaracion de funciones
float Porcentaje_De_Desviacion_Frecuencia();
void Calcula_Frecuencia_Estadistica();

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
void Test_Conexion_Diana();
void readMacAddress();
void Inicia_ESP_NOW();
void Peer_Monitor_En_Linea();
void Envia_Resultados_Al_Monitor();
void Dibuja_Valor_Del_Laser(int local_valor_laser);

int reloj_1seg();
int creloj_500mseg();
int reloj_200mseg();
int reloj_150mseg();
int creloj_100mseg();
int creloj_10mseg();

int8_t detecto_laser = NO;
int8_t encender_monitor_de__frecuencia = NO;

// publicidad
void Muestra_Figura_Publicidad(int local_figura_publicidad);
void Ejecuta_Publicidad();
void Matriz_Ilumina_Mira_Aleatorio();
void Matriz_Ilumina_Mira_Color_Jugador_Apuntado();

// figuras

void Matriz_Oso();
void Matriz_Zorra();
void Matriz_Mariposa();
void Matriz_Pinguino();
void Matriz_Arana();
void Matriz_Lagartija();
void Matriz_Rana();

// sin usar figuras
void Matriz_TimeOut();
void Matriz_Perra();
void Matriz_Conejo();

// figura castigos
void Matriz_Unicornio();
void Matriz_Botella();
void Matriz_Copa();
void Matriz_Sin_Castigo();

// figuras bonos
void Matriz_Corazon();
void Matriz_Corazon_400();
void Matriz_Corazon_300();
void Matriz_Corazon_200();

// figuras calibra
void Matriz_Ok();

void Analiza_Posicion_En_Encendido();
void Asigna_Pixeles_Para_Definir_Color_Original(uint8_t local_color);
void Asigna_Frecuencias_Base_Para_Tiro(uint8_t local_color);
void Actualiza_Relojes();
uint8_t Calcula__Valor_Del_Laser();
void Switchea_Izquierda_Por_Derecha();
uint8_t Evalua_y_Dibuja_Figura();
void Aro_Sin_Deteccion();
void Aro_Apuntando();
void Barra_Blanca();
void Aro_Disparo();
void Reinicia_Todas_Las_Variables_Al_Termnar();
uint8_t Evalua_Figura_Normal();
void Matriz_Calibra();
void Matriz_Calibra_Publicidad();
void Asigna_Color_Basandose_En_Color_Recibido_De_Monitor();
void Aro_Sin_Deteccion_Secuencia();
void Matriz_Limpia_Secuencias();
void Matriz_Aro_1();
void Matriz_Aro_2();
void Matriz_Aro_3();
void Matriz_Aro_4();
void Matriz_Aro_5();
void Matriz_Aro_6();
void Matriz_Aro_7();
void No_Wifi();
void Espera(int local_millisegundos_de_espera);

// figuras de valores del tiro
void Aro_Disparo_Secuencia();
void Valor_De_Tiro(uint8_t local_valor_de_disparo);
void Matriz_Green();
void Matriz_Blue();
void Matriz_100();
void Matriz_90();
void Matriz_80();
void Matriz_75();
void Matriz_70();
void Matriz_60();
void Matriz_50();
void Matriz_45();
void Matriz_40();
void Matriz_35();
void Matriz_30();
void Matriz_500();
void Matriz_400();
void Matriz_300();
void Matriz_200();
void Matriz_500_Negativo();
void Matriz_400_Negativo();
void Matriz_300_Negativo();
void Matriz_200_Negativo();
void Matriz_100_Negativo();
void Matriz_75_Negativo();

void Matriz_Valor_De_Tiro(uint8_t local_color_de_disparo);
void Aro_Apuntando_Secuencia();
void Matriz_Aro_1_Apuntado();
void Matriz_Aro_2_Apuntado();
uint8_t Evalua_Figura_Castigo();
uint8_t Evalua_Figura_Bono();
void Arma_Paquete_De_Envio();
uint8_t Evalua_Figura_Calibra();

void Matriz_Aro_1_Disparo();
void Matriz_Aro_2_Disparo();
void Matriz_Aro_3_Disparo();
void Matriz_Aro_4_Disparo();
void Matriz_Aro_5_Disparo();
void Matriz_Aro_6_Disparo();
void Matriz_Aro_7_Disparo();

// Auxiliares
void Despliega_datos_recibidos(); // auxiliar para estado de datos_recibidos
void Despliega_datos_enviados();

/* -------------------- RECEPCION DE DATOS -----------------------*/
// Callback when data is received
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
  memcpy(&datos_recibidos, incomingData, sizeof(datos_recibidos));
  // informacion de longuitud de paquete
  Serial.println("---------Paquete recibido---------------");
  // Control
  Serial.println(".n = No. Paquete : " + String(datos_recibidos.n));
  Serial.print("Bytes received: ");
  Serial.println(len);
  char macStr[18];
  Serial.print("Packet  from: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
  Despliega_datos_recibidos();

  // se guada tipo de tiro recibido para validar el rady de loc jugadores de contadores descendentes
  // SE REGRESA JUGADOR READY PARA EVITAR ACUMULAR TIROS EN JUGADOR

  if (datos_recibidos.t >= TIRO_ACTIVO) // TIRO ACTIVO =1 &&  JUGADOR READY =2
  {
    // inicializa los pulsos para analisis de frecuencia
    pulsos = 0;

    // Asigna "color original" de tirador privado -> rojo,verde o azul
    Asigna_Pixeles_Para_Definir_Color_Original(datos_recibidos.c);

    // Asigna frecuencia de tirador privado para "apunta" y "dispara"
    Asigna_Frecuencias_Base_Para_Tiro(datos_recibidos.c);

    case_estado_diana = ENCENDIDO;
    // ->se pone para limpiar matriz para limpiar la publicidad antes de entrar
    tira.clear();
    tira.show();
    // <-
    encender_monitor_de__frecuencia = SI;
    Serial.println("case_estado_diana= ENCENDIDO");
  }
  else
  {
    // tiro test
  }
}

// Contadores
int i;
int casilla_muestra=0;
float tolerancia_desviacion_std=0.35;

/*-----------------------------------------------------------------------------*/
// Contador para AttachInterrupt para proceso de identificacion de freceuncia recibida por laser
void Cuenta_Pulso()
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

int contador = 0;
int temporizador_publicidad = 0;
int enciende_publicidad = 30; // ENCIENDE FIGURA
int apaga_publicidad = 34; // CAMBIA A FIGURA DE TEST
int cambio_publicidad = 38; // APAGA FIGURA Y REGRESA
int segundos_apagado= 3; // se regresa apagado anted de enciende de nuevo
int cambia_figura_publicidad = SI;
int figura_publicidad = 1;
int ilumina_mira=NO;

/* -----------------------INICIO    S E T U P --------------------------------*/

void setup()
{
  Asigna_Pantalla();
  Serial.println("DIANA");
  attachInterrupt(digitalPinToInterrupt(pin_pulsos), Cuenta_Pulso, FALLING);

  // pinMode(pin_led_blanco, OUTPUT);

  // inicia Neopixel tira
  tira.begin();
  tira.setBrightness(30);
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

  // incio de medidor de tiempo muestreo
  tiempo_inicial_muestreo = millis();
  // Serial.println(tiempo_inicial_muestreo);

  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  esp_now_register_send_cb(OnDataSent);

  Actualiza_Relojes(); // Control Diana

} // fin setup

/* ----------------------  F I N    S E T U P -------------------------*/

/* ------------------------inicio loop ----------------------------*/
void loop()
{
  encender_monitor_de__frecuencia=SI;
  if (encender_monitor_de__frecuencia == SI) // se habilita cuando se reciben datos, se deshabilita cuando se detecta disparo para evitar conflicto mientras se manda resultado
  {
    //Serial.print("CF ");
    Calcula_Frecuencia_Estadistica();
  }
  switch (case_estado_diana)
  {
  case APAGADO:
    // Serial.print("- ");
    Ejecuta_Publicidad();
    break;

  case ENCENDIDO:
    // Serial.print("+ ");
    Analiza_Posicion_En_Encendido();
    break;
  }
  // PROCESO FINAL,
  // ENVIA LOS DATOS AMONITOR Y APAGA DIANA, este proceso tiene que estar en nivel loop por requerimiento del esp_now_send
  if (habilita_enviar_paquete == SI)
  {
    Serial.println("ENVIO PAQUETE :" + String(datos_enviados.n));
    esp_err_t result = esp_now_send(broadcastAddressMonitor, (uint8_t *)&datos_enviados, sizeof(structura_mensaje));
    Espera(250);
    if (result == ESP_OK)
    {
      case_posicion_del_proceso = FINALIZA_REINICIA_VARIBLES; // termina el proceso de encendido y resetea todas las variables
      Reinicia_Todas_Las_Variables_Al_Termnar();
      Serial.println("Envio Paquete OK, intento : " + String(intentos_envio));
      habilita_enviar_paquete = NO;
      paquete_enviado_ok = SI;
      case_proceso_envio = STANDBYE;
      tira.clear();
    }
    else
    {
      Serial.println("Error envio : " + String(intentos_envio));
      intentos_envio++;
      paquete_enviado_ok = NO;
      case_proceso_envio = PAUSA_REENVIO;
      tira.clear();
      tira.show();
      tira.setPixelColor(1, tira.Color(0, 250, 0));
      tira.setPixelColor(1, tira.Color(1, 250, 0));
      tira.setPixelColor(1, tira.Color(2, 250, 0));
      tira.show();
      previous_time_reenvio = millis();
    }
  } // fin if habilita_enviar_paquete
}
// FIN  loop ---------------------------------------------------

void Ejecuta_Publicidad()
{
  // aumenta contador de tiempo de espera
  if (reloj_1seg() == DETECTA_CAMBIO_ESTADO_RELOJ)
  {
    temporizador_publicidad++;
    // define la figura a mostrar en el periodo
    if (temporizador_publicidad == enciende_publicidad)
    {
      figura_publicidad = int(rand() % 10) + 1;
    }
  }
  // Inicia mostrado de figura
  if (temporizador_publicidad >= enciende_publicidad)
  {
    Muestra_Figura_Publicidad(figura_publicidad);
  }
  // Apaga figura para esperar el cambio
  if (temporizador_publicidad >= apaga_publicidad)
  {
    figura_publicidad = 0;
    ilumina_mira=SI;
    // tira.clear();
  }
  // detecta cambio y regresa 1 segundo antes de encender para seleccionar una nueva figura
  if (temporizador_publicidad >= cambio_publicidad)
  {
    temporizador_publicidad = enciende_publicidad -segundos_apagado;
    ilumina_mira=NO;
    tira.clear();
    tira.show();
  }
}

void Muestra_Figura_Publicidad(int local_figura_publicidad)
{
  if (reloj_200mseg() == DETECTA_CAMBIO_ESTADO_RELOJ)
  {
    // Serial.print("x");
    Switchea_Izquierda_Por_Derecha();
    if(local_figura_publicidad>0){
      tira.clear();
    }
    if (ilumina_mira==SI){
        // Matriz_Ilumina_Mira_Aleatorio();
    }
    // 
    switch (local_figura_publicidad)
    {
    case 0: // solo para espacio muerto entre figuras asignado por apaga_publicidad
      // tira.clear();
      // Matriz_Calibra_Publicidad();
      Matriz_Ilumina_Mira_Aleatorio();
      break;
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
      tira.clear();
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

void Analiza_Posicion_En_Encendido()
{
  // Serial.print("case_posicion_del_proceso"+String(case_posicion_del_proceso));
  switch (case_posicion_del_proceso)
  {
  case INICIA_STANDBYE:
    Serial.println("INICIA_STANDBYE");
    case_posicion_del_proceso = INICIALIZA_TIEMPO;
    break;

  case INICIALIZA_TIEMPO:
    Serial.println("INICIALIZA_TIEMPO");
    // incia tiempo inicial del tiro
    tiempo_inicia_tiro = millis();
    tira.clear();
    case_posicion_del_proceso = MONITOREA_DISPARO;
    break;
  case MONITOREA_DISPARO:
    Serial.println("Monitorea");
    if ((evalua_frecuencia == SI) || (reloj_200mseg() == DETECTA_CAMBIO_ESTADO_RELOJ)) // solo se presenta cada 20 muestras o 200 mseg
    {
      case_valor_del_laser = Calcula__Valor_Del_Laser(); // cuando calcula modifica
      Dibuja_Valor_Del_Laser(case_valor_del_laser);
      if ((case_valor_del_laser == DISPARO_VERDE) || (case_valor_del_laser == DISPARO_AZUL) || (salir_por_timeout == SI))
      {
        encender_monitor_de__frecuencia = NO; // suspende la lectura de la frecuencia para evitar conflicto durante el envio al monitor
        tiempo_termina_tiro = millis();
        // En  Envia_Resultados_Al_Monitor(), cambia el proceso inicial de stanbye a preparar la informacion a enviar
        case_proceso_envio = PREPARA_PAQUETE_ENVIO;
        // Activa el envio del resultado a nivel raiz del programa
        // activa_envio_de_resultados = SI;
        case_posicion_del_proceso = ENVIA_RESULTADOS;
      }
      evalua_frecuencia = NO; // para que ya no evalua el valor del laser hasta la siguiente frecuencia valida o 200 mseg
    }
    break;
  case ENVIA_RESULTADOS:
    Envia_Resultados_Al_Monitor();
    break;
  case FINALIZA_REINICIA_VARIBLES:
    Reinicia_Todas_Las_Variables_Al_Termnar();
    break;
  } // fin case case_posicion_del_proceso
}

void Dibuja_Valor_Del_Laser(int local_case_valor_del_laser)
{
  switch (local_case_valor_del_laser)
  {
  case SIN_DETECCION:
    // if (reloj_200mseg() == DETECTA_CAMBIO_ESTADO_RELOJ)
    {
      // figura de fondo la imprime 
      Switchea_Izquierda_Por_Derecha(); // activa movimiento patas cada 200 milisegundos
      tira.clear();
      figura_actual = Evalua_y_Dibuja_Figura(); // evalua y Dibuja figura cada 200 milisegundos
      tira.show();
    }
    break;

  case APUNTA_VERDE:
    /*code*/
    Serial.print("APUNTA-VERDE");
    Aro_Apuntando();
    tira.show();
    break;

  case DISPARO_VERDE:
    /*code*/
    Serial.print("DISPARO-VERDE");
    Aro_Disparo(); // Se queda 250 milisegundos, no lleva tira.show() se activa dentro del Aro_Disparo
    posicion_nueva = DISPARO_DETECTADO;
    break;

  case APUNTA_AZUL:
    /*code*/
    Aro_Apuntando();
    tira.show();
    break;
  case DISPARO_AZUL:
    /*code*/
    // encender_monitor_de__frecuencia = NO;
    Aro_Disparo(); // Se queda 250 milisegundos, no lleva tira.show() se activa dentro del Aro_Disparo
    posicion_nueva = DISPARO_DETECTADO;
    break;
  }
}

void Reinicia_Todas_Las_Variables_Al_Termnar()
{
  // finaliza envio
  Serial.println("APAGA DIANA");
  case_estado_diana = APAGADO;
  temporizador_publicidad = 0; // resetea valor para que vuelva a esperar un tiempo determinado para iniciar en el estado apagado
  pulsos = 0;
  captura_tiempo_inicial = SI;
  case_proceso_envio = STANDBYE;
  salir_por_timeout = NO;

  // reinicia varibles de menus
  case_posicion_del_proceso = INICIA_STANDBYE;
  prende_leds = SI;
  led_inicio = 0;

  // tiempo de duracion del tiro
  tiempo_inicia_tiro = 0;
  tiempo_termina_tiro = 0;
  activa_envio_de_resultados = NO;

  // inicializa variables de freceuncia
  rango_actual = 0;
  evalua_frecuencia = NO;
  posicion_nueva = SIN_DETECCION;
  captura_tiempo_inicial = SI;
  promedio_muestreo = 0;
  nueva_frecuencia = 0;
  acumulado = 0;

  // reinicia el acumulado y rangos
  frecuencia_disparo = 0;
  frecuencia_apunta = 0;
}

void Actualiza_Relojes()
{
  // para switchear los parpadeos de las figuras
  tiempo_actual_reloj_1seg = millis();
  tiempo_actual_reloj_500mseg = millis();
  tiempo_actual_reloj_200mseg = millis();
  tiempo_actual_reloj_150mseg =millis();
  tiempo_actual_reloj_100mseg = millis();
  tiempo_actual_reloj_10mseg=millis();
}

/*-----------------------------------------------*/
void Envia_Resultados_Al_Monitor()
{
  Serial.println("Envia resultados al monitor");
  Serial.println("case_proceso_envio = " + String(case_proceso_envio));

  switch (case_proceso_envio)
  {
  case STANDBYE:
    /* solo espera */
    break;

  case PREPARA_PAQUETE_ENVIO:
    Arma_Paquete_De_Envio();
    intentos_envio = 1;
    case_proceso_envio = ENVIA_PAQUETE;
    break;

  case ENVIA_PAQUETE: // se pone a primer nivel por deficion del la función
    habilita_enviar_paquete = SI;
    break;

  case PAUSA_REENVIO:
    current_time_reenvio = millis();
    if ((current_time_reenvio - previous_time_reenvio) > 250)
    {
      if (intentos_envio < 4)
      {
        case_proceso_envio = ENVIA_PAQUETE;
      }
      else // manda mensaje de error
      {
        tira.clear();
        No_Wifi();
        tira.show();
        Espera(3000);
        Reinicia_Todas_Las_Variables_Al_Termnar();
        Serial.println("APAGA DIANA en PAUSA REENVIO");
        case_estado_diana = APAGADO;
        captura_tiempo_inicial = NO;
      }
    }
    break;
  } // fin switch case_proceso_envio
}

/*------------------------------------------------------------*/

void Despliega_datos_enviados()
{

  // datos obtenidos del disparo
  Serial.println();
  Serial.println("---------DATOS ENVIADOS A MONITOR-------------------");
  Serial.println("No. jugador que impacto : " + String(datos_enviados.jg));
  Serial.println("Puntuacion del disparo  : " + String(datos_enviados.po));
  Serial.println("figura que impacto      : " + String(datos_enviados.fi));

  // Propiedades del Paquete
  switch (datos_enviados.t)
  {
  case TEST:
    Serial.println(".t = 0= TEST DE FUNCIONAMIENTO");
    break;
  case TIRO_ACTIVO:
    Serial.println(".t = 1= SOLICITUD DE TIRO ACTIVO");
    break;
  case JUGADOR_READY:
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
    Serial.println("tf =1 = TIPO_NORMAL ");
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
    Serial.println(".tf = no encontrado el valor   --> " + String(datos_enviados.tf));
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

  Serial.println("---------FIN DE DATOS ENVIADOS  ----------------");
  Serial.println();
}

/* ----------------------------------------------------------*/

void Despliega_datos_recibidos()
{
  Serial.println("---------DATOS RECIBIDOS-------------------");
  // Propiedades del Paquete
  switch (datos_recibidos.t)
  {
  case TEST:
    Serial.println(".t = 0= TEST DE FUNCIONAMIENTO");
    break;
  case TIRO_ACTIVO:
    Serial.println(".t = 1= SOLICITUD DE TIRO ACTIVO");
    break;
  case JUGADOR_READY:
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

  Serial.println("---------FIN DE DATOS RECIBIDOS ----------------");
}


void Calcula_Frecuencia_Estadistica()
{
  if (creloj_10mseg()==DETECTA_CAMBIO_ESTADO_RELOJ)
  { 
    muestra[casilla_muestra] = pulsos;
    pulsos = 0;
    casilla_muestra++;
    if (casilla_muestra == 20) // se paso a la 20 por orden anterior,se regresa al origen
    {
      casilla_muestra = 0; // regresa a casilla[0]
      if (Porcentaje_De_Desviacion_Frecuencia() <= tolerancia_desviacion_std)
      { // datos mayores a 1Khz y desviacion menor a 35% respectoa la media
        evalua_frecuencia = SI;
        nueva_frecuencia = media_estadistica_obtenida;
        Serial.println(nueva_frecuencia);
      }
      else  //solo valores arriba de 1khz que desviacion en mayor a 35%
      {
        evalua_frecuencia = NO;
        // Serial.print("NO valida = ");
        // Serial.print("Media Estadistica ="+String(media_estadistica_obtenida)+"  D: ");
        // Serial.println(global_porcentaje_desviacion);

        // nueva_frecuencia= se mantiene la anterior
      }
    }
  }
}

float Porcentaje_De_Desviacion_Frecuencia()   // Calculos de la media, varianza, desviacion estandar
{
  float local_media_muestral = 0;
  float local_suma_cuadrados = 0;
  float local_varianza_muestral = 0;
  float local_distribucion_estandar=1;
  float local_porcentaje_desviacion=1;
  volatile unsigned long local_resta_muestra[20] =            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  volatile unsigned long local_cuadrados_resta_muestra[20] =  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  // Obtiene Media
  for (i = 0; i <= 19; i++)
  {
    local_media_muestral = local_media_muestral + muestra[i];
  }
  local_media_muestral = local_media_muestral / 19; // son # casillas de muestreo

  // Resta Media de casillas
  for (i = 0; i <= 19; i++)
  {
    local_resta_muestra[i] = muestra[i] - int(local_media_muestral);
  }
  // Obtiene Cuadrados de casillas de resta para quitar los negativos
  for (i = 0; i <= 19; i++)
  {
    local_cuadrados_resta_muestra[i] = local_resta_muestra[i] * local_resta_muestra[i];
  }
  // Suma Cuadrados
  for (i = 0; i <= 19; i++)
  {
    local_suma_cuadrados = local_suma_cuadrados + local_cuadrados_resta_muestra[i];
  }
  // Varianza Muestral (n-1) n=20 periodos
  local_varianza_muestral = local_suma_cuadrados / 19; 
  
  // Desviacion estandar
  local_distribucion_estandar=sqrt(local_varianza_muestral);

  // Calculo de la desviacion vs la media de la muestra
  if (local_media_muestral >= 10) // evalua si son validos los datos arriba de 1khz
    { 
      // para no dividir entre 0
      local_porcentaje_desviacion = local_distribucion_estandar / local_media_muestral;
      media_estadistica_obtenida = int(local_media_muestral);
      global_porcentaje_desviacion=local_porcentaje_desviacion;
    }
  else // todos los datos de abajo de 1 khz lo manda a 0
    { // Se quita la basura y se deja limpia la señal con frecuencia=0
      local_porcentaje_desviacion = 0; // para hacer valida la lectura
      media_estadistica_obtenida = 0;  // para dar un valor 0 en el ruido
      Serial.print("*");
      Serial.println(int(local_media_muestral));
    }
  return local_porcentaje_desviacion;
}




/* ----------------------------------------------------- */
uint8_t Calcula__Valor_Del_Laser()
{
  //Sin deteccion valor menor a 900 Hrz
  uint8_t resultado = SIN_DETECCION; // en caso que sea cualquier otra frecuencia regresa el valor SIN_DETECCION

  // Verde apunta 1.2K
  if ((nueva_frecuencia > (frecuencia_apunta_tiro_verde - ajuste_frecuencia_abajo)) && (nueva_frecuencia < (frecuencia_apunta_tiro_verde + ajuste_frecuencia_arriba)))
  {
    resultado = APUNTA_VERDE;
    color_de_disparo = GREEN;
    Serial.println();
    Serial.print("Apunta-Verde");
  }
  // Verde disparo 2.2K
  if ((nueva_frecuencia > (frecuencia_disparo_tiro_verde - ajuste_frecuencia_abajo)) && (nueva_frecuencia < (frecuencia_disparo_tiro_verde + ajuste_frecuencia_arriba)))
  {
    resultado = DISPARO_VERDE;
    color_de_disparo = GREEN;
    Serial.println();
    Serial.print("Disparo-Verde");
  }
  // Azul apunta 3.2K
  if ((nueva_frecuencia > (frecuencia_apunta_tiro_azul - ajuste_frecuencia_abajo)) && (nueva_frecuencia < (frecuencia_apunta_tiro_azul + ajuste_frecuencia_arriba)))
  {
    resultado = APUNTA_AZUL;
    color_de_disparo = BLUE;
    Serial.print("Apunta-Azul");
  }
  // Azul disparo 4.2K
  if (nueva_frecuencia > (frecuencia_disparo_tiro_azul - ajuste_frecuencia_abajo) && (nueva_frecuencia < (frecuencia_disparo_tiro_azul + ajuste_frecuencia_arriba+2)))
  {
    resultado = DISPARO_AZUL;
    color_de_disparo = BLUE;
    Serial.print("Disparo-Azul");
  }
  return resultado;
}

/*--------------------------------------------------------*/

int reloj_1seg() // CD001
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

int reloj_200mseg() // CD001
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

int creloj_10mseg() // CD001
{
  tiempo_actual_reloj_10mseg = millis();
  if ((tiempo_inicial_reloj_10mseg + 10) < tiempo_actual_reloj_10mseg)
  {
    tiempo_inicial_reloj_10mseg = millis();
    actualiza_display_10mseg = SI;
    return DETECTA_CAMBIO_ESTADO_RELOJ;
  }
  else
  {
    actualiza_display_10mseg = NO;
    return SIN_CAMBIO_ESTADO_RELOJ;
  }
}



/* ---------------------------------------------------------------------*/

void Asigna_Pixeles_Para_Definir_Color_Original(uint8_t local_color)
{
  switch (datos_recibidos.c)
  {
  case RED:
    /*code*/
    // Serial.println("Programa color rojo para castigos");
    pixel_rojo = 0;
    pixel_verde = 250;
    pixel_azul = 0;
    color_original = RED;
    break;

  case GREEN:
    // Serial.println("Programa color verde");
    pixel_rojo = 250;
    pixel_verde = 0;
    pixel_azul = 0;
    color_original = GREEN;
    break;

  case BLUE:
    // Serial.println("Programa color azul");
    pixel_rojo = 0;
    pixel_verde = 0;
    pixel_azul = 250;
    color_original = BLUE;
    break;
  }
}

void Asigna_Frecuencias_Base_Para_Tiro(uint8_t local_color)
{

  switch (local_color)
  {
  case GREEN:
    /*code*/
    frecuencia_apunta = frecuencia_apunta_tiro_verde;
    frecuencia_disparo = frecuencia_disparo_tiro_verde;
    break;

  case BLUE:
    /*code*/
    frecuencia_apunta = frecuencia_apunta_tiro_azul;
    frecuencia_disparo = frecuencia_disparo_tiro_azul;
    break;

  case RED:
    /*code  solo de relleno, no debe de usarse esta opcion,solo es valida en nivel Publico
     y ahi se decide directamente una frecuencia OR la otra frecuencia*/
    frecuencia_apunta = frecuencia_apunta_tiro_verde;
    frecuencia_disparo = frecuencia_disparo_tiro_verde;
    break;
  }
}

uint8_t Evalua_y_Dibuja_Figura()
{
  // Serial.println("tipo de datos.recibidos.tf "+String(datos_recibidos.tf));
  uint8_t resultado = FIGURA_SIN_DEFINIR;
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
    resultado = Evalua_Figura_Calibra();
    break;
  }
  return resultado;
}

/*------------------------------------------------------------------------*/
uint8_t Evalua_Figura_Normal()
{
  uint8_t resultado_figura = FIGURA_SIN_DEFINIR;
  int rango = millis() - tiempo_inicia_tiro;

  // clasifica la figura dependiendo del tiempo df (velocidad de cambio figura)

  if (rango < datos_recibidos.vf * 1)
  {
    resultado_figura = NORMAL_OSO;
    valor_puntuacion_figura = PUNTOS_NORMAL_OSO;
    Matriz_Oso();
  }
  else
  {
    if (rango < (datos_recibidos.vf * 2))
    {
      resultado_figura = NORMAL_MARIPOSA;
      valor_puntuacion_figura = PUNTOS_NORMAL_MARIPOSA;
      Matriz_Mariposa();
    }
    else
    {
      if (rango < (datos_recibidos.vf * 3))
      {
        resultado_figura = NORMAL_ZORRA;
        valor_puntuacion_figura = PUNTOS_NORMAL_ZORRA;
        Matriz_Zorra();
      }
      else
      {
        if (rango < (datos_recibidos.vf * 4))
        {
          resultado_figura = NORMAL_ARANA;
          valor_puntuacion_figura = PUNTOS_NORMAL_ARANA;
          Matriz_Arana();
        }
        else
        {
          resultado_figura = NORMAL_LAGARTIJA;
          valor_puntuacion_figura = PUNTOS_NORMAL_LAGARTIJA;
          Matriz_Lagartija();
        }
      } // end if vf*3
    } // end if vf*2
  } // end if vf
  return resultado_figura;
}

/*-----------------------------------------------------------------------*/
uint8_t Evalua_Figura_Bono()
{
  // Mostrara la figura por dos periodos, en caso de no impacto manda el valor a cero
  uint8_t resultado = FIGURA_SIN_DEFINIR;
  int rango = millis() - tiempo_inicia_tiro;
  if (rango < (datos_recibidos.vf * 2))
  {
    // Serial.println("Bono- valor de datos_recibidos.xf : " + String(datos_recibidos.xf));
    switch (datos_recibidos.xf)
    {
    case BONO_CORAZON:
      resultado = datos_recibidos.xf;
      valor_puntuacion_figura = PUNTOS_BONO_CORAZON;
      Matriz_Corazon();
      break;

    case BONO_PINGUINO:
      resultado = datos_recibidos.xf;
      valor_puntuacion_figura = PUNTOS_BONO_PIGUINO;
      Matriz_Pinguino();
      break;

    case BONO_RANA:
      resultado = datos_recibidos.xf;
      valor_puntuacion_figura = PUNTOS_BONO_RANA;
      Matriz_Rana();
      break;
    }
  }
  else
  {
    resultado = BONO_RANA;
    valor_puntuacion_figura = PUNTOS_BONO_RANA;
    Matriz_Rana();
    tira.show();
    // salir_por_timeout=SI;
  }
  tira.show();
  return resultado;
}

uint8_t Evalua_Figura_Castigo()
{
  uint8_t resultado = FIGURA_SIN_DEFINIR;
  int rango = millis() - tiempo_inicia_tiro;
  if (rango < (datos_recibidos.vf * 2))
  {
    // Serial.println("Castigo- valor de datos_recibidos.xf : " + String(datos_recibidos.xf));
    switch (datos_recibidos.xf)
    {
    case CASTIGO_UNICORNIO:
      resultado = datos_recibidos.xf;
      valor_puntuacion_figura = PUNTOS_CASTIGO_UNICORNIO;
      Matriz_Unicornio();
      break;

    case CASTIGO_BOTELLA:
      resultado = datos_recibidos.xf;
      valor_puntuacion_figura = PUNTOS_CASTIGO_BOTELLA;
      Matriz_Botella();
      break;

    case CASTIGO_COPA:
      resultado = datos_recibidos.xf;
      valor_puntuacion_figura = PUNTOS_CASTIGO_COPA;
      Matriz_Copa();
      break;
    }
  }
  else
  {
    if (rango < (datos_recibidos.vf * 4)) // si no dispara al castigo, se da el premio segun el castigo
    {
      // Serial.println("CastigoBono- valor de datos_recibidos.xf : " + String(datos_recibidos.xf));
      switch (datos_recibidos.xf)
      {
      case CASTIGO_UNICORNIO: // se premia con mayor punturacion  por no disparar a unicornio
        resultado = BONO_CORAZON;
        valor_puntuacion_figura = PUNTOS_BONO_CORAZON;
        Matriz_Corazon();
        break;

      case CASTIGO_BOTELLA:
        resultado = BONO_PINGUINO;
        valor_puntuacion_figura = PUNTOS_BONO_PINGUINO;
        Matriz_Pinguino();
        break;

      case CASTIGO_COPA:
        resultado = BONO_RANA;
        valor_puntuacion_figura = PUNTOS_BONO_RANA;
        Matriz_Rana();
        break;
      }
    }

    else
    {
      resultado = BONO_RANA; // espera el disparo y sa bono menor
      valor_puntuacion_figura = PUNTOS_BONO_RANA;
      Matriz_Rana();
      tira.show();
    }
  }
  tira.show();
  return resultado;
}

uint8_t Evalua_Figura_Calibra()
{
  uint8_t resultado = FIGURA_SIN_DEFINIR;
  resultado = CALIBRA_OK;
  valor_puntuacion_figura = PUNTOS_CALIBRA_0;
  tira.clear();
  Asigna_Color_Basandose_En_Color_Recibido_De_Monitor();
  Matriz_Calibra();
  tira.show();
  return resultado;
}

void Switchea_Izquierda_Por_Derecha()
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

/*----------------*/

void Matriz_Conejo()
{

  if (lado == IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[0], 0, 0, 0);
    tira.setPixelColor(Pantalla[1], 100, 100, 100);
    tira.setPixelColor(Pantalla[2], 105, 210, 30);
    tira.setPixelColor(Pantalla[3], 105, 210, 30);
    tira.setPixelColor(Pantalla[4], 105, 210, 30);
    tira.setPixelColor(Pantalla[5], 100, 100, 100);
    tira.setPixelColor(Pantalla[6], 0, 0, 0);
    tira.setPixelColor(Pantalla[7], 0, 0, 0);
    tira.setPixelColor(Pantalla[8], 0, 0, 0);
    tira.setPixelColor(Pantalla[9], 0, 0, 0);
    tira.setPixelColor(Pantalla[10], 100, 100, 100);
    tira.setPixelColor(Pantalla[11], 105, 210, 30);
    tira.setPixelColor(Pantalla[12], 105, 210, 30);
    tira.setPixelColor(Pantalla[13], 105, 210, 30);
    tira.setPixelColor(Pantalla[14], 100, 100, 100);
    tira.setPixelColor(Pantalla[15], 0, 0, 0);
    tira.setPixelColor(Pantalla[31], 0, 0, 0);
    tira.setPixelColor(Pantalla[30], 100, 100, 100);
    tira.setPixelColor(Pantalla[29], 100, 100, 100);
    tira.setPixelColor(Pantalla[28], 105, 210, 30);
    tira.setPixelColor(Pantalla[27], 105, 210, 30);
    tira.setPixelColor(Pantalla[26], 100, 100, 100);
    tira.setPixelColor(Pantalla[25], 0, 0, 0);
    tira.setPixelColor(Pantalla[24], 0, 0, 0);
    tira.setPixelColor(Pantalla[23], 0, 0, 0);
    tira.setPixelColor(Pantalla[22], 0, 0, 0);
    tira.setPixelColor(Pantalla[21], 100, 100, 100);
    tira.setPixelColor(Pantalla[20], 105, 210, 30);
    tira.setPixelColor(Pantalla[19], 105, 210, 30);
    tira.setPixelColor(Pantalla[18], 100, 100, 100);
    tira.setPixelColor(Pantalla[17], 100, 100, 100);
    tira.setPixelColor(Pantalla[16], 0, 0, 0);
    tira.setPixelColor(Pantalla[32], 0, 0, 0);
    tira.setPixelColor(Pantalla[33], 0, 0, 0);
    tira.setPixelColor(Pantalla[34], 100, 100, 100);
    tira.setPixelColor(Pantalla[35], 100, 100, 100);
    tira.setPixelColor(Pantalla[36], 100, 100, 100);
    tira.setPixelColor(Pantalla[37], 105, 210, 30);
    tira.setPixelColor(Pantalla[38], 100, 100, 100);
    tira.setPixelColor(Pantalla[39], 0, 0, 0);
    tira.setPixelColor(Pantalla[40], 0, 0, 0);
    tira.setPixelColor(Pantalla[41], 100, 100, 100);
    tira.setPixelColor(Pantalla[42], 105, 210, 30);
    tira.setPixelColor(Pantalla[43], 100, 100, 100);
    tira.setPixelColor(Pantalla[44], 100, 100, 100);
    tira.setPixelColor(Pantalla[45], 100, 100, 100);
    tira.setPixelColor(Pantalla[46], 0, 0, 0);
    tira.setPixelColor(Pantalla[47], 0, 0, 0);
    tira.setPixelColor(Pantalla[63], 0, 0, 0);
    tira.setPixelColor(Pantalla[62], 0, 0, 0);
    tira.setPixelColor(Pantalla[61], 0, 0, 0);
    tira.setPixelColor(Pantalla[60], 0, 0, 0);
    tira.setPixelColor(Pantalla[59], 100, 100, 100);
    tira.setPixelColor(Pantalla[58], 105, 210, 30);
    tira.setPixelColor(Pantalla[57], 100, 100, 100);
    tira.setPixelColor(Pantalla[56], 100, 100, 100);
    tira.setPixelColor(Pantalla[55], 100, 100, 100);
    tira.setPixelColor(Pantalla[54], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], 105, 210, 30);
    tira.setPixelColor(Pantalla[52], 100, 100, 100);
    tira.setPixelColor(Pantalla[51], 0, 0, 0);
    tira.setPixelColor(Pantalla[50], 0, 0, 0);
    tira.setPixelColor(Pantalla[49], 0, 0, 0);
    tira.setPixelColor(Pantalla[48], 0, 0, 0);
    tira.setPixelColor(Pantalla[64], 0, 0, 0);
    tira.setPixelColor(Pantalla[65], 0, 0, 0);
    tira.setPixelColor(Pantalla[66], 0, 0, 0);
    tira.setPixelColor(Pantalla[67], 0, 0, 0);
    tira.setPixelColor(Pantalla[68], 105, 210, 30);
    tira.setPixelColor(Pantalla[69], 100, 100, 100);
    tira.setPixelColor(Pantalla[70], 100, 100, 100);
    tira.setPixelColor(Pantalla[71], 100, 100, 100);
    tira.setPixelColor(Pantalla[72], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], 100, 100, 100);
    tira.setPixelColor(Pantalla[74], 100, 100, 100);
    tira.setPixelColor(Pantalla[75], 105, 210, 30);
    tira.setPixelColor(Pantalla[76], 0, 0, 0);
    tira.setPixelColor(Pantalla[77], 0, 0, 0);
    tira.setPixelColor(Pantalla[78], 0, 0, 0);
    tira.setPixelColor(Pantalla[79], 0, 0, 0);
    tira.setPixelColor(Pantalla[95], 0, 0, 0);
    tira.setPixelColor(Pantalla[94], 0, 0, 0);
    tira.setPixelColor(Pantalla[93], 0, 0, 0);
    tira.setPixelColor(Pantalla[92], 100, 100, 100);
    tira.setPixelColor(Pantalla[91], 100, 100, 100);
    tira.setPixelColor(Pantalla[90], 100, 100, 100);
    tira.setPixelColor(Pantalla[89], 100, 100, 100);
    tira.setPixelColor(Pantalla[88], 100, 100, 100);
    tira.setPixelColor(Pantalla[87], 100, 100, 100);
    tira.setPixelColor(Pantalla[86], 100, 100, 100);
    tira.setPixelColor(Pantalla[85], 100, 100, 100);
    tira.setPixelColor(Pantalla[84], 100, 100, 100);
    tira.setPixelColor(Pantalla[83], 100, 100, 100);
    tira.setPixelColor(Pantalla[82], 0, 0, 0);
    tira.setPixelColor(Pantalla[81], 0, 0, 0);
    tira.setPixelColor(Pantalla[80], 0, 0, 0);
    tira.setPixelColor(Pantalla[98], 100, 100, 100);
    tira.setPixelColor(Pantalla[99], 100, 100, 100);
    tira.setPixelColor(Pantalla[100], 100, 100, 100);
    tira.setPixelColor(Pantalla[101], 100, 100, 100);
    tira.setPixelColor(Pantalla[102], 100, 100, 100);
    tira.setPixelColor(Pantalla[103], 100, 100, 100);
    tira.setPixelColor(Pantalla[104], 100, 100, 100);
    tira.setPixelColor(Pantalla[105], 100, 100, 100);
    tira.setPixelColor(Pantalla[106], 100, 100, 100);
    tira.setPixelColor(Pantalla[107], 100, 100, 100);
    tira.setPixelColor(Pantalla[108], 100, 100, 100);
    tira.setPixelColor(Pantalla[109], 100, 100, 100);
    tira.setPixelColor(Pantalla[125], 100, 100, 100);
    tira.setPixelColor(Pantalla[124], 100, 100, 100);
    tira.setPixelColor(Pantalla[123], 100, 100, 100);
    tira.setPixelColor(Pantalla[122], 0, 255, 0);
    tira.setPixelColor(Pantalla[121], 100, 100, 100);
    tira.setPixelColor(Pantalla[120], 100, 100, 100);
    tira.setPixelColor(Pantalla[119], 100, 100, 100);
    tira.setPixelColor(Pantalla[118], 100, 100, 100);
    tira.setPixelColor(Pantalla[117], 0, 255, 0);
    tira.setPixelColor(Pantalla[116], 100, 100, 100);
    tira.setPixelColor(Pantalla[115], 100, 100, 100);
    tira.setPixelColor(Pantalla[114], 100, 100, 100);
    tira.setPixelColor(Pantalla[130], 100, 100, 100);
    tira.setPixelColor(Pantalla[131], 100, 100, 100);
    tira.setPixelColor(Pantalla[132], 100, 100, 100);
    tira.setPixelColor(Pantalla[133], 100, 100, 100);
    tira.setPixelColor(Pantalla[134], 100, 100, 100);
    tira.setPixelColor(Pantalla[135], 100, 100, 100);
    tira.setPixelColor(Pantalla[136], 100, 100, 100);
    tira.setPixelColor(Pantalla[137], 100, 100, 100);
    tira.setPixelColor(Pantalla[138], 100, 100, 100);
    tira.setPixelColor(Pantalla[139], 100, 100, 100);
    tira.setPixelColor(Pantalla[140], 100, 100, 100);
    tira.setPixelColor(Pantalla[141], 100, 100, 100);
    tira.setPixelColor(Pantalla[157], 100, 100, 100);
    tira.setPixelColor(Pantalla[156], 100, 100, 100);
    tira.setPixelColor(Pantalla[155], 100, 100, 100);
    tira.setPixelColor(Pantalla[154], 100, 100, 100);
    tira.setPixelColor(Pantalla[153], 100, 100, 100);
    tira.setPixelColor(Pantalla[150], 100, 100, 100);
    tira.setPixelColor(Pantalla[149], 100, 100, 100);
    tira.setPixelColor(Pantalla[148], 100, 100, 100);
    tira.setPixelColor(Pantalla[147], 100, 100, 100);
    tira.setPixelColor(Pantalla[146], 100, 100, 100);
    tira.setPixelColor(Pantalla[162], 100, 100, 100);
    tira.setPixelColor(Pantalla[163], 100, 100, 100);
    tira.setPixelColor(Pantalla[164], 100, 100, 100);
    tira.setPixelColor(Pantalla[165], 100, 100, 100);
    tira.setPixelColor(Pantalla[166], 100, 100, 100);
    tira.setPixelColor(Pantalla[167], 100, 100, 100);
    tira.setPixelColor(Pantalla[168], 100, 100, 100);
    tira.setPixelColor(Pantalla[169], 100, 100, 100);
    tira.setPixelColor(Pantalla[170], 100, 100, 100);
    tira.setPixelColor(Pantalla[171], 100, 100, 100);
    tira.setPixelColor(Pantalla[172], 100, 100, 100);
    tira.setPixelColor(Pantalla[173], 100, 100, 100);
    tira.setPixelColor(Pantalla[188], 100, 100, 100);
    tira.setPixelColor(Pantalla[187], 100, 100, 100);
    tira.setPixelColor(Pantalla[186], 100, 100, 100);
    tira.setPixelColor(Pantalla[181], 100, 100, 100);
    tira.setPixelColor(Pantalla[180], 100, 100, 100);
    tira.setPixelColor(Pantalla[179], 100, 100, 100);
    tira.setPixelColor(Pantalla[178], 100, 100, 100);
    tira.setPixelColor(Pantalla[195], 100, 100, 100);
    tira.setPixelColor(Pantalla[196], 100, 100, 100);
    tira.setPixelColor(Pantalla[197], 100, 100, 100);
    tira.setPixelColor(Pantalla[199], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[200], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[202], 100, 100, 100);
    tira.setPixelColor(Pantalla[203], 100, 100, 100);
    tira.setPixelColor(Pantalla[204], 100, 100, 100);
    tira.setPixelColor(Pantalla[219], 100, 100, 100);
    tira.setPixelColor(Pantalla[218], 100, 100, 100);
    tira.setPixelColor(Pantalla[216], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[215], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[213], 100, 100, 100);
    tira.setPixelColor(Pantalla[212], 100, 100, 100);
    tira.setPixelColor(Pantalla[229], 100, 100, 100);
    tira.setPixelColor(Pantalla[234], 100, 100, 100);
    tira.setPixelColor(Pantalla[249], 100, 100, 100);
    tira.setPixelColor(Pantalla[248], 100, 100, 100);
    tira.setPixelColor(Pantalla[247], 100, 100, 100);
    tira.setPixelColor(Pantalla[246], 100, 100, 100);
  }
  else
  {
    tira.setPixelColor(Pantalla[0], 0, 0, 0);
    tira.setPixelColor(Pantalla[1], 0, 0, 0);
    tira.setPixelColor(Pantalla[2], 0, 0, 0);
    tira.setPixelColor(Pantalla[3], 0, 0, 0);
    tira.setPixelColor(Pantalla[4], 0, 0, 0);
    tira.setPixelColor(Pantalla[5], 0, 0, 0);
    tira.setPixelColor(Pantalla[6], 0, 0, 0);
    tira.setPixelColor(Pantalla[7], 0, 0, 0);
    tira.setPixelColor(Pantalla[8], 0, 0, 0);
    tira.setPixelColor(Pantalla[9], 0, 0, 0);
    tira.setPixelColor(Pantalla[10], 0, 0, 0);
    tira.setPixelColor(Pantalla[11], 0, 0, 0);
    tira.setPixelColor(Pantalla[12], 0, 0, 0);
    tira.setPixelColor(Pantalla[13], 0, 0, 0);
    tira.setPixelColor(Pantalla[14], 0, 0, 0);
    tira.setPixelColor(Pantalla[15], 0, 0, 0);
    tira.setPixelColor(Pantalla[31], 0, 0, 0);
    tira.setPixelColor(Pantalla[30], 100, 100, 100);
    tira.setPixelColor(Pantalla[29], 100, 100, 100);
    tira.setPixelColor(Pantalla[28], 100, 100, 100);
    tira.setPixelColor(Pantalla[27], 100, 100, 100);
    tira.setPixelColor(Pantalla[26], 100, 100, 100);
    tira.setPixelColor(Pantalla[25], 0, 0, 0);
    tira.setPixelColor(Pantalla[24], 0, 0, 0);
    tira.setPixelColor(Pantalla[23], 0, 0, 0);
    tira.setPixelColor(Pantalla[22], 0, 0, 0);
    tira.setPixelColor(Pantalla[21], 100, 100, 100);
    tira.setPixelColor(Pantalla[20], 100, 100, 100);
    tira.setPixelColor(Pantalla[19], 100, 100, 100);
    tira.setPixelColor(Pantalla[18], 100, 100, 100);
    tira.setPixelColor(Pantalla[17], 100, 100, 100);
    tira.setPixelColor(Pantalla[16], 0, 0, 0);
    tira.setPixelColor(Pantalla[32], 100, 100, 100);
    tira.setPixelColor(Pantalla[33], 100, 100, 100);
    tira.setPixelColor(Pantalla[34], 100, 100, 100);
    tira.setPixelColor(Pantalla[35], 100, 100, 100);
    tira.setPixelColor(Pantalla[36], 100, 100, 100);
    tira.setPixelColor(Pantalla[37], 105, 210, 30);
    tira.setPixelColor(Pantalla[38], 100, 100, 100);
    tira.setPixelColor(Pantalla[39], 0, 0, 0);
    tira.setPixelColor(Pantalla[40], 0, 0, 0);
    tira.setPixelColor(Pantalla[41], 100, 100, 100);
    tira.setPixelColor(Pantalla[42], 105, 210, 30);
    tira.setPixelColor(Pantalla[43], 100, 100, 100);
    tira.setPixelColor(Pantalla[44], 100, 100, 100);
    tira.setPixelColor(Pantalla[45], 100, 100, 100);
    tira.setPixelColor(Pantalla[46], 100, 100, 100);
    tira.setPixelColor(Pantalla[47], 100, 100, 100);
    tira.setPixelColor(Pantalla[63], 100, 100, 100);
    tira.setPixelColor(Pantalla[62], 100, 100, 100);
    tira.setPixelColor(Pantalla[61], 100, 100, 100);
    tira.setPixelColor(Pantalla[60], 100, 100, 100);
    tira.setPixelColor(Pantalla[59], 100, 100, 100);
    tira.setPixelColor(Pantalla[58], 105, 210, 30);
    tira.setPixelColor(Pantalla[57], 100, 100, 100);
    tira.setPixelColor(Pantalla[56], 100, 100, 100);
    tira.setPixelColor(Pantalla[55], 100, 100, 100);
    tira.setPixelColor(Pantalla[54], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], 105, 210, 30);
    tira.setPixelColor(Pantalla[52], 100, 100, 100);
    tira.setPixelColor(Pantalla[51], 100, 100, 100);
    tira.setPixelColor(Pantalla[50], 100, 100, 100);
    tira.setPixelColor(Pantalla[49], 100, 100, 100);
    tira.setPixelColor(Pantalla[48], 100, 100, 100);
    tira.setPixelColor(Pantalla[64], 100, 100, 100);
    tira.setPixelColor(Pantalla[65], 100, 100, 100);
    tira.setPixelColor(Pantalla[66], 0, 0, 0);
    tira.setPixelColor(Pantalla[67], 0, 0, 0);
    tira.setPixelColor(Pantalla[68], 105, 210, 30);
    tira.setPixelColor(Pantalla[69], 100, 100, 100);
    tira.setPixelColor(Pantalla[70], 100, 100, 100);
    tira.setPixelColor(Pantalla[71], 100, 100, 100);
    tira.setPixelColor(Pantalla[72], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], 100, 100, 100);
    tira.setPixelColor(Pantalla[74], 100, 100, 100);
    tira.setPixelColor(Pantalla[75], 105, 210, 30);
    tira.setPixelColor(Pantalla[76], 0, 0, 0);
    tira.setPixelColor(Pantalla[77], 0, 0, 0);
    tira.setPixelColor(Pantalla[78], 100, 100, 100);
    tira.setPixelColor(Pantalla[79], 100, 100, 100);
    tira.setPixelColor(Pantalla[95], 100, 100, 100);
    tira.setPixelColor(Pantalla[94], 0, 0, 0);
    tira.setPixelColor(Pantalla[93], 0, 0, 0);
    tira.setPixelColor(Pantalla[92], 100, 100, 100);
    tira.setPixelColor(Pantalla[91], 100, 100, 100);
    tira.setPixelColor(Pantalla[90], 100, 100, 100);
    tira.setPixelColor(Pantalla[89], 100, 100, 100);
    tira.setPixelColor(Pantalla[88], 100, 100, 100);
    tira.setPixelColor(Pantalla[87], 100, 100, 100);
    tira.setPixelColor(Pantalla[86], 100, 100, 100);
    tira.setPixelColor(Pantalla[85], 100, 100, 100);
    tira.setPixelColor(Pantalla[84], 100, 100, 100);
    tira.setPixelColor(Pantalla[83], 100, 100, 100);
    tira.setPixelColor(Pantalla[82], 0, 0, 0);
    tira.setPixelColor(Pantalla[81], 0, 0, 0);
    tira.setPixelColor(Pantalla[80], 100, 100, 100);
    tira.setPixelColor(Pantalla[98], 100, 100, 100);
    tira.setPixelColor(Pantalla[99], 100, 100, 100);
    tira.setPixelColor(Pantalla[100], 100, 100, 100);
    tira.setPixelColor(Pantalla[101], 100, 100, 100);
    tira.setPixelColor(Pantalla[102], 100, 100, 100);
    tira.setPixelColor(Pantalla[103], 100, 100, 100);
    tira.setPixelColor(Pantalla[104], 100, 100, 100);
    tira.setPixelColor(Pantalla[105], 100, 100, 100);
    tira.setPixelColor(Pantalla[106], 100, 100, 100);
    tira.setPixelColor(Pantalla[107], 100, 100, 100);
    tira.setPixelColor(Pantalla[108], 100, 100, 100);
    tira.setPixelColor(Pantalla[109], 100, 100, 100);
    tira.setPixelColor(Pantalla[125], 100, 100, 100);
    tira.setPixelColor(Pantalla[124], 100, 100, 100);
    tira.setPixelColor(Pantalla[123], 100, 100, 100);
    tira.setPixelColor(Pantalla[122], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[121], 100, 100, 100);
    tira.setPixelColor(Pantalla[120], 100, 100, 100);
    tira.setPixelColor(Pantalla[119], 100, 100, 100);
    tira.setPixelColor(Pantalla[118], 100, 100, 100);
    tira.setPixelColor(Pantalla[117], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[116], 100, 100, 100);
    tira.setPixelColor(Pantalla[115], 100, 100, 100);
    tira.setPixelColor(Pantalla[114], 100, 100, 100);
    tira.setPixelColor(Pantalla[130], 100, 100, 100);
    tira.setPixelColor(Pantalla[131], 100, 100, 100);
    tira.setPixelColor(Pantalla[132], 100, 100, 100);
    tira.setPixelColor(Pantalla[133], 100, 100, 100);
    tira.setPixelColor(Pantalla[134], 100, 100, 100);
    tira.setPixelColor(Pantalla[135], 100, 100, 100);
    tira.setPixelColor(Pantalla[136], 100, 100, 100);
    tira.setPixelColor(Pantalla[137], 100, 100, 100);
    tira.setPixelColor(Pantalla[138], 100, 100, 100);
    tira.setPixelColor(Pantalla[139], 100, 100, 100);
    tira.setPixelColor(Pantalla[140], 100, 100, 100);
    tira.setPixelColor(Pantalla[141], 100, 100, 100);
    tira.setPixelColor(Pantalla[157], 100, 100, 100);
    tira.setPixelColor(Pantalla[156], 100, 100, 100);
    tira.setPixelColor(Pantalla[155], 100, 100, 100);
    tira.setPixelColor(Pantalla[154], 100, 100, 100);
    tira.setPixelColor(Pantalla[153], 100, 100, 100);
    tira.setPixelColor(Pantalla[150], 100, 100, 100);
    tira.setPixelColor(Pantalla[149], 100, 100, 100);
    tira.setPixelColor(Pantalla[148], 100, 100, 100);
    tira.setPixelColor(Pantalla[147], 100, 100, 100);
    tira.setPixelColor(Pantalla[146], 100, 100, 100);
    tira.setPixelColor(Pantalla[162], 100, 100, 100);
    tira.setPixelColor(Pantalla[163], 100, 100, 100);
    tira.setPixelColor(Pantalla[164], 100, 100, 100);
    tira.setPixelColor(Pantalla[165], 100, 100, 100);
    tira.setPixelColor(Pantalla[166], 100, 100, 100);
    tira.setPixelColor(Pantalla[167], 100, 100, 100);
    tira.setPixelColor(Pantalla[168], 100, 100, 100);
    tira.setPixelColor(Pantalla[169], 100, 100, 100);
    tira.setPixelColor(Pantalla[170], 100, 100, 100);
    tira.setPixelColor(Pantalla[171], 100, 100, 100);
    tira.setPixelColor(Pantalla[172], 100, 100, 100);
    tira.setPixelColor(Pantalla[173], 100, 100, 100);
    tira.setPixelColor(Pantalla[188], 100, 100, 100);
    tira.setPixelColor(Pantalla[187], 100, 100, 100);
    tira.setPixelColor(Pantalla[186], 100, 100, 100);
    tira.setPixelColor(Pantalla[181], 100, 100, 100);
    tira.setPixelColor(Pantalla[180], 100, 100, 100);
    tira.setPixelColor(Pantalla[179], 100, 100, 100);
    tira.setPixelColor(Pantalla[178], 100, 100, 100);
    tira.setPixelColor(Pantalla[195], 100, 100, 100);
    tira.setPixelColor(Pantalla[196], 100, 100, 100);
    tira.setPixelColor(Pantalla[197], 100, 100, 100);
    tira.setPixelColor(Pantalla[199], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[200], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[202], 100, 100, 100);
    tira.setPixelColor(Pantalla[203], 100, 100, 100);
    tira.setPixelColor(Pantalla[204], 100, 100, 100);
    tira.setPixelColor(Pantalla[219], 100, 100, 100);
    tira.setPixelColor(Pantalla[218], 100, 100, 100);
    tira.setPixelColor(Pantalla[216], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[215], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[213], 100, 100, 100);
    tira.setPixelColor(Pantalla[212], 100, 100, 100);
    tira.setPixelColor(Pantalla[229], 100, 100, 100);
    tira.setPixelColor(Pantalla[234], 100, 100, 100);
    tira.setPixelColor(Pantalla[249], 100, 100, 100);
    tira.setPixelColor(Pantalla[248], 100, 100, 100);
    tira.setPixelColor(Pantalla[247], 100, 100, 100);
    tira.setPixelColor(Pantalla[246], 100, 100, 100);
  }
}

void Matriz_Zorra()
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

void Matriz_Perra()
{
  tira.setBrightness(30);
  if (lado == IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[1], 0, 0, 0);
    tira.setPixelColor(Pantalla[2], 0, 0, 0);
    tira.setPixelColor(Pantalla[3], 0, 0, 0);
    tira.setPixelColor(Pantalla[4], 255, 255, 0);
    tira.setPixelColor(Pantalla[11], 255, 255, 0);
    tira.setPixelColor(Pantalla[12], 0, 0, 0);
    tira.setPixelColor(Pantalla[13], 0, 0, 0);
    tira.setPixelColor(Pantalla[14], 0, 0, 0);
    tira.setPixelColor(Pantalla[15], 0, 0, 0);
    tira.setPixelColor(Pantalla[31], 0, 0, 0);
    tira.setPixelColor(Pantalla[30], 0, 0, 0);
    tira.setPixelColor(Pantalla[29], 0, 0, 0);
    tira.setPixelColor(Pantalla[28], 0, 0, 0);
    tira.setPixelColor(Pantalla[27], 127, 255, 80);
    tira.setPixelColor(Pantalla[26], 255, 255, 0);
    tira.setPixelColor(Pantalla[21], 255, 255, 0);
    tira.setPixelColor(Pantalla[20], 127, 255, 80);
    tira.setPixelColor(Pantalla[19], 0, 0, 0);
    tira.setPixelColor(Pantalla[18], 0, 0, 0);
    tira.setPixelColor(Pantalla[17], 0, 0, 0);
    tira.setPixelColor(Pantalla[16], 0, 0, 0);
    tira.setPixelColor(Pantalla[32], 0, 0, 0);
    tira.setPixelColor(Pantalla[33], 0, 0, 0);
    tira.setPixelColor(Pantalla[34], 0, 0, 0);
    tira.setPixelColor(Pantalla[35], 0, 0, 0);
    tira.setPixelColor(Pantalla[36], 127, 255, 80);
    tira.setPixelColor(Pantalla[37], 255, 255, 0);
    tira.setPixelColor(Pantalla[38], 255, 255, 0);
    tira.setPixelColor(Pantalla[39], 255, 255, 0);
    tira.setPixelColor(Pantalla[40], 255, 255, 0);
    tira.setPixelColor(Pantalla[41], 255, 255, 0);
    tira.setPixelColor(Pantalla[42], 255, 255, 0);
    tira.setPixelColor(Pantalla[43], 127, 255, 80);
    tira.setPixelColor(Pantalla[44], 0, 0, 0);
    tira.setPixelColor(Pantalla[45], 0, 0, 0);
    tira.setPixelColor(Pantalla[46], 0, 0, 0);
    tira.setPixelColor(Pantalla[47], 0, 0, 0);
    tira.setPixelColor(Pantalla[63], 0, 0, 0);
    tira.setPixelColor(Pantalla[62], 0, 0, 0);
    tira.setPixelColor(Pantalla[61], 0, 0, 0);
    tira.setPixelColor(Pantalla[60], 0, 0, 0);
    tira.setPixelColor(Pantalla[59], 255, 255, 0);
    tira.setPixelColor(Pantalla[58], 255, 255, 0);
    tira.setPixelColor(Pantalla[57], 255, 255, 0);
    tira.setPixelColor(Pantalla[56], 255, 255, 0);
    tira.setPixelColor(Pantalla[55], 255, 255, 0);
    tira.setPixelColor(Pantalla[54], 255, 255, 0);
    tira.setPixelColor(Pantalla[53], 255, 255, 0);
    tira.setPixelColor(Pantalla[52], 255, 255, 0);
    tira.setPixelColor(Pantalla[51], 0, 0, 0);
    tira.setPixelColor(Pantalla[50], 0, 0, 0);
    tira.setPixelColor(Pantalla[49], 0, 0, 0);
    tira.setPixelColor(Pantalla[48], 0, 0, 0);
    tira.setPixelColor(Pantalla[64], 0, 0, 0);
    tira.setPixelColor(Pantalla[65], 0, 0, 0);
    tira.setPixelColor(Pantalla[66], 255, 255, 0);
    tira.setPixelColor(Pantalla[68], 255, 255, 0);
    tira.setPixelColor(Pantalla[69], 255, 255, 0);
    tira.setPixelColor(Pantalla[70], 255, 255, 0);
    tira.setPixelColor(Pantalla[71], 255, 255, 0);
    tira.setPixelColor(Pantalla[72], 255, 255, 0);
    tira.setPixelColor(Pantalla[73], 255, 255, 0);
    tira.setPixelColor(Pantalla[74], 255, 255, 0);
    tira.setPixelColor(Pantalla[75], 255, 255, 0);
    tira.setPixelColor(Pantalla[95], 0, 0, 0);
    tira.setPixelColor(Pantalla[94], 255, 255, 0);
    tira.setPixelColor(Pantalla[93], 255, 255, 0);
    tira.setPixelColor(Pantalla[91], 255, 255, 0);
    tira.setPixelColor(Pantalla[90], 0, 255, 0);
    tira.setPixelColor(Pantalla[89], 255, 255, 0);
    tira.setPixelColor(Pantalla[88], 255, 255, 0);
    tira.setPixelColor(Pantalla[87], 255, 255, 0);
    tira.setPixelColor(Pantalla[86], 255, 255, 0);
    tira.setPixelColor(Pantalla[85], 0, 255, 0);
    tira.setPixelColor(Pantalla[84], 255, 255, 0);
    tira.setPixelColor(Pantalla[96], 255, 255, 0);
    tira.setPixelColor(Pantalla[97], 255, 255, 0);
    tira.setPixelColor(Pantalla[98], 255, 255, 0);
    tira.setPixelColor(Pantalla[100], 100, 100, 100);
    tira.setPixelColor(Pantalla[101], 100, 100, 100);
    tira.setPixelColor(Pantalla[102], 100, 100, 100);
    tira.setPixelColor(Pantalla[103], 0, 0, 0);
    tira.setPixelColor(Pantalla[104], 0, 0, 0);
    tira.setPixelColor(Pantalla[105], 100, 100, 100);
    tira.setPixelColor(Pantalla[106], 100, 100, 100);
    tira.setPixelColor(Pantalla[107], 100, 100, 100);
    tira.setPixelColor(Pantalla[127], 255, 255, 0);
    tira.setPixelColor(Pantalla[126], 255, 255, 0);
    tira.setPixelColor(Pantalla[125], 255, 255, 0);
    tira.setPixelColor(Pantalla[124], 100, 100, 100);
    tira.setPixelColor(Pantalla[123], 100, 100, 100);
    tira.setPixelColor(Pantalla[122], 100, 100, 100);
    tira.setPixelColor(Pantalla[121], 100, 100, 100);
    tira.setPixelColor(Pantalla[120], 100, 100, 100);
    tira.setPixelColor(Pantalla[119], 100, 100, 100);
    tira.setPixelColor(Pantalla[118], 100, 100, 100);
    tira.setPixelColor(Pantalla[117], 100, 100, 100);
    tira.setPixelColor(Pantalla[116], 100, 100, 100);
    tira.setPixelColor(Pantalla[115], 100, 100, 100);
    tira.setPixelColor(Pantalla[128], 255, 255, 0);
    tira.setPixelColor(Pantalla[129], 255, 255, 0);
    tira.setPixelColor(Pantalla[130], 255, 255, 0);
    tira.setPixelColor(Pantalla[132], 100, 100, 100);
    tira.setPixelColor(Pantalla[133], 100, 100, 100);
    tira.setPixelColor(Pantalla[134], 100, 100, 100);
    tira.setPixelColor(Pantalla[135], 100, 100, 100);
    tira.setPixelColor(Pantalla[136], 100, 100, 100);
    tira.setPixelColor(Pantalla[137], 100, 100, 100);
    tira.setPixelColor(Pantalla[138], 100, 100, 100);
    tira.setPixelColor(Pantalla[139], 100, 100, 100);
    tira.setPixelColor(Pantalla[159], 0, 0, 0);
    tira.setPixelColor(Pantalla[158], 255, 255, 0);
    tira.setPixelColor(Pantalla[157], 255, 255, 0);
    tira.setPixelColor(Pantalla[156], 255, 255, 0);
    tira.setPixelColor(Pantalla[154], 100, 100, 100);
    tira.setPixelColor(Pantalla[153], 100, 100, 100);
    tira.setPixelColor(Pantalla[152], 100, 100, 100);
    tira.setPixelColor(Pantalla[151], 100, 100, 100);
    tira.setPixelColor(Pantalla[150], 100, 100, 100);
    tira.setPixelColor(Pantalla[149], 100, 100, 100);
    tira.setPixelColor(Pantalla[161], 0, 0, 0);
    tira.setPixelColor(Pantalla[162], 255, 255, 0);
    tira.setPixelColor(Pantalla[163], 255, 255, 0);
    tira.setPixelColor(Pantalla[164], 255, 255, 0);
    tira.setPixelColor(Pantalla[165], 255, 255, 0);
    tira.setPixelColor(Pantalla[166], 255, 255, 0);
    tira.setPixelColor(Pantalla[167], 255, 255, 0);
    tira.setPixelColor(Pantalla[168], 255, 255, 0);
    tira.setPixelColor(Pantalla[169], 255, 255, 0);
    tira.setPixelColor(Pantalla[170], 255, 255, 0);
    tira.setPixelColor(Pantalla[171], 255, 255, 0);
    tira.setPixelColor(Pantalla[190], 0, 0, 0);
    tira.setPixelColor(Pantalla[189], 0, 0, 0);
    tira.setPixelColor(Pantalla[188], 0, 0, 0);
    tira.setPixelColor(Pantalla[187], 255, 255, 0);
    tira.setPixelColor(Pantalla[186], 100, 100, 100);
    tira.setPixelColor(Pantalla[185], 100, 100, 100);
    tira.setPixelColor(Pantalla[184], 100, 100, 100);
    tira.setPixelColor(Pantalla[183], 100, 100, 100);
    tira.setPixelColor(Pantalla[182], 100, 100, 100);
    tira.setPixelColor(Pantalla[181], 100, 100, 100);
    tira.setPixelColor(Pantalla[180], 255, 255, 0);
    tira.setPixelColor(Pantalla[193], 0, 0, 0);
    tira.setPixelColor(Pantalla[194], 0, 0, 0);
    tira.setPixelColor(Pantalla[195], 0, 0, 0);
    tira.setPixelColor(Pantalla[196], 255, 255, 0);
    tira.setPixelColor(Pantalla[197], 100, 100, 100);
    tira.setPixelColor(Pantalla[202], 100, 100, 100);
    tira.setPixelColor(Pantalla[203], 255, 255, 0);
    tira.setPixelColor(Pantalla[222], 0, 0, 0);
    tira.setPixelColor(Pantalla[221], 0, 0, 0);
    tira.setPixelColor(Pantalla[220], 0, 0, 0);
    tira.setPixelColor(Pantalla[219], 255, 255, 0);
    tira.setPixelColor(Pantalla[218], 100, 100, 100);
    tira.setPixelColor(Pantalla[216], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[215], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[213], 100, 100, 100);
    tira.setPixelColor(Pantalla[212], 255, 255, 0);
    tira.setPixelColor(Pantalla[225], 0, 0, 0);
    tira.setPixelColor(Pantalla[226], 0, 0, 0);
    tira.setPixelColor(Pantalla[227], 0, 0, 0);
    tira.setPixelColor(Pantalla[228], 255, 255, 0);
    tira.setPixelColor(Pantalla[229], 100, 100, 100);
    tira.setPixelColor(Pantalla[231], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[232], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[234], 100, 100, 100);
    tira.setPixelColor(Pantalla[235], 255, 255, 0);
  }
  else
  {
    tira.setPixelColor(Pantalla[2], 0, 0, 0);
    tira.setPixelColor(Pantalla[3], 0, 0, 0);
    tira.setPixelColor(Pantalla[4], 0, 0, 0);
    tira.setPixelColor(Pantalla[11], 0, 0, 0);
    tira.setPixelColor(Pantalla[12], 0, 0, 0);
    tira.setPixelColor(Pantalla[13], 0, 0, 0);
    tira.setPixelColor(Pantalla[30], 0, 0, 0);
    tira.setPixelColor(Pantalla[29], 255, 255, 0);
    tira.setPixelColor(Pantalla[28], 255, 255, 0);
    tira.setPixelColor(Pantalla[27], 255, 255, 0);
    tira.setPixelColor(Pantalla[26], 255, 255, 0);
    tira.setPixelColor(Pantalla[21], 255, 255, 0);
    tira.setPixelColor(Pantalla[20], 255, 255, 0);
    tira.setPixelColor(Pantalla[19], 255, 255, 0);
    tira.setPixelColor(Pantalla[18], 255, 255, 0);
    tira.setPixelColor(Pantalla[17], 0, 0, 0);
    tira.setPixelColor(Pantalla[33], 255, 255, 0);
    tira.setPixelColor(Pantalla[34], 255, 255, 0);
    tira.setPixelColor(Pantalla[35], 255, 255, 0);
    tira.setPixelColor(Pantalla[36], 127, 255, 80);
    tira.setPixelColor(Pantalla[37], 255, 255, 0);
    tira.setPixelColor(Pantalla[38], 255, 255, 0);
    tira.setPixelColor(Pantalla[39], 255, 255, 0);
    tira.setPixelColor(Pantalla[40], 255, 255, 0);
    tira.setPixelColor(Pantalla[41], 255, 255, 0);
    tira.setPixelColor(Pantalla[42], 255, 255, 0);
    tira.setPixelColor(Pantalla[43], 127, 255, 80);
    tira.setPixelColor(Pantalla[44], 255, 255, 0);
    tira.setPixelColor(Pantalla[45], 255, 255, 0);
    tira.setPixelColor(Pantalla[46], 255, 255, 0);
    tira.setPixelColor(Pantalla[62], 255, 255, 0);
    tira.setPixelColor(Pantalla[61], 0, 0, 0);
    tira.setPixelColor(Pantalla[60], 0, 0, 0);
    tira.setPixelColor(Pantalla[59], 255, 255, 0);
    tira.setPixelColor(Pantalla[58], 255, 255, 0);
    tira.setPixelColor(Pantalla[57], 255, 255, 0);
    tira.setPixelColor(Pantalla[56], 255, 255, 0);
    tira.setPixelColor(Pantalla[55], 255, 255, 0);
    tira.setPixelColor(Pantalla[54], 255, 255, 0);
    tira.setPixelColor(Pantalla[53], 255, 255, 0);
    tira.setPixelColor(Pantalla[52], 255, 255, 0);
    tira.setPixelColor(Pantalla[51], 0, 0, 0);
    tira.setPixelColor(Pantalla[50], 0, 0, 0);
    tira.setPixelColor(Pantalla[49], 255, 255, 0);
    tira.setPixelColor(Pantalla[64], 0, 0, 0);
    tira.setPixelColor(Pantalla[65], 0, 0, 0);
    tira.setPixelColor(Pantalla[66], 0, 0, 0);
    tira.setPixelColor(Pantalla[67], 0, 0, 0);
    tira.setPixelColor(Pantalla[68], 255, 255, 0);
    tira.setPixelColor(Pantalla[69], 255, 255, 0);
    tira.setPixelColor(Pantalla[70], 255, 255, 0);
    tira.setPixelColor(Pantalla[71], 255, 255, 0);
    tira.setPixelColor(Pantalla[72], 255, 255, 0);
    tira.setPixelColor(Pantalla[73], 255, 255, 0);
    tira.setPixelColor(Pantalla[74], 255, 255, 0);
    tira.setPixelColor(Pantalla[75], 255, 255, 0);
    tira.setPixelColor(Pantalla[95], 0, 0, 0);
    tira.setPixelColor(Pantalla[94], 0, 0, 0);
    tira.setPixelColor(Pantalla[93], 0, 0, 0);
    tira.setPixelColor(Pantalla[92], 0, 0, 0);
    tira.setPixelColor(Pantalla[91], 255, 255, 0);
    tira.setPixelColor(Pantalla[90], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[89], 255, 255, 0);
    tira.setPixelColor(Pantalla[88], 255, 255, 0);
    tira.setPixelColor(Pantalla[87], 255, 255, 0);
    tira.setPixelColor(Pantalla[86], 255, 255, 0);
    tira.setPixelColor(Pantalla[85], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[84], 255, 255, 0);
    tira.setPixelColor(Pantalla[96], 0, 0, 0);
    tira.setPixelColor(Pantalla[97], 0, 0, 0);
    tira.setPixelColor(Pantalla[98], 0, 0, 0);
    tira.setPixelColor(Pantalla[99], 0, 0, 0);
    tira.setPixelColor(Pantalla[100], 100, 100, 100);
    tira.setPixelColor(Pantalla[101], 100, 100, 100);
    tira.setPixelColor(Pantalla[102], 100, 100, 100);
    tira.setPixelColor(Pantalla[103], 0, 0, 0);
    tira.setPixelColor(Pantalla[104], 0, 0, 0);
    tira.setPixelColor(Pantalla[105], 100, 100, 100);
    tira.setPixelColor(Pantalla[106], 100, 100, 100);
    tira.setPixelColor(Pantalla[107], 100, 100, 100);
    tira.setPixelColor(Pantalla[127], 0, 0, 0);
    tira.setPixelColor(Pantalla[126], 0, 0, 0);
    tira.setPixelColor(Pantalla[125], 0, 0, 0);
    tira.setPixelColor(Pantalla[124], 100, 100, 100);
    tira.setPixelColor(Pantalla[123], 100, 100, 100);
    tira.setPixelColor(Pantalla[122], 100, 100, 100);
    tira.setPixelColor(Pantalla[121], 100, 100, 100);
    tira.setPixelColor(Pantalla[120], 100, 100, 100);
    tira.setPixelColor(Pantalla[119], 100, 100, 100);
    tira.setPixelColor(Pantalla[118], 100, 100, 100);
    tira.setPixelColor(Pantalla[117], 100, 100, 100);
    tira.setPixelColor(Pantalla[116], 100, 100, 100);
    tira.setPixelColor(Pantalla[115], 100, 100, 100);
    tira.setPixelColor(Pantalla[128], 0, 0, 0);
    tira.setPixelColor(Pantalla[129], 0, 0, 0);
    tira.setPixelColor(Pantalla[130], 0, 0, 0);
    tira.setPixelColor(Pantalla[131], 0, 0, 0);
    tira.setPixelColor(Pantalla[132], 100, 100, 100);
    tira.setPixelColor(Pantalla[133], 100, 100, 100);
    tira.setPixelColor(Pantalla[134], 100, 100, 100);
    tira.setPixelColor(Pantalla[135], 100, 100, 100);
    tira.setPixelColor(Pantalla[136], 100, 100, 100);
    tira.setPixelColor(Pantalla[137], 100, 100, 100);
    tira.setPixelColor(Pantalla[138], 100, 100, 100);
    tira.setPixelColor(Pantalla[139], 100, 100, 100);
    tira.setPixelColor(Pantalla[159], 0, 0, 0);
    tira.setPixelColor(Pantalla[158], 255, 255, 0);
    tira.setPixelColor(Pantalla[157], 255, 255, 0);
    tira.setPixelColor(Pantalla[156], 255, 255, 0);
    tira.setPixelColor(Pantalla[154], 100, 100, 100);
    tira.setPixelColor(Pantalla[153], 100, 100, 100);
    tira.setPixelColor(Pantalla[152], 100, 100, 100);
    tira.setPixelColor(Pantalla[151], 100, 100, 100);
    tira.setPixelColor(Pantalla[150], 100, 100, 100);
    tira.setPixelColor(Pantalla[149], 100, 100, 100);
    tira.setPixelColor(Pantalla[160], 0, 0, 0);
    tira.setPixelColor(Pantalla[161], 255, 255, 0);
    tira.setPixelColor(Pantalla[162], 255, 255, 0);
    tira.setPixelColor(Pantalla[163], 255, 255, 0);
    tira.setPixelColor(Pantalla[164], 255, 255, 0);
    tira.setPixelColor(Pantalla[165], 255, 255, 0);
    tira.setPixelColor(Pantalla[166], 255, 255, 0);
    tira.setPixelColor(Pantalla[167], 255, 255, 0);
    tira.setPixelColor(Pantalla[168], 255, 255, 0);
    tira.setPixelColor(Pantalla[169], 255, 255, 0);
    tira.setPixelColor(Pantalla[170], 255, 255, 0);
    tira.setPixelColor(Pantalla[171], 255, 255, 0);
    tira.setPixelColor(Pantalla[191], 255, 255, 0);
    tira.setPixelColor(Pantalla[190], 255, 255, 0);
    tira.setPixelColor(Pantalla[189], 0, 0, 0);
    tira.setPixelColor(Pantalla[188], 0, 0, 0);
    tira.setPixelColor(Pantalla[187], 255, 255, 0);
    tira.setPixelColor(Pantalla[186], 100, 100, 100);
    tira.setPixelColor(Pantalla[185], 100, 100, 100);
    tira.setPixelColor(Pantalla[184], 100, 100, 100);
    tira.setPixelColor(Pantalla[183], 100, 100, 100);
    tira.setPixelColor(Pantalla[182], 100, 100, 100);
    tira.setPixelColor(Pantalla[181], 100, 100, 100);
    tira.setPixelColor(Pantalla[180], 255, 255, 0);
    tira.setPixelColor(Pantalla[192], 255, 255, 0);
    tira.setPixelColor(Pantalla[193], 255, 255, 0);
    tira.setPixelColor(Pantalla[194], 0, 0, 0);
    tira.setPixelColor(Pantalla[195], 0, 0, 0);
    tira.setPixelColor(Pantalla[196], 255, 255, 0);
    tira.setPixelColor(Pantalla[197], 100, 100, 100);
    tira.setPixelColor(Pantalla[202], 100, 100, 100);
    tira.setPixelColor(Pantalla[203], 255, 255, 0);
    tira.setPixelColor(Pantalla[223], 255, 255, 0);
    tira.setPixelColor(Pantalla[222], 0, 0, 0);
    tira.setPixelColor(Pantalla[221], 0, 0, 0);
    tira.setPixelColor(Pantalla[220], 0, 0, 0);
    tira.setPixelColor(Pantalla[219], 255, 255, 0);
    tira.setPixelColor(Pantalla[218], 100, 100, 100);
    tira.setPixelColor(Pantalla[216], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[215], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[213], 100, 100, 100);
    tira.setPixelColor(Pantalla[212], 255, 255, 0);
    tira.setPixelColor(Pantalla[225], 0, 0, 0);
    tira.setPixelColor(Pantalla[226], 0, 0, 0);
    tira.setPixelColor(Pantalla[227], 0, 0, 0);
    tira.setPixelColor(Pantalla[228], 255, 255, 0);
    tira.setPixelColor(Pantalla[229], 100, 100, 100);
    tira.setPixelColor(Pantalla[231], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[232], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[234], 100, 100, 100);
    tira.setPixelColor(Pantalla[235], 255, 255, 0);
  }
}

void Matriz_Arana()
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

void Matriz_Lagartija()
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

void Matriz_Mariposa()
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

void Matriz_TimeOut()
{
  tira.setPixelColor(Pantalla[29], 0, 255, 0);
  tira.setPixelColor(Pantalla[18], 0, 255, 0);
  tira.setPixelColor(Pantalla[35], 0, 255, 0);
  tira.setPixelColor(Pantalla[44], 0, 255, 0);
  tira.setPixelColor(Pantalla[59], 0, 255, 0);
  tira.setPixelColor(Pantalla[52], 0, 255, 0);
  tira.setPixelColor(Pantalla[69], 0, 255, 0);
  tira.setPixelColor(Pantalla[74], 0, 255, 0);
  tira.setPixelColor(Pantalla[89], 0, 255, 0);
  tira.setPixelColor(Pantalla[86], 0, 255, 0);
  tira.setPixelColor(Pantalla[103], 0, 255, 0);
  tira.setPixelColor(Pantalla[104], 0, 255, 0);
  tira.setPixelColor(Pantalla[120], 0, 255, 0);
  tira.setPixelColor(Pantalla[119], 0, 255, 0);
  tira.setPixelColor(Pantalla[134], 0, 255, 0);
  tira.setPixelColor(Pantalla[137], 0, 255, 0);
  tira.setPixelColor(Pantalla[154], 0, 255, 0);
  tira.setPixelColor(Pantalla[149], 0, 255, 0);
  tira.setPixelColor(Pantalla[164], 0, 255, 0);
  tira.setPixelColor(Pantalla[171], 0, 255, 0);
  tira.setPixelColor(Pantalla[188], 0, 255, 0);
  tira.setPixelColor(Pantalla[179], 0, 255, 0);
  tira.setPixelColor(Pantalla[194], 0, 255, 0);
  tira.setPixelColor(Pantalla[205], 0, 255, 0);
}

void Matriz_Corazon()
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

void Matriz_Ilumina_Mira_Aleatorio()
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

void Matriz_Oso()
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

void Matriz_Rana()
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

void Matriz_Unicornio()
{
  if (lado == IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[0], 0, 255, 0);
    tira.setPixelColor(Pantalla[1], 0, 255, 0);
    tira.setPixelColor(Pantalla[2], 0, 0, 0);
    tira.setPixelColor(Pantalla[3], 0, 0, 0);
    tira.setPixelColor(Pantalla[4], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[5], 0, 255, 0);
    tira.setPixelColor(Pantalla[6], 0, 0, 0);
    tira.setPixelColor(Pantalla[7], 0, 0, 0);
    tira.setPixelColor(Pantalla[8], 0, 255, 0);
    tira.setPixelColor(Pantalla[9], 0, 255, 0);
    tira.setPixelColor(Pantalla[10], 0, 0, 0);
    tira.setPixelColor(Pantalla[11], 0, 0, 0);
    tira.setPixelColor(Pantalla[12], 0, 255, 0);
    tira.setPixelColor(Pantalla[13], 0, 255, 0);
    tira.setPixelColor(Pantalla[14], 0, 0, 0);
    tira.setPixelColor(Pantalla[15], 0, 0, 0);
    tira.setPixelColor(Pantalla[31], 0, 0, 0);
    tira.setPixelColor(Pantalla[27], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[16], 0, 255, 0);
    tira.setPixelColor(Pantalla[32], 0, 0, 0);
    tira.setPixelColor(Pantalla[35], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[36], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[37], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[43], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[44], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[47], 0, 255, 0);
    tira.setPixelColor(Pantalla[63], 0, 255, 0);
    tira.setPixelColor(Pantalla[61], 100, 100, 100);
    tira.setPixelColor(Pantalla[60], 100, 100, 100);
    tira.setPixelColor(Pantalla[59], 100, 100, 100);
    tira.setPixelColor(Pantalla[58], 100, 100, 100);
    tira.setPixelColor(Pantalla[57], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[51], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[48], 0, 0, 0);
    tira.setPixelColor(Pantalla[64], 0, 255, 0);
    tira.setPixelColor(Pantalla[66], 100, 100, 100);
    tira.setPixelColor(Pantalla[67], 0, 0, 255);
    tira.setPixelColor(Pantalla[68], 100, 100, 100);
    tira.setPixelColor(Pantalla[69], 100, 100, 100);
    tira.setPixelColor(Pantalla[70], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[74], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[75], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[76], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[77], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[78], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[79], 0, 0, 0);
    tira.setPixelColor(Pantalla[95], 0, 0, 0);
    tira.setPixelColor(Pantalla[94], 100, 100, 100);
    tira.setPixelColor(Pantalla[93], 100, 100, 100);
    tira.setPixelColor(Pantalla[92], 0, 0, 255);
    tira.setPixelColor(Pantalla[91], 100, 100, 100);
    tira.setPixelColor(Pantalla[90], 100, 100, 100);
    tira.setPixelColor(Pantalla[89], 100, 100, 100);
    tira.setPixelColor(Pantalla[86], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[85], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[84], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[83], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[82], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[81], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[80], 0, 255, 0);
    tira.setPixelColor(Pantalla[96], 0, 0, 0);
    tira.setPixelColor(Pantalla[97], 100, 100, 100);
    tira.setPixelColor(Pantalla[98], 100, 100, 100);
    tira.setPixelColor(Pantalla[99], 100, 100, 100);
    tira.setPixelColor(Pantalla[100], 100, 100, 100);
    tira.setPixelColor(Pantalla[101], 100, 100, 100);
    tira.setPixelColor(Pantalla[102], 100, 100, 100);
    tira.setPixelColor(Pantalla[105], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[106], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[107], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[109], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[110], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[111], 0, 255, 0);
    tira.setPixelColor(Pantalla[127], 0, 255, 0);
    tira.setPixelColor(Pantalla[123], 255, 0, 255);
    tira.setPixelColor(Pantalla[122], 255, 255, 0);
    tira.setPixelColor(Pantalla[121], 0, 255, 0);
    tira.setPixelColor(Pantalla[117], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[116], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[113], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[112], 0, 0, 0);
    tira.setPixelColor(Pantalla[128], 0, 255, 0);
    tira.setPixelColor(Pantalla[132], 0, 0, 255);
    tira.setPixelColor(Pantalla[133], 100, 100, 100);
    tira.setPixelColor(Pantalla[134], 100, 100, 100);
    tira.setPixelColor(Pantalla[135], 100, 100, 100);
    tira.setPixelColor(Pantalla[136], 100, 100, 100);
    tira.setPixelColor(Pantalla[137], 100, 100, 100);
    tira.setPixelColor(Pantalla[138], 105, 210, 30);
    tira.setPixelColor(Pantalla[143], 0, 0, 0);
    tira.setPixelColor(Pantalla[159], 0, 0, 0);
    tira.setPixelColor(Pantalla[155], 100, 100, 100);
    tira.setPixelColor(Pantalla[154], 100, 100, 100);
    tira.setPixelColor(Pantalla[153], 100, 100, 100);
    tira.setPixelColor(Pantalla[152], 100, 100, 100);
    tira.setPixelColor(Pantalla[151], 100, 100, 100);
    tira.setPixelColor(Pantalla[150], 100, 100, 100);
    tira.setPixelColor(Pantalla[149], 100, 100, 100);
    tira.setPixelColor(Pantalla[144], 0, 255, 0);
    tira.setPixelColor(Pantalla[160], 0, 0, 0);
    tira.setPixelColor(Pantalla[164], 100, 100, 100);
    tira.setPixelColor(Pantalla[165], 100, 100, 100);
    tira.setPixelColor(Pantalla[166], 100, 100, 100);
    tira.setPixelColor(Pantalla[167], 100, 100, 100);
    tira.setPixelColor(Pantalla[168], 100, 100, 100);
    tira.setPixelColor(Pantalla[169], 100, 100, 100);
    tira.setPixelColor(Pantalla[170], 100, 100, 100);
    tira.setPixelColor(Pantalla[175], 0, 255, 0);
    tira.setPixelColor(Pantalla[191], 0, 255, 0);
    tira.setPixelColor(Pantalla[187], 100, 100, 100);
    tira.setPixelColor(Pantalla[186], 100, 100, 100);
    tira.setPixelColor(Pantalla[183], 0, 255, 0);
    tira.setPixelColor(Pantalla[182], 100, 100, 100);
    tira.setPixelColor(Pantalla[181], 100, 100, 100);
    tira.setPixelColor(Pantalla[176], 0, 0, 0);
    tira.setPixelColor(Pantalla[192], 0, 255, 0);
    tira.setPixelColor(Pantalla[196], 100, 100, 100);
    tira.setPixelColor(Pantalla[197], 100, 100, 100);
    tira.setPixelColor(Pantalla[201], 100, 100, 100);
    tira.setPixelColor(Pantalla[202], 100, 100, 100);
    tira.setPixelColor(Pantalla[207], 0, 0, 0);
    tira.setPixelColor(Pantalla[223], 0, 0, 0);
    tira.setPixelColor(Pantalla[222], 0, 0, 0);
    tira.setPixelColor(Pantalla[221], 0, 255, 0);
    tira.setPixelColor(Pantalla[220], 105, 210, 30);
    tira.setPixelColor(Pantalla[219], 105, 210, 30);
    tira.setPixelColor(Pantalla[218], 0, 0, 0);
    tira.setPixelColor(Pantalla[217], 0, 255, 0);
    tira.setPixelColor(Pantalla[216], 0, 255, 0);
    tira.setPixelColor(Pantalla[215], 105, 210, 30);
    tira.setPixelColor(Pantalla[214], 105, 210, 30);
    tira.setPixelColor(Pantalla[213], 0, 255, 0);
    tira.setPixelColor(Pantalla[212], 0, 255, 0);
    tira.setPixelColor(Pantalla[211], 0, 0, 0);
    tira.setPixelColor(Pantalla[210], 0, 0, 0);
    tira.setPixelColor(Pantalla[209], 0, 255, 0);
    tira.setPixelColor(Pantalla[208], 0, 255, 0);
  }
  else
  {
    tira.setPixelColor(Pantalla[0], 0, 0, 0);
    tira.setPixelColor(Pantalla[1], 0, 0, 0);
    tira.setPixelColor(Pantalla[2], 0, 255, 0);
    tira.setPixelColor(Pantalla[3], 0, 255, 0);
    tira.setPixelColor(Pantalla[4], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[5], 0, 0, 0);
    tira.setPixelColor(Pantalla[6], 0, 255, 0);
    tira.setPixelColor(Pantalla[7], 0, 255, 0);
    tira.setPixelColor(Pantalla[8], 0, 0, 0);
    tira.setPixelColor(Pantalla[9], 0, 0, 0);
    tira.setPixelColor(Pantalla[10], 0, 255, 0);
    tira.setPixelColor(Pantalla[11], 0, 255, 0);
    tira.setPixelColor(Pantalla[12], 0, 0, 0);
    tira.setPixelColor(Pantalla[13], 0, 0, 0);
    tira.setPixelColor(Pantalla[14], 0, 255, 0);
    tira.setPixelColor(Pantalla[15], 0, 255, 0);
    tira.setPixelColor(Pantalla[31], 0, 255, 0);
    tira.setPixelColor(Pantalla[27], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[16], 0, 0, 0);
    tira.setPixelColor(Pantalla[32], 0, 255, 0);
    tira.setPixelColor(Pantalla[35], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[36], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[37], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[43], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[44], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[47], 0, 0, 0);
    tira.setPixelColor(Pantalla[63], 0, 0, 0);
    tira.setPixelColor(Pantalla[61], 100, 100, 100);
    tira.setPixelColor(Pantalla[60], 100, 100, 100);
    tira.setPixelColor(Pantalla[59], 100, 100, 100);
    tira.setPixelColor(Pantalla[58], 100, 100, 100);
    tira.setPixelColor(Pantalla[57], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[51], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[48], 0, 255, 0);
    tira.setPixelColor(Pantalla[64], 0, 0, 0);
    tira.setPixelColor(Pantalla[66], 100, 100, 100);
    tira.setPixelColor(Pantalla[67], 0, 0, 255);
    tira.setPixelColor(Pantalla[68], 100, 100, 100);
    tira.setPixelColor(Pantalla[69], 100, 100, 100);
    tira.setPixelColor(Pantalla[70], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[74], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[75], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[76], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[77], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[78], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[79], 0, 255, 0);
    tira.setPixelColor(Pantalla[95], 0, 255, 0);
    tira.setPixelColor(Pantalla[94], 100, 100, 100);
    tira.setPixelColor(Pantalla[93], 100, 100, 100);
    tira.setPixelColor(Pantalla[92], 0, 0, 255);
    tira.setPixelColor(Pantalla[91], 100, 100, 100);
    tira.setPixelColor(Pantalla[90], 100, 100, 100);
    tira.setPixelColor(Pantalla[89], 100, 100, 100);
    tira.setPixelColor(Pantalla[86], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[85], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[84], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[83], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[82], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[81], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[80], 0, 0, 0);
    tira.setPixelColor(Pantalla[96], 0, 255, 0);
    tira.setPixelColor(Pantalla[97], 100, 100, 100);
    tira.setPixelColor(Pantalla[98], 100, 100, 100);
    tira.setPixelColor(Pantalla[99], 100, 100, 100);
    tira.setPixelColor(Pantalla[100], 100, 100, 100);
    tira.setPixelColor(Pantalla[101], 100, 100, 100);
    tira.setPixelColor(Pantalla[102], 100, 100, 100);
    tira.setPixelColor(Pantalla[105], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[106], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[107], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[109], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[110], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[111], 0, 0, 0);
    tira.setPixelColor(Pantalla[127], 0, 0, 0);
    tira.setPixelColor(Pantalla[123], 255, 0, 255);
    tira.setPixelColor(Pantalla[122], 255, 255, 0);
    tira.setPixelColor(Pantalla[121], 0, 255, 0);
    tira.setPixelColor(Pantalla[117], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[116], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[113], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[112], 0, 255, 0);
    tira.setPixelColor(Pantalla[128], 0, 0, 0);
    tira.setPixelColor(Pantalla[132], 0, 0, 255);
    tira.setPixelColor(Pantalla[133], 100, 100, 100);
    tira.setPixelColor(Pantalla[134], 100, 100, 100);
    tira.setPixelColor(Pantalla[135], 100, 100, 100);
    tira.setPixelColor(Pantalla[136], 100, 100, 100);
    tira.setPixelColor(Pantalla[137], 100, 100, 100);
    tira.setPixelColor(Pantalla[138], 105, 210, 30);
    tira.setPixelColor(Pantalla[143], 0, 255, 0);
    tira.setPixelColor(Pantalla[159], 0, 255, 0);
    tira.setPixelColor(Pantalla[155], 100, 100, 100);
    tira.setPixelColor(Pantalla[154], 100, 100, 100);
    tira.setPixelColor(Pantalla[153], 100, 100, 100);
    tira.setPixelColor(Pantalla[152], 100, 100, 100);
    tira.setPixelColor(Pantalla[151], 100, 100, 100);
    tira.setPixelColor(Pantalla[150], 100, 100, 100);
    tira.setPixelColor(Pantalla[149], 100, 100, 100);
    tira.setPixelColor(Pantalla[144], 0, 0, 0);
    tira.setPixelColor(Pantalla[160], 0, 255, 0);
    tira.setPixelColor(Pantalla[164], 100, 100, 100);
    tira.setPixelColor(Pantalla[165], 100, 100, 100);
    tira.setPixelColor(Pantalla[166], 100, 100, 100);
    tira.setPixelColor(Pantalla[167], 100, 100, 100);
    tira.setPixelColor(Pantalla[168], 100, 100, 100);
    tira.setPixelColor(Pantalla[169], 100, 100, 100);
    tira.setPixelColor(Pantalla[170], 100, 100, 100);
    tira.setPixelColor(Pantalla[175], 0, 0, 0);
    tira.setPixelColor(Pantalla[191], 0, 0, 0);
    tira.setPixelColor(Pantalla[187], 100, 100, 100);
    tira.setPixelColor(Pantalla[186], 100, 100, 100);
    tira.setPixelColor(Pantalla[183], 0, 255, 0);
    tira.setPixelColor(Pantalla[182], 100, 100, 100);
    tira.setPixelColor(Pantalla[181], 100, 100, 100);
    tira.setPixelColor(Pantalla[176], 0, 255, 0);
    tira.setPixelColor(Pantalla[192], 0, 0, 0);
    tira.setPixelColor(Pantalla[196], 100, 100, 100);
    tira.setPixelColor(Pantalla[197], 100, 100, 100);
    tira.setPixelColor(Pantalla[201], 100, 100, 100);
    tira.setPixelColor(Pantalla[202], 100, 100, 100);
    tira.setPixelColor(Pantalla[207], 0, 255, 0);
    tira.setPixelColor(Pantalla[223], 0, 255, 0);
    tira.setPixelColor(Pantalla[222], 0, 255, 0);
    tira.setPixelColor(Pantalla[221], 0, 0, 0);
    tira.setPixelColor(Pantalla[220], 105, 210, 30);
    tira.setPixelColor(Pantalla[219], 105, 210, 30);
    tira.setPixelColor(Pantalla[218], 0, 255, 0);
    tira.setPixelColor(Pantalla[217], 0, 0, 0);
    tira.setPixelColor(Pantalla[216], 0, 0, 0);
    tira.setPixelColor(Pantalla[215], 105, 210, 30);
    tira.setPixelColor(Pantalla[214], 105, 210, 30);
    tira.setPixelColor(Pantalla[213], 0, 0, 0);
    tira.setPixelColor(Pantalla[212], 0, 0, 0);
    tira.setPixelColor(Pantalla[211], 0, 255, 0);
    tira.setPixelColor(Pantalla[210], 0, 255, 0);
    tira.setPixelColor(Pantalla[209], 0, 0, 0);
    tira.setPixelColor(Pantalla[208], 0, 0, 0);
  }
}

void Matriz_Botella()
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

void Matriz_Copa()
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

void Matriz_Sin_Castigo()
{
  tira.setPixelColor(Pantalla[61], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[60], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[59], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[58], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[55], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[66], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[69], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[72], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[75], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[93], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[90], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[87], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[85], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[98], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[101], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[104], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[106], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[125], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[122], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[119], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[118], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[130], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[133], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[136], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[138], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[157], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[154], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[151], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[149], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[162], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[165], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[168], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[171], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[189], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[188], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[187], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[186], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[183], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
}

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
// SIN DETECCION
void Aro_Sin_Deteccion()
{
  // La logica de color se envia desde el monitor mediante el datos_recibidos.c
  Asigna_Color_Basandose_En_Color_Recibido_De_Monitor();
  switch (datos_recibidos.pp)
  {
  case PRIVADO:
    /*code*/
    // solo prende el aro si corresponde al color del tirador esperado
    if (datos_recibidos.c == color_de_disparo)
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
  // rick
  case UNO:
    Matriz_Limpia_Secuencias();
    Matriz_Aro_1();
    Posicion_Aro = DOS;
    break;
  case DOS:
    Matriz_Aro_2();
    Posicion_Aro = TRES;
    break;
  case TRES:
    Matriz_Aro_3();
    Posicion_Aro = CUATRO;
    break;
  case CUATRO:
    Matriz_Aro_4();
    Posicion_Aro = CINCO;
    break;
  case CINCO:
    Matriz_Aro_5();
    Posicion_Aro = SEIS;
    break;
  case SEIS:
    Matriz_Aro_6();
    Posicion_Aro = SIETE;
    break;
  case SIETE:
    Matriz_Aro_7();
    Posicion_Aro = UNO;
    break;
  }
}

/*--------------------------------------------------*/
void Asigna_Color_Aro_Global()
{
  // define color del aro
  if (datos_recibidos.vt == CASTIGO)
  {
    // ROJO
    // Serial.println("Programa color rojo para castigos");
    pixel_rojo = 0;
    pixel_verde = 250;
    pixel_azul = 0;
  }
  else
  {
    /*code*/
    // AMARILLO
    pixel_rojo = 250;
    pixel_verde = 250;
    pixel_azul = 0;
  }
}

/*--------------------------------------------------*/
void Asigna_Color_Basandose_En_Color_Recibido_De_Monitor()
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

  case RED:
    /*code*/
    // Serial.println("Programa color rojo para castigos");
    pixel_rojo = 0;
    pixel_verde = 250;
    pixel_azul = 0;
    break;

  case BLUE:
    /*code*/
    // Serial.println("Programa color azul");
    pixel_rojo = 0;
    pixel_verde = 0;
    pixel_azul = 250;
    break;

  case ARCOIRIS:
    /*code*/
    pixel_rojo = int(rand() % 255);
    pixel_verde = int(rand() % 255);
    pixel_azul = int(rand() % 255);
    break;
  }
}

void Asigna_Color_Aro_Basandose_En_Tiro_Pistola()
{
  // define color con base en el color de la pistola detectada
  switch (color_de_disparo)
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
  }
}

/*---------------------------------*/
// DISPARO
void Aro_Disparo()
{
  switch (datos_recibidos.pp)
  {
  case PRIVADO:
    if (datos_recibidos.c == color_de_disparo) // solo prende el aro si corresponde el color
    {
      tiempo_termina_tiro = millis();
      Aro_Disparo_Secuencia();
      Valor_De_Tiro(color_de_disparo);
      posicion_nueva = DISPARO_DETECTADO;
    }
    break;

  case PUBLICO:
    tiempo_termina_tiro = millis();
    Aro_Apuntando_Secuencia(); // prende el apuntar sin importar el color que apunte
    posicion_nueva = DISPARO_DETECTADO;
    Valor_De_Tiro(color_de_disparo);
    break;
  }
}

/*----------------------------------*/

void Arma_Paquete_De_Envio()
{
  // LLENADO DE PAQUETE A ENVIAR
  // Control
  datos_enviados.n = datos_recibidos.n;
  // Propiedades de juego
  datos_enviados.ju = datos_recibidos.ju; //[Monitor]1 CLASICO-WESTERN / 2 TORNEO /3 PAREJAS/4 VELOCIDAD
  datos_enviados.jr = datos_recibidos.jr; // [Monitor]No de Round para definicion de velocidad, no aplica en diana
  // Propiedades del Paquete
  datos_enviados.t = datos_recibidos.t; //[Monitor]TEST=0, TIRO_ACTIVO=1, JUGADOR_READY=2 es es el recibido en el mensaje de llegada
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
    if (color_de_disparo == datos_recibidos.c) //[Diana]color de jugador original
    {
      datos_enviados.jg = datos_recibidos.p;
    }
    else
    {
      datos_enviados.jg = datos_recibidos.pa;
    }
  }
  datos_enviados.po = valor_puntuacion_figura; //[Diana]puntuacion de la figura impactada
  datos_enviados.fi = figura_actual;           //[Diana]figura impactada en tiro
  Despliega_datos_enviados();
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
    // solo prende el aro si corresponde el color
    if (datos_recibidos.c == color_de_disparo)
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
  // tira.clear();
  switch (Posicion_Aro)
  { // rick
  case UNO:
    Matriz_Aro_1_Apuntado();
    Matriz_Ilumina_Mira_Color_Jugador_Apuntado();
    Posicion_Aro = DOS;
    break;
  case DOS:
    Matriz_Aro_2_Apuntado();
    Matriz_Ilumina_Mira_Color_Jugador_Apuntado();
    Posicion_Aro = UNO;
    break;
  }
}

void Matriz_Ilumina_Mira_Color_Jugador_Apuntado(){
  tira.setPixelColor(Pantalla[256], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[257], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[258], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[259], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[260], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[261], pixel_rojo, pixel_verde, pixel_azul);

}


void Barra_Blanca()
{
  tira.setPixelColor(Pantalla[223], 0, 255, 0);
  tira.setPixelColor(Pantalla[222], 0, 255, 0);
  tira.setPixelColor(Pantalla[221], 0, 255, 0);
  tira.setPixelColor(Pantalla[220], 100, 100, 100);
  tira.setPixelColor(Pantalla[219], 100, 100, 100);
  tira.setPixelColor(Pantalla[218], 100, 100, 100);
  tira.setPixelColor(Pantalla[217], 255, 255, 0);
  tira.setPixelColor(Pantalla[216], 255, 255, 0);
  tira.setPixelColor(Pantalla[215], 255, 255, 0);
  tira.setPixelColor(Pantalla[214], 255, 255, 0);
  tira.setPixelColor(Pantalla[213], 100, 100, 100);
  tira.setPixelColor(Pantalla[212], 100, 100, 100);
  tira.setPixelColor(Pantalla[211], 100, 100, 100);
  tira.setPixelColor(Pantalla[210], 0, 255, 0);
  tira.setPixelColor(Pantalla[209], 0, 255, 0);
  tira.setPixelColor(Pantalla[208], 0, 255, 0);
}
void Aro_Disparo_Secuencia()
{
  Matriz_Aro_1_Disparo();
  tira.show();
  Espera(10);
  Matriz_Aro_2_Disparo();
  tira.show();
  Espera(10);
  Matriz_Aro_3_Disparo();
  tira.show();
  Espera(10);
  Matriz_Aro_4_Disparo();
  tira.show();
  Espera(10);
  Matriz_Aro_5_Disparo();
  tira.show();
  Espera(10);
  Matriz_Aro_6_Disparo();
  tira.show();
  Espera(10);
  Matriz_Aro_7_Disparo();
  tira.show();
  Espera(10);
}

void Valor_De_Tiro(uint8_t local_color_de_disparo)
{
  tira.clear();
  tira.show();
  for (int i = 0; i <= 6; i++)
  {
    Matriz_Valor_De_Tiro(local_color_de_disparo);
    tira.show();
    Espera(100);
  }
  // Espera(100);
}

void Matriz_Valor_De_Tiro(uint8_t local_color_de_disparo)
{
  // Serial.println("Matriz_Valor_De_Tiro--> Figura Actual => "+String(figura_actual));
  // Serial.println("Matriz_Valor_De_Tiro--> Color de tiro => "+String(local_color_de_disparo));
  if (local_color_de_disparo == GREEN)
  {
    Matriz_Green();
  }
  else
  { // BLUE
    Matriz_Blue();
  }
  switch (figura_actual)
  {
  // castigos
  case CASTIGO_UNICORNIO:
    Matriz_200_Negativo();
    break;
  case CASTIGO_BOTELLA:
    Matriz_100_Negativo();
    break;
  case CASTIGO_COPA:
    Matriz_75_Negativo();
    break;
  // Normal
  case NORMAL_OSO:
    /* code */
    Matriz_50();
    break;
  case NORMAL_ZORRA:
    Matriz_40();
    break;
  case NORMAL_MARIPOSA:
    Matriz_45();
    break;
  case NORMAL_PINGUINO:
    Matriz_75();
    break;
  case NORMAL_ARANA:
    Matriz_35();
    break;
  case NORMAL_LAGARTIJA:
    Matriz_30();
    break;
  case NORMAL_RANA:
    Matriz_50();
    break;
  // Bonos
  case BONO_CORAZON:
    Matriz_100();
    break;
  case BONO_PINGUINO:
    Matriz_75();
    break;
  case BONO_RANA:
    Matriz_50();
    break;
  case BONO_CORAZON_200:
    Matriz_100();
    break;
  case CALIBRA_OK:
    Matriz_Ok();
    break;
  default:
    Serial.println("Matriz_Valor_De_Tiro--> Figura Actual-NO RECONOCIDA => " + String(figura_actual));
    break;
  }
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

void Matriz_100()
{
  tira.setPixelColor(Pantalla[62], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[61], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[58], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[57], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[56], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[55], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[53], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[51], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[66], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[69], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[72], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[74], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[77], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[93], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[90], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[87], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[85], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[82], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[98], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[101], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[104], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[106], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[109], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[125], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[122], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[119], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[117], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[114], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[130], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[133], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[136], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[138], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[141], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[157], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[154], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[151], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[149], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[162], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[165], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[168], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[170], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[173], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[190], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[189], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[188], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[186], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[185], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[184], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[183], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[181], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
}

void Matriz_90()
{
  tira.setPixelColor(Pantalla[61], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[60], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[59], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[58], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[57], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[54], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[53], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[51], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[66], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[70], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[73], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[77], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[93], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[89], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[86], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[82], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[98], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[102], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[105], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[109], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[125], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[124], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[123], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[122], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[121], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[118], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[114], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[134], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[137], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[141], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[153], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[150], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[166], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[169], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[173], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[185], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[181], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
}

void Matriz_80()
{
  tira.setPixelColor(Pantalla[61], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[60], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[59], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[58], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[57], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[54], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[53], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[51], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[66], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[70], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[73], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[77], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[93], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[89], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[86], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[82], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[98], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[102], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[105], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[109], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[125], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[124], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[123], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[122], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[121], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[118], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[114], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[130], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[134], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[137], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[141], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[157], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[153], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[150], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[162], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[166], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[169], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[173], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[189], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[188], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[187], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[186], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[185], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[181], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
}

void Matriz_75()
{
  tira.setPixelColor(Pantalla[61], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[60], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[59], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[58], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[57], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[54], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[53], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[51], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[70], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[73], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[89], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[86], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[102], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[105], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[121], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[118], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[117], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[116], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[115], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[114], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[134], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[141], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[153], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[166], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[173], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[185], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[181], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
}

void Matriz_70()
{
  tira.setPixelColor(Pantalla[61], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[60], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[59], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[58], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[57], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[54], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[53], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[51], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[70], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[73], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[77], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[89], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[86], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[82], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[102], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[105], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[109], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[121], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[118], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[114], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[134], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[137], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[141], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[153], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[150], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[166], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[169], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[173], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[185], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[181], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
}

void Matriz_60()
{
  tira.setPixelColor(Pantalla[61], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[54], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[53], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[51], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[66], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[73], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[77], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[93], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[86], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[82], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[98], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[105], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[109], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[125], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[124], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[123], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[122], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[121], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[118], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[114], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[130], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[134], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[137], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[141], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[157], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[153], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[150], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[162], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[166], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[169], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[173], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[189], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[188], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[187], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[186], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[185], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[181], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
}

void Matriz_50()
{
  tira.setPixelColor(Pantalla[61], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[60], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[59], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[58], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[57], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[54], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[53], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[51], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[66], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[73], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[77], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[93], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[86], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[82], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[98], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[105], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[109], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[125], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[124], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[123], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[122], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[121], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[118], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[114], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[134], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[137], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[141], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[153], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[150], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[166], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[169], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[173], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[189], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[188], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[187], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[186], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[185], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[181], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
}

void Matriz_45()
{
  tira.setPixelColor(Pantalla[61], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[57], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[54], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[53], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[51], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[66], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[70], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[73], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[93], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[89], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[86], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[98], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[102], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[105], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[125], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[124], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[123], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[122], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[121], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[118], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[117], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[116], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[115], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[114], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[134], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[141], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[153], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[166], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[173], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[185], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[181], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
}

void Matriz_40()
{
  tira.setPixelColor(Pantalla[61], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[57], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[54], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[53], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[51], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[66], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[70], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[73], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[77], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[93], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[89], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[86], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[82], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[98], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[102], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[105], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[109], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[125], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[124], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[123], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[122], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[121], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[118], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[114], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[134], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[137], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[141], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[153], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[150], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[166], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[169], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[173], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[185], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[181], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
}

void Matriz_35()
{
  tira.setPixelColor(Pantalla[61], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[60], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[59], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[58], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[57], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[54], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[53], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[51], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[70], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[73], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[89], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[86], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[102], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[105], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[125], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[124], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[123], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[122], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[121], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[118], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[117], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[116], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[115], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[114], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[134], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[141], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[153], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[166], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[173], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[189], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[188], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[187], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[186], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[185], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[181], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
}

void Matriz_30()
{
  tira.setPixelColor(Pantalla[61], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[60], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[59], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[58], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[57], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[54], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[53], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[51], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[70], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[73], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[77], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[89], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[86], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[82], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[102], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[105], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[109], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[125], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[124], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[123], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[122], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[121], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[118], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[114], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[134], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[137], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[141], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[153], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[150], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[166], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[169], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[173], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[189], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[188], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[187], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[186], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[185], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[181], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
}

void Matriz_200_Negativo()
{
  tira.setPixelColor(Pantalla[62], 0, 255, 0);
  tira.setPixelColor(Pantalla[61], 0, 255, 0);
  tira.setPixelColor(Pantalla[60], 0, 255, 0);
  tira.setPixelColor(Pantalla[59], 0, 255, 0);
  tira.setPixelColor(Pantalla[57], 0, 255, 0);
  tira.setPixelColor(Pantalla[56], 0, 255, 0);
  tira.setPixelColor(Pantalla[55], 0, 255, 0);
  tira.setPixelColor(Pantalla[54], 0, 255, 0);
  tira.setPixelColor(Pantalla[52], 0, 255, 0);
  tira.setPixelColor(Pantalla[51], 0, 255, 0);
  tira.setPixelColor(Pantalla[50], 0, 255, 0);
  tira.setPixelColor(Pantalla[49], 0, 255, 0);
  tira.setPixelColor(Pantalla[68], 0, 255, 0);
  tira.setPixelColor(Pantalla[70], 0, 255, 0);
  tira.setPixelColor(Pantalla[73], 0, 255, 0);
  tira.setPixelColor(Pantalla[75], 0, 255, 0);
  tira.setPixelColor(Pantalla[78], 0, 255, 0);
  tira.setPixelColor(Pantalla[91], 0, 255, 0);
  tira.setPixelColor(Pantalla[89], 0, 255, 0);
  tira.setPixelColor(Pantalla[86], 0, 255, 0);
  tira.setPixelColor(Pantalla[84], 0, 255, 0);
  tira.setPixelColor(Pantalla[81], 0, 255, 0);
  tira.setPixelColor(Pantalla[100], 0, 255, 0);
  tira.setPixelColor(Pantalla[102], 0, 255, 0);
  tira.setPixelColor(Pantalla[105], 0, 255, 0);
  tira.setPixelColor(Pantalla[107], 0, 255, 0);
  tira.setPixelColor(Pantalla[110], 0, 255, 0);
  tira.setPixelColor(Pantalla[126], 0, 255, 0);
  tira.setPixelColor(Pantalla[125], 0, 255, 0);
  tira.setPixelColor(Pantalla[124], 0, 255, 0);
  tira.setPixelColor(Pantalla[123], 0, 255, 0);
  tira.setPixelColor(Pantalla[121], 0, 255, 0);
  tira.setPixelColor(Pantalla[118], 0, 255, 0);
  tira.setPixelColor(Pantalla[116], 0, 255, 0);
  tira.setPixelColor(Pantalla[113], 0, 255, 0);
  tira.setPixelColor(Pantalla[129], 0, 255, 0);
  tira.setPixelColor(Pantalla[134], 0, 255, 0);
  tira.setPixelColor(Pantalla[137], 0, 255, 0);
  tira.setPixelColor(Pantalla[139], 0, 255, 0);
  tira.setPixelColor(Pantalla[142], 0, 255, 0);
  tira.setPixelColor(Pantalla[158], 0, 255, 0);
  tira.setPixelColor(Pantalla[153], 0, 255, 0);
  tira.setPixelColor(Pantalla[150], 0, 255, 0);
  tira.setPixelColor(Pantalla[148], 0, 255, 0);
  tira.setPixelColor(Pantalla[145], 0, 255, 0);
  tira.setPixelColor(Pantalla[161], 0, 255, 0);
  tira.setPixelColor(Pantalla[166], 0, 255, 0);
  tira.setPixelColor(Pantalla[169], 0, 255, 0);
  tira.setPixelColor(Pantalla[171], 0, 255, 0);
  tira.setPixelColor(Pantalla[174], 0, 255, 0);
  tira.setPixelColor(Pantalla[190], 0, 255, 0);
  tira.setPixelColor(Pantalla[189], 0, 255, 0);
  tira.setPixelColor(Pantalla[188], 0, 255, 0);
  tira.setPixelColor(Pantalla[187], 0, 255, 0);
  tira.setPixelColor(Pantalla[185], 0, 255, 0);
  tira.setPixelColor(Pantalla[184], 0, 255, 0);
  tira.setPixelColor(Pantalla[183], 0, 255, 0);
  tira.setPixelColor(Pantalla[182], 0, 255, 0);
  tira.setPixelColor(Pantalla[180], 0, 255, 0);
  tira.setPixelColor(Pantalla[179], 0, 255, 0);
  tira.setPixelColor(Pantalla[178], 0, 255, 0);
  tira.setPixelColor(Pantalla[177], 0, 255, 0);
}

void Matriz_100_Negativo()
{
  tira.setPixelColor(Pantalla[62], 0, 255, 0);
  tira.setPixelColor(Pantalla[61], 0, 255, 0);
  tira.setPixelColor(Pantalla[58], 0, 255, 0);
  tira.setPixelColor(Pantalla[57], 0, 255, 0);
  tira.setPixelColor(Pantalla[56], 0, 255, 0);
  tira.setPixelColor(Pantalla[55], 0, 255, 0);
  tira.setPixelColor(Pantalla[53], 0, 255, 0);
  tira.setPixelColor(Pantalla[52], 0, 255, 0);
  tira.setPixelColor(Pantalla[51], 0, 255, 0);
  tira.setPixelColor(Pantalla[50], 0, 255, 0);
  tira.setPixelColor(Pantalla[66], 0, 255, 0);
  tira.setPixelColor(Pantalla[69], 0, 255, 0);
  tira.setPixelColor(Pantalla[72], 0, 255, 0);
  tira.setPixelColor(Pantalla[74], 0, 255, 0);
  tira.setPixelColor(Pantalla[77], 0, 255, 0);
  tira.setPixelColor(Pantalla[93], 0, 255, 0);
  tira.setPixelColor(Pantalla[90], 0, 255, 0);
  tira.setPixelColor(Pantalla[87], 0, 255, 0);
  tira.setPixelColor(Pantalla[85], 0, 255, 0);
  tira.setPixelColor(Pantalla[82], 0, 255, 0);
  tira.setPixelColor(Pantalla[98], 0, 255, 0);
  tira.setPixelColor(Pantalla[101], 0, 255, 0);
  tira.setPixelColor(Pantalla[104], 0, 255, 0);
  tira.setPixelColor(Pantalla[106], 0, 255, 0);
  tira.setPixelColor(Pantalla[109], 0, 255, 0);
  tira.setPixelColor(Pantalla[125], 0, 255, 0);
  tira.setPixelColor(Pantalla[122], 0, 255, 0);
  tira.setPixelColor(Pantalla[119], 0, 255, 0);
  tira.setPixelColor(Pantalla[117], 0, 255, 0);
  tira.setPixelColor(Pantalla[114], 0, 255, 0);
  tira.setPixelColor(Pantalla[130], 0, 255, 0);
  tira.setPixelColor(Pantalla[133], 0, 255, 0);
  tira.setPixelColor(Pantalla[136], 0, 255, 0);
  tira.setPixelColor(Pantalla[138], 0, 255, 0);
  tira.setPixelColor(Pantalla[141], 0, 255, 0);
  tira.setPixelColor(Pantalla[157], 0, 255, 0);
  tira.setPixelColor(Pantalla[154], 0, 255, 0);
  tira.setPixelColor(Pantalla[151], 0, 255, 0);
  tira.setPixelColor(Pantalla[149], 0, 255, 0);
  tira.setPixelColor(Pantalla[146], 0, 255, 0);
  tira.setPixelColor(Pantalla[162], 0, 255, 0);
  tira.setPixelColor(Pantalla[165], 0, 255, 0);
  tira.setPixelColor(Pantalla[168], 0, 255, 0);
  tira.setPixelColor(Pantalla[170], 0, 255, 0);
  tira.setPixelColor(Pantalla[173], 0, 255, 0);
  tira.setPixelColor(Pantalla[190], 0, 255, 0);
  tira.setPixelColor(Pantalla[189], 0, 255, 0);
  tira.setPixelColor(Pantalla[188], 0, 255, 0);
  tira.setPixelColor(Pantalla[186], 0, 255, 0);
  tira.setPixelColor(Pantalla[185], 0, 255, 0);
  tira.setPixelColor(Pantalla[184], 0, 255, 0);
  tira.setPixelColor(Pantalla[183], 0, 255, 0);
  tira.setPixelColor(Pantalla[181], 0, 255, 0);
  tira.setPixelColor(Pantalla[180], 0, 255, 0);
  tira.setPixelColor(Pantalla[179], 0, 255, 0);
  tira.setPixelColor(Pantalla[178], 0, 255, 0);
}

void Matriz_75_Negativo()
{
  tira.setPixelColor(Pantalla[62], 0, 255, 0);
  tira.setPixelColor(Pantalla[61], 0, 255, 0);
  tira.setPixelColor(Pantalla[60], 0, 255, 0);
  tira.setPixelColor(Pantalla[59], 0, 255, 0);
  tira.setPixelColor(Pantalla[58], 0, 255, 0);
  tira.setPixelColor(Pantalla[55], 0, 255, 0);
  tira.setPixelColor(Pantalla[54], 0, 255, 0);
  tira.setPixelColor(Pantalla[53], 0, 255, 0);
  tira.setPixelColor(Pantalla[52], 0, 255, 0);
  tira.setPixelColor(Pantalla[51], 0, 255, 0);
  tira.setPixelColor(Pantalla[69], 0, 255, 0);
  tira.setPixelColor(Pantalla[72], 0, 255, 0);
  tira.setPixelColor(Pantalla[90], 0, 255, 0);
  tira.setPixelColor(Pantalla[87], 0, 255, 0);
  tira.setPixelColor(Pantalla[101], 0, 255, 0);
  tira.setPixelColor(Pantalla[104], 0, 255, 0);
  tira.setPixelColor(Pantalla[122], 0, 255, 0);
  tira.setPixelColor(Pantalla[119], 0, 255, 0);
  tira.setPixelColor(Pantalla[118], 0, 255, 0);
  tira.setPixelColor(Pantalla[117], 0, 255, 0);
  tira.setPixelColor(Pantalla[116], 0, 255, 0);
  tira.setPixelColor(Pantalla[115], 0, 255, 0);
  tira.setPixelColor(Pantalla[133], 0, 255, 0);
  tira.setPixelColor(Pantalla[140], 0, 255, 0);
  tira.setPixelColor(Pantalla[154], 0, 255, 0);
  tira.setPixelColor(Pantalla[147], 0, 255, 0);
  tira.setPixelColor(Pantalla[165], 0, 255, 0);
  tira.setPixelColor(Pantalla[172], 0, 255, 0);
  tira.setPixelColor(Pantalla[186], 0, 255, 0);
  tira.setPixelColor(Pantalla[183], 0, 255, 0);
  tira.setPixelColor(Pantalla[182], 0, 255, 0);
  tira.setPixelColor(Pantalla[181], 0, 255, 0);
  tira.setPixelColor(Pantalla[180], 0, 255, 0);
  tira.setPixelColor(Pantalla[179], 0, 255, 0);
}

void Matriz_Aro_1()
{
  tira.setPixelColor(Pantalla[224], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[231], 100, 100, 100);
  tira.setPixelColor(Pantalla[232], 100, 100, 100);
  tira.setPixelColor(Pantalla[239], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[255], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[248], 100, 100, 100);
  tira.setPixelColor(Pantalla[247], 100, 100, 100);
  tira.setPixelColor(Pantalla[240], pixel_rojo, pixel_verde, pixel_azul);
}

void Matriz_Aro_2()
{
  tira.setPixelColor(Pantalla[225], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[231], 100, 100, 100);
  tira.setPixelColor(Pantalla[232], 100, 100, 100);
  tira.setPixelColor(Pantalla[238], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[254], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[248], 100, 100, 100);
  tira.setPixelColor(Pantalla[247], 100, 100, 100);
  tira.setPixelColor(Pantalla[241], pixel_rojo, pixel_verde, pixel_azul);
}

void Matriz_Aro_3()
{
  tira.setPixelColor(Pantalla[226], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[231], 255, 255, 0);
  tira.setPixelColor(Pantalla[232], 255, 255, 0);
  tira.setPixelColor(Pantalla[237], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[253], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[248], 255, 255, 0);
  tira.setPixelColor(Pantalla[247], 255, 255, 0);
  tira.setPixelColor(Pantalla[242], pixel_rojo, pixel_verde, pixel_azul);
}

void Matriz_Aro_4()
{
  tira.setPixelColor(Pantalla[227], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[231], 255, 255, 0);
  tira.setPixelColor(Pantalla[232], 255, 255, 0);
  tira.setPixelColor(Pantalla[236], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[252], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[248], 255, 255, 0);
  tira.setPixelColor(Pantalla[247], 255, 255, 0);
  tira.setPixelColor(Pantalla[243], pixel_rojo, pixel_verde, pixel_azul);
}

void Matriz_Aro_5()
{

  tira.setPixelColor(Pantalla[228], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[231], 100, 100, 100);
  tira.setPixelColor(Pantalla[232], 100, 100, 100);
  tira.setPixelColor(Pantalla[235], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[251], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[248], 100, 100, 100);
  tira.setPixelColor(Pantalla[247], 100, 100, 100);
  tira.setPixelColor(Pantalla[244], pixel_rojo, pixel_verde, pixel_azul);
}

void Matriz_Aro_6()
{
  tira.setPixelColor(Pantalla[229], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[231], 100, 100, 100);
  tira.setPixelColor(Pantalla[232], 100, 100, 100);
  tira.setPixelColor(Pantalla[234], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[248], 100, 100, 100);
  tira.setPixelColor(Pantalla[247], 100, 100, 100);
  tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
}

void Matriz_Aro_7()
{
  tira.setPixelColor(Pantalla[217], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[216], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[215], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[214], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[230], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[231], 255, 255, 0);
  tira.setPixelColor(Pantalla[232], 255, 255, 0);
  tira.setPixelColor(Pantalla[233], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[248], 255, 255, 0);
  tira.setPixelColor(Pantalla[247], 255, 255, 0);
  tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
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

void Matriz_Aro_1_Apuntado()
{
  tira.setPixelColor(Pantalla[0], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[1], 0, 0, 0);
  tira.setPixelColor(Pantalla[2], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[3], 0, 0, 0);
  tira.setPixelColor(Pantalla[4], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[5], 0, 0, 0);
  tira.setPixelColor(Pantalla[6], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[7], 0, 0, 0);
  tira.setPixelColor(Pantalla[8], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[9], 0, 0, 0);
  tira.setPixelColor(Pantalla[10], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[11], 0, 0, 0);
  tira.setPixelColor(Pantalla[12], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[13], 0, 0, 0);
  tira.setPixelColor(Pantalla[14], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[15], 0, 0, 0);
  tira.setPixelColor(Pantalla[31], 0, 0, 0);
  tira.setPixelColor(Pantalla[16], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[32], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[47], 0, 0, 0);
  tira.setPixelColor(Pantalla[63], 0, 0, 0);
  tira.setPixelColor(Pantalla[48], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[64], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[79], 0, 0, 0);
  tira.setPixelColor(Pantalla[95], 0, 0, 0);
  tira.setPixelColor(Pantalla[80], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[96], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[111], 0, 0, 0);
  tira.setPixelColor(Pantalla[127], 0, 0, 0);
  tira.setPixelColor(Pantalla[112], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[128], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[143], 0, 0, 0);
  tira.setPixelColor(Pantalla[159], 0, 0, 0);
  tira.setPixelColor(Pantalla[144], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[160], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[175], 0, 0, 0);
  tira.setPixelColor(Pantalla[191], 0, 0, 0);
  tira.setPixelColor(Pantalla[176], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[192], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[207], 0, 0, 0);
  tira.setPixelColor(Pantalla[223], 0, 0, 0);
  tira.setPixelColor(Pantalla[208], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[224], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[239], 0, 0, 0);
  tira.setPixelColor(Pantalla[255], 0, 0, 0);
  tira.setPixelColor(Pantalla[254], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[253], 0, 0, 0);
  tira.setPixelColor(Pantalla[252], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[251], 0, 0, 0);
  tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[249], 0, 0, 0);
  tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[247], 0, 0, 0);
  tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[245], 0, 0, 0);
  tira.setPixelColor(Pantalla[244], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[243], 0, 0, 0);
  tira.setPixelColor(Pantalla[242], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[241], 0, 0, 0);
  tira.setPixelColor(Pantalla[240], pixel_rojo, pixel_verde, pixel_azul);
}

void Matriz_Aro_2_Apuntado()
{
  tira.setPixelColor(Pantalla[0], 0, 0, 0);
  tira.setPixelColor(Pantalla[1], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[2], 0, 0, 0);
  tira.setPixelColor(Pantalla[3], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[4], 0, 0, 0);
  tira.setPixelColor(Pantalla[5], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[6], 0, 0, 0);
  tira.setPixelColor(Pantalla[7], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[8], 0, 0, 0);
  tira.setPixelColor(Pantalla[9], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[10], 0, 0, 0);
  tira.setPixelColor(Pantalla[11], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[12], 0, 0, 0);
  tira.setPixelColor(Pantalla[13], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[14], 0, 0, 0);
  tira.setPixelColor(Pantalla[15], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[31], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[16], 0, 0, 0);
  tira.setPixelColor(Pantalla[32], 0, 0, 0);
  tira.setPixelColor(Pantalla[47], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[63], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[48], 0, 0, 0);
  tira.setPixelColor(Pantalla[64], 0, 0, 0);
  tira.setPixelColor(Pantalla[79], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[95], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[80], 0, 0, 0);
  tira.setPixelColor(Pantalla[96], 0, 0, 0);
  tira.setPixelColor(Pantalla[111], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[127], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[112], 0, 0, 0);
  tira.setPixelColor(Pantalla[128], 0, 0, 0);
  tira.setPixelColor(Pantalla[143], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[159], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[144], 0, 0, 0);
  tira.setPixelColor(Pantalla[160], 0, 0, 0);
  tira.setPixelColor(Pantalla[175], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[191], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[176], 0, 0, 0);
  tira.setPixelColor(Pantalla[192], 0, 0, 0);
  tira.setPixelColor(Pantalla[207], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[223], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[208], 0, 0, 0);
  tira.setPixelColor(Pantalla[224], 0, 0, 0);
  tira.setPixelColor(Pantalla[239], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[255], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[254], 0, 0, 0);
  tira.setPixelColor(Pantalla[253], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[252], 0, 0, 0);
  tira.setPixelColor(Pantalla[251], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[250], 0, 0, 0);
  tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[248], 0, 0, 0);
  tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[246], 0, 0, 0);
  tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[244], 0, 0, 0);
  tira.setPixelColor(Pantalla[243], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[242], 0, 0, 0);
  tira.setPixelColor(Pantalla[241], pixel_rojo, pixel_verde, pixel_azul);
  tira.setPixelColor(Pantalla[240], 0, 0, 0);
}

void Muestra_Datos_Programador()
{
  // Serial.println("datos_recibidos.no_paquete_test ="+String(datos_recibidos.n));
  Serial.println("datos_recibidos.tipo_tiro =" + String(datos_recibidos.t));
  // Serial.println("datos_recibidos.color ="+String(datos_recibidos.c));
  // Serial.println("datos_recibidos.diana ="+String(datos_recibidos.d));
  // Serial.println("datos_recibidos.propietario ="+String(datos_recibidos.p));
  // Serial.println("datos_recibidos.tiempo ="+String(datos_recibidos.tiempo));
  // Serial.println("datos_recibidos.figura ="+String(datos_recibidos.f));
  // Serial.println("datos_recibidos.shot ="+String(datos_recibidos.s));
}

void Matriz_Aro_1_Disparo()
{
  tira.setPixelColor(Pantalla[6], 0, 0, 255);
  tira.setPixelColor(Pantalla[7], 0, 0, 255);
  tira.setPixelColor(Pantalla[8], 0, 0, 255);
  tira.setPixelColor(Pantalla[9], 0, 0, 255);
  tira.setPixelColor(Pantalla[26], 0, 0, 255);
  tira.setPixelColor(Pantalla[25], 0, 0, 255);
  tira.setPixelColor(Pantalla[24], 0, 0, 255);
  tira.setPixelColor(Pantalla[23], 0, 0, 255);
  tira.setPixelColor(Pantalla[22], 0, 0, 255);
  tira.setPixelColor(Pantalla[21], 0, 0, 255);
  tira.setPixelColor(Pantalla[36], 0, 0, 255);
  tira.setPixelColor(Pantalla[37], 0, 0, 255);
  tira.setPixelColor(Pantalla[38], 255, 0, 255);
  tira.setPixelColor(Pantalla[39], 100, 100, 100);
  tira.setPixelColor(Pantalla[40], 100, 100, 100);
  tira.setPixelColor(Pantalla[41], 255, 0, 255);
  tira.setPixelColor(Pantalla[42], 0, 0, 255);
  tira.setPixelColor(Pantalla[43], 0, 0, 255);
  tira.setPixelColor(Pantalla[60], 0, 0, 255);
  tira.setPixelColor(Pantalla[59], 0, 0, 255);
  tira.setPixelColor(Pantalla[58], 255, 0, 255);
  tira.setPixelColor(Pantalla[57], 255, 0, 255);
  tira.setPixelColor(Pantalla[56], 100, 100, 100);
  tira.setPixelColor(Pantalla[55], 100, 100, 100);
  tira.setPixelColor(Pantalla[54], 255, 0, 255);
  tira.setPixelColor(Pantalla[53], 255, 0, 255);
  tira.setPixelColor(Pantalla[52], 0, 0, 255);
  tira.setPixelColor(Pantalla[51], 0, 0, 255);
  tira.setPixelColor(Pantalla[66], 0, 0, 255);
  tira.setPixelColor(Pantalla[67], 0, 0, 255);
  tira.setPixelColor(Pantalla[68], 255, 0, 255);
  tira.setPixelColor(Pantalla[69], 255, 0, 255);
  tira.setPixelColor(Pantalla[70], 100, 100, 100);
  tira.setPixelColor(Pantalla[71], 100, 100, 100);
  tira.setPixelColor(Pantalla[72], 100, 100, 100);
  tira.setPixelColor(Pantalla[73], 100, 100, 100);
  tira.setPixelColor(Pantalla[74], 255, 0, 255);
  tira.setPixelColor(Pantalla[75], 255, 0, 255);
  tira.setPixelColor(Pantalla[76], 0, 0, 255);
  tira.setPixelColor(Pantalla[77], 0, 0, 255);
  tira.setPixelColor(Pantalla[94], 0, 0, 255);
  tira.setPixelColor(Pantalla[93], 0, 0, 255);
  tira.setPixelColor(Pantalla[92], 255, 0, 255);
  tira.setPixelColor(Pantalla[91], 255, 0, 255);
  tira.setPixelColor(Pantalla[90], 100, 100, 100);
  tira.setPixelColor(Pantalla[89], 100, 100, 100);
  tira.setPixelColor(Pantalla[88], 100, 100, 100);
  tira.setPixelColor(Pantalla[87], 100, 100, 100);
  tira.setPixelColor(Pantalla[86], 100, 100, 100);
  tira.setPixelColor(Pantalla[85], 100, 100, 100);
  tira.setPixelColor(Pantalla[84], 255, 0, 255);
  tira.setPixelColor(Pantalla[83], 255, 0, 255);
  tira.setPixelColor(Pantalla[82], 0, 0, 255);
  tira.setPixelColor(Pantalla[81], 0, 0, 255);
  tira.setPixelColor(Pantalla[97], 0, 0, 255);
  tira.setPixelColor(Pantalla[98], 0, 0, 255);
  tira.setPixelColor(Pantalla[99], 100, 100, 100);
  tira.setPixelColor(Pantalla[100], 100, 100, 100);
  tira.setPixelColor(Pantalla[101], 100, 100, 100);
  tira.setPixelColor(Pantalla[102], 100, 100, 100);
  tira.setPixelColor(Pantalla[103], 100, 100, 100);
  tira.setPixelColor(Pantalla[104], 100, 100, 100);
  tira.setPixelColor(Pantalla[105], 100, 100, 100);
  tira.setPixelColor(Pantalla[106], 100, 100, 100);
  tira.setPixelColor(Pantalla[107], 100, 100, 100);
  tira.setPixelColor(Pantalla[108], 100, 100, 100);
  tira.setPixelColor(Pantalla[109], 0, 0, 255);
  tira.setPixelColor(Pantalla[110], 0, 0, 255);
  tira.setPixelColor(Pantalla[126], 0, 0, 255);
  tira.setPixelColor(Pantalla[125], 0, 0, 255);
  tira.setPixelColor(Pantalla[124], 100, 100, 100);
  tira.setPixelColor(Pantalla[123], 100, 100, 100);
  tira.setPixelColor(Pantalla[122], 100, 100, 100);
  tira.setPixelColor(Pantalla[121], 100, 100, 100);
  tira.setPixelColor(Pantalla[120], 100, 100, 100);
  tira.setPixelColor(Pantalla[119], 100, 100, 100);
  tira.setPixelColor(Pantalla[118], 100, 100, 100);
  tira.setPixelColor(Pantalla[117], 100, 100, 100);
  tira.setPixelColor(Pantalla[116], 100, 100, 100);
  tira.setPixelColor(Pantalla[115], 100, 100, 100);
  tira.setPixelColor(Pantalla[114], 0, 0, 255);
  tira.setPixelColor(Pantalla[113], 0, 0, 255);
  tira.setPixelColor(Pantalla[129], 0, 0, 255);
  tira.setPixelColor(Pantalla[130], 0, 0, 255);
  tira.setPixelColor(Pantalla[131], 255, 0, 255);
  tira.setPixelColor(Pantalla[132], 255, 0, 255);
  tira.setPixelColor(Pantalla[133], 100, 100, 100);
  tira.setPixelColor(Pantalla[134], 100, 100, 100);
  tira.setPixelColor(Pantalla[135], 100, 100, 100);
  tira.setPixelColor(Pantalla[136], 100, 100, 100);
  tira.setPixelColor(Pantalla[137], 100, 100, 100);
  tira.setPixelColor(Pantalla[138], 100, 100, 100);
  tira.setPixelColor(Pantalla[139], 255, 0, 255);
  tira.setPixelColor(Pantalla[140], 255, 0, 255);
  tira.setPixelColor(Pantalla[141], 0, 0, 255);
  tira.setPixelColor(Pantalla[142], 0, 0, 255);
  tira.setPixelColor(Pantalla[157], 0, 0, 255);
  tira.setPixelColor(Pantalla[156], 0, 0, 255);
  tira.setPixelColor(Pantalla[155], 255, 0, 255);
  tira.setPixelColor(Pantalla[154], 255, 0, 255);
  tira.setPixelColor(Pantalla[153], 100, 100, 100);
  tira.setPixelColor(Pantalla[152], 100, 100, 100);
  tira.setPixelColor(Pantalla[151], 100, 100, 100);
  tira.setPixelColor(Pantalla[150], 100, 100, 100);
  tira.setPixelColor(Pantalla[149], 255, 0, 255);
  tira.setPixelColor(Pantalla[148], 255, 0, 255);
  tira.setPixelColor(Pantalla[147], 0, 0, 255);
  tira.setPixelColor(Pantalla[146], 0, 0, 255);
  tira.setPixelColor(Pantalla[163], 0, 0, 255);
  tira.setPixelColor(Pantalla[164], 0, 0, 255);
  tira.setPixelColor(Pantalla[165], 255, 0, 255);
  tira.setPixelColor(Pantalla[166], 255, 0, 255);
  tira.setPixelColor(Pantalla[167], 100, 100, 100);
  tira.setPixelColor(Pantalla[168], 100, 100, 100);
  tira.setPixelColor(Pantalla[169], 255, 0, 255);
  tira.setPixelColor(Pantalla[170], 255, 0, 255);
  tira.setPixelColor(Pantalla[171], 0, 0, 255);
  tira.setPixelColor(Pantalla[172], 0, 0, 255);
  tira.setPixelColor(Pantalla[187], 0, 0, 255);
  tira.setPixelColor(Pantalla[186], 0, 0, 255);
  tira.setPixelColor(Pantalla[185], 255, 0, 255);
  tira.setPixelColor(Pantalla[184], 100, 100, 100);
  tira.setPixelColor(Pantalla[183], 100, 100, 100);
  tira.setPixelColor(Pantalla[182], 255, 0, 255);
  tira.setPixelColor(Pantalla[181], 0, 0, 255);
  tira.setPixelColor(Pantalla[180], 0, 0, 255);
  tira.setPixelColor(Pantalla[217], 0, 0, 255);
  tira.setPixelColor(Pantalla[216], 0, 0, 255);
  tira.setPixelColor(Pantalla[215], 0, 0, 255);
  tira.setPixelColor(Pantalla[214], 0, 0, 255);
}

void Matriz_Aro_2_Disparo()
{
  tira.setPixelColor(Pantalla[0], 255, 0, 255);
  tira.setPixelColor(Pantalla[1], 100, 100, 100);
  tira.setPixelColor(Pantalla[2], 255, 0, 255);
  tira.setPixelColor(Pantalla[3], 255, 0, 255);
  tira.setPixelColor(Pantalla[4], 0, 0, 255);
  tira.setPixelColor(Pantalla[5], 0, 0, 255);
  tira.setPixelColor(Pantalla[6], 0, 0, 255);
  tira.setPixelColor(Pantalla[7], 0, 0, 255);
  tira.setPixelColor(Pantalla[8], 0, 0, 255);
  tira.setPixelColor(Pantalla[9], 0, 0, 255);
  tira.setPixelColor(Pantalla[10], 0, 0, 255);
  tira.setPixelColor(Pantalla[11], 0, 0, 255);
  tira.setPixelColor(Pantalla[12], 255, 0, 255);
  tira.setPixelColor(Pantalla[13], 255, 0, 255);
  tira.setPixelColor(Pantalla[14], 0, 0, 255);
  tira.setPixelColor(Pantalla[15], 0, 0, 255);
  tira.setPixelColor(Pantalla[31], 255, 0, 255);
  tira.setPixelColor(Pantalla[30], 100, 100, 100);
  tira.setPixelColor(Pantalla[29], 100, 100, 100);
  tira.setPixelColor(Pantalla[28], 255, 0, 255);
  tira.setPixelColor(Pantalla[27], 255, 0, 255);
  tira.setPixelColor(Pantalla[26], 255, 0, 255);
  tira.setPixelColor(Pantalla[25], 0, 0, 255);
  tira.setPixelColor(Pantalla[24], 0, 0, 255);
  tira.setPixelColor(Pantalla[23], 0, 0, 255);
  tira.setPixelColor(Pantalla[22], 0, 0, 255);
  tira.setPixelColor(Pantalla[21], 0, 0, 255);
  tira.setPixelColor(Pantalla[20], 255, 0, 255);
  tira.setPixelColor(Pantalla[19], 100, 100, 100);
  tira.setPixelColor(Pantalla[18], 100, 100, 100);
  tira.setPixelColor(Pantalla[17], 255, 0, 255);
  tira.setPixelColor(Pantalla[16], 0, 0, 255);
  tira.setPixelColor(Pantalla[32], 0, 0, 255);
  tira.setPixelColor(Pantalla[33], 255, 0, 255);
  tira.setPixelColor(Pantalla[34], 100, 100, 100);
  tira.setPixelColor(Pantalla[35], 100, 100, 100);
  tira.setPixelColor(Pantalla[36], 100, 100, 100);
  tira.setPixelColor(Pantalla[37], 255, 0, 255);
  tira.setPixelColor(Pantalla[38], 100, 100, 100);
  tira.setPixelColor(Pantalla[39], 100, 100, 100);
  tira.setPixelColor(Pantalla[40], 100, 100, 100);
  tira.setPixelColor(Pantalla[41], 100, 100, 100);
  tira.setPixelColor(Pantalla[42], 255, 0, 255);
  tira.setPixelColor(Pantalla[43], 100, 100, 100);
  tira.setPixelColor(Pantalla[44], 100, 100, 100);
  tira.setPixelColor(Pantalla[45], 100, 100, 100);
  tira.setPixelColor(Pantalla[46], 255, 0, 255);
  tira.setPixelColor(Pantalla[47], 0, 0, 255);
  tira.setPixelColor(Pantalla[63], 0, 0, 255);
  tira.setPixelColor(Pantalla[62], 0, 0, 255);
  tira.setPixelColor(Pantalla[61], 255, 0, 255);
  tira.setPixelColor(Pantalla[60], 100, 100, 100);
  tira.setPixelColor(Pantalla[59], 100, 100, 100);
  tira.setPixelColor(Pantalla[58], 100, 100, 100);
  tira.setPixelColor(Pantalla[57], 100, 100, 100);
  tira.setPixelColor(Pantalla[56], 100, 100, 100);
  tira.setPixelColor(Pantalla[55], 100, 100, 100);
  tira.setPixelColor(Pantalla[54], 100, 100, 100);
  tira.setPixelColor(Pantalla[53], 100, 100, 100);
  tira.setPixelColor(Pantalla[52], 100, 100, 100);
  tira.setPixelColor(Pantalla[51], 100, 100, 100);
  tira.setPixelColor(Pantalla[50], 255, 0, 255);
  tira.setPixelColor(Pantalla[49], 0, 0, 255);
  tira.setPixelColor(Pantalla[48], 255, 0, 255);
  tira.setPixelColor(Pantalla[64], 0, 0, 255);
  tira.setPixelColor(Pantalla[65], 0, 0, 255);
  tira.setPixelColor(Pantalla[66], 255, 0, 255);
  tira.setPixelColor(Pantalla[67], 255, 0, 255);
  tira.setPixelColor(Pantalla[68], 100, 100, 100);
  tira.setPixelColor(Pantalla[69], 100, 100, 100);
  tira.setPixelColor(Pantalla[70], 100, 100, 100);
  tira.setPixelColor(Pantalla[71], 100, 100, 100);
  tira.setPixelColor(Pantalla[72], 100, 100, 100);
  tira.setPixelColor(Pantalla[73], 100, 100, 100);
  tira.setPixelColor(Pantalla[74], 100, 100, 100);
  tira.setPixelColor(Pantalla[75], 100, 100, 100);
  tira.setPixelColor(Pantalla[76], 255, 0, 255);
  tira.setPixelColor(Pantalla[77], 255, 0, 255);
  tira.setPixelColor(Pantalla[78], 0, 0, 255);
  tira.setPixelColor(Pantalla[79], 0, 0, 255);
  tira.setPixelColor(Pantalla[95], 0, 0, 255);
  tira.setPixelColor(Pantalla[94], 255, 0, 255);
  tira.setPixelColor(Pantalla[93], 255, 0, 255);
  tira.setPixelColor(Pantalla[92], 255, 0, 255);
  tira.setPixelColor(Pantalla[91], 100, 100, 100);
  tira.setPixelColor(Pantalla[90], 100, 100, 100);
  tira.setPixelColor(Pantalla[89], 100, 100, 100);
  tira.setPixelColor(Pantalla[88], 100, 100, 100);
  tira.setPixelColor(Pantalla[87], 100, 100, 100);
  tira.setPixelColor(Pantalla[86], 100, 100, 100);
  tira.setPixelColor(Pantalla[85], 100, 100, 100);
  tira.setPixelColor(Pantalla[84], 100, 100, 100);
  tira.setPixelColor(Pantalla[83], 100, 100, 100);
  tira.setPixelColor(Pantalla[82], 255, 0, 255);
  tira.setPixelColor(Pantalla[81], 255, 0, 255);
  tira.setPixelColor(Pantalla[80], 0, 0, 255);
  tira.setPixelColor(Pantalla[96], 255, 0, 255);
  tira.setPixelColor(Pantalla[97], 255, 0, 255);
  tira.setPixelColor(Pantalla[98], 100, 100, 100);
  tira.setPixelColor(Pantalla[99], 100, 100, 100);
  tira.setPixelColor(Pantalla[100], 100, 100, 100);
  tira.setPixelColor(Pantalla[101], 100, 100, 100);
  tira.setPixelColor(Pantalla[102], 100, 100, 100);
  tira.setPixelColor(Pantalla[103], 100, 100, 100);
  tira.setPixelColor(Pantalla[104], 100, 100, 100);
  tira.setPixelColor(Pantalla[105], 100, 100, 100);
  tira.setPixelColor(Pantalla[106], 100, 100, 100);
  tira.setPixelColor(Pantalla[107], 100, 100, 100);
  tira.setPixelColor(Pantalla[108], 100, 100, 100);
  tira.setPixelColor(Pantalla[109], 100, 100, 100);
  tira.setPixelColor(Pantalla[110], 255, 0, 255);
  tira.setPixelColor(Pantalla[111], 255, 0, 255);
  tira.setPixelColor(Pantalla[127], 0, 0, 255);
  tira.setPixelColor(Pantalla[126], 255, 0, 255);
  tira.setPixelColor(Pantalla[125], 100, 100, 100);
  tira.setPixelColor(Pantalla[124], 100, 100, 100);
  tira.setPixelColor(Pantalla[123], 100, 100, 100);
  tira.setPixelColor(Pantalla[122], 100, 100, 100);
  tira.setPixelColor(Pantalla[121], 100, 100, 100);
  tira.setPixelColor(Pantalla[120], 100, 100, 100);
  tira.setPixelColor(Pantalla[119], 100, 100, 100);
  tira.setPixelColor(Pantalla[118], 100, 100, 100);
  tira.setPixelColor(Pantalla[117], 100, 100, 100);
  tira.setPixelColor(Pantalla[116], 100, 100, 100);
  tira.setPixelColor(Pantalla[115], 100, 100, 100);
  tira.setPixelColor(Pantalla[114], 100, 100, 100);
  tira.setPixelColor(Pantalla[113], 255, 0, 255);
  tira.setPixelColor(Pantalla[112], 255, 0, 255);
  tira.setPixelColor(Pantalla[128], 0, 0, 255);
  tira.setPixelColor(Pantalla[129], 255, 0, 255);
  tira.setPixelColor(Pantalla[130], 255, 0, 255);
  tira.setPixelColor(Pantalla[131], 255, 0, 255);
  tira.setPixelColor(Pantalla[132], 100, 100, 100);
  tira.setPixelColor(Pantalla[133], 100, 100, 100);
  tira.setPixelColor(Pantalla[134], 100, 100, 100);
  tira.setPixelColor(Pantalla[135], 100, 100, 100);
  tira.setPixelColor(Pantalla[136], 100, 100, 100);
  tira.setPixelColor(Pantalla[137], 100, 100, 100);
  tira.setPixelColor(Pantalla[138], 100, 100, 100);
  tira.setPixelColor(Pantalla[139], 100, 100, 100);
  tira.setPixelColor(Pantalla[140], 100, 100, 100);
  tira.setPixelColor(Pantalla[141], 255, 0, 255);
  tira.setPixelColor(Pantalla[142], 255, 0, 255);
  tira.setPixelColor(Pantalla[143], 0, 0, 255);
  tira.setPixelColor(Pantalla[159], 0, 0, 255);
  tira.setPixelColor(Pantalla[158], 0, 0, 255);
  tira.setPixelColor(Pantalla[157], 255, 0, 255);
  tira.setPixelColor(Pantalla[156], 255, 0, 255);
  tira.setPixelColor(Pantalla[155], 100, 100, 100);
  tira.setPixelColor(Pantalla[154], 100, 100, 100);
  tira.setPixelColor(Pantalla[153], 100, 100, 100);
  tira.setPixelColor(Pantalla[152], 100, 100, 100);
  tira.setPixelColor(Pantalla[151], 100, 100, 100);
  tira.setPixelColor(Pantalla[150], 100, 100, 100);
  tira.setPixelColor(Pantalla[149], 100, 100, 100);
  tira.setPixelColor(Pantalla[148], 100, 100, 100);
  tira.setPixelColor(Pantalla[147], 255, 0, 255);
  tira.setPixelColor(Pantalla[146], 255, 0, 255);
  tira.setPixelColor(Pantalla[145], 0, 0, 255);
  tira.setPixelColor(Pantalla[144], 0, 0, 255);
  tira.setPixelColor(Pantalla[160], 0, 0, 255);
  tira.setPixelColor(Pantalla[161], 0, 0, 255);
  tira.setPixelColor(Pantalla[162], 255, 0, 255);
  tira.setPixelColor(Pantalla[163], 100, 100, 100);
  tira.setPixelColor(Pantalla[164], 100, 100, 100);
  tira.setPixelColor(Pantalla[165], 100, 100, 100);
  tira.setPixelColor(Pantalla[166], 100, 100, 100);
  tira.setPixelColor(Pantalla[167], 100, 100, 100);
  tira.setPixelColor(Pantalla[168], 100, 100, 100);
  tira.setPixelColor(Pantalla[169], 100, 100, 100);
  tira.setPixelColor(Pantalla[170], 100, 100, 100);
  tira.setPixelColor(Pantalla[171], 100, 100, 100);
  tira.setPixelColor(Pantalla[172], 100, 100, 100);
  tira.setPixelColor(Pantalla[173], 255, 0, 255);
  tira.setPixelColor(Pantalla[174], 255, 0, 255);
  tira.setPixelColor(Pantalla[175], 0, 0, 255);
  tira.setPixelColor(Pantalla[191], 0, 0, 255);
  tira.setPixelColor(Pantalla[190], 255, 0, 255);
  tira.setPixelColor(Pantalla[189], 100, 100, 100);
  tira.setPixelColor(Pantalla[188], 100, 100, 100);
  tira.setPixelColor(Pantalla[187], 100, 100, 100);
  tira.setPixelColor(Pantalla[186], 255, 0, 255);
  tira.setPixelColor(Pantalla[185], 255, 0, 255);
  tira.setPixelColor(Pantalla[184], 100, 100, 100);
  tira.setPixelColor(Pantalla[183], 100, 100, 100);
  tira.setPixelColor(Pantalla[182], 255, 0, 255);
  tira.setPixelColor(Pantalla[181], 0, 0, 255);
  tira.setPixelColor(Pantalla[180], 100, 100, 100);
  tira.setPixelColor(Pantalla[179], 100, 100, 100);
  tira.setPixelColor(Pantalla[178], 100, 100, 100);
  tira.setPixelColor(Pantalla[177], 255, 0, 255);
  tira.setPixelColor(Pantalla[176], 255, 0, 255);
  tira.setPixelColor(Pantalla[192], 255, 0, 255);
  tira.setPixelColor(Pantalla[193], 100, 100, 100);
  tira.setPixelColor(Pantalla[194], 100, 100, 100);
  tira.setPixelColor(Pantalla[195], 100, 100, 100);
  tira.setPixelColor(Pantalla[196], 255, 0, 255);
  tira.setPixelColor(Pantalla[197], 255, 0, 255);
  tira.setPixelColor(Pantalla[198], 255, 0, 255);
  tira.setPixelColor(Pantalla[199], 100, 100, 100);
  tira.setPixelColor(Pantalla[200], 100, 100, 100);
  tira.setPixelColor(Pantalla[201], 100, 100, 100);
  tira.setPixelColor(Pantalla[202], 255, 0, 255);
  tira.setPixelColor(Pantalla[203], 255, 0, 255);
  tira.setPixelColor(Pantalla[204], 100, 100, 100);
  tira.setPixelColor(Pantalla[205], 100, 100, 100);
  tira.setPixelColor(Pantalla[206], 100, 100, 100);
  tira.setPixelColor(Pantalla[207], 255, 0, 255);
  tira.setPixelColor(Pantalla[223], 255, 0, 255);
  tira.setPixelColor(Pantalla[222], 100, 100, 100);
  tira.setPixelColor(Pantalla[221], 100, 100, 100);
  tira.setPixelColor(Pantalla[220], 255, 0, 255);
  tira.setPixelColor(Pantalla[219], 255, 0, 255);
  tira.setPixelColor(Pantalla[218], 255, 0, 255);
  tira.setPixelColor(Pantalla[217], 0, 0, 255);
  tira.setPixelColor(Pantalla[216], 255, 0, 255);
  tira.setPixelColor(Pantalla[215], 100, 100, 100);
  tira.setPixelColor(Pantalla[214], 100, 100, 100);
  tira.setPixelColor(Pantalla[213], 255, 0, 255);
  tira.setPixelColor(Pantalla[212], 0, 0, 255);
  tira.setPixelColor(Pantalla[211], 255, 0, 255);
  tira.setPixelColor(Pantalla[210], 100, 100, 100);
  tira.setPixelColor(Pantalla[209], 100, 100, 100);
  tira.setPixelColor(Pantalla[208], 255, 0, 255);
  tira.setPixelColor(Pantalla[224], 255, 0, 255);
  tira.setPixelColor(Pantalla[225], 255, 0, 255);
  tira.setPixelColor(Pantalla[226], 100, 100, 100);
  tira.setPixelColor(Pantalla[227], 255, 0, 255);
  tira.setPixelColor(Pantalla[228], 0, 0, 255);
  tira.setPixelColor(Pantalla[229], 0, 0, 255);
  tira.setPixelColor(Pantalla[230], 0, 0, 255);
  tira.setPixelColor(Pantalla[231], 255, 0, 255);
  tira.setPixelColor(Pantalla[232], 255, 0, 255);
  tira.setPixelColor(Pantalla[233], 255, 0, 255);
  tira.setPixelColor(Pantalla[234], 255, 0, 255);
  tira.setPixelColor(Pantalla[235], 0, 0, 255);
  tira.setPixelColor(Pantalla[236], 255, 0, 255);
  tira.setPixelColor(Pantalla[237], 255, 0, 255);
  tira.setPixelColor(Pantalla[238], 255, 0, 255);
  tira.setPixelColor(Pantalla[239], 0, 0, 255);
  tira.setPixelColor(Pantalla[255], 0, 0, 255);
  tira.setPixelColor(Pantalla[254], 0, 0, 255);
  tira.setPixelColor(Pantalla[253], 255, 0, 255);
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

void Matriz_Aro_3_Disparo()
{
  tira.setPixelColor(Pantalla[0], 100, 100, 100);
  tira.setPixelColor(Pantalla[1], 100, 100, 100);
  tira.setPixelColor(Pantalla[2], 255, 0, 255);
  tira.setPixelColor(Pantalla[3], 255, 0, 255);
  tira.setPixelColor(Pantalla[4], 100, 100, 100);
  tira.setPixelColor(Pantalla[5], 0, 0, 255);
  tira.setPixelColor(Pantalla[6], 100, 100, 100);
  tira.setPixelColor(Pantalla[7], 100, 100, 100);
  tira.setPixelColor(Pantalla[8], 100, 100, 100);
  tira.setPixelColor(Pantalla[9], 100, 100, 100);
  tira.setPixelColor(Pantalla[10], 100, 100, 100);
  tira.setPixelColor(Pantalla[11], 0, 0, 255);
  tira.setPixelColor(Pantalla[12], 255, 0, 255);
  tira.setPixelColor(Pantalla[13], 100, 100, 100);
  tira.setPixelColor(Pantalla[14], 100, 100, 100);
  tira.setPixelColor(Pantalla[15], 100, 100, 100);
  tira.setPixelColor(Pantalla[31], 255, 0, 255);
  tira.setPixelColor(Pantalla[30], 100, 100, 100);
  tira.setPixelColor(Pantalla[29], 100, 100, 100);
  tira.setPixelColor(Pantalla[28], 255, 0, 255);
  tira.setPixelColor(Pantalla[27], 100, 100, 100);
  tira.setPixelColor(Pantalla[26], 100, 100, 100);
  tira.setPixelColor(Pantalla[25], 100, 100, 100);
  tira.setPixelColor(Pantalla[24], 100, 100, 100);
  tira.setPixelColor(Pantalla[23], 100, 100, 100);
  tira.setPixelColor(Pantalla[22], 100, 100, 100);
  tira.setPixelColor(Pantalla[21], 100, 100, 100);
  tira.setPixelColor(Pantalla[20], 100, 100, 100);
  tira.setPixelColor(Pantalla[19], 100, 100, 100);
  tira.setPixelColor(Pantalla[18], 100, 100, 100);
  tira.setPixelColor(Pantalla[17], 100, 100, 100);
  tira.setPixelColor(Pantalla[16], 100, 100, 100);
  tira.setPixelColor(Pantalla[32], 0, 0, 255);
  tira.setPixelColor(Pantalla[33], 255, 0, 255);
  tira.setPixelColor(Pantalla[34], 100, 100, 100);
  tira.setPixelColor(Pantalla[35], 100, 100, 100);
  tira.setPixelColor(Pantalla[36], 100, 100, 100);
  tira.setPixelColor(Pantalla[37], 100, 100, 100);
  tira.setPixelColor(Pantalla[38], 100, 100, 100);
  tira.setPixelColor(Pantalla[39], 100, 100, 100);
  tira.setPixelColor(Pantalla[40], 100, 100, 100);
  tira.setPixelColor(Pantalla[41], 100, 100, 100);
  tira.setPixelColor(Pantalla[42], 100, 100, 100);
  tira.setPixelColor(Pantalla[43], 100, 100, 100);
  tira.setPixelColor(Pantalla[44], 100, 100, 100);
  tira.setPixelColor(Pantalla[45], 100, 100, 100);
  tira.setPixelColor(Pantalla[46], 255, 0, 255);
  tira.setPixelColor(Pantalla[47], 0, 0, 255);
  tira.setPixelColor(Pantalla[63], 100, 100, 100);
  tira.setPixelColor(Pantalla[62], 100, 100, 100);
  tira.setPixelColor(Pantalla[61], 100, 100, 100);
  tira.setPixelColor(Pantalla[60], 100, 100, 100);
  tira.setPixelColor(Pantalla[59], 100, 100, 100);
  tira.setPixelColor(Pantalla[58], 100, 100, 100);
  tira.setPixelColor(Pantalla[57], 100, 100, 100);
  tira.setPixelColor(Pantalla[56], 100, 100, 100);
  tira.setPixelColor(Pantalla[55], 100, 100, 100);
  tira.setPixelColor(Pantalla[54], 100, 100, 100);
  tira.setPixelColor(Pantalla[53], 100, 100, 100);
  tira.setPixelColor(Pantalla[52], 100, 100, 100);
  tira.setPixelColor(Pantalla[51], 100, 100, 100);
  tira.setPixelColor(Pantalla[50], 255, 0, 255);
  tira.setPixelColor(Pantalla[49], 0, 0, 255);
  tira.setPixelColor(Pantalla[48], 255, 0, 255);
  tira.setPixelColor(Pantalla[64], 100, 100, 100);
  tira.setPixelColor(Pantalla[65], 100, 100, 100);
  tira.setPixelColor(Pantalla[66], 100, 100, 100);
  tira.setPixelColor(Pantalla[67], 100, 100, 100);
  tira.setPixelColor(Pantalla[68], 100, 100, 100);
  tira.setPixelColor(Pantalla[69], 100, 100, 100);
  tira.setPixelColor(Pantalla[70], 100, 100, 100);
  tira.setPixelColor(Pantalla[71], 100, 100, 100);
  tira.setPixelColor(Pantalla[72], 100, 100, 100);
  tira.setPixelColor(Pantalla[73], 100, 100, 100);
  tira.setPixelColor(Pantalla[74], 100, 100, 100);
  tira.setPixelColor(Pantalla[75], 100, 100, 100);
  tira.setPixelColor(Pantalla[76], 100, 100, 100);
  tira.setPixelColor(Pantalla[77], 100, 100, 100);
  tira.setPixelColor(Pantalla[78], 0, 0, 255);
  tira.setPixelColor(Pantalla[79], 0, 0, 255);
  tira.setPixelColor(Pantalla[95], 100, 100, 100);
  tira.setPixelColor(Pantalla[94], 100, 100, 100);
  tira.setPixelColor(Pantalla[93], 100, 100, 100);
  tira.setPixelColor(Pantalla[92], 100, 100, 100);
  tira.setPixelColor(Pantalla[91], 100, 100, 100);
  tira.setPixelColor(Pantalla[90], 100, 100, 100);
  tira.setPixelColor(Pantalla[89], 100, 100, 100);
  tira.setPixelColor(Pantalla[88], 100, 100, 100);
  tira.setPixelColor(Pantalla[87], 100, 100, 100);
  tira.setPixelColor(Pantalla[86], 100, 100, 100);
  tira.setPixelColor(Pantalla[85], 100, 100, 100);
  tira.setPixelColor(Pantalla[84], 100, 100, 100);
  tira.setPixelColor(Pantalla[83], 100, 100, 100);
  tira.setPixelColor(Pantalla[82], 100, 100, 100);
  tira.setPixelColor(Pantalla[81], 100, 100, 100);
  tira.setPixelColor(Pantalla[80], 100, 100, 100);
  tira.setPixelColor(Pantalla[96], 100, 100, 100);
  tira.setPixelColor(Pantalla[97], 100, 100, 100);
  tira.setPixelColor(Pantalla[98], 100, 100, 100);
  tira.setPixelColor(Pantalla[99], 100, 100, 100);
  tira.setPixelColor(Pantalla[100], 100, 100, 100);
  tira.setPixelColor(Pantalla[101], 100, 100, 100);
  tira.setPixelColor(Pantalla[102], 100, 100, 100);
  tira.setPixelColor(Pantalla[103], 100, 100, 100);
  tira.setPixelColor(Pantalla[104], 100, 100, 100);
  tira.setPixelColor(Pantalla[105], 100, 100, 100);
  tira.setPixelColor(Pantalla[106], 100, 100, 100);
  tira.setPixelColor(Pantalla[107], 100, 100, 100);
  tira.setPixelColor(Pantalla[108], 100, 100, 100);
  tira.setPixelColor(Pantalla[109], 100, 100, 100);
  tira.setPixelColor(Pantalla[110], 100, 100, 100);
  tira.setPixelColor(Pantalla[111], 100, 100, 100);
  tira.setPixelColor(Pantalla[127], 0, 0, 255);
  tira.setPixelColor(Pantalla[126], 100, 100, 100);
  tira.setPixelColor(Pantalla[125], 100, 100, 100);
  tira.setPixelColor(Pantalla[124], 100, 100, 100);
  tira.setPixelColor(Pantalla[123], 100, 100, 100);
  tira.setPixelColor(Pantalla[122], 100, 100, 100);
  tira.setPixelColor(Pantalla[121], 100, 100, 100);
  tira.setPixelColor(Pantalla[120], 100, 100, 100);
  tira.setPixelColor(Pantalla[119], 100, 100, 100);
  tira.setPixelColor(Pantalla[118], 100, 100, 100);
  tira.setPixelColor(Pantalla[117], 100, 100, 100);
  tira.setPixelColor(Pantalla[116], 100, 100, 100);
  tira.setPixelColor(Pantalla[115], 100, 100, 100);
  tira.setPixelColor(Pantalla[114], 100, 100, 100);
  tira.setPixelColor(Pantalla[113], 100, 100, 100);
  tira.setPixelColor(Pantalla[112], 100, 100, 100);
  tira.setPixelColor(Pantalla[128], 100, 100, 100);
  tira.setPixelColor(Pantalla[129], 100, 100, 100);
  tira.setPixelColor(Pantalla[130], 100, 100, 100);
  tira.setPixelColor(Pantalla[131], 100, 100, 100);
  tira.setPixelColor(Pantalla[132], 100, 100, 100);
  tira.setPixelColor(Pantalla[133], 100, 100, 100);
  tira.setPixelColor(Pantalla[134], 100, 100, 100);
  tira.setPixelColor(Pantalla[135], 100, 100, 100);
  tira.setPixelColor(Pantalla[136], 100, 100, 100);
  tira.setPixelColor(Pantalla[137], 100, 100, 100);
  tira.setPixelColor(Pantalla[138], 100, 100, 100);
  tira.setPixelColor(Pantalla[139], 100, 100, 100);
  tira.setPixelColor(Pantalla[140], 100, 100, 100);
  tira.setPixelColor(Pantalla[141], 100, 100, 100);
  tira.setPixelColor(Pantalla[142], 100, 100, 100);
  tira.setPixelColor(Pantalla[143], 100, 100, 100);
  tira.setPixelColor(Pantalla[159], 100, 100, 100);
  tira.setPixelColor(Pantalla[158], 100, 100, 100);
  tira.setPixelColor(Pantalla[157], 100, 100, 100);
  tira.setPixelColor(Pantalla[156], 100, 100, 100);
  tira.setPixelColor(Pantalla[155], 100, 100, 100);
  tira.setPixelColor(Pantalla[154], 100, 100, 100);
  tira.setPixelColor(Pantalla[153], 100, 100, 100);
  tira.setPixelColor(Pantalla[152], 100, 100, 100);
  tira.setPixelColor(Pantalla[151], 100, 100, 100);
  tira.setPixelColor(Pantalla[150], 100, 100, 100);
  tira.setPixelColor(Pantalla[149], 100, 100, 100);
  tira.setPixelColor(Pantalla[148], 100, 100, 100);
  tira.setPixelColor(Pantalla[147], 100, 100, 100);
  tira.setPixelColor(Pantalla[146], 100, 100, 100);
  tira.setPixelColor(Pantalla[145], 100, 100, 100);
  tira.setPixelColor(Pantalla[144], 100, 100, 100);
  tira.setPixelColor(Pantalla[160], 0, 0, 255);
  tira.setPixelColor(Pantalla[161], 100, 100, 100);
  tira.setPixelColor(Pantalla[162], 100, 100, 100);
  tira.setPixelColor(Pantalla[163], 100, 100, 100);
  tira.setPixelColor(Pantalla[164], 100, 100, 100);
  tira.setPixelColor(Pantalla[165], 100, 100, 100);
  tira.setPixelColor(Pantalla[166], 100, 100, 100);
  tira.setPixelColor(Pantalla[167], 100, 100, 100);
  tira.setPixelColor(Pantalla[168], 100, 100, 100);
  tira.setPixelColor(Pantalla[169], 100, 100, 100);
  tira.setPixelColor(Pantalla[170], 100, 100, 100);
  tira.setPixelColor(Pantalla[171], 100, 100, 100);
  tira.setPixelColor(Pantalla[172], 100, 100, 100);
  tira.setPixelColor(Pantalla[173], 100, 100, 100);
  tira.setPixelColor(Pantalla[174], 100, 100, 100);
  tira.setPixelColor(Pantalla[175], 0, 0, 255);
  tira.setPixelColor(Pantalla[191], 0, 0, 255);
  tira.setPixelColor(Pantalla[190], 255, 0, 255);
  tira.setPixelColor(Pantalla[189], 100, 100, 100);
  tira.setPixelColor(Pantalla[188], 100, 100, 100);
  tira.setPixelColor(Pantalla[187], 100, 100, 100);
  tira.setPixelColor(Pantalla[186], 100, 100, 100);
  tira.setPixelColor(Pantalla[185], 100, 100, 100);
  tira.setPixelColor(Pantalla[184], 100, 100, 100);
  tira.setPixelColor(Pantalla[183], 100, 100, 100);
  tira.setPixelColor(Pantalla[182], 100, 100, 100);
  tira.setPixelColor(Pantalla[181], 100, 100, 100);
  tira.setPixelColor(Pantalla[180], 100, 100, 100);
  tira.setPixelColor(Pantalla[179], 100, 100, 100);
  tira.setPixelColor(Pantalla[178], 100, 100, 100);
  tira.setPixelColor(Pantalla[177], 255, 0, 255);
  tira.setPixelColor(Pantalla[176], 255, 0, 255);
  tira.setPixelColor(Pantalla[192], 255, 0, 255);
  tira.setPixelColor(Pantalla[193], 100, 100, 100);
  tira.setPixelColor(Pantalla[194], 100, 100, 100);
  tira.setPixelColor(Pantalla[195], 100, 100, 100);
  tira.setPixelColor(Pantalla[196], 100, 100, 100);
  tira.setPixelColor(Pantalla[197], 100, 100, 100);
  tira.setPixelColor(Pantalla[198], 100, 100, 100);
  tira.setPixelColor(Pantalla[199], 100, 100, 100);
  tira.setPixelColor(Pantalla[200], 100, 100, 100);
  tira.setPixelColor(Pantalla[201], 100, 100, 100);
  tira.setPixelColor(Pantalla[202], 100, 100, 100);
  tira.setPixelColor(Pantalla[203], 100, 100, 100);
  tira.setPixelColor(Pantalla[204], 100, 100, 100);
  tira.setPixelColor(Pantalla[205], 100, 100, 100);
  tira.setPixelColor(Pantalla[206], 100, 100, 100);
  tira.setPixelColor(Pantalla[207], 255, 0, 255);
  tira.setPixelColor(Pantalla[223], 255, 0, 255);
  tira.setPixelColor(Pantalla[222], 100, 100, 100);
  tira.setPixelColor(Pantalla[221], 100, 100, 100);
  tira.setPixelColor(Pantalla[220], 255, 0, 255);
  tira.setPixelColor(Pantalla[219], 255, 0, 255);
  tira.setPixelColor(Pantalla[218], 100, 100, 100);
  tira.setPixelColor(Pantalla[217], 100, 100, 100);
  tira.setPixelColor(Pantalla[216], 100, 100, 100);
  tira.setPixelColor(Pantalla[215], 100, 100, 100);
  tira.setPixelColor(Pantalla[214], 100, 100, 100);
  tira.setPixelColor(Pantalla[213], 100, 100, 100);
  tira.setPixelColor(Pantalla[212], 0, 0, 255);
  tira.setPixelColor(Pantalla[211], 255, 0, 255);
  tira.setPixelColor(Pantalla[210], 100, 100, 100);
  tira.setPixelColor(Pantalla[209], 100, 100, 100);
  tira.setPixelColor(Pantalla[208], 255, 0, 255);
  tira.setPixelColor(Pantalla[224], 255, 0, 255);
  tira.setPixelColor(Pantalla[225], 255, 0, 255);
  tira.setPixelColor(Pantalla[226], 100, 100, 100);
  tira.setPixelColor(Pantalla[227], 255, 0, 255);
  tira.setPixelColor(Pantalla[228], 0, 0, 255);
  tira.setPixelColor(Pantalla[229], 100, 100, 100);
  tira.setPixelColor(Pantalla[230], 100, 100, 100);
  tira.setPixelColor(Pantalla[231], 100, 100, 100);
  tira.setPixelColor(Pantalla[232], 100, 100, 100);
  tira.setPixelColor(Pantalla[233], 100, 100, 100);
  tira.setPixelColor(Pantalla[234], 100, 100, 100);
  tira.setPixelColor(Pantalla[235], 0, 0, 255);
  tira.setPixelColor(Pantalla[236], 255, 0, 255);
  tira.setPixelColor(Pantalla[237], 255, 0, 255);
  tira.setPixelColor(Pantalla[238], 255, 0, 255);
  tira.setPixelColor(Pantalla[239], 0, 0, 255);
  tira.setPixelColor(Pantalla[255], 0, 0, 255);
  tira.setPixelColor(Pantalla[254], 0, 0, 255);
  tira.setPixelColor(Pantalla[253], 255, 0, 255);
  tira.setPixelColor(Pantalla[252], 0, 0, 255);
  tira.setPixelColor(Pantalla[251], 0, 0, 255);
  tira.setPixelColor(Pantalla[250], 0, 0, 255);
  tira.setPixelColor(Pantalla[249], 100, 100, 100);
  tira.setPixelColor(Pantalla[248], 100, 100, 100);
  tira.setPixelColor(Pantalla[247], 100, 100, 100);
  tira.setPixelColor(Pantalla[246], 100, 100, 100);
  tira.setPixelColor(Pantalla[245], 0, 0, 255);
  tira.setPixelColor(Pantalla[244], 0, 0, 255);
  tira.setPixelColor(Pantalla[243], 0, 0, 255);
  tira.setPixelColor(Pantalla[242], 0, 0, 255);
  tira.setPixelColor(Pantalla[241], 0, 0, 255);
  tira.setPixelColor(Pantalla[240], 0, 0, 255);
}

void Matriz_Aro_4_Disparo()
{
  tira.setPixelColor(Pantalla[0], 100, 100, 100);
  tira.setPixelColor(Pantalla[1], 100, 100, 100);
  tira.setPixelColor(Pantalla[2], 255, 0, 255);
  tira.setPixelColor(Pantalla[3], 255, 255, 0);
  tira.setPixelColor(Pantalla[4], 105, 210, 30);
  tira.setPixelColor(Pantalla[5], 0, 0, 255);
  tira.setPixelColor(Pantalla[6], 105, 210, 30);
  tira.setPixelColor(Pantalla[7], 105, 210, 30);
  tira.setPixelColor(Pantalla[8], 0, 0, 255);
  tira.setPixelColor(Pantalla[9], 255, 255, 0);
  tira.setPixelColor(Pantalla[10], 105, 210, 30);
  tira.setPixelColor(Pantalla[11], 0, 0, 255);
  tira.setPixelColor(Pantalla[12], 255, 0, 255);
  tira.setPixelColor(Pantalla[13], 100, 100, 100);
  tira.setPixelColor(Pantalla[14], 100, 100, 100);
  tira.setPixelColor(Pantalla[15], 100, 100, 100);
  tira.setPixelColor(Pantalla[31], 255, 0, 255);
  tira.setPixelColor(Pantalla[30], 100, 100, 100);
  tira.setPixelColor(Pantalla[29], 105, 210, 30);
  tira.setPixelColor(Pantalla[28], 0, 0, 255);
  tira.setPixelColor(Pantalla[27], 0, 0, 255);
  tira.setPixelColor(Pantalla[26], 0, 0, 255);
  tira.setPixelColor(Pantalla[25], 0, 0, 255);
  tira.setPixelColor(Pantalla[24], 0, 0, 255);
  tira.setPixelColor(Pantalla[23], 0, 0, 255);
  tira.setPixelColor(Pantalla[22], 0, 0, 255);
  tira.setPixelColor(Pantalla[21], 0, 0, 255);
  tira.setPixelColor(Pantalla[20], 255, 255, 0);
  tira.setPixelColor(Pantalla[19], 255, 255, 0);
  tira.setPixelColor(Pantalla[18], 105, 210, 30);
  tira.setPixelColor(Pantalla[17], 100, 100, 100);
  tira.setPixelColor(Pantalla[16], 100, 100, 100);
  tira.setPixelColor(Pantalla[32], 0, 0, 255);
  tira.setPixelColor(Pantalla[33], 255, 0, 255);
  tira.setPixelColor(Pantalla[34], 255, 255, 0);
  tira.setPixelColor(Pantalla[35], 0, 0, 255);
  tira.setPixelColor(Pantalla[36], 100, 100, 100);
  tira.setPixelColor(Pantalla[37], 100, 100, 100);
  tira.setPixelColor(Pantalla[38], 100, 100, 100);
  tira.setPixelColor(Pantalla[39], 100, 100, 100);
  tira.setPixelColor(Pantalla[40], 100, 100, 100);
  tira.setPixelColor(Pantalla[41], 100, 100, 100);
  tira.setPixelColor(Pantalla[42], 100, 100, 100);
  tira.setPixelColor(Pantalla[43], 0, 0, 255);
  tira.setPixelColor(Pantalla[44], 0, 0, 255);
  tira.setPixelColor(Pantalla[45], 255, 255, 0);
  tira.setPixelColor(Pantalla[46], 105, 210, 30);
  tira.setPixelColor(Pantalla[47], 0, 0, 255);
  tira.setPixelColor(Pantalla[63], 105, 210, 30);
  tira.setPixelColor(Pantalla[62], 255, 255, 0);
  tira.setPixelColor(Pantalla[61], 0, 0, 255);
  tira.setPixelColor(Pantalla[60], 100, 100, 100);
  tira.setPixelColor(Pantalla[59], 100, 100, 100);
  tira.setPixelColor(Pantalla[58], 100, 100, 100);
  tira.setPixelColor(Pantalla[57], 100, 100, 100);
  tira.setPixelColor(Pantalla[56], 100, 100, 100);
  tira.setPixelColor(Pantalla[55], 100, 100, 100);
  tira.setPixelColor(Pantalla[54], 100, 100, 100);
  tira.setPixelColor(Pantalla[53], 100, 100, 100);
  tira.setPixelColor(Pantalla[52], 100, 100, 100);
  tira.setPixelColor(Pantalla[51], 100, 100, 100);
  tira.setPixelColor(Pantalla[50], 0, 0, 255);
  tira.setPixelColor(Pantalla[49], 0, 0, 255);
  tira.setPixelColor(Pantalla[48], 105, 210, 30);
  tira.setPixelColor(Pantalla[64], 255, 255, 0);
  tira.setPixelColor(Pantalla[65], 0, 0, 255);
  tira.setPixelColor(Pantalla[66], 100, 100, 100);
  tira.setPixelColor(Pantalla[67], 100, 100, 100);
  tira.setPixelColor(Pantalla[68], 100, 100, 100);
  tira.setPixelColor(Pantalla[69], 100, 100, 100);
  tira.setPixelColor(Pantalla[70], 100, 100, 100);
  tira.setPixelColor(Pantalla[71], 100, 100, 100);
  tira.setPixelColor(Pantalla[72], 100, 100, 100);
  tira.setPixelColor(Pantalla[73], 100, 100, 100);
  tira.setPixelColor(Pantalla[74], 100, 100, 100);
  tira.setPixelColor(Pantalla[75], 100, 100, 100);
  tira.setPixelColor(Pantalla[76], 100, 100, 100);
  tira.setPixelColor(Pantalla[77], 100, 100, 100);
  tira.setPixelColor(Pantalla[78], 0, 0, 255);
  tira.setPixelColor(Pantalla[79], 105, 210, 30);
  tira.setPixelColor(Pantalla[95], 105, 210, 30);
  tira.setPixelColor(Pantalla[94], 0, 0, 255);
  tira.setPixelColor(Pantalla[93], 100, 100, 100);
  tira.setPixelColor(Pantalla[92], 100, 100, 100);
  tira.setPixelColor(Pantalla[91], 100, 100, 100);
  tira.setPixelColor(Pantalla[90], 100, 100, 100);
  tira.setPixelColor(Pantalla[89], 100, 100, 100);
  tira.setPixelColor(Pantalla[88], 100, 100, 100);
  tira.setPixelColor(Pantalla[87], 100, 100, 100);
  tira.setPixelColor(Pantalla[86], 100, 100, 100);
  tira.setPixelColor(Pantalla[85], 100, 100, 100);
  tira.setPixelColor(Pantalla[84], 100, 100, 100);
  tira.setPixelColor(Pantalla[83], 100, 100, 100);
  tira.setPixelColor(Pantalla[82], 100, 100, 100);
  tira.setPixelColor(Pantalla[81], 0, 0, 255);
  tira.setPixelColor(Pantalla[80], 255, 255, 0);
  tira.setPixelColor(Pantalla[96], 255, 255, 0);
  tira.setPixelColor(Pantalla[97], 0, 0, 255);
  tira.setPixelColor(Pantalla[98], 100, 100, 100);
  tira.setPixelColor(Pantalla[99], 100, 100, 100);
  tira.setPixelColor(Pantalla[100], 100, 100, 100);
  tira.setPixelColor(Pantalla[101], 100, 100, 100);
  tira.setPixelColor(Pantalla[102], 100, 100, 100);
  tira.setPixelColor(Pantalla[103], 100, 100, 100);
  tira.setPixelColor(Pantalla[104], 100, 100, 100);
  tira.setPixelColor(Pantalla[105], 100, 100, 100);
  tira.setPixelColor(Pantalla[106], 100, 100, 100);
  tira.setPixelColor(Pantalla[107], 100, 100, 100);
  tira.setPixelColor(Pantalla[108], 100, 100, 100);
  tira.setPixelColor(Pantalla[109], 100, 100, 100);
  tira.setPixelColor(Pantalla[110], 0, 0, 255);
  tira.setPixelColor(Pantalla[111], 255, 255, 0);
  tira.setPixelColor(Pantalla[127], 0, 0, 255);
  tira.setPixelColor(Pantalla[126], 0, 0, 255);
  tira.setPixelColor(Pantalla[125], 100, 100, 100);
  tira.setPixelColor(Pantalla[124], 100, 100, 100);
  tira.setPixelColor(Pantalla[123], 100, 100, 100);
  tira.setPixelColor(Pantalla[122], 100, 100, 100);
  tira.setPixelColor(Pantalla[121], 100, 100, 100);
  tira.setPixelColor(Pantalla[120], 100, 100, 100);
  tira.setPixelColor(Pantalla[119], 100, 100, 100);
  tira.setPixelColor(Pantalla[118], 100, 100, 100);
  tira.setPixelColor(Pantalla[117], 100, 100, 100);
  tira.setPixelColor(Pantalla[116], 100, 100, 100);
  tira.setPixelColor(Pantalla[115], 100, 100, 100);
  tira.setPixelColor(Pantalla[114], 100, 100, 100);
  tira.setPixelColor(Pantalla[113], 100, 100, 100);
  tira.setPixelColor(Pantalla[112], 0, 0, 255);
  tira.setPixelColor(Pantalla[128], 255, 255, 0);
  tira.setPixelColor(Pantalla[129], 0, 0, 255);
  tira.setPixelColor(Pantalla[130], 100, 100, 100);
  tira.setPixelColor(Pantalla[131], 100, 100, 100);
  tira.setPixelColor(Pantalla[132], 100, 100, 100);
  tira.setPixelColor(Pantalla[133], 100, 100, 100);
  tira.setPixelColor(Pantalla[134], 100, 100, 100);
  tira.setPixelColor(Pantalla[135], 100, 100, 100);
  tira.setPixelColor(Pantalla[136], 100, 100, 100);
  tira.setPixelColor(Pantalla[137], 100, 100, 100);
  tira.setPixelColor(Pantalla[138], 100, 100, 100);
  tira.setPixelColor(Pantalla[139], 100, 100, 100);
  tira.setPixelColor(Pantalla[140], 100, 100, 100);
  tira.setPixelColor(Pantalla[141], 100, 100, 100);
  tira.setPixelColor(Pantalla[142], 100, 100, 100);
  tira.setPixelColor(Pantalla[143], 105, 210, 30);
  tira.setPixelColor(Pantalla[159], 255, 255, 0);
  tira.setPixelColor(Pantalla[158], 0, 0, 255);
  tira.setPixelColor(Pantalla[157], 100, 100, 100);
  tira.setPixelColor(Pantalla[156], 100, 100, 100);
  tira.setPixelColor(Pantalla[155], 100, 100, 100);
  tira.setPixelColor(Pantalla[154], 100, 100, 100);
  tira.setPixelColor(Pantalla[153], 100, 100, 100);
  tira.setPixelColor(Pantalla[152], 100, 100, 100);
  tira.setPixelColor(Pantalla[151], 100, 100, 100);
  tira.setPixelColor(Pantalla[150], 100, 100, 100);
  tira.setPixelColor(Pantalla[149], 100, 100, 100);
  tira.setPixelColor(Pantalla[148], 100, 100, 100);
  tira.setPixelColor(Pantalla[147], 100, 100, 100);
  tira.setPixelColor(Pantalla[146], 100, 100, 100);
  tira.setPixelColor(Pantalla[145], 0, 0, 255);
  tira.setPixelColor(Pantalla[144], 255, 255, 0);
  tira.setPixelColor(Pantalla[160], 0, 0, 255);
  tira.setPixelColor(Pantalla[161], 0, 0, 255);
  tira.setPixelColor(Pantalla[162], 100, 100, 100);
  tira.setPixelColor(Pantalla[163], 100, 100, 100);
  tira.setPixelColor(Pantalla[164], 100, 100, 100);
  tira.setPixelColor(Pantalla[165], 100, 100, 100);
  tira.setPixelColor(Pantalla[166], 100, 100, 100);
  tira.setPixelColor(Pantalla[167], 100, 100, 100);
  tira.setPixelColor(Pantalla[168], 100, 100, 100);
  tira.setPixelColor(Pantalla[169], 100, 100, 100);
  tira.setPixelColor(Pantalla[170], 100, 100, 100);
  tira.setPixelColor(Pantalla[171], 100, 100, 100);
  tira.setPixelColor(Pantalla[172], 100, 100, 100);
  tira.setPixelColor(Pantalla[173], 0, 0, 255);
  tira.setPixelColor(Pantalla[174], 255, 255, 0);
  tira.setPixelColor(Pantalla[175], 105, 210, 30);
  tira.setPixelColor(Pantalla[191], 0, 0, 255);
  tira.setPixelColor(Pantalla[190], 105, 210, 30);
  tira.setPixelColor(Pantalla[189], 0, 0, 255);
  tira.setPixelColor(Pantalla[188], 0, 0, 255);
  tira.setPixelColor(Pantalla[187], 100, 100, 100);
  tira.setPixelColor(Pantalla[186], 100, 100, 100);
  tira.setPixelColor(Pantalla[185], 100, 100, 100);
  tira.setPixelColor(Pantalla[184], 100, 100, 100);
  tira.setPixelColor(Pantalla[183], 100, 100, 100);
  tira.setPixelColor(Pantalla[182], 100, 100, 100);
  tira.setPixelColor(Pantalla[181], 100, 100, 100);
  tira.setPixelColor(Pantalla[180], 100, 100, 100);
  tira.setPixelColor(Pantalla[179], 100, 100, 100);
  tira.setPixelColor(Pantalla[178], 0, 0, 255);
  tira.setPixelColor(Pantalla[177], 255, 255, 0);
  tira.setPixelColor(Pantalla[176], 255, 0, 255);
  tira.setPixelColor(Pantalla[192], 255, 0, 255);
  tira.setPixelColor(Pantalla[193], 100, 100, 100);
  tira.setPixelColor(Pantalla[194], 105, 210, 30);
  tira.setPixelColor(Pantalla[195], 255, 255, 0);
  tira.setPixelColor(Pantalla[196], 0, 0, 255);
  tira.setPixelColor(Pantalla[197], 0, 0, 255);
  tira.setPixelColor(Pantalla[198], 100, 100, 100);
  tira.setPixelColor(Pantalla[199], 100, 100, 100);
  tira.setPixelColor(Pantalla[200], 100, 100, 100);
  tira.setPixelColor(Pantalla[201], 100, 100, 100);
  tira.setPixelColor(Pantalla[202], 100, 100, 100);
  tira.setPixelColor(Pantalla[203], 0, 0, 255);
  tira.setPixelColor(Pantalla[204], 0, 0, 255);
  tira.setPixelColor(Pantalla[205], 255, 255, 0);
  tira.setPixelColor(Pantalla[206], 255, 255, 0);
  tira.setPixelColor(Pantalla[207], 255, 0, 255);
  tira.setPixelColor(Pantalla[223], 255, 0, 255);
  tira.setPixelColor(Pantalla[222], 100, 100, 100);
  tira.setPixelColor(Pantalla[221], 100, 100, 100);
  tira.setPixelColor(Pantalla[220], 105, 210, 30);
  tira.setPixelColor(Pantalla[219], 105, 210, 30);
  tira.setPixelColor(Pantalla[218], 255, 255, 0);
  tira.setPixelColor(Pantalla[217], 0, 0, 255);
  tira.setPixelColor(Pantalla[216], 0, 0, 255);
  tira.setPixelColor(Pantalla[215], 105, 210, 30);
  tira.setPixelColor(Pantalla[214], 0, 0, 255);
  tira.setPixelColor(Pantalla[213], 0, 0, 255);
  tira.setPixelColor(Pantalla[212], 0, 0, 255);
  tira.setPixelColor(Pantalla[211], 105, 210, 30);
  tira.setPixelColor(Pantalla[210], 100, 100, 100);
  tira.setPixelColor(Pantalla[209], 100, 100, 100);
  tira.setPixelColor(Pantalla[208], 255, 0, 255);
  tira.setPixelColor(Pantalla[224], 255, 0, 255);
  tira.setPixelColor(Pantalla[225], 255, 0, 255);
  tira.setPixelColor(Pantalla[226], 100, 100, 100);
  tira.setPixelColor(Pantalla[227], 255, 0, 255);
  tira.setPixelColor(Pantalla[228], 0, 0, 255);
  tira.setPixelColor(Pantalla[229], 100, 100, 100);
  tira.setPixelColor(Pantalla[230], 105, 210, 30);
  tira.setPixelColor(Pantalla[231], 255, 255, 0);
  tira.setPixelColor(Pantalla[232], 0, 0, 255);
  tira.setPixelColor(Pantalla[233], 105, 210, 30);
  tira.setPixelColor(Pantalla[234], 255, 255, 0);
  tira.setPixelColor(Pantalla[235], 0, 0, 255);
  tira.setPixelColor(Pantalla[236], 105, 210, 30);
  tira.setPixelColor(Pantalla[237], 255, 0, 255);
  tira.setPixelColor(Pantalla[238], 255, 0, 255);
  tira.setPixelColor(Pantalla[239], 0, 0, 255);
  tira.setPixelColor(Pantalla[255], 0, 0, 255);
  tira.setPixelColor(Pantalla[254], 0, 0, 255);
  tira.setPixelColor(Pantalla[253], 255, 0, 255);
  tira.setPixelColor(Pantalla[252], 0, 0, 255);
  tira.setPixelColor(Pantalla[251], 0, 0, 255);
  tira.setPixelColor(Pantalla[250], 0, 0, 255);
  tira.setPixelColor(Pantalla[249], 100, 100, 100);
  tira.setPixelColor(Pantalla[248], 100, 100, 100);
  tira.setPixelColor(Pantalla[247], 100, 100, 100);
  tira.setPixelColor(Pantalla[246], 100, 100, 100);
  tira.setPixelColor(Pantalla[245], 0, 0, 255);
  tira.setPixelColor(Pantalla[244], 0, 0, 255);
  tira.setPixelColor(Pantalla[243], 0, 0, 255);
  tira.setPixelColor(Pantalla[242], 0, 0, 255);
  tira.setPixelColor(Pantalla[241], 0, 0, 255);
  tira.setPixelColor(Pantalla[240], 0, 0, 255);
}

void Matriz_Aro_5_Disparo()
{
  tira.setPixelColor(Pantalla[0], 100, 100, 100);
  tira.setPixelColor(Pantalla[1], 100, 100, 100);
  tira.setPixelColor(Pantalla[2], 255, 0, 255);
  tira.setPixelColor(Pantalla[3], 255, 0, 255);
  tira.setPixelColor(Pantalla[4], 105, 210, 30);
  tira.setPixelColor(Pantalla[5], 105, 210, 30);
  tira.setPixelColor(Pantalla[6], 105, 210, 30);
  tira.setPixelColor(Pantalla[7], 105, 210, 30);
  tira.setPixelColor(Pantalla[8], 0, 0, 255);
  tira.setPixelColor(Pantalla[9], 0, 0, 255);
  tira.setPixelColor(Pantalla[10], 105, 210, 30);
  tira.setPixelColor(Pantalla[11], 105, 210, 30);
  tira.setPixelColor(Pantalla[12], 105, 210, 30);
  tira.setPixelColor(Pantalla[13], 100, 100, 100);
  tira.setPixelColor(Pantalla[14], 100, 100, 100);
  tira.setPixelColor(Pantalla[15], 100, 100, 100);
  tira.setPixelColor(Pantalla[31], 255, 0, 255);
  tira.setPixelColor(Pantalla[30], 105, 210, 30);
  tira.setPixelColor(Pantalla[29], 105, 210, 30);
  tira.setPixelColor(Pantalla[28], 0, 0, 255);
  tira.setPixelColor(Pantalla[27], 100, 100, 100);
  tira.setPixelColor(Pantalla[26], 100, 100, 100);
  tira.setPixelColor(Pantalla[25], 100, 100, 100);
  tira.setPixelColor(Pantalla[24], 100, 100, 100);
  tira.setPixelColor(Pantalla[23], 100, 100, 100);
  tira.setPixelColor(Pantalla[22], 100, 100, 100);
  tira.setPixelColor(Pantalla[21], 0, 0, 255);
  tira.setPixelColor(Pantalla[20], 0, 0, 255);
  tira.setPixelColor(Pantalla[19], 0, 0, 255);
  tira.setPixelColor(Pantalla[18], 105, 210, 30);
  tira.setPixelColor(Pantalla[17], 100, 100, 100);
  tira.setPixelColor(Pantalla[16], 100, 100, 100);
  tira.setPixelColor(Pantalla[32], 0, 0, 255);
  tira.setPixelColor(Pantalla[33], 105, 210, 30);
  tira.setPixelColor(Pantalla[34], 0, 0, 255);
  tira.setPixelColor(Pantalla[35], 100, 100, 100);
  tira.setPixelColor(Pantalla[36], 100, 100, 100);
  tira.setPixelColor(Pantalla[37], 100, 100, 100);
  tira.setPixelColor(Pantalla[38], 100, 100, 100);
  tira.setPixelColor(Pantalla[39], 100, 100, 100);
  tira.setPixelColor(Pantalla[40], 100, 100, 100);
  tira.setPixelColor(Pantalla[41], 100, 100, 100);
  tira.setPixelColor(Pantalla[42], 100, 100, 100);
  tira.setPixelColor(Pantalla[43], 100, 100, 100);
  tira.setPixelColor(Pantalla[44], 100, 100, 100);
  tira.setPixelColor(Pantalla[45], 105, 210, 30);
  tira.setPixelColor(Pantalla[46], 105, 210, 30);
  tira.setPixelColor(Pantalla[47], 0, 0, 255);
  tira.setPixelColor(Pantalla[63], 105, 210, 30);
  tira.setPixelColor(Pantalla[62], 0, 0, 255);
  tira.setPixelColor(Pantalla[61], 100, 100, 100);
  tira.setPixelColor(Pantalla[60], 100, 100, 100);
  tira.setPixelColor(Pantalla[59], 100, 100, 100);
  tira.setPixelColor(Pantalla[58], 100, 100, 100);
  tira.setPixelColor(Pantalla[57], 100, 100, 100);
  tira.setPixelColor(Pantalla[56], 100, 100, 100);
  tira.setPixelColor(Pantalla[55], 100, 100, 100);
  tira.setPixelColor(Pantalla[54], 100, 100, 100);
  tira.setPixelColor(Pantalla[53], 100, 100, 100);
  tira.setPixelColor(Pantalla[52], 100, 100, 100);
  tira.setPixelColor(Pantalla[51], 100, 100, 100);
  tira.setPixelColor(Pantalla[50], 255, 0, 255);
  tira.setPixelColor(Pantalla[49], 105, 210, 30);
  tira.setPixelColor(Pantalla[48], 105, 210, 30);
  tira.setPixelColor(Pantalla[64], 105, 210, 30);
  tira.setPixelColor(Pantalla[65], 0, 0, 255);
  tira.setPixelColor(Pantalla[66], 100, 100, 100);
  tira.setPixelColor(Pantalla[67], 100, 100, 100);
  tira.setPixelColor(Pantalla[68], 100, 100, 100);
  tira.setPixelColor(Pantalla[69], 100, 100, 100);
  tira.setPixelColor(Pantalla[70], 100, 100, 100);
  tira.setPixelColor(Pantalla[71], 0, 255, 0);
  tira.setPixelColor(Pantalla[72], 0, 255, 0);
  tira.setPixelColor(Pantalla[73], 100, 100, 100);
  tira.setPixelColor(Pantalla[74], 100, 100, 100);
  tira.setPixelColor(Pantalla[75], 100, 100, 100);
  tira.setPixelColor(Pantalla[76], 100, 100, 100);
  tira.setPixelColor(Pantalla[77], 100, 100, 100);
  tira.setPixelColor(Pantalla[78], 0, 0, 255);
  tira.setPixelColor(Pantalla[79], 105, 210, 30);
  tira.setPixelColor(Pantalla[95], 0, 0, 255);
  tira.setPixelColor(Pantalla[94], 100, 100, 100);
  tira.setPixelColor(Pantalla[93], 100, 100, 100);
  tira.setPixelColor(Pantalla[92], 100, 100, 100);
  tira.setPixelColor(Pantalla[91], 100, 100, 100);
  tira.setPixelColor(Pantalla[90], 100, 100, 100);
  tira.setPixelColor(Pantalla[89], 0, 255, 0);
  tira.setPixelColor(Pantalla[88], 0, 255, 0);
  tira.setPixelColor(Pantalla[87], 0, 255, 0);
  tira.setPixelColor(Pantalla[86], 0, 255, 0);
  tira.setPixelColor(Pantalla[85], 100, 100, 100);
  tira.setPixelColor(Pantalla[84], 100, 100, 100);
  tira.setPixelColor(Pantalla[83], 100, 100, 100);
  tira.setPixelColor(Pantalla[82], 100, 100, 100);
  tira.setPixelColor(Pantalla[81], 0, 0, 255);
  tira.setPixelColor(Pantalla[80], 105, 210, 30);
  tira.setPixelColor(Pantalla[96], 105, 210, 30);
  tira.setPixelColor(Pantalla[97], 100, 100, 100);
  tira.setPixelColor(Pantalla[98], 100, 100, 100);
  tira.setPixelColor(Pantalla[99], 100, 100, 100);
  tira.setPixelColor(Pantalla[100], 100, 100, 100);
  tira.setPixelColor(Pantalla[101], 0, 255, 0);
  tira.setPixelColor(Pantalla[102], 0, 255, 0);
  tira.setPixelColor(Pantalla[103], 0, 0, 0);
  tira.setPixelColor(Pantalla[104], 0, 0, 0);
  tira.setPixelColor(Pantalla[105], 0, 255, 0);
  tira.setPixelColor(Pantalla[106], 0, 255, 0);
  tira.setPixelColor(Pantalla[107], 100, 100, 100);
  tira.setPixelColor(Pantalla[108], 100, 100, 100);
  tira.setPixelColor(Pantalla[109], 100, 100, 100);
  tira.setPixelColor(Pantalla[110], 100, 100, 100);
  tira.setPixelColor(Pantalla[111], 105, 210, 30);
  tira.setPixelColor(Pantalla[127], 105, 210, 30);
  tira.setPixelColor(Pantalla[126], 100, 100, 100);
  tira.setPixelColor(Pantalla[125], 100, 100, 100);
  tira.setPixelColor(Pantalla[124], 100, 100, 100);
  tira.setPixelColor(Pantalla[123], 100, 100, 100);
  tira.setPixelColor(Pantalla[122], 0, 255, 0);
  tira.setPixelColor(Pantalla[121], 0, 0, 0);
  tira.setPixelColor(Pantalla[120], 0, 0, 0);
  tira.setPixelColor(Pantalla[119], 0, 0, 0);
  tira.setPixelColor(Pantalla[118], 0, 0, 0);
  tira.setPixelColor(Pantalla[117], 0, 255, 0);
  tira.setPixelColor(Pantalla[116], 100, 100, 100);
  tira.setPixelColor(Pantalla[115], 100, 100, 100);
  tira.setPixelColor(Pantalla[114], 100, 100, 100);
  tira.setPixelColor(Pantalla[113], 100, 100, 100);
  tira.setPixelColor(Pantalla[112], 105, 210, 30);
  tira.setPixelColor(Pantalla[128], 105, 210, 30);
  tira.setPixelColor(Pantalla[129], 100, 100, 100);
  tira.setPixelColor(Pantalla[130], 100, 100, 100);
  tira.setPixelColor(Pantalla[131], 100, 100, 100);
  tira.setPixelColor(Pantalla[132], 100, 100, 100);
  tira.setPixelColor(Pantalla[133], 0, 255, 0);
  tira.setPixelColor(Pantalla[134], 0, 0, 0);
  tira.setPixelColor(Pantalla[135], 0, 0, 0);
  tira.setPixelColor(Pantalla[136], 0, 0, 0);
  tira.setPixelColor(Pantalla[137], 0, 0, 0);
  tira.setPixelColor(Pantalla[138], 0, 255, 0);
  tira.setPixelColor(Pantalla[139], 100, 100, 100);
  tira.setPixelColor(Pantalla[140], 100, 100, 100);
  tira.setPixelColor(Pantalla[141], 100, 100, 100);
  tira.setPixelColor(Pantalla[142], 100, 100, 100);
  tira.setPixelColor(Pantalla[143], 105, 210, 30);
  tira.setPixelColor(Pantalla[159], 105, 210, 30);
  tira.setPixelColor(Pantalla[158], 0, 0, 255);
  tira.setPixelColor(Pantalla[157], 100, 100, 100);
  tira.setPixelColor(Pantalla[156], 100, 100, 100);
  tira.setPixelColor(Pantalla[155], 100, 100, 100);
  tira.setPixelColor(Pantalla[154], 0, 255, 0);
  tira.setPixelColor(Pantalla[153], 0, 255, 0);
  tira.setPixelColor(Pantalla[152], 0, 0, 0);
  tira.setPixelColor(Pantalla[151], 0, 0, 0);
  tira.setPixelColor(Pantalla[150], 0, 255, 0);
  tira.setPixelColor(Pantalla[149], 100, 100, 100);
  tira.setPixelColor(Pantalla[148], 100, 100, 100);
  tira.setPixelColor(Pantalla[147], 100, 100, 100);
  tira.setPixelColor(Pantalla[146], 100, 100, 100);
  tira.setPixelColor(Pantalla[145], 0, 0, 255);
  tira.setPixelColor(Pantalla[144], 105, 210, 30);
  tira.setPixelColor(Pantalla[160], 0, 0, 255);
  tira.setPixelColor(Pantalla[161], 105, 210, 30);
  tira.setPixelColor(Pantalla[162], 0, 0, 255);
  tira.setPixelColor(Pantalla[163], 100, 100, 100);
  tira.setPixelColor(Pantalla[164], 100, 100, 100);
  tira.setPixelColor(Pantalla[165], 100, 100, 100);
  tira.setPixelColor(Pantalla[166], 100, 100, 100);
  tira.setPixelColor(Pantalla[167], 0, 255, 0);
  tira.setPixelColor(Pantalla[168], 0, 255, 0);
  tira.setPixelColor(Pantalla[169], 100, 100, 100);
  tira.setPixelColor(Pantalla[170], 100, 100, 100);
  tira.setPixelColor(Pantalla[171], 100, 100, 100);
  tira.setPixelColor(Pantalla[172], 100, 100, 100);
  tira.setPixelColor(Pantalla[173], 0, 0, 255);
  tira.setPixelColor(Pantalla[174], 0, 0, 255);
  tira.setPixelColor(Pantalla[175], 105, 210, 30);
  tira.setPixelColor(Pantalla[191], 0, 0, 255);
  tira.setPixelColor(Pantalla[190], 105, 210, 30);
  tira.setPixelColor(Pantalla[189], 0, 0, 255);
  tira.setPixelColor(Pantalla[188], 100, 100, 100);
  tira.setPixelColor(Pantalla[187], 100, 100, 100);
  tira.setPixelColor(Pantalla[186], 100, 100, 100);
  tira.setPixelColor(Pantalla[185], 100, 100, 100);
  tira.setPixelColor(Pantalla[184], 100, 100, 100);
  tira.setPixelColor(Pantalla[183], 100, 100, 100);
  tira.setPixelColor(Pantalla[182], 100, 100, 100);
  tira.setPixelColor(Pantalla[181], 100, 100, 100);
  tira.setPixelColor(Pantalla[180], 100, 100, 100);
  tira.setPixelColor(Pantalla[179], 0, 0, 255);
  tira.setPixelColor(Pantalla[178], 0, 0, 255);
  tira.setPixelColor(Pantalla[177], 105, 210, 30);
  tira.setPixelColor(Pantalla[176], 105, 210, 30);
  tira.setPixelColor(Pantalla[192], 255, 0, 255);
  tira.setPixelColor(Pantalla[193], 100, 100, 100);
  tira.setPixelColor(Pantalla[194], 105, 210, 30);
  tira.setPixelColor(Pantalla[195], 0, 0, 255);
  tira.setPixelColor(Pantalla[196], 100, 100, 100);
  tira.setPixelColor(Pantalla[197], 100, 100, 100);
  tira.setPixelColor(Pantalla[198], 100, 100, 100);
  tira.setPixelColor(Pantalla[199], 100, 100, 100);
  tira.setPixelColor(Pantalla[200], 100, 100, 100);
  tira.setPixelColor(Pantalla[201], 100, 100, 100);
  tira.setPixelColor(Pantalla[202], 100, 100, 100);
  tira.setPixelColor(Pantalla[203], 0, 0, 255);
  tira.setPixelColor(Pantalla[204], 105, 210, 30);
  tira.setPixelColor(Pantalla[205], 105, 210, 30);
  tira.setPixelColor(Pantalla[206], 100, 100, 100);
  tira.setPixelColor(Pantalla[207], 255, 0, 255);
  tira.setPixelColor(Pantalla[223], 255, 0, 255);
  tira.setPixelColor(Pantalla[222], 100, 100, 100);
  tira.setPixelColor(Pantalla[221], 100, 100, 100);
  tira.setPixelColor(Pantalla[220], 105, 210, 30);
  tira.setPixelColor(Pantalla[219], 0, 0, 255);
  tira.setPixelColor(Pantalla[218], 100, 100, 100);
  tira.setPixelColor(Pantalla[217], 100, 100, 100);
  tira.setPixelColor(Pantalla[216], 100, 100, 100);
  tira.setPixelColor(Pantalla[215], 100, 100, 100);
  tira.setPixelColor(Pantalla[214], 100, 100, 100);
  tira.setPixelColor(Pantalla[213], 0, 0, 255);
  tira.setPixelColor(Pantalla[212], 105, 210, 30);
  tira.setPixelColor(Pantalla[211], 105, 210, 30);
  tira.setPixelColor(Pantalla[210], 100, 100, 100);
  tira.setPixelColor(Pantalla[209], 100, 100, 100);
  tira.setPixelColor(Pantalla[208], 255, 0, 255);
  tira.setPixelColor(Pantalla[224], 255, 0, 255);
  tira.setPixelColor(Pantalla[225], 255, 0, 255);
  tira.setPixelColor(Pantalla[226], 100, 100, 100);
  tira.setPixelColor(Pantalla[227], 105, 210, 30);
  tira.setPixelColor(Pantalla[228], 0, 0, 255);
  tira.setPixelColor(Pantalla[229], 0, 0, 255);
  tira.setPixelColor(Pantalla[230], 100, 100, 100);
  tira.setPixelColor(Pantalla[231], 100, 100, 100);
  tira.setPixelColor(Pantalla[232], 0, 0, 255);
  tira.setPixelColor(Pantalla[233], 0, 0, 255);
  tira.setPixelColor(Pantalla[234], 105, 210, 30);
  tira.setPixelColor(Pantalla[235], 105, 210, 30);
  tira.setPixelColor(Pantalla[236], 255, 0, 255);
  tira.setPixelColor(Pantalla[237], 255, 0, 255);
  tira.setPixelColor(Pantalla[238], 255, 0, 255);
  tira.setPixelColor(Pantalla[239], 0, 0, 255);
  tira.setPixelColor(Pantalla[255], 0, 0, 255);
  tira.setPixelColor(Pantalla[254], 0, 0, 255);
  tira.setPixelColor(Pantalla[253], 255, 0, 255);
  tira.setPixelColor(Pantalla[252], 0, 0, 255);
  tira.setPixelColor(Pantalla[251], 105, 210, 30);
  tira.setPixelColor(Pantalla[250], 105, 210, 30);
  tira.setPixelColor(Pantalla[249], 105, 210, 30);
  tira.setPixelColor(Pantalla[248], 105, 210, 30);
  tira.setPixelColor(Pantalla[247], 105, 210, 30);
  tira.setPixelColor(Pantalla[246], 105, 210, 30);
  tira.setPixelColor(Pantalla[245], 0, 0, 255);
  tira.setPixelColor(Pantalla[244], 0, 0, 255);
  tira.setPixelColor(Pantalla[243], 0, 0, 255);
  tira.setPixelColor(Pantalla[242], 0, 0, 255);
  tira.setPixelColor(Pantalla[241], 0, 0, 255);
  tira.setPixelColor(Pantalla[240], 0, 0, 255);
}

void Matriz_Aro_6_Disparo()
{
  tira.setPixelColor(Pantalla[0], 255, 0, 255);
  tira.setPixelColor(Pantalla[1], 100, 100, 100);
  tira.setPixelColor(Pantalla[2], 255, 0, 255);
  tira.setPixelColor(Pantalla[3], 255, 0, 255);
  tira.setPixelColor(Pantalla[4], 0, 0, 255);
  tira.setPixelColor(Pantalla[5], 0, 0, 255);
  tira.setPixelColor(Pantalla[6], 0, 0, 255);
  tira.setPixelColor(Pantalla[7], 0, 0, 255);
  tira.setPixelColor(Pantalla[8], 0, 0, 255);
  tira.setPixelColor(Pantalla[9], 0, 0, 255);
  tira.setPixelColor(Pantalla[10], 0, 0, 255);
  tira.setPixelColor(Pantalla[11], 0, 0, 255);
  tira.setPixelColor(Pantalla[12], 255, 0, 255);
  tira.setPixelColor(Pantalla[13], 255, 0, 255);
  tira.setPixelColor(Pantalla[14], 0, 0, 255);
  tira.setPixelColor(Pantalla[15], 0, 0, 255);
  tira.setPixelColor(Pantalla[31], 255, 0, 255);
  tira.setPixelColor(Pantalla[30], 100, 100, 100);
  tira.setPixelColor(Pantalla[29], 100, 100, 100);
  tira.setPixelColor(Pantalla[28], 255, 0, 255);
  tira.setPixelColor(Pantalla[27], 255, 0, 255);
  tira.setPixelColor(Pantalla[26], 255, 0, 255);
  tira.setPixelColor(Pantalla[25], 0, 0, 255);
  tira.setPixelColor(Pantalla[24], 0, 255, 0);
  tira.setPixelColor(Pantalla[23], 0, 255, 0);
  tira.setPixelColor(Pantalla[22], 0, 0, 255);
  tira.setPixelColor(Pantalla[21], 0, 0, 255);
  tira.setPixelColor(Pantalla[20], 255, 0, 255);
  tira.setPixelColor(Pantalla[19], 100, 100, 100);
  tira.setPixelColor(Pantalla[18], 100, 100, 100);
  tira.setPixelColor(Pantalla[17], 255, 0, 255);
  tira.setPixelColor(Pantalla[16], 0, 0, 255);
  tira.setPixelColor(Pantalla[32], 0, 0, 255);
  tira.setPixelColor(Pantalla[33], 255, 0, 255);
  tira.setPixelColor(Pantalla[34], 100, 100, 100);
  tira.setPixelColor(Pantalla[35], 0, 255, 0);
  tira.setPixelColor(Pantalla[36], 100, 100, 100);
  tira.setPixelColor(Pantalla[37], 255, 0, 255);
  tira.setPixelColor(Pantalla[38], 0, 255, 0);
  tira.setPixelColor(Pantalla[39], 0, 255, 0);
  tira.setPixelColor(Pantalla[40], 0, 255, 0);
  tira.setPixelColor(Pantalla[41], 0, 255, 0);
  tira.setPixelColor(Pantalla[42], 0, 255, 0);
  tira.setPixelColor(Pantalla[43], 100, 100, 100);
  tira.setPixelColor(Pantalla[44], 100, 100, 100);
  tira.setPixelColor(Pantalla[45], 100, 100, 100);
  tira.setPixelColor(Pantalla[46], 255, 0, 255);
  tira.setPixelColor(Pantalla[47], 0, 0, 255);
  tira.setPixelColor(Pantalla[63], 0, 0, 255);
  tira.setPixelColor(Pantalla[62], 0, 255, 0);
  tira.setPixelColor(Pantalla[61], 255, 0, 255);
  tira.setPixelColor(Pantalla[60], 100, 100, 100);
  tira.setPixelColor(Pantalla[59], 100, 100, 100);
  tira.setPixelColor(Pantalla[58], 0, 255, 0);
  tira.setPixelColor(Pantalla[57], 0, 255, 0);
  tira.setPixelColor(Pantalla[56], 0, 255, 0);
  tira.setPixelColor(Pantalla[55], 0, 255, 0);
  tira.setPixelColor(Pantalla[54], 0, 255, 0);
  tira.setPixelColor(Pantalla[53], 0, 255, 0);
  tira.setPixelColor(Pantalla[52], 0, 255, 0);
  tira.setPixelColor(Pantalla[51], 100, 100, 100);
  tira.setPixelColor(Pantalla[50], 0, 255, 0);
  tira.setPixelColor(Pantalla[49], 0, 0, 255);
  tira.setPixelColor(Pantalla[48], 255, 0, 255);
  tira.setPixelColor(Pantalla[64], 0, 0, 255);
  tira.setPixelColor(Pantalla[65], 0, 0, 255);
  tira.setPixelColor(Pantalla[66], 255, 0, 255);
  tira.setPixelColor(Pantalla[67], 0, 255, 0);
  tira.setPixelColor(Pantalla[68], 0, 255, 0);
  tira.setPixelColor(Pantalla[69], 0, 255, 0);
  tira.setPixelColor(Pantalla[70], 0, 255, 0);
  tira.setPixelColor(Pantalla[71], 0, 255, 0);
  tira.setPixelColor(Pantalla[72], 0, 255, 0);
  tira.setPixelColor(Pantalla[73], 0, 255, 0);
  tira.setPixelColor(Pantalla[74], 0, 255, 0);
  tira.setPixelColor(Pantalla[75], 0, 255, 0);
  tira.setPixelColor(Pantalla[76], 0, 255, 0);
  tira.setPixelColor(Pantalla[77], 255, 0, 255);
  tira.setPixelColor(Pantalla[78], 0, 0, 255);
  tira.setPixelColor(Pantalla[79], 0, 0, 255);
  tira.setPixelColor(Pantalla[95], 0, 0, 255);
  tira.setPixelColor(Pantalla[94], 255, 0, 255);
  tira.setPixelColor(Pantalla[93], 255, 0, 255);
  tira.setPixelColor(Pantalla[92], 0, 255, 0);
  tira.setPixelColor(Pantalla[91], 0, 255, 0);
  tira.setPixelColor(Pantalla[90], 0, 255, 0);
  tira.setPixelColor(Pantalla[89], 0, 255, 0);
  tira.setPixelColor(Pantalla[88], 0, 0, 0);
  tira.setPixelColor(Pantalla[87], 0, 0, 0);
  tira.setPixelColor(Pantalla[86], 0, 0, 0);
  tira.setPixelColor(Pantalla[85], 0, 255, 0);
  tira.setPixelColor(Pantalla[84], 0, 255, 0);
  tira.setPixelColor(Pantalla[83], 0, 255, 0);
  tira.setPixelColor(Pantalla[82], 0, 255, 0);
  tira.setPixelColor(Pantalla[81], 255, 0, 255);
  tira.setPixelColor(Pantalla[80], 0, 0, 255);
  tira.setPixelColor(Pantalla[96], 255, 0, 255);
  tira.setPixelColor(Pantalla[97], 255, 0, 255);
  tira.setPixelColor(Pantalla[98], 0, 255, 0);
  tira.setPixelColor(Pantalla[99], 0, 255, 0);
  tira.setPixelColor(Pantalla[100], 0, 255, 0);
  tira.setPixelColor(Pantalla[101], 0, 255, 0);
  tira.setPixelColor(Pantalla[102], 0, 0, 0);
  tira.setPixelColor(Pantalla[103], 0, 0, 0);
  tira.setPixelColor(Pantalla[104], 0, 0, 0);
  tira.setPixelColor(Pantalla[105], 0, 0, 0);
  tira.setPixelColor(Pantalla[106], 0, 0, 0);
  tira.setPixelColor(Pantalla[107], 0, 255, 0);
  tira.setPixelColor(Pantalla[108], 0, 255, 0);
  tira.setPixelColor(Pantalla[109], 100, 100, 100);
  tira.setPixelColor(Pantalla[110], 255, 0, 255);
  tira.setPixelColor(Pantalla[111], 255, 0, 255);
  tira.setPixelColor(Pantalla[127], 0, 0, 255);
  tira.setPixelColor(Pantalla[126], 0, 255, 0);
  tira.setPixelColor(Pantalla[125], 100, 100, 100);
  tira.setPixelColor(Pantalla[124], 100, 100, 100);
  tira.setPixelColor(Pantalla[123], 0, 255, 0);
  tira.setPixelColor(Pantalla[122], 0, 255, 0);
  tira.setPixelColor(Pantalla[121], 0, 0, 0);
  tira.setPixelColor(Pantalla[120], 0, 0, 0);
  tira.setPixelColor(Pantalla[119], 0, 0, 0);
  tira.setPixelColor(Pantalla[118], 0, 0, 0);
  tira.setPixelColor(Pantalla[117], 0, 0, 0);
  tira.setPixelColor(Pantalla[116], 0, 0, 0);
  tira.setPixelColor(Pantalla[115], 0, 255, 0);
  tira.setPixelColor(Pantalla[114], 0, 255, 0);
  tira.setPixelColor(Pantalla[113], 255, 0, 255);
  tira.setPixelColor(Pantalla[112], 255, 0, 255);
  tira.setPixelColor(Pantalla[128], 0, 0, 255);
  tira.setPixelColor(Pantalla[129], 0, 255, 0);
  tira.setPixelColor(Pantalla[130], 255, 0, 255);
  tira.setPixelColor(Pantalla[131], 0, 255, 0);
  tira.setPixelColor(Pantalla[132], 0, 255, 0);
  tira.setPixelColor(Pantalla[133], 0, 255, 0);
  tira.setPixelColor(Pantalla[134], 0, 0, 0);
  tira.setPixelColor(Pantalla[135], 0, 0, 0);
  tira.setPixelColor(Pantalla[136], 0, 0, 0);
  tira.setPixelColor(Pantalla[137], 0, 0, 0);
  tira.setPixelColor(Pantalla[138], 0, 0, 0);
  tira.setPixelColor(Pantalla[139], 0, 255, 0);
  tira.setPixelColor(Pantalla[140], 0, 255, 0);
  tira.setPixelColor(Pantalla[141], 0, 255, 0);
  tira.setPixelColor(Pantalla[142], 255, 0, 255);
  tira.setPixelColor(Pantalla[143], 0, 0, 255);
  tira.setPixelColor(Pantalla[159], 0, 0, 255);
  tira.setPixelColor(Pantalla[158], 0, 0, 255);
  tira.setPixelColor(Pantalla[157], 0, 255, 0);
  tira.setPixelColor(Pantalla[156], 255, 0, 255);
  tira.setPixelColor(Pantalla[155], 0, 255, 0);
  tira.setPixelColor(Pantalla[154], 0, 255, 0);
  tira.setPixelColor(Pantalla[153], 0, 0, 0);
  tira.setPixelColor(Pantalla[152], 0, 0, 0);
  tira.setPixelColor(Pantalla[151], 0, 0, 0);
  tira.setPixelColor(Pantalla[150], 0, 0, 0);
  tira.setPixelColor(Pantalla[149], 0, 255, 0);
  tira.setPixelColor(Pantalla[148], 0, 255, 0);
  tira.setPixelColor(Pantalla[147], 255, 0, 255);
  tira.setPixelColor(Pantalla[146], 255, 0, 255);
  tira.setPixelColor(Pantalla[145], 0, 255, 0);
  tira.setPixelColor(Pantalla[144], 0, 0, 255);
  tira.setPixelColor(Pantalla[160], 0, 0, 255);
  tira.setPixelColor(Pantalla[161], 0, 0, 255);
  tira.setPixelColor(Pantalla[162], 0, 255, 0);
  tira.setPixelColor(Pantalla[163], 0, 255, 0);
  tira.setPixelColor(Pantalla[164], 100, 100, 100);
  tira.setPixelColor(Pantalla[165], 0, 255, 0);
  tira.setPixelColor(Pantalla[166], 0, 255, 0);
  tira.setPixelColor(Pantalla[167], 0, 255, 0);
  tira.setPixelColor(Pantalla[168], 0, 255, 0);
  tira.setPixelColor(Pantalla[169], 0, 255, 0);
  tira.setPixelColor(Pantalla[170], 0, 255, 0);
  tira.setPixelColor(Pantalla[171], 0, 255, 0);
  tira.setPixelColor(Pantalla[172], 100, 100, 100);
  tira.setPixelColor(Pantalla[173], 255, 0, 255);
  tira.setPixelColor(Pantalla[174], 255, 0, 255);
  tira.setPixelColor(Pantalla[175], 0, 0, 255);
  tira.setPixelColor(Pantalla[191], 0, 0, 255);
  tira.setPixelColor(Pantalla[190], 255, 0, 255);
  tira.setPixelColor(Pantalla[189], 100, 100, 100);
  tira.setPixelColor(Pantalla[188], 100, 100, 100);
  tira.setPixelColor(Pantalla[187], 100, 100, 100);
  tira.setPixelColor(Pantalla[186], 0, 255, 0);
  tira.setPixelColor(Pantalla[185], 255, 0, 255);
  tira.setPixelColor(Pantalla[184], 0, 255, 0);
  tira.setPixelColor(Pantalla[183], 0, 255, 0);
  tira.setPixelColor(Pantalla[182], 0, 255, 0);
  tira.setPixelColor(Pantalla[181], 0, 255, 0);
  tira.setPixelColor(Pantalla[180], 0, 255, 0);
  tira.setPixelColor(Pantalla[179], 0, 255, 0);
  tira.setPixelColor(Pantalla[178], 0, 255, 0);
  tira.setPixelColor(Pantalla[177], 0, 255, 0);
  tira.setPixelColor(Pantalla[176], 255, 0, 255);
  tira.setPixelColor(Pantalla[192], 255, 0, 255);
  tira.setPixelColor(Pantalla[193], 100, 100, 100);
  tira.setPixelColor(Pantalla[194], 100, 100, 100);
  tira.setPixelColor(Pantalla[195], 0, 255, 0);
  tira.setPixelColor(Pantalla[196], 0, 255, 0);
  tira.setPixelColor(Pantalla[197], 255, 0, 255);
  tira.setPixelColor(Pantalla[198], 0, 255, 0);
  tira.setPixelColor(Pantalla[199], 100, 100, 100);
  tira.setPixelColor(Pantalla[200], 0, 255, 0);
  tira.setPixelColor(Pantalla[201], 0, 255, 0);
  tira.setPixelColor(Pantalla[202], 0, 255, 0);
  tira.setPixelColor(Pantalla[203], 255, 0, 255);
  tira.setPixelColor(Pantalla[204], 100, 100, 100);
  tira.setPixelColor(Pantalla[205], 100, 100, 100);
  tira.setPixelColor(Pantalla[206], 100, 100, 100);
  tira.setPixelColor(Pantalla[207], 255, 0, 255);
  tira.setPixelColor(Pantalla[223], 255, 0, 255);
  tira.setPixelColor(Pantalla[222], 100, 100, 100);
  tira.setPixelColor(Pantalla[221], 0, 255, 0);
  tira.setPixelColor(Pantalla[220], 0, 255, 0);
  tira.setPixelColor(Pantalla[219], 0, 255, 0);
  tira.setPixelColor(Pantalla[218], 255, 0, 255);
  tira.setPixelColor(Pantalla[217], 0, 0, 255);
  tira.setPixelColor(Pantalla[216], 255, 0, 255);
  tira.setPixelColor(Pantalla[215], 0, 255, 0);
  tira.setPixelColor(Pantalla[214], 100, 100, 100);
  tira.setPixelColor(Pantalla[213], 255, 0, 255);
  tira.setPixelColor(Pantalla[212], 0, 0, 255);
  tira.setPixelColor(Pantalla[211], 255, 0, 255);
  tira.setPixelColor(Pantalla[210], 0, 255, 0);
  tira.setPixelColor(Pantalla[209], 100, 100, 100);
  tira.setPixelColor(Pantalla[208], 255, 0, 255);
  tira.setPixelColor(Pantalla[224], 255, 0, 255);
  tira.setPixelColor(Pantalla[225], 255, 0, 255);
  tira.setPixelColor(Pantalla[226], 100, 100, 100);
  tira.setPixelColor(Pantalla[227], 255, 0, 255);
  tira.setPixelColor(Pantalla[228], 0, 0, 255);
  tira.setPixelColor(Pantalla[229], 0, 0, 255);
  tira.setPixelColor(Pantalla[230], 0, 0, 255);
  tira.setPixelColor(Pantalla[231], 255, 0, 255);
  tira.setPixelColor(Pantalla[232], 255, 0, 255);
  tira.setPixelColor(Pantalla[233], 255, 0, 255);
  tira.setPixelColor(Pantalla[234], 255, 0, 255);
  tira.setPixelColor(Pantalla[235], 0, 0, 255);
  tira.setPixelColor(Pantalla[236], 255, 0, 255);
  tira.setPixelColor(Pantalla[237], 255, 0, 255);
  tira.setPixelColor(Pantalla[238], 255, 0, 255);
  tira.setPixelColor(Pantalla[239], 0, 0, 255);
  tira.setPixelColor(Pantalla[255], 0, 0, 255);
  tira.setPixelColor(Pantalla[254], 0, 0, 255);
  tira.setPixelColor(Pantalla[253], 255, 0, 255);
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

void Matriz_Aro_7_Disparo()
{
  tira.setPixelColor(Pantalla[0], 0, 0, 0);
  tira.setPixelColor(Pantalla[1], 0, 0, 0);
  tira.setPixelColor(Pantalla[2], 0, 0, 0);
  tira.setPixelColor(Pantalla[3], 0, 0, 0);
  tira.setPixelColor(Pantalla[4], 0, 0, 0);
  tira.setPixelColor(Pantalla[5], 0, 255, 0);
  tira.setPixelColor(Pantalla[6], 0, 255, 0);
  tira.setPixelColor(Pantalla[7], 0, 255, 0);
  tira.setPixelColor(Pantalla[8], 0, 255, 0);
  tira.setPixelColor(Pantalla[9], 0, 255, 0);
  tira.setPixelColor(Pantalla[10], 0, 255, 0);
  tira.setPixelColor(Pantalla[11], 0, 0, 0);
  tira.setPixelColor(Pantalla[12], 0, 0, 0);
  tira.setPixelColor(Pantalla[13], 0, 0, 0);
  tira.setPixelColor(Pantalla[14], 0, 255, 0);
  tira.setPixelColor(Pantalla[15], 0, 0, 0);
  tira.setPixelColor(Pantalla[31], 0, 0, 0);
  tira.setPixelColor(Pantalla[30], 0, 255, 0);
  tira.setPixelColor(Pantalla[29], 0, 255, 0);
  tira.setPixelColor(Pantalla[28], 0, 255, 0);
  tira.setPixelColor(Pantalla[27], 0, 255, 0);
  tira.setPixelColor(Pantalla[26], 0, 255, 0);
  tira.setPixelColor(Pantalla[25], 0, 255, 0);
  tira.setPixelColor(Pantalla[24], 0, 255, 0);
  tira.setPixelColor(Pantalla[23], 0, 255, 0);
  tira.setPixelColor(Pantalla[22], 0, 255, 0);
  tira.setPixelColor(Pantalla[21], 0, 255, 0);
  tira.setPixelColor(Pantalla[20], 0, 255, 0);
  tira.setPixelColor(Pantalla[19], 0, 255, 0);
  tira.setPixelColor(Pantalla[18], 0, 255, 0);
  tira.setPixelColor(Pantalla[17], 0, 0, 0);
  tira.setPixelColor(Pantalla[16], 0, 0, 0);
  tira.setPixelColor(Pantalla[32], 0, 0, 0);
  tira.setPixelColor(Pantalla[33], 0, 0, 0);
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
  tira.setPixelColor(Pantalla[46], 0, 0, 0);
  tira.setPixelColor(Pantalla[47], 0, 0, 0);
  tira.setPixelColor(Pantalla[63], 0, 255, 0);
  tira.setPixelColor(Pantalla[62], 0, 255, 0);
  tira.setPixelColor(Pantalla[61], 0, 255, 0);
  tira.setPixelColor(Pantalla[60], 0, 255, 0);
  tira.setPixelColor(Pantalla[59], 0, 255, 0);
  tira.setPixelColor(Pantalla[58], 0, 255, 0);
  tira.setPixelColor(Pantalla[57], 0, 255, 0);
  tira.setPixelColor(Pantalla[56], 0, 0, 0);
  tira.setPixelColor(Pantalla[55], 0, 0, 0);
  tira.setPixelColor(Pantalla[54], 0, 255, 0);
  tira.setPixelColor(Pantalla[53], 0, 255, 0);
  tira.setPixelColor(Pantalla[52], 0, 255, 0);
  tira.setPixelColor(Pantalla[51], 0, 255, 0);
  tira.setPixelColor(Pantalla[50], 0, 255, 0);
  tira.setPixelColor(Pantalla[49], 0, 255, 0);
  tira.setPixelColor(Pantalla[48], 0, 0, 0);
  tira.setPixelColor(Pantalla[64], 0, 255, 0);
  tira.setPixelColor(Pantalla[65], 0, 255, 0);
  tira.setPixelColor(Pantalla[66], 0, 255, 0);
  tira.setPixelColor(Pantalla[67], 0, 255, 0);
  tira.setPixelColor(Pantalla[68], 0, 255, 0);
  tira.setPixelColor(Pantalla[69], 0, 0, 0);
  tira.setPixelColor(Pantalla[70], 0, 0, 0);
  tira.setPixelColor(Pantalla[71], 0, 0, 0);
  tira.setPixelColor(Pantalla[72], 0, 0, 0);
  tira.setPixelColor(Pantalla[73], 0, 0, 0);
  tira.setPixelColor(Pantalla[74], 0, 0, 0);
  tira.setPixelColor(Pantalla[75], 0, 255, 0);
  tira.setPixelColor(Pantalla[76], 0, 255, 0);
  tira.setPixelColor(Pantalla[77], 0, 255, 0);
  tira.setPixelColor(Pantalla[78], 0, 255, 0);
  tira.setPixelColor(Pantalla[79], 0, 0, 0);
  tira.setPixelColor(Pantalla[95], 0, 0, 0);
  tira.setPixelColor(Pantalla[94], 0, 255, 0);
  tira.setPixelColor(Pantalla[93], 0, 255, 0);
  tira.setPixelColor(Pantalla[92], 0, 255, 0);
  tira.setPixelColor(Pantalla[91], 0, 0, 0);
  tira.setPixelColor(Pantalla[90], 0, 0, 0);
  tira.setPixelColor(Pantalla[89], 0, 0, 0);
  tira.setPixelColor(Pantalla[88], 0, 0, 0);
  tira.setPixelColor(Pantalla[87], 0, 0, 0);
  tira.setPixelColor(Pantalla[86], 0, 0, 0);
  tira.setPixelColor(Pantalla[85], 0, 0, 0);
  tira.setPixelColor(Pantalla[84], 0, 0, 0);
  tira.setPixelColor(Pantalla[83], 0, 255, 0);
  tira.setPixelColor(Pantalla[82], 0, 255, 0);
  tira.setPixelColor(Pantalla[81], 0, 255, 0);
  tira.setPixelColor(Pantalla[80], 0, 255, 0);
  tira.setPixelColor(Pantalla[96], 0, 255, 0);
  tira.setPixelColor(Pantalla[97], 0, 255, 0);
  tira.setPixelColor(Pantalla[98], 0, 255, 0);
  tira.setPixelColor(Pantalla[99], 0, 255, 0);
  tira.setPixelColor(Pantalla[100], 0, 0, 0);
  tira.setPixelColor(Pantalla[101], 0, 0, 0);
  tira.setPixelColor(Pantalla[102], 0, 0, 0);
  tira.setPixelColor(Pantalla[103], 0, 0, 0);
  tira.setPixelColor(Pantalla[104], 0, 0, 0);
  tira.setPixelColor(Pantalla[105], 0, 0, 0);
  tira.setPixelColor(Pantalla[106], 0, 0, 0);
  tira.setPixelColor(Pantalla[107], 0, 0, 0);
  tira.setPixelColor(Pantalla[108], 0, 255, 0);
  tira.setPixelColor(Pantalla[109], 0, 255, 0);
  tira.setPixelColor(Pantalla[110], 0, 255, 0);
  tira.setPixelColor(Pantalla[111], 0, 255, 0);
  tira.setPixelColor(Pantalla[127], 0, 255, 0);
  tira.setPixelColor(Pantalla[126], 0, 255, 0);
  tira.setPixelColor(Pantalla[125], 0, 255, 0);
  tira.setPixelColor(Pantalla[124], 0, 255, 0);
  tira.setPixelColor(Pantalla[123], 0, 0, 0);
  tira.setPixelColor(Pantalla[122], 0, 0, 0);
  tira.setPixelColor(Pantalla[121], 0, 0, 0);
  tira.setPixelColor(Pantalla[120], 0, 0, 0);
  tira.setPixelColor(Pantalla[119], 0, 0, 0);
  tira.setPixelColor(Pantalla[118], 0, 0, 0);
  tira.setPixelColor(Pantalla[117], 0, 0, 0);
  tira.setPixelColor(Pantalla[116], 0, 0, 0);
  tira.setPixelColor(Pantalla[115], 0, 255, 0);
  tira.setPixelColor(Pantalla[114], 0, 255, 0);
  tira.setPixelColor(Pantalla[113], 0, 255, 0);
  tira.setPixelColor(Pantalla[112], 0, 255, 0);
  tira.setPixelColor(Pantalla[128], 0, 0, 0);
  tira.setPixelColor(Pantalla[129], 0, 255, 0);
  tira.setPixelColor(Pantalla[130], 0, 255, 0);
  tira.setPixelColor(Pantalla[131], 0, 255, 0);
  tira.setPixelColor(Pantalla[132], 0, 0, 0);
  tira.setPixelColor(Pantalla[133], 0, 0, 0);
  tira.setPixelColor(Pantalla[134], 0, 0, 0);
  tira.setPixelColor(Pantalla[135], 0, 0, 0);
  tira.setPixelColor(Pantalla[136], 0, 0, 0);
  tira.setPixelColor(Pantalla[137], 0, 0, 0);
  tira.setPixelColor(Pantalla[138], 0, 0, 0);
  tira.setPixelColor(Pantalla[139], 0, 0, 0);
  tira.setPixelColor(Pantalla[140], 0, 255, 0);
  tira.setPixelColor(Pantalla[141], 0, 255, 0);
  tira.setPixelColor(Pantalla[142], 0, 255, 0);
  tira.setPixelColor(Pantalla[143], 0, 0, 0);
  tira.setPixelColor(Pantalla[159], 0, 0, 0);
  tira.setPixelColor(Pantalla[158], 0, 255, 0);
  tira.setPixelColor(Pantalla[157], 0, 255, 0);
  tira.setPixelColor(Pantalla[156], 0, 255, 0);
  tira.setPixelColor(Pantalla[155], 0, 255, 0);
  tira.setPixelColor(Pantalla[154], 0, 0, 0);
  tira.setPixelColor(Pantalla[153], 0, 0, 0);
  tira.setPixelColor(Pantalla[152], 0, 0, 0);
  tira.setPixelColor(Pantalla[151], 0, 0, 0);
  tira.setPixelColor(Pantalla[150], 0, 0, 0);
  tira.setPixelColor(Pantalla[149], 0, 0, 0);
  tira.setPixelColor(Pantalla[148], 0, 255, 0);
  tira.setPixelColor(Pantalla[147], 0, 255, 0);
  tira.setPixelColor(Pantalla[146], 0, 255, 0);
  tira.setPixelColor(Pantalla[145], 0, 255, 0);
  tira.setPixelColor(Pantalla[144], 0, 255, 0);
  tira.setPixelColor(Pantalla[160], 0, 0, 0);
  tira.setPixelColor(Pantalla[161], 0, 255, 0);
  tira.setPixelColor(Pantalla[162], 0, 255, 0);
  tira.setPixelColor(Pantalla[163], 0, 255, 0);
  tira.setPixelColor(Pantalla[164], 0, 255, 0);
  tira.setPixelColor(Pantalla[165], 0, 255, 0);
  tira.setPixelColor(Pantalla[166], 0, 255, 0);
  tira.setPixelColor(Pantalla[167], 0, 0, 0);
  tira.setPixelColor(Pantalla[168], 0, 0, 0);
  tira.setPixelColor(Pantalla[169], 0, 0, 0);
  tira.setPixelColor(Pantalla[170], 0, 255, 0);
  tira.setPixelColor(Pantalla[171], 0, 255, 0);
  tira.setPixelColor(Pantalla[172], 0, 255, 0);
  tira.setPixelColor(Pantalla[173], 0, 255, 0);
  tira.setPixelColor(Pantalla[174], 0, 0, 0);
  tira.setPixelColor(Pantalla[175], 0, 0, 0);
  tira.setPixelColor(Pantalla[191], 0, 0, 0);
  tira.setPixelColor(Pantalla[190], 0, 255, 0);
  tira.setPixelColor(Pantalla[189], 0, 255, 0);
  tira.setPixelColor(Pantalla[188], 0, 255, 0);
  tira.setPixelColor(Pantalla[187], 0, 255, 0);
  tira.setPixelColor(Pantalla[186], 0, 255, 0);
  tira.setPixelColor(Pantalla[185], 0, 255, 0);
  tira.setPixelColor(Pantalla[184], 0, 255, 0);
  tira.setPixelColor(Pantalla[183], 0, 255, 0);
  tira.setPixelColor(Pantalla[182], 0, 255, 0);
  tira.setPixelColor(Pantalla[181], 0, 255, 0);
  tira.setPixelColor(Pantalla[180], 0, 255, 0);
  tira.setPixelColor(Pantalla[179], 0, 255, 0);
  tira.setPixelColor(Pantalla[178], 0, 255, 0);
  tira.setPixelColor(Pantalla[177], 0, 0, 0);
  tira.setPixelColor(Pantalla[176], 0, 0, 0);
  tira.setPixelColor(Pantalla[192], 0, 0, 0);
  tira.setPixelColor(Pantalla[193], 0, 255, 0);
  tira.setPixelColor(Pantalla[194], 0, 255, 0);
  tira.setPixelColor(Pantalla[195], 0, 255, 0);
  tira.setPixelColor(Pantalla[196], 0, 255, 0);
  tira.setPixelColor(Pantalla[197], 0, 255, 0);
  tira.setPixelColor(Pantalla[198], 0, 255, 0);
  tira.setPixelColor(Pantalla[199], 0, 255, 0);
  tira.setPixelColor(Pantalla[200], 0, 255, 0);
  tira.setPixelColor(Pantalla[201], 0, 255, 0);
  tira.setPixelColor(Pantalla[202], 0, 255, 0);
  tira.setPixelColor(Pantalla[203], 0, 255, 0);
  tira.setPixelColor(Pantalla[204], 0, 255, 0);
  tira.setPixelColor(Pantalla[205], 0, 255, 0);
  tira.setPixelColor(Pantalla[206], 0, 0, 0);
  tira.setPixelColor(Pantalla[207], 0, 0, 0);
  tira.setPixelColor(Pantalla[223], 0, 0, 0);
  tira.setPixelColor(Pantalla[222], 0, 0, 0);
  tira.setPixelColor(Pantalla[221], 0, 0, 0);
  tira.setPixelColor(Pantalla[220], 0, 255, 0);
  tira.setPixelColor(Pantalla[219], 0, 255, 0);
  tira.setPixelColor(Pantalla[218], 0, 255, 0);
  tira.setPixelColor(Pantalla[217], 0, 255, 0);
  tira.setPixelColor(Pantalla[216], 0, 255, 0);
  tira.setPixelColor(Pantalla[215], 0, 255, 0);
  tira.setPixelColor(Pantalla[214], 0, 255, 0);
  tira.setPixelColor(Pantalla[213], 0, 255, 0);
  tira.setPixelColor(Pantalla[212], 0, 255, 0);
  tira.setPixelColor(Pantalla[211], 0, 255, 0);
  tira.setPixelColor(Pantalla[210], 0, 255, 0);
  tira.setPixelColor(Pantalla[209], 0, 255, 0);
  tira.setPixelColor(Pantalla[208], 0, 255, 0);
  tira.setPixelColor(Pantalla[224], 0, 0, 0);
  tira.setPixelColor(Pantalla[225], 0, 255, 0);
  tira.setPixelColor(Pantalla[226], 0, 255, 0);
  tira.setPixelColor(Pantalla[227], 0, 255, 0);
  tira.setPixelColor(Pantalla[228], 0, 255, 0);
  tira.setPixelColor(Pantalla[229], 0, 0, 0);
  tira.setPixelColor(Pantalla[230], 0, 255, 0);
  tira.setPixelColor(Pantalla[231], 0, 255, 0);
  tira.setPixelColor(Pantalla[232], 0, 255, 0);
  tira.setPixelColor(Pantalla[233], 0, 255, 0);
  tira.setPixelColor(Pantalla[234], 0, 0, 0);
  tira.setPixelColor(Pantalla[235], 0, 255, 0);
  tira.setPixelColor(Pantalla[236], 0, 0, 0);
  tira.setPixelColor(Pantalla[237], 0, 255, 0);
  tira.setPixelColor(Pantalla[238], 0, 255, 0);
  tira.setPixelColor(Pantalla[239], 0, 0, 0);
  tira.setPixelColor(Pantalla[255], 0, 0, 0);
  tira.setPixelColor(Pantalla[254], 0, 0, 0);
  tira.setPixelColor(Pantalla[253], 0, 0, 0);
  tira.setPixelColor(Pantalla[252], 0, 0, 0);
  tira.setPixelColor(Pantalla[251], 0, 0, 0);
  tira.setPixelColor(Pantalla[250], 0, 0, 0);
  tira.setPixelColor(Pantalla[249], 0, 0, 0);
  tira.setPixelColor(Pantalla[248], 0, 0, 0);
  tira.setPixelColor(Pantalla[247], 0, 0, 0);
  tira.setPixelColor(Pantalla[246], 0, 0, 0);
  tira.setPixelColor(Pantalla[245], 0, 0, 0);
  tira.setPixelColor(Pantalla[244], 0, 0, 0);
  tira.setPixelColor(Pantalla[243], 0, 0, 0);
  tira.setPixelColor(Pantalla[242], 0, 0, 0);
  tira.setPixelColor(Pantalla[241], 0, 0, 0);
  tira.setPixelColor(Pantalla[240], 0, 0, 0);
}

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
    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
  //   tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
  //   tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
  //   tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
  //   tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
  //   tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
 }
}

void Matriz_Calibra()
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

    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
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
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
  }
}

void Matriz_Pinguino()
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

void Matriz_Corazon_400()
{
  if (lado == IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[14], 100, 100, 100);
    tira.setPixelColor(Pantalla[13], 100, 100, 100);
    tira.setPixelColor(Pantalla[12], 100, 100, 100);
    tira.setPixelColor(Pantalla[4], 100, 100, 100);
    tira.setPixelColor(Pantalla[3], 100, 100, 100);
    tira.setPixelColor(Pantalla[2], 100, 100, 100);
    tira.setPixelColor(Pantalla[18], 100, 100, 100);
    tira.setPixelColor(Pantalla[19], 100, 100, 100);
    tira.setPixelColor(Pantalla[20], 100, 100, 100);
    tira.setPixelColor(Pantalla[22], 255, 255, 0);
    tira.setPixelColor(Pantalla[23], 100, 100, 100);
    tira.setPixelColor(Pantalla[24], 255, 255, 0);
    tira.setPixelColor(Pantalla[26], 100, 100, 100);
    tira.setPixelColor(Pantalla[27], 100, 100, 100);
    tira.setPixelColor(Pantalla[28], 100, 100, 100);
    tira.setPixelColor(Pantalla[44], 100, 100, 100);
    tira.setPixelColor(Pantalla[43], 100, 100, 100);
    tira.setPixelColor(Pantalla[42], 100, 100, 100);
    tira.setPixelColor(Pantalla[41], 0, 0, 255);
    tira.setPixelColor(Pantalla[40], 255, 255, 0);
    tira.setPixelColor(Pantalla[39], 0, 0, 255);
    tira.setPixelColor(Pantalla[38], 100, 100, 100);
    tira.setPixelColor(Pantalla[37], 100, 100, 100);
    tira.setPixelColor(Pantalla[36], 100, 100, 100);
    tira.setPixelColor(Pantalla[52], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], 100, 100, 100);
    tira.setPixelColor(Pantalla[54], 0, 0, 255);
    tira.setPixelColor(Pantalla[55], 0, 0, 255);
    tira.setPixelColor(Pantalla[56], 0, 0, 255);
    tira.setPixelColor(Pantalla[57], 100, 100, 100);
    tira.setPixelColor(Pantalla[58], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], 0, 0, 255);
    tira.setPixelColor(Pantalla[72], 0, 0, 255);
    tira.setPixelColor(Pantalla[71], 0, 0, 255);
    tira.setPixelColor(Pantalla[82], 255, 255, 0);
    tira.setPixelColor(Pantalla[85], 0, 0, 255);
    tira.setPixelColor(Pantalla[86], 0, 0, 255);
    tira.setPixelColor(Pantalla[87], 0, 0, 255);
    tira.setPixelColor(Pantalla[88], 0, 0, 255);
    tira.setPixelColor(Pantalla[89], 0, 0, 255);
    tira.setPixelColor(Pantalla[92], 255, 255, 0);
    tira.setPixelColor(Pantalla[110], 255, 255, 0);
    tira.setPixelColor(Pantalla[109], 255, 255, 0);
    tira.setPixelColor(Pantalla[108], 255, 255, 0);
    tira.setPixelColor(Pantalla[106], 0, 0, 255);
    tira.setPixelColor(Pantalla[105], 0, 0, 255);
    tira.setPixelColor(Pantalla[104], 0, 0, 255);
    tira.setPixelColor(Pantalla[103], 0, 0, 255);
    tira.setPixelColor(Pantalla[102], 0, 0, 255);
    tira.setPixelColor(Pantalla[100], 255, 255, 0);
    tira.setPixelColor(Pantalla[99], 255, 255, 0);
    tira.setPixelColor(Pantalla[98], 255, 255, 0);
    tira.setPixelColor(Pantalla[114], 255, 255, 0);
    tira.setPixelColor(Pantalla[118], 105, 210, 30);
    tira.setPixelColor(Pantalla[120], 105, 210, 30);
    tira.setPixelColor(Pantalla[124], 255, 255, 0);

    tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[148], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[150], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[151], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[152], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[154], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[155], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[156], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[173], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[171], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[169], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[167], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[165], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[163], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[184], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[186], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[188], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[203], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[201], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[199], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[197], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[195], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[212], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[214], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[215], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[216], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[218], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[219], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[220], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));

    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
  }
  else
  {
    tira.setPixelColor(Pantalla[22], 255, 255, 0);
    tira.setPixelColor(Pantalla[23], 100, 100, 100);
    tira.setPixelColor(Pantalla[24], 255, 255, 0);
    tira.setPixelColor(Pantalla[45], 100, 100, 100);
    tira.setPixelColor(Pantalla[44], 100, 100, 100);
    tira.setPixelColor(Pantalla[43], 100, 100, 100);
    tira.setPixelColor(Pantalla[42], 100, 100, 100);
    tira.setPixelColor(Pantalla[41], 0, 0, 255);
    tira.setPixelColor(Pantalla[40], 255, 255, 0);
    tira.setPixelColor(Pantalla[39], 0, 0, 255);
    tira.setPixelColor(Pantalla[38], 100, 100, 100);
    tira.setPixelColor(Pantalla[37], 100, 100, 100);
    tira.setPixelColor(Pantalla[36], 100, 100, 100);
    tira.setPixelColor(Pantalla[35], 100, 100, 100);
    tira.setPixelColor(Pantalla[49], 100, 100, 100);
    tira.setPixelColor(Pantalla[50], 100, 100, 100);
    tira.setPixelColor(Pantalla[51], 100, 100, 100);
    tira.setPixelColor(Pantalla[52], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], 100, 100, 100);
    tira.setPixelColor(Pantalla[54], 0, 0, 255);
    tira.setPixelColor(Pantalla[55], 0, 0, 255);
    tira.setPixelColor(Pantalla[56], 0, 0, 255);
    tira.setPixelColor(Pantalla[57], 100, 100, 100);
    tira.setPixelColor(Pantalla[58], 100, 100, 100);
    tira.setPixelColor(Pantalla[59], 100, 100, 100);
    tira.setPixelColor(Pantalla[60], 100, 100, 100);
    tira.setPixelColor(Pantalla[61], 100, 100, 100);
    tira.setPixelColor(Pantalla[79], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], 0, 0, 255);
    tira.setPixelColor(Pantalla[72], 0, 0, 255);
    tira.setPixelColor(Pantalla[71], 0, 0, 255);
    tira.setPixelColor(Pantalla[65], 100, 100, 100);
    tira.setPixelColor(Pantalla[82], 255, 255, 0);
    tira.setPixelColor(Pantalla[85], 0, 0, 255);
    tira.setPixelColor(Pantalla[86], 0, 0, 255);
    tira.setPixelColor(Pantalla[87], 0, 0, 255);
    tira.setPixelColor(Pantalla[88], 0, 0, 255);
    tira.setPixelColor(Pantalla[89], 0, 0, 255);
    tira.setPixelColor(Pantalla[92], 255, 255, 0);
    tira.setPixelColor(Pantalla[110], 255, 255, 0);
    tira.setPixelColor(Pantalla[109], 255, 255, 0);
    tira.setPixelColor(Pantalla[108], 255, 255, 0);
    tira.setPixelColor(Pantalla[106], 0, 0, 255);
    tira.setPixelColor(Pantalla[105], 0, 0, 255);
    tira.setPixelColor(Pantalla[104], 0, 0, 255);
    tira.setPixelColor(Pantalla[103], 0, 0, 255);
    tira.setPixelColor(Pantalla[102], 0, 0, 255);
    tira.setPixelColor(Pantalla[100], 255, 255, 0);
    tira.setPixelColor(Pantalla[99], 255, 255, 0);
    tira.setPixelColor(Pantalla[98], 255, 255, 0);
    tira.setPixelColor(Pantalla[114], 255, 255, 0);
    tira.setPixelColor(Pantalla[118], 105, 210, 30);
    tira.setPixelColor(Pantalla[120], 105, 210, 30);
    tira.setPixelColor(Pantalla[124], 255, 255, 0);

    tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[148], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[150], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[151], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[152], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[154], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[155], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[156], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[173], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[171], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[169], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[167], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[165], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[163], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[184], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[186], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[188], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[203], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[201], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[199], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[197], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[195], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[212], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[214], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[215], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[216], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[218], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[219], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[220], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));

    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
  }
}

void Matriz_Corazon_300()
{
  if (lado == IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[14], 100, 100, 100);
    tira.setPixelColor(Pantalla[13], 100, 100, 100);
    tira.setPixelColor(Pantalla[12], 100, 100, 100);
    tira.setPixelColor(Pantalla[4], 100, 100, 100);
    tira.setPixelColor(Pantalla[3], 100, 100, 100);
    tira.setPixelColor(Pantalla[2], 100, 100, 100);
    tira.setPixelColor(Pantalla[18], 100, 100, 100);
    tira.setPixelColor(Pantalla[19], 100, 100, 100);
    tira.setPixelColor(Pantalla[20], 100, 100, 100);
    tira.setPixelColor(Pantalla[22], 255, 255, 0);
    tira.setPixelColor(Pantalla[23], 100, 100, 100);
    tira.setPixelColor(Pantalla[24], 255, 255, 0);
    tira.setPixelColor(Pantalla[26], 100, 100, 100);
    tira.setPixelColor(Pantalla[27], 100, 100, 100);
    tira.setPixelColor(Pantalla[28], 100, 100, 100);
    tira.setPixelColor(Pantalla[44], 100, 100, 100);
    tira.setPixelColor(Pantalla[43], 100, 100, 100);
    tira.setPixelColor(Pantalla[42], 100, 100, 100);
    tira.setPixelColor(Pantalla[41], 0, 0, 255);
    tira.setPixelColor(Pantalla[40], 255, 255, 0);
    tira.setPixelColor(Pantalla[39], 0, 0, 255);
    tira.setPixelColor(Pantalla[38], 100, 100, 100);
    tira.setPixelColor(Pantalla[37], 100, 100, 100);
    tira.setPixelColor(Pantalla[36], 100, 100, 100);
    tira.setPixelColor(Pantalla[52], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], 100, 100, 100);
    tira.setPixelColor(Pantalla[54], 0, 0, 255);
    tira.setPixelColor(Pantalla[55], 0, 0, 255);
    tira.setPixelColor(Pantalla[56], 0, 0, 255);
    tira.setPixelColor(Pantalla[57], 100, 100, 100);
    tira.setPixelColor(Pantalla[58], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], 0, 0, 255);
    tira.setPixelColor(Pantalla[72], 0, 0, 255);
    tira.setPixelColor(Pantalla[71], 0, 0, 255);
    tira.setPixelColor(Pantalla[82], 255, 255, 0);
    tira.setPixelColor(Pantalla[85], 0, 0, 255);
    tira.setPixelColor(Pantalla[86], 0, 0, 255);
    tira.setPixelColor(Pantalla[87], 0, 0, 255);
    tira.setPixelColor(Pantalla[88], 0, 0, 255);
    tira.setPixelColor(Pantalla[89], 0, 0, 255);
    tira.setPixelColor(Pantalla[110], 255, 255, 0);
    tira.setPixelColor(Pantalla[109], 255, 255, 0);
    tira.setPixelColor(Pantalla[108], 255, 255, 0);
    tira.setPixelColor(Pantalla[106], 0, 0, 255);
    tira.setPixelColor(Pantalla[105], 0, 0, 255);
    tira.setPixelColor(Pantalla[104], 0, 0, 255);
    tira.setPixelColor(Pantalla[103], 0, 0, 255);
    tira.setPixelColor(Pantalla[102], 0, 0, 255);
    tira.setPixelColor(Pantalla[114], 255, 255, 0);
    tira.setPixelColor(Pantalla[118], 105, 210, 30);
    tira.setPixelColor(Pantalla[120], 105, 210, 30);

    tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[147], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[148], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[150], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[151], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[152], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[154], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[155], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[156], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[171], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[169], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[167], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[165], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[163], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[184], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[186], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[188], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[203], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[201], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[199], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[197], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[195], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[210], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[211], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[212], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[214], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[215], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[216], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[218], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[219], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[220], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));

    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
  }
  else
  {
    tira.setPixelColor(Pantalla[22], 255, 255, 0);
    tira.setPixelColor(Pantalla[23], 100, 100, 100);
    tira.setPixelColor(Pantalla[24], 255, 255, 0);
    tira.setPixelColor(Pantalla[45], 100, 100, 100);
    tira.setPixelColor(Pantalla[44], 100, 100, 100);
    tira.setPixelColor(Pantalla[43], 100, 100, 100);
    tira.setPixelColor(Pantalla[42], 100, 100, 100);
    tira.setPixelColor(Pantalla[41], 0, 0, 255);
    tira.setPixelColor(Pantalla[40], 255, 255, 0);
    tira.setPixelColor(Pantalla[39], 0, 0, 255);
    tira.setPixelColor(Pantalla[38], 100, 100, 100);
    tira.setPixelColor(Pantalla[37], 100, 100, 100);
    tira.setPixelColor(Pantalla[36], 100, 100, 100);
    tira.setPixelColor(Pantalla[35], 100, 100, 100);
    tira.setPixelColor(Pantalla[49], 100, 100, 100);
    tira.setPixelColor(Pantalla[50], 100, 100, 100);
    tira.setPixelColor(Pantalla[51], 100, 100, 100);
    tira.setPixelColor(Pantalla[52], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], 100, 100, 100);
    tira.setPixelColor(Pantalla[54], 0, 0, 255);
    tira.setPixelColor(Pantalla[55], 0, 0, 255);
    tira.setPixelColor(Pantalla[56], 0, 0, 255);
    tira.setPixelColor(Pantalla[57], 100, 100, 100);
    tira.setPixelColor(Pantalla[58], 100, 100, 100);
    tira.setPixelColor(Pantalla[59], 100, 100, 100);
    tira.setPixelColor(Pantalla[60], 100, 100, 100);
    tira.setPixelColor(Pantalla[61], 100, 100, 100);
    tira.setPixelColor(Pantalla[79], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], 0, 0, 255);
    tira.setPixelColor(Pantalla[72], 0, 0, 255);
    tira.setPixelColor(Pantalla[71], 0, 0, 255);
    tira.setPixelColor(Pantalla[65], 100, 100, 100);
    tira.setPixelColor(Pantalla[82], 255, 255, 0);
    tira.setPixelColor(Pantalla[85], 0, 0, 255);
    tira.setPixelColor(Pantalla[86], 0, 0, 255);
    tira.setPixelColor(Pantalla[87], 0, 0, 255);
    tira.setPixelColor(Pantalla[88], 0, 0, 255);
    tira.setPixelColor(Pantalla[89], 0, 0, 255);
    tira.setPixelColor(Pantalla[110], 255, 255, 0);
    tira.setPixelColor(Pantalla[109], 255, 255, 0);
    tira.setPixelColor(Pantalla[108], 255, 255, 0);
    tira.setPixelColor(Pantalla[106], 0, 0, 255);
    tira.setPixelColor(Pantalla[105], 0, 0, 255);
    tira.setPixelColor(Pantalla[104], 0, 0, 255);
    tira.setPixelColor(Pantalla[103], 0, 0, 255);
    tira.setPixelColor(Pantalla[102], 0, 0, 255);
    tira.setPixelColor(Pantalla[114], 255, 255, 0);
    tira.setPixelColor(Pantalla[118], 105, 210, 30);
    tira.setPixelColor(Pantalla[120], 105, 210, 30);

    tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[147], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[148], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[150], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[151], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[152], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[154], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[155], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[156], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[171], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[169], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[167], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[165], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[163], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[184], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[186], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[188], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[203], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[201], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[199], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[197], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[195], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[210], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[211], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[212], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[214], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[215], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[216], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[218], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[219], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[220], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));

    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
  }
}

void Matriz_Corazon_200()
{
  if (lado == IZQUIERDA)
  {
    tira.setPixelColor(Pantalla[14], 100, 100, 100);
    tira.setPixelColor(Pantalla[13], 100, 100, 100);
    tira.setPixelColor(Pantalla[12], 100, 100, 100);
    tira.setPixelColor(Pantalla[4], 100, 100, 100);
    tira.setPixelColor(Pantalla[3], 100, 100, 100);
    tira.setPixelColor(Pantalla[2], 100, 100, 100);
    tira.setPixelColor(Pantalla[18], 100, 100, 100);
    tira.setPixelColor(Pantalla[19], 100, 100, 100);
    tira.setPixelColor(Pantalla[20], 100, 100, 100);
    tira.setPixelColor(Pantalla[22], 255, 255, 0);
    tira.setPixelColor(Pantalla[23], 100, 100, 100);
    tira.setPixelColor(Pantalla[24], 255, 255, 0);
    tira.setPixelColor(Pantalla[26], 100, 100, 100);
    tira.setPixelColor(Pantalla[27], 100, 100, 100);
    tira.setPixelColor(Pantalla[28], 100, 100, 100);
    tira.setPixelColor(Pantalla[44], 100, 100, 100);
    tira.setPixelColor(Pantalla[43], 100, 100, 100);
    tira.setPixelColor(Pantalla[42], 100, 100, 100);
    tira.setPixelColor(Pantalla[41], 0, 0, 255);
    tira.setPixelColor(Pantalla[40], 255, 255, 0);
    tira.setPixelColor(Pantalla[39], 0, 0, 255);
    tira.setPixelColor(Pantalla[38], 100, 100, 100);
    tira.setPixelColor(Pantalla[37], 100, 100, 100);
    tira.setPixelColor(Pantalla[36], 100, 100, 100);
    tira.setPixelColor(Pantalla[52], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], 100, 100, 100);
    tira.setPixelColor(Pantalla[54], 0, 0, 255);
    tira.setPixelColor(Pantalla[55], 0, 0, 255);
    tira.setPixelColor(Pantalla[56], 0, 0, 255);
    tira.setPixelColor(Pantalla[57], 100, 100, 100);
    tira.setPixelColor(Pantalla[58], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], 0, 0, 255);
    tira.setPixelColor(Pantalla[72], 0, 0, 255);
    tira.setPixelColor(Pantalla[71], 0, 0, 255);
    tira.setPixelColor(Pantalla[85], 0, 0, 255);
    tira.setPixelColor(Pantalla[86], 0, 0, 255);
    tira.setPixelColor(Pantalla[87], 0, 0, 255);
    tira.setPixelColor(Pantalla[88], 0, 0, 255);
    tira.setPixelColor(Pantalla[89], 0, 0, 255);
    tira.setPixelColor(Pantalla[106], 0, 0, 255);
    tira.setPixelColor(Pantalla[105], 0, 0, 255);
    tira.setPixelColor(Pantalla[104], 0, 0, 255);
    tira.setPixelColor(Pantalla[103], 0, 0, 255);
    tira.setPixelColor(Pantalla[102], 0, 0, 255);
    tira.setPixelColor(Pantalla[118], 105, 210, 30);
    tira.setPixelColor(Pantalla[120], 105, 210, 30);

    tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[147], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[148], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[150], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[151], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[152], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[154], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[155], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[156], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[171], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[169], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[167], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[165], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[163], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[184], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[186], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[188], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[205], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[201], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[199], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[197], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[195], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[210], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[211], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[212], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[214], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[215], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[216], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[218], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[219], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[220], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));

    tira.setPixelColor(Pantalla[245], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[250], pixel_rojo, pixel_verde, pixel_azul);
  }
  else
  {
    tira.setPixelColor(Pantalla[22], 255, 255, 0);
    tira.setPixelColor(Pantalla[23], 100, 100, 100);
    tira.setPixelColor(Pantalla[24], 255, 255, 0);
    tira.setPixelColor(Pantalla[45], 100, 100, 100);
    tira.setPixelColor(Pantalla[44], 100, 100, 100);
    tira.setPixelColor(Pantalla[43], 100, 100, 100);
    tira.setPixelColor(Pantalla[42], 100, 100, 100);
    tira.setPixelColor(Pantalla[41], 0, 0, 255);
    tira.setPixelColor(Pantalla[40], 255, 255, 0);
    tira.setPixelColor(Pantalla[39], 0, 0, 255);
    tira.setPixelColor(Pantalla[38], 100, 100, 100);
    tira.setPixelColor(Pantalla[37], 100, 100, 100);
    tira.setPixelColor(Pantalla[36], 100, 100, 100);
    tira.setPixelColor(Pantalla[35], 100, 100, 100);
    tira.setPixelColor(Pantalla[49], 100, 100, 100);
    tira.setPixelColor(Pantalla[50], 100, 100, 100);
    tira.setPixelColor(Pantalla[51], 100, 100, 100);
    tira.setPixelColor(Pantalla[52], 100, 100, 100);
    tira.setPixelColor(Pantalla[53], 100, 100, 100);
    tira.setPixelColor(Pantalla[54], 0, 0, 255);
    tira.setPixelColor(Pantalla[55], 0, 0, 255);
    tira.setPixelColor(Pantalla[56], 0, 0, 255);
    tira.setPixelColor(Pantalla[57], 100, 100, 100);
    tira.setPixelColor(Pantalla[58], 100, 100, 100);
    tira.setPixelColor(Pantalla[59], 100, 100, 100);
    tira.setPixelColor(Pantalla[60], 100, 100, 100);
    tira.setPixelColor(Pantalla[61], 100, 100, 100);
    tira.setPixelColor(Pantalla[79], 100, 100, 100);
    tira.setPixelColor(Pantalla[73], 0, 0, 255);
    tira.setPixelColor(Pantalla[72], 0, 0, 255);
    tira.setPixelColor(Pantalla[71], 0, 0, 255);
    tira.setPixelColor(Pantalla[65], 100, 100, 100);
    tira.setPixelColor(Pantalla[85], 0, 0, 255);
    tira.setPixelColor(Pantalla[86], 0, 0, 255);
    tira.setPixelColor(Pantalla[87], 0, 0, 255);
    tira.setPixelColor(Pantalla[88], 0, 0, 255);
    tira.setPixelColor(Pantalla[89], 0, 0, 255);
    tira.setPixelColor(Pantalla[106], 0, 0, 255);
    tira.setPixelColor(Pantalla[105], 0, 0, 255);
    tira.setPixelColor(Pantalla[104], 0, 0, 255);
    tira.setPixelColor(Pantalla[103], 0, 0, 255);
    tira.setPixelColor(Pantalla[102], 0, 0, 255);
    tira.setPixelColor(Pantalla[118], 105, 210, 30);
    tira.setPixelColor(Pantalla[120], 105, 210, 30);

    tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[147], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[148], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[150], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[151], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[152], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[154], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[155], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[156], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[171], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[169], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[167], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[165], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[163], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[182], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[184], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[186], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[188], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[205], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[201], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[199], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[197], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[195], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[210], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[211], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[212], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[214], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[215], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[216], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[218], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[219], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
    tira.setPixelColor(Pantalla[220], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));

    tira.setPixelColor(Pantalla[246], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[247], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[248], pixel_rojo, pixel_verde, pixel_azul);
    tira.setPixelColor(Pantalla[249], pixel_rojo, pixel_verde, pixel_azul);
  }
}

void Matriz_500()
{
}
void Matriz_400()
{
}
void Matriz_300()
{
}
void Matriz_200()
{
}

void Matriz_500_Negativo()
{
}

void Matriz_400_Negativo()
{
}
void Matriz_300_Negativo()
{
}

void Matriz_Ok()
{
  tira.setPixelColor(Pantalla[50], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[51], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[52], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[53], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[56], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[57], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[58], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[59], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[77], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[71], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[68], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[82], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[88], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[91], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[109], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[103], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[100], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[114], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[115], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[116], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[117], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[120], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[123], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[141], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[138], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[135], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[132], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[146], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[149], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[152], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[155], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[173], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[170], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[167], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[164], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[178], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[179], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[180], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[181], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[184], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[185], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[186], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
  tira.setPixelColor(Pantalla[187], (10 + rand() % 245), (10 + rand() % 245), (10 + rand() % 245));
}


