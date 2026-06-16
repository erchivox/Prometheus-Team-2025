//acuerdate que este codigo tiene 4 segundos para deteccion de una nueva linea
// advertencia : toma 2 tasas de cafe, guarda una copia de seguridad y cuidate del bug que solo pasa a las 
// 12pm, de resto suerte. 
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <Adafruit_BNO055.h>

#include <ESP32Servo.h>
#include <ColorConverterLib.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_NeoPixel.h>

// --- Configuración de NeoPixels ---
#define NEOPIXEL_PIN 14   // ¡CÁMBIALO al pin de datos que vayas a usar!
#define NUMPIXELS 10       // ¡CÁMBIALO a la cantidad exacta de LEDs que tenga tu tira/módulo!

// Crear el objeto de la tira LED
Adafruit_NeoPixel tiraLED(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// --- Pines XSHUT para ToF ---
#define XSHUT_FRONTAL 17
#define XSHUT_TRASERO 16
#define XSHUT_IZQ     5
#define XSHUT_DER     18

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_2_4MS, TCS34725_GAIN_1X);
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);
Servo direccion;

#define I2C2_SDA 32
#define I2C2_SCL 33
TwoWire I2CSegundo = TwoWire(1);

// --- Direcciones I2C ToF ---
#define ADDR_FRONTAL 0x30
#define ADDR_TRASERO 0x31
#define ADDR_IZQ     0x32
#define ADDR_DER     0x33

// --- Objetos ---
Adafruit_VL53L0X tofF = Adafruit_VL53L0X();
Adafruit_VL53L0X tofT = Adafruit_VL53L0X();
Adafruit_VL53L0X tofI = Adafruit_VL53L0X();
Adafruit_VL53L0X tofD = Adafruit_VL53L0X();

// --- Variables BNO055 ---
float yaw_offset = 0.0f;

// --- Configuración de Pines del Motor (Puente H) ---
const int MOTOR_IN1 = 27; // ¡Corregido! Estaba en conflicto con el pin 27 del interruptor
const int MOTOR_IN2 = 26; // Pin de dirección
const int MOTOR_ENA = 25; // Pin PWM para la velocidad

// --- Variables de Velocidad (rango de 0 a 255) ---
int velocidadBase = 255;      // Velocidad normal para avanzar
int velocidadRetroceso = 200; // Velocidad para echar hacia atrás
int velocidadLenta = 180;           // Velocidad reducida para avanzar 
int velocidadRetrocesoLento = 160;  // Velocidad reducida para retroceder

// --- Configuración de Canales PWM Independientes para ESP32 ---
const int canalMotor  = 4;
const int canalBuzzer = 5;
const int canalLED = 6;// Reservado por si quieres atenuar el LED luego

const int freqMotor = 1000;
const int resolucionPWM = 8;  // Resolución de 8 bits (0-255)

//const int MOTOR_RETROCESO_PIN = 19;  // Pin para motor de retroceso
//const int MOTOR_PIN = 15;
//const int MOTOR_RETROCESO_PIN_LENTO = 25;  // Pin para motor de retroceso
//const int MOTOR_PIN_LENTO = 26;
const int SERVO_PIN = 13;
// Declaraciones de pines (usar pines analógicos como digitales)
#define BUZZER_PIN  4  // Pin analógico A1 usado como digital para buzzer

// esta línea cerca del inicio del archivo, con las otras variables globales
bool primerGiroRealizado = false; // Flag para controlar el primer giro especial

//******************** colores 
// Variables para la detección del color azul
const int HUE_AZUL_MIN = 185;
const int HUE_AZUL_MAX = 240;
const float SATURACION_AZUL_MIN = 0.57;//0.40;
const float VALOR_AZUL_MIN = 0.40;
const float VALOR_AZUL_MAX = 0.80;
const float PROPORCION_AZUL_MIN = 1.1;//1.66
const int CONFIRMAR_AZUL_TOLERANCIA = 1;
const int CONFIRMAR_AZUL_DELAY = 1;
  
// Variables para la detección del color naranja
const int HUE_NARANJA_MIN = 2;//8;
const int HUE_NARANJA_MAX = 45;
const float SATURACION_NARANJA_MIN = 0.53;//40;
const float VALOR_NARANJA_MIN = 0.35;
const float VALOR_NARANJA_MAX = 0.72;
const int CONFIRMAR_NARANJA_TOLERANCIA = 1;
const int CONFIRMAR_NARANJA_DELAY = 1;
//*****888*******************************


const int INTERRUPTOR_PIN = 23;  // Pin digital para el interruptor (usa pin con interrupt)

// Variables de estado del sistema
bool sistemaIniciado = false;
bool interruptorAnterior = false;
unsigned long tiempoDebounce = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// ===== CONFIGURACIÓN DE SERVO =====
const int SERVO_RIGHT = 0;
const int SERVO_CENTER = 90;
const int SERVO_LEFT  = 180;

const int SERVO_OrientadoObjeto_LEFT = 130;
const int SERVO_OrientadoObjeto_RIGH = 50;

const int SERVO_CENTRADO = SERVO_CENTER;

int contadorEntradaEstadoFinal = 0;

// Contador de líneas
int contadorLineas = 0;
int limiteLineas = 12;

int16_t ax, ay, az, gx, gy, gz;

float anguloZ = 0.0;
float pitch = 0.0;          // Ángulo Pitch (inclinación adelante/atrás)
float roll = 0.0;           // Ángulo Roll (inclinación lateral)

float anguloObjetivoGlobal = 0.0; // angulo recto para el giro


  float distanciaDerecha= 0 ;
    float distanciaIzquierda = 0 ;

const unsigned long TIEMPO_RETROCESO_MAXIMO = 1000;      // 1000Mayor tiempo para el giro más cerrado
const unsigned long TIEMPO_RETROCESO_ALTO = 0;        // 800Tiempo intermdio
const unsigned long TIEMPO_RETROCESO_MEDIO_ALTO = 0;  //600 Menor tiempo

// Ángulos de giro modificables para cada sentido
float giroObjetivoAzul = 70.0;    // Giro a la izquierda para línea azul
float giroObjetivoNaranja = 70.0; // Giro a la derecha para línea naranja (modificable)

float offsetGz = 0.0;
float offsetGx = 0.0; // Offset del giroscopio en X (Roll)
float offsetGy = 0.0; // Offset del giroscopio en Y (Pitch)

float umbralGiro = 1.0;
float umbralReposo = 0.5;
// Control de tiempo
unsigned long lastYawPrint = 0;
const unsigned long YAW_INTERVAL = 300;

unsigned long lastTime, startTime, timeoutGiro = 2000;


// ===== VARIABLES DE CONTROL DE DIRECCIÓN =====
bool corrigiendo = false;           // Estado de corrección activa
unsigned long tiempoInicioCorreccion = 0;
const unsigned long TIEMPO_MAX_CORRECCION = 6000; // 3 segundos máximo corrigiendo
const unsigned long TIEMPO_AVANCE_REINTENTO = 400; // 400ms de avance para reintentar
const unsigned long TIEMPO_ESTABILIZACION = 200;  // 200ms para estabilizar antes de verificar

float anguloObjetivo = 0.0;         // Ángulo objetivo dinámico
const float TOLERANCIA_ANGULO = 5.0; //4 antes Tolerancia de ±15° para considerar "recto"
const float TOLERANCIA_ANGULO_CORRECCION_LINEA_ANTES_DEL_GIRO = 15.0;
// ===== ESTADOS DE CORRECCIÓN =====
enum EstadoCorreccion {
  RECTO,                  // Vehículo va recto (0° ± margen)
  CORRIGIENDO_IZQUIERDA,  // Corrigiendo hacia la izquierda
  CORRIGIENDO_DERECHA,    // Corrigiendo hacia la derecha
  ESTABILIZANDO           // Estabilizando después de corrección
};

EstadoCorreccion estadoActualCorreccion = RECTO;
bool correccionCompletada = false;  // Flag para indicar que la corrección terminó

// Variables para control de centrado
bool enGiro = false;
unsigned long tiempoFinGiro = 0;
const unsigned long tiempoEsperaPosgiro = 0;
const int anguloCorreccionMax = 50;
const unsigned long intervaloCentrado = 100;
unsigned long ultimoTiempoCentrado = 0;

// Variables para pausar centrado durante detección
bool pausarCentrado = false;
unsigned long tiempoInicioDeteccion = 0;
const unsigned long tiempoPausaCentrado = 100;

// Variables para detección de líneas
bool lineaNaranjaDetectada = false;
unsigned long tiempoUltimaDeteccion = 0;
//tiempo de espera para deteccion de linea correcta, tambien se activa cuando inicia girando a carril.
const unsigned long tiempoEsperaDeteccion = 5000;//4000

// Flag para forzar tiempo de giro a 500ms al evadir obstáculo frontal
//Flag para controlar el inicio del cooldown tras un comando de carril
//bool iniciarCooldownCarril = false;

// Variables para direccionalidad
enum SentidoGiro {
  INDEFINIDO,
  IZQUIERDA,  // Detectó azul primero
  DERECHA     // Detectó naranja primero
};

SentidoGiro sentidoVehiculo = INDEFINIDO;
bool primeraDeteccion = true;

// ==================== VARIABLES PID PARA GIROS ====================

// Variables para controlador PID de giros
struct PIDController {
  float kp;           // Ganancia proporcional
  float ki;           // Ganancia integral
  float kd;           // Ganancia derivativa
  float error_anterior;
  float integral;
  float integral_max; // Límite para evitar windup
  unsigned long tiempo_anterior;
};

// Configuración del PID para giros
PIDController pidGiro = {
  .kp = 2.5,          // Ganancia proporcional - ajusta respuesta principal
  .ki = 0.8,          // Ganancia integral - elimina error en estado estacionario
  .kd = 0.4,          // Ganancia derivativa - reduce oscilaciones
  .error_anterior = 0,
  .integral = 0,
  .integral_max = 30, // Límite para integral windup
  .tiempo_anterior = 0
};

// Variables adicionales para control PID
float anguloObjetivoActual = 0;
bool giroEnProceso = false;
const float TOLERANCIA_PID = 5.0;      // Tolerancia más estricta con PID

// ==================== VARIABLES PID PARA CENTRADO ====================

// Variables para controlador PID de centrado
struct PIDCentrado {
  float kp;           // Ganancia proporcional
  float ki;           // Ganancia integral  
  float kd;           // Ganancia derivativa
  float error_anterior;
  float integral;
  float integral_max; // Límite para evitar windup
  unsigned long tiempo_anterior;
  bool inicializado;  // Para manejar primera ejecución
};

// Configuración del PID para centrado
PIDCentrado pidCentrado = {
  .kp = 2.0,          // Ganancia proporcional - respuesta principal al error
  .ki = 0.0,          // Ganancia integral - elimina error residual
  .kd = 1.2,          // Ganancia derivativa - reduce oscilaciones
  .error_anterior = 0,
  .integral = 0,
  .integral_max = 20, // Límite para integral windup
  .tiempo_anterior = 0,
  .inicializado = false
};

// Variables adicionales para control PID del centrado
const float TOLERANCIA_CENTRADO_PID = 5.0;    // Tolerancia más estricta
const float ERROR_MAXIMO_CENTRADO = 30.0;     // Error máximo esperado
const float FACTOR_SUAVIZADO = 0.7;           // Para suavizar cambios bruscos

// Buffer para comandos serie
String comandoRecibido = "";
  bool comandoListo = false;

// Pines para sensores de estacionamiento (el frontal ya está definido)
const int ULTRASONIDO_TRASERO_TRIG_PIN  = 18; // Pin para Trig del sensor ultrasónico trasero
const int ULTRASONIDO_TRASERO_ECHO_PIN = 35; // Pin para Echo del sensor ultrasónico trasero

//-------variables de evasion de objetos--------------

// Estados del sistema integrado
enum EstadoSistema {
  ESTADO_NORMAL,
  ESTADO_DETENCION_FINAL,
  ESPERANDO_COMANDO_CARRIL,
  EVALUANDO_POSICION,
  GIRANDO_HACIA_CARRIL,
  ENDEREZANDO_CARRIL,
  MANTENIENDO_CARRIL,
  ESTADO_CORRECCION_LATERAL 
};

// Variables para recepción serial optimizada (del código de esquive)
const int BUFFER_SIZE = 32;
char buffer[BUFFER_SIZE];
int bufferIndex = 0;

// Variables para parsing ultra rápido
char colorDetectado = 'N';
int distanciaObjeto = 0;
char orientacionObjeto = 'C';

// Variables de estado
EstadoSistema estadoActual = ESPERANDO_COMANDO_CARRIL;
char colorObjetivo = 'N';
unsigned long tiempoEsquive = 0;
int etapaEsquive = 0;
char lastOrientation = 'X';

// Configuración de distancias para esquive
const int DISTANCIA_ROJO = 35;//40;    // Distancia de esquive para objetos rojos
const int DISTANCIA_VERDE = 40;//45;   // Distancia de esquive para objetos verdes
const int DISTANCIA_EMERGENCIA = 8;
// Tiempos de esquive por color (en milisegundos)
const unsigned long TIEMPO_ESQUIVE_VERDE = 600;   // Tiempo para esquivar verde
const unsigned long TIEMPO_ESQUIVE_ROJO = 400;   // Tiempo para esquivar rojo
const unsigned long TIEMPO_RETORNO_VERDE = 600;   // Tiempo de retorno para verde
const unsigned long TIEMPO_RETORNO_ROJO = 600;    // Tiempo de retorno para rojo

// Variables para almacenar los tiempos actuales
unsigned long tiempoEsquiveActual = 0;
unsigned long tiempoRetornoActual = 0;

// Variables para recordar último color detectado
char ultimoColorDetectado = 'N';           // Último color válido recibido
unsigned long tiempoUltimoColor = 0;       // Timestamp del último color detectado
const unsigned long TIEMPO_MEMORIA_COLOR = 0; // 4 segundos de memoria

// Variables para función de emergencia
bool emergenciaActivada = false;
unsigned long tiempoUltimaEmergencia = 0;
const unsigned long TIEMPO_ESPERA_EMERGENCIA = 2000; // Evitar activaciones múltiples
char colorEmergencia = 'N';

// Variables para interrupción de cruce
volatile bool interrupcionCruce = false;
volatile char comandoInterrupcion = 'N';
bool cruceEnProceso = false;
//------------------- Fin de variables de esquive--------------

// ==================== CONFIGURACIÓN Y VARIABLES SHARP ====================

// Pines para sensores Sharp diagonales
const int SHARP_DIAGONAL_IZQ_PIN = 35;  // Pin analógico para sensor Sharp izquierdo
const int SHARP_DIAGONAL_DER_PIN = 34;  // Pin analógico para sensor Sharp derecho

const float DISTANCIA_EMERGENCIA_SHARP_PARED = 13.0; // Distancia en cm para activar el retroceso de emergencia
const unsigned long TIEMPO_RETROCESO_SHARP_PARED = 700;  //800 Tiempo de retroceso en ms (ajustable)

 //CONSTANTES PARA CORRECCIÓN EN PARED *****
const float DISTANCIA_MINIMA_SHARP_PARED = 12.0; // Distancia en cm para activar la corrección
const int ANGULO_CORRECCION_SHARP_IZQ = 180;     // Ángulo para girar a la izquierda (alejarse de la pared derecha)
const int ANGULO_CORRECCION_SHARP_DER = 0;      // Ángulo para girar a la derecha (alejarse de la pared izquierda)
unsigned long tiempoUltimaEmergenciaSharpDer = 0;
unsigned long tiempoUltimaEmergenciaSharpIzq = 0;
const unsigned long COOLDOWN_EMERGENCIA_SHARP = 2000; // 3 segundos para considerar una detección como "reciente"
const unsigned long DURACION_AVANCE_CORRECTIVO = 800; // 400ms de avance

// Variables para esquive diagonal por sensores Sharp
bool esquiveDiagonalActivo = false;
unsigned long tiempoInicioEsquiveDiagonal = 0;
unsigned long tiempoUltimaDeteccionSharp = 0;
char direccionEsquiveDiagonal = 'N'; // 'I' = izquierda, 'D' = derecha
int etapaEsquiveDiagonal = 0;

// Configuración para sensores Sharp diagonales
const float DISTANCIA_SHARP_EMERGENCIA = 15.0;     // 10cm de detección
const unsigned long TIEMPO_CRUCE_DIAGONAL = 500;  // Tiempo de cruce diagonal
const unsigned long TIEMPO_ESPERA_SHARP = 100;     // Evitar detecciones múltiples

// Sistema de prioridades para gestión de emergencias
enum PrioridadEmergencia {
  PRIORIDAD_NORMAL,
  PRIORIDAD_RETROCESO,  // Máxima prioridad
  PRIORIDAD_ESQUIVE     // Prioridad normal
};
PrioridadEmergencia prioridadActual = PRIORIDAD_NORMAL;

// Variables mejoradas para retroceso de emergencia
const int DISTANCIA_RETROCESO_EMERGENCIA = 10;    // Distancia mínima para activar retroceso (cm)
const unsigned long TIEMPO_RETROCESO = 2000;     // Tiempo de retroceso en ms
const unsigned long TIMEOUT_RETROCESO = 2500;    // Timeout de seguridad para retroceso
bool retrocesoPorEmergencia = false;    
// Variables de control temporal
unsigned long tiempoInicioRetroceso = 0;          
unsigned long tiempoUltimaVerificacionRetroceso = 0; 
const unsigned long INTERVALO_VERIFICACION_RETROCESO = 100; // Verificación cada 100ms
const unsigned long TIEMPO_ESPERA_POST_RETROCESO = 100;     // Tiempo de espera después del retroceso
bool motorRetrocesoConfigurado = false;  // controlar configuración de motores

bool retrocesoOrientadoActivo = false;
bool enFaseContragiro = false; // true cuando está ejecutando el contragiro de enderezado
bool omitioGiro = false; // true cuando saltó directo a MANTENIENDO sin hacer giro
unsigned long tiempoInicioRetrocesoOrientado = 0;
const unsigned long TIEMPO_RETROCESO_ORIENTADO = 700; // 1 segundo (modificable)
int direccionRetroceso = SERVO_CENTRADO;

// ==================== ESTRUCTURA DE CONTROL LATERAL ====================
struct ControlDistanciaLateral {
  enum Estado {
    IDLE,
    MOVIENDO_DERECHA,
    MOVIENDO_IZQUIERDA,
    CORRIGIENDO_ANGULO_DERECHA,
    CORRIGIENDO_ANGULO_IZQUIERDA,
    RETROCEDIENDO_ORIENTADO,     
    CORRIGIENDO_POST_RETROCESO,   
    RETROCEDIENDO_FINAL
    };     
  Estado estadoActual = IDLE;
  unsigned long tiempoInicioEstado = 0;
  float anguloObjetivo = 0.0;
  bool funcionActiva = false; 
  unsigned long ultimaVerificacion = 0;
  bool esCorreccionLarga = false; //  Flag para saber qué maniobra ejecutar
  char direccionManiobra = 'N';   // 'D' o 'I' para recordar la dirección

  // Parámetros de control
  const float UMBRAL_MIN = 30.0;//38
  const float UMBRAL_MAX = 55.0; 
  unsigned long tiempoCruceActual = 1000;
  
  const float DISTANCIA_TRASERA_OBJETIVO = 15.0; // cm
  const float TOLERANCIA_ANGULAR = 2.0; 
  const unsigned long INTERVALO_VERIFICACION = 1000;
};

ControlDistanciaLateral controlLateral_PostGiro; // Usamos un nuevo nombre para no confundir con el existente

// Variables para control de retroceso post-giro
unsigned long tiempoInicioEvaluacion = 0;
// ============== CONSTANTES DE ÁNGULOS ==============
const float GIRO_MAXIMO = 70.0;      // Giro máximo - obstáculo muy cerca
const float GIRO_ALTO = 60.0;        // Giro alto - obstáculo cerca
const float GIRO_MEDIO_ALTO = 50.0;  // Giro medio-alto
const float GIRO_MEDIO = 50.0;       // Giro medio
const float GIRO_MEDIO_BAJO = 50.0;  // Giro medio-bajo
const float GIRO_BAJO = 50.0;        // Giro bajo - obstáculo lejano
const float GIRO_MINIMO = 60.0;//30.0 Giro mínimo - obstáculo muy lejano
float anguloObjetivoGiroGlobal = 0;
// Variable para distancia mínima para terminar el retroceso post giro
float DISTANCIA_MINIMA_RETROCESO = 15.0; // 15cm
const unsigned long TIMEOUT_RETROCESO_MAXIMO = 4000;    // Timeout de seguridad de retroceso orientado
// Variable global para medir tiempo de retroceso durante corrección
unsigned long tiempoTotalRetroceso = 0;
bool huboRetrocesoEnCorreccion = false;

// Variables para el giro de apertura
bool giroAperturaActivo = false;
bool giroAperturaCompletado = false;
unsigned long tiempoInicioApertura = 0;
const unsigned long DURACION_APERTURA_MINIMO = 200;//500     // Tiempo para el giro más suave
const unsigned long DURACION_APERTURA_BAJO = 0;   //200    // tiempo, más corto
const unsigned long DURACION_APERTURA_MEDIO_BAJO = 0; //  tiempo, el más corto
bool esGiroApertura = false;
unsigned long duracionAperturaActual = DURACION_APERTURA_MINIMO; // Variable para almacenar la duración del giro actualbool esGiroApertura = false;
bool direccionGiroAperturaIzquierda = false;

bool sistemaActivo = false;  // Estado del sistema (activo/inactivo)

// Variable para controlar giro antes de estacionamiento
bool giroParaEstacionamiento = false;

// tiempo de finalizacion del codigo para avance final
unsigned long tiempoInicio = millis();
//unsigned long duracionCiclo = 3000; // 5 segundos en milisegundos

unsigned long tiempoInicioDetencionFinal = 0;


// Variables globales para el control de arranque del motor
unsigned long tiempoInicioSistema = 0;
bool motorArrancado = false;
const unsigned long DELAY_ARRANQUE_MOTOR = 1000; // 1 segundo en milisegundos

static unsigned long tiempoInicioAvance = 0;

// Variable para pausa de prueba (configurable)
static unsigned long tiempoInicioPausa = 0;

// Configuración para control angular prioritario
#define UMBRAL_ANGULO_MAX 15.0      // Umbral máximo de desviación angular (±20°)
#define TOLERANCIA_ANGULO_FINO 2.0  // Tolerancia para considerar ángulo correcto
#define FACTOR_CORRECCION_ANGULAR 0.8  // Factor fuerte para corrección angular
#define FACTOR_CORRECCION_DISTANCIA 0.3  // Factor menor para distancia cuando hay error angular


// ==================== CONFIGURACIÓN DEL SISTEMA DE CARRILES ====================
struct SistemaCarriles {
  // Definición de carriles
  float distanciaCarril1;
  float distanciaCarril2;  
  float toleranciaCarril;
  float umbralCambio;
  
  // Parámetros de control
  float velocidadGiro;
  unsigned long tiempoGiro;
  unsigned long tiempoGiroCarril1;
  unsigned long tiempoGiroCarril2;
  float toleranciaAngulo;
  
  // Variables de control
  int carrilObjetivo;
  char ladoControl;
  unsigned long tiempoInicioEstado;
  float direccionGiro;
  bool necesitaCambio;
  bool esComandoC21;
  bool esComandoC14; 
  bool esComandoC15;
  bool esComandoC20;
  bool esComandoC26; 
  bool esComandoC33;
  bool esComandoC27;
  bool esComandoC32;

  bool esComandoC36;

  //individuales
  bool esComandoC02;
  bool esComandoC08;

  bool esComandoC07;
  bool esComandoC01;
    bool esComandoC01ModoDistancia; // C01 con sentido IZQUIERDA: tiempo de giro variable por ToF derecho
    bool esComandoC07ModoDistancia;
    bool esComandoC02ModoDistancia;
    bool esComandoC08ModoDistancia;
  
};

SistemaCarriles carriles = {
  .distanciaCarril1 = 20.0,
  .distanciaCarril2 = 20.0,
  .toleranciaCarril = 3.0,
  .umbralCambio = 1.0,
  .velocidadGiro = 45.0,
  .tiempoGiro = 1500,
  .tiempoGiroCarril1 = 1400,//1800
  .tiempoGiroCarril2 = 1300,//1500
  .toleranciaAngulo = 5.0,
  .carrilObjetivo = 0,
  .ladoControl = 'D',
  .tiempoInicioEstado = 0,
  .direccionGiro = 0,
  .necesitaCambio = false,
  .esComandoC21 = false,
  .esComandoC14 = false,
  .esComandoC15 = false,
  .esComandoC20 = false,
  .esComandoC26 = false,
  .esComandoC33 = false,
  .esComandoC27 = false,
  .esComandoC32 = false,
  .esComandoC36 = false,

  //individuales
  .esComandoC02 = false,
  .esComandoC08 = false,

  .esComandoC07 = false,
  .esComandoC01 = false,
    .esComandoC01ModoDistancia = false,
    .esComandoC07ModoDistancia = false,
    .esComandoC02ModoDistancia = false,
    .esComandoC08ModoDistancia = false


  
};
int ultimoCarrilActivo = 0; 
// Variables de tiempo y monitoreo de carriles
unsigned long ultimaActualizacion = 0;
const unsigned long INTERVALO_CONTROL = 50;
unsigned long ultimoReporte = 0;
const unsigned long INTERVALO_REPORTE = 300;


const unsigned long INTERVALO_maxAngulo = 500;  // 2000 ms = 2 segundos

unsigned long ultimaImpresion_maxAngulo = 0;

// Estructura para el control lateral
struct FiltroDistancia {
  float historial[5];
  int indice;
  bool inicializado;
  float ultimaDistanciaValida;
  float umbralCambioMaximo;
  
  void inicializar() {
    for (int i = 0; i < 5; i++) historial[i] = 0;
    indice = 0;
    inicializado = false;
    ultimaDistanciaValida = 0;
    umbralCambioMaximo = 50.0;
  }
  
  float filtrar(float nuevaLectura) {
    if (nuevaLectura <= 0 || nuevaLectura > 150) {
      return ultimaDistanciaValida;
    }
    if (!inicializado || abs(nuevaLectura - ultimaDistanciaValida) <= umbralCambioMaximo) {
      historial[indice] = nuevaLectura;
      indice = (indice + 1) % 5;
      if (!inicializado) {
        for (int i = 0; i < 5; i++) historial[i] = nuevaLectura;
        inicializado = true;
      }
      float suma = 0;
      for (int i = 0; i < 5; i++) suma += historial[i];
      ultimaDistanciaValida = suma / 5.0;
      return ultimaDistanciaValida;
    } else {
      return ultimaDistanciaValida;
    }
  }
};

struct ControlLateral {
  float distanciaObjetivo;
  float toleranciaDistancia;
  float toleranciaAngulo;
  float anguloObjetivo;
  float anguloUmbral;
  float kpDistancia, kiDistancia, kdDistancia;
  float correccionMaxima;
  float factorGiroIzquierda;
  float factorGiroDerecha; 
  float errorDistanciaAnterior;
  float integralDistancia;
  float integralMaxDistancia;
  char ladoControl;
  FiltroDistancia filtroIzquierda;
  FiltroDistancia filtroDerecha;
};

ControlLateral controlLateral = {
  .toleranciaDistancia = 3.0,
  .toleranciaAngulo = 3.0,//algulo aceptado como recto
  .anguloObjetivo = 0.0,
  .anguloUmbral = 20.0, //angulo de desviasion maxima para empezar correccion agresiva
  .kpDistancia = 4.0,
  .kiDistancia = 0.0,
  .kdDistancia = 1.2,
  .correccionMaxima = 40.0,
  .factorGiroIzquierda = 0.8,//1.0
  .factorGiroDerecha = 0.8,
  .errorDistanciaAnterior = 0,
  .integralDistancia = 0,
  .integralMaxDistancia = 20.0,
  .ladoControl = 'D'
};
unsigned long ultimaActualizacionf = 0;
const unsigned long INTERVALO_CONTROLf = 50;
unsigned long ultimoReportef = 0; // Añade esta línea
const unsigned long INTERVALO_REPORTEf = 300; // Añade esta línea (imprime ~3 veces/seg)

// --- Configuración de Tareas Multi-núcleo (FreeRTOS) ---

TaskHandle_t TareaColorHandle = NULL;



// ==================== TEMPORIZADOR NO BLOQUEANTE PARA LEDS ====================
unsigned long tiempoInicioLedTemporal = 0;
unsigned long duracionLedTemporal = 0;
bool ledTemporalActivo = false;

// Tiempo mínimo que el vehículo avanza en MANTENIENDO_CARRIL antes del cambio C21
const unsigned long TIEMPO_AVANCE_MANTENIENDO_C21 = 500; // ms — ajustable
const unsigned long TIEMPO_AVANCE_MANTENIENDO_C14 = 100; // ms — ajustable
const unsigned long TIEMPO_AVANCE_MANTENIENDO_C15 = 800; // ms — ajustable
const unsigned long TIEMPO_AVANCE_MANTENIENDO_C20 = 700; // ms — ajustable

const unsigned long TIEMPO_AVANCE_MANTENIENDO_C26 = 0; // rojo derecha, verde derecha

const unsigned long TIEMPO_AVANCE_MANTENIENDO_C27 = 500; // rojo derecha, verde derecha

const unsigned long TIEMPO_AVANCE_MANTENIENDO_C32 = 200; // rojo derecha, verde derecha
const unsigned long TIEMPO_AVANCE_MANTENIENDO_C33 = 200; // rojo derecha, verde derecha

// Función para lanzar un destello sin bloquear el código
void encenderLedTemporal(uint8_t r, uint8_t g, uint8_t b, unsigned long duracion) {
  colorVehiculo(r, g, b);
  tiempoInicioLedTemporal = millis();
  duracionLedTemporal = duracion;
  ledTemporalActivo = true;
}

// Función que vigila si ya es hora de apagar el destello
void actualizarLedTemporal() {
  if (ledTemporalActivo && (millis() - tiempoInicioLedTemporal >= duracionLedTemporal)) {
    // Si estamos en modo de espera, debe volver a morado. Si no, se apaga.
    if (estadoActual == ESPERANDO_COMANDO_CARRIL) {
      colorVehiculo(128, 0, 255); // Morado
    } else {
      apagarVehiculo();
    }
    ledTemporalActivo = false;
  }
}
// ==============================================================================
// ==================== CONTROL DE NEOPIXELS ====================
// Función para poner todos los LEDs del mismo color de golpe
void colorVehiculo(uint8_t r, uint8_t g, uint8_t b) {
  for(int i = 0; i < tiraLED.numPixels(); i++) {
    tiraLED.setPixelColor(i, tiraLED.Color(r, g, b));
  }
  tiraLED.show();
}

// Función para apagar los LEDs
void apagarVehiculo() {
  tiraLED.clear();
  tiraLED.show();
}
// ==============================================================

// Función independiente que se ejecuta continuamente en el Núcleo 0
void TareaDeteccionColor(void * pvParameters) {
  Serial.print(F("Control del sensor de color iniciado en el núcleo: "));
  Serial.println(xPortGetCoreID());

  while (1) {
    // Ejecuta la lectura del sensor solo si el interruptor activó el sistema
    if (sistemaActivo) {
      detectarColor();
    }
    

    vTaskDelay(pdMS_TO_TICKS(30)); 
  }
}

// ==================== FUNCIONES DE CONTROL DE MOTOR ====================
void detenerMotor() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  ledcWrite(MOTOR_ENA, 0); // <-- Cambiado canalMotor por MOTOR_ENA
}

void avanzarMotor(int velocidad = velocidadBase) {
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  ledcWrite(MOTOR_ENA, velocidad); // <-- Cambiado canalMotor por MOTOR_ENA
}

void retrocederMotor(int velocidad = velocidadRetroceso) {
   digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, HIGH); 
  ledcWrite(MOTOR_ENA, velocidad); // <-- Cambiado canalMotor por MOTOR_ENA
}


void avanzarLento() {
  // Reutiliza la lógica de avanzar, pero le inyecta la velocidad reducida
  avanzarMotor(velocidadLenta); 
}

void retrocederLento() {
  // Reutiliza la lógica de retroceder, pero le inyecta la velocidad reducida
  retrocederMotor(velocidadRetrocesoLento); 
}
// =======================================================================
float leerToF(char sensor) {
  VL53L0X_RangingMeasurementData_t measure;
  switch(sensor) {
    case 'F': tofF.rangingTest(&measure, false); break;
    case 'T': tofT.rangingTest(&measure, false); break;
    case 'I': tofI.rangingTest(&measure, false); break;
    case 'D': tofD.rangingTest(&measure, false); break;
    default: return 999.0;
  }
  if (measure.RangeStatus != 4) {
    return measure.RangeMilliMeter / 10.0;
  } else {
    return 999.0;
  }
}


// ==================== FUNCIONES DE CARRIL ====================
float obtenerDistanciaObjetivo() {
  if (carriles.carrilObjetivo == 1) {
    return carriles.distanciaCarril1;
  } else {
    return carriles.distanciaCarril2;
  }
}

char obtenerLadoControlPorCarril() {
  if (carriles.carrilObjetivo == 1) {
    return 'D';  // Carril 1 siempre usa sensor derecho
  } else {
    return 'I';  // Carril 2 siempre usa sensor izquierdo
  }
}

String obtenerNombreEstado(EstadoSistema estado) { // 1. Cambiado el tipo de parámetro
  switch (estado) {
    // Estados generales
    case ESTADO_NORMAL: return "ESTADO_NORMAL";
    case ESTADO_DETENCION_FINAL: return "ESTADO_DETENCION_FINAL";
    
    // Estados de carril (usando los nombres de EstadoSistema)
    case ESPERANDO_COMANDO_CARRIL: return "ESPERANDO_COMANDO_CARRIL";
    case EVALUANDO_POSICION: return "EVALUANDO_POSICION";
    case GIRANDO_HACIA_CARRIL: return "GIRANDO_HACIA_CARRIL";
    case ENDEREZANDO_CARRIL: return "ENDEREZANDO_CARRIL";
    case MANTENIENDO_CARRIL: return "MANTENIENDO_CARRIL";
    case ESTADO_CORRECCION_LATERAL: return "ESTADO_CORRECCION_LATERAL"; 
    
    default: return "DESCONOCIDO";
  }
}

void cambiarEstado(EstadoSistema nuevoEstado) { // 1. Cambiado el tipo de parámetro
  if (estadoActual != nuevoEstado) { // 2. Se usa la variable global estadoActual
    Serial.println("🔄 Cambio estado: " + obtenerNombreEstado(estadoActual) + 
                  " → " + obtenerNombreEstado(nuevoEstado));
    // ==========================================
    // Control de luz de estado Morada
    if (nuevoEstado == ESPERANDO_COMANDO_CARRIL) {
      colorVehiculo(128, 0, 255); // Morado
    } else {
      apagarVehiculo(); // Asegura que se apague al salir del estado
    }
    // ==========================================
    //*********
    // Si entramos en el estado de mantener carril, guardamos cuál es el carril activo.
    // Esta será nuestra "memoria" para cuando se detecte una línea.
    if (nuevoEstado == MANTENIENDO_CARRIL) {
      ultimoCarrilActivo = carriles.carrilObjetivo;
      Serial.println("✍️ Último carril activo actualizado a: " + String(ultimoCarrilActivo));
    }
    //******
    estadoActual = nuevoEstado; // 3. Se actualiza la variable global
    carriles.tiempoInicioEstado = millis();
    
    // Resetear variables de control del mantenimiento de pared
    controlLateral.integralDistancia = 0;
    controlLateral.errorDistanciaAnterior = 0;

    controlLateral.filtroIzquierda.inicializar();
    controlLateral.filtroDerecha.inicializar();
       // Reset flag C21 si se cambia manualmente de estado
    if (nuevoEstado == ESPERANDO_COMANDO_CARRIL) { // 4. Usar el nombre de EstadoSistema
      carriles.esComandoC21 = false;
      carriles.esComandoC14 = false;
      carriles.esComandoC15 = false;
      carriles.esComandoC20 = false; 
      carriles.esComandoC26 = false;
      carriles.esComandoC33 = false; 
      carriles.esComandoC27 = false; 
      carriles.esComandoC32 = false;
      carriles.esComandoC02 = false;
      carriles.esComandoC08 = false;
      carriles.esComandoC36 = false;
      carriles.esComandoC07 = false;
      carriles.esComandoC01 = false;
      omitioGiro = false; // Resetear flag al volver a esperar comando
      
      // Limpiar buffers de comunicación para asegurar que se espere un comando NUEVO.
      Serial.println("Buffers de comunicación limpiados. Esperando nuevo comando...");

      while (Serial.available() > 0) {
        Serial.read(); // Lee y descarta cada byte del búfer USB
      }
    }
  }
}

// ==================== MÁQUINA DE ESTADOS DE CARRIL ====================
float procesarEstadoEsperandoComando() {
  // Sistema detenido esperando comando
  return 90.0;
}

float procesarEstadoEvaluandoPosicion() {
  
  float distanciaIzq = leerToF('I');
  float distanciaDer = leerToF('D');
  
  // Determinar qué sensor usar para el control principal según 'sentidoVehiculo'
  char ladoControlEvaluacion = (sentidoVehiculo == DERECHA) ? 'I' : 'D';
  float distanciaControl = (ladoControlEvaluacion == 'I') ? distanciaIzq : distanciaDer;

  Serial.println(" Sin obstáculos detectados, evaluando posición normal");
  
  if (distanciaControl < 0 || distanciaControl > 200) {
    Serial.println("⚠ No se puede leer distancia, manteniendo posición");
    cambiarEstado(ESPERANDO_COMANDO_CARRIL);
    return 90.0;
  }
  
  // =========================================================================
  // Verificar si el robot YA ESTÁ en el carril objetivo
  // dependiendo de si el carril representa la distancia corta o larga
  // según el sentido de giro.
  // =========================================================================
  
  const float MIN_DISTANCIA_CORTA = 0.0;
  const float MAX_DISTANCIA_CORTA = 26.0;
  
  const float MIN_DISTANCIA_LARGA = 65.0;
  const float MAX_DISTANCIA_LARGA = 83.0;

  bool yaEnCarrilObjetivo = false;

  if (carriles.carrilObjetivo == 1) {
    if (sentidoVehiculo == IZQUIERDA) {
      // Pared a la derecha -> Carril 1 es la distancia corta
      if (distanciaControl >= MIN_DISTANCIA_CORTA && distanciaControl <= MAX_DISTANCIA_CORTA) {
          yaEnCarrilObjetivo = true;
      }
    } else if (sentidoVehiculo == DERECHA) {
      // Pared a la izquierda -> Carril 1 es la distancia larga
      if (distanciaControl >= MIN_DISTANCIA_LARGA && distanciaControl <= MAX_DISTANCIA_LARGA) {
          yaEnCarrilObjetivo = true;
      }
    }
  } else if (carriles.carrilObjetivo == 2) {
    if (sentidoVehiculo == IZQUIERDA) {
      // Pared a la derecha -> Carril 2 es la distancia larga
      if (distanciaControl >= MIN_DISTANCIA_LARGA && distanciaControl <= MAX_DISTANCIA_LARGA) {
          yaEnCarrilObjetivo = true;
      }
    } else if (sentidoVehiculo == DERECHA) {
      // Pared a la izquierda -> Carril 2 es la distancia corta
      if (distanciaControl >= MIN_DISTANCIA_CORTA && distanciaControl <= MAX_DISTANCIA_CORTA) {
          yaEnCarrilObjetivo = true;
      }
    }
  }

  // Si ya estamos dentro del rango del carril, saltamos el giro
  if (yaEnCarrilObjetivo && !carriles.esComandoC14 && carriles.esComandoC21 && carriles.esComandoC20) {
    Serial.println("✅ El vehículo ya está en el Carril " + String(carriles.carrilObjetivo) + 
                     " (Sensor " + String(ladoControlEvaluacion) + ": " + String(distanciaControl, 1) + "cm).");
    Serial.println(">> OMITIENDO GIRO: Pasando directo a MANTENIENDO_CARRIL.");
    
    // Actualizar el lado de control para el mantenimiento de pared
    controlLateral.ladoControl = ladoControlEvaluacion;
    
    omitioGiro = true; // Marcar que no hubo giro previo
    cambiarEstado(MANTENIENDO_CARRIL);
    return 90.0;
  }
  // =========================================================================

  // --- SI NO ESTÁ EN EL CARRIL, SE PROCEDE CON EL GIRO NORMAL ---
  float distanciaObjetivo = obtenerDistanciaObjetivo();
  float diferencia = distanciaControl - distanciaObjetivo;
  
  Serial.println(" Carril " + String(carriles.carrilObjetivo) + 
                " | Sensor: " + String(ladoControlEvaluacion) +
                " | Distancia actual: " + String(distanciaControl, 1) + 
                "cm | Objetivo: " + String(distanciaObjetivo, 1) + 
                "cm | Diferencia: " + String(diferencia, 1) + "cm");
  
  // Determinar la dirección del giro basado únicamente en el carril objetivo.
  if (carriles.carrilObjetivo == 1) {
    // Para ir al carril 1, SIEMPRE giramos a la DERECHA.
    carriles.direccionGiro = 1.0; 
  } else { 
    // Para ir al carril 2, SIEMPRE giramos a la IZQUIERDA.
    carriles.direccionGiro = -1.0;
  }

  // Actualizar el lado de control según el carril
  controlLateral.ladoControl = ladoControlEvaluacion;
  
  Serial.println(" Iniciando giro hacia carril " + String(carriles.carrilObjetivo) + 
                " (sensor: " + String(ladoControlEvaluacion) + 
                ", dirección: " + String(carriles.direccionGiro > 0 ? "derecha" : "izquierda") + ")");
                
  cambiarEstado(GIRANDO_HACIA_CARRIL);
  
  return 90.0;
}
// Función auxiliar para verificar si el ángulo está dentro del umbral
bool estaEnRangoAngulo(float anguloActual, float anguloObjetivo, float umbral = 3.0) {
  float diferencia = abs(anguloActual - anguloObjetivo);
  
  // Manejar la transición 0°-360°
  if (diferencia > 180.0) {
    diferencia = 360.0 - diferencia;
  }
  
  return diferencia <= umbral;
}
float procesarEstadoGirandoHaciaCarril() {
  // el cooldown ahora se activa inmediatamente en 'procesarComandoCarril'.
  // Variables estáticas para recordar el estado de la interrupción
  static bool sharpInterrupcionActiva = false;
  static bool lineaInterrupcionActiva = false;
  static unsigned long tiempoInicioInterrupcion = 0;
  static bool pausaPorSharpRealizada = false;
  
  // Variables estáticas para el control de las fases
  static float anguloGiroFijo = 90.0;
  static bool anguloCalculado = false;
  static bool mensajeFase1Mostrado = false;
  static bool mensajeFase2Mostrado = false;
  

  // Resetear el estado cada vez que se entra a este estado de giro
  static unsigned long ultimoTiempoInicioEstado = 0;
  if (carriles.tiempoInicioEstado != ultimoTiempoInicioEstado) {
    sharpInterrupcionActiva = false;
    lineaInterrupcionActiva = false;
    pausaPorSharpRealizada = false;
    anguloCalculado = false; 
    
    // Reseteamos los flags de los mensajes para el nuevo giro
    mensajeFase1Mostrado = false;
    mensajeFase2Mostrado = false;
        enFaseContragiro = false; // Resetear para que el nuevo giro arranque a velocidad normal


    ultimoTiempoInicioEstado = carriles.tiempoInicioEstado;
    Serial.println("🔄 Nuevo giro iniciado - Sistemas de interrupción reseteados.");

    // ── NUEVO: Girar el servo a su posición de giro inicial ANTES de arrancar ──
    // Esto garantiza que C21 (y cualquier otro comando) respete la posición
    // del servo antes de que el loop active avanzarMotor().
    float anguloArranque = 90.0;
    if (carriles.esComandoC21) {
      anguloArranque = (carriles.direccionGiro < 0) ? 180.0 : 0.0;
    } else if (carriles.esComandoC14) {
      anguloArranque = (carriles.direccionGiro < 0) ? 180.0 : 0.0;
    } else {
      // Para los demás comandos, usar el ángulo general según la dirección
      anguloArranque = (carriles.direccionGiro < 0) ? 170.0 : 10.0;
    }
    detenerMotor();
    direccion.write((int)anguloArranque);
    Serial.println("⏳ Pausa de arranque: servo posicionado a " + String((int)anguloArranque) + "° antes de avanzar.");
    delay(300); // Tiempo para que el servo llegue. Ajustable entre 200-400ms.
    avanzarMotor();
    // ──────────────────────────────────────────────────────────────────────────
  }

  unsigned long tiempoEnGiro = millis() - carriles.tiempoInicioEstado;
  unsigned long tiempoGiroActual;
    if (carriles.esComandoC21 && carriles.carrilObjetivo == 1) tiempoGiroActual = 1400;//1600
  else if (carriles.esComandoC21 && carriles.carrilObjetivo == 2) tiempoGiroActual = 1500;//1700;
  
  else if (carriles.esComandoC20 && carriles.carrilObjetivo == 2) tiempoGiroActual = 800;//1800;

  else if (carriles.esComandoC20 && carriles.carrilObjetivo == 1) tiempoGiroActual = 1200;//1800;

  else if (carriles.esComandoC14 && carriles.carrilObjetivo == 1) tiempoGiroActual = 2000;//2000;

  else if (carriles.esComandoC15 && carriles.carrilObjetivo == 1) tiempoGiroActual = 800;//1900;

  else if (carriles.esComandoC15 && carriles.carrilObjetivo == 2) tiempoGiroActual = 1200;//1900;

  //C33 (Rojo, Adelante Izquierda y Verde, Atrás Izquierda)
  //Primer giro
  else if (carriles.esComandoC33 && carriles.carrilObjetivo == 1) tiempoGiroActual = 1100;//1550;
  // retorno
  else if (carriles.esComandoC33 && carriles.carrilObjetivo == 2) tiempoGiroActual = 1500;//2200;
  //C26 (Verde, Adelante Derecha y Rojo, Atrás Derecha)
  // Primer giro verde izquierda
  else if (carriles.esComandoC26 && carriles.carrilObjetivo == 2) tiempoGiroActual = 1100;//1800;
  // retorno rojo izquierda
  else if (carriles.esComandoC26 && carriles.carrilObjetivo == 1) tiempoGiroActual = 1500;//1800;
  //C27 (Rojo, Adelante Derecha y Verde, Atrás Derecha)
  else if (carriles.esComandoC27 && carriles.carrilObjetivo == 1) tiempoGiroActual = 1400;//1600;
  else if (carriles.esComandoC27 && carriles.carrilObjetivo == 2) tiempoGiroActual = 800;//1800;
  //C32 (Verde, Adelante Izquierda y Rojo, Atrás Izquierda
  else if (carriles.esComandoC32 && carriles.carrilObjetivo == 1) tiempoGiroActual = 1500;//1600;

  else if (carriles.esComandoC32 && carriles.carrilObjetivo == 2) tiempoGiroActual = 1300;//1800;

  else if (carriles.esComandoC08 && carriles.carrilObjetivo == 1) tiempoGiroActual = 1500;//2000;
  
  else if (carriles.esComandoC36 && carriles.carrilObjetivo == 1) tiempoGiroActual = 1500;//2000;
  
  else if (carriles.carrilObjetivo == 1)  {
    tiempoGiroActual = carriles.tiempoGiroCarril1;
    Serial.println(" TIEMPO ARBITRARIO CARRIL 1");

    }

  else if (carriles.carrilObjetivo == 2){ 
   tiempoGiroActual = carriles.tiempoGiroCarril2;
    Serial.println(" TIEMPO ARBITRARIO CARRIL 2");

    }
  else{ 
    tiempoGiroActual = carriles.tiempoGiro;
    Serial.println(" TIEMPO ARBITRARIO CARRIL GIRO");
}
  // LÓGICA DE INTERRUPCIÓN 
  if (!sharpInterrupcionActiva && !lineaInterrupcionActiva) {
    float distanciaFrontalGiro = leerToF('F');

    float distancia_interrupcion_frontal = 38.0; // Valor por defecto para todos los comandos
    
    if (carriles.esComandoC26 && carriles.carrilObjetivo == 1) {
      Serial.println(" cambiado por 30cm");
      distancia_interrupcion_frontal = 30.0;
    }
    if (distanciaFrontalGiro <= distancia_interrupcion_frontal && distanciaFrontalGiro > 0) {
      Serial.println(" INTERRUPCIÓN POR TOF FRONTAL! (" + String(distanciaFrontalGiro, 1) + "cm). Saltando a la fase de corrección AHORA.");
      sharpInterrupcionActiva = true; 
      tiempoInicioInterrupcion = millis();
    }
    else {
      //miestras esta retornando al angulo objtivo no me detecte colores por un tiempo especifico
        if (millis() - tiempoUltimaDeteccion > tiempoEsperaDeteccion) {
            bool lineaDetectada = false;
            if (sentidoVehiculo == IZQUIERDA) {
                if (confirmarColor(HUE_AZUL_MIN, HUE_AZUL_MAX, SATURACION_AZUL_MIN, VALOR_AZUL_MIN, VALOR_AZUL_MAX, CONFIRMAR_AZUL_TOLERANCIA, CONFIRMAR_AZUL_DELAY)) {
                    lineaDetectada = true;
                    Serial.println(" INTERRUPCIÓN POR LÍNEA AZUL! Saltando a la fase de corrección AHORA.");
                }
            }
            else if (sentidoVehiculo == DERECHA) {
                if (confirmarColor(HUE_NARANJA_MIN, HUE_NARANJA_MAX, SATURACION_NARANJA_MIN, VALOR_NARANJA_MIN, VALOR_NARANJA_MAX, CONFIRMAR_NARANJA_TOLERANCIA, CONFIRMAR_NARANJA_DELAY)) {
                    lineaDetectada = true;
                    Serial.println(" INTERRUPCIÓN POR LÍNEA NARANJA! Saltando a la fase de corrección AHORA.");
                }
            }
            if (lineaDetectada) {
                lineaInterrupcionActiva = true;
                tiempoInicioInterrupcion = millis();
            }
        }
      }
  }

  // ==================== ====================
  // FASE DE CORRECCIÓN Y ENDEREZADO (Lógica Unificada)
  if (tiempoEnGiro >= tiempoGiroActual || sharpInterrupcionActiva || lineaInterrupcionActiva) { 
    // ── SEGURO: Si la desviación respecto al objetivo es pequeña, no hace falta contragiro ──
    if (!primerGiroRealizado) {
        anguloObjetivo = 0.0;
    } else {
        calcularAnguloObjetivoMantener();
    }

    const float UMBRAL_CONTRAGIRO = 8.0; // Si está a menos de 12° del objetivo, va directo
    if (estaEnRangoAngulo(anguloZ, anguloObjetivo, UMBRAL_CONTRAGIRO)) {
        Serial.println("✅ Desviación pequeña, sin contragiro. Directo a MANTENIENDO_CARRIL.");
        controlLateral.ladoControl = obtenerLadoControlPorCarril();
        cambiarEstado(MANTENIENDO_CARRIL);
        return 90.0;
    }
    // ─────────────────────────────────────────────────────────────────────────────────────

   // ── VERIFICACIÓN SHARP DENTRO DE LA FASE DE CORRECCIÓN ───────────────
    static unsigned long ultimoRetrocesoSharpCorreccion = 0;
    const unsigned long COOLDOWN_SHARP_CORRECCION = 400; // ms de pausa entre retrocesos

    float distSharpCorreccion = 999;
    if (carriles.carrilObjetivo == 1) {
      distSharpCorreccion = leerSensorSharp(SHARP_DIAGONAL_DER_PIN);
    } else {
      distSharpCorreccion = leerSensorSharp(SHARP_DIAGONAL_IZQ_PIN);
    }

    float distanciaLimite = 14.0; // Valor por defecto
    if (carriles.esComandoC20) {
      distanciaLimite = 14.0;
    } else if (carriles.esComandoC21) {
      distanciaLimite = 14.0;
    } else if (carriles.esComandoC14) {
      distanciaLimite = 14.0;
    }

    if (distSharpCorreccion <= distanciaLimite && distSharpCorreccion > 0 &&
        millis() - ultimoRetrocesoSharpCorreccion > COOLDOWN_SHARP_CORRECCION) {

      Serial.println("⚠️ SHARP en CORRECCIÓN: " + String(distSharpCorreccion, 1) + "cm. Retrocediendo...");

      detenerMotor();
      direccion.write(90);
      delay(200);

      retrocederMotor();
      unsigned long t0 = millis();
      unsigned long duracion = (carriles.esComandoC21 || carriles.esComandoC14) ? 800 : 800;
      while (millis() - t0 < duracion) {
        actualizarGiro();
        delay(10);
      }
      detenerMotor();
      delay(100);

      if (carriles.direccionGiro < 0) {
        direccion.write(180);
      } else {
        direccion.write(0);
      }
      delay(300);

      avanzarMotor();
      ultimoRetrocesoSharpCorreccion = millis(); // marca el tiempo del último retroceso
      Serial.println("▶️ SHARP CORRECCIÓN: Reanudando enderezado.");
    }
    // ─────────────────────────────────────────────────────────────────────
   // Manejo de la interrupción por ToF Frontal → contragiro directo sin detener
    if (sharpInterrupcionActiva && !pausaPorSharpRealizada) {
      Serial.println("↩️ TOF FRONTAL: Iniciando contragiro directo...");

      // Detener el motor, girar el servo y esperar a que llegue antes de avanzar
      detenerMotor();

      if (carriles.carrilObjetivo == 1) {
        direccion.write(180); // Giro inicial fue derecha, contragiro es izquierda
      } else {
        direccion.write(0);   // Giro inicial fue izquierda, contragiro es derecha
      }

      delay(300); // Pausa para que el servo alcance su posición antes de avanzar

      avanzarMotor(); // Reanudar avance ya con el servo en posición

      pausaPorSharpRealizada = true;
      tiempoInicioInterrupcion = millis();
      Serial.println("▶️ Contragiro activo. FASE A controlará por ángulo.");
    }

    // 1. Calcular el ángulo objetivo final
    // Protección: durante los primeros 400ms tras la interrupción forzar el contragiro
    // para evitar que un ángulo casual mande a MANTENIENDO antes de cruzar.
    if (sharpInterrupcionActiva && (millis() - tiempoInicioInterrupcion < 400)) {
      return (carriles.carrilObjetivo == 1) ? 180.0 : 0.0;
    }

    if (!primerGiroRealizado) {
        anguloObjetivo = 0.0;
        if (!mensajeFase2Mostrado) {
            Serial.println(">>> PRIMER GIRO: Ángulo de enderezado forzado a 0.0°");
        }
    } else {
        calcularAnguloObjetivoMantener();
    }

    // 2. Verificar si ya hemos llegado al ángulo objetivo
    if (estaEnRangoAngulo(anguloZ, anguloObjetivo, 8.0)) {
      Serial.println(" Ángulo corregido. Pasando a mantener carril.");
      enFaseContragiro = false;
      controlLateral.ladoControl = obtenerLadoControlPorCarril();
      cambiarEstado(MANTENIENDO_CARRIL);
      return 90.0;
      // ── NUEVO: Detectar si se sobrepasó el ángulo objetivo ──────────────
      // En el contragiro: carril 1 gira a izquierda (anguloZ sube), carril 2 gira a derecha (anguloZ baja)
      float errorContragiro = calcularDiferenciaAngular(anguloZ, anguloObjetivo);
      bool sobrepasado = false;
      if (carriles.carrilObjetivo == 1 && errorContragiro < -5.0) {
          sobrepasado = true; // Iba subiendo y ya pasó por arriba del objetivo
      } else if (carriles.carrilObjetivo == 2 && errorContragiro > 5.0) {
          sobrepasado = true; // Iba bajando y ya pasó por debajo del objetivo
      }

      if (sobrepasado) {
          Serial.println("⚠️ Contragiro sobrepasado. Forzando MANTENIENDO_CARRIL.");
          cambiarEstado(MANTENIENDO_CARRIL);
          return 90.0;
      }
    } else {
        if (!mensajeFase2Mostrado) {
            Serial.println(" Fase 2: Ejecutando contra-giro hasta enderezar.");
            mensajeFase2Mostrado = true;
        }
        enFaseContragiro = true; // Activar velocidad lenta en el loop
        if (carriles.carrilObjetivo == 1) {
            return 180.0;
        } else {
            return 0.0;
        }
    }
  }
  // ===================== ======================

  // FASE B: GIRO INICIAL 
  else {
    if (!anguloCalculado) {
      if (!mensajeFase1Mostrado) {
        Serial.println("🔄 Fase 1: Ejecutando giro inicial por tiempo (cálculo único).");
        mensajeFase1Mostrado = true;
      }
    
      float servoAngleTemporal = 90.0;
 
      if (carriles.esComandoC15) {
        if (carriles.carrilObjetivo == 1) {
          // Para el primer tramo del C15 (hacia carril 1), el giro es a 40 grados.
          servoAngleTemporal = 30.0;//50
          Serial.println("   -> Comando C15 (Fase 1/2): Ángulo de giro FIJO a 40°.");
        } else { // carriles.carrilObjetivo == 2
          // Para el segundo tramo (hacia carril 2), el giro es a 140 grados.
          servoAngleTemporal = 120.0;//160
          Serial.println("   -> Comando C15 (Fase 2/2): Ángulo de giro FIJO a 140°.");
        }
      } else if (carriles.esComandoC20) { 
        if (carriles.carrilObjetivo == 2) {
          // Para el primer tramo del C20 (hacia carril 2, izq.), el giro es a 140 grados.
          servoAngleTemporal = 140.0;//140
          Serial.println("   -> Comando C20 (Fase 1/2): Ángulo de giro FIJO a 140°.");
        } else { // carriles.carrilObjetivo == 1
          // Para el segundo tramo (hacia carril 1, der.), el giro es a 40 grados.
          servoAngleTemporal = 0.0;//40
          Serial.println("   -> Comando C20 (Fase 2/2): Ángulo de giro FIJO a 40°.");
        }
      }   else if (carriles.esComandoC26) {
        if (carriles.carrilObjetivo == 2) {
          // Para el primer tramo del C26 (hacia carril 2, izq.), el giro es a 140 grados.
          servoAngleTemporal = 140.0;
          Serial.println("   -> Comando C26 (Fase 1/2): Ángulo de giro FIJO a 140°.");
        } else { // carriles.carrilObjetivo == 1
          // Para el segundo tramo (hacia carril 1, der.), el giro es a 20 grados.
          servoAngleTemporal = 0.0;
          Serial.println("   -> Comando C26 (Fase 2/2): Ángulo de giro FIJO a 0°.");
         }
       } else if (carriles.esComandoC33) {
        if (carriles.carrilObjetivo == 1) {
          // Para el primer tramo del C33 (hacia carril 1, dere.), el giro es a 40 grados.
          servoAngleTemporal = 30.0;
          Serial.println("   -> C33 Comando (Phase 1/2): Ángulo de giro FIJO a 30°");
        } else { // carriles.carrilObjetivo == 2
          //Para el primer tramo del C33 (hacia carril 2, izq.), el giro es a 160 grados.
          servoAngleTemporal = 180.0;
          Serial.println("   -> C33 Command (Phase 2/2): Ángulo de giro FIJO a 180°.");
          }
        }
       else if (carriles.esComandoC21) {
        Serial.println("   -> Comando C21: Aplicando ángulo de giro reducido.");
        if (carriles.direccionGiro < 0) { // Giro a la IZQUIERDA
          servoAngleTemporal = 180.0; // Ángulo solicitado para la izquierda
          Serial.println("       Giro a la IZQUIERDA a 180°.");
        } else { // Giro a la DERECHA
          servoAngleTemporal = 0.0; // Ángulo solicitado para la derecha
          Serial.println("       Giro a la DERECHA a 0°.");
        }
      }  else if (carriles.esComandoC14) {
        Serial.println("   -> Comando C14: Aplicando ángulo de giro reducido.");
        if (carriles.direccionGiro < 0) { // Giro a la IZQUIERDA
          servoAngleTemporal = 170.0; // Ángulo solicitado para la izquierda
          Serial.println("       Giro a la IZQUIERDA a 170°.");
        } else { // Giro a la DERECHA
          servoAngleTemporal = 0.0; // Ángulo solicitado para la derecha
          Serial.println("       Giro a la DERECHA a °0.");
        }
      } else if (carriles.esComandoC27) {
        if (carriles.carrilObjetivo == 1) {
          // Para el primer tramo del C27 (hacia carril 1, dere.), el giro es a 20 grados.
          servoAngleTemporal = 0.0;
          Serial.println("   -> Comando C27 (Fase 1/2): Ángulo de giro FIJO a 0°.");
        } else { // carriles.carrilObjetivo == 2
          // Para el segundo tramo (hacia carril 2, der.), el giro es a 160 grados.
          servoAngleTemporal = 160.0;
          Serial.println("   -> Comando C27 (Fase 2/2): Ángulo de giro FIJO a 160°.");
        } 
      } else if (carriles.esComandoC32) {
        if (carriles.carrilObjetivo == 2) {
          // Para el primer tramo del C32 (hacia carril 2, izq.), el giro es a 160 grados.
          servoAngleTemporal = 170.0;
          Serial.println("   -> Comando C32 (Fase 1/2): Ángulo de giro FIJO a 170°.");
        } else { // carriles.carrilObjetivo == 1
          // Para el segundo tramo (hacia carril 1, der.), el giro es a 20 grados.
          servoAngleTemporal = 0.0;
          Serial.println("   -> Comando C32 (Fase 2/2): Ángulo de giro FIJO a 0°.");
         }
       }
       else if (carriles.esComandoC02) {
        if (carriles.carrilObjetivo == 1) {
          // rojo derecha
          servoAngleTemporal = 0.0;
          Serial.println("   -> C02 Comando : Ángulo de giro FIJO a 0°");
          }
        }
        else if (carriles.esComandoC08 ||carriles.esComandoC36 ) {
        if (carriles.carrilObjetivo == 1) {
          // rojo izquierda
          servoAngleTemporal = 20.0;
          Serial.println("   -> C08 Comando : Ángulo de giro FIJO a 20°");
          } 
        }
        else if (carriles.esComandoC01) {
        if (carriles.carrilObjetivo == 2) {
          // rojo izquierda
          servoAngleTemporal = 170.0;
          Serial.println("   -> C01 Comando : Ángulo de giro FIJO a 170°");
          } 
        }else if (carriles.esComandoC07) {
        if (carriles.carrilObjetivo == 2) {
          // rojo izquierda
          servoAngleTemporal = 170.0;
          Serial.println("   -> C07 Comando : Ángulo de giro FIJO a 170°");
          } 
        }
      else { // Lógica original para todos los demás comandos
          bool calcularAnguloVariable = (carriles.carrilObjetivo == 1 && sentidoVehiculo == DERECHA) ||
                                        (carriles.carrilObjetivo == 2 && sentidoVehiculo == IZQUIERDA); 
          
          if (calcularAnguloVariable) {
              Serial.println("--- [Ángulo Variable Activo] ---");
              float distanciaLateral = -1.0; 
              if (carriles.carrilObjetivo == 1) {
                  distanciaLateral = leerToF('I');
                  Serial.print("Carril Objetivo 1: Leyendo ToF Izquierdo -> Distancia: ");
                  Serial.println(distanciaLateral);
              } else {
                  distanciaLateral = leerToF('D'); 
                  Serial.print("Carril Objetivo 2: Leyendo ToF Derecho -> Distancia: ");
                  Serial.println(distanciaLateral);
              }
              
              if (distanciaLateral > 0 && distanciaLateral < 100) {
                  float distanciaRestringida = constrain(distanciaLateral, 32.0, 50.0);
                  float ajuste = map(distanciaRestringida, 32.0, 50.0, 0.0, 20.0); 
                  
                  Serial.print("Distancia acotada (32-50): ");
                  Serial.print(distanciaRestringida);
                  Serial.print(" | Ajuste calculado por map: ");
                  Serial.println(ajuste);

                  if (carriles.direccionGiro < 0) { 
                      servoAngleTemporal = 170.0 - ajuste; 
                      Serial.print("Giro Izquierda. Servo Temporal (170 - ajuste): ");
                      Serial.println(servoAngleTemporal);
                  } else { 
                      servoAngleTemporal = 10.0 + ajuste; 
                      Serial.print("Giro Derecha. Servo Temporal (10 + ajuste): ");
                      Serial.println(servoAngleTemporal);
                  }
              } else {
                  Serial.print("Distancia ToF fuera de rango (0-100). Valor obtenido: ");
                  Serial.println(distanciaLateral);
                  
                  if (carriles.direccionGiro < 0) {
                      servoAngleTemporal = 170.0;
                      Serial.println("Aplicando giro fijo Izquierda: 170.0");
                  } else {
                      servoAngleTemporal = 10.0;
                      Serial.println("Aplicando giro fijo Derecha: 10.0");
                  }
              }
          } else {
              Serial.println("--- [Giro Fijo Activo] (No cumple condiciones de carril/sentido) ---");
              if (carriles.direccionGiro < 0) {
                  servoAngleTemporal = 170.0;
                  Serial.println("Giro Fijo Izquierda: 170.0");
              } else {
                  servoAngleTemporal = 10.0;
                  Serial.println("Giro Fijo Derecha: 10.0");
              }
          }
      }

      anguloGiroFijo = constrain(servoAngleTemporal, 0, 180);
      anguloCalculado = true;
      
      Serial.print(">>> ANGULO FINAL ENVIADO AL SERVO: ");
      Serial.println(anguloGiroFijo);
      Serial.println("----------------------------------------");

      // ── NUEVO: Girar el servo ANTES de arrancar el motor ──────────────
      // Se escribe el ángulo aquí mismo y se espera a que el servo llegue
      // a su posición antes de que el loop principal active avanzarMotor().
      direccion.write((int)anguloGiroFijo);
      delay(400); // 300ms le da tiempo al servo de posicionarse. Puedes ajustar entre 200-500ms.
      // ──────────────────────────────────────────────────────────────────
    }
    
   // ── CONTROL DE DESVIACIÓN MÁXIMA DE 45° ─────────────────────────────
    // Calcular cuánto nos hemos desviado desde que inició el giro
    float desviacionActual = 0.0;
    if (carriles.carrilObjetivo == 1) {
      // Giro a la derecha: el ángulo Z baja (se hace negativo o sube hacia 360)
      desviacionActual = calcularDiferenciaAngular(anguloZ, anguloObjetivoGlobal);
      desviacionActual = abs(desviacionActual);
    } else {
      // Giro a la izquierda: el ángulo Z sube
      desviacionActual = calcularDiferenciaAngular(anguloZ, anguloObjetivoGlobal);
      desviacionActual = abs(desviacionActual);
    }
    float MAX_DESVIACION_GIRO = 35.0;
     if (carriles.esComandoC20|| carriles.esComandoC15 ) { 
      MAX_DESVIACION_GIRO = 15.0;
     }else if(carriles.esComandoC26 && carriles.carrilObjetivo == 1 || carriles.esComandoC32 && carriles.carrilObjetivo == 1){
      MAX_DESVIACION_GIRO = 45.0;
     }else if(carriles.esComandoC26 && carriles.carrilObjetivo == 2){
      MAX_DESVIACION_GIRO = 21.0;
     }else if(carriles.esComandoC27 && carriles.carrilObjetivo == 2){
      MAX_DESVIACION_GIRO = 20.0;
     }else if(carriles.esComandoC14 && carriles.carrilObjetivo == 1){
      MAX_DESVIACION_GIRO = 70.0;
     }
    unsigned long ahoraA = millis();
    if (desviacionActual >= MAX_DESVIACION_GIRO) {
       if (ahoraA - ultimaImpresion_maxAngulo >= INTERVALO_maxAngulo) {
          ultimaImpresion_maxAngulo = ahoraA;  // Actualiza el tiempo
          Serial.println("*");
          Serial.print("regulacion de grados: ");
          Serial.print(MAX_DESVIACION_GIRO);
          Serial.println(" grados************");
        }
      // Alcanzó el máximo permitido: mantener ruedas centradas para
      // no desviarse más, esperar a que el tiempo termine y empiece el contragiro
      return 90.0;
    }
    // ────────────────────────────────────────────────────────────────────

    return anguloGiroFijo;
  }
}

float procesarEstadoEnderezandoCarril() {

  
  if (abs(anguloZ) <= carriles.toleranciaAngulo) {
    Serial.println("📐 Vehículo enderezado, iniciando mantenimiento de carril");
    cambiarEstado(MANTENIENDO_CARRIL);
    return 90.0;
  }
  
  // Control proporcional para enderezar
  float correccionAngulo = -anguloZ * 2.0;  // Ganancia proporcional
  correccionAngulo = constrain(correccionAngulo, -40, 50);
  
  return constrain(90.0 + correccionAngulo, 0, 180);
}
float obtenerDistanciaPorSentido(char lado) {
  float lectura;
  if (lado == 'I') {
    lectura = leerToF('I');
    return controlLateral.filtroIzquierda.filtrar(lectura);
  } else {
    lectura = leerToF('D');
    return controlLateral.filtroDerecha.filtrar(lectura);
  }
}
float procesarMantenimientoPared() {
      enFaseContragiro = false; // Resetear para que al entrar en manteniendo vaya a velicidad normal

  // ── 1. VERIFICACIÓN FRONTAL ────────────────────────────────────────────────
  float distanciaFrontal = leerToF('F');
  if (distanciaFrontal > 0 && distanciaFrontal < 15.0) {
    Serial.println("!!! OBSTÁCULO FRONTAL (" + String(distanciaFrontal, 1) + "cm) !!!");
    detenerMotor();
    direccion.write(SERVO_CENTER);
    delay(100);
    
    // Retroceder 
    Serial.println("⏪ Retrocediendo por obstáculo frontal...");
    retrocederMotor();
    unsigned long t0 = millis();
    while (millis() - t0 < 900) { actualizarGiro(); delay(10); }
    detenerMotor(); delay(100);

    // Evaluar distancia lateral post-retroceso
    // Carril 1 → sensor DERECHO | Carril 2 → sensor IZQUIERDO
    {
      const float UMBRAL_LEJOS_PARED = 1.0; // cm extra sobre el objetivo para considerar "lejos"
      float distLateral = 999.0;
      String ladoStr = "";
      if (carriles.carrilObjetivo == 1) {
        distLateral = leerToF('D');
        ladoStr = "DER";
      } else {
        distLateral = leerToF('I');
        ladoStr = "IZQ";
      }
      float distanciaObjetivoCarril = obtenerDistanciaObjetivo();

      Serial.print("📏 Post-retroceso | Sensor ");
      Serial.print(ladoStr);
      Serial.print(": ");
      Serial.print(distLateral, 1);
      Serial.print("cm | Objetivo: ");
      Serial.print(distanciaObjetivoCarril, 1);
      Serial.println("cm");

      bool estaLejos = (distLateral > (distanciaObjetivoCarril + UMBRAL_LEJOS_PARED)) ||
                       (distLateral <= 0 || distLateral > 150);

      if (estaLejos) {
        Serial.println("🔄 Está LEJOS de la pared. Iniciando giro evasivo hacia el carril...");
        cambiarEstado(GIRANDO_HACIA_CARRIL);
      } else {
        Serial.println("✅ Está CERCA de la pared. Solo retroceso fue suficiente. Volviendo a MANTENIENDO_CARRIL.");
        avanzarMotor();
        cambiarEstado(MANTENIENDO_CARRIL);
      }
    }
    return -1.0;
  }

  // ── 2. EMERGENCIA SHARP ────────────────────────────────────────────────────
  if (carriles.carrilObjetivo == 1) {
    float d = leerSensorSharp(SHARP_DIAGONAL_DER_PIN);
    if (d > 0 && d < DISTANCIA_EMERGENCIA_SHARP_PARED) {
      detenerMotor();
      direccion.write(SERVO_RIGHT); 
      delay(100);
      retrocederMotor();
      unsigned long t0 = millis();
      while (millis() - t0 < TIEMPO_RETROCESO_SHARP_PARED) { 
        actualizarGiro(); 
        delay(10);
       }
      detenerMotor(); 
      direccion.write(SERVO_CENTER); 
      avanzarMotor();
      return ANGULO_CORRECCION_SHARP_IZQ;
    }
  } else {
    float d = leerSensorSharp(SHARP_DIAGONAL_IZQ_PIN);
    if (d > 0 && d < DISTANCIA_EMERGENCIA_SHARP_PARED) {
      detenerMotor(); direccion.write(SERVO_LEFT); delay(100);
      retrocederMotor();
      unsigned long t0 = millis();
      while (millis() - t0 < TIEMPO_RETROCESO_SHARP_PARED) { actualizarGiro(); delay(10); }
      detenerMotor(); direccion.write(SERVO_CENTER); avanzarMotor();
      return ANGULO_CORRECCION_SHARP_DER;
    }
  }

  // ── 3. LECTURA SENSOR LATERAL ──────────────────────────────────────────────
  // Carril 1 → sensor DERECHO | Carril 2 → sensor IZQUIERDO
  char ladoControl = obtenerLadoControlPorCarril(); // 'D' o 'I'
  float distanciaControl = obtenerDistanciaPorSentido(ladoControl);
  if (distanciaControl <= 0 || distanciaControl > 150) return 90.0;

  // ── 4. ÁNGULO OBJETIVO ────────────────────────────────────────────────────
  if (!primerGiroRealizado) {
    anguloObjetivo = 0.0;
  } else {
    calcularAnguloObjetivoMantener();
  }
  controlLateral.anguloObjetivo = anguloObjetivo;

  // Error de ángulo normalizado [-180, 180]
  // positivo = Yaw < objetivo → robot apunta menos de lo que debería
  // negativo = Yaw > objetivo → robot apunta más de lo que debería
  float errorAngulo = anguloObjetivo - anguloZ;
  if (errorAngulo >  180.0) errorAngulo -= 360.0;
  if (errorAngulo < -180.0) errorAngulo += 360.0;

  // ── 5. ERROR DE DISTANCIA (PID) ────────────────────────────────────────────
  float distanciaObjetivo = obtenerDistanciaObjetivo();
  // positivo = demasiado lejos de la pared, negativo = demasiado cerca
  float errorDistancia = distanciaControl - distanciaObjetivo;

  controlLateral.integralDistancia += errorDistancia;
  controlLateral.integralDistancia = constrain(controlLateral.integralDistancia,
                                               -controlLateral.integralMaxDistancia,
                                               controlLateral.integralMaxDistancia);
  float derivadaDistancia = errorDistancia - controlLateral.errorDistanciaAnterior;
  controlLateral.errorDistanciaAnterior = errorDistancia;

  float correccionDistancia = (controlLateral.kpDistancia * errorDistancia)
                            + (controlLateral.kiDistancia * controlLateral.integralDistancia)
                            + (controlLateral.kdDistancia * derivadaDistancia);

  // ── 6. FUSIÓN Y SIGNO ─────────────────────────────────────────────────────
  //
  // CONVENCIÓN SERVO: servo > 90 → gira IZQUIERDA | servo < 90 → gira DERECHA
  //
  // CARRIL 1 (sensor DERECHO, pared a la derecha):
  //   errorAngulo > 0 → Yaw < obj → apunta muy a la derecha → corregir IZQUIERDA → servo+ ✓ directo
  //   errorAngulo < 0 → Yaw > obj → apunta muy a la izquierda → corregir DERECHA → servo- ✓ directo
  //   errorDistancia > 0 → muy lejos de pared derecha → acercarse → girar DERECHA → servo- → invertir
  //   errorDistancia < 0 → muy cerca de pared derecha → alejarse → girar IZQUIERDA → servo+ → invertir
  //
  // CARRIL 2 (sensor IZQUIERDO, pared a la izquierda):
  //   errorAngulo > 0 → mismo razonamiento → directo
  //   errorAngulo < 0 → mismo razonamiento → directo  
  //   errorDistancia > 0 → muy lejos de pared izquierda → acercarse → girar IZQUIERDA → servo+ → directo
  //   errorDistancia < 0 → muy cerca de pared izquierda → alejarse → girar DERECHA → servo- → directo
  //
  // Resumen: corrección de ángulo siempre DIRECTA.
  //          corrección de distancia: INVERTIDA para carril 1, DIRECTA para carril 2.

  const float KP_ANGULO = 2.5;
  const float UMBRAL_ANGULO_DOMINANTE = 15.0; // ° — por encima de esto el ángulo domina

  float pesoAngulo    = constrain(abs(errorAngulo) / UMBRAL_ANGULO_DOMINANTE, 0.0, 1.0);
  float pesoDistancia = 1.0 - pesoAngulo;

  float corrAngulo = KP_ANGULO * errorAngulo; // siempre directo

  float corrDistancia = correccionDistancia;
  if (ladoControl == 'D') {
    corrDistancia = -corrDistancia; // invertir para carril 1
  }
  // carril 2 (ladoControl == 'I'): directo

  float correccionTotal = (pesoAngulo * corrAngulo) + (pesoDistancia * corrDistancia);

  // Factores de asimetría mecánica
  if (correccionTotal > 0) correccionTotal *= controlLateral.factorGiroIzquierda;
  else                     correccionTotal *= controlLateral.factorGiroDerecha;

  correccionTotal = constrain(correccionTotal,
                              -controlLateral.correccionMaxima,
                              controlLateral.correccionMaxima);

  float valorServo = constrain(90.0 + correccionTotal, 0.0, 180.0);

  // ── 7. DEBUG ──────────────────────────────────────────────────────────────
  static unsigned long ultimoPrint = 0;
  if (millis() - ultimoPrint > 300) {
    Serial.print(F("ToF(")); Serial.print(ladoControl);
    Serial.print(F("):")); Serial.print(distanciaControl, 1);
    Serial.print(F(" ErrD:")); Serial.print(errorDistancia, 1);
    Serial.print(F(" ErrA:")); Serial.print(errorAngulo, 1);
    Serial.print(F(" wA:")); Serial.print(pesoAngulo, 2);
    Serial.print(F(" Srv:")); Serial.println(valorServo, 0);
    ultimoPrint = millis();
  }

  // ── 8. TRANSICIONES AUTOMÁTICAS DE COMANDO ───────────────────────────────
  if (carriles.esComandoC21 && carriles.carrilObjetivo == 1) {
    // Calcular cuánto tiempo llevamos en MANTENIENDO_CARRIL
    unsigned long tiempoEnManteniendo = millis() - carriles.tiempoInicioEstado;

    if (tiempoEnManteniendo >= TIEMPO_AVANCE_MANTENIENDO_C21) {
        // Ya avanzó suficiente, ahora sí proceder al cambio de carril
        Serial.println("⏱️ C21: Tiempo en MANTENIENDO_CARRIL cumplido (" + 
                         String(tiempoEnManteniendo) + "ms). Iniciando cambio a carril 2.");
        carriles.carrilObjetivo = 2;
        controlLateral.ladoControl = 'I';
        cambiarEstado(EVALUANDO_POSICION);
        return -1;
    } else {
        // Aún no ha pasado suficiente tiempo, seguir manteniendo
        Serial.print("⏳ C21: Avanzando en carril 1 antes del cambio... ");
        Serial.print(tiempoEnManteniendo);
        Serial.print("/");
        Serial.println(TIEMPO_AVANCE_MANTENIENDO_C21);
        // No retornar -1: dejar que el PID de pared siga controlando el servo
    }
}
  if (carriles.esComandoC15 && carriles.carrilObjetivo == 1) {
     // Calcular cuánto tiempo llevamos en MANTENIENDO_CARRIL
    unsigned long tiempoEnManteniendo = millis() - carriles.tiempoInicioEstado;

    if (tiempoEnManteniendo >= TIEMPO_AVANCE_MANTENIENDO_C15) {
        // Ya avanzó suficiente, ahora sí proceder al cambio de carril
        Serial.println("⏱️ C15: Tiempo en MANTENIENDO_CARRIL cumplido (" + 
                         String(tiempoEnManteniendo) + "ms). Iniciando cambio a carril 2.");
        carriles.carrilObjetivo = 2; controlLateral.ladoControl = 'I';
        cambiarEstado(EVALUANDO_POSICION); 
        return -1;
    } else {
        // Aún no ha pasado suficiente tiempo, seguir manteniendo
        Serial.print("⏳ C21: Avanzando en carril 1 antes del cambio... ");
        Serial.print(tiempoEnManteniendo);
        Serial.print("/");
        Serial.println(TIEMPO_AVANCE_MANTENIENDO_C15);
        // No retornar -1: dejar que el PID de pared siga controlando el servo
    }

  }
  if (carriles.esComandoC20 && carriles.carrilObjetivo == 2) {
    unsigned long tiempoEnManteniendo = millis() - carriles.tiempoInicioEstado;
    unsigned long tiempoEsperaC20 = TIEMPO_AVANCE_MANTENIENDO_C20 + (omitioGiro ? 1500UL : 0UL);

    if (tiempoEnManteniendo >= tiempoEsperaC20) {
        // Ya avanzó suficiente, ahora sí proceder al cambio de carril
        Serial.println("⏱️ C20: Tiempo en MANTENIENDO_CARRIL cumplido (" + 
                         String(tiempoEnManteniendo) + "ms). Iniciando cambio a carril 1.");
        carriles.carrilObjetivo = 1; controlLateral.ladoControl = 'D';
        carriles.distanciaCarril1 = 22.0;
        cambiarEstado(EVALUANDO_POSICION); 
        return -1;
    } else {
        // Aún no ha pasado suficiente tiempo, seguir manteniendo
        Serial.print("⏳ C20: Avanzando en carril 2 antes del cambio... ");
        Serial.print(tiempoEnManteniendo);
        Serial.print("/");
        Serial.println(tiempoEsperaC20);        // No retornar -1: dejar que el PID de pared siga controlando el servo
    }
  }
  if (carriles.esComandoC26 && carriles.carrilObjetivo == 2) {
    // Calcular cuánto tiempo llevamos en MANTENIENDO_CARRIL
    unsigned long tiempoEnManteniendo = millis() - carriles.tiempoInicioEstado;

    if (tiempoEnManteniendo >= TIEMPO_AVANCE_MANTENIENDO_C26) {
        // Ya avanzó suficiente, ahora sí proceder al cambio de carril
        Serial.println("⏱️ C25: Tiempo en MANTENIENDO_CARRIL cumplido (" + 
                         String(tiempoEnManteniendo) + "ms). Iniciando cambio a carril 1.");
        carriles.carrilObjetivo = 1; 
        controlLateral.ladoControl = 'D';
        carriles.distanciaCarril1 = 22.0;
        cambiarEstado(EVALUANDO_POSICION); 
        return -1;
    } else {
        // Aún no ha pasado suficiente tiempo, seguir manteniendo
        Serial.print("⏳ C26: Avanzando en carril 2 antes del cambio... ");
        Serial.print(tiempoEnManteniendo);
        Serial.print("/");
        Serial.println(TIEMPO_AVANCE_MANTENIENDO_C26);
        // No retornar -1: dejar que el PID de pared siga controlando el servo
    }
  }
  if (carriles.esComandoC32 && carriles.carrilObjetivo == 2) {
    // Calcular cuánto tiempo llevamos en MANTENIENDO_CARRIL
    unsigned long tiempoEnManteniendo = millis() - carriles.tiempoInicioEstado;

    if (tiempoEnManteniendo >= TIEMPO_AVANCE_MANTENIENDO_C32) {
        // Ya avanzó suficiente, ahora sí proceder al cambio de carril
        Serial.println("⏱️ C32: Tiempo en MANTENIENDO_CARRIL cumplido (" + 
                         String(tiempoEnManteniendo) + "ms). Iniciando cambio a carril 1.");
        carriles.carrilObjetivo = 1; 
        carriles.distanciaCarril1 = 25.0;
        controlLateral.ladoControl = 'D';
        cambiarEstado(EVALUANDO_POSICION); 
        return -1;
    } else {
        // Aún no ha pasado suficiente tiempo, seguir manteniendo
        Serial.print("⏳ C32: Avanzando en carril 2 antes del cambio... ");
        Serial.print(tiempoEnManteniendo);
        Serial.print("/");
        Serial.println(TIEMPO_AVANCE_MANTENIENDO_C32);
        // No retornar -1: dejar que el PID de pared siga controlando el servo
    }
  }
  if (carriles.esComandoC33 && carriles.carrilObjetivo == 1) {
       // Calcular cuánto tiempo llevamos en MANTENIENDO_CARRIL
    unsigned long tiempoEnManteniendo = millis() - carriles.tiempoInicioEstado;

    if (tiempoEnManteniendo >= TIEMPO_AVANCE_MANTENIENDO_C33) {
        // Ya avanzó suficiente, ahora sí proceder al cambio de carril
        Serial.println("⏱️ C33: Tiempo en MANTENIENDO_CARRIL cumplido (" + 
                         String(tiempoEnManteniendo) + "ms). Iniciando cambio a carril 1.");
        carriles.carrilObjetivo = 2; 
        carriles.distanciaCarril1 = 25.0;
        controlLateral.ladoControl = 'D';
        cambiarEstado(EVALUANDO_POSICION); 
        return -1;
    } else {
        // Aún no ha pasado suficiente tiempo, seguir manteniendo
        Serial.print("⏳ C33: Avanzando en carril 2 antes del cambio... ");
        Serial.print(tiempoEnManteniendo);
        Serial.print("/");
        Serial.println(TIEMPO_AVANCE_MANTENIENDO_C33);
        // No retornar -1: dejar que el PID de pared siga controlando el servo
    }
  }
  if (carriles.esComandoC14 && carriles.carrilObjetivo == 2) {
    // Calcular cuánto tiempo llevamos en MANTENIENDO_CARRIL
    unsigned long tiempoEnManteniendo = millis() - carriles.tiempoInicioEstado;

    if (tiempoEnManteniendo >= TIEMPO_AVANCE_MANTENIENDO_C14) {
        // Ya avanzó suficiente, ahora sí proceder al cambio de carril
        Serial.println("⏱️ C14: Tiempo en MANTENIENDO_CARRIL cumplido (" + 
                         String(tiempoEnManteniendo) + "ms). Iniciando cambio a carril 1.");
        carriles.carrilObjetivo = 1; controlLateral.ladoControl = 'D';
        cambiarEstado(EVALUANDO_POSICION); 
        return -1;
    } else {
        // Aún no ha pasado suficiente tiempo, seguir manteniendo
        Serial.print("⏳ C14: Avanzando en carril 2 antes del cambio... ");
        Serial.print(tiempoEnManteniendo);
        Serial.print("/");
        Serial.println(TIEMPO_AVANCE_MANTENIENDO_C14);
        // No retornar -1: dejar que el PID de pared siga controlando el servo
    }

  }
  if (carriles.esComandoC27 && carriles.carrilObjetivo == 1) {
    // Calcular cuánto tiempo llevamos en MANTENIENDO_CARRIL
    unsigned long tiempoEnManteniendo = millis() - carriles.tiempoInicioEstado;

    if (tiempoEnManteniendo >= TIEMPO_AVANCE_MANTENIENDO_C27) {
        // Ya avanzó suficiente, ahora sí proceder al cambio de carril
        Serial.println("⏱️ C27: Tiempo en MANTENIENDO_CARRIL cumplido (" + 
                         String(tiempoEnManteniendo) + "ms). Iniciando cambio a carril 2.");
        carriles.carrilObjetivo = 2;
        controlLateral.ladoControl = 'I';
        cambiarEstado(EVALUANDO_POSICION);
        return -1;
    } else {
        // Aún no ha pasado suficiente tiempo, seguir manteniendo
        Serial.print("⏳ C27: Avanzando en carril 1 antes del cambio... ");
        Serial.print(tiempoEnManteniendo);
        Serial.print("/");
        Serial.println(TIEMPO_AVANCE_MANTENIENDO_C27);
        // No retornar -1: dejar que el PID de pared siga controlando el servo
    }
  }

  return valorServo;
}
// ====================  FUNCIÓN DE CORRECCIÓN LATERAL POST-GIRO ====================
void ejecutarCorreccionDistanciaLateral() {
  if (!controlLateral_PostGiro.funcionActiva) {
    return;
  }

  unsigned long tiempoActual = millis();
  switch (controlLateral_PostGiro.estadoActual) {

    case ControlDistanciaLateral::IDLE: {

      // ── PAUSA DE ESTABILIZACIÓN: Detener y esperar 1s antes de leer ──
      // Se usa tiempoInicioEstado que ya se actualiza cada vez que
      // se entra a este estado, así el timer se resetea solo.
      detenerMotor();
      direccion.write(SERVO_CENTRADO);

      if (millis() - controlLateral_PostGiro.tiempoInicioEstado < 1000) {
        Serial.println("⏸️ Acomodo lateral: estabilizando 1s antes de medir...");
        break; // Salir y volver al loop, el motor ya está detenido
      }
      // ─────────────────────────────────────────────────────────────────

      float distanciaLateral;
      String ladoSensor;

      // 1. MEDIR LA DISTANCIA LATERAL
      if (sentidoVehiculo == DERECHA) {
        distanciaLateral = leerToF('I'); 
        ladoSensor = "izquierdo"; 
      } else {
        distanciaLateral = leerToF('D'); 
        ladoSensor = "derecho"; 
      }

      if (distanciaLateral == 999) break; 

      Serial.print("📏 CORRECCIÓN LATERAL - Distancia ");
      Serial.print(ladoSensor);
      Serial.print(": ");
      Serial.println(distanciaLateral, 1); 

      bool distanciaCorrecta = (distanciaLateral >= controlLateral_PostGiro.UMBRAL_MIN && distanciaLateral <= controlLateral_PostGiro.UMBRAL_MAX);

      if (!distanciaCorrecta) {
        // --- CASO A: LA DISTANCIA ES INCORRECTA ---
        // Se inicia el ciclo de corrección de distancia normal.
        float diferencia = 0.0;
        if (distanciaLateral < controlLateral_PostGiro.UMBRAL_MIN) {
          diferencia = controlLateral_PostGiro.UMBRAL_MIN - distanciaLateral; 
        } else if (distanciaLateral > controlLateral_PostGiro.UMBRAL_MAX) {
          diferencia = distanciaLateral - controlLateral_PostGiro.UMBRAL_MAX; 
        }

        if (diferencia >= 5.0) {
          controlLateral_PostGiro.tiempoCruceActual = 500; //800
          Serial.println("   -> Diferencia >= 6cm. Usando TIEMPO_CRUCE largo (700ms).");
        } else {
          controlLateral_PostGiro.tiempoCruceActual = 400; 
          Serial.println("   -> Diferencia < 6cm. Usando TIEMPO_CRUCE corto (450ms)."); 
        }
         // Si la diferencia es muy grande, activa la bandera para la maniobra de retroceso especial.
       /* if (diferencia > 16.0) {
          Serial.println("!!! Diferencia > 15cm. Se activará maniobra de retroceso orientado.");
          controlLateral_PostGiro.esCorreccionLarga = true;
        } else {
          controlLateral_PostGiro.esCorreccionLarga = false;
        }*/

        if (sentidoVehiculo == DERECHA) {
          if (distanciaLateral < controlLateral_PostGiro.UMBRAL_MIN) {
            Serial.println("⬇️ Muy cerca. Moviendo a la derecha."); 
            controlLateral_PostGiro.estadoActual = ControlDistanciaLateral::MOVIENDO_DERECHA;
            controlLateral_PostGiro.direccionManiobra = 'D';
            direccion.write(0);
          } else {
            Serial.println("⬆️ Muy lejos. Moviendo a la izquierda."); 
            controlLateral_PostGiro.estadoActual = ControlDistanciaLateral::MOVIENDO_IZQUIERDA;
            direccion.write(180);
            controlLateral_PostGiro.direccionManiobra = 'I';
          }
        } else { // Sentido IZQUIERDA
          if (distanciaLateral < controlLateral_PostGiro.UMBRAL_MIN) {
            Serial.println("⬇️ Muy cerca. Moviendo a la izquierda."); 
            controlLateral_PostGiro.estadoActual = ControlDistanciaLateral::MOVIENDO_IZQUIERDA;
            direccion.write(180);
            controlLateral_PostGiro.direccionManiobra = 'I';
          } else {
            Serial.println("⬆️ Muy lejos. Moviendo a la derecha."); 
            controlLateral_PostGiro.estadoActual = ControlDistanciaLateral::MOVIENDO_DERECHA;
            direccion.write(0);
            controlLateral_PostGiro.direccionManiobra = 'D';
          }
        }
        controlLateral_PostGiro.tiempoInicioEstado = tiempoActual;
        avanzarMotor();

      } else {
        // --- CASO B: LA DISTANCIA ES CORRECTA ---
        // Ahora realizamos la nueva verificación del ángulo.
        const float UMBRAL_ANGULO_FINAL = 5.0;
        float anguloObjetivoFinal = calcularAnguloRetroceso();
        float diferenciaAngular = calcularDiferenciaAngular(anguloZ, anguloObjetivoFinal);

        if (abs(diferenciaAngular) > UMBRAL_ANGULO_FINAL) {
          // El ángulo está desviado, se necesita una corrección corta.
          Serial.print("! Distancia OK, pero ángulo desviado > ");
          Serial.print(UMBRAL_ANGULO_FINAL);
          Serial.print("°. | Actual: ");
          Serial.print(anguloZ, 1);
          Serial.print("°, Objetivo: ");
          Serial.print(anguloObjetivoFinal, 1);
          Serial.println("°. Iniciando corrección corta.");

          // Usamos una duración de cruce corta y reiniciamos el ciclo de corrección
          controlLateral_PostGiro.tiempoCruceActual = 300; // 300ms de movimiento lateral

          // Si la diferencia es > 0, el robot apunta a la DERECHA del objetivo.
          // Nos movemos lateralmente a la IZQUIERDA para corregir.
          if (diferenciaAngular > 0) {
            controlLateral_PostGiro.estadoActual = ControlDistanciaLateral::MOVIENDO_IZQUIERDA;
            direccion.write(180); // Girar ruedas a la izquierda
          }
          // Si no, el robot apunta a la IZQUIERDA y nos movemos a la DERECHA.
          else {
            controlLateral_PostGiro.estadoActual = ControlDistanciaLateral::MOVIENDO_DERECHA;
            direccion.write(0); // Girar ruedas a la derecha
          }
          controlLateral_PostGiro.tiempoInicioEstado = tiempoActual;
          avanzarMotor();// Avanzar para iniciar la corrección

        } else {
          // ÉXITO: La distancia y el ángulo son correctos.
          Serial.println("✅ Distancia y ángulo correctos. Corrección finalizada.");
          controlLateral_PostGiro.funcionActiva = false;
          detenerMotor();
          direccion.write(SERVO_CENTRADO);
          cambiarEstado(ESPERANDO_COMANDO_CARRIL);
        }
      }
      break;
    }

    case ControlDistanciaLateral::MOVIENDO_DERECHA: {
      if (tiempoActual - controlLateral_PostGiro.tiempoInicioEstado >= controlLateral_PostGiro.tiempoCruceActual) {
        controlLateral_PostGiro.anguloObjetivo = calcularAnguloRetroceso(); 
        Serial.print("⏱️ Tiempo de cruce DERECHA finalizado. Corrigiendo ángulo hacia: "); 
        Serial.println(controlLateral_PostGiro.anguloObjetivo); 
        controlLateral_PostGiro.estadoActual = ControlDistanciaLateral::CORRIGIENDO_ANGULO_IZQUIERDA;
        direccion.write(170); 
      }
      break;
    }

    case ControlDistanciaLateral::MOVIENDO_IZQUIERDA: {
      if (tiempoActual - controlLateral_PostGiro.tiempoInicioEstado >= controlLateral_PostGiro.tiempoCruceActual) {
        controlLateral_PostGiro.anguloObjetivo = calcularAnguloRetroceso(); 
        Serial.print("⏱️ Tiempo de cruce IZQUIERDA finalizado. Corrigiendo ángulo hacia: "); 
        Serial.println(controlLateral_PostGiro.anguloObjetivo); 
        controlLateral_PostGiro.estadoActual = ControlDistanciaLateral::CORRIGIENDO_ANGULO_DERECHA;
        direccion.write(10); 
      }
      break;
    }

    case ControlDistanciaLateral::CORRIGIENDO_ANGULO_DERECHA:
    case ControlDistanciaLateral::CORRIGIENDO_ANGULO_IZQUIERDA: {
      avanzarLento(); // Velocidad reducida durante el enderezado de ángulo
      // ===== LÓGICA DE INTERRUPCIÓN Y CONTINUACIÓN =====
      float distanciaFrontal = leerToF('F');
      if (distanciaFrontal <= 13 && distanciaFrontal > 0) {
        Serial.println("! OBSTÁCULO FRONTAL durante contra-giro !");
        Serial.println("  -> Pausando, retrocediendo 1 seg y continuando...");

        // 1. Pausar el avance y enderezar las ruedas temporalmente
        detenerMotor();
        direccion.write(SERVO_CENTRADO);
        delay(100);

        // 2. Retroceder por 1 segundo mientras se actualiza el ángulo
        retrocederMotor();
        unsigned long tiempoInicioRetroceso = millis();
        while (millis() - tiempoInicioRetroceso < 700) {
          actualizarGiro();
          delay(10);
        }
        detenerMotor();
        delay(100);

        // 3. Reanudar el contra-giro exactamente como estaba
        if (controlLateral_PostGiro.estadoActual == ControlDistanciaLateral::CORRIGIENDO_ANGULO_DERECHA) {
            Serial.println("  -> Reanudando contra-giro a la DERECHA.");
            direccion.write(0); // Vuelve a girar las ruedas para la corrección
        } else { // Es CORRIGIENDO_ANGULO_IZQUIERDA
            Serial.println("  -> Reanudando contra-giro a la IZQUIERDA.");
            direccion.write(180); // Vuelve a girar las ruedas para la corrección
        }
        avanzarLento();// Reanuda el avance
      }
      // ==========
      float anguloActual = anguloZ; 
      float diferenciaAngular = abs(anguloActual - controlLateral_PostGiro.anguloObjetivo);
      if (diferenciaAngular > 180) {
        diferenciaAngular = 360 - diferenciaAngular; 
      }

      if (diferenciaAngular <= controlLateral_PostGiro.TOLERANCIA_ANGULAR) {
        Serial.println("🎯 Ángulo corregido. Iniciando retroceso."); 
        controlLateral_PostGiro.estadoActual = ControlDistanciaLateral::RETROCEDIENDO_FINAL;
        detenerMotor();
        delay(100);
         if (controlLateral_PostGiro.esCorreccionLarga) {
          Serial.println("🎯 Ángulo corregido. Iniciando RETROCESO ORIENTADO.");
          controlLateral_PostGiro.estadoActual = ControlDistanciaLateral::RETROCEDIENDO_ORIENTADO;
        } else {
          Serial.println("🎯 Ángulo corregido. Iniciando RETROCESO FINAL (recto).");
          controlLateral_PostGiro.estadoActual = ControlDistanciaLateral::RETROCEDIENDO_FINAL;
          retrocederMotor();
          direccion.write(SERVO_CENTRADO);
        }
        controlLateral_PostGiro.tiempoInicioEstado = millis();
      }
      break;
    }
    case ControlDistanciaLateral::RETROCEDIENDO_ORIENTADO: {
      // 1. Apuntar en la dirección original de la maniobra
      if (controlLateral_PostGiro.direccionManiobra == 'D') {
        direccion.write(0); // Misma dirección que MOVIENDO_DERECHA
      } else {
        direccion.write(180); // Misma dirección que MOVIENDO_IZQUIERDA
      }

      // 2. Retroceder por 500ms
      retrocederMotor();

      // 3. Después de 500ms, pasar a la fase de corrección en retroceso
      if (tiempoActual - controlLateral_PostGiro.tiempoInicioEstado >= 800) {
        Serial.println("⏱️ 500ms de retroceso orientado completados. Invirtiendo dirección.");
        controlLateral_PostGiro.estadoActual = ControlDistanciaLateral::CORRIGIENDO_POST_RETROCESO;
        controlLateral_PostGiro.anguloObjetivo = calcularAnguloRetroceso(); // Re-calcular por si acaso
      }
      break;
    }
    case ControlDistanciaLateral::CORRIGIENDO_POST_RETROCESO: {
      // 1. Apuntar en la dirección CONTRARIA (contra-giro)
      if (controlLateral_PostGiro.direccionManiobra == 'D') {
        direccion.write(180); // Contra-giro
      } else {
        direccion.write(0); // Contra-giro
      }

      // 2. Continuar retrocediendo y monitorear el ángulo
      retrocederMotor();
      float anguloActual = anguloZ;
      float diferenciaAngular = abs(anguloActual - controlLateral_PostGiro.anguloObjetivo);
      if (diferenciaAngular > 180) {
        diferenciaAngular = 360 - diferenciaAngular;
      }

      // 3. Cuando el ángulo sea correcto, enderezar ruedas y pasar al retroceso final
      if (diferenciaAngular <= controlLateral_PostGiro.TOLERANCIA_ANGULAR) {
        Serial.println("🎯 Ángulo enderezado en retroceso. Pasando a retroceso final.");
        direccion.write(SERVO_CENTRADO);
        controlLateral_PostGiro.estadoActual = ControlDistanciaLateral::RETROCEDIENDO_FINAL;
      }
      break;
    }
    case ControlDistanciaLateral::RETROCEDIENDO_FINAL: {
      retrocederMotor();
        direccion.write(SERVO_CENTRADO);
      float distanciaTrasera = leerToF('T'); 
      if (distanciaTrasera == 999) break;

      if (distanciaTrasera < controlLateral_PostGiro.DISTANCIA_TRASERA_OBJETIVO) {
        Serial.print("🏁 Retroceso finalizado. Distancia trasera:"); 
        Serial.println(distanciaTrasera); 
        Serial.print("🏁 Re-evaluando posición lateral y angular.");
        detenerMotor();
        controlLateral_PostGiro.estadoActual = ControlDistanciaLateral::IDLE;// Vuelve a evaluar
        controlLateral_PostGiro.tiempoInicioEstado = millis(); // Reset timer para nueva evaluación
        controlLateral_PostGiro.esCorreccionLarga = false;
      }
      break;
    }
  }
}
float calcularControlCarriles() {
  switch (estadoActual) { // Usa la variable global 'estadoActual'
    case ESPERANDO_COMANDO_CARRIL: // Usa el nombre correcto del enum
      return procesarEstadoEsperandoComando();
      
    case EVALUANDO_POSICION:
      return procesarEstadoEvaluandoPosicion();
      
    case GIRANDO_HACIA_CARRIL:
      return procesarEstadoGirandoHaciaCarril();
      
    case ENDEREZANDO_CARRIL:
      return procesarEstadoEnderezandoCarril();

    case MANTENIENDO_CARRIL: {
      float resultado = procesarMantenimientoPared();
      // Si es -1, significa que no se debe cambiar la dirección
      if (resultado == -1) {
        return -1;  // Propagar el valor especial
      }
      return resultado;
    }

      
    default:
      return 90.0;
  }
}

void procesarComandoCarril(String comando) {
  int carrilAnterior = carriles.carrilObjetivo;
  // VERDE se esquiva por la izquierda. y posicionado a la izquierda **apartir del 13 son dobles verdes
  if ( comando == "C07" || comando == "C09" || comando == "C13" || comando == "C37" ||  comando == "C19"|| comando == "C31") {
    carriles.esComandoC07 = true;
    carriles.carrilObjetivo = 2;
    carriles.distanciaCarril2 = 18.0; // Restablece la distancia al valor por defecto
    carriles.esComandoC21 = false;
    // Modo distancia solo para C07 con sentido IZQUIERDA (giro a carril 2)
      if (comando == "C07" || comando == "C09" || comando == "C13" || comando == "C37" && sentidoVehiculo == IZQUIERDA) {
        carriles.esComandoC07ModoDistancia = true;
        // Medir distancia derecha y calcular tiempo de giro proporcional
        float distTof = leerToF('D');
        // Clampear entre 30cm (máx tiempo) y 65cm (mín tiempo)
        float distClamped = constrain(distTof, 30.0, 65.0);
        // Interpolación lineal: 30cm → 1800ms, 65cm → 800ms
        //float tiempoCalculado = 1300.0 - ((distClamped - 30.0) / (65.0 - 30.0)) * (1300.0 - 500.0);

        float tiempoC01 = 1600.0 - ((distClamped - 30.0) / (65.0 - 30.0)) * (1600.0 - 600.0);
        carriles.tiempoGiroCarril2 = (unsigned long)tiempoC01;
        Serial.println("🎯 C01 Modo Distancia: ToF D=" + String(distTof, 1) +
                        "cm → Tiempo giro=" + String((unsigned long)tiempoC01) + "ms");
      } else {
        carriles.esComandoC07ModoDistancia = false;
      }
  
  //  Casaos Verde a la derecha. **apartir del 13 son dobles verdes **apartir del 25 son dobles verdes 
  } else if (comando == "C01" || comando == "C03"|| comando == "C05"|| comando == "C25") {
    carriles.esComandoC01 = true;
    carriles.carrilObjetivo = 2;
    carriles.distanciaCarril2 = 20.0;
    carriles.esComandoC21 = false;

    // Modo distancia solo para C01 con sentido IZQUIERDA (giro a carril 2)
    if (comando == "C01" || comando == "C03"|| comando == "C05"|| comando == "C25" && sentidoVehiculo == IZQUIERDA) {
      carriles.esComandoC01ModoDistancia = true;
      // Medir distancia derecha y calcular tiempo de giro proporcional
      float distTof = leerToF('D');
      // Clampear entre 30cm (máx tiempo) y 65cm (mín tiempo)
      float distClamped = constrain(distTof, 30.0, 65.0);
      // Interpolación lineal: 30cm → 1800ms, 65cm → 800ms
      float tiempoC01 = 1100.0 - ((distClamped - 30.0) / (65.0 - 30.0)) * (1100.0 - 300.0);
      carriles.tiempoGiroCarril2 = (unsigned long)tiempoC01;
      Serial.println("🎯 C01 Modo Distancia: ToF D=" + String(distTof, 1) +
                       "cm → Tiempo giro=" + String((unsigned long)tiempoC01) + "ms");
    } else {
      carriles.esComandoC01ModoDistancia = false;
    }

  // Comando especial C21 - primero va a carril 1, luego a carril 2
  } else if (comando == "C21") {
    carriles.carrilObjetivo = 1;    // Primero ir a carril 1
    carriles.esComandoC21 = true;   // Marcar como comando especial
    // ── NUEVO: Tiempo dinámico por distancia ──
    if (sentidoVehiculo == DERECHA) {
      float distTof = leerToF('I');
      float distClamped = constrain(distTof, 30.0, 65.0);
      float tiempoCalculado = 1500.0 - ((distClamped - 30.0) / (65.0 - 30.0)) * (1500.0 - 600.0);
      carriles.tiempoGiroCarril1 = (unsigned long)tiempoCalculado;
      Serial.println("🎯 C21 Modo Distancia: ToF D=" + String(distTof, 1) +
                       "cm → Tiempo giro=" + String((unsigned long)tiempoCalculado) + "ms");
    }
    Serial.println("🔄 Comando C21: A Carril 1, luego a Carril 2.");  // C27 Rojo, Adelante Derecha y Verde, Atrás Derecha
  }else if (comando == "C27") { 
    carriles.carrilObjetivo = 1;      // 1. El primer objetivo es el carril 1.
    carriles.esComandoC27 = true;      // 2. Activamos el flag especial para C27.
    // 3. Nos aseguramos de que los otros flags estén desactivados.
    carriles.esComandoC21 = false;
    carriles.esComandoC14 = false;
    carriles.esComandoC15 = false;
    carriles.esComandoC20 = false;
    // Tiempo dinámico por distancia ──
    if (sentidoVehiculo == DERECHA) {
      float distTof = leerToF('I');
      float distClamped = constrain(distTof, 30.0, 65.0);
      float tiempoCalculado = 1500.0 - ((distClamped - 30.0) / (65.0 - 30.0)) * (1500.0 - 600.0);
      carriles.tiempoGiroCarril1 = (unsigned long)tiempoCalculado;
      Serial.println("🎯 C27 Modo Distancia: ToF I=" + String(distTof, 1) +
                       "cm → Tiempo giro=" + String((unsigned long)tiempoCalculado) + "ms");
    }
    Serial.println("🔄 Comando C27: A Carril 1, luego a Carril 2.");
  // Comando especial C15 - primero va a carril 2, luego a carril 1
  } else if (comando == "C15") { 
    carriles.carrilObjetivo = 1;      // 1. El primer objetivo es el carril 1.
    carriles.esComandoC15 = true;     // 2. Activamos el flag especial para C15.
    carriles.esComandoC21 = false;    // 3. Nos aseguramos de que los otros flags estén desactivados.
    carriles.esComandoC14 = false;
    Serial.println("🔄 Comando C15: A Carril 1 (40°), luego a Carril 2 (140°).");
    // ── NUEVO: Tiempo dinámico por distancia ──
    if (sentidoVehiculo == DERECHA) {
      float distTof = leerToF('I');
      float distClamped = constrain(distTof, 30.0, 65.0);
      float tiempoCalculado = 800.0 - ((distClamped - 30.0) / (65.0 - 30.0)) * (800.0 - 300.0);
      carriles.tiempoGiroCarril1 = (unsigned long)tiempoCalculado;
      Serial.println("🎯 C15 Modo Distancia: ToF I=" + String(distTof, 1) +
                       "cm → Tiempo giro=" + String((unsigned long)tiempoCalculado) + "ms");
    }
  }  else if (comando == "C14") {
    carriles.carrilObjetivo = 2;    // Primero ir a carril 2
    carriles.esComandoC14 = true;   // Marcar como comando especial
    carriles.esComandoC21 = false;  // Resetear flag C21
    // ── NUEVO: Tiempo dinámico por distancia ──
    if (sentidoVehiculo == IZQUIERDA) {
      float distTof = leerToF('D');
      float distClamped = constrain(distTof, 30.0, 65.0);
      float tiempoCalculado = 1300.0 - ((distClamped - 30.0) / (65.0 - 30.0)) * (1300.0 - 500.0);
      carriles.tiempoGiroCarril2 = (unsigned long)tiempoCalculado;
      Serial.println("🎯 C14 Modo Distancia: ToF D=" + String(distTof, 1) +
                       "cm → Tiempo giro=" + String((unsigned long)tiempoCalculado) + "ms");
    }
    Serial.println("🔄 Comando C14: Primero carril 2, luego carril 1 automáticamente");
  
  // Comandos para carril 1  DERECHA COMANDOS PARA ROJO. rojos a la derecha.
  }else if (comando == "C32") {
    carriles.carrilObjetivo = 2;      // 1. The first target is lane 1.
    carriles.esComandoC32 = true;       // 2. Activate the special flag for C33.
    // 3. Make sure other special flags are disabled.
    carriles.esComandoC21 = false;
    carriles.esComandoC14 = false;
    carriles.esComandoC15 = false;
    carriles.esComandoC20 = false;
    carriles.esComandoC26 = false;
    if (sentidoVehiculo == IZQUIERDA) {
      float distTof = leerToF('D');
      float distClamped = constrain(distTof, 30.0, 65.0);
      float tiempoCalculado = 1300.0 - ((distClamped - 30.0) / (65.0 - 30.0)) * (1300.0 - 500.0);
      carriles.tiempoGiroCarril2 = (unsigned long)tiempoCalculado;
      Serial.println("🎯 C32 Modo Distancia: ToF D=" + String(distTof, 1) +
                       "cm → Tiempo giro=" + String((unsigned long)tiempoCalculado) + "ms");
    }
    Serial.println("🔄 Comando C32: A Carril 2 (160°), luego a Carril 1 (20°).");
  // Comandos para carril 1  DERECHA COMANDOS PARA ROJO. rojos a la derecha.
  }  else if (comando == "C20") { 
    carriles.carrilObjetivo = 2;      // 1. El primer objetivo es el carril 2.
    carriles.distanciaCarril2 = 35.0;
    carriles.esComandoC20 = true;     // 2. Activamos el flag especial para C20.
    carriles.esComandoC21 = false;    // 3. Nos aseguramos de que los otros flags estén desactivados.
    carriles.esComandoC14 = false;
    carriles.esComandoC15 = false;
    
    Serial.println("🔄 Comando C20: A Carril 2 (140°), luego a Carril 1 (40°).");
    // ── NUEVO: Tiempo dinámico por distancia ──
    if (sentidoVehiculo == IZQUIERDA) {
      float distTof = leerToF('D');  
      float distClamped = constrain(distTof, 30.0, 65.0);
      float tiempoCalculado = 800.0 - ((distClamped - 30.0) / (65.0 - 30.0)) * (800.0 - 300.0);
      carriles.tiempoGiroCarril1 = (unsigned long)tiempoCalculado;
      Serial.println("🎯 C20 Modo Distancia: ToF D=" + String(distTof, 1) +
                       "cm → Tiempo giro=" + String((unsigned long)tiempoCalculado) + "ms");
    }
  } else if (comando == "C26") { 
    carriles.carrilObjetivo = 2;      // 1. El primer objetivo es el carril 2.
    carriles.esComandoC26 = true;      // 2. Activamos el flag especial para C26.
    // 3. Nos aseguramos de que los otros flags estén desactivados.
    carriles.esComandoC21 = false;
    carriles.esComandoC14 = false;
    carriles.esComandoC15 = false;
    carriles.esComandoC20 = false;
    // ── NUEVO: Tiempo dinámico por distancia ──
    if (sentidoVehiculo == IZQUIERDA) {
      float distTof = leerToF('D');
      float distClamped = constrain(distTof, 30.0, 65.0);
      float tiempoCalculado = 1300.0 - ((distClamped - 30.0) / (65.0 - 30.0)) * (1300.0 - 400.0);
      carriles.tiempoGiroCarril1 = (unsigned long)tiempoCalculado;
      Serial.println("🎯 C26 Modo Distancia: ToF D=" + String(distTof, 1) +
                       "cm → Tiempo giro=" + String((unsigned long)tiempoCalculado) + "ms");
    }
    Serial.println("🔄 Comando C26: A Carril 2 (140°), luego a Carril 1 (20°).");

  } else if (comando == "C33") {
    carriles.carrilObjetivo = 1;      
    carriles.esComandoC33 = true;       
    carriles.esComandoC21 = false;
    carriles.esComandoC14 = false;
    carriles.esComandoC15 = false;
    carriles.esComandoC20 = false;
    carriles.esComandoC26 = false;
    Serial.println("🔄 Comando C33: To Lane 1 (20°), then to Lane 2 (140°).");
    // ── NUEVO: Tiempo dinámico por distancia ──
    if (sentidoVehiculo == DERECHA) {
      float distTof = leerToF('I');
      float distClamped = constrain(distTof, 30.0, 65.0);
      float tiempoCalculado = 800.0 - ((distClamped - 30.0) / (65.0 - 30.0)) * (800.0 - 300.0);
      carriles.tiempoGiroCarril1 = (unsigned long)tiempoCalculado;
      Serial.println("🎯 C33 Modo Distancia: ToF I=" + String(distTof, 1) +
                       "cm → Tiempo giro=" + String((unsigned long)tiempoCalculado) + "ms");
    }

  } else if (comando == "C02" || comando == "C04"  ||comando == "C06" ||comando == "C18"||comando == "C24"||comando == "C30") {
    carriles.carrilObjetivo = 1;
    carriles.esComandoC02 = true;
    carriles.distanciaCarril1 = 10.0; 
    carriles.esComandoC21 = false;  // Resetear flag 
      // Modo distancia solo para C07 con sentido IZQUIERDA (giro a carril 2)
      if (comando == "C02" || comando == "C04"  ||comando == "C06" ||comando == "C18"||comando == "C24"||comando == "C30" && sentidoVehiculo == DERECHA) {
        carriles.esComandoC02ModoDistancia = true;
        carriles.distanciaCarril2 = 38.0; 
        // Medir distancia derecha y calcular tiempo de giro proporcional
        float distTof = leerToF('I');
        // Clampear entre 30cm (máx tiempo) y 65cm (mín tiempo)
        float distClamped = constrain(distTof, 30.0, 65.0);
        // Interpolación lineal: 30cm → 1800ms, 65cm → 800ms
        float tiempoC01 = 1600.0 - ((distClamped - 30.0) / (65.0 - 30.0)) * (1500.0 - 600.0);
        carriles.tiempoGiroCarril2 = (unsigned long)tiempoC01;
        Serial.println("🎯 C01 Modo Distancia: ToF I=" + String(distTof, 1) +
                        "cm → Tiempo giro=" + String((unsigned long)tiempoC01) + "ms");
      } else {
        carriles.esComandoC02ModoDistancia = false;
      }
    
  //  Casaos Rojos a la izquierda. mayor distancia con la pared. 
  } else if (comando == "C08" || comando == "C10"||comando == "C38"||comando == "C36") {
    carriles.esComandoC08 = true;
    carriles.carrilObjetivo = 1;
    carriles.distanciaCarril1 = 20.0; // Cambia la distancia solo para este comando
    carriles.esComandoC21 = false;
      // Modo distancia solo para C01 con sentido IZQUIERDA (giro a carril 2)
    if (comando == "C08" || comando == "C10"||comando == "C38"||comando == "C36" && sentidoVehiculo == DERECHA) {
      carriles.esComandoC08ModoDistancia = true;
      // Medir distancia derecha y calcular tiempo de giro proporcional
      float distTof = leerToF('I');
      // Clampear entre 30cm (máx tiempo) y 65cm (mín tiempo)
      float distClamped = constrain(distTof, 30.0, 65.0);
      // Interpolación lineal: 30cm → 1800ms, 65cm → 800ms
      float tiempoC01 = 1100.0 - ((distClamped - 30.0) / (65.0 - 30.0)) * (1100.0 - 300.0);
      carriles.tiempoGiroCarril2 = (unsigned long)tiempoC01;
      Serial.println("🎯 C01 Modo Distancia: ToF D=" + String(distTof, 1) +
                       "cm → Tiempo giro=" + String((unsigned long)tiempoC01) + "ms");
    } else {
      carriles.esComandoC08ModoDistancia = false;
    }

  // Comando especial C21 - primero va a carril 1, luego a carril 2
  }  else {
    Serial.println(" Comando no reconocido: " + comando);
    return;
  }
   if (carriles.carrilObjetivo == 2) {
      carriles.direccionGiro = -1.0; // Valor negativo para giro a la IZQUIERDA
      Serial.println("   -> Dirección de giro establecida a: IZQUIERDA");
     //  Destello Verde por 400ms (No bloqueante)
      encenderLedTemporal(0, 255, 0, 400);
  } else if (carriles.carrilObjetivo == 1) {
      carriles.direccionGiro = 1.0; // Valor positivo para giro a la DERECHA
      Serial.println("   -> Dirección de giro establecida a: DERECHA");
     // Destello Rojo por 400ms (No bloqueante)
      encenderLedTemporal(255, 0, 0, 400);
  }
  // CAMBIO PRINCIPAL: Inicia el cooldown de detección de líneas INMEDIATAMENTE.
  // Esto evita que se detecte una línea mientras se evalúa la posición, antes de empezar el giro.
  tiempoUltimaDeteccion = millis();
  Serial.println("COOLDOWN DE DETECCIÓN DE LÍNEAS ACTIVADO INMEDIATAMENTE.");

  Serial.println(" Comando recibido: " + comando + " → Carril " + String(carriles.carrilObjetivo));
  
  // Si es el primer giro (y es uno de los comandos de inicio), salta directo a la acción
  if (!primerGiroRealizado && (comando == "C01" || comando == "C02" || comando == "C07" || comando == "C08")) {
    Serial.println(">>> PRIMER GIRO: Saltando evaluación, yendo directo a GIRANDO_HACIA_CARRIL.");
    cambiarEstado(GIRANDO_HACIA_CARRIL); 
  } 
  // Para cualquier otro caso, sigue la lógica normal
  else if (carrilAnterior != carriles.carrilObjetivo || estadoActual == ESPERANDO_COMANDO_CARRIL) { 
    cambiarEstado(EVALUANDO_POSICION); 
  }
}
void resetSistema(bool conservarAngulo = true) {
  // Reset de variables de control lateral
  controlLateral.integralDistancia = 0;
  controlLateral.errorDistanciaAnterior = 0;
  controlLateral.filtroIzquierda.inicializar();
  controlLateral.filtroDerecha.inicializar();

  // Reset de flags de comandos especiales
  carriles.esComandoC21 = false;
  carriles.esComandoC14 = false;

  // Reset de estado del sistema de carriles
  carriles.carrilObjetivo = 1;
  carriles.ladoControl = 'D';
  estadoActual = ESTADO_NORMAL; // Asigna a la variable global 'estadoActual'
  carriles.tiempoInicioEstado = millis();
  carriles.necesitaCambio = false;

  // Recentrar servo
 // direccion.write(90);

  Serial.println("🔄 Sistema reseteado" + String(conservarAngulo ? " (ángulo conservado)" : " (ángulo reiniciado)"));
}
// Función auxiliar para generar pulsos cortos (volumen bajo)
void buzzerVolumenBajo(int duracionTotal) {
  int tiempoTranscurrido = 0;
  while (tiempoTranscurrido < duracionTotal) {
    tone(BUZZER_PIN, 1500); // Enciende el tono
    delay(100); 
    noTone(BUZZER_PIN);     // Apaga el tono
    delay(20); 
    tiempoTranscurrido += 120;
  }
}

void esperarInterruptor() {
  ////ln(F("Esperando activacion del interruptor (LOW)..."));
  
  // LED parpadea lentamente mientras espera
  bool ledState = false;
  unsigned long ultimoParpadeo = 0;

  
  while (digitalRead(INTERRUPTOR_PIN) == HIGH) {  // Espera a que pase a LOW
    unsigned long ahora = millis();
    
    // Parpadeo lento del LED (cada 500ms)
    if (ahora - ultimoParpadeo > 500) {
      ledState = !ledState;
      colorVehiculo(255, 100, 0);
      ultimoParpadeo = ahora;
    }
    
    delay(10);
  }
  
  // Cuando el interruptor pasa a LOW, activar sistema
  apagarVehiculo(); // Apagar LED de espera
  delay(50); // Pequeña pausa para debounce
  iniciarSistema();
} 
  // Función mejorada para verificar el interruptor y controlar el sistema
void verificarInterruptor() {
  bool estadoActual = digitalRead(INTERRUPTOR_PIN);
  unsigned long tiempoActual = millis();
  
  // Debounce del interruptor
  if (estadoActual != interruptorAnterior) {
    tiempoDebounce = tiempoActual;
  }
  
  if ((tiempoActual - tiempoDebounce) > DEBOUNCE_DELAY) {
    if (estadoActual != interruptorAnterior) {
      // Cambio de estado confirmado
      if (estadoActual == LOW) {
        // Interruptor en LOW - INICIAR SISTEMA
        if (!sistemaActivo) {
          iniciarSistema();
        }
      } else {
        // Interruptor en HIGH - DETENER SISTEMA
        if (sistemaActivo) {
          detenerSistema();
        }
      }
      interruptorAnterior = estadoActual;
    }
  }
}
// LED indicador de sistema activo - 3 parpadeos rápidos con sonido
void indicadorSistemaActivo() {
  for (int i = 0; i < 3; i++) {
    colorVehiculo(255, 0, 200);
 //   buzzerVolumenBajo(100);  // 100ms con pulsos cortos
    apagarVehiculo();
    delay(100);
  }
}


// Función para iniciar el sistema completamente
void iniciarSistema() {
  // //ln(F("=== INICIANDO SISTEMA ==="));
  
  // Activar el sistema
  sistemaActivo = true;
  sistemaIniciado = true;
  resetYaw(); // Fija el 0° exactamente cuando arranca la carrera/ruta
  
  // Reinicializar todas las variables del sistema
  estadoActual = ESTADO_NORMAL;
  prioridadActual = PRIORIDAD_NORMAL;
  emergenciaActivada = false;
  retrocesoPorEmergencia = false;
  esquiveDiagonalActivo = false;
  giroEnProceso = false;
  enGiro = false;
  cruceEnProceso = false;
  retrocesoOrientadoActivo = false;
  
  // Reinicializar variables de detección
  colorDetectado = 'N';
  colorObjetivo = 'N';
  ultimoColorDetectado = 'N';
  sentidoVehiculo = INDEFINIDO;
  primeraDeteccion = true;
  lineaNaranjaDetectada = false;
  
  // Reinicializar contadores y timers
  contadorLineas = 0;
  tiempoEsquive = 0;
  tiempoUltimaDeteccion = millis();
  ultimoTiempoCentrado = millis();
  lastTime = millis();
  
  // Reinicializar PIDs
  pidGiro.error_anterior = 0;
  pidGiro.integral = 0;
  pidGiro.tiempo_anterior = 0;
  
  pidCentrado.error_anterior = 0;
  pidCentrado.integral = 0;
  pidCentrado.tiempo_anterior = 0;
  pidCentrado.inicializado = false;
  
 // Centrar servo
  direccion.write(SERVO_CENTRADO);
  
  // Inicializar variables para el arranque retrasado del motor
  tiempoInicioSistema = millis();
  motorArrancado = false;
  
  // Motor permanece apagado hasta que pase el delay
  detenerMotor();  // Motor apagado
  
  indicadorSistemaActivo();
  // //ln(F("Sistema iniciado - Motor arrancará en 1 segundo"));
}

void resetearRetrocesoEmergencia() {
  ////ln("RESET COMPLETO DEL SISTEMA DE RETROCESO");
  
  // Apagar motores de forma segura
  detenerMotor();           // Apagar motor principal temporalmente
  delay(100);
  apagarVehiculo();              // Apagar LED
  
  // Resetear TODAS las variables de estado
  retrocesoPorEmergencia = false;
  motorRetrocesoConfigurado = false;  // IMPORTANTE: Resetear flag
  prioridadActual = PRIORIDAD_NORMAL;

  estadoActual = ESTADO_NORMAL;

  tiempoInicioRetroceso = 0;
  tiempoUltimaVerificacionRetroceso = 0;
  
  // Centrar servo
  direccion.write(SERVO_CENTRADO);
  delay(100);
  
  // Reactivar motor principal
  avanzarMotor();
  
  ////ln("Reset completo finalizado");
}
// Función mejorada para detener el sistema completamente
void detenerSistema() {
  ////ln(F("=== DETENIENDO SISTEMA ==="));
  
  // Desactivar el sistema
  sistemaActivo = false;
  sistemaIniciado = false;
  
  // Detener todos los motores inmediatamente
  detenerMotor();  // HIGH para detener según tu configuración  
  // Centrar servo
  direccion.write(SERVO_CENTRADO);
  
  // Resetear sistema de retroceso y emergencias
  resetearRetrocesoEmergencia();
  retrocesoOrientadoActivo = false;
  emergenciaActivada = false;
  retrocesoPorEmergencia = false;
  esquiveDiagonalActivo = false;
  giroEnProceso = false;
  enGiro = false;
  cruceEnProceso = false;
  
  // Resetear estado a normal
  estadoActual = ESTADO_NORMAL;
  prioridadActual = PRIORIDAD_NORMAL;
  
  // Resetear variables de detección
  colorDetectado = 'N';
  colorObjetivo = 'N';
  ultimoColorDetectado = 'N';
  
  // LED parpadea rápidamente indicando sistema detenido, luego se apaga
  for (int i = 0; i < 6; i++) {
    colorVehiculo(0, 255, 0);
    delay(150);
    apagarVehiculo();
    delay(150);
  }
  apagarVehiculo(); // Asegurar que LED esté apagado
  
  Serial.println(F("Sistema detenido completamente"));
}
int DELAYESTA = 500;
// ==================== FUNCIÓN DE SALIDA DE ESTACIONAMIENTO ====================
void ejecutarSalidaEstacionamiento(bool salirHaciaIzquierda) {
  // Imprime en el monitor qué maniobra se va a ejecutar
  if (salirHaciaIzquierda) {
    Serial.println("INICIANDO MANIOBRA: Salida de Estacionamiento (Hacia la Izquierda)");
  } else {
    Serial.println("INICIANDO MANIOBRA: Salida de Estacionamiento (Hacia la Derecha - Invertida)");
  }
  
  // Pausa inicial para asegurar que todo esté estable
  delay(500);

  // Se definen las direcciones del servo usando un operador ternario.
  // Esto asigna el valor correcto a cada variable dependiendo de la dirección de salida elegida.
  int primerGiroAvance    = salirHaciaIzquierda ? SERVO_LEFT : SERVO_RIGHT;
  int primerGiroRetroceso = salirHaciaIzquierda ? SERVO_RIGHT : SERVO_LEFT;
  int segundoGiroLargo    = salirHaciaIzquierda ? SERVO_LEFT : SERVO_RIGHT;
  int giroEnderezado      = salirHaciaIzquierda ? SERVO_RIGHT : SERVO_LEFT;
  
  // --- PASOS 1 Y 2: Maniobra corta de vaivén (se repite dos veces) ---
  for (int i = 0; i < 2; i++) {
    Serial.println("Paso 1." + String(i + 1) + ": Avance corto en giro");
    direccion.write(primerGiroAvance);
    tiempoInicioPausa = millis();
    while (millis() - tiempoInicioPausa < DELAYESTA) {
      actualizarGiro(); // ¡Ahora el ángulo se actualiza durante la pausa!
      delay(5); // Pequeña pausa para no sobrecargar el procesador
    }
    avanzarLento(); // Avanza
    unsigned long tiempoInicioPaso1 = millis();
    while (millis() - tiempoInicioPaso1 < 275) {
      actualizarGiro(); // Sigue actualizando el ángulo del MPU6050
      delay(10);
        Serial.println("esta");

    }
    detenerMotor(); // Detiene avance
    tiempoInicioPausa = millis();
    while (millis() - tiempoInicioPausa < DELAYESTA) {
      actualizarGiro(); // ¡Ahora el ángulo se actualiza durante la pausa!
      delay(5); // Pequeña pausa para no sobrecargar el procesador
    }

    Serial.println("Paso 2." + String(i + 1) + ": Retroceso corto en giro");
    direccion.write(primerGiroRetroceso);
    tiempoInicioPausa = millis();
    while (millis() - tiempoInicioPausa < DELAYESTA) {
      actualizarGiro(); // ¡Ahora el ángulo se actualiza durante la pausa!
      delay(5); // Pequeña pausa para no sobrecargar el procesador
    }
    retrocederLento();// Retrocede
    unsigned long tiempoInicioPaso2 = millis();
    while (millis() - tiempoInicioPaso2 < 350) {
      actualizarGiro();
      delay(10);
       Serial.println("esta");
    }
    detenerMotor(); // Detiene retroceso
    tiempoInicioPausa = millis();
    while (millis() - tiempoInicioPausa < DELAYESTA) {
      actualizarGiro(); // ¡Ahora el ángulo se actualiza durante la pausa!
      delay(5); // Pequeña pausa para no sobrecargar el procesador
    }
  }

  // --- PASO 3: Avanzar en giro por un tiempo más largo para salir del cajón ---
  Serial.println("Paso 3: Avance largo en giro (700ms)");
  direccion.write(segundoGiroLargo);
  tiempoInicioPausa = millis();
    while (millis() - tiempoInicioPausa < DELAYESTA) {
      actualizarGiro(); // ¡Ahora el ángulo se actualiza durante la pausa!
      delay(5); // Pequeña pausa para no sobrecargar el procesador
    }

  avanzarLento();
  unsigned long tiempoInicioPaso3 = millis();
  while (millis() - tiempoInicioPaso3 < 900) {
    actualizarGiro();
    delay(10);
     Serial.println("esta");
  }
  detenerMotor();
  
  // --- PASO 4: Avanzar en el giro opuesto hasta que el vehículo esté recto (ángulo ~ 0) ---
  Serial.println("Paso 4: Enderezando hasta anguloZ = 0");
  direccion.write(giroEnderezado);
  tiempoInicioPausa = millis();
    while (millis() - tiempoInicioPausa < DELAYESTA) {
      actualizarGiro(); // ¡Ahora el ángulo se actualiza durante la pausa!
      delay(5); // Pequeña pausa para no sobrecargar el procesador
    }
  avanzarMotor();
  unsigned long tiempoInicioEnderezado = millis();
  // El bucle se detiene si el ángulo está cerca de 0 o si tarda demasiado (timeout de 4s)
  while (abs(anguloZ) > 1.0 && (millis() - tiempoInicioEnderezado < 4000)) {
    actualizarGiro();
    Serial.print("Enderezando... Angulo actual: ");
    Serial.println(anguloZ);
    delay(10);
     Serial.println("esta");
  }
  //delay(300);
  detenerMotor();

  Serial.println("Maniobra de salida de estacionamiento completada.");
  direccion.write(SERVO_CENTER); // Enderezar ruedas
  // --- PASO 5 (AÑADIDO): Retroceso final en línea recta ---
  Serial.println("Paso 5: Iniciando retroceso final de 1.5 segundos.");
  delay(200); // Pequeña pausa para asegurar que las ruedas estén centradas
  
  // Se define una duración variable para el retroceso final.
  unsigned long duracionRetrocesoFinal;
  if (salirHaciaIzquierda) {
    duracionRetrocesoFinal = 3500;// Duración original para la salida a la izquierda. 
    Serial.println("   -> Duracion de retroceso: " + String(duracionRetrocesoFinal) + "ms (Salida Izquierda)");
  } else {
    duracionRetrocesoFinal = 2000; // Duración más corta para la salida a la derecha.
    Serial.println("   -> Duracion de retroceso: " + String(duracionRetrocesoFinal) + "ms (Salida Derecha)");
  }
  retrocederMotor(); // Inicia el retroceso
  unsigned long tiempoInicioRetrocesoFinal = millis();
  while (millis() - tiempoInicioRetrocesoFinal < duracionRetrocesoFinal) {
      actualizarGiro(); // Es buena práctica seguir actualizando el ángulo 
      delay(10);
       Serial.println("esta");
  }
  Serial.println("Retroceso final completado. Motores detenidos.");

  // Detener todos los motores por seguridad
  detenerMotor();

// --- PASO 6: Esperar comando de inicio específico ---
  if (salirHaciaIzquierda) {
    Serial.println("Esperando comando para salida IZQUIERDA (C01 o C07)...");
  } else {
    Serial.println("Esperando comando para salida DERECHA (C02 o C08)...");
  }
  
  const unsigned long TIEMPO_ESPERA_COMANDO_MS = 5000; // 5 segundos de espera
  unsigned long tiempoInicioEspera = millis();
  bool comandoValidoRecibido = false;

  bool ledState = false;
  unsigned long ultimoParpadeo = 0;
  
  // El bucle se detiene si recibe un comando válido O si se acaba el tiempo
  while (millis() - tiempoInicioEspera < TIEMPO_ESPERA_COMANDO_MS) {
    // Parpadeo del LED para indicar que está esperando
    unsigned long ahora = millis();
    if (ahora - ultimoParpadeo > 500) {
      ledState = !ledState;
      colorVehiculo(255, 100, 0);
      ultimoParpadeo = ahora;
    }

    String comando = "";
    if (Serial.available() > 0) {
      comando = Serial.readStringUntil('\n');
    } else if (Serial.available() > 0) {
      comando = Serial.readStringUntil('\n');
    }

    if (comando != "") {
      comando.trim();
      comando.toUpperCase();

      bool comandoEsValido = false;
      // Lógica para validar el comando según la dirección de salida
      if (salirHaciaIzquierda) {
        // Si la salida fue a la izquierda, solo aceptar C01 o C07
        if (comando == "C01" || comando == "C07") {
          comandoEsValido = true;
        }
      } else {
        // Si la salida fue a la derecha, solo aceptar C02 o C08
        if (comando == "C02" || comando == "C08") {
          comandoEsValido = true;
        }
      }

      if (comandoEsValido) {
        Serial.println("Comando de inicio válido recibido: " + comando);
        procesarComandoCarril(comando);
        comandoValidoRecibido = true;
        break; // Sale del bucle de espera
      } else {
        // Informa que el comando no es válido para esta dirección de salida
        Serial.print("Comando '" + comando + "' inválido. ");
        if (salirHaciaIzquierda) {
          Serial.println("Solo se acepta C01 o C07.");
        } else {
          Serial.println("Solo se acepta C02 o C08.");
        }
      }
    }
    delay(50);
  }
 
  // Si al salir del bucle no se recibió un comando válido, el tiempo se agotó.
  if (!comandoValidoRecibido) {
    Serial.println("Tiempo de espera agotado. El sistema continuará en modo normal.");
    // No es necesario hacer nada extra, la función loop() se encargará de arrancar el motor
    // y continuar en ESTADO_NORMAL.
  }

  apagarVehiculo(); // Apaga el LED de espera al finalizar
}

float calcularPID(PIDController* pid, float setpoint, float medicion) {
  unsigned long tiempo_actual = millis();
  float dt = (tiempo_actual - pid->tiempo_anterior) / 1000.0;
  
  // Evitar divisiones por cero o deltas muy grandes
  if (dt <= 0 || dt > 0.1) {
    dt = 0.02; // 20ms por defecto
  }
  
  // Calcular error
  float error = setpoint - medicion;
  
  // Normalizar error angular (manejar cruce de 0°/360°)
  if (error > 180) error -= 360;
  if (error < -180) error += 360;
  
  // Término proporcional
  float proporcional = pid->kp * error;
  
  // Término integral con anti-windup
  pid->integral += error * dt;
  pid->integral = constrain(pid->integral, -pid->integral_max, pid->integral_max);
  float integral = pid->ki * pid->integral;
  
  // Término derivativo
  float derivativo = 0;
  if (pid->tiempo_anterior > 0) {
    derivativo = pid->kd * (error - pid->error_anterior) / dt;
  }
  
  // Salida PID total
  float salida = proporcional + integral + derivativo;
  
  // Guardar valores para próxima iteración
  pid->error_anterior = error;
  pid->tiempo_anterior = tiempo_actual;
  
  return salida;
}

void resetearPID(PIDController* pid) {
  pid->error_anterior = 0;
  pid->integral = 0;
  pid->tiempo_anterior = millis();
}

float calcularPIDCentrado(PIDCentrado* pid, float error) {
  unsigned long tiempo_actual = millis();
  
  // Inicializar en primera ejecución
  if (!pid->inicializado) {
    pid->tiempo_anterior = tiempo_actual;
    pid->error_anterior = error;
    pid->integral = 0;
    pid->inicializado = true;
    return 0; // No aplicar corrección en primera ejecución
  }
  
  float dt = (tiempo_actual - pid->tiempo_anterior) / 1000.0;
  
  // Evitar deltas muy grandes o muy pequeños
  if (dt <= 0 || dt > 0.5) {
    dt = 0.1; // 100ms por defecto
  }
  
  // Término proporcional
  float proporcional = pid->kp * error;
  
  // Término integral con anti-windup
  pid->integral += error * dt;
  pid->integral = constrain(pid->integral, -pid->integral_max, pid->integral_max);
  float integral = pid->ki * pid->integral;
  
  // Término derivativo
  float derivativo = pid->kd * (error - pid->error_anterior) / dt;
  
  // Salida PID total
  float salida = proporcional + integral + derivativo;
  
  // Guardar valores para próxima iteración
  pid->error_anterior = error;
  pid->tiempo_anterior = tiempo_actual;
  
  return salida;
}

void resetearPIDCentrado(PIDCentrado* pid) {
  pid->error_anterior = 0;
  pid->integral = 0;
  pid->tiempo_anterior = millis();
  pid->inicializado = false;
}

void ajustarParametrosPIDCentrado(float kp, float ki, float kd) {
  pidCentrado.kp = kp;
  pidCentrado.ki = ki;
  pidCentrado.kd = kd;
  resetearPIDCentrado(&pidCentrado);

}


// ==================== FUNCIONES ORIGINALES ====================

// Función para confirmar color - MEJORADA
bool confirmarColor(int hueMin, int hueMax, float satMin, float valueMin, float valueMax, int lecturas, int confirmacionesRequeridas) {
  int detecciones = 0;
  
  for (int i = 0; i < lecturas; i++) {
    uint16_t r, g, b, c;
    double hue, saturation, value;
    
    tcs.getRawData(&r, &g, &b, &c);
    
    if (c > 0) {
      float r_norm = (float)r / c;
      float g_norm = (float)g / c;
      float b_norm = (float)b / c;
      
      uint8_t r_byte = (uint8_t)(r_norm * 255.0);
      uint8_t g_byte = (uint8_t)(g_norm * 255.0);
      uint8_t b_byte = (uint8_t)(b_norm * 255.0);
      
      ColorConverter::RgbToHsv(r_byte, g_byte, b_byte, hue, saturation, value);
      hue *= 360;
      
      if (hueMin >= 180) { // Es azul
        float proporcionAzul = b_norm / (r_norm + g_norm + 0.001);
        if (hue >= hueMin && hue <= hueMax && 
            saturation > satMin && 
            value > valueMin && value < valueMax &&
            proporcionAzul > 0.9) {
          detecciones++;
        }
      } 
      else { // Es naranja
        bool enRangoNormal = (hue >= hueMin && hue <= hueMax);
        bool enRangoAlto = (hueMax > 300 && hue >= 300) || (hueMin < 60 && hue <= 60);
        
        if ((enRangoNormal || enRangoAlto) && 
            saturation > satMin && 
            value > valueMin && value < valueMax) {
          detecciones++;
        }
      }
    }
    delay(5);
  }
  
  return (detecciones >= confirmacionesRequeridas);
}

// Función auxiliar para normalizar la diferencia angular
float normalizarDiferenciaAngular(float diferencia) {
  while (diferencia > 180.0) diferencia -= 360.0;
  while (diferencia < -180.0) diferencia += 360.0;
  return diferencia;
}

void calcularAnguloObjetivoCenter() {
  if (sentidoVehiculo == IZQUIERDA) {
    // Giro a la izquierda: 1->0°, 2->90°, 3->180°, 4->270°, 5->0°, 6->90°...
    int ciclo = (contadorLineas - 1) % 4;  // Ajustar para que comience en 1
    switch (ciclo) {
      case 0: anguloObjetivo = 0.0; break;   // contadorLineas = 1, 5, 9...
      case 1: anguloObjetivo = 90.0; break;  // contadorLineas = 2, 6, 10...
      case 2: anguloObjetivo = 180.0; break; // contadorLineas = 3, 7, 11...
      case 3: anguloObjetivo = 270.0; break; // contadorLineas = 4, 8, 12...
    }
  } else {
    // Giro a la derecha: 1->0°, 2->270°, 3->180°, 4->90°, 5->0°, 6->270°...
    int ciclo = (contadorLineas - 1) % 4;  // Ajustar para que comience en 1
    switch (ciclo) {
      case 0: anguloObjetivo = 0.0; break;   // contadorLineas = 1, 5, 9...
      case 1: anguloObjetivo = 270.0; break; // contadorLineas = 2, 6, 10...
      case 2: anguloObjetivo = 180.0; break; // contadorLineas = 3, 7, 11...
      case 3: anguloObjetivo = 90.0; break;  // contadorLineas = 4, 8, 12...
    }
  }
}
void calcularAnguloObjetivoMantener() {
  if (sentidoVehiculo == IZQUIERDA) {
    // Giro a la izquierda: 1->90°, 2->180°, 3->270°, 4->0°, etc.
    int ciclo = (contadorLineas -1) % 4; // Ajuste para que la primera línea sea ciclo 0
    switch (ciclo) {
      case 0: anguloObjetivo = 90.0; break;  // Línea 1, 5, 9...
      case 1: anguloObjetivo = 180.0; break; // Línea 2, 6, 10...
      case 2: anguloObjetivo = 270.0; break; // Línea 3, 7, 11...
      case 3: anguloObjetivo = 0.0; break;   // Línea 4, 8, 12...
    }
  } else { // sentidoVehiculo == DERECHA
    // Giro a la derecha: 1->270°, 2->180°, 3->90°, 4->0°, etc.
    int ciclo = (contadorLineas - 1) % 4; // Ajuste para que la primera línea sea ciclo 0
    switch (ciclo) {
      case 0: anguloObjetivo = 270.0; break; // Línea 1, 5, 9...
      case 1: anguloObjetivo = 180.0; break; // Línea 2, 6, 10...
      case 2: anguloObjetivo = 90.0; break;  // Línea 3, 7, 11...
      case 3: anguloObjetivo = 0.0; break;   // Línea 4, 8, 12...
    }
  }
}
// Función para centrar el vehículo con PRIORIDAD ANGULAR
void centrarVehiculo() {
    if (retrocesoOrientadoActivo) {
    return;
  }
  // VERIFICACIÓN: No centrar si hay emergencias activas
  if (prioridadActual != PRIORIDAD_NORMAL || 
      esquiveDiagonalActivo || 
      retrocesoPorEmergencia) {
    if (pidCentrado.inicializado) {
      resetearPIDCentrado(&pidCentrado);
    }
    return;
  }
  
  // No centrar si está en giro, recién terminó giro, pausado por detección, o en esquive
  if (enGiro || 
      millis() - tiempoFinGiro < tiempoEsperaPosgiro ||
      (pausarCentrado && millis() - tiempoInicioDeteccion < tiempoPausaCentrado)) {
    
    if (pidCentrado.inicializado) {
      resetearPIDCentrado(&pidCentrado);
    }
    return;
  }
  
  if (millis() - ultimoTiempoCentrado < intervaloCentrado) {
    return;
  }
  
  // Calcular el ángulo objetivo SIEMPRE antes de proceder
  calcularAnguloObjetivoCenter();
  
  // Leer sensores con pequeña pausa para estabilidad
  distanciaDerecha = leerToF('D');
  delay(5);
  distanciaIzquierda = leerToF('I');
  
  // Validar lecturas de sensores
  if (distanciaDerecha > 80 || distanciaIzquierda > 80 || 
      distanciaDerecha < 1 || distanciaIzquierda < 1) {
    resetearPIDCentrado(&pidCentrado);
    return;
  }
  
  // VERIFICACIÓN FINAL antes de mover servo
  if (prioridadActual != PRIORIDAD_NORMAL || 
      esquiveDiagonalActivo || 
      retrocesoPorEmergencia) {
    resetearPIDCentrado(&pidCentrado);
    return;
  }
  
  // ===== CÁLCULO CON PRIORIDAD ANGULAR =====
  
  // 1. Calcular diferencia angular
  float diferenciaAngular = normalizarDiferenciaAngular(anguloZ - anguloObjetivo);
  
  // 2. VERIFICAR UMBRAL MÁXIMO - Si excede ±20°, SOLO corregir ángulo
  if (abs(diferenciaAngular) > UMBRAL_ANGULO_MAX) {
    // EMERGENCIA ANGULAR: Solo corregir ángulo, ignorar distancia
    float errorFinal = diferenciaAngular * FACTOR_CORRECCION_ANGULAR;
    
    // Debug para emergencia angular
    /*
    Serial.print(F("EMERGENCIA_ANG - AngZ:"));
    Serial.print(anguloZ, 1);
    Serial.print(F(" AngObj:"));
    Serial.print(anguloObjetivo, 1);
    Serial.print(F(" Dif:"));
    Serial.print(diferenciaAngular, 1);
    Serial.print(F(" ErrFinal:"));
    Serial.println(errorFinal, 1);
    */
    
    // Aplicar corrección PID solo angular
    float salidaPID = calcularPIDCentrado(&pidCentrado, errorFinal);
    salidaPID = constrain(salidaPID, -ERROR_MAXIMO_CENTRADO, ERROR_MAXIMO_CENTRADO);
    
    // Convertir a ángulo de servo
    float factorEscala = anguloCorreccionMax / ERROR_MAXIMO_CENTRADO;
    int correccionAngulo = salidaPID * factorEscala;
    
    static int anguloAnterior = SERVO_CENTRADO;
    int anguloDeseado = SERVO_CENTRADO - correccionAngulo;
    anguloDeseado = constrain(anguloDeseado, SERVO_CENTRADO - anguloCorreccionMax, 
                              SERVO_CENTRADO + anguloCorreccionMax);
    
    int anguloFinal = anguloAnterior * (1 - FACTOR_SUAVIZADO) + anguloDeseado * FACTOR_SUAVIZADO;
    anguloAnterior = anguloFinal;
    
    // Aplicar al servo
    if (prioridadActual == PRIORIDAD_NORMAL && 
        !esquiveDiagonalActivo && 
        !retrocesoPorEmergencia) {
      direccion.write(anguloFinal);
    }
    
    ultimoTiempoCentrado = millis();
    return; // Salir sin considerar distancia
  }
  
  // 3. Si está dentro del umbral, proceder con lógica de prioridad
  float errorDistancia = distanciaDerecha - distanciaIzquierda;
  float errorFinal = 0;
  
  // PRIORIDAD 1: Corregir ángulo si está fuera de tolerancia fina
  if (abs(diferenciaAngular) > TOLERANCIA_ANGULO_FINO) {
    // El ángulo es la prioridad principal
    errorFinal = diferenciaAngular * FACTOR_CORRECCION_ANGULAR;
    
    // Solo agregar corrección de distancia si también está muy descentrado
    if (abs(errorDistancia) > TOLERANCIA_CENTRADO_PID * 2) {
      errorFinal += errorDistancia * FACTOR_CORRECCION_DISTANCIA;
    }
    
    /*
    Serial.print(F("MODO_ANGULAR - Dif:"));
    Serial.print(diferenciaAngular, 1);
    Serial.print(F(" ErrDist:"));
    Serial.print(errorDistancia, 1);
    Serial.print(F(" ErrFinal:"));
    Serial.println(errorFinal, 1);
    */
    
  } 
  // PRIORIDAD 2: Si el ángulo está bien, entonces corregir distancia
  else if (abs(errorDistancia) > TOLERANCIA_CENTRADO_PID) {
    // Solo corrección por distancia
    errorFinal = errorDistancia;
    
    /*
    Serial.print(F("MODO_DISTANCIA - ErrDist:"));
    Serial.print(errorDistancia, 1);
    Serial.print(F(" ErrFinal:"));
    Serial.println(errorFinal, 1);
    */
  }
  // PRIORIDAD 3: Todo está bien
  else {
    // Ya está centrado tanto en ángulo como en distancia
    direccion.write(SERVO_CENTRADO);
    pidCentrado.integral = 0;
    pausarCentrado = false;
    
    /*
    Serial.println(F("CENTRADO_TOTAL"));
    */
    
    ultimoTiempoCentrado = millis();
    return;
  }
  
  // ===== APLICAR CONTROL PID =====
  float salidaPID = calcularPIDCentrado(&pidCentrado, errorFinal);
  salidaPID = constrain(salidaPID, -ERROR_MAXIMO_CENTRADO, ERROR_MAXIMO_CENTRADO);
  
  // Convertir salida PID a ángulo de servo
  float factorEscala = anguloCorreccionMax / ERROR_MAXIMO_CENTRADO;
  int correccionAngulo = salidaPID * factorEscala;
  
  // Aplicar suavizado para evitar cambios bruscos
  static int anguloAnterior = SERVO_CENTRADO;
  int anguloDeseado = SERVO_CENTRADO - correccionAngulo;
  anguloDeseado = constrain(anguloDeseado, SERVO_CENTRADO - anguloCorreccionMax, 
                            SERVO_CENTRADO + anguloCorreccionMax);
  
  // Aplicar suavizado
  int anguloFinal = anguloAnterior * (1 - FACTOR_SUAVIZADO) + anguloDeseado * FACTOR_SUAVIZADO;
  anguloAnterior = anguloFinal;
  
  // VERIFICACIÓN FINAL antes de mover servo
  if (prioridadActual == PRIORIDAD_NORMAL && 
      !esquiveDiagonalActivo && 
      !retrocesoPorEmergencia) {
    direccion.write(anguloFinal);
  }
  
 
  ultimoTiempoCentrado = millis();
}

void detenerRobot() {
  // Detener ambos motores para asegurar una parada completa
             
  detenerMotor(); 

  // Centrar el servo de la dirección
  direccion.write(SERVO_CENTRADO); 

  // Resetear cualquier flag de estado de movimiento
  enGiro = false; 
  
  // YA NO HAY BUCLE INFINITO. La función termina aquí.
}
// ==================== FUNCIÓN realizarGiro MEJORADA CON PID ====================


//calcular el ángulo dinámico
float calcularAnguloGiroDinamico(bool giroIzquierda) {
  // Se verifica si se cumple la condición especial. Si es así, se retorna un
  // GIRO_MINIMO inmediatamente, ignorando las mediciones de distancia. forzar apertura
  if ((sentidoVehiculo == IZQUIERDA && ultimoCarrilActivo == 2 && (carriles.esComandoC07||carriles.esComandoC21||carriles.esComandoC33 ||carriles.esComandoC15 ||carriles.esComandoC27 )) ||
      (sentidoVehiculo == DERECHA && ultimoCarrilActivo == 1 && (carriles.esComandoC02||carriles.esComandoC14||carriles.esComandoC26|| carriles.esComandoC32 || carriles.esComandoC20))) {

    esGiroApertura = true; // Asegura que se ejecute como un giro suave.
    Serial.println(">>> Condición prioritaria cumplida: Forzando GIRO_MINIMO.");
    return GIRO_MINIMO;
  }
  float distanciaSensor;
  
  // Medir distancias justo antes del giro
  distanciaDerecha = leerToF('D');
  distanciaIzquierda = leerToF('I');
  
  // Seleccionar sensor según sentido del vehículo
  if (sentidoVehiculo == DERECHA) {
    distanciaSensor = distanciaIzquierda;
  } else if (sentidoVehiculo == IZQUIERDA) {
    distanciaSensor = distanciaDerecha;
  } else {
    // Fallback: usar ángulos fijos si no hay sentisdo definido
    float anguloFallback = giroIzquierda ? giroObjetivoAzul : giroObjetivoNaranja;
    Serial.println("VAINA RARA EN calcularAnguloGiroDinamico ME VUELVE EL ES GIRO APERTURA A FALSE");
    //esGiroApertura = false; // No es giro mínimo en fallback
    return giroParaEstacionamiento ? anguloFallback + 5.0 : anguloFallback;
  }
  
  // Calcular ángulo basado en distancia con más subdivisiones
  float anguloCalculado;
  
  if (distanciaSensor < 25.0) {
    // Obstáculo muy cerca - giro máximo
    anguloCalculado = GIRO_MAXIMO;
    esGiroApertura = false;
  } else if (distanciaSensor >= 25.0 && distanciaSensor < 30.0) {
    // Obstáculo cerca - giro alto
    anguloCalculado = GIRO_ALTO;
    esGiroApertura = false;
  } else if (distanciaSensor >= 30.0 && distanciaSensor < 35.0) {
    // Obstáculo medio-cerca - giro medio-alto
    anguloCalculado = GIRO_MEDIO_ALTO;
    esGiroApertura = false;
  } else if (distanciaSensor >= 35.0 && distanciaSensor < 40.0) {
    // Obstáculo medio - giro medio
    anguloCalculado = GIRO_MEDIO;
    esGiroApertura = false;
  } else if (distanciaSensor >= 40.0 && distanciaSensor < 45.0) {
    // Obstáculo medio-lejos - giro medio-bajo
    anguloCalculado = GIRO_MEDIO_BAJO;
    esGiroApertura = true; 
  } else if (distanciaSensor >= 45.0 && distanciaSensor < 60.0) {
    // Obstáculo lejos - giro bajo
    anguloCalculado = GIRO_BAJO; 
    esGiroApertura = true; 
  } else {
    // Obstáculo muy lejos o no detectado - giro mínimo
   anguloCalculado = GIRO_MINIMO; 
    esGiroApertura = true; 
  }
  // Si está en modo de estacionamiento, agregar 5 grados
  if (giroParaEstacionamiento) {
    anguloCalculado += 5.0;
  }
  
  // Debug opcional - descomenta para ver valores
  
  Serial.print("Distancia: ");
  Serial.print(distanciaSensor);
  Serial.print(" cm, Ángulo: ");
  Serial.print(anguloCalculado);
  Serial.println("°");
  
  
  return anguloCalculado;
}
void realizarGiroApertura(bool giroIzquierda) {
  Serial.println(" INICIANDO GIRO DE APERTURA...");
  Serial.println("   Duración objetivo: " + String(duracionAperturaActual) + " ms");
  
  // Activar flags de apertura
  giroAperturaActivo = true;
  giroAperturaCompletado = false;
  tiempoInicioApertura = millis();

  //Guardar la dirección del giro para usarla después
  direccionGiroAperturaIzquierda = giroIzquierda;
  
  // Determinar dirección del servo para apertura (sentido CONTRARIO al giro actual)
  int servoApertura;
  if (giroIzquierda) {
    // Si el giro principal es a la IZQUIERDA, la apertura es a la DERECHA.
    servoApertura = SERVO_RIGHT;
  } else {
    // Si el giro principal es a la DERECHA, la apertura es a la IZQUIERDA.
    servoApertura = SERVO_LEFT;
  }
  
  // Imprimir un mensaje de debug para confirmar el ángulo que se va a usar
  Serial.println("   Giro Apertura: Orientando servo a " + String(servoApertura) + "°");

  // Activar motor y orientar servo para apertura
  avanzarMotor();
  direccion.write(servoApertura);
  
  // Debug opcional

  Serial.print("Iniciando giro de apertura - Servo: ");
  Serial.print(servoApertura);
  Serial.print(", Sentido vehículo: ");
  Serial.println(sentidoVehiculo);

}
bool procesarGiroApertura() {
  // Si no está activo el giro de apertura, no hacer nada
  if (!giroAperturaActivo) {
    return false; // 
  }
  actualizarGiro();
  // Verificar si ha pasado el tiempo de apertura
  if (millis() - tiempoInicioApertura >= duracionAperturaActual) { // 
    unsigned long tiempoRealTranscurrido = millis() - tiempoInicioApertura;
    Serial.println(" Giro de Apertura completado en " + String(tiempoRealTranscurrido) + " ms. Iniciando giro principal PID.");
   
    // *** PASO 1: Marcar la apertura como finalizada ***
    giroAperturaActivo = false;
    giroAperturaCompletado = true; // Marcar como completado por si se necesita
    
    // Centrar servo momentáneamente para una transición limpia
    //direccion.write(SERVO_CENTER); // 
    delay(50); // Pausa breve

    // *** PASO 2: INICIAR EL GIRO MÁXIMO INMEDIATAMENTE ***
    // Usamos la variable guardada 'direccionGiroAperturaIzquierda' para saber la dirección
    ejecutarGiroPID(anguloObjetivoGiroGlobal, direccionGiroAperturaIzquierda);

    // *** PASO 3: Resetear flags de apertura para el próximo ciclo ***
    giroAperturaCompletado = false;
    esGiroApertura = false;
    
    return false; // Se completó la transición de apertura a giro máximo.
  }
  
  return true; // La apertura aún está en proceso. 
}

// ==================== FUNCIÓN ejecutarGiroPID  ====================
void ejecutarGiroPID(float anguloObjetivo, bool giroIzquierda) {
  enGiro = true;
  giroEnProceso = true;
  cruceEnProceso = true;
  interrupcionCruce = false; 
  startTime = millis();

  // Se mantiene el cálculo del ángulo objetivo final
  actualizarGiro();
  float anguloInicial = anguloZ;
  anguloObjetivoActual = anguloInicial + (giroIzquierda ? anguloObjetivo : -anguloObjetivo);

  // Se mantiene la normalización del ángulo objetivo (0-360 grados)
  while (anguloObjetivoActual >= 360) anguloObjetivoActual -= 360;
  while (anguloObjetivoActual < 0) anguloObjetivoActual += 360;

  // Activar motor para el giro
  avanzarMotor();

  // Se elimina el cálculo PID. En su lugar, se aplica un giro constante y
  // brusco en la dirección deseada. Este comando se ejecuta una sola vez.
  if (giroIzquierda) {
    direccion.write(SERVO_LEFT); // Giro máximo a la izquierda
  } else {
    direccion.write(SERVO_RIGHT); // Giro máximo a la derecha
  }
  
  
  bool giroCompletado = false;
  int contadorEstable = 0;
  const int CONFIRMACIONES_ESTABLE = 1;

  // El bucle ahora solo verifica si se ha alcanzado el ángulo, sin calcular PID
  while (millis() - startTime < timeoutGiro && !giroCompletado && !interrupcionCruce) {
    actualizarGiro(); // Se sigue actualizando el ángulo actual
  
    // VERIFICACIÓN DE EMERGENCIA FRONTAL DURANTE EL GIRO
    float distanciaFrontal = leerToF('F');
    if (distanciaFrontal < 14 && distanciaFrontal > 0) {
        Serial.println("!!! OBSTÁCULO FRONTAL DURANTE GIRO !!! Abortando.");
        break; // Sale del bucle de giro inmediatamente.
    }

    // Se calcula la diferencia con el objetivo para saber cuándo parar
    float errorActual = anguloObjetivoActual - anguloZ;
    if (errorActual > 180) errorActual -= 360;
    if (errorActual < -180) errorActual += 360;
    Serial.print("ERROR ACTUAL DE ANGULO GIRO DE: ");
    Serial.println(errorActual);
    // Si el error es menor que la tolerancia, consideramos que el giro terminó
    if (abs(errorActual) <= TOLERANCIA_PID) {
      contadorEstable++;
      if (contadorEstable >= CONFIRMACIONES_ESTABLE) {
        giroCompletado = true;
        Serial.println("Giro Completado***************");
        break;
      }
    } else {
      contadorEstable = 0;
    }
    
    delay(20);
  }

  // Se mantiene toda la lógica de finalización del giro:
  // centrar dirección, resetear flags y activar el retroceso post-giro.
  direccion.write(SERVO_CENTRADO);
  avanzarMotor();

  enGiro = false;
  giroEnProceso = false;
  cruceEnProceso = false;
  tiempoFinGiro = millis();

  direccionRetroceso = giroIzquierda ? SERVO_OrientadoObjeto_RIGH : SERVO_OrientadoObjeto_LEFT;
  retrocesoOrientadoActivo = true;
  tiempoInicioRetrocesoOrientado = millis();
}
void controlarDireccion(float anguloObjetivoParam) {
  // Determinar si el ángulo está en rango correcto respecto al ángulo objetivo
  bool enRangoCorrecto = estaEnRangoCorrecto(anguloZ, anguloObjetivoParam);
  
  // Debug: mostrar estado del rango cada 500ms
  static unsigned long lastRangeDebug = 0;
  if (millis() - lastRangeDebug > 500) {
    Serial.print("Ángulo actual: ");
    Serial.print(anguloZ);
    Serial.print("° - Objetivo: ");
    Serial.print(anguloObjetivoParam);
    Serial.print("° - En rango: ");
    Serial.print(enRangoCorrecto ? "SÍ" : "NO");
    Serial.print(" - Estado: ");
    
    switch (estadoActualCorreccion) {
      case RECTO: Serial.println("RECTO"); break;
      case CORRIGIENDO_IZQUIERDA: Serial.println("CORRIGIENDO IZQUIERDA"); break;
      case CORRIGIENDO_DERECHA: Serial.println("CORRIGIENDO DERECHA"); break;
      case ESTABILIZANDO: Serial.println("ESTABILIZANDO"); break;
    }
    lastRangeDebug = millis();
  }
  
  switch (estadoActualCorreccion) {
    case RECTO:
      if (!enRangoCorrecto) {
        iniciarCorreccion(anguloObjetivoParam);
      }
      break;
      
    case CORRIGIENDO_IZQUIERDA:
      direccion.write(SERVO_LEFT); 
      if (enRangoCorrecto) {
          finalizarCorreccion();
      } else if (millis() - tiempoInicioCorreccion > TIEMPO_MAX_CORRECCION) {
          // Timeout de corrección - detener y reintentar
          Serial.println("Timeout de corrección - reiniciando");
          finalizarCorreccion();
      } 
      break;

    case CORRIGIENDO_DERECHA:
      direccion.write(SERVO_RIGHT); 
      if (enRangoCorrecto) {
          finalizarCorreccion();
      } else if (millis() - tiempoInicioCorreccion > TIEMPO_MAX_CORRECCION) {
          // Timeout de corrección - detener y reintentar
          Serial.println("Timeout de corrección - reiniciando");
          finalizarCorreccion();
      }
      break;
      
    case ESTABILIZANDO:
      if (millis() - tiempoInicioCorreccion > TIEMPO_ESTABILIZACION) {
        estadoActualCorreccion = RECTO;
        correccionCompletada = true;
        Serial.println("Estabilización completa - modo recto");
      }
      break;
  }
}

void iniciarCorreccion(float anguloObjetivoParam) {
  Serial.print("Iniciando corrección - Ángulo actual: ");
  Serial.print(anguloZ);
  Serial.print("° - Objetivo: ");
  Serial.print(anguloObjetivoParam);
  Serial.println("°");
  
  // LÓGICA PULL-UP: HIGH = detenido, LOW = activado
  // Detener motor principal y activar retroceso
  detenerMotor();  // DETENER motor principal (pull-up)
  retrocederMotor(); // ACTIVAR retroceso si es necesario
  
  // Determinar dirección de corrección basada en la diferencia angular
  float diferenciaAngular = calcularDiferenciaAngular(anguloZ, anguloObjetivoParam);
  
  if (diferenciaAngular > 0) {
    // El ángulo actual está a la derecha del objetivo -> corregir hacia la izquierda
    direccion.write(SERVO_LEFT);
    estadoActualCorreccion = CORRIGIENDO_IZQUIERDA;
    Serial.print("Corrigiendo hacia la IZQUIERDA (diferencia: +");
    Serial.print(diferenciaAngular);
    Serial.println("°)");
    
  } else {
    // El ángulo actual está a la izquierda del objetivo -> corregir hacia la derecha
    direccion.write(SERVO_RIGHT);
    estadoActualCorreccion = CORRIGIENDO_DERECHA;
    Serial.print("Corrigiendo hacia la DERECHA (diferencia: ");
    Serial.print(diferenciaAngular);
    Serial.println("°)");
  }
  
  tiempoInicioCorreccion = millis();
}

void finalizarCorreccion() {
 /* Serial.print("Corrección completada - Ángulo final: ");
  Serial.print(anguloZ);
  Serial.println("°");
  */
  // Detener retroceso
  detenerMotor(); // DETENER retroceso (pull-up)
  
  // Enderezar dirección
  direccion.write(SERVO_CENTER);
  
  // Período de estabilización
  estadoActualCorreccion = ESTABILIZANDO;
  tiempoInicioCorreccion = millis();
  
  // Pausa para estabilización
  delay(100);
  
  // Solo reactivar motor si la distancia objetivo ya está alcanzada
  float distanciaTrasera = leerToF('T');
  bool distanciaAlcanzada = (distanciaTrasera <= DISTANCIA_MINIMA_RETROCESO && distanciaTrasera > 0);
  
  if (distanciaAlcanzada) {
    // Si ya estamos en la distancia objetivo, podemos reactivar el motor de avance
    avanzarMotor();// ACTIVAR motor principal (pull-up)
    Serial.println("Motor de avance reactivado - distancia objetivo ya alcanzada");
  } else {
    // Si no hemos alcanzado la distancia, mantener el motor de avance detenido
    // El retroceso orientado se encargará de activarlo cuando sea necesario
    detenerMotor(); // MANTENER motor principal detenido (pull-up)
    Serial.println("Motor de avance mantenido detenido - falta alcanzar distancia objetivo");
  }
}
// ===== FUNCIONES DE NAVEGACIÓN Y LÍNEAS =====
void calcularAnguloObjetivo() {
  if (sentidoVehiculo == IZQUIERDA) {
    // Giro a la izquierda: 1->0°, 2->90°, 3->180°, 4->270°, 5->0°, 6->90°...
    int ciclo = (contadorLineas - 1) % 4;  // Ajustar para que comience en 1
    switch (ciclo) {
      case 0: anguloObjetivo = 0.0; break;   // contadorLineas = 1, 5, 9...
      case 1: anguloObjetivo = 90.0; break;  // contadorLineas = 2, 6, 10...
      case 2: anguloObjetivo = 180.0; break; // contadorLineas = 3, 7, 11...
      case 3: anguloObjetivo = 270.0; break; // contadorLineas = 4, 8, 12...
    }
  } else {
    // Giro a la derecha: 1->0°, 2->270°, 3->180°, 4->90°, 5->0°, 6->270°...
    int ciclo = (contadorLineas - 1) % 4;  // Ajustar para que comience en 1
    switch (ciclo) {
      case 0: anguloObjetivo = 0.0; break;   // contadorLineas = 1, 5, 9...
      case 1: anguloObjetivo = 270.0; break; // contadorLineas = 2, 6, 10...
      case 2: anguloObjetivo = 180.0; break; // contadorLineas = 3, 7, 11...
      case 3: anguloObjetivo = 90.0; break;  // contadorLineas = 4, 8, 12...
    }
  }
}

void mostrarEstadoActual() {
  Serial.println("=== ESTADO ACTUAL ===");
  Serial.print("Contador de líneas: ");
  Serial.println(contadorLineas);
  Serial.print("Sentido de giro: ");
  Serial.println(sentidoVehiculo == IZQUIERDA ? "IZQUIERDA" : "DERECHA");
  Serial.print("Ángulo objetivo: ");
  Serial.print(anguloObjetivo);
  Serial.println("°");
  Serial.print("Ángulo actual: ");
  Serial.print(anguloZ);
  Serial.println("°");
  Serial.print("Tolerancia: ±");
  Serial.print(TOLERANCIA_ANGULO);
  Serial.println("°");
  Serial.println("====================");
}

bool estaEnRangoCorrecto(float anguloActual, float objetivo) {
  float diferencia = abs(calcularDiferenciaAngular(anguloActual, objetivo));
  return diferencia <= TOLERANCIA_ANGULO;
}
bool estaEnRangoCorrectoCorrecionDeLineaAntesDelGiro(float anguloActual, float objetivo) {
  float diferencia = abs(calcularDiferenciaAngular(anguloActual, objetivo));
  return diferencia <= TOLERANCIA_ANGULO_CORRECCION_LINEA_ANTES_DEL_GIRO;
}

float calcularDiferenciaAngular(float actual, float objetivo) {
  // Calcula la diferencia angular considerando el cruce de 0°/360°
  float diferencia = actual - objetivo;
  
  // Normalizar la diferencia al rango [-180, 180]
  while (diferencia > 180.0) diferencia -= 360.0;
  while (diferencia < -180.0) diferencia += 360.0;
  
  return diferencia;
}
//funcion de detectar color reducida solo para reconfirmar que detecto una linea sin activar nada.
bool chequearLinea() {
  // No chequear si estamos en maniobras de alta prioridad como el retroceso final.
  if (retrocesoOrientadoActivo) {
    return false;
  }

  uint16_t r, g, b, c;
  double hue, saturation, value;

  tcs.getRawData(&r, &g, &b, &c);
  if (c > 0) {
    // Se realiza la misma conversión de color que en la función principal
    float r_norm = (float)r / c; 
    float g_norm = (float)g / c; 
    float b_norm = (float)b / c; 

    uint8_t r_byte = (uint8_t)(r_norm * 255.0); 
    uint8_t g_byte = (uint8_t)(g_norm * 255.0); 
    uint8_t b_byte = (uint8_t)(b_norm * 255.0); 

    ColorConverter::RgbToHsv(r_byte, g_byte, b_byte, hue, saturation, value); 

    bool esBlanco = (value > 0.85 && saturation < 0.15); 
    float proporcionAzul = b_norm / (r_norm + g_norm + 0.001); 
    // Detección azul (usando las mismas condiciones que en detectarColor)
    if (hue >= HUE_AZUL_MIN && hue <= HUE_AZUL_MAX &&
      saturation > SATURACION_AZUL_MIN &&
      value > VALOR_AZUL_MIN && value < VALOR_AZUL_MAX &&
      proporcionAzul > PROPORCION_AZUL_MIN &&
      !esBlanco) {
      if (confirmarColor(HUE_AZUL_MIN, HUE_AZUL_MAX, SATURACION_AZUL_MIN, VALOR_AZUL_MIN, VALOR_AZUL_MAX, CONFIRMAR_AZUL_TOLERANCIA, CONFIRMAR_AZUL_DELAY)) {
   
        return true; // ¡Línea azul encontrada!
      }
    }
      else if (((hue >= HUE_NARANJA_MIN && hue <= HUE_NARANJA_MAX)) &&
            saturation > SATURACION_NARANJA_MIN &&
            value > VALOR_NARANJA_MIN && value < VALOR_NARANJA_MAX &&
            !esBlanco) {
        if (confirmarColor(HUE_NARANJA_MIN, HUE_NARANJA_MAX, SATURACION_NARANJA_MIN, VALOR_NARANJA_MIN, VALOR_NARANJA_MAX, CONFIRMAR_NARANJA_TOLERANCIA, CONFIRMAR_NARANJA_DELAY)) {
        return true; // ¡Línea naranja encontrada!
      }
    }
  }
  // Si llegamos hasta aquí, no se detectó nada.
  return false;
}

// ===== FUNCIÓN PRINCIPAL DE INTEGRACIÓN =====
void realizarGiro(bool giroIzquierda) {
  // RESETEAR VARIABLES DE ESTADO ANTES DE INICIAR
  correccionCompletada = false;
  estadoActualCorreccion = RECTO;
  
  // RESETEAR variables de compensación
  tiempoTotalRetroceso = 0;
  huboRetrocesoEnCorreccion = false;

  // ===================== =====================
  // PASO 1: Calcular el ángulo de giro dinámico y el tiempo de retroceso necesario AL PRINCIPIO.
  // Esto nos permite saber de antemano si se requerirá un retroceso por giro cerrado.
  float anguloObjetivoGiro = calcularAnguloGiroDinamico(giroIzquierda);
  anguloObjetivoGiroGlobal = anguloObjetivoGiro;
  unsigned long tiempoRetrocesoAdicional = 0;
  bool retrocesoGestionado = false; // Flag para controlar que solo se ejecute una lógica de retroceso.

  /*if (anguloObjetivoGiro == GIRO_MAXIMO) {
    tiempoRetrocesoAdicional = TIEMPO_RETROCESO_MAXIMO;
  } else if (anguloObjetivoGiro == GIRO_ALTO) {
    tiempoRetrocesoAdicional = TIEMPO_RETROCESO_ALTO;
  } else if (anguloObjetivoGiro == GIRO_MEDIO_ALTO) {
    tiempoRetrocesoAdicional = TIEMPO_RETROCESO_MEDIO_ALTO;
  }*/
  // ==========================================

  // PASO 2: Actualizar ángulo objetivo de navegación basado en contadorLineas actual
  calcularAnguloObjetivo();

  // PASO 3: Verificar si estamos en el rango correcto
  bool enRangoCorrecto = estaEnRangoCorrectoCorrecionDeLineaAntesDelGiro(anguloZ, anguloObjetivo);

  if (!enRangoCorrecto) {
    Serial.println("Ángulo fuera de rango - iniciando corrección antes del giro");
    Serial.print(" > Actual: "); Serial.print(anguloZ);
    Serial.print(" | Objetivo: "); Serial.println(anguloObjetivo);
    
    tiempoInicioCorreccion = millis();
    unsigned long tiempoInicioRetrocesoLocal = 0;
    bool retrocediendoAhora = false;
    
      // Bucle de corrección hasta que el ángulo esté en rango
      while (!estaEnRangoCorrectoCorrecionDeLineaAntesDelGiro(anguloZ, anguloObjetivo)) {
        actualizarGiro();
        
        // NUENA FORMA: Comprueba si IN1 está en HIGH (tu estado de retroceso)
        bool motorRetrocesoActivo = (digitalRead(MOTOR_IN1) == HIGH); 
        
        if (motorRetrocesoActivo && !retrocediendoAhora) {
          tiempoInicioRetrocesoLocal = millis();
          retrocediendoAhora = true;
          huboRetrocesoEnCorreccion = true;
        } else if (!motorRetrocesoActivo && retrocediendoAhora) {
          tiempoTotalRetroceso += (millis() - tiempoInicioRetrocesoLocal);
          retrocediendoAhora = false;
        }
      
      controlarDireccion(anguloObjetivo);
      delay(10);
      
      if (millis() - tiempoInicioCorreccion > TIEMPO_MAX_CORRECCION) {
        Serial.println("Timeout de corrección - procediendo con giro");
        if (retrocediendoAhora) {
          tiempoTotalRetroceso += (millis() - tiempoInicioRetrocesoLocal);
        }
        break;
      }
    }
    
    if (retrocediendoAhora) {
      tiempoTotalRetroceso += (millis() - tiempoInicioRetrocesoLocal);
    }
    
    finalizarCorreccion();
    delay(100);


     if (huboRetrocesoEnCorreccion && tiempoTotalRetroceso > 0) {
      // CASO B: NO se necesita retroceso pre-giro, PERO sí hubo retroceso por corrección.
      // Solo en este caso, hacemos el avance compensatorio.
      unsigned long tiempoAvanceCompensatorio = tiempoTotalRetroceso/2;
      Serial.print("Iniciando avance compensatorio por hasta ");
      Serial.print(tiempoAvanceCompensatorio);
      Serial.println(" ms, buscando línea activamente...");
      
      int servoDireccion = giroIzquierda ? SERVO_OrientadoObjeto_RIGH : SERVO_OrientadoObjeto_LEFT;
      
      detenerMotor();
      avanzarMotor();
      direccion.write(servoDireccion);
      
      unsigned long tiempoInicioAvance = millis();
      bool lineaReDetectada = false;
      while ((millis() - tiempoInicioAvance) < tiempoAvanceCompensatorio) {
        actualizarGiro();
        if (chequearLinea()) {
          lineaReDetectada = true;
          break;
        }
        delay(5);
      }
      
      detenerMotor();
      direccion.write(SERVO_CENTER); 

      if (lineaReDetectada) {
        Serial.println("Avance compensatorio interrumpido por re-detección de línea.");
      } else {
        Serial.println("Avance compensatorio completado por tiempo.");
      }
      delay(50);
    }
    // ===================== FIN DE LA LÓGICA DE DECISIÓN =====================

  } else {
    Serial.println("PRE-GIRO: Ángulo ya en rango. Omitiendo corrección.");
    Serial.print(" > Actual: "); Serial.print(anguloZ);
    Serial.print(" | Objetivo: "); Serial.println(anguloObjetivo);
  }
  
  // PASO 4: Ejecutar la lógica de giro.

  // Si es un giro de apertura (suave), la lógica no cambia.
  if (esGiroApertura) {
     if (anguloObjetivoGiro == GIRO_MEDIO) {
      duracionAperturaActual = DURACION_APERTURA_MINIMO;
    } else if (anguloObjetivoGiro == GIRO_MINIMO) {
      duracionAperturaActual = DURACION_APERTURA_MINIMO;
    } else if (anguloObjetivoGiro == GIRO_BAJO) {
      duracionAperturaActual = DURACION_APERTURA_BAJO;
    } else if (anguloObjetivoGiro == GIRO_MEDIO_BAJO) {
      duracionAperturaActual = DURACION_APERTURA_MEDIO_BAJO;
    }
    
    if (!giroAperturaCompletado && !giroAperturaActivo) {
       realizarGiroApertura(giroIzquierda);
    }
    return;
  }

  // ===================== LÓGICA DE RETROCESO MODIFICADA =====================
  // Este bloque ahora solo se ejecuta si el ángulo ESTABA BIEN desde el principio
  // y, por lo tanto, la lógica de unificación no se activó.
  if (tiempoRetrocesoAdicional > 0 && !retrocesoGestionado) {
    Serial.print("GIRO CERRADO (sin corrección previa): Retrocediendo recto por ");
    Serial.print(tiempoRetrocesoAdicional);
    Serial.println(" ms.");

    detenerMotor();
    direccion.write(SERVO_CENTER);
    retrocederMotor();
    delay(tiempoRetrocesoAdicional);
    detenerMotor();
    delay(100);
  }
  // ===================== FIN =====================

  // Finalmente, ejecutar el giro PID normal.
  ejecutarGiroPID(anguloObjetivoGiro, giroIzquierda);
}
float calcularAnguloRetroceso() {
  float anguloRetroceso;
  
  if (sentidoVehiculo == IZQUIERDA) {
    // Giro a la izquierda: 1->90°, 2->180°, 3->270°, 4->0°, 5->90°...
    int ciclo = (contadorLineas - 1) % 4;
    switch (ciclo) {
      case 0: anguloRetroceso = 90.0; break;   // contadorLineas = 1, 5, 9...
      case 1: anguloRetroceso = 180.0; break;  // contadorLineas = 2, 6, 10...
      case 2: anguloRetroceso = 270.0; break;  // contadorLineas = 3, 7, 11...
      case 3: anguloRetroceso = 0.0; break;    // contadorLineas = 4, 8, 12...
    }
  } else {
    // Giro a la derecha: 1->270°, 2->180°, 3->90°, 4->0°, 5->270°...
    int ciclo = (contadorLineas - 1) % 4;
    switch (ciclo) {
      case 0: anguloRetroceso = 270.0; break;  // contadorLineas = 1, 5, 9...
      case 1: anguloRetroceso = 180.0; break;  // contadorLineas = 2, 6, 10...
      case 2: anguloRetroceso = 90.0; break;   // contadorLineas = 3, 7, 11...
      case 3: anguloRetroceso = 0.0; break;    // contadorLineas = 4, 8, 12...
    }
  }
  
  return anguloRetroceso;
}


// ============== FUNCIÓN DE RETROCESO POST-GIRO ORIENTADO  ==============
void realizarAvanceDeReintento() {
  Serial.println(">> Ejecutando avance de reintento...");
  
  // 1. Detener el retroceso y centrar la dirección.
  detenerMotor();
  direccion.write(SERVO_CENTER);
  delay(200); // Pausa para estabilizar.

  // 2. Avanzar por un tiempo definido, actualizando el giro constantemente.
  avanzarMotor();
  unsigned long tiempoInicioAvance = millis();
  while (millis() - tiempoInicioAvance < TIEMPO_AVANCE_REINTENTO) {
    actualizarGiro(); // Actualiza el ángulo mientras avanza.
    direccion.write(SERVO_CENTER);
    delay(10);        // Pequeña pausa para no saturar el procesador.
  }

  // 3. Detener el avance.
  detenerMotor();
  delay(200); // Pausa antes de volver a intentar.
}
bool procesarRetrocesoPostGiro() {
  // Flags estáticos para controlar el flujo dentro de la función.
  static bool anguloCorregido = false;
  static unsigned long ultimoTiempoInicio = 0;
  // Si el tiempo de inicio ha cambiado, es un nuevo giro. Reseteamos el flag.
  if (tiempoInicioRetrocesoOrientado != ultimoTiempoInicio) {
    anguloCorregido = false;
    ultimoTiempoInicio = tiempoInicioRetrocesoOrientado;
  }

  // Si el retroceso no está activo, no hacer nada.
  if (!retrocesoOrientadoActivo) {
    return false;
  }

  actualizarGiro();
  unsigned long tiempoTranscurrido = millis() - tiempoInicioRetrocesoOrientado;
  // ==================== FASE 1: CORRECCIÓN DE ÁNGULO (SE EJECUTA UNA SOLA VEZ) ====================
  if (!anguloCorregido) {
    float anguloObjetivoRetroceso = calcularAnguloRetroceso();
    bool enRangoCorrecto = estaEnRangoCorrecto(anguloZ, anguloObjetivoRetroceso); 

    if (!enRangoCorrecto) {
      Serial.println("POST-GIRO: Ángulo fuera de rango. Iniciando corrección...");
      unsigned long tiempoInicioCorreccionLocal = millis();

      // Bucle para corregir el ángulo hasta que esté en rango o haya un timeout.
      while (!estaEnRangoCorrecto(anguloZ, anguloObjetivoRetroceso)) {
        actualizarGiro();
        float diferencia = calcularDiferenciaAngular(anguloZ, anguloObjetivoRetroceso);
        // Gira en la dirección opuesta al error para corregir.
        if (diferencia > 0) {
          direccion.write(180);
        } else {
          direccion.write(0);
        }
        
        retrocederMotor(); 
        //detenerMotor();
        delay(20);

        // ===== VERIFICACIÓN DE PROXIMIDAD TRASERA DURANTE LA CORRECCIÓN =====
        float distanciaTrasera = leerToF('T');
        if (distanciaTrasera < 6.0 && distanciaTrasera > 0) {
            Serial.println("! Muy cerca por detrás (" + String(distanciaTrasera, 1) + "cm).");
            realizarAvanceDeReintento(); // Ejecuta la maniobra de avance.
            tiempoInicioCorreccionLocal = millis(); // Reinicia el temporizador de timeout.
            Serial.println("Reintentando corrección de ángulo...");
            continue; // Vuelve al inicio del bucle para re-evaluar el ángulo.
        }
        // =========================================================================

        // Timeout de seguridad para la corrección.
        if (millis() - tiempoInicioCorreccionLocal > TIEMPO_MAX_CORRECCION) {
          Serial.println("Timeout de corrección.");
          // ===== MODIFICADO: Llamamos a la nueva función de avance =====
          realizarAvanceDeReintento(); 
          // ============================================================
          
          // Reiniciar el temporizador del timeout para el nuevo intento de corrección.
          tiempoInicioCorreccionLocal = millis(); 
          Serial.println("Reintentando corrección de ángulo...");
        }
      }
    }
    
    Serial.println("POST-GIRO: Corrección de ángulo finalizada.");
    detenerMotor();
    direccion.write(SERVO_CENTER); 
    delay(250); // Pausa para estabilizar.
    anguloCorregido = true; // Marcamos que la corrección ya se hizo.
  }

  // ==================== FASE 2: CONTROL DE DISTANCIA (RETROCESO O AVANCE) ====================
  
  // Timeout de seguridad general para toda la maniobra.
  if (tiempoTranscurrido >= TIMEOUT_RETROCESO_MAXIMO) {
    Serial.println("Timeout de seguridad alcanzado en retroceso post-giro.");
    finalizarRetrocesoPostGiro();
    return false;
  }

  // Medimos la distancia trasera.
  float distanciaTrasera = leerToF('T');
  // Si la lectura del sensor falla, esperamos un poco y continuamos.
  if (distanciaTrasera > 200) {
      delay(50);
      return true; // Maniobra sigue activa.
  }

  float errorDistancia = distanciaTrasera - DISTANCIA_MINIMA_RETROCESO;
  const float toleranciaDistancia = 1.5;

  // CASO 1: La distancia es correcta (dentro de la tolerancia).
  if (abs(errorDistancia) <= toleranciaDistancia) {
    Serial.println("Distancia objetivo (" + String(DISTANCIA_MINIMA_RETROCESO) + " cm) alcanzada. Finalizando.");
    finalizarRetrocesoPostGiro();
    return false; // Maniobra terminada.
  }
  // CASO 2: Estamos muy lejos de la pared, hay que seguir retrocediendo.
  else if (errorDistancia > toleranciaDistancia) {
    direccion.write(SERVO_CENTER); // Servo recto.
    retrocederMotor(); // Activar retroceso.
    //detenerMotor();
    return true; // Maniobra sigue activa.
  }
  // CASO 3: Nos pasamos (estamos muy cerca), hay que avanzar para corregir.
  else { // (errorDistancia < -toleranciaDistancia)
    Serial.println("Nos pasamos de largo. Avanzando para corregir...");
    detenerMotor();
    direccion.write(SERVO_CENTER);
    delay(100);

    avanzarMotor();// Activar avance.
    unsigned long tiempoInicioAvance = millis();
    while (leerToF('T') < DISTANCIA_MINIMA_RETROCESO) {
      if (millis() - tiempoInicioAvance > 300) {
        actualizarGiro(); 
        Serial.println("Timeout en avance de corrección.");
        break;
      }
      delay(20);
    }
    
    Serial.println("Corrección por avance finalizada.");
    finalizarRetrocesoPostGiro();
    return false; // Maniobra terminada.
  }
}
// ============== FUNCIÓN PARA FINALIZAR RETROCESO ==============
void finalizarRetrocesoPostGiro() {
  // Paso 1: Detener motores inmediatamente
  
  detenerMotor();
  
  // Paso 2: Centrar dirección
  direccion.write(SERVO_CENTER);
  //delay(5000);
  
  // Paso 3: Pausa para estabilizar sistema
  delay(100);
  
  // Paso 4: Resetear flags de estado completamente
  retrocesoOrientadoActivo = false;
  enGiro = false;
  giroEnProceso = false;
  cruceEnProceso = false;
  
  // Paso 5: Resetear variables de tiempo
  tiempoInicioRetrocesoOrientado = 0;
  // Paso 6: Verificar si el giro era para el estacionamiento final
  if (giroParaEstacionamiento) {
    // Si era el último giro, proceder al estado de detención
    estadoActual = ESTADO_DETENCION_FINAL; 
    tiempoInicioDetencionFinal = millis(); 
    giroParaEstacionamiento = false; 
  } else {
     // CAMBIO CLAVE: Si no es el giro final, transicionar a la CORRECCIÓN LATERAL.
    estadoActual = ESTADO_CORRECCION_LATERAL;
    controlLateral_PostGiro.funcionActiva = true; // Activamos la nueva lógica
    controlLateral_PostGiro.estadoActual = ControlDistanciaLateral::IDLE;
    Serial.println("🚀 Transición a ESTADO_CORRECCION_LATERAL post-giro.");
    controlLateral_PostGiro.tiempoInicioEstado = millis(); // Reset timer para la pausa
    }

  // Resetear el PID de centrado
  resetearPIDCentrado(&pidCentrado);

  // El motor se activará cuando comience un nuevo estado de carril
  detenerMotor(); // Apagado por defecto en espera
  
  // Resetear variables de apertura
  giroAperturaActivo = false;
  giroAperturaCompletado = false;
  esGiroApertura = false;
}

void manejarInterrupcionCruce() {
  // Detener el giro inmediatamente
  direccion.write(SERVO_CENTRADO);
  avanzarMotor();// Mantener motor activo
  
  // Resetear flags de giro
  enGiro = false;
  giroEnProceso = false;
  cruceEnProceso = false;
  tiempoFinGiro = millis();
  
  // Procesar el comando que causó la interrupción
  colorDetectado = comandoInterrupcion;

  // Resetear flags de interrupción
  interrupcionCruce = false;
  comandoInterrupcion = 'N';
}

// ========== ESTRATEGIA 1: CORRECCIÓN PERIÓDICA SIMPLE ==========
void actualizarGiro() {
  readIMU(); // Lee el sensor BNO055 y actualiza la variable anguloZ
  
  // Imprimir el ángulo en todo momento (con un límite seguro de 50ms para no saturar el Bluetooth)
 /* static unsigned long ultimoPrintGiro = 0;
  if (millis() - ultimoPrintGiro > 50) {
    Serial.print("🎯 Ángulo Z actual: ");
    Serial.println(anguloZ);
    // Serial.print("🎯 Ángulo Z actual: "); Serial.println(anguloZ); // Descomenta esta si también lo quieres ver por cable USB
    ultimoPrintGiro = millis();
  }*/
}
void applyCalibrationOffsets() {
  adafruit_bno055_offsets_t calOffsets;
  calOffsets.accel_offset_x = 12;
  calOffsets.accel_offset_y = -46;
  calOffsets.accel_offset_z = -13;
  calOffsets.accel_radius   = 1000;
  calOffsets.gyro_offset_x  = 0;
  calOffsets.gyro_offset_y  = 1;
  calOffsets.gyro_offset_z  = 0;
  calOffsets.mag_offset_x   = 156;
  calOffsets.mag_offset_y   = 563;
  calOffsets.mag_offset_z   = -287;
  calOffsets.mag_radius     = 651;
  bno.setSensorOffsets(calOffsets);
  // Serial.println(F("Offsets de calibracion de IMU aplicados."));
}
void readIMU() {
  imu::Quaternion quat = bno.getQuat();
  double w = quat.w(), x = quat.x(), y = quat.y(), z = quat.z();
  double norm = sqrt(w*w + x*x + y*y + z*z);
  if (norm < 1e-6) return;
  w /= norm; x /= norm; y /= norm; z /= norm;

  double yaw = atan2(2.0*(w*z + x*y), 1.0 - 2.0*(y*y + z*z));
  float yaw_deg_temp = (float)(yaw * 180.0 / M_PI);
  
  yaw_deg_temp -= yaw_offset;
  while (yaw_deg_temp < 0.0f) yaw_deg_temp += 360.0f;
  while (yaw_deg_temp >= 360.0f) yaw_deg_temp -= 360.0f;

  anguloZ = yaw_deg_temp;
}

void resetYaw() {
  imu::Quaternion quat = bno.getQuat();
  double w = quat.w(), x = quat.x(), y = quat.y(), z = quat.z();
  double norm = sqrt(w*w + x*x + y*y + z*z);
  if (norm < 1e-6) return;
  w /= norm; x /= norm; y /= norm; z /= norm;

  float rawYaw = (float)(atan2(2.0*(w*z + x*y), 1.0 - 2.0*(y*y + z*z)) * 180.0 / M_PI);
  yaw_offset = rawYaw; 
  anguloZ = 0.0f;
}


void mostrarInformacion() {
  if (millis() - lastYawPrint >= YAW_INTERVAL) {
    Serial.print("Yaw: ");
    Serial.print(anguloZ, 1);
    Serial.print(" | Objetivo: ");
    Serial.print(anguloObjetivo);
    Serial.print("° | Estado: ");
    
    switch (estadoActualCorreccion) {
      case RECTO: Serial.println("RECTO"); break;
      case CORRIGIENDO_IZQUIERDA: Serial.println("CORRIGIENDO IZQUIERDA"); break;
      case CORRIGIENDO_DERECHA: Serial.println("CORRIGIENDO DERECHA"); break;
      case ESTABILIZANDO: Serial.println("ESTABILIZANDO"); break;
    }
    
    lastYawPrint = millis();
  }
}
void detectarColor() {
  if (giroEnProceso) return;
  uint16_t r, g, b, c;
  double hue, saturation, value;
  
  // Ignorar TODA detección durante retroceso post-giro
  if (retrocesoOrientadoActivo) {
    return; // Salir inmediatamente sin procesar nada
  }
   // ── NUEVO GUARD ──────────────────────────────────────────────────────────
  // No contar líneas ni cambiar ángulo objetivo durante maniobras de carril.
  // Solo se permite detectar si estamos en ESTADO_NORMAL, MANTENIENDO_CARRIL,
  // o ESTADO_DETENCION_FINAL. En cualquier otro estado de maniobra se ignora.
  bool estadoPermiteDeteccion = (estadoActual == ESTADO_NORMAL     ||
                                  estadoActual == MANTENIENDO_CARRIL ||
                                  estadoActual == ESTADO_DETENCION_FINAL);
  if (!estadoPermiteDeteccion) {
    return;
  }
  // ─────────────────────────────────────────────────────────────────────────


  tcs.getRawData(&r, &g, &b, &c);

  if (c > 0) {
    float r_norm = (float)r / c;
    float g_norm = (float)g / c;
    float b_norm = (float)b / c;

    uint8_t r_byte = (uint8_t)(r_norm * 255.0);
    uint8_t g_byte = (uint8_t)(g_norm * 255.0);
    uint8_t b_byte = (uint8_t)(b_norm * 255.0);

    ColorConverter::RgbToHsv(r_byte, g_byte, b_byte, hue, saturation, value);
    hue *= 360;

    // MODIFICACIÓN: Solo aplicar espera si ya se detectó al menos una línea
    bool puedeDetectar = (contadorLineas == 0) || (millis() - tiempoUltimaDeteccion > tiempoEsperaDeteccion);
    
    if (puedeDetectar) {
      bool esBlanco = (value > 0.85 && saturation < 0.15);
      float proporcionAzul = b_norm / (r_norm + g_norm + 0.001);
      
      bool azulDetectado = false;
      bool naranjaDetectado = false;
        
      // Detección azul
     // Detección azul
    if (hue >= HUE_AZUL_MIN && hue <= HUE_AZUL_MAX &&
        saturation > SATURACION_AZUL_MIN &&
        value > VALOR_AZUL_MIN && value < VALOR_AZUL_MAX &&
        proporcionAzul > PROPORCION_AZUL_MIN &&
        !esBlanco) {
        if (confirmarColor(HUE_AZUL_MIN, HUE_AZUL_MAX, SATURACION_AZUL_MIN, VALOR_AZUL_MIN, VALOR_AZUL_MAX, CONFIRMAR_AZUL_TOLERANCIA, CONFIRMAR_AZUL_DELAY)) {
             
          azulDetectado = true;
          
          pausarCentrado = true;
          tiempoInicioDeteccion = millis();
          
          if (primeraDeteccion) {
            sentidoVehiculo = IZQUIERDA;
            primeraDeteccion = false;
          }
        }
      }
      // Detección naranja
      else if (((hue >= HUE_NARANJA_MIN && hue <= HUE_NARANJA_MAX)) &&
          saturation > SATURACION_NARANJA_MIN &&
          value > VALOR_NARANJA_MIN && value < VALOR_NARANJA_MAX &&
          !esBlanco) {
        if (confirmarColor(HUE_NARANJA_MIN, HUE_NARANJA_MAX, SATURACION_NARANJA_MIN, VALOR_NARANJA_MIN, VALOR_NARANJA_MAX, CONFIRMAR_NARANJA_TOLERANCIA, CONFIRMAR_NARANJA_DELAY)) {
          naranjaDetectado = true;
          
          pausarCentrado = true;
          tiempoInicioDeteccion = millis();
          
          if (primeraDeteccion) {
            sentidoVehiculo = DERECHA;
            primeraDeteccion = false;
          }
        }
      }

      // Al detectar línea, verificar si puede interrumpir esquive
      if (azulDetectado || naranjaDetectado) {
        
        // INTERRUMPIR ESQUIVE DIAGONAL si está activo
        if (esquiveDiagonalActivo) {
          resetearEsquiveDiagonal();
          resetearPIDCentrado(&pidCentrado);
          estadoActual = ESTADO_NORMAL;
        }
        
        // Ejecutar giro normal según el sentido del vehículo
        if (azulDetectado && sentidoVehiculo == IZQUIERDA) {
          //  LÓGICA DE INTERRUPCIÓN: Si estamos en una maniobra de carril, la reseteamos
          if (estadoActual == GIRANDO_HACIA_CARRIL || estadoActual == ENDEREZANDO_CARRIL || estadoActual == MANTENIENDO_CARRIL) {
            resetSistema(true); // Resetea la lógica de carriles conservando el ángulo
          }
          // Destello azul por 300ms (No bloqueante)
          encenderLedTemporal(0, 0, 255, 300);
          contadorLineas++;
          Serial.println(">>> LÍNEA DETECTADA (AZUL) — Contador: " + String(contadorLineas) + "/" + String(limiteLineas));

          // Al contar la primera línea, se desactiva el modo de inicio especial
          if (contadorLineas == 1 && !primerGiroRealizado) {
              primerGiroRealizado = true;
              Serial.println(">>> Primera línea detectada. El sistema operará de forma normal a partir de ahora.");
          }

          if (contadorLineas >= limiteLineas) {
            // Se activa la bandera para indicar que este es el último giro.
            giroParaEstacionamiento = true;
            resetearPIDCentrado(&pidCentrado);
            // Se realiza el giro a la izquierda (true) correspondiente a la línea azul.
            realizarGiro(true);
          } else {

            resetearPIDCentrado(&pidCentrado);
            realizarGiro(true); // Giro a la izquierda
          }
          
          apagarVehiculo();
          tiempoUltimaDeteccion = millis(); // Actualizar tiempo después de procesar la detección
        }
        else if (naranjaDetectado && sentidoVehiculo == DERECHA) {
          //  LÓGICA DE INTERRUPCIÓN: Si estamos en una maniobra de carril, la reseteamos
          if (estadoActual == GIRANDO_HACIA_CARRIL || estadoActual == ENDEREZANDO_CARRIL || estadoActual == MANTENIENDO_CARRIL) {
            resetSistema(true); // Resetea la lógica de carriles conservando el ángulo
          }
          // Destello naranja por 300ms (No bloqueante)
          encenderLedTemporal(255, 128, 0, 300);
          contadorLineas++;
          Serial.println(">>> LÍNEA DETECTADA (NARANJA) — Contador: " + String(contadorLineas) + "/" + String(limiteLineas));

           // Al contar la primera línea, se desactiva el modo de inicio especial
          if (contadorLineas == 1 && !primerGiroRealizado) {
              primerGiroRealizado = true;
              Serial.println(">>> Primera línea detectada. El sistema operará de forma normal a partir de ahora.");
          }

           if (contadorLineas >= limiteLineas) {
            // Se activa la bandera para indicar que este es el último giro.
            giroParaEstacionamiento = true;
            resetearPIDCentrado(&pidCentrado);
            // Se realiza el giro a la derecha (false) correspondiente a la línea naranja.
            realizarGiro(false);
          }else {
            resetearPIDCentrado(&pidCentrado);
            realizarGiro(false); // Giro a la derecha
          }
          
          apagarVehiculo();
          tiempoUltimaDeteccion = millis(); // Actualizar tiempo después de procesar la detección
        }
        else if (naranjaDetectado && sentidoVehiculo == IZQUIERDA) {
          // Línea naranja detectada pero el vehículo va hacia la izquierda
          // Parpadeo de advertencia
          for (int i = 0; i < 2; i++) {
            colorVehiculo(255, 128, 0); // Naranja
            delay(100);
            apagarVehiculo();
            delay(100);
          }
        }
      }
    }
  }
}

// ==================== FUNCIÓN LEER SENSOR SHARP ====================
float leerSensorSharp(int pin) {
  // Leer valor analógico del sensor Sharp
  int valorAnalogico = analogRead(pin);
  
  // Convertir a voltaje (asumiendo 3.3V de referencia del ESP32)
  float voltaje = valorAnalogico * (3.3 / 4095.0);
  
  // Conversión aproximada para sensor Sharp GP2Y0A21YK0F (10–80 cm)
  // La fórmula puede requerir ajuste fino con pruebas reales
  if (voltaje < 0.3) return 999; // Fuera de rango útil
  
  float distancia = 27.728 / (voltaje - 0.1696);
  
  // Limitar rango válido
  if (distancia < 10 || distancia > 80) return 999;
  
  return distancia;
}
// ==================== FUNCIÓN VERIFICAR OBSTÁCULO DIAGONAL ====================

bool verificarObstaculoDiagonal() {
  // No verificar si ya está en esquive diagonal o retroceso, o muy reciente
  if (esquiveDiagonalActivo || 
      prioridadActual == PRIORIDAD_RETROCESO ||
      millis() - tiempoUltimaDeteccionSharp < TIEMPO_ESPERA_SHARP) {
    return false;
  }
  
  // Leer ambos sensores Sharp
  float distanciaIzq = leerSensorSharp(SHARP_DIAGONAL_IZQ_PIN);
  float distanciaDer = leerSensorSharp(SHARP_DIAGONAL_DER_PIN);
  
  // Verificar obstáculo en diagonal izquierda
  if (distanciaIzq <= DISTANCIA_SHARP_EMERGENCIA && distanciaIzq > 0) {
    esquiveDiagonalActivo = true;
    direccionEsquiveDiagonal = 'D'; // Esquivar hacia la derecha
    tiempoInicioEsquiveDiagonal = millis();
    tiempoUltimaDeteccionSharp = millis();
    etapaEsquiveDiagonal = 0;
    return true;
  }
  
  // Verificar obstáculo en diagonal derecha
  if (distanciaDer <= DISTANCIA_SHARP_EMERGENCIA && distanciaDer > 0) {
    esquiveDiagonalActivo = true;
    direccionEsquiveDiagonal = 'I'; // Esquivar hacia la izquierda
    tiempoInicioEsquiveDiagonal = millis();
    tiempoUltimaDeteccionSharp = millis();
    etapaEsquiveDiagonal = 0;
    return true;
  }
  
  return false;
}

// ==================== FUNCIÓN PROCESAR ESQUIVE DIAGONAL ====================

void procesarEsquiveDiagonal() {
  // Verificar si ha sido interrumpido por una línea
  if (interrupcionCruce) {
    resetearEsquiveDiagonal();
    return;
  }
  if (!esquiveDiagonalActivo) {
    return;
  }
  
  unsigned long tiempoTranscurrido = millis() - tiempoInicioEsquiveDiagonal;
  
  switch(etapaEsquiveDiagonal) {
    case 0: // Orientar servo para cruce diagonal
      if (direccionEsquiveDiagonal == 'D') {
        direccion.write(SERVO_RIGHT); // Cruzar hacia la derecha
      } else if (direccionEsquiveDiagonal == 'I') {
        direccion.write(SERVO_LEFT); // Cruzar hacia la izquierda
      }
      
      // Asegurar que el motor esté activo
      avanzarMotor();
      
      etapaEsquiveDiagonal = 1;
      tiempoInicioEsquiveDiagonal = millis();
      break;
      
    case 1: // Ejecutar cruce diagonal
      if (tiempoTranscurrido > TIEMPO_CRUCE_DIAGONAL) {
        etapaEsquiveDiagonal = 2;
        tiempoInicioEsquiveDiagonal = millis();
      }
      break;
      
    case 2: // Centrar servo y finalizar
      direccion.write(SERVO_CENTRADO);
      
      // Finalizar esquive diagonal
      esquiveDiagonalActivo = false;
      direccionEsquiveDiagonal = 'N';
      etapaEsquiveDiagonal = 0;
      
      // Resetear PID de centrado para retomar navegación normal
      // (Si tienes sistema PID de centrado)
      // resetearPIDCentrado(&pidCentrado);
      break;
  }
}
void resetearEsquiveDiagonal() {
  //direccion.write(SERVO_CENTRADO);
  esquiveDiagonalActivo = false;
  direccionEsquiveDiagonal = 'N';
  etapaEsquiveDiagonal = 0;
  prioridadActual = PRIORIDAD_NORMAL;
  //estadoActual = ESTADO_NORMAL;
  //Reactivar la navegación normal ---
  enGiro = false;
  giroEnProceso = false;
  cruceEnProceso = false;
}

bool verificarRetrocesoEmergencia() {
  // NO verificar si ya está en retroceso o si no ha pasado suficiente tiempo
  if (retrocesoPorEmergencia || 
      millis() - tiempoUltimaVerificacionRetroceso < INTERVALO_VERIFICACION_RETROCESO) {
    return false;
  }
  
  tiempoUltimaVerificacionRetroceso = millis();
  
  // Medir distancia frontal
  float distanciaFrontal = leerToF('F');
  
  // Verificar si hay obstáculo muy cerca y no está ya en retroceso
  if (distanciaFrontal <= DISTANCIA_RETROCESO_EMERGENCIA && 
    distanciaFrontal > 0 && distanciaFrontal < 200) {
    
    // Activar retroceso de emergencia
    retrocesoPorEmergencia = true;
    motorRetrocesoConfigurado = false;  // RESETEAR FLAG AQUÍ
    tiempoInicioRetroceso = millis();
    prioridadActual = PRIORIDAD_RETROCESO;  // MÁXIMA PRIORIDAD
    
    Serial.println("ACTIVANDO RETROCESO DE EMERGENCIA");
    
    return true;
  }
  
  return false;
}
void procesarRetrocesoEmergencia() {
  if (!retrocesoPorEmergencia) {
    return;
  }
  
  unsigned long tiempoTranscurrido = millis() - tiempoInicioRetroceso;
  
  // Verificar timeout de seguridad
  if (tiempoTranscurrido > TIMEOUT_RETROCESO) {
    Serial.println("TIMEOUT DE RETROCESO - FINALIZANDO FORZADAMENTE");
    
    // Timeout alcanzado - forzar finalización
    detenerMotor(); // Apagar motor de retroceso
    delay(100);
    avanzarMotor();           // Reactivar motor principal
    apagarVehiculo();              // Apagar LED
    
    // RESETEAR TODAS LAS VARIABLES
    retrocesoPorEmergencia = false;
    motorRetrocesoConfigurado = false;
    prioridadActual = PRIORIDAD_NORMAL;
    estadoActual = ESTADO_NORMAL;
    
    delay(TIEMPO_ESPERA_POST_RETROCESO);
    return;
  }
  
  if (tiempoTranscurrido < TIEMPO_RETROCESO) {
    // FASE DE RETROCESO ACTIVO
    
    // Configurar motores solo una vez por ciclo
    if (!motorRetrocesoConfigurado) {
     // //ln("Configurando motores para retroceso...");
      
      detenerMotor();           // APAGAR motor principal COMPLETAMENTE
      delay(150);                              // Pausa para asegurar parada
      retrocederMotor();  // ACTIVAR motor de retroceso
      direccion.write(SERVO_CENTRADO);         // Centrar servo para retroceso recto
      
      motorRetrocesoConfigurado = true;
     // //ln("Motores configurados - Retrocediendo...");
    }
    
    // Parpadeo rápido del LED para indicar retroceso
    if ((tiempoTranscurrido / 150) % 2 == 0) {
      colorVehiculo(255, 0, 0); // Rojo de emergencia
    } else {
      apagarVehiculo();
    }
    
  } else {
    // FASE DE FINALIZACIÓN
    ////ln("Finalizando retroceso...");
    
    detenerMotor(); // Apagar motor de retroceso PRIMERO
    delay(200);                              // Pausa para que se detenga completamente
    
    // Verificar distancia SOLO al final
    float distanciaFinal = leerToF('F');
    ////("Distancia final tras retroceso: ");
    ////ln(distanciaFinal);
    
    if (distanciaFinal > DISTANCIA_RETROCESO_EMERGENCIA + 5 || distanciaFinal > 100) {
      // Hay suficiente espacio O sensor fuera de rango (espacio libre)
      avanzarMotor();           // Reactivar motor principal
      apagarVehiculo();              // Apagar LED
      // Se resetea el búfer para ignorar comandos recibidos durante la emergencia.
      bufferIndex = 0;
      buffer[0] = '\0';
      // RESETEAR TODAS LAS VARIABLES CORRECTAMENTE
      retrocesoPorEmergencia = false;
      motorRetrocesoConfigurado = false;  // IMPORTANTE: Resetear para próxima vez
      prioridadActual = PRIORIDAD_NORMAL;
      estadoActual = ESTADO_NORMAL;
      
      ////ln("Retroceso completado exitosamente - Reanudando marcha normal");
      delay(TIEMPO_ESPERA_POST_RETROCESO);
      
    } else {
      // AÚN hay obstáculo - reiniciar ciclo de retroceso
     Serial.println("Obstáculo aún presente - Reiniciando ciclo de retroceso");
      
      // RESETEAR PARA NUEVO CICLO
      motorRetrocesoConfigurado = false;  // PERMITIR NUEVA CONFIGURACIÓN
      tiempoInicioRetroceso = millis();   //  TIEMPO DE INICIO
      
      // Mantener retrocesoPorEmergencia = true para continuar
    }
  }
}


// Función adicional para verificar si el sistema puede ejecutar operaciones
bool sistemaListo() {
  return sistemaActivo && sistemaIniciado;
}

// Modificación para tu función setup() - agregar al final
void configurarInterruptor() {
  pinMode(INTERRUPTOR_PIN, INPUT_PULLUP);  // Configurar con pull-up interno
  sistemaActivo = false;  // Sistema inicia desactivado
  sistemaIniciado = false;
  
  // Si el interruptor ya está presionado al encender, iniciar automáticamente
  if (digitalRead(INTERRUPTOR_PIN) == LOW) {
    delay(100);  // Pequeña pausa para estabilizar la lectura
    if (digitalRead(INTERRUPTOR_PIN) == LOW) {
      iniciarSistema();
      //delay(1500);
    }
  } else {
    esperarInterruptor();  // Si no está presionado, esperar
  }
}
//----------------- FIN Funciones de evasion y recepcion de serial---------------

// Función que debes llamar en tu loop principal
void verificarArranqueMotor() {
  // Solo verificar si el sistema está activo y el motor no ha arrancado aún
  if (sistemaActivo && !motorArrancado) {
    // Verificar si ha pasado el tiempo de delay
    if (millis() - tiempoInicioSistema >= DELAY_ARRANQUE_MOTOR) {
      // Arrancar el motor
      avanzarMotor(); // Activar motor (LOW para avanzar según tu configuración)
      motorArrancado = true;
      Serial.println(F("Tiempo de espera serial agotado. Arrancando motor..."));
      // //ln(F("Motor arrancado"));
    }
  }
}
void iniciarConSalidaEstacionamiento() {
  // --- Paso 1: Esperar la activación del interruptor ---
  Serial.println("Esperando activación del interruptor (LOW) para iniciar...");
  bool ledState = false;
  unsigned long ultimoParpadeo = 0;
  while (digitalRead(INTERRUPTOR_PIN) == HIGH) {
    unsigned long ahora = millis();
    if (ahora - ultimoParpadeo > 500) {
      ledState = !ledState;
      colorVehiculo(255, 100, 0);
      ultimoParpadeo = ahora;
    }
    delay(10);
  }

  // --- Paso 2: Medir distancias laterales para decidir la dirección de salida ---
  colorVehiculo(255, 0, 255); // LED fijo para indicar que está procesando
  Serial.println("Interruptor activado. Midiendo distancias laterales para decidir la maniobra...");
  
  // Medir distancia de ambos sensores
  float distDer = leerToF('D');
  delay(50); // Pequeña pausa entre lecturas para evitar interferencias
  float distIzq = leerToF('I');

  Serial.println("-> Distancia Derecha: " + String(distDer) + " cm, Izquierda: " + String(distIzq) + " cm");

  // --- Paso 3: Decidir la dirección de la maniobra ---
  bool salirHaciaIzquierda; // true para la maniobra original, false para la invertida

  // Lógica de decisión: salir por el lado con MÁS espacio.
  // Si el sensor izquierdo mide una distancia MENOR, significa que la pared está a la izquierda,
  // por lo que debemos salir por la derecha (maniobra invertida).
  if (distIzq < distDer) {
      salirHaciaIzquierda = false; // Hay menos espacio a la izquierda, salimos por la derecha.
      Serial.println(">> Decisión: Menor distancia a la izquierda. Se ejecutará la salida hacia la DERECHA (invertida).");
  } else {
      salirHaciaIzquierda = true; // Menor distancia a la derecha (o son iguales), salimos por la izquierda (original).
      Serial.println(">> Decisión: Menor distancia a la derecha. Se ejecutará la salida hacia la IZQUIERDA (original).");
  }
  
  // --- Paso 4: Iniciar el sistema y ejecutar la maniobra seleccionada ---
  iniciarSistema(); 
  ejecutarSalidaEstacionamiento(salirHaciaIzquierda);
}

void setup() {
  //analogWrite(BUZZER_PIN, 160);
  //delay(100);
  //analogWrite(BUZZER_PIN, 0);
  direccion.write(SERVO_CENTRADO);
  Serial.begin(115200);
  Serial.setTimeout(10);

    /// Iniciar ambos buses I2C
  Wire.begin(); // Pines por defecto para BNO055 y ToFs
  I2CSegundo.begin(I2C2_SDA, I2C2_SCL);
  Wire.setClock(400000);       // Acelera el bus 1 a 400kHz
  I2CSegundo.setClock(400000); // Acelera el bus 2 a 400kHz

  // --- Iniciar NeoPixels ---
  tiraLED.begin();
  tiraLED.setBrightness(50); // Ajusta el brillo de 0 a 255 (50 es seguro para no consumir demasiada corriente)
  apagarVehiculo();          // Asegurar que inicien apagados

  Serial.println(F("\n--- INICIANDO DIAGNÓSTICO DE SENSORES ---"));
  
  // 1. Configuración y Apagado inicial de ToFs
  pinMode(XSHUT_FRONTAL, OUTPUT); pinMode(XSHUT_TRASERO, OUTPUT);
  pinMode(XSHUT_IZQ, OUTPUT); pinMode(XSHUT_DER, OUTPUT);
  
  digitalWrite(XSHUT_FRONTAL, LOW); digitalWrite(XSHUT_TRASERO, LOW);
  digitalWrite(XSHUT_IZQ, LOW); digitalWrite(XSHUT_DER, LOW);
  delay(100);

  // 2. Encendido secuencial y verificación de ToFs
  Serial.print(F("Iniciando ToF Frontal (0x30)... "));
  digitalWrite(XSHUT_FRONTAL, HIGH); delay(50);
  if (!tofF.begin(ADDR_FRONTAL)) {
    Serial.println(F("FALLO. Verifica cableado o pin 17."));
    while (1); // Bucle infinito: detiene el robot si falla
  }
  Serial.println(F("OK"));

  Serial.print(F("Iniciando ToF Trasero (0x31)... "));
  digitalWrite(XSHUT_TRASERO, HIGH); delay(50);
  if (!tofT.begin(ADDR_TRASERO)) {
    Serial.println(F("FALLO. Verifica cableado o pin 16."));
    while (1);
  }
  Serial.println(F("OK"));

  Serial.print(F("Iniciando ToF Izquierdo (0x32)... "));
  digitalWrite(XSHUT_IZQ, HIGH); delay(50);
  if (!tofI.begin(ADDR_IZQ)) {
    Serial.println(F("FALLO. Verifica cableado o pin 5."));
    while (1);
  }
  Serial.println(F("OK"));

  Serial.print(F("Iniciando ToF Derecho (0x33)...   "));
  digitalWrite(XSHUT_DER, HIGH); delay(50);
  if (!tofD.begin(ADDR_DER)) {
    Serial.println(F("FALLO. Verifica cableado o pin 18."));
    while (1);
  }
  Serial.println(F("OK"));

  // Configurar pin del interruptor con pull-up interno
  pinMode(INTERRUPTOR_PIN, INPUT_PULLUP);
  
// 3. Iniciar Sensor de Color en Bus 2
  Serial.print(F("Iniciando Color TCS34725 (Bus 2)... "));
  if (tcs.begin(TCS34725_ADDRESS, &I2CSegundo)) {
    Serial.println(F("OK"));
  } else {
    Serial.println(F("FALLO sensor de color. Verifica los pines 32 y 33."));
    while (1);
  }
  /*************/
 // 4. Iniciar BNO055
  Serial.print(F("Iniciando IMU BNO055 (Bus 1)...     "));
  if (bno.begin()) {
    Serial.println(F("OK"));
    bno.setExtCrystalUse(true);
    bno.setMode(OPERATION_MODE_IMUPLUS);
    applyCalibrationOffsets();
    delay(100); // Dar tiempo al sensor para estabilizarse en el nuevo modo
    
    // Lecturas de calentamiento
    for (int i = 0; i < 5; i++) { 
      bno.getQuat(); 
      delay(10); 
    } 
    //resetYaw(); // Fija el frente del vehículo como 0°
  } else {
    Serial.println(F("FALLO bno055. Verifica cableado o el pin ADR a GND."));
    while (1);
  }

  Serial.println(F("--- TODOS LOS SENSORES LISTOS ---"));

  direccion.setPeriodHertz(50); // Le decimos explícitamente que trabaje a 50Hz
  direccion.attach(SERVO_PIN, 500, 2400); // Mismos anchos de pulso que usaste en la prueba
 
  
  //*******************************

  direccion.attach(SERVO_PIN);
  // Configurar pines como salidas digitales
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  // 1. Configurar PWM del Motor (Nueva sintaxis ESP32 v3)
  ledcAttach(MOTOR_ENA, freqMotor, resolucionPWM);

  // 2. Configurar PWM del Buzzer (Nueva sintaxis ESP32 v3)
  ledcAttach(BUZZER_PIN, 2000, resolucionPWM); 
  noTone(BUZZER_PIN); // Asegurar silencio inicial

  // Inicializar el motor detenido de forma segura
  detenerMotor();

 // Inicializar variables de motor
  tiempoInicioSistema = 0;
  motorArrancado = false;
  
  // Inicializar variables de interruptor
  interruptorAnterior = digitalRead(INTERRUPTOR_PIN);
  tiempoDebounce = 0;
  sistemaActivo = false;
  sistemaIniciado = false;
  
  // Iniciar período de evaluación para retroceso post-giro
  tiempoInicioEvaluacion = millis();
  
   // Inicializar variables de memoria de color
  ultimoColorDetectado = 'N';
  tiempoUltimoColor = 0;
  bufferIndex = 0;
  lastOrientation = 'C';
  // Calibración inicial
  ////ln(F("Calibrando..."));
  // //(F("Offset: "));
  ////ln(offsetGz, 4);
  resetearPIDCentrado(&pidCentrado);
  configurarInterruptor(); // Inicia por interruptor físico (pin 23)

  //iniciarConSalidaEstacionamiento(); 
  ////ln(F("Sistema listo"));
  lastTime = millis();
  tiempoUltimaDeteccion = millis();
  ultimoTiempoCentrado = millis();

  // Crear la tarea para ejecutar la detección de color en el Núcleo 0
  xTaskCreatePinnedToCore(
    TareaDeteccionColor,    // Función que contiene la lógica de la tarea
    "TareaColor",           // Nombre descriptivo de la tarea
    4096,                   // Tamaño de la pila (Stack de 4KB es seguro para I2C)
    NULL,                   // Parámetros de entrada de la tarea
    1,                      // Prioridad de la tarea (1 es prioridad normal)
    &TareaColorHandle,      // Referencia del Handle declarado globalmente
    0                       // Código del núcleo: Núcleo 0
  );

}

void loop() {

  // PRIMERA PRIORIDAD: Verificar interruptor SIEMPRE
  verificarInterruptor();
  
  verificarArranqueMotor();

  // Si el sistema no está activo, no ejecutar nada más
  if (!sistemaActivo) {
    delay(50);  // Pequeña pausa para no saturar el procesador
    return;
  }
  
  // ────────────────────────────────────────────────────────────────────────
  
    // Procesar giro de apertura si está activo
  if (giroAperturaActivo) {
    procesarGiroApertura();
    return; // No ejecutar nada más mientras se procesa la apertura
  }
    // EJECUTAR SIEMPRE (independiente de comandos seriales)
  actualizarGiro();
  actualizarLedTemporal(); //  Vigila los LEDs sin bloquear
    // Detección de color se ejecuta en estados de navegación, no durante maniobras de carril
  // (La interrupción ya se maneja dentro de los casos del switch)
  /*if (estadoActual == ESTADO_NORMAL) {
      detectarColor();
  }*/
 /*if (estadoActual != ESTADO_DETENCION_FINAL){
    detectarColor();
  }*/
   // MÁQUINA DE ESTADOS - Solo ejecutar si no hay procesos de alta prioridad
  switch(estadoActual) {
    case ESTADO_NORMAL:
      //centrarVehiculo();
           if (!enGiro && 
          prioridadActual == PRIORIDAD_NORMAL && 
          !esquiveDiagonalActivo && 
          !retrocesoPorEmergencia &&
          !retrocesoOrientadoActivo) { 
        centrarVehiculo();
      }
      break;
    case ESPERANDO_COMANDO_CARRIL:
      detenerMotor();

      // Buffer acumulador para armar el comando carácter a carácter
      while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
          if (bufferIndex > 0) {
            buffer[bufferIndex] = '\0';
            comandoRecibido = String(buffer);
            comandoListo = true;
            bufferIndex = 0;
          }
        } else if (bufferIndex < BUFFER_SIZE - 1) {
          buffer[bufferIndex++] = c;
        }
      }

      if (comandoListo) {
        comandoListo = false;
        comandoRecibido.trim();

        comandoRecibido.toUpperCase();
        if (comandoRecibido.startsWith("C") && comandoRecibido.length() >= 3) {
          comandoRecibido = comandoRecibido.substring(0, 3);
          Serial.println("Comando aceptado: " + comandoRecibido);
          Serial.println("Comando aceptado: " + comandoRecibido);
          procesarComandoCarril(comandoRecibido);
        } else {
          Serial.println("Comando ignorado: " + comandoRecibido);
        }
      }

      // Reporte de sensores cada 1000ms (sin bloquear)
      static unsigned long ultimoReporteEspera = 0;
      if (millis() - ultimoReporteEspera > 1000) {
        float distF = leerToF('F');
        float distI = leerToF('I');
        float distD = leerToF('D');
        float distSharpIzq = leerSensorSharp(SHARP_DIAGONAL_IZQ_PIN);
        float distSharpDer = leerSensorSharp(SHARP_DIAGONAL_DER_PIN);
        Serial.print("⏸️ ESTADO ESPERA | ToF F: ");
        Serial.print(distF > 0 && distF < 999 ? String(distF, 1) + "cm" : "N/A");
        Serial.print(" | ToF I: ");
        Serial.print(distI > 0 && distI < 999 ? String(distI, 1) + "cm" : "N/A");
        Serial.print(" | ToF D: ");
        Serial.print(distD > 0 && distD < 999 ? String(distD, 1) + "cm" : "N/A");
        Serial.print(" | Sharp Izq: ");
        Serial.print(distSharpIzq > 0 && distSharpIzq < 150 ? String(distSharpIzq, 1) + "cm" : "N/A");
        Serial.print(" | Sharp Der: ");
        Serial.println(distSharpDer > 0 && distSharpDer < 150 ? String(distSharpDer, 1) + "cm" : "N/A");
        ultimoReporteEspera = millis();
      }

      break;

     case EVALUANDO_POSICION:
    case GIRANDO_HACIA_CARRIL:
    case ENDEREZANDO_CARRIL:
    case MANTENIENDO_CARRIL:
      { 
        EstadoSistema estadoPrevio = estadoActual; // Guardar el estado actual

        // Durante todos los estados de carril, permitir interrupción por línea
        //detectarColor(); // 

        // Si detectarColor cambió el estado, significa que encontró una línea
        // y comenzó un giro. Debemos salir del switch INMEDIATAMENTE.
       /* if (estadoActual != estadoPrevio) {
            break; 
        }*/

        // Si no hubo cambio de estado, continuar con la lógica de carriles
        if (millis() - ultimaActualizacionf >= INTERVALO_CONTROLf) {
            float anguloServo = calcularControlCarriles(); // 
            if (anguloServo != -1) {
                direccion.write((int)anguloServo); // 
                
               if (millis() - ultimoReportef > INTERVALO_REPORTEf) {
                    float distSharpIzqR = leerSensorSharp(SHARP_DIAGONAL_IZQ_PIN);
                    float distSharpDerR = leerSensorSharp(SHARP_DIAGONAL_DER_PIN);
                    Serial.print("Servo: ");
                    Serial.print((int)anguloServo);
                    Serial.print(" | Carril: ");
                    Serial.print(carriles.carrilObjetivo);
                    Serial.print(" | Lado: ");
                    Serial.print(controlLateral.ladoControl);
                    Serial.print(" | Err Dist: ");
                    Serial.print(controlLateral.errorDistanciaAnterior, 2);
                    Serial.print(" | Err Ang: ");
                    float errorAngulo = controlLateral.anguloObjetivo - anguloZ;
                    if (errorAngulo > 180.0) errorAngulo -= 360.0;
                    else if (errorAngulo < -180.0) errorAngulo += 360.0;
                    Serial.print(errorAngulo, 1);
                    Serial.print(" | Sharp Izq: ");
                    Serial.print(distSharpIzqR > 0 && distSharpIzqR < 150 ? String(distSharpIzqR, 1) + "cm" : "N/A");
                    Serial.print(" | Sharp Der: ");
                    Serial.print(distSharpDerR > 0 && distSharpDerR < 150 ? String(distSharpDerR, 1) + "cm" : "N/A");
                    Serial.println();
                    ultimoReportef = millis();
                }
            }
            if (enFaseContragiro) {
                avanzarLento(); // Lento solo durante el contragiro de enderezado
            } else {
                avanzarMotor(); // Velocidad normal para todo lo demás
            }
            ultimaActualizacionf = millis();
        }
      } 
      break;
   /* float anguloServo = calcularControlCarriles();
    if (anguloServo != -1) {
        direccion.write((int)anguloServo);

        // ====== INICIO: NUEVO MONITOR DE DEPURACIÓN ======
        // Este bloque imprimirá solo cuando esté en el estado de mantenimiento de carril
        if (millis() - ultimoReportef > INTERVALO_REPORTEf) {
            Serial.print("Servo: ");
            Serial.print((int)anguloServo);
            Serial.print(" | Carril: ");
            Serial.print(carriles.carrilObjetivo);
            Serial.print(" | Lado: ");
            Serial.print(controlLateral.ladoControl);
            Serial.print(" | Err Dist: ");
            Serial.print(controlLateral.errorDistanciaAnterior, 2); // Muestra el último error de distancia calculado
            Serial.print(" | Err Ang: ");
            float errorAngulo = controlLateral.anguloObjetivo - anguloZ;
            if (errorAngulo > 180.0) errorAngulo -= 360.0;
            else if (errorAngulo < -180.0) errorAngulo += 360.0;
            Serial.print(errorAngulo, 1);
            Serial.println();
            ultimoReportef = millis();
        }
        // ====== FIN: NUEVO MONITOR DE DEPURACIÓN ======
    }
    avanzarMotor();
    ultimaActualizacionf = millis();
    }
      break;
*/
    case ESTADO_CORRECCION_LATERAL:
      ejecutarCorreccionDistanciaLateral();
      break;
    case ESTADO_DETENCION_FINAL:
     if (contadorEntradaEstadoFinal >= 1){
      /*  analogWrite(BUZZER_PIN, 255);
        delay(50);
        analogWrite(BUZZER_PIN, 0);
        delay(50);
        analogWrite(BUZZER_PIN, 255);
        delay(50);
        analogWrite(BUZZER_PIN, 0);*/
        contadorEntradaEstadoFinal += 1; 
      }
      // Avanzar en línea recta durante 2.5 segundos
      if (millis() - tiempoInicioDetencionFinal < 1500) {
        if (!enGiro && 
          prioridadActual == PRIORIDAD_NORMAL) {
        centrarVehiculo();

        //Serial.println("normal");
          }
        
        // Debug cada segundo
       /* static unsigned long ultimoDebug = 0;
        if (millis() - ultimoDebug > 1000) {
          //("Avance final - Tiempo restante: ");
          //(2500 - (millis() - tiempoInicioAvanceFinal));
          //ln(" ms");
          ultimoDebug = millis();
        }*/
        
      } else {
        //ln("Avance final completado - Motor detenido");
        detenerRobot();
       // estadoActual = ESTADO_NORMAL; // Opcional: volver a estado normal
        
      }
      break;
      
  }  

    
  // MÁXIMA PRIORIDAD: Retroceso de emergencia (SOLO si NO está girando)
  if (prioridadActual == PRIORIDAD_RETROCESO && !giroEnProceso && !retrocesoOrientadoActivo && estadoActual !=ESTADO_DETENCION_FINAL){
    if (retrocesoPorEmergencia) {
      //procesarRetrocesoEmergencia();
    }
    return;
  }
  
  // ALTA PRIORIDAD: Retroceso post-giro
  if (procesarRetrocesoPostGiro()) {
    // El retroceso está activo, no ejecutar nada más
    return;
  }
    // Verificar obstáculos diagonales SOLO si NO está girando
    //if (!esquiveDiagonalActivo && !giroEnProceso && verificarObstaculoDiagonal()) {
      // Se activó esquive diagonal
    //}

    // Procesar esquive diagonal si está activo Y NO está girando
    if (esquiveDiagonalActivo && !giroEnProceso) {
      procesarEsquiveDiagonal();
    }
   
  // Mostrar el valor cada 300 ms
  if (millis() - lastYawPrint >= YAW_INTERVAL) {
        Serial.print("Yaw: ");
        //Serial.print(yawFiltrado);
        Serial.print(anguloZ);
        Serial.println(anguloZ);
        Serial.print("°  ");
        lastYawPrint = millis();
        Serial.println("Estado actual: " + obtenerNombreEstado(estadoActual));
        
    }

  delay(20);
}


