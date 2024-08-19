#include <Arduino.h>
#include "nvs.h"
#include "nvs_flash.h"
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>




// botones de navegacion
#define BTN_AVANZA  0
#define BTN_ENTER  1
#define BTN_REGRESA    2
#define BTN_CREDITOS  3

// Este arreglo contiene los pines utilizados para los BOTONES
uint8_t button[4] = {7,10,12,2};


// Este arreglo contiene elULTIMO conocido de cada línea
uint8_t button_estate[4];

// Facilita la detección de flancos de subidan en los pines
// monitoreados. Asume la existencia de un arreglo button
// con la asignación actual de pines y un arreglo button_estate
// con los valores de línea

uint8_t flancoSubida(int btn) 
{
  uint8_t valor_nuevo = digitalRead(button[btn]);
  uint8_t result = button_estate[btn]!=valor_nuevo && valor_nuevo == 1;
  button_estate[btn] = valor_nuevo;
  return result;
}


// inicalizacion de memoria flash NVS_TEST.Creditos(valor de creditos)
static char *TAG ="NVS_TEST";
// variables para creditos
// declaracion de funcines  
char *key ="Creditos";
uint8_t creditos = 0;

char *key1 ="Clave1";
uint8_t clave1=0;

char *key2 = "Clave2";
uint8_t clave2=30;

char *key3 ="Clave3";
uint8_t clave3=0;


  
nvs_handle_t app_nvs_handle;

static esp_err_t init_nvs()
{
  esp_err_t error;
  nvs_flash_init();

  error=nvs_open(TAG,NVS_READWRITE,&app_nvs_handle );
  if (error != ESP_OK)
    {
      ESP_LOGE(TAG, "Error opening NVS"); 
    }
    else
    {
      ESP_LOGI(TAG, "init_NVS complete");
    }
    return error;
}

// READ DE MEMORIA NVS
static esp_err_t read_nvs(char *key,uint8_t *value)
{
  esp_err_t error;
  error = nvs_get_u8(app_nvs_handle,key,value);
  if (error != ESP_OK)
      {
        ESP_LOGE(TAG, "Error reading NVS"); 
      }
      else
      {
        ESP_LOGI(TAG, "valor leido : %u ",*value);
      }
      return error;
}

// write a memoria nvs
static esp_err_t write_nvs(char * key, uint8_t value)
{
  esp_err_t error;
  error = nvs_set_u8(app_nvs_handle,key, value);
  if (error != ESP_OK)
      {
        ESP_LOGE(TAG, "Error writing NVS"); 
      }
      else
      {
        ESP_LOGI(TAG, "valor escrito : %u ",value);
      }
      return error;
}


//Definicion de estados
#define INTRODUCCION 3
#define CONFIGURACION 4
#define JUEGO1_M 1
#define JUEGO2_M 2
#define CREDITOS_INSUFICIENTES 5
uint8_t a_menu= INTRODUCCION;


#define TEXTO_NOMBRE_JUEGO 1
#define TEXTO_ROUND 2
#define TEXTO_TIROS 3
#define TEXTO_VELOCIDAD 4
#define TEXTO_DESTREZA 5
#define TEXTO_4JUGADORES 6
#define TEXTO_CREDITOS 7
#define CAPTURA_SALDO 8
uint8_t b_titulos= TEXTO_NOMBRE_JUEGO;

#define REINICIA_PAUSA 1
#define CALCULA_DURACION 2
#define ESCRIBE_PASSWORD 3
#define ESCRIBE_CENTENAS 4
#define ESCRIBE_DECENAS 5
#define ESCRIBE_UNIDADES 6
#define RESULTADO_CREDITOS 7
#define ESCRIBE_RESULTADOS 8
uint8_t c_pausa = REINICIA_PAUSA;


int8_t password = 1;
int8_t centenas=0;
int8_t decenas=0;
int8_t unidades=0;
int8_t c_pausa1 = REINICIA_PAUSA;

#define ESCRIBE_TXT_NUMERO 1
#define SELECCIONA_NUMERO  2
int8_t seleccion=ESCRIBE_TXT_NUMERO;

#define SELECCIONA_JUEGOS 1
#define SELECCIONA_NO_JUGADORES 2
#define SELECCIONA_NOMBRES 3
uint8_t d_configuracion=SELECCIONA_JUEGOS;

#define NO 0
#define SI 1

#define JUEGO1 1
#define JUEGO2 2

#define CERO 0
#define UNO 1
#define DOS 2
#define TRES 3
#define CUATRO 4

#define ESCRIBE_MENSAJE_SELECCION 1
#define ESCRIBE_NOMBRE_JUEGO 2
uint8_t e_juego_seleccion = ESCRIBE_MENSAJE_SELECCION;


#define REINICIA_ESCRITURA 0
#define ESCRIBE_MENSAJE_NO_JUGADORES 1
#define ESCRIBE_NO_ACTUAL 2
uint8_t f_no_jugadores = REINICIA_ESCRITURA;
String jugadores_al_juego = "DOS";

//variables para seleccion de nombres de jugadores
#define REINICIA_VARIABLES_NOMBRES 0
#define ESCRIBE_TITULO_NOMBRES 1
#define ESCRIBE_GRUPO_NOMBRES 2
int8_t h_nombre_jugadores = REINICIA_VARIABLES_NOMBRES;

/*Tablero*/
// Posicion de inicio en tablero
#define A1 0
#define A2 16
#define B1 8
#define B2 16
#define AC 12
#define BC 12

// aspecto de el texto en tablero
#define FIJO 1
#define PARPADEO 2

// Colores del texto en tablero
#define RED   1
#define GREEN 2
#define BLUE  3

          
// variables de control para pausa
ulong previous_time, current_time;


// variables para configuracion
int8_t lapso2segundos=2000;
int8_t texto_escrito1_juego =NO;
int8_t texto_escrito2_juego =NO;
int8_t juego_actual = JUEGO1;

// variables para no. de jugadores
int8_t texto_escrito1_numero= NO;
int8_t texto_escrito2_numero=NO;
int8_t numero_actual_jugadores = DOS;

// variables para nombres de jugadores
int8_t nombres_asignados = 1;
String j0 ="numeros";
String j1 = "UNO";
String j2 = "DOS";
String j3 = "TRES";
String j4 = "CUATRO";
String todos_los_nombres = "----------";
String nombres_jugadores[5]={"numeros","UNO","DOS","TRES","CUATRO"};

int8_t titulo_escrito=NO;
int8_t grupo_escrito=NO;

/*variables para juego 1*/
#define J1_REINICIA_JUEGO        0
#define J1_BIENVENIDA_JUEGO      1
#define J1_DESCUENTA_CREDITOS    11
#define J1_BIENVENIDA_ROUND_1      2 
#define J1_EJECUTA_ROUND_1         3
#define J1_PAUSA_ROUND_1           4
#define J1_BIENVENIDA_ROUND_2      5 
#define J1_EJECUTA_ROUND_2         6
#define J1_PAUSA_ROUND_2           7
#define J1_BIENVENIDA_ROUND_3      8 
#define J1_EJECUTA_ROUND_3         9
#define J1_RESULTADO_FINAL 10
int8_t j1_menu_navegacion = J1_REINICIA_JUEGO;


/*variables para juego 2*/
#define DESCUENTA_CREDITOS_JUEGO2    11

/*variables para juego 2*/
#define J2_REINICIA_JUEGO          0
#define J2_BIENVENIDA_JUEGO        1
#define J2_DESCUENTA_CREDITOS      2
#define J2_BIENVENIDA_ROUND_1      3 
#define J2_EJECUTA_ROUND_1         4
#define J2_PAUSA_ROUND_1           5
#define J2_BIENVENIDA_ROUND_2      6 
#define J2_EJECUTA_ROUND_2         7
#define J2_PAUSA_ROUND_2           8
#define J2_BIENVENIDA_ROUND_3      9 
#define J2_EJECUTA_ROUND_3        10
#define J2_PAUSA_ROUND_3          11
#define J2_ASIGNA_FINALISTAS      12
#define J2_BIENVENIDA_ROUND_4     13
#define J2_EJECUTA_ROUND_4        14
#define J2_RESULTADO_FINAL        15
int8_t j2_menu_navegacion= J2_BIENVENIDA_JUEGO;


// variables para cada jugadores


#define TURNO_UNO 1
#define TURNO_DOS 2
#define TURNO_TRES 3
#define TURNO_CUATRO 4

#define JUGADOR1 1
#define JUGADOR2 2
#define JUGADOR3 3
#define JUGADOR4 4
#define JUGADOR5 5
#define JUGADOR6 6
#define ROUND1 1
#define ROUND2 2
#define ROUND3 3
#define ROUND4 4
int8_t turno_de_jugadores = TURNO_UNO;

// varibles para turno jugador en round1
#define REINICIA_PASOS_TURNO_JUGADOR 0
#define ESCRIBE_BIENVENIDA_JUGADOR_X 1
#define ESCRIBE_CONTADOR_DESCENDENTE_X 2
#define CALCULA_CONTADOR_DESCENDENTE_J1 3
#define INICIA_PROCESO_DISPAROS_X 4
#define MARCADOR_ROUND_JUGADOR_JX 5
#define ANALIZA_CANTIDAD_JUGADORES_FINAL_CADA_ROUND 6
int8_t pasos_turno_jugador_X =REINICIA_PASOS_TURNO_JUGADOR;
int8_t texto_escrito=NO;



#define REINICIA_CONTADOR 0
#define ESCRIBE_CONTADOR 1
#define ACTUALIZA_CONTADOR 2
int8_t contador_segundos=0;
int8_t c_contador = 0;

// variables para envio de tiro a DIana
#define LIBRE 0
#define OCUPADO 1

// control de disparos
#define REINICIA_CONTROL_DISPAROS   0
#define ESCRIBE_MEJOR_TIEMPO        1
#define ESCRIBE_MARCADOR_JUGADORES   2
#define SOLICITA_DISPARO            3
int8_t menu_control_de_disparo = REINICIA_CONTROL_DISPAROS;
int8_t contador_disparos = 0;
int8_t texto_escrito1 = NO;
int8_t texto_escrito2 = NO;
int8_t mejor_tiempo = 999.99;
int8_t tiros_programados_round_juego=30;
int8_t tiros_programados_round =30;
int8_t tiempo_jugador_1 = 0;
int8_t buzon_envio =LIBRE;
int8_t enviar_disparo_a_diana=NO;
int8_t todos_los_tiros_contabilizados=NO;

//control_marcador_round1_j1

#define REINICIA_CONTROL_MARCADOR 0
#define ESCRIBE_POSICION 1
#define ESPERA_3_SEGUNDOS 2
#define ANALIZA_CANTIDAD_JUGADORES 3
int8_t control_marcador = REINICIA_CONTROL_MARCADOR;
int8_t posicion =1;
int8_t tabla_marcador[5]={0,4,3,2,1};
int8_t jugador_tiempo[5] = {0,999,999,999,999}; 
int8_t posiciones_jugadores[5]={0,0,0,0,0}; 
int8_t tiros_de_jugador[5]={0,0,0,0,0};
// fyi String nombres_jugadores[5]={"numeros","UNO","DOS","TRES","CUATRO"};


// varibles de void Resultado_final_juego_x
int8_t inicio_secuencia_mostrar = UNO;
int8_t temporal_posicion=0;

// estructura para guardar estado de disparo antes de enviarlo
typedef struct estructura_para_etiqueta_remitente
{
  uint8_t ts_juego;
  uint8_t ts_round;
  uint8_t ts_primer_jugador;
  uint8_t ts_segundo_jugador;
  uint8_t ts_no_disparo;
  uint8_t ts_propietario_tiro; // se asigna al escoger el color
    /* data */
} estructura_para_etiqueta_remitente;

estructura_para_etiqueta_remitente etiqueta_remitente;


#define SELECCION_ASCENDENTE  1
#define SELECCION_DESCENDENTE 2
#define SELECCION_MEDIA 3

#define DISPONIBLE 1
#define FUERA_DE_LINEA 0

int8_t status_diana[4]={LIBRE,LIBRE,LIBRE,LIBRE};
int diana_destino=0;
int diana_color=0;

// varables para envio de datos a diana
#define INICIALIZA_TRANSMISION 1
#define SELECCIONA_DIANA_LIBRE 2
#define MENU_SELECCIONA_COLOR 10
#define PREPARA_PAQUETE_ENVIO  3
#define ENVIA_PAQUETE          4
#define CONFIRMA_RECEPCION_DEL_PAQUETE 5
#define ESPERA_20MSEG_3INTENTOS 6
#define ENVIO_EXITOSO 7
#define LIBERA_BUZON_Y_LIMPIA_DATOS_REMITENTE 8
int8_t menu_transmision = INICIALIZA_TRANSMISION;
int intentos_envio=0;

// direcciones mac de receptores

uint8_t   broadcastAddress1[]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
uint8_t   broadcastAddress2[]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
uint8_t   broadcastAddress3[]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};


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
structura_mensaje diana1;
structura_mensaje diana2; 
structura_mensaje diana3;


#define TEST 0
#define TIRO_ACTIVO 1

//variables para recibir el mensaje
int incomin_test; //test
int incomin_diana; //diana
int incomin_color; //color
int incomin_jugador; //jugador
float incomin_tiempo; // tiempo del ejecucin del tiro en diana


// variable tipo esp_now_info_t para almacenar informacion del compañero
esp_now_peer_info_t peerInfo;

/* ---------------------------------------------------------------------*/
// obtiene la direccion mac de  ESTE EQUIPO ESP-32
void readMacAddress()
{
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) {
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                  baseMac[0], baseMac[1], baseMac[2],
                  baseMac[3], baseMac[4], baseMac[5]);
  } else {
    Serial.println("Failed to read MAC address");
  }
}
  
#define VACIO 0
#define CON_MENSAJE 1

/* ---------------------------------------------------------------------*/

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
      // actualiza datos juegador propietario del disparo
         jugador_tiempo[datos_recibidos.p]= jugador_tiempo[datos_recibidos.p]+datos_recibidos.tiempo;
         tiros_de_jugador[datos_recibidos.p]=tiros_de_jugador[datos_recibidos.p]++;
      // libera diana para recibir una nueva peticion de tiro
          status_diana[datos_recibidos.d] =LIBRE;
    }
}

/* ---------------------------------------------------------------------*/


// chequeo de dianas en linea
#define OFF_LINE 0
#define ON_LINE 1
uint8_t conexion_diana[4]={NO,NO,NO,NO};

/* ---------------------------------------------------------------------*/
// firma peer de
void Valida_Dianas_En_Linea()
{
  // chequeo diana 1
  if (conexion_diana[1]==OFF_LINE)
    {
      // register peer1
      peerInfo.channel = 0;  
      peerInfo.encrypt = false;
      memcpy(peerInfo.peer_addr, broadcastAddress1, 6);
      if (esp_now_add_peer(&peerInfo) != ESP_OK)
        {
          Serial.println("Failed to add peer");
          conexion_diana[1]=OFF_LINE;
          status_diana[1]=OCUPADO;
          return;
        }
      else
        { 
          conexion_diana[1]=ON_LINE;
          status_diana[1]=LIBRE;
        }
    }  // fin 1

  if (conexion_diana[2]==OFF_LINE)
    {
      // register peer1
      peerInfo.channel = 0;  
      peerInfo.encrypt = false;
      memcpy(peerInfo.peer_addr, broadcastAddress2, 6);
      if (esp_now_add_peer(&peerInfo) != ESP_OK)
        {
          Serial.println("Failed to add peer");
          conexion_diana[2]=OFF_LINE;
          status_diana[2]=OCUPADO;
          return;
        }
      else
        { 
          conexion_diana[2]=ON_LINE;
          status_diana[2]=LIBRE;
        }
    } // fin 2

  if (conexion_diana[3]==OFF_LINE)
    {
      // register peer1
      peerInfo.channel = 0;  
      peerInfo.encrypt = false;
      memcpy(peerInfo.peer_addr, broadcastAddress3, 6);
      if (esp_now_add_peer(&peerInfo) != ESP_OK)
        {
          Serial.println("Failed to add peer");
          conexion_diana[3]=OFF_LINE;
          status_diana[3]=OCUPADO;
          return;
        }
      else
        { 
          conexion_diana[3]=ON_LINE;
          status_diana[3]=LIBRE;
        }
    } // fin 3
}

/* ---------------------------------------------------------------------*/

void Test_Conexion_Diana()
{ 
  // DATOS A ENVIAR  
    datos_enviados.t=TEST;
    datos_enviados.c=1;   //SE MANDA ROJO PARA TEST
    datos_enviados.p=0; // JUGADOR 0 NO ACTIVO
    datos_enviados.tiempo=0; // para regresar el tiempo
  // diana 1 test
  if (conexion_diana[1]==ON_LINE)
  {
    datos_enviados.d=1;
    esp_err_t result1 = esp_now_send(broadcastAddress1, (uint8_t *) &datos_enviados, sizeof(structura_mensaje));
    if (result1 == ESP_OK) 
     {
      status_diana[1]=LIBRE;
      Serial.println("Sent with success");
     }
    else 
     {
      status_diana[1]=OCUPADO;
      Serial.println("Error sending the data");
     }
  }

  if (conexion_diana[2]==ON_LINE)
  {
    datos_enviados.d=2;
    esp_err_t result2 = esp_now_send(broadcastAddress2, (uint8_t *) &datos_enviados, sizeof(structura_mensaje));
    if (result2 == ESP_OK) 
     {
      status_diana[2]=LIBRE;
      Serial.println("Sent with success");
     }
    else 
     {
      status_diana[2]=OCUPADO;
      Serial.println("Error sending the data");
     }
  } 

  if (conexion_diana[3]==ON_LINE)
  {
    datos_enviados.d=3;
    esp_err_t result3 = esp_now_send(broadcastAddress3, (uint8_t *) &datos_enviados, sizeof(structura_mensaje));
    if (result3 == ESP_OK) 
     {
      status_diana[3]=LIBRE;
      Serial.println("Sent with success");
     }
    else 
     {
      status_diana[3]=OCUPADO;
      Serial.println("Error sending the data");
     }
  }   

}

 

/* ---------------------------------------------------------------------*/


void setup() 
{

// inicia comunicacion serial
Serial.begin(115200);

// Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

// escribe direccion mac
Serial.print("[DEFAULT] ESP32 Board MAC Address: ");
readMacAddress();

// Init ESP-NOW
if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

// sincronizacin de dianas (peer)
Valida_Dianas_En_Linea();

// inicia memoria NVS
ESP_ERROR_CHECK(init_nvs());

//char *key = "Creditos";
read_nvs(key,&creditos);
read_nvs(key1,&clave1);
read_nvs(key2,&clave2);
if (clave1==0)
  {
    Genera_Claves_y_guarda();
    read_nvs(key1,&clave1);
    read_nvs(key2,&clave2);
  }



// Configurar como PULL-UP para ahorrar resistencias
  pinMode(button[BTN_AVANZA], INPUT_PULLUP);
  pinMode(button[BTN_ENTER], INPUT_PULLUP);
  pinMode(button[BTN_REGRESA], INPUT_PULLUP);
  pinMode(button[BTN_CREDITOS], INPUT_PULLUP);
  

// Se asume que el estado inicial es HIGH
  button_estate[0] = HIGH;
  button_estate[1] = HIGH;
  button_estate[2] = HIGH;
  button_estate[3] = HIGH;

// Inicio de estados
a_menu= INTRODUCCION;
b_titulos=TEXTO_NOMBRE_JUEGO;
c_pausa=REINICIA_PAUSA;
d_configuracion=SELECCIONA_JUEGOS;
e_juego_seleccion=ESCRIBE_MENSAJE_SELECCION;
f_no_jugadores = REINICIA_ESCRITURA;
h_nombre_jugadores=REINICIA_VARIABLES_NOMBRES;

//variables menu juego 2
#define REINICIA_JUEGO_2 1
int8_t menu_navegacion_juego2 =REINICIA_JUEGO_2;

//dianas operandor

}// fin setup

/* ---------------------------------------------------------------------*/

void loop() 
{
  if (enviar_disparo_a_diana==SI)
  {
   Enviar_Disparo();
  } // fin de if enviar_disparo_a_diana

  switch (a_menu)
  {
    case INTRODUCCION:
        Introduccion_x();
        break;

    case CONFIGURACION:
        switch (d_configuracion)
          {
            case SELECCIONA_JUEGOS:
              /*code*/
              Selecciona_Juegos_z();
              break;

            case SELECCIONA_NO_JUGADORES:
              /* code*/
              Selecciona_No_Jugadores_x();
              break;

            case SELECCIONA_NOMBRES:
              /*code*/
              Selecciona_Nombres_x();
              break;
          } // fin switch
        break;

    case JUEGO1_M:
        switch (j1_menu_navegacion)
        {
          case J1_REINICIA_JUEGO:
             //v1 J1_REINICIA_JUEGO_x();
              Reinicia_Juego_z(JUEGO1,J1_BIENVENIDA_JUEGO);
              break; 
          case J1_BIENVENIDA_JUEGO:
             //V1 Bienvenida_Juego_x(JUEGO1,J1_DESCUENTA_CREDITOS);
              Bienvenida_Juego_z(JUEGO1,J1_DESCUENTA_CREDITOS);
              break;
          case J1_DESCUENTA_CREDITOS:
              Descuenta_Creditos_Juego_z (JUEGO1,J1_BIENVENIDA_ROUND_1);
              break;
          case J1_BIENVENIDA_ROUND_1:
              Bienvenida_Round_x(JUEGO1,ROUND1,J1_EJECUTA_ROUND_1);
              break;
          case J1_EJECUTA_ROUND_1:
              Ejecuta_Round_Juego1( JUEGO1,ROUND1);
              break;
          case J1_PAUSA_ROUND_1:
              PausaRound_Con_Contador_Descendente(JUEGO1, ROUND1,J1_BIENVENIDA_ROUND_2 );
              break;
          case J1_BIENVENIDA_ROUND_2:
              Bienvenida_Round_x(JUEGO1, ROUND2,J1_EJECUTA_ROUND_2);
              break;
          case J1_EJECUTA_ROUND_2:
            {
              Ejecuta_Round_Juego1( JUEGO1,ROUND2);
              break;
            }
          case J1_PAUSA_ROUND_2:
            {
              PausaRound_Con_Contador_Descendente(JUEGO1,ROUND2,J1_BIENVENIDA_ROUND_3 );
              break;
            } 
          case J1_BIENVENIDA_ROUND_3:
            Bienvenida_Round_x(JUEGO1,ROUND3,J1_EJECUTA_ROUND_3);
            break;
          case J1_EJECUTA_ROUND_3:
            {
              Ejecuta_Round_Juego1( JUEGO1, ROUND3);
              break;
            }
          case J1_RESULTADO_FINAL:
              Resultado_Final_Juego_x(JUEGO1);
              break;
        } // fin switch(menu_navegacion_juego_1)
        break;

    case JUEGO2_M:
        switch (j2_menu_navegacion)
        {
          case J2_REINICIA_JUEGO:
              Reinicia_Juego_z(JUEGO2,J2_BIENVENIDA_JUEGO);
              break; 
          case J2_BIENVENIDA_JUEGO:
              Bienvenida_Juego_z(JUEGO2, J2_DESCUENTA_CREDITOS);
              break;
          case J2_DESCUENTA_CREDITOS:
              Descuenta_Creditos_Juego_z(JUEGO2,J2_BIENVENIDA_ROUND_1);
              break;
          case J2_BIENVENIDA_ROUND_1: //1 VS 2
              Bienvenida_Round_x(JUEGO2,ROUND1,J2_EJECUTA_ROUND_1);
              break;
          case J2_EJECUTA_ROUND_1:
              Ejecuta_Round_Juego2( JUEGO2,ROUND1);
              break;
          case J2_PAUSA_ROUND_1:
              PausaRound_Con_Contador_Descendente(JUEGO2,ROUND1,J2_BIENVENIDA_ROUND_2);
              break;
          case J2_BIENVENIDA_ROUND_2: // 3 VS 4
              Bienvenida_Round_x(JUEGO2,ROUND2,J2_EJECUTA_ROUND_2);
              break;
          case J2_EJECUTA_ROUND_2:
              Ejecuta_Round_Juego2( JUEGO2, ROUND2);
              break;
          case J2_PAUSA_ROUND_2:
              PausaRound_Con_Contador_Descendente(JUEGO2,ROUND2,J2_BIENVENIDA_ROUND_3);
              break;
          case J2_BIENVENIDA_ROUND_3:
            Bienvenida_Round_x(JUEGO2,ROUND3,J2_EJECUTA_ROUND_3);
            break;
          case J2_EJECUTA_ROUND_3:
              // el nivel de regreso se decide en Bloque_disparo- ANALIZA_CANTIDAD_JUGADORES_FINAL_CADA_ROUND
              Ejecuta_Round_Juego2( JUEGO2, ROUND3);               
              break;          
          case J2_PAUSA_ROUND_3:
              PausaRound_Con_Contador_Descendente(JUEGO2,ROUND3,J2_ASIGNA_FINALISTAS);
              break;          
          case J2_ASIGNA_FINALISTAS:
               Asigna_Finalistas(JUEGO2,ROUND4,J2_BIENVENIDA_ROUND_4);
               break;
          case J2_BIENVENIDA_ROUND_4:
               Bienvenida_Round_x(JUEGO2,ROUND4,J2_EJECUTA_ROUND_4);
               break;
          case J2_EJECUTA_ROUND_4:
              Ejecuta_Round_Juego2( JUEGO2, ROUND4);               
              break;
          case J2_RESULTADO_FINAL:
              Resultado_Final_Juego_x(JUEGO2);
              break;
        } // fin switch(j2_menu_navegacion)
        break;

    case CREDITOS_INSUFICIENTES:
      Creditos_insuficientes_x();
      break;

  } // fin switch (a_menu)
} // FIN  loop ---------------------------------------------------



/* ---------------------------------------------------------------------*/
void Enviar_Disparo()
{
   switch (menu_transmision)
    {
    case INICIALIZA_TRANSMISION:
         /* code */
         menu_transmision=MENU_SELECCIONA_COLOR;
         break;

    case MENU_SELECCIONA_COLOR:
          diana_color=Color_de_Tiro();
          menu_transmision=SELECCIONA_DIANA_LIBRE;
          break;
    case SELECCIONA_DIANA_LIBRE:
         diana_destino=Selecciona_Diana();
         menu_transmision=PREPARA_PAQUETE_ENVIO;
         break;  
    case PREPARA_PAQUETE_ENVIO:
          datos_enviados.t=TIRO_ACTIVO; // TEST SI O TEST NO->VALIDO
          datos_enviados.d=diana_destino; //diana
          datos_enviados.c=diana_color;   // color disparo
          datos_enviados.p=etiqueta_remitente.ts_propietario_tiro;
          datos_enviados.tiempo=0; // para regresar el tiempo
          menu_transmision=ENVIA_PAQUETE;
          intentos_envio=0;
          break;
    case ENVIA_PAQUETE:
          switch (diana_destino)
              {
                case CERO:
                    /* no hace nada, regresa y busca dianas disponibles*/
                    menu_transmision=SELECCIONA_DIANA_LIBRE;
                    break;

                case UNO:
                    esp_err_t result1 = esp_now_send(broadcastAddress1,(uint8_t *) &datos_enviados,sizeof(structura_mensaje));         
                    if (result1 == ESP_OK) 
                      {
                        Serial.println("Sent with success");
                        status_diana[1] =OCUPADO;
                        menu_transmision=ENVIO_EXITOSO;
                      }
                    else 
                      {
                        Serial.println("Error sending the data");
                        intentos_envio++;
                        menu_transmision=ESPERA_20MSEG_3INTENTOS;
                        previous_time=millis();
                      }
                    break;

                 case DOS:
                    esp_err_t result2 = esp_now_send(broadcastAddress2,(uint8_t *) &datos_enviados,sizeof(structura_mensaje));         
                    if (result2 == ESP_OK) 
                      {
                        Serial.println("Sent with success");
                        status_diana[2] =OCUPADO;
                        menu_transmision=ENVIO_EXITOSO;
                      }
                    else 
                      {
                        Serial.println("Error sending the data");
                        intentos_envio++;
                        menu_transmision=ESPERA_20MSEG_3INTENTOS;
                        previous_time=millis();
                      }
                    break;                  

                 case TRES:
                    esp_err_t result3 = esp_now_send(broadcastAddress3,(uint8_t *) &datos_enviados,sizeof(structura_mensaje));         
                    if (result3 == ESP_OK) 
                      {
                        Serial.println("Sent with success");
                        status_diana[3] =OCUPADO;
                        menu_transmision=ENVIO_EXITOSO;
                      }
                    else 
                      {
                        Serial.println("Error sending the data");
                        intentos_envio++;
                        menu_transmision=ESPERA_20MSEG_3INTENTOS;
                        previous_time=millis();
                      }
                    break;

              } //fin  switch diana_destino
          break;

    case ESPERA_20MSEG_3INTENTOS:
        current_time=millis();
        if (current_time>(previous_time+200))
          {
            if (intentos_envio<4)
              {
                menu_transmision=ENVIA_PAQUETE;
              }
            else
              { 
                status_diana[diana_destino]=OCUPADO;
                menu_transmision=SELECCIONA_DIANA_LIBRE;                  
              }
          }
        break;

    case ENVIO_EXITOSO:
      //LIBERA LA OFICINA DE ENVIO para recibir nuevos tiros
      buzon_envio=LIBRE;
      enviar_disparo_a_diana=NO;
      menu_transmision=CERO;
      break;
    case LIBERA_BUZON_Y_LIMPIA_DATOS_REMITENTE:
      break;
    }
}



/* ---------------------------------------------------------------------*/

int8_t Color_de_Tiro()
{
  int local_color= 2+rand()%2;
  switch (etiqueta_remitente.ts_juego)
    { 
      case JUEGO1:
          etiqueta_remitente.ts_propietario_tiro=etiqueta_remitente.ts_primer_jugador;
          return local_color; // no importa el color se carga a primer jugador
        break;

      case JUEGO2:
        switch (local_color)
          {
            case GREEN: // valida tenga tiros libres el verde
              /* code */
              if (tiros_de_jugador[etiqueta_remitente.ts_primer_jugador]<tiros_programados_round_juego)
              {
                etiqueta_remitente.ts_propietario_tiro=etiqueta_remitente.ts_primer_jugador;
                return GREEN;
              }
              else
              {
                etiqueta_remitente.ts_propietario_tiro=etiqueta_remitente.ts_segundo_jugador;
                return BLUE;
              }
              break;

            case BLUE: // valida tenga tiros libres el azul
              if (tiros_de_jugador[etiqueta_remitente.ts_segundo_jugador]<tiros_programados_round_juego)
              {
                etiqueta_remitente.ts_propietario_tiro=etiqueta_remitente.ts_segundo_jugador;
                return BLUE;
              }
              else
              {
                etiqueta_remitente.ts_propietario_tiro=etiqueta_remitente.ts_primer_jugador;
                return GREEN;
              }  
              break;
              
          } //fin switch
        break;

    }
}

/* ---------------------------------------------------------------------*/

int8_t Selecciona_Diana()
{ 
  int8_t resultado;
  int local_orden;
  local_orden=1+rand()%3;
  switch (local_orden)
    {
      case 1:
        // orden de busques 1-2-3
        if (status_diana[1]==OCUPADO)
          { if (status_diana[2]==OCUPADO)
              { if (status_diana[3]==OCUPADO)
                  {
                    resultado=CERO; // todas las dianas ocupadas 
                  }
                else
                  { resultado=TRES;
                  }
              }
            else
              { resultado=DOS; 
              }
          }
        else
          { resultado=UNO;
          }
        break;
      case 2:
        // orden de busques 3-2-1
        if (status_diana[3]==OCUPADO)
          { if (status_diana[2]==OCUPADO)
              { if (status_diana[1]==OCUPADO)
                  {
                   resultado=CERO; // todas las dianas ocupadas 
                  }
                else
                  { resultado=UNO; 
                  }
              }
            else
              { resultado=DOS; 
              }
          }
        else
          { resultado=TRES;
          }
        break;

      case 3:
        // orden de busqueda 2-1-3
        if (status_diana[2]==OCUPADO)
          { if (status_diana[1]==OCUPADO)
              { if (status_diana[3]==OCUPADO)
                  {
                   resultado=CERO; // todas las dianas ocupadas 
                  }
                else
                  { resultado=TRES; 
                  }
              }
            else
              { resultado=UNO; 
              }
          }
        else
          { resultado=DOS;
          }
        break; 

    }// fin swtich local_orden
    return resultado;
}

/* ---------------------------------------------------------------------*/

void  Asigna_Finalistas(int8_t local_juego,int8_t local_round,int8_t local_regreso)
{
  //busca los  ganadores y coloca en posicion 1 y 2 para ejecutar el 4o round
  
  /* busca los dos jugadores lider y asigna su nombre  para despues solo cambiarlo*/
  String nombre_lider   =  nombres_jugadores[tabla_marcador[1]];
  String nombre_retador =  nombres_jugadores[tabla_marcador[2]];

  /* cambia nombres de lider y jugadora a JUGADOR1 Y JUGADOR 2 para 4o round
     y es como iniciar un nuevo juego del 1 vs 2 y sale al final de 4round
  */
  nombres_jugadores[1]=nombre_lider;
  nombres_jugadores[2]=nombre_retador;
  
  // resetea tiempo jugador1 y jugador 2
   
  jugador_tiempo[1]=0.00;
  jugador_tiempo[2]=0.00;
  // resetea numero de jugadores
  numero_actual_jugadores = DOS;
  // regreso a round4
    j2_menu_navegacion=local_regreso;
}

/* ---------------------------------------------------------------------*/

void Escribir(uint8_t posicion,uint8_t condiciontexto,uint8_t color, String oracion)
{

}

/* ---------------------------------------------------------------------*/

void Limpia_A1()
{
// limpia la linea completa
}

/* ---------------------------------------------------------------------*/

void Limpia_A2()
{
// limpia la linea 2 completa
}

/* ---------------------------------------------------------------------*/

uint8_t Calcula_Posicion_Jugador(int jugador) 
{  
  // reacomoda las posiciones  en marcador
  for (int vuelta=1;vuelta<=3;vuelta++)
    {
    ComparaTiempo_Y_CambiaPosicion (1,2); 
    ComparaTiempo_Y_CambiaPosicion (2,3);
    ComparaTiempo_Y_CambiaPosicion (3,4);
    }
  // asigna a cada jugador su posicion en tabla 
   posiciones_jugadores[tabla_marcador[1]]=1;
   posiciones_jugadores[tabla_marcador[2]]=2;
   posiciones_jugadores[tabla_marcador[3]]=3;
   posiciones_jugadores[tabla_marcador[4]]=4;

   uint8_t result =posiciones_jugadores[jugador];
  // regresa la posición del jugador como entero
   return result;
}

/* ---------------------------------------------------------------------*/

void Round_escribe_bienvenida (int8_t local_juego, int8_t local_round, int8_t local_jugadorA, int8_t local_jugadorB)
{
  if(texto_escrito==NO )
    {
      Escribir(A1,FIJO,GREEN ,nombres_jugadores[local_jugadorA]);
      texto_escrito=SI;
      c_pausa=REINICIA_PAUSA;
      if (local_juego==JUEGO2)
      {
        Escribir(B1,FIJO,BLUE ,nombres_jugadores[local_jugadorB]);
      }
    }
  switch (c_pausa)
    {
      case REINICIA_PAUSA:
          previous_time=millis();
          c_pausa=CALCULA_DURACION;
          break;

      case CALCULA_DURACION:
          current_time=millis();
          if ((previous_time+3000< current_time))
             {
               c_pausa=REINICIA_PAUSA;
               pasos_turno_jugador_X=ESCRIBE_CONTADOR_DESCENDENTE_X;
               Limpia_A1();
               Limpia_A2();
             }
          break;

     } // fin de swtich(c_pausa)
}

/* ---------------------------------------------------------------------*/

void Round_Escribe_Contador_Descentente()
{

  switch (c_contador)
    {
      case REINICIA_CONTADOR:
        /* code */
        contador_segundos=10;
        texto_escrito=NO;
        c_contador=ESCRIBE_CONTADOR;
        previous_time=millis();
        break; 

      case ESCRIBE_CONTADOR:
        if (texto_escrito=NO)
          {
            Escribir(A1,FIJO,GREEN , String(contador_segundos) );
            texto_escrito =SI;
            c_contador = ACTUALIZA_CONTADOR;
          }
        break; 

      case ACTUALIZA_CONTADOR:
        current_time=millis();
        if (current_time>(previous_time+1000))
          {
            contador_segundos=contador_segundos-1;
            previous_time=millis();
            c_contador=ESCRIBE_CONTADOR;
            texto_escrito=NO;
            if (contador_segundos=0)
              {
                pasos_turno_jugador_X=INICIA_PROCESO_DISPAROS_X;
              }
          }
        break; 

    } //fin switch (c_contador)
}

/* ---------------------------------------------------------------------*/
void Round_Inicia_Proceso_disparos(int8_t local_juego,int8_t local_round, int8_t local_jugadorA, int8_t local_jugadorB)
{
  switch (menu_control_de_disparo)
    {
      case REINICIA_CONTROL_DISPAROS:
        contador_disparos=1;
        todos_los_tiros_contabilizados=NO;
        if  (local_juego==JUEGO1)
          {
            tiros_programados_round=tiros_programados_round_juego;  
          }
        else //JUEGO2
        {
            tiros_programados_round=2*tiros_programados_round_juego;  
        }
        texto_escrito1=NO;
        texto_escrito2=NO;
        menu_control_de_disparo=ESCRIBE_MEJOR_TIEMPO;
        break;

      case ESCRIBE_MEJOR_TIEMPO:
        if (texto_escrito1==NO)
          {
            if (local_juego==JUEGO1)
            {
              Escribir(A2,FIJO,GREEN , String(mejor_tiempo)) ;
              texto_escrito1=SI; 
            }                                
          }
        menu_control_de_disparo=ESCRIBE_MARCADOR_JUGADORES;
        break;
                              
      case ESCRIBE_MARCADOR_JUGADORES:
        if (texto_escrito2==NO)
          {
            if (local_juego==JUEGO1)
            {
              Escribir(B1,FIJO,GREEN , String(tiros_programados_round-tiros_de_jugador[local_jugadorA])) ;
              Escribir(B2,FIJO,GREEN , String(jugador_tiempo[local_jugadorA])) ;
              texto_escrito2=SI;
            }
            else
            {
              //TIRADOR VERDE-1
              Escribir(A1,FIJO,GREEN , String(tiros_programados_round-tiros_de_jugador[local_jugadorA])) ;
              Escribir(A2,FIJO,GREEN , String(jugador_tiempo[local_jugadorA])) ;
              texto_escrito2=SI; 
              // TIRADOR AZUL-2
              Escribir(B1,FIJO,GREEN , String(tiros_programados_round-tiros_de_jugador[local_jugadorB])) ;
              Escribir(B2,FIJO,GREEN , String(jugador_tiempo[local_jugadorB])) ;
              texto_escrito2=SI; 
              //fyi int8_t jugador_tiempo[5] = {0,999,999,999,999}; 
              //fyi int8_t tiros_de_jugador[5]={0,0,0,0,0}; 
            }
          }
        menu_control_de_disparo=SOLICITA_DISPARO;
        break;

      case SOLICITA_DISPARO:
        if (contador_disparos>tiros_programados_round)
          { 
            if (todos_los_tiros_contabilizados == SI) 
              //todos_los_tiros_contabilizados: es cambiado solo por el gestor de 
              //recepcion_de_dianas al recibirse y acumularse todos los tiros de un round
              { 
                menu_control_de_disparo=REINICIA_CONTROL_DISPAROS;
                pasos_turno_jugador_X=MARCADOR_ROUND_JUGADOR_JX;
              }
           }
        if ((contador_disparos<tiros_programados_round) && (buzon_envio==LIBRE))
          {
                //llena datos de remitente del disparo (se usan datos globales)
                etiqueta_remitente.ts_juego=local_juego;
                etiqueta_remitente.ts_round=local_round;
                etiqueta_remitente.ts_primer_jugador=local_jugadorA;
                etiqueta_remitente.ts_segundo_jugador=local_jugadorB;
                etiqueta_remitente.ts_no_disparo=contador_disparos;
                // bloquea la recepcin de otro tiro y avisa que se envie a una diana
                buzon_envio=OCUPADO;
                enviar_disparo_a_diana=SI; 
                // aumenta disparo x
                contador_disparos++;  
          }
        break;

      } //fin switch menu_control_de_disparo
}
/* ---------------------------------------------------------------------*/

void Marcador_Round_Jugador_Z( int8_t local_round,int8_t local_jugador)
{
  switch (control_marcador)
    {
      case REINICIA_CONTROL_MARCADOR:
        Limpia_A1();
        control_marcador=ESCRIBE_POSICION ;
        c_pausa=REINICIA_PAUSA;
        break;
                              
      case ESCRIBE_POSICION:
        posicion=Calcula_Posicion_Jugador(local_jugador);
        switch (posicion)
          {
            case 1:
              Escribir(A1,FIJO,GREEN , "PRIMERO") ;
              break;
            case 2:
              Escribir(A1,FIJO,GREEN , "SEGUNDO") ;
              break;
            case 3:
              Escribir(A1,FIJO,GREEN , "TERCERO") ;
              break;
            case 4:
              Escribir(A1,FIJO,GREEN , "CUARTO") ;
              break;
          } //fin switch (posicion)
        control_marcador=ESPERA_3_SEGUNDOS;
        break; // fin ESCRIBE_POSICION

      case ESPERA_3_SEGUNDOS:
        switch (c_pausa)
          {
            case REINICIA_PAUSA:
              previous_time=millis();
              c_pausa=CALCULA_DURACION;
              break;
            case CALCULA_DURACION:
              current_time=millis();
              if ((previous_time+3000< current_time))
               {
                  c_pausa=REINICIA_PAUSA;
                  pasos_turno_jugador_X=ANALIZA_CANTIDAD_JUGADORES_FINAL_CADA_ROUND;
               }
               break;

          } // fin de swtich(c_pausa)
        break; // din ESPERA_3_SEGUNDOS

    } //fin switch(control_marcador)
}

/* ---------------------------------------------------------------------*/

void Bloque_Disparos( int8_t local_juego, int8_t local_round, int8_t local_jugadorA,int8_t local_jugadorB, int8_t local_turno)
{
  switch (pasos_turno_jugador_X)
    {
      case REINICIA_PASOS_TURNO_JUGADOR:
        menu_control_de_disparo=REINICIA_CONTROL_DISPAROS;
        pasos_turno_jugador_X= ESCRIBE_BIENVENIDA_JUGADOR_X;
        turno_de_jugadores=TURNO_UNO;
        break;
           
      case ESCRIBE_BIENVENIDA_JUGADOR_X:
        Round_escribe_bienvenida(local_juego, local_round, local_jugadorA,local_jugadorB);                          
        break; // fin de ESCRIBE_BIENVENIDA_JUGADOR_X
                    
      case ESCRIBE_CONTADOR_DESCENDENTE_X:
        Round_Escribe_Contador_Descentente();
        break;  //fin ESCRIBE_CONTADOR_DESCENDENTE
        
      case INICIA_PROCESO_DISPAROS_X:                
        Round_Inicia_Proceso_disparos(local_juego,local_round,local_jugadorA,local_jugadorB);
        break; //fin INICIA_PROCESO_DISPAROS_X
                      
      case MARCADOR_ROUND_JUGADOR_JX:
        Marcador_Round_Jugador_Z (local_round, local_jugadorA);
        break; // fin MARCADOR_ROUND_JUGADOR_JX

      case ANALIZA_CANTIDAD_JUGADORES_FINAL_CADA_ROUND:
        /*code 
        Al final de cada round analiza si sigue otro
        jugador o se cambia de round e inicia de nuevo 
        */
        switch (local_juego)
          {
            case JUEGO1:
                if (local_turno<numero_actual_jugadores) 
                  {// cambio de TURNO
                    switch (local_turno) 
                      {
                        case TURNO_UNO:
                          turno_de_jugadores=TURNO_DOS;
                          break;
                        case TURNO_DOS:
                          turno_de_jugadores=TURNO_TRES;
                          break;
                        case TURNO_TRES:
                          turno_de_jugadores=TURNO_CUATRO;
                          break;
                            
                      }
                  }
                else    
                  { // cambio de ROUND       
                    switch (local_round)
                      {
                        case ROUND1:
                          j1_menu_navegacion=J1_PAUSA_ROUND_1;
                          break;
                        case ROUND2:
                          j1_menu_navegacion=J1_PAUSA_ROUND_2;
                          break;
                        case ROUND3:
                          j1_menu_navegacion=J1_RESULTADO_FINAL;
                          break;
                      } //fin  switch
                  }
                break;
        
            case JUEGO2:
                if (local_turno<numero_actual_jugadores) 
                  {// cambio de TURNO
                    switch (local_turno) 
                      {
                        case TURNO_UNO:
                          turno_de_jugadores=TURNO_DOS;
                          break;
                        case TURNO_DOS:
                          turno_de_jugadores=TURNO_TRES;
                          break;
                        case TURNO_TRES:
                          turno_de_jugadores=TURNO_CUATRO;
                          break;
                            
                      }
                  }
                else    
                  { // cambio de ROUND       
                    switch (local_round)
                      {
                        case ROUND1:
                          j2_menu_navegacion=J2_PAUSA_ROUND_1;
                          break;
                        case ROUND2:
                          j2_menu_navegacion=J2_PAUSA_ROUND_2;
                          break;
                        case ROUND3:
                          j2_menu_navegacion=J2_PAUSA_ROUND_3;
                          break;
                        case ROUND4:
                          j2_menu_navegacion=J2_RESULTADO_FINAL;
                          break;
                      } //fin  switch
                  }

                break;

          } // fin switch local_juego

    } // fin switch (pasos_turno_jugador_X)
}

/* ---------------------------------------------------------------------*/

void Bienvenida_Round_x ( int8_t local_juego, int8_t local_round,int8_t local_regreso)
{
  if (texto_escrito1_numero==NO)
     {
        Escribir(A1,FIJO,GREEN ,"ROUND");
        Escribir(B1,FIJO,GREEN ,String(local_round));
        texto_escrito1_numero=SI;
        c_pausa=REINICIA_PAUSA;
     }
  switch (c_pausa) // calcula pausa de 2 segundos
     {
        case REINICIA_PAUSA:
            previous_time=millis();
            c_pausa=CALCULA_DURACION;
            break;

        case CALCULA_DURACION:
            current_time=millis();
            if ((previous_time+2000)>=current_time)
               {   
                 current_time=millis();
               }
            else
              {
                if (local_juego==JUEGO1)
                  {
                    j1_menu_navegacion=local_regreso;
                  }
                else
                  {
                    j2_menu_navegacion=local_regreso;
                  }
                c_pausa=REINICIA_PAUSA;
                texto_escrito1_numero=NO;
                Limpia_A1();
                Limpia_A2();                
               }
            break;

      } // fin switch c_pausa

}
/* ---------------------------------------------------------------------*/

void Creditos_insuficientes_x()
{
  Escribir(A1,FIJO,GREEN ,"SIN CREDITO");
  Escribir(B1,FIJO,GREEN, String(creditos)); // CONVERTIR A TEXTO LOS CREDITOS
  switch (c_pausa) // calcula pausa de 3 segundos
    {
      case REINICIA_PAUSA:
          previous_time=millis();
          c_pausa=CALCULA_DURACION;
          break;
      case CALCULA_DURACION:
          current_time=millis();
          if ((previous_time+3000)>=current_time)
             {   
               current_time=millis();
             }
          else
             {    
              a_menu=INTRODUCCION; 
              c_pausa=REINICIA_PAUSA;
              Escribir(A1,FIJO,GREEN ,"        ");
              Escribir(B1,FIJO,GREEN ,"        ");
              }
          break; 
  
    } // fin switch
} 
/* ---------------------------------------------------------------------*/

void Ejecuta_Round_Juego1 (int8_t local_juego, int8_t local_roundx )
{ 
  switch (turno_de_jugadores) 
                {
                  case TURNO_UNO:
                      /*code */
                      Bloque_Disparos (local_juego,local_roundx,JUGADOR1,0,TURNO_UNO); 
                      break; 
             
                  case  TURNO_DOS:
                      /*code */
                      Bloque_Disparos(local_juego, local_roundx,JUGADOR2,0,TURNO_DOS);
                      break;

                  case TURNO_TRES:
                      /*code*/
                      Bloque_Disparos (local_juego, local_roundx,JUGADOR3,0,TURNO_TRES);
                      break;

                  case TURNO_CUATRO:
                      /* code */
                      Bloque_Disparos (local_juego, local_roundx,JUGADOR4,0,TURNO_CUATRO);
                      break;
               } // fin switch(turno_de_jugadores)
} // fin ejecuta_round_x

/* ---------------------------------------------------------------------*/

void Ejecuta_Round_Juego2 (int8_t local_juego, int8_t local_roundx )
{ 
  switch (turno_de_jugadores) 
                {
                  case TURNO_UNO:
                      /*code */
                      Bloque_Disparos (local_juego,local_roundx,JUGADOR1,JUGADOR2,TURNO_UNO); 
                      break; 
             
                  case  TURNO_DOS:
                      /*code */
                      Bloque_Disparos(local_juego, local_roundx,JUGADOR1,JUGADOR2,TURNO_DOS);
                      break;

                  case TURNO_TRES:
                      /*code*/
                      Bloque_Disparos (local_juego, local_roundx,JUGADOR3,JUGADOR4,TURNO_TRES);
                      break;

                  case TURNO_CUATRO:
                      /* code */
                      Bloque_Disparos(local_juego, local_roundx,JUGADOR3,JUGADOR4,TURNO_CUATRO);
                      break;
               } // fin switch(turno_de_jugadores)
} // fin ejecuta_round_x

/* ---------------------------------------------------------------------*/

void Bienvenida_Juego_z (int8_t local_entro_por, int8_t local_salgo_a)  
{
  if (texto_escrito1_numero==NO)
   {
     if (local_entro_por==JUEGO1)
        {
          Escribir(A1,FIJO,GREEN ,"TORNEO");
          Escribir(B1,FIJO,GREEN ,"CON DOS PISTOLAS");
          texto_escrito1_numero=SI;
          c_pausa=REINICIA_PAUSA;                   
        }
      else
        {
          Escribir(A1,FIJO,GREEN ,"DUELO");
          Escribir(B1,FIJO,GREEN ,"1 PISTOLA");
          texto_escrito1_numero=SI;
          c_pausa=REINICIA_PAUSA;                   
        }                  
    }
  switch (c_pausa) // calcula pausa de 2 segundos
    {
      case REINICIA_PAUSA:
        previous_time=millis();
        c_pausa=CALCULA_DURACION;
        break;
      case CALCULA_DURACION:
        current_time=millis();
        if ((previous_time+2000)>=current_time)
          {   
            current_time=millis();
          }
        else
          {     
            c_pausa=REINICIA_PAUSA;
            texto_escrito1_numero=NO;
            Limpia_A1();
            Limpia_A2(); 
            if (local_entro_por==JUEGO1)
              {
                j1_menu_navegacion=J1_DESCUENTA_CREDITOS; 
              }
            else
              {
                j2_menu_navegacion=J2_DESCUENTA_CREDITOS; 
              }
          }
    }
} // fin J1_BIENVENIDA_JUEGO


/* ---------------------------------------------------------------------*/


void Texto_Creditos_x()
{
  switch (c_pausa) // calcula pausa de 2 segundos
    {
      case REINICIA_PAUSA:
          Escribir(A1,FIJO,GREEN ,"Cred: "+String(creditos));
          previous_time=millis();
          c_pausa=CALCULA_DURACION;
          break;
      case CALCULA_DURACION:
          current_time=millis();
          if ((previous_time+2000)>=current_time)
            {   
              current_time=millis();
            }
          else
            {    
              b_titulos=TEXTO_ROUND; 
              c_pausa=REINICIA_PAUSA;
              Escribir(A1,FIJO,GREEN ,"        ");
            }
            break;         

    }
}

/* ---------------------------------------------------------------------*/

void Introduccion_x()
{
       if ( flancoSubida(BTN_AVANZA)) // avanza a confirugacion
          {
            a_menu=CONFIGURACION;
            texto_escrito1_juego=NO;
            texto_escrito2_juego=NO;
            c_pausa=REINICIA_PAUSA;
            Limpia_A1();
            Limpia_A2();
          }

       if ( flancoSubida(BTN_CREDITOS)) // configuracon de creditos
          {
            b_titulos=CAPTURA_SALDO;
            texto_escrito1_juego=NO;
            texto_escrito2_juego=NO;
            c_pausa=REINICIA_PAUSA;
            Limpia_A1();
            Limpia_A2();
          }

        switch (b_titulos)
        {
          case TEXTO_NOMBRE_JUEGO:
              //TEXTO_NOMBRE_JUEGO_general();
              Escribe_Titulos("DUELO DE","PISTOLEROS",TEXTO_CREDITOS);
               break;

          case TEXTO_CREDITOS:
              Texto_Creditos_x();
              break;

          case TEXTO_ROUND:
               //Texto_Round_juego_general();
              Escribe_Titulos("3","ROUNDS",TEXTO_TIROS);
               break;

          case TEXTO_TIROS:
              // Texto_Tiros_x();
              Escribe_Titulos("100","TIROS",TEXTO_VELOCIDAD);

               break;

         case TEXTO_VELOCIDAD:
              //Texto_Velocidad_x();
              Escribe_Titulos("VELOCIDAD"," ",TEXTO_DESTREZA);
              break;

        case TEXTO_DESTREZA:
              //Texto_Destreza_x();
              Escribe_Titulos("DESTREZA"," ",TEXTO_4JUGADORES);
               break;

        case TEXTO_4JUGADORES:
              // Texto_4Jugadores();
              Escribe_Titulos("1 a 4","JUGADORES ",TEXTO_NOMBRE_JUEGO);
               break;
        case CAPTURA_SALDO:
             break;

        }
}

/* ---------------------------------------------------------------------*/
void Captura_Saldo_x()
{ 
  switch (c_pausa1) // calcula pausa de 2 segundos
    {
      case REINICIA_PAUSA:
        Escribir(A1,FIJO,GREEN ,String(clave1)+"-"+String(clave2));
        texto_escrito2=NO;
        c_pausa1=ESCRIBE_CENTENAS;
        seleccion=ESCRIBE_TXT_NUMERO;
        break;
       
      case ESCRIBE_CENTENAS:
        switch (seleccion)
          {
            case ESCRIBE_TXT_NUMERO:
              if (texto_escrito2==NO)
                {
                  Escribir(B1,FIJO,GREEN,"<"+String(centenas)+"> "+"["+String(decenas )+"] "+"["+String(unidades)+"]");
                  texto_escrito2=SI;
                  seleccion=SELECCIONA_NUMERO;
                }
              break;

            case SELECCIONA_NUMERO:
              /*boton avanza*/
              if ( flancoSubida(BTN_AVANZA))
                {
                  if (centenas<9) // switchea el nombre
                    {
                      centenas++; 
                    }
                  else 
                    {
                      centenas=0;
                    }
                  texto_escrito2=NO;
                  seleccion=ESCRIBE_TXT_NUMERO;
                }
              /* btn enter  brinca a siguiente nivel*/
              if ( flancoSubida(BTN_ENTER))
                {
                  texto_escrito2=NO;
                  c_pausa1=ESCRIBE_DECENAS;
                  seleccion=ESCRIBE_TXT_NUMERO;
                }   
             // BTN creditos, es la opion de exit de la edicion sin grabar    
              if ( flancoSubida(BTN_CREDITOS))
                {
                  texto_escrito2=NO;
                  b_titulos=TEXTO_NOMBRE_JUEGO;
                } 
              break;  
                      
          } // switch seleccion
        break;
        
      case ESCRIBE_DECENAS:
           switch (seleccion)
              {
                case ESCRIBE_TXT_NUMERO:
                    if (texto_escrito2==NO)
                      {
                        Escribir(B1,FIJO,GREEN,"["+String(centenas)+"] "+"<"+String(decenas )+"> "+"["+String(unidades)+"]");
                          texto_escrito2=SI;
                        seleccion=SELECCIONA_NUMERO;
                      }
                    break;
                case SELECCIONA_NUMERO:
                    /*boton avanza*/
                    if ( flancoSubida(BTN_AVANZA))
                      {
                        if (decenas<9) // switchea el nombre
                            {
                            decenas++; 
                            }
                        else 
                            {
                            decenas=0;
                            }
                        texto_escrito2=NO;
                        seleccion=ESCRIBE_TXT_NUMERO;
                      }
                   /* btn enter  brinca a siguiente nivel*/
                    if ( flancoSubida(BTN_ENTER))
                       {
                        texto_escrito2=NO;
                        seleccion=ESCRIBE_TXT_NUMERO;
                        c_pausa1=ESCRIBE_UNIDADES;
                       }           
              } // switch seleccion
            break;      

        case ESCRIBE_UNIDADES:
           switch (seleccion)
              {
                case ESCRIBE_TXT_NUMERO:
                    if (texto_escrito2==NO)
                      {
                        Escribir(B1,FIJO,GREEN,"["+String(centenas)+"] "+"["+String(decenas )+"] "+"<"+String(unidades)+">");
                          texto_escrito2=SI;
                        seleccion=SELECCIONA_NUMERO;
                      }
                    break;
                case SELECCIONA_NUMERO:
                    /*boton avanza*/
                    if ( flancoSubida(BTN_AVANZA))
                      {
                        if (unidades<9) // switchea el nombre
                            {
                              unidades++; 
                            }
                        else 
                            {
                              unidades=0;
                            }
                        texto_escrito2=NO;
                        seleccion=ESCRIBE_TXT_NUMERO;
                      }
                   /* btn enter  brinca a siguiente nivel*/
                    if ( flancoSubida(BTN_ENTER))
                       {
                        texto_escrito2=NO;
                        c_pausa1=RESULTADO_CREDITOS;
                       }           
              } // switch seleccion
            break;      

        case RESULTADO_CREDITOS:
            // calculo de clave 3
            password=centenas*100+decenas*10+unidades;
            read_nvs(key3,&clave3);
            if (password==clave3)
              {
                creditos=creditos+50;
                write_nvs(key,creditos);
                Captura_Exitosa("SALDO");
              }
            else
              {
                Captura_Exitosa("ERROR");
              }

            break;



    } // fin switch

}

/* ---------------------------------------------------------------------*/
void Captura_Exitosa(String mensaje1)
{
  switch (c_pausa) // calcula pausa de 3 segundos
    {
      case REINICIA_PAUSA:
          Escribir(A1,FIJO,GREEN ,mensaje1);
          Escribir(B1,FIJO,GREEN ,String(creditos));
          previous_time=millis();
          c_pausa=CALCULA_DURACION;
          break;
      case CALCULA_DURACION:
          current_time=millis();
          if ((previous_time+3000)>=current_time)
            {   
              current_time=millis();
            }
          else
            {    
              b_titulos=TEXTO_NOMBRE_JUEGO;
              c_pausa=REINICIA_PAUSA;
              Escribir(A1,FIJO,GREEN ,"        ");
            }
            break;         

    }
}

/* ---------------------------------------------------------------------*/

void Selecciona_Juegos_z()
{
  /*boton avanza*/
  if ( flancoSubida(BTN_AVANZA))
     {
        if (juego_actual==JUEGO1) // switchea el nombre
            {
              juego_actual=JUEGO2; 
             texto_escrito2_juego=NO;
           }
        else 
            {
              juego_actual=JUEGO1;
             texto_escrito2_juego=NO;
            }
      }

   /* btn enter */

  if ( flancoSubida(BTN_ENTER))
    {
      // regresa variables a valor inicial antes de salir
      texto_escrito1_numero=NO;      
      e_juego_seleccion=ESCRIBE_MENSAJE_SELECCION;
      // continuar a -->
      d_configuracion=SELECCIONA_NO_JUGADORES;

    }           
  /* fin enter*/

  switch (e_juego_seleccion)
    {
      case ESCRIBE_MENSAJE_SELECCION:
          if (texto_escrito1_juego==NO)
            {
              Escribir(A1,FIJO,GREEN ,"JUEGO");
              texto_escrito1_juego=SI;
              e_juego_seleccion=ESCRIBE_NOMBRE_JUEGO;
            }
          break;

      case ESCRIBE_NOMBRE_JUEGO:
        if (texto_escrito2_juego==NO)
          if (juego_actual==JUEGO1)
            {
              Escribir(B1,FIJO,GREEN ,"2 PISTOLAS");
              texto_escrito2_juego=SI;

            }
          else
            {
              Escribir(B1,FIJO,GREEN ,"1 PISTOLA");
              texto_escrito2_juego=SI;
            }
          break;

    } // fin switch
} 

/* ---------------------------------------------------------------------*/

void Descuenta_Creditos_Juego_z (int8_t local_num_juego, int8_t local_regresa_a)
{
  if (numero_actual_jugadores>creditos)
     {
      a_menu=CREDITOS_INSUFICIENTES;
      c_pausa=REINICIA_PAUSA;
     }
  else
     {
        creditos= creditos-numero_actual_jugadores;
        write_nvs(key,creditos);
        if (local_num_juego==JUEGO1)
          {
            j1_menu_navegacion=local_regresa_a;
          }
        else
          {
            j2_menu_navegacion=local_regresa_a;
          }
     }
}

/* ---------------------------------------------------------------------*/

void Selecciona_No_Jugadores_x()
{

  /* Boton Avaza   Asigna el numero de jugadores con base en el juego seleccionado*/
  if ( flancoSubida(BTN_AVANZA))
     {
        switch (juego_actual)
           {
              case JUEGO1:
                  /* code */
                  if (numero_actual_jugadores<4)
                    {
                      numero_actual_jugadores++;
                    }
                  else
                    {
                      numero_actual_jugadores=1;
                    }
                  break;

               case JUEGO2:
                    /* code */
                    if(numero_actual_jugadores<=2)
                      {
                        numero_actual_jugadores=4;                       
                      }
                    else
                      {
                        numero_actual_jugadores=2;
                      }
                    break;
           }
          texto_escrito2_numero=NO;
     } /* fin avanza*/


    /* Boton Enter */
    if ( flancoSubida(BTN_ENTER))
       {
          d_configuracion=SELECCIONA_NOMBRES;
          // el juego ya quedo guardado en juego_actual 
       }  /* fin Enter*/

     switch (f_no_jugadores)
       {
          case REINICIA_ESCRITURA:
              texto_escrito1_numero=NO;
              texto_escrito2_numero=NO;
              f_no_jugadores=ESCRIBE_MENSAJE_NO_JUGADORES;
              break;

          case ESCRIBE_MENSAJE_NO_JUGADORES:
              /* code */
              if (texto_escrito1_numero==NO)
                 {
                   Escribir(A1,FIJO,GREEN ,"JUGADORES");
                   texto_escrito1_numero=SI;
                   f_no_jugadores=ESCRIBE_NO_ACTUAL;
                 }
              break;

          case ESCRIBE_NO_ACTUAL:
            if (texto_escrito2_numero=NO)
              {
                switch (numero_actual_jugadores)
                  {
                    case 1:
                      Escribir(B1,FIJO,GREEN ,"UNO");
                      break;
                    case 2:
                      Escribir(B1,FIJO,GREEN ,"DOS");
                      break;
                    case 3:
                      Escribir(B1,FIJO,GREEN ,"TRES");
                      break;
                    case 4:
                      Escribir(B1,FIJO,GREEN ,"CUATRO");
                      break;
                    default:
                      break;
                  }
                texto_escrito2_numero=SI;
              }
            /* code */
            break; 
       } //fin switch
}

/* ---------------------------------------------------------------------*/

void Selecciona_Nombres_x()
{

  /*boton avanza*/
  if ( flancoSubida(BTN_AVANZA))
     {
        if (nombres_asignados<2)
            {
              nombres_asignados++;
            }
        else
            { 
              nombres_asignados=1;
            }
        switch (nombres_asignados)
            {
              case UNO:
                  j0="NUMEROS"; j1="UNO";j2="DOS"; j3="TRES"; j4="CUATRO";
                  nombres_jugadores[0]="NUMEROS";
                  nombres_jugadores[1]="UNO";
                  nombres_jugadores[2]="DOS";
                  nombres_jugadores[3]="TRES";
                  nombres_jugadores[4]="CUATRO";
                  break;

              case DOS:
                  j0="SUPERHEROES";j1="BARMAN"; j2="DROGIN"; j3="SUPER-BAR"; j4="RAPIDIN";
                  nombres_jugadores[0]="HEROES";
                  nombres_jugadores[1]="BARMAN";
                  nombres_jugadores[2]="DROGIN";
                  nombres_jugadores[3]="SUPER-BAR";
                  nombres_jugadores[4]="RAPIDIN";  
                  break;

            }
        todos_los_nombres=j1+"-"+j2+"-"+j3+"-"+j4;
      }
    /*fin avanza*/

    /*boton Enter*/
    if ( flancoSubida(BTN_ENTER))
        {
          if (juego_actual=JUEGO1)
            {
             h_nombre_jugadores=REINICIA_VARIABLES_NOMBRES;
             a_menu=JUEGO1;
            }
          else
            {
            h_nombre_jugadores=REINICIA_VARIABLES_NOMBRES;              
              a_menu=JUEGO2;
            }
        }  
    /* fin Enter*/

    switch(h_nombre_jugadores)
      {
        case REINICIA_VARIABLES_NOMBRES:
            titulo_escrito=NO;
            grupo_escrito=NO;
            h_nombre_jugadores=ESCRIBE_TITULO_NOMBRES;
            break;

        case ESCRIBE_TITULO_NOMBRES:
            if (titulo_escrito==NO)
              {
                Escribir(A1,FIJO,GREEN ,j0);
                titulo_escrito=SI;
                h_nombre_jugadores =ESCRIBE_GRUPO_NOMBRES;
              }
             break;
              
        case ESCRIBE_GRUPO_NOMBRES:
              if (grupo_escrito==NO)
                {
                  Escribir(A1,FIJO,GREEN , todos_los_nombres);
                  grupo_escrito=SI;
                }
              break;
      } // fin switch
}


/* ------------------------------------------------------------------- */
void PausaRound_Con_Contador_Descendente(int8_t local_juego, int8_t local_round,int8_t local_regresa_a)
{
 switch (c_contador)
  {
    case REINICIA_CONTADOR:
        /* code */
        contador_segundos=30;
        texto_escrito2=NO;
        c_contador=ESCRIBE_CONTADOR;
        previous_time=millis();
        if (local_round<ROUND4)
            Escribir(A1,FIJO,GREEN , "PAUSA" );
        else
             Escribir(A1,FIJO,GREEN , "SEMI-FINAL" );
        break; 

    case ESCRIBE_CONTADOR:
        /*CODE*/
        if (texto_escrito2=NO)
          {
            Escribir(B1,FIJO,GREEN , String(contador_segundos) );
            texto_escrito2 =SI;
            c_contador = ACTUALIZA_CONTADOR;
          }
        break; 

     case ACTUALIZA_CONTADOR:
        /* code */
        current_time=millis();
        if (current_time>(previous_time+1000))
            {
              contador_segundos=contador_segundos-1;
              previous_time=millis();
              c_contador=ESCRIBE_CONTADOR;
              texto_escrito2=NO;
              if (contador_segundos=0)
                { 
                  switch (local_juego)
                  {
                    case JUEGO1:
                         j1_menu_navegacion=local_regresa_a;
                         break;
                    case JUEGO2:
                         j2_menu_navegacion=local_regresa_a;
                         break;
 
                  } // fin switch local_juego

                  
                }
            }
        break; 

  } //fin switch (c_contador)
}

/* -------------------------------------------------------------------*/

void ComparaTiempo_Y_CambiaPosicion(int8_t primer_marcador, int8_t segundo_marcador)
{
 int8_t casilla_de_paso;
 if (jugador_tiempo[tabla_marcador[primer_marcador]]>jugador_tiempo[tabla_marcador[primer_marcador]])
  {
    casilla_de_paso=tabla_marcador[primer_marcador];
    tabla_marcador[primer_marcador]= tabla_marcador[segundo_marcador];
    tabla_marcador[primer_marcador]= casilla_de_paso;
  }
}

/* -----------------------------------------------------------------------*/

void Resultado_Final_Juego_x(int8_t local_juego)
{ 

  switch (control_marcador)
     {
        case REINICIA_CONTROL_MARCADOR:
            /* code */
            Limpia_A1();
            Limpia_A2();
            temporal_posicion= Calcula_Posicion_Jugador(UNO);
            c_pausa=REINICIA_PAUSA;
            control_marcador=ESCRIBE_POSICION ;
            texto_escrito1=NO;
            break;
                              
       case ESCRIBE_POSICION:
            /*code*/
            // para reacomodar las posiciones finales
            switch (inicio_secuencia_mostrar )
               {
                  case UNO:
                        if (texto_escrito1==NO)
                        {
                          Escribir(A1,FIJO,GREEN , "PRIMERO") ;
                          Escribir(A1,FIJO,GREEN , nombres_jugadores[tabla_marcador[1]]);
                          texto_escrito1=SI;
                        }
                        break;

                  case DOS:
                        if (texto_escrito1==NO)
                        {
                          Escribir(A1,FIJO,GREEN , "SEGUNDO") ;
                          Escribir(A1,FIJO,GREEN , nombres_jugadores[tabla_marcador[2]]);
                          texto_escrito1=SI;
                        }
                        break;
                  case TRES:
                        if (texto_escrito1==NO)
                        {
                          Escribir(A1,FIJO,GREEN , "TERCERO") ;
                          Escribir(A1,FIJO,GREEN , nombres_jugadores[tabla_marcador[3]]);
                          texto_escrito1=SI;
                        }
                        break;

                  case CUATRO:
                        if (texto_escrito1==NO)
                          {
                            Escribir(A1,FIJO,GREEN , "CUARTO") ;
                            Escribir(A1,FIJO,GREEN , nombres_jugadores[tabla_marcador[4]]);
                            texto_escrito1=SI;
                          }
                        break;

               } //fin switch (inicio_secuencia_mostrar)
            control_marcador=ESPERA_3_SEGUNDOS;
            break; // fin ESCRIBE_POSICION

        case ESPERA_3_SEGUNDOS:
              /*code*/
              switch (c_pausa)
                 {
                    case REINICIA_PAUSA:
                        /* code */
                        previous_time=millis();
                        c_pausa=CALCULA_DURACION;
                        break;

                    case CALCULA_DURACION:
                        /*CODE*/
                        current_time=millis();
                        if ((previous_time+3000< current_time))
                          {
                            switch (local_juego)
                            {
                              case JUEGO1:
                                c_pausa=REINICIA_PAUSA;
                                inicio_secuencia_mostrar++;
                                if (inicio_secuencia_mostrar<5)
                                  {
                                    texto_escrito1=NO;
                                    control_marcador=ESCRIBE_POSICION;
                                  }
                                else
                                  {
                                    Limpia_A1();
                                    Limpia_A2();
                                    a_menu=INTRODUCCION;
                                  }
                                  break;

                              case JUEGO2:
                                c_pausa=REINICIA_PAUSA;
                                inicio_secuencia_mostrar++;
                                if (inicio_secuencia_mostrar<3)
                                  {
                                    texto_escrito1=NO;
                                    control_marcador=ESCRIBE_POSICION;
                                  }
                                else
                                  {
                                    Limpia_A1();
                                    Limpia_A2();
                                    a_menu=INTRODUCCION;
                                  }
                                  break;

                            } //final swtich

                          }
                        break;

                 } // fin de swtich(c_pausa)
              break; // din ESPERA_3_SEGUNDOS 

     } //fin switch(control_marcador)
}
 
/*-----------------------------------------------------------------*/

void Genera_Claves_y_guarda()
{
  float acumulado;
  float dclave1;
  float dclave2;
  dclave1=2+rand()%97;
  dclave2=2+rand()%97; 
  acumulado=log(dclave1/10)+log(dclave2/10);
  acumulado = acumulado-int(acumulado);
  acumulado=acumulado*1000;
  clave3=int(acumulado);
  clave1=int(dclave1);
  clave2=int(dclave2);
  write_nvs(key1,clave1);
  write_nvs(key2,clave2);
  write_nvs(key3,clave3);

}

/*----------------------------------------------------------------*/

void Escribe_Titulos( String textoa, String textob,int8_t siguiente_menu)
{
  switch (c_pausa) // calcula pausa de 2 segundos
   {
      case REINICIA_PAUSA:
                        Escribir(A1,FIJO,GREEN ,textoa);
                        Escribir(B1,FIJO,GREEN ,textob);
                        previous_time=millis();
                        c_pausa=CALCULA_DURACION;
                        break;
                    case CALCULA_DURACION:
                        current_time=millis();
                        if ((previous_time+2000)>=current_time)
                        {   
                          current_time=millis();
                        }
                        else
                        {    
                          b_titulos=siguiente_menu;
                          c_pausa=REINICIA_PAUSA;
                          Limpia_A1();
                          Limpia_A2(); 
                        }
                        break;
                    }
}

/*--------------------------------------------------------------*/

void Reinicia_Juego_z(int8_t local_juego,int8_t local_regreso)
{ 
  texto_escrito1_numero=NO;
  texto_escrito2_numero=NO;
  // posiciones manracado
  tabla_marcador[0]=0;
  tabla_marcador[1]=4;
  tabla_marcador[2]=3;
  tabla_marcador[3]=2;
  tabla_marcador[4]=1;
  //reinicia tiempo de juegadores solo al inicia del juego
  for (int i=0;i<=4;i++)
    {
    jugador_tiempo[i]=0.00;    
    }
  // reinicia primer turno a jugar
  turno_de_jugadores=TURNO_UNO;
  // varibles de void Resultado_final_juego_x
  inicio_secuencia_mostrar=UNO;
  temporal_posicion=0;
  // analiza nuevamente si puede comunicarse conlo peer que no se
  // al encender el monitor
  // sincronizacin de dianas (peer)
  Valida_Dianas_En_Linea();
  Test_Conexion_Diana();
  //regreso
  if (local_juego==JUEGO1)
    {
    j1_menu_navegacion=J1_BIENVENIDA_JUEGO;
    }
  else
  {
    j2_menu_navegacion=J2_BIENVENIDA_JUEGO;
   }


}



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

*/



