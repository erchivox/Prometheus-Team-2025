#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <ESP32Servo.h>
#include <ColorConverterLib.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

// --- Agrega esto en la parte superior, con tus otras librerías ---
#include <Adafruit_NeoPixel.h>

#define NEOPIXEL_PIN 14 // Usaremos el mismo pin que tenía tu LED normal
#define NUMPIXELS 10     // Cantidad de LEDs (si le pones una tira pequeña, cambia este número)

// Crear el objeto NeoPixel
Adafruit_NeoPixel strip(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
// ------------------------------------------------------------------

// --- Definición de Pines XSHUT para ToF ---
#define XSHUT_FRONTAL 17
#define XSHUT_TRASERO 16
#define XSHUT_IZQ     5
#define XSHUT_DER     18

// --- Pines para el Segundo Bus I2C (Color) ---
#define I2C2_SDA 32
#define I2C2_SCL 33
TwoWire I2CSegundo = TwoWire(1);

// --- Direcciones I2C para los ToF ---
#define ADDR_FRONTAL 0x30
#define ADDR_TRASERO 0x31
#define ADDR_IZQ     0x32
#define ADDR_DER     0x33

// --- Creación de Objetos ---
Adafruit_VL53L0X tofF = Adafruit_VL53L0X();
Adafruit_VL53L0X tofT = Adafruit_VL53L0X();
Adafruit_VL53L0X tofI = Adafruit_VL53L0X();
Adafruit_VL53L0X tofD = Adafruit_VL53L0X();

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_2_4MS, TCS34725_GAIN_1X);
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);

const int INTERRUPTOR_PIN = 23;  // Pin digital para el interruptor (usa pin con interrupt)

// --- Variables BNO055 ---
float yaw_offset = 0.0f;

Servo direccion;

// --- Configuración de Pines del Motor (Puente H) ---
const int MOTOR_IN1 = 27; // Pin de dirección (Avance)
const int MOTOR_IN2 = 26; // Pin de dirección (Retroceso)
const int MOTOR_ENA = 25;  // Pin PWM para la velocidad - 

// --- Variables de Velocidad (rango de 0 a 255) ---
int velocidadBase = 255;      // Velocidad normal para avanzar
int velocidadRetroceso = 150; // Velocidad para echar hacia atrás

// --- Configuración de PWM para ESP32 ---
// --- Configuración de Canales PWM Independientes para ESP32 ---
const int canalMotor  = 4;  // Lo alejamos de los canales del servo
const int canalBuzzer = 5;
const int canalLED    = 6;// Reservado por si quieres atenuar el LED luego

const int freqMotor = 1000;
const int resolucionPWM = 8;  // Resolución de 8 bits (0-255)

const int SERVO_PIN = 13;
// Declaraciones de pines (usar pines analógicos como digitales)
#define BUZZER_PIN  4  // Pin analógico A1 usado como digital para buzzer

//******************** colores 
// Variables para la detección del color azul
const int HUE_AZUL_MIN = 185;
const int HUE_AZUL_MAX = 238;
const float SATURACION_AZUL_MIN = 0.40;
const float VALOR_AZUL_MIN = 0.30;
const float VALOR_AZUL_MAX = 0.70;
const float PROPORCION_AZUL_MIN = 1.1;//1.66
const int CONFIRMAR_AZUL_TOLERANCIA = 1;
const int CONFIRMAR_AZUL_DELAY = 1;
  
// Variables para la detección del color naranja
const int HUE_NARANJA_MIN = 8;
const int HUE_NARANJA_MAX = 45;
const float SATURACION_NARANJA_MIN = 0.40;
const float VALOR_NARANJA_MIN = 0.34;
const float VALOR_NARANJA_MAX = 0.72;
const int CONFIRMAR_NARANJA_TOLERANCIA = 1;
const int CONFIRMAR_NARANJA_DELAY = 1;
//*****888*******************************

float distanciaTraseraInicial = 0.0;

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

const int SERVO_CENTRADO = 90;

int contadorEntradaEstadoFinal = 0;

// Contador de líneas
int contadorLineas = 0;
int limiteLineas = 12;

int16_t ax, ay, az, gx, gy, gz;

float anguloZ = 0.0;        // Ángulo Yaw (giro alrededor del eje Z)
float pitch = 0.0;          // Ángulo Pitch (inclinación adelante/atrás)
float roll = 0.0;           // Ángulo Roll (inclinación lateral)
float anguloObjetivoGlobal = 0.0; // angulo recto para el giro


  float distanciaDerecha= 0 ;
    float distanciaIzquierda = 0 ;

// Factores de peso para el filtro complementario
// K_ACC: Peso del acelerómetro (0.0 a 1.0). Un valor pequeño es común.
// K_GYRO: Peso del giroscopio (1.0 - K_ACC).
const float K_ACC = 0.02; // Puedes ajustar este valor. Más alto = más influencia del acelerómetro, más bajo = más influencia del giroscopio.
const float K_GYRO = 1.0 - K_ACC;

// Ángulos de giro modificables para cada sentido
float giroObjetivoAzul = 90.0;    // Giro a la izquierda para línea azul
float giroObjetivoNaranja = 90.0; // Giro a la derecha para línea naranja (modificable)


float toleranciaGiro = 2.0;

//mantener este timeout o aumentarlo para que termine el giro correctamente.
unsigned long lastTime, startTime, timeoutGiro = 5500;//2500;

unsigned long lastYawPrint = 0; // Para el intervalo de impresión del Yaw
const long YAW_INTERVAL = 300; // Intervalo de impresión en ms

// Variables para control de centrado
bool enGiro = false;
unsigned long tiempoFinGiro = 0;
const unsigned long tiempoEsperaPosgiro = 0;
const int anguloCorreccionMax = 40;// normal 50 16/07/2025
const unsigned long intervaloCentrado = 100;

unsigned long tiempofinalManiobraGiroExterna = 0;
const unsigned long intervaloCentradoCorreccionCruce = 600;

unsigned long ultimoTiempoCentrado = 0;

// Variables para pausar centrado durante detección
bool pausarCentrado = false;
unsigned long tiempoInicioDeteccion = 0;
const unsigned long tiempoPausaCentrado = 100;

// Variables para detección de líneas
bool lineaNaranjaDetectada = false;
unsigned long tiempoUltimaDeteccion = 0;
const unsigned long tiempoEsperaDeteccion = 500;
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
volatile bool giroEnProceso = false; // "volatile" hace que los 2 núcleos la lean en tiempo real
const float TOLERANCIA_PID = 2.0;      // Tolerancia más estricta con PID

// ==================== VARIABLES PID PARA CENTRADO ====================

// ==================== CONFIGURACIÓN DEL ESCUDO LATERAL (ToF) ====================
const float UMBRAL_REPULSION = 20.0;//14.0; // cm: Distancia mínima permitida antes de que el ToF intervenga
const float K_REPULSION = 3.5;       // Multiplicador: Cuántos grados gira el servo por cada cm que invade el umbral
// ================================================================================

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
  .kp = 2.0,          // Kp moderado para corrección angular suave
  .ki = 0.05,         // Ki muy bajo (el ángulo no suele requerir tanta acumulación)
  .kd = 1.2,          // Kd alto para reaccionar rápido si el robot se desvía de golpe
  .error_anterior = 0,
  .integral = 0,
  .integral_max = 15, 
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


//-------variables de evasion de objetos--------------

// Estados del sistema integrado
enum EstadoSistema {
  ESTADO_NORMAL,      // Navegación normal + detección de líneas + recepción comandos
  SIGUIENDO_OBJETO,   // Siguiendo objeto detectado por app
  ESQUIVANDO,         // Esquivando objeto
  ESTADO_DETENCION_FINAL
};
float distanciaFrontalInicial = 0.0;
const float TOLERANCIA_DISTANCIA_FINAL = 30.0;  // cm de tolerancia aceptable

// Variables para recepción serial optimizada (del código de esquive)
const int BUFFER_SIZE = 32;
char buffer[BUFFER_SIZE];
int bufferIndex = 0;

// Variables para parsing ultra rápido
char colorDetectado = 'N';
int distanciaObjeto = 0;
char orientacionObjeto = 'C';

// Variables de estado
EstadoSistema estadoActual = ESTADO_NORMAL;
char colorObjetivo = 'N';
unsigned long tiempoEsquive = 0;
int etapaEsquive = 0;
char lastOrientation = 'X';

// Configuración de distancias para esquive
const int DISTANCIA_ROJO = 40;//35;    // Distancia de esquive para objetos rojos
const int DISTANCIA_VERDE = 45;//40;   // Distancia de esquive para objetos verdes
const int DISTANCIA_EMERGENCIA = 13;
// Tiempos de esquive por color (en milisegundos)
const unsigned long TIEMPO_ESQUIVE_VERDE = 800;   // Tiempo para esquivar verde
const unsigned long TIEMPO_ESQUIVE_ROJO = 800;   // Tiempo para esquivar rojo
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

// Variables para esquive diagonal por sensores Sharp
bool esquiveDiagonalActivo = false;
unsigned long tiempoInicioEsquiveDiagonal = 0;
unsigned long tiempoUltimaDeteccionSharp = 0;
char direccionEsquiveDiagonal = 'N'; // 'I' = izquierda, 'D' = derecha
int etapaEsquiveDiagonal = 0;

// Configuración para sensores Sharp diagonales
const float DISTANCIA_SHARP_EMERGENCIA = 23.0;     // 10cm de detección
const unsigned long TIEMPO_CRUCE_DIAGONAL = 500;  // Tiempo de cruce diagonal
const unsigned long TIEMPO_ESPERA_SHARP = 80;     // Evitar detecciones múltiples

// Sistema de prioridades para gestión de emergencias
enum PrioridadEmergencia {
  PRIORIDAD_NORMAL,
  PRIORIDAD_RETROCESO,  // Máxima prioridad
  PRIORIDAD_ESQUIVE     // Prioridad normal
};
PrioridadEmergencia prioridadActual = PRIORIDAD_NORMAL;

// Variables mejoradas para retroceso de emergencia
const int DISTANCIA_RETROCESO_EMERGENCIA = 13;    // Distancia mínima para activar retroceso (cm)
const unsigned long TIEMPO_RETROCESO = 1500;     // Tiempo de retroceso en ms
const unsigned long TIMEOUT_RETROCESO = 3000;    // Timeout de seguridad para retroceso
bool retrocesoPorEmergencia = false;    
// Variables de control temporal
unsigned long tiempoInicioRetroceso = 0;          
unsigned long tiempoUltimaVerificacionRetroceso = 0; 
const unsigned long INTERVALO_VERIFICACION_RETROCESO = 50; // Verificación cada 100ms
const unsigned long TIEMPO_ESPERA_POST_RETROCESO = 100;     // Tiempo de espera después del retroceso
bool motorRetrocesoConfigurado = false;  // controlar configuración de motores


// Variables para control de retroceso post-giro
unsigned long tiempoInicioEvaluacion = 0;


bool sistemaActivo = false;  // Estado del sistema (activo/inactivo)

// Variable para controlar giro antes de estacionamiento
bool giroParaEstacionamiento = false;

// tiempo de finalizacion del codigo para avance final
unsigned long tiempoInicio = millis();
//unsigned long duracionCiclo = 3000; // 5 segundos en milisegundos

 // Añadir esta línea con las otras variables de estado/tiempo
unsigned long tiempoInicioDetencionFinal = 0;

// Variables globales para el control de arranque del motor
unsigned long tiempoInicioSistema = 0;
bool motorArrancado = false;
const unsigned long DELAY_ARRANQUE_MOTOR = 1000; // 1 segundo en milisegundos

void resetYaw() {
  // Limpiar lecturas antiguas para asegurar estabilidad
  for (int i = 0; i < 10; i++) {
    bno.getQuat();
    delay(10);
  }

  imu::Quaternion quat = bno.getQuat();
  double w = quat.w(), x = quat.x(), y = quat.y(), z = quat.z();
  double norm = sqrt(w*w + x*x + y*y + z*z);
  
  if (norm < 1e-6) return; // Evitar procesar si la lectura falla
  
  w /= norm; x /= norm; y /= norm; z /= norm;

  // Calcular el Yaw actual y guardarlo como el nuevo punto de referencia (0)
  float rawYaw = (float)(atan2(2.0*(w*z + x*y), 1.0 - 2.0*(y*y + z*z)) * 180.0 / M_PI);
  yaw_offset = rawYaw; 
  anguloZ = 0.0f;
  
  Serial.print("Yaw reseteado. Offset aplicado: ");
  Serial.println(yaw_offset);
}

// ========================================================================


// --- AGREGA ESTAS FUNCIONES ANTES DEL SETUP ---
// Función para encender la tira NeoPixel completa de un color específico (R, G, B)
void encenderNeoPixel(uint8_t r, uint8_t g, uint8_t b) {
  // Recorrer todos los LEDs desde el 0 hasta NUMPIXELS - 1
  for(int i = 0; i < NUMPIXELS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b)); 
  }
  strip.show(); // Mostrar el color en toda la tira una vez cargado
}

// Función para apagar el NeoPixel
void apagarNeoPixel() {
  strip.clear();
  strip.show();
}
// ----------------------------------------------

// ==================== FUNCIONES DE CONTROL DE MOTOR ====================
void detenerMotor() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  ledcWrite(MOTOR_ENA, 0); // Apagar el PWM del motor
}

void avanzarMotor(int velocidad = velocidadBase) {
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  ledcWrite(MOTOR_ENA, velocidad); // Aplica la intensidad al motor
}

void retrocederMotor(int velocidad = velocidadRetroceso) {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, HIGH); 
  ledcWrite(MOTOR_ENA, velocidad); // Aplica la intensidad al motor
}
// =======================================================================

// Devuelve la distancia en centímetros
float leerToF(char sensor) {
  VL53L0X_RangingMeasurementData_t measure;
  
  switch(sensor) {
    case 'F': tofF.rangingTest(&measure, false); break; // Frontal
    case 'T': tofT.rangingTest(&measure, false); break; // Trasero
    case 'I': tofI.rangingTest(&measure, false); break; // Izquierdo
    case 'D': tofD.rangingTest(&measure, false); break; // Derecho
    default: return 999.0;
  }

  if (measure.RangeStatus != 4) { // 4 significa fuera de rango
    return measure.RangeMilliMeter / 10.0; // Convertir de mm a cm
  } else {
    return 999.0; // Fuera de rango
  }
}

float medirDistanciaTraseraInicialConfiable() {
  Serial.println("Iniciando medicion trasera confiable...");
  float suma = 0;
  int medicionesValidas = 0;
  const int NUMERO_DE_INTENTOS = 5;

  for (int i = 0; i < NUMERO_DE_INTENTOS; i++) {
    float lecturaActual = leerToF('T');

    // Verificar si la lectura está dentro de un rango razonable
    if (lecturaActual > 90 && lecturaActual < 200) {
      suma += lecturaActual;
      medicionesValidas++;
      Serial.print("Intento ");
      Serial.print(i + 1);
      Serial.print(": Lectura valida = ");
      Serial.println(lecturaActual, 1);
    } else {
      Serial.print("Intento ");
      Serial.print(i + 1);
      Serial.println(": Lectura invalida, descartada.");
    }
    delay(50); // Pequeña pausa entre mediciones
  }

  if (medicionesValidas >= 2) { // Exigir al menos 2 lecturas buenas
    float promedio = suma / medicionesValidas;
    Serial.print("Medicion exitosa. Promedio final: ");
    Serial.println(promedio, 1);
    return promedio;
  } else {
    Serial.println("ERROR: No se pudieron obtener suficientes lecturas validas.");
    return -1.0; // Devolver -1 para indicar que la medición falló
  }
}
// Función auxiliar para generar pulsos con el canal independiente
void buzzerVolumenBajo(int duracionTotal) {
  int tiempoTranscurrido = 0;
  while (tiempoTranscurrido < duracionTotal) {
    // Genera tono en el canal específico asignado al Buzzer
    ledcWriteTone(BUZZER_PIN, 1500); 
    delay(100); // Mantiene el sonido por 100ms
    
    ledcWriteTone(BUZZER_PIN, 0); // Silencia el canal
    delay(20); // Pequeña pausa de separación
    
    tiempoTranscurrido += 120;
  }
}
void esperarInterruptor() {
  bool ledState = false;
  unsigned long ultimoParpadeo = 0;

  while (digitalRead(INTERRUPTOR_PIN) == HIGH) {
    unsigned long ahora = millis();
    if (ahora - ultimoParpadeo > 500) {
      ledState = !ledState;
      ultimoParpadeo = ahora;
    }
    delay(10);
  }
  
  apagarNeoPixel();
  delay(50);
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
    encenderNeoPixel(255, 0, 0);;
    //buzzerVolumenBajo(500);  // 100ms con pulsos cortos
    apagarNeoPixel(); // <--- CAMBIO: Apagar NeoPixel
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
  
  // Medir distancia frontal al inicio
  distanciaTraseraInicial = medirDistanciaTraseraInicialConfiable();
  // Si la medición falla, podemos asignar un valor por defecto o detener.
  // Por ahora, solo lo reportamos. El robot usará 125cm como referencia.
  if (distanciaTraseraInicial == -1.0) {
      Serial.println("Fallo en medicion inicial, usando 125cm como referencia por defecto.");
      distanciaTraseraInicial = 125.0; // Asignar un valor seguro por defecto
  }

  prioridadActual = PRIORIDAD_NORMAL;
  emergenciaActivada = false;
  retrocesoPorEmergencia = false;
  esquiveDiagonalActivo = false;
  giroEnProceso = false;
  enGiro = false;
  cruceEnProceso = false;
  
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
  detenerMotor();
  
  indicadorSistemaActivo();
  // //ln(F("Sistema iniciado - Motor arrancará en 1 segundo"));
}

// Función mejorada para detener el sistema completamente
void detenerSistema() {
  ////ln(F("=== DETENIENDO SISTEMA ==="));
  
  // Desactivar el sistema
  sistemaActivo = false;
  sistemaIniciado = false;
  
  // Detener todos los motores inmediatamente
  detenerMotor();
  
  // Centrar servo
  direccion.write(SERVO_CENTRADO);
  
  // Resetear sistema de retroceso y emergencias
  resetearRetrocesoEmergencia();
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
    encenderNeoPixel(255, 0, 0);;
    delay(150);
    apagarNeoPixel(); // <--- CAMBIO: Apagar NeoPixel
    delay(150);
  }
  apagarNeoPixel(); // <--- CAMBIO: Apagar NeoPixel // Asegurar que LED esté apagado
  
  ////ln(F("Sistema detenido completamente"));
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
  
 /* //(F("PID Centrado actualizado - Kp:"));
  //(kp);
  //(F(" Ki:"));
  //(ki);
  //(F(" Kd:"));
  //ln(kd);
  */
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

// Función para medir distancia
float medirDistancia(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duracion = pulseIn(echoPin, HIGH, 30000);
  if (duracion == 0) return 999;
  
  return (duracion * 0.034) / 2;
}

// Función de emergencia para detectar obstáculos frontales

bool verificarEmergencia() {
  // Solo verificar si está en modo seguimiento y no hay emergencia reciente
  if (estadoActual != SIGUIENDO_OBJETO || 
      millis() - tiempoUltimaEmergencia < TIEMPO_ESPERA_EMERGENCIA) {
    return false;
  }
  
  // Medir distancia frontal
  float distanciaFrontal = leerToF('F');
  Serial.print("distancia frontal emergencia = ");
  Serial.print(distanciaFrontal);
  Serial.println("cm");

  // Verificar si hay obstáculo en distancia de emergencia
  if (distanciaFrontal <= DISTANCIA_EMERGENCIA && distanciaFrontal > 0) {
    // Verificar si hay un color objetivo válido ACTUAL o RECORDADO
    bool colorActualValido = (colorObjetivo == 'R' || colorObjetivo == 'G');
   /* bool colorRecordadoValido = (ultimoColorDetectado == 'R' || ultimoColorDetectado == 'G') && 
                                (millis() - tiempoUltimoColor <= TIEMPO_MEMORIA_COLOR);*/
    
   // if (colorActualValido || colorRecordadoValido) { 
    if (colorActualValido) {
      emergenciaActivada = true;
      
      //  Usar color actual si está disponible, sino usar el recordado
      if (colorActualValido) {
        colorEmergencia = colorObjetivo;
      }/* else {
        colorEmergencia = ultimoColorDetectado;
        // Actualizar colorObjetivo para que el esquive funcione correctamente
        colorObjetivo = ultimoColorDetectado;
      }*/
      
      tiempoUltimaEmergencia = millis();
      
      // Debug opcional (mantener comentado en producción)
      /*
      //(F("EMERGENCIA: Obstáculo a "));
      //(distanciaFrontal);
      //(F("cm - Esquivando "));
      //(colorEmergencia);
      //(F(" ("));
      //(colorActualValido ? F("actual") : F("recordado"));
      //ln(F(")"));
      */
      
      return true;
    }
  }
  
  return false;
}

// Función para centrar el vehículo 
void centrarVehiculo() {
  // Candados de seguridad: No centrar si hay emergencias activas o si YA inició un giro
  if (prioridadActual != PRIORIDAD_NORMAL || 
      esquiveDiagonalActivo || 
      retrocesoPorEmergencia || 
      giroEnProceso) {
    if (pidCentrado.inicializado) resetearPIDCentrado(&pidCentrado);
    return;
  }
  
  if (millis() - ultimoTiempoCentrado < intervaloCentrado) return;

  // ==========================================================
  // CAPA 1: SEGUIDOR DE RUMBO (PID ANGULAR CON BNO055)
  // ==========================================================
  actualizarGiro(); // Asegurar tener el último ángulo Z
  
  // Calcular el error angular
  float errorAngular = anguloObjetivoActual - anguloZ;
  
  // Normalizar el error para que siempre busque el camino más corto (-180 a 180)
  if (errorAngular > 180) errorAngular -= 360;
  if (errorAngular < -180) errorAngular += 360;

  // Calcular la fuerza de giro necesaria con el PID
  float salidaPIDAngular = calcularPIDCentrado(&pidCentrado, errorAngular);

  // ==========================================================
  // CAPA 2: ESCUDO DE REPULSIÓN (ToF LATERALES)
  // ==========================================================
  distanciaDerecha = leerToF('D');
  delay(5);
  distanciaIzquierda = leerToF('I');
  
  float fuerzaRepulsion = 0.0;
  
  // Filtrar lecturas inválidas (>80cm o errores 999)
  if (distanciaDerecha < 80.0 && distanciaIzquierda < 80.0) {
    
    if (distanciaDerecha < UMBRAL_REPULSION && distanciaDerecha > 0) {
      // --- CORREGIDO: Peligro a la derecha. Fuerza POSITIVA para doblar Izquierda (>90) ---
      fuerzaRepulsion = (UMBRAL_REPULSION - distanciaDerecha) * K_REPULSION;
    } 
    else if (distanciaIzquierda < UMBRAL_REPULSION && distanciaIzquierda > 0) {
      // --- CORREGIDO: Peligro a la izquierda. Fuerza NEGATIVA para doblar Derecha (<90) ---
      fuerzaRepulsion = -(UMBRAL_REPULSION - distanciaIzquierda) * K_REPULSION;
    }
  }

  // ==========================================================
  // FUSIÓN Y EJECUCIÓN
  // ==========================================================
  // Sumamos la orden del giroscopio + el instinto de supervivencia de los ToF
  float correccionTotal = salidaPIDAngular + fuerzaRepulsion;
  
  // Limitar la corrección máxima para evitar coleos bruscos
  correccionTotal = constrain(correccionTotal, -anguloCorreccionMax, anguloCorreccionMax);

  // Aplicar suavizado mecánico para proteger el servo
  static int anguloAnterior = SERVO_CENTRADO;
  
  // --- CORRECCIÓN PRINCIPAL: Ahora SUMAMOS la corrección al centro ---
  int anguloDeseado = SERVO_CENTRADO + correccionTotal; 
  
  anguloDeseado = constrain(anguloDeseado, SERVO_CENTRADO - anguloCorreccionMax, SERVO_CENTRADO + anguloCorreccionMax);
  
  int anguloFinal = anguloAnterior * (1 - FACTOR_SUAVIZADO) + anguloDeseado * FACTOR_SUAVIZADO;
  anguloAnterior = anguloFinal;

  // Verificación final anti-choque de núcleos antes de mover el hardware
  if (!giroEnProceso) {
    direccion.write(anguloFinal);
  }
  
  ultimoTiempoCentrado = millis();
}
void detenerRobot() {
  // 1. Detener todos los motores y centrar la dirección
  detenerMotor();
  direccion.write(SERVO_CENTRADO);
  enGiro = false;

  Serial.println("=== MODO MONITOR: LEYENDO DISTANCIA TRASERA ===");

  // 2. Bucle infinito que lee y reporta la distancia trasera
  while(1) {
    // Medir la distancia trasera
    float distancia = leerToF('T');

    Serial.print("Distancia Trasera: ");
    Serial.print(distancia, 1); // Imprimir con 1 decimal
    Serial.println(" cm");

    // Parpadear el LED 
    encenderNeoPixel(255, 0, 0);;
    delay(100);
    apagarNeoPixel(); // <--- CAMBIO: Apagar NeoPixel

    delay(400);
  }
}
// ==================== FUNCIÓN realizarGiro MEJORADA CON PID ====================

void realizarGiro(bool giroIzquierda) {
  giroEnProceso = true; // 1. Avisar inmediatamente al Núcleo 1 que se detenga
  delay(20);            // 2. Darle tiempo al Núcleo 1 de terminar lo que estuviera leyendo

  Serial.println("iniciando Giro tras linea detectada");
  enGiro = true;
  cruceEnProceso = true;
  interrupcionCruce = false;
  startTime = millis();

  // Banderas para asegurar que cada maniobra se ejecute solo una vez por giro
  bool maniobraInteriorRealizada = false; // Para la nueva lógica (sensor interior)
  bool maniobraExteriorRealizada = false; // Para la lógica original (sensor exterior)

  // Actualizar giroscopio y calcular ángulo objetivo
  actualizarGiro();
  anguloObjetivoActual = anguloObjetivoGlobal;
  while (anguloObjetivoActual >= 360) anguloObjetivoActual -= 360;
  while (anguloObjetivoActual < 0) anguloObjetivoActual += 360;
  
  // Activar motor y dirección del giro
  avanzarMotor();
  if (giroIzquierda) {
    Serial.println("*****giro a la izquierda****************");
    direccion.write(165);
  } else {
    Serial.println("*****giro a la derecha****************");
    direccion.write(SERVO_RIGHT);
  }
  unsigned long lastDebugPrint = 0; /
  while (millis() - startTime < timeoutGiro && !interrupcionCruce) {
    
    // ----------- LÓGICA #1: NUEVA (Sensor Interior para esquinas) -----------
    if (!maniobraInteriorRealizada) {
      float distInterior;
      if (sentidoVehiculo == DERECHA) {
        distInterior = leerSensorSharp(SHARP_DIAGONAL_DER_PIN); // Sensor derecho
      } else { 
        distInterior = leerSensorSharp(SHARP_DIAGONAL_IZQ_PIN); // Sensor izquierdo
      }

      if (distInterior < 15.0) {
        maniobraInteriorRealizada = true; // Activar bandera
        // --- PRINTS AÑADIDOS PARA BLUETOOTH ---
        Serial.print(">>> OBSTACULO INTERIOR DETECTADO A: ");
        Serial.print(distInterior);
        Serial.println(" cm <<<");
        Serial.println("--> Iniciando acomodo complejo...");
        // --------------------------------------
        detenerMotor();
        // Parte A: Retroceso con giro (500ms)
        Serial.println("    1. Retrocediendo y abriendo curva...");
        if (sentidoVehiculo == DERECHA) { 
          direccion.write(SERVO_RIGHT);
           } else { 
            direccion.write(SERVO_LEFT); 
            }
        delay(200);
        retrocederMotor();
        unsigned long t_maniobra = millis();
        while (millis() - t_maniobra < 700) { 
          actualizarGiro();
           delay(10);
           }
        detenerMotor();
        
        // Parte B: Avance recto (800ms)
        Serial.println("    2. Avanzando recto para esquivar pared...");
        direccion.write(SERVO_CENTRADO);
        delay(200);
        avanzarMotor();
        t_maniobra = millis();
        while (millis() - t_maniobra < 700) { 
          actualizarGiro();
          delay(10);
           }
        detenerMotor();
        
        // Parte C: Reanudar giro
        Serial.println("    3. Reincorporando al giro...");
        if (giroIzquierda) { 
            Serial.println("*****giro a la izquierda****************");
            direccion.write(SERVO_LEFT);
          } else {
            Serial.println("*****giro a la derecha****************");
            direccion.write(SERVO_RIGHT);
          }
        delay(200);
        avanzarMotor();
        startTime = millis(); // Resetear temporizador del giro
      }
    }



    // ----------- LÓGICA #2:  (Sensor Exterior para paredes) -----------
    // Se ejecuta si la maniobra interior no está activa
    if (!maniobraInteriorRealizada && !maniobraExteriorRealizada) {
        float distExterior;
        if (sentidoVehiculo == DERECHA) {
            distExterior = leerSensorSharp(SHARP_DIAGONAL_IZQ_PIN); // Sensor izquierdo
        } else {
            distExterior = leerSensorSharp(SHARP_DIAGONAL_DER_PIN); // Sensor derecho
        }

        if (distExterior < 20.0) {
            maniobraExteriorRealizada = true; // Activar bandera
// --- PRINTS AÑADIDOS PARA BLUETOOTH ---
            Serial.print(">>> OBSTACULO EXTERIOR DETECTADO A: ");
            Serial.print(distExterior);
            Serial.println(" cm <<<");
            Serial.println("--> Iniciando acomodo simple (Retroceso)...");
            // --------------------------------------            
            detenerMotor(); // Detener avance
            direccion.write(SERVO_CENTRADO);
            delay(300);
            retrocederMotor(); // Activar retroceso

            unsigned long t_retroceso = millis();
            while (millis() - t_retroceso < 800) {
                actualizarGiro(); // Mantener el ángulo actualizado
                delay(10);
            }

            detenerMotor(); // Detener retroceso
            delay(300);
            
            // Reanudar el giro
            Serial.println("    Reincorporando al giro...");
            avanzarMotor();
            if (giroIzquierda) { direccion.write(SERVO_LEFT); } else { direccion.write(SERVO_RIGHT); }
            startTime = millis(); // Resetear temporizador del giro
            
          tiempofinalManiobraGiroExterna = millis();
        }

        
    }
  
    if (millis() - tiempofinalManiobraGiroExterna > intervaloCentradoCorreccionCruce) {
    maniobraExteriorRealizada = false;
    }
        
      
    // --- Lógica de control del ángulo ---
    actualizarGiro();
    float errorActual = anguloObjetivoActual - anguloZ;
    if (errorActual > 180) errorActual -= 360;
    if (errorActual < -180) errorActual += 360;

    // --- NUEVO: MONITOREO DEL GIRO EN TIEMPO REAL ---
    if (millis() - lastDebugPrint > 200) { // Imprime cada 200ms
       Serial.print("   -> Girando... Meta: ");
       Serial.print(anguloObjetivoActual);
       Serial.print(" | Actual: ");
       Serial.print(anguloZ);
       Serial.print(errorActual);
       Serial.print(" | Angulo Servo: ");       // <--- NUEVO
       Serial.println(direccion.read());       // <--- LEE EL SERVO REAL
       lastDebugPrint = millis();
    }
    // ------------------------------------------------
    if (abs(errorActual) <= toleranciaGiro) {
      Serial.println(">>> Angulo alcanzado PERFECTAMENTE. Terminando giro <<<");
      break; // Salir del bucle si se alcanza el ángulo
    }
    delay(10);
  }
  // --- NUEVO: AVISO SI OCURRE UN TIMEOUT (Se rindió) ---
  if (millis() - startTime >= timeoutGiro) {
      Serial.println("!!! ALERTA: Giro abortado por TIMEOUT (No alcanzó la meta) !!!");
  }
  
 // --- Finalización del Giro ---
  direccion.write(SERVO_CENTRADO);
  enGiro = false;
  giroEnProceso = false;
  cruceEnProceso = false;
  tiempoFinGiro = millis();
  
  // Decidir qué hacer después del giro directamente aquí
  if (giroParaEstacionamiento) {
      estadoActual = ESTADO_DETENCION_FINAL;
      tiempoInicioDetencionFinal = millis();
      giroParaEstacionamiento = false; // Resetear bandera
      detenerMotor();
      Serial.println("Giro final completo. Iniciando ajuste de posicion...");
  } else {
      if (contadorLineas < limiteLineas) {
        estadoActual = ESTADO_NORMAL;
      }
      resetearPIDCentrado(&pidCentrado);
      avanzarMotor(); // Reanudar marcha normal hacia la siguiente línea
  }
}


void calcularAnguloObjetivoMantener() {
  if (sentidoVehiculo == IZQUIERDA) {
    // Giro a la izquierda: 1->90°, 2->180°, 3->270°, 4->0°, etc.
    int ciclo = (contadorLineas -1) % 4; // Ajuste para que la primera línea sea ciclo 0
    switch (ciclo) {
      case 0: anguloObjetivoGlobal = 90.0; break;  // Línea 1, 5, 9...
      case 1: anguloObjetivoGlobal = 180.0; break; // Línea 2, 6, 10...
      case 2: anguloObjetivoGlobal = 270.0; break; // Línea 3, 7, 11...
      case 3: anguloObjetivoGlobal = 0.0; break;   // Línea 4, 8, 12...
    }
  } else { // sentidoVehiculo == DERECHA
    // Giro a la derecha: 1->270°, 2->180°, 3->90°, 4->0°, etc.
    int ciclo = (contadorLineas - 1) % 4; // Ajuste para que la primera línea sea ciclo 0
    switch (ciclo) {
      case 0: anguloObjetivoGlobal = 270.0; break; // Línea 1, 5, 9...
      case 1: anguloObjetivoGlobal = 180.0; break; // Línea 2, 6, 10...
      case 2: anguloObjetivoGlobal = 90.0; break;  // Línea 3, 7, 11...
      case 3: anguloObjetivoGlobal = 0.0; break;   // Línea 4, 8, 12...
    }
  }

  // --- CÓDIGO NUEVO PARA EL MONITOR SERIAL ---
  Serial.print(">>> Nuevo Angulo Objetivo Global calculado: ");
  Serial.print(anguloObjetivoGlobal);
  Serial.print(" grados <<<    ");
  Serial.print(">>> lineas : ");
    Serial.println(contadorLineas);

  // -------------------------------------------
}


void manejarInterrupcionCruce() {
  // Detener el giro inmediatamente
  direccion.write(SERVO_CENTRADO);
  avanzarMotor(); // Mantener motor activo
  
  // Resetear flags de giro
  enGiro = false;
  giroEnProceso = false;
  cruceEnProceso = false;
  tiempoFinGiro = millis();
  
  // Procesar el comando que causó la interrupción
  colorDetectado = comandoInterrupcion;
  
  // Cambiar al estado apropiado según el comando
  if (comandoInterrupcion == 'R' || comandoInterrupcion == 'G') {
    colorObjetivo = comandoInterrupcion;
    estadoActual = SIGUIENDO_OBJETO;
   // //(F("Iniciando seguimiento de objeto: "));
    ////ln(comandoInterrupcion);
    
    // Orientar servo según la última orientación recibida
    orientarServoSeguimiento();
  }
  
  // Resetear flags de interrupción
  interrupcionCruce = false;
  comandoInterrupcion = 'N';
}


// ========== ESTRATEGIA 1: CORRECCIÓN PERIÓDICA SIMPLE ==========
void actualizarGiro() {
  readIMU();
}

void readIMU() {
  imu::Quaternion quat = bno.getQuat();
  double w = quat.w(), x = quat.x(), y = quat.y(), z = quat.z();
  
  double norm = sqrt(w*w + x*x + y*y + z*z);
  if (norm < 1e-6) return; // Defensa ante datos nulos
  w /= norm; x /= norm; y /= norm; z /= norm;

  // Extraer Yaw
  double yaw = atan2(2.0*(w*z + x*y), 1.0 - 2.0*(y*y + z*z));
  float yaw_deg_temp = (float)(yaw * 180.0 / M_PI);
  
  // Aplicar origen relativo para que arranque en 0°
  yaw_deg_temp -= yaw_offset;
  
  // Normalizar estrictamente de 0 a 360 grados (como lo usaba el MPU6050)
  while (yaw_deg_temp < 0.0f) yaw_deg_temp += 360.0f;
  while (yaw_deg_temp >= 360.0f) yaw_deg_temp -= 360.0f;

  anguloZ = yaw_deg_temp; // Actualizar variable global usada por PID y Giros
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
}

void detectarColor() {
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

    if (millis() - tiempoUltimaDeteccion > tiempoEsperaDeteccion) {
      bool esBlanco = (value > 0.85 && saturation < 0.15);
      float proporcionAzul = b_norm / (r_norm + g_norm + 0.001);
      
      bool azulDetectado = false;
      bool naranjaDetectado = false;
        
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
       // Detección naranja/rojo/amarillo
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
        // INTERRUMPIR ESQUIVE si está en etapa de retorno
        if (estadoActual == ESQUIVANDO && (etapaEsquive == 3 || etapaEsquive == 4)) {
          estadoActual = ESTADO_NORMAL;
          etapaEsquive = 0;
        }
        
        // Ejecutar giro normal según el sentido del vehículo
        if (azulDetectado && sentidoVehiculo == IZQUIERDA) {
          encenderNeoPixel(0, 0, 255); // <--- CAMBIO: NeoPixel Azul          
          contadorLineas++;
          //("linea contada   ");
            //ln(contadorLineas);
          // *** MODIFICACIÓN PRINCIPAL ***
          /*if (contadorLineas >= limiteLineas) {
            //ln("limite de lineas alcanzaado");

            resetearPIDCentrado(&pidCentrado);
            
            realizarGiro(true); // Giro a la izquierda
            // REALIZAR EL GIRO PRIMERO antes de buscar estacionamiento
            // Marcar que después del giro debe iniciar búsqueda de estacionamiento
            // Esto se manejará en la función actualizarGiro() cuando termine el giro
            giroParaEstacionamiento = true;
            
          }*/
          if (contadorLineas >= limiteLineas) {
            //ln("Limite de lineas alcanzado, iniciando detencion en 3s...");
            estadoActual = ESTADO_DETENCION_FINAL;
            giroParaEstacionamiento = true;
            resetearPIDCentrado(&pidCentrado);
            calcularAnguloObjetivoMantener(); // calculo de angulo dependiendo de contadorLineas
            realizarGiro(true); // Giro a la izquierda
            tiempoInicioDetencionFinal = millis(); // Inicia el temporizador de 3 segundos
          }else {
            resetearPIDCentrado(&pidCentrado);
            calcularAnguloObjetivoMantener(); // calculo de angulo dependiendo de contadorLineas
            realizarGiro(true); // Giro a la izquierda
            
            // Después del giro, gestionar el estado apropiado
            if (estadoActual == ESQUIVANDO) {
              estadoActual = ESQUIVANDO;
            } else if (estadoActual == SIGUIENDO_OBJETO) {
              estadoActual = SIGUIENDO_OBJETO;
            }
          }
          
          apagarNeoPixel(); // <--- CAMBIO: Apagar NeoPixel
          tiempoUltimaDeteccion = millis();
        }
        else if (naranjaDetectado && sentidoVehiculo == DERECHA) {
          encenderNeoPixel(255, 128, 0); // <--- CAMBIO: NeoPixel Naranja
          contadorLineas++;
          //("linea contada   ");
            //ln(contadorLineas);
          // *** MODIFICACIÓN PRINCIPAL ***
          /*if (contadorLineas >= limiteLineas) {
            //ln("limite de lineas alcanzaado");
            // REALIZAR EL GIRO PRIMERO antes de buscar estacionamiento
            // Marcar que después del giro debe iniciar búsqueda de estacionamiento
            // Esto se manejará en la función actualizarGiro() cuando termine el giro
            giroParaEstacionamiento = true;
            resetearPIDCentrado(&pidCentrado);
           
            realizarGiro(false); // Giro a la derecha
            
            
          }*/
           if (contadorLineas >= limiteLineas) {
            //ln("Limite de lineas alcanzado, iniciando detencion en 3s...");
            giroParaEstacionamiento = true;
            resetearPIDCentrado(&pidCentrado);
            calcularAnguloObjetivoMantener(); // calculo de angulo dependiendo de contadorLineas
            realizarGiro(false); // Giro a la derecha
            estadoActual = ESTADO_DETENCION_FINAL;
            tiempoInicioDetencionFinal = millis(); // Inicia el temporizador de 3 segundos
          }else {
            resetearPIDCentrado(&pidCentrado);
            calcularAnguloObjetivoMantener(); // calculo de angulo dependiendo de contadorLineas
            realizarGiro(false); // Giro a la derecha
            
            // Después del giro, gestionar el estado apropiado
            if (estadoActual == ESQUIVANDO) {
              estadoActual = ESQUIVANDO;
            } else if (estadoActual == SIGUIENDO_OBJETO) {
              estadoActual = SIGUIENDO_OBJETO;
            }
          }
          
          apagarNeoPixel(); // <--- CAMBIO: Apagar NeoPixel
          tiempoUltimaDeteccion = millis();
        }
        else if (naranjaDetectado && sentidoVehiculo == IZQUIERDA) {
          // Línea naranja detectada pero el vehículo va hacia la izquierda
          // Parpadeo de advertencia
          for (int i = 0; i < 2; i++) {
            encenderNeoPixel(255, 0, 0);;
            delay(100);
            apagarNeoPixel(); // <--- CAMBIO: Apagar NeoPixel
            delay(100);
          }
        }
      }
    }
  }
}


//-----------------Funciones de evasion y recepcion de serial---------------
//PASO 3: Integrar funciones de recepción serial optimizada
void processSerialData() {
  while (Serial.available() > 0) {
    char inChar = Serial.read();
    
    if (inChar == '\n' || inChar == '\r') {
      if (bufferIndex > 0) {
        buffer[bufferIndex] = '\0';
        parseCommand();
        bufferIndex = 0;
      }
      return;
    }
    
    if (bufferIndex < BUFFER_SIZE - 1) {
      buffer[bufferIndex++] = inChar;
    } else {
      bufferIndex = 0;
    }
  }
}

void parseCommand() {
  //  Ignorar comandos durante AVANCE FINAL
  /*if (estadoActual == ESTADO_AVANCE_FINAL) {
    // Durante el avance final, ignorar todos los comandos de detección
    // Solo permitir comandos de emergencia críticos si es necesario
    return;
  }*/
  
  // Comando de no detección
  if (buffer[0] == 'N' && bufferIndex == 1) {
    handleNoDetection();
    return;
  }
  
  // Parsing formato: R,25,I o G,30,D
  int firstComma = -1;
  int secondComma = -1;
  
  for (int i = 0; i < bufferIndex; i++) {
    if (buffer[i] == ',') {
      if (firstComma == -1) {
        firstComma = i;
      } else if (secondComma == -1) {
        secondComma = i;
        break;
      }
    }
  }
  
  if (firstComma == -1 || secondComma == -1) return;
  
  // Extraer datos
  colorDetectado = buffer[0];
  
  distanciaObjeto = 0;
  for (int i = firstComma + 1; i < secondComma; i++) {
    if (buffer[i] >= '0' && buffer[i] <= '9') {
      distanciaObjeto = distanciaObjeto * 10 + (buffer[i] - '0');
    }
  }
  
  orientacionObjeto = buffer[secondComma + 1];
  
  // Guardar último color válido detectado
  if (colorDetectado == 'R' || colorDetectado == 'G') {
    ultimoColorDetectado = colorDetectado;
    tiempoUltimoColor = millis();
  }
  
  // Verificar interrupción de cruce
  if (cruceEnProceso && (colorDetectado == 'R' || colorDetectado == 'G')) {
    interrupcionCruce = true;
    comandoInterrupcion = colorDetectado;
    return; // Salir inmediatamente para procesar la interrupción
  }
  
  processCommand();
}
// PASO 4: Máquina de estados principal
void processCommand() {
  switch(estadoActual) {
    case ESTADO_NORMAL:
      // Cambiar a seguimiento si detecta R o G
      if (colorDetectado == 'R' || colorDetectado == 'G') {
        colorObjetivo = colorDetectado;
        estadoActual = SIGUIENDO_OBJETO;
        // Mantener motor activo, servo se orienta en loop principal
      }
      break;
      
    case SIGUIENDO_OBJETO:{
      // Actualizar orientación del servo
      orientarServoSeguimiento();
      
      // NUEVA FUNCIONALIDAD: Verificar emergencia por sensor frontal
      if (verificarEmergencia()) {
        // Activar esquive de emergencia
        estadoActual = ESQUIVANDO;
        etapaEsquive = 0;
        tiempoEsquive = millis();
        colorObjetivo = colorEmergencia; // Usar el color detectado en emergencia
        
        // Debug opcional
        /*
        //ln(F("ESQUIVE DE EMERGENCIA ACTIVADO"));
        */
        break;
      
      }
      
      // Verificar si debe esquivar por comando normal (distancia específica por color)
      int distanciaEsquive = 0;
      if (colorDetectado == 'R') {
        distanciaEsquive = DISTANCIA_ROJO;
      } else if (colorDetectado == 'G') {
        distanciaEsquive = DISTANCIA_VERDE;
      }
      
      if (distanciaObjeto <= distanciaEsquive && distanciaObjeto > 0 && 
          (colorDetectado == 'R' || colorDetectado == 'G')) {
        colorObjetivo = colorDetectado;
        estadoActual = ESQUIVANDO;
        etapaEsquive = 0;
        tiempoEsquive = millis();
      }
      break;
    }
    case ESQUIVANDO:
      // Durante retorno (etapas 3-4), puede interrumpirse
      if ((etapaEsquive == 3 || etapaEsquive == 4) && 
          (colorDetectado == 'R' || colorDetectado == 'G')) {
        colorObjetivo = colorDetectado;
        estadoActual = SIGUIENDO_OBJETO;
        etapaEsquive = 0;
        orientarServoSeguimiento();
      }
      break;
      
   
  }
}
void handleNoDetection() {
  //  No procesar durante AVANCE FINAL
  /*if (estadoActual == ESTADO_AVANCE_FINAL) {
    return; // Ignorar completamente durante avance final
  }*/
  
  // Si estabas siguiendo un objeto y ya no lo ves...
  if (estadoActual == SIGUIENDO_OBJETO) {
    // Centra el servo
    if (lastOrientation != 'C') {
      direccion.write(SERVO_CENTRADO);
      lastOrientation = 'C';
    }
    
    // Solo regresar a estado normal si ha pasado suficiente tiempo
    // desde la última detección válida (para evitar cambios bruscos)
    unsigned long tiempoSinDeteccion = millis() - tiempoUltimoColor;
    
    if (tiempoSinDeteccion > TIEMPO_MEMORIA_COLOR) {
      // Ha pasado mucho tiempo, regresar al estado normal
      estadoActual = ESTADO_NORMAL;
      resetearPIDCentrado(&pidCentrado);
      ultimoColorDetectado = 'N'; // Limpiar memoria
    }
    // Si no ha pasado suficiente tiempo, mantener el estado de seguimiento
    // para permitir que la emergencia funcione con el color recordado
  }
}

//PASO 5: Funciones de orientación del servo
void orientarServoSeguimiento() {
  if (orientacionObjeto != lastOrientation) {
    int servoPosition = SERVO_CENTRADO;
    
    switch(orientacionObjeto) {
      case 'I':
        servoPosition = SERVO_OrientadoObjeto_LEFT; // Izquierda moderada para seguimiento
        break;
      case 'D':
        servoPosition = SERVO_OrientadoObjeto_RIGH; // Derecha moderada para seguimiento
        break;
      case 'C':
        servoPosition = SERVO_CENTRADO;
        break;
    }
    
    direccion.write(servoPosition);
    lastOrientation = orientacionObjeto;
  }
}

void orientarServoEsquive() {
  if (orientacionObjeto != lastOrientation) {
    int servoPosition = SERVO_CENTRADO;
    
    switch(orientacionObjeto) {
      case 'I':
        servoPosition = SERVO_LEFT; // Izquierda extrema para esquive
        break;
      case 'D':
        servoPosition = SERVO_RIGHT; // Derecha extrema para esquive
        break;
      case 'C':
        servoPosition = SERVO_CENTRADO;
        break;
    }
    
    direccion.write(servoPosition);
    lastOrientation = orientacionObjeto;
  }
}
void configurarTiemposEsquive() {
  if (colorObjetivo == 'G') {
    tiempoEsquiveActual = TIEMPO_ESQUIVE_VERDE;
    tiempoRetornoActual = TIEMPO_RETORNO_VERDE;
  } else if (colorObjetivo == 'R') {
    tiempoEsquiveActual = TIEMPO_ESQUIVE_ROJO;
    tiempoRetornoActual = TIEMPO_RETORNO_ROJO;
  } else {
    // Valores por defecto si hay otro color
    tiempoEsquiveActual = 800;
    tiempoRetornoActual = 600;
  }
}
//PASO 6: Función de esquive completa
void procesarEsquive() {
  unsigned long tiempoActual = millis();
  
  switch(etapaEsquive) {
   case 0: // Orientar servo hacia lado de esquive
      if (colorObjetivo == 'G') {
        direccion.write(SERVO_LEFT); // Esquivar izquierda extrema
        lastOrientation = 'I';
      } else if (colorObjetivo == 'R') {
        direccion.write(SERVO_RIGHT); // Esquivar derecha extrema
        lastOrientation = 'D';
      }
          configurarTiemposEsquive();
      
      tiempoEsquive = tiempoActual;
      etapaEsquive = 1;
      break;
        if (colorObjetivo == 'G') {
          direccion.write(SERVO_LEFT); // Esquivar izquierda extrema
          lastOrientation = 'I';
        } else if (colorObjetivo == 'R') {
          direccion.write(SERVO_RIGHT); // Esquivar derecha extrema
          lastOrientation = 'D';
        }
        tiempoEsquive = tiempoActual;
        etapaEsquive = 1;
        break;
      
    case 1: // Esperar orientación y activar motor
      if (tiempoActual - tiempoEsquive > 300) {
        // Motor ya está activo en estado normal
        etapaEsquive = 2;
        tiempoEsquive = tiempoActual;
      }
      break;
      
    case 2: // Avanzar esquivando
      if (tiempoActual - tiempoEsquive > tiempoEsquiveActual) {
        // Girar al lado contrario para retornar
        if (colorObjetivo == 'G') {
          direccion.write(SERVO_RIGHT); // Retornar por derecha
          lastOrientation = 'D';
        } else if (colorObjetivo == 'R') {
          direccion.write(SERVO_LEFT); // Retornar por izquierda
          lastOrientation = 'I';
        }
        etapaEsquive = 3;
        tiempoEsquive = tiempoActual;
      }
      break;
      
    case 3: // Esperar orientación de retorno
      if (tiempoActual - tiempoEsquive > 300) {
        etapaEsquive = 4;
        tiempoEsquive = tiempoActual;
      }
      break;
      
    case 4: // Avanzar de retorno (INTERRUMPIBLE)
      // CAMBIAR ESTA LÍNEA:
      if (tiempoActual - tiempoEsquive > tiempoRetornoActual) {
        etapaEsquive = 5;
        tiempoEsquive = tiempoActual;
      }
      break;
      
    case 5: // Centrar servo y volver a estado normal
      if (tiempoActual - tiempoEsquive > 100) {
        direccion.write(SERVO_CENTRADO);
        lastOrientation = 'C';

        //prueba cambio de lo de abajo
          estadoActual = ESTADO_NORMAL;

        // Si fue una emergencia, volver a seguimiento del mismo color
        /*if (emergenciaActivada) {
          estadoActual = SIGUIENDO_OBJETO;
          // Mantener el mismo colorObjetivo
          emergenciaActivada = false;
        } else {
          estadoActual = ESTADO_NORMAL;
        }*/
        
        etapaEsquive = 0;
      }
      break;
  }
}

// ==================== FUNCIÓN LEER SENSOR SHARP ====================

float leerSensorSharp(int pin) {

   // Leer valor analógico del sensor Sharp
  int valorAnalogico = analogRead(pin);
  
  // Convertir a voltaje (asumiendo 5V de referencia)
  float voltaje = valorAnalogico * (3.3 / 4095);
  
  // Conversión aproximada para sensor Sharp GP2Y0A21YK0F (10-80cm)
  // Fórmula: distancia = 27.728 / (voltaje - 0.1696)
  if (voltaje < 0.4) return 999; // Fuera de rango
  
  float distancia = 27.728 / (voltaje - 0.1696);
  
  // Limitar rango válido, el sensor sharp agarra minimo 10cm
  if (distancia < 10 || distancia > 80) return 999;
  
  return distancia; 
}

// ==================== FUNCIÓN VERIFICAR OBSTÁCULO DIAGONAL ====================

bool verificarObstaculoDiagonal() {
    if (estadoActual == ESQUIVANDO) {
    return false; // Desactivar detección en ESQUIVANDO
  }
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
    // --- PRINT PARA BLUETOOTH ---
    Serial.print(">>> ALERTA SHARP IZQUIERDO: Obstaculo a ");
    Serial.print(distanciaIzq);
    Serial.println(" cm <<<");
    Serial.println("--> Iniciando volantazo hacia la DERECHA...");
    // ----------------------------
    tiempoInicioEsquiveDiagonal = millis();
    tiempoUltimaDeteccionSharp = millis();
    etapaEsquiveDiagonal = 0;
    return true;
  }
  
  // Verificar obstáculo en diagonal derecha
  if (distanciaDer <= DISTANCIA_SHARP_EMERGENCIA && distanciaDer > 0) {
    esquiveDiagonalActivo = true;
    direccionEsquiveDiagonal = 'I'; // Esquivar hacia la izquierda
    // --- PRINT PARA BLUETOOTH ---
    Serial.print(">>> ALERTA SHARP DERECHO: Obstaculo a ");
    Serial.print(distanciaDer);
    Serial.println(" cm <<<");
    Serial.println("--> Iniciando volantazo hacia la IZQUIERDA...");
    // ----------------------------
    tiempoInicioEsquiveDiagonal = millis();
    tiempoUltimaDeteccionSharp = millis();
    etapaEsquiveDiagonal = 0;
    return true;
  }
  
  return false;
}

// ==================== FUNCIÓN PROCESAR ESQUIVE DIAGONAL ====================

void procesarEsquiveDiagonal() {
   if (estadoActual == ESQUIVANDO || !esquiveDiagonalActivo) {
   
    return;
  }
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
    // --- PRINT PARA BLUETOOTH ---
      Serial.println("    1. Girando direccion para esquivar...");
      // ----------------------------
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
    Serial.println("    2. Esquive completado. Retomando centrado normal.");
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
  estadoActual = ESTADO_NORMAL;
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
    
    ////ln("ACTIVANDO RETROCESO DE EMERGENCIA");
    
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
    ////ln("TIMEOUT DE RETROCESO - FINALIZANDO FORZADAMENTE");
    
    // Timeout alcanzado - forzar finalización
    detenerMotor(); // Apagar motor de retroceso
    delay(100);
    avanzarMotor();            // Reactivar motor principal
    apagarNeoPixel(); // <--- CAMBIO: Apagar NeoPixel              // Apagar LED
    
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
      encenderNeoPixel(255, 0, 0);;
    } else {
      apagarNeoPixel(); // <--- CAMBIO: Apagar NeoPixel
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
      avanzarMotor();            // Reactivar motor principal
      apagarNeoPixel(); // <--- CAMBIO: Apagar NeoPixel              // Apagar LED
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
      ////ln("Obstáculo aún presente - Reiniciando ciclo de retroceso");
      
      // RESETEAR PARA NUEVO CICLO
      motorRetrocesoConfigurado = false;  // PERMITIR NUEVA CONFIGURACIÓN
      tiempoInicioRetroceso = millis();   // NUEVO TIEMPO DE INICIO
      
      // Mantener retrocesoPorEmergencia = true para continuar
    }
  }
}

void resetearRetrocesoEmergencia() {
  ////ln("RESET COMPLETO DEL SISTEMA DE RETROCESO");
  
  // Apagar motores de forma segura
  detenerMotor(); // Apagar motor de retroceso
  delay(100);
  detenerMotor();           // Apagar motor principal temporalmente
  delay(100);
  apagarNeoPixel(); // <--- CAMBIO: Apagar NeoPixel              // Apagar LED
  
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
    delay(100);  // Pequeña pausa para estabilizar la lecturaa
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
      avanzarMotor();  // Activar motor (LOW para avanzar según tu configuración)

      motorArrancado = true;
      // //ln(F("Motor arrancado"));
    }
  }
}

// --- AGREGA ESTE BLOQUE ANTES DEL SETUP() ---
TaskHandle_t TareaColorHandle;

void TareaDeteccionColor(void * parameter) {
  for(;;) {
    // Solo detectar color si el sistema está activo y no en detención final
    if (sistemaActivo && estadoActual != ESTADO_DETENCION_FINAL) {
       detectarColor();
    }
    // Yield: Le dice al procesador que espere 10ms (100 Hz de actualización). 
    // Esto previene que el Watchdog del ESP32 reinicie la placa.
    vTaskDelay(10 / portTICK_PERIOD_MS); 
  }
}
// --------------------------------------------

void setup() {
   // Configurar pines como salidas digitales
  // --- AGREGA ESTO AL PRINCIPIO DEL SETUP ---
  strip.begin();            // Inicializa el NeoPixel
  strip.setBrightness(50);  // Ajusta el brillo (de 0 a 255. 50 es buen brillo sin cegar)
  apagarNeoPixel();         // Asegura que empiece apagado
  // ------------------------------------------
  pinMode(BUZZER_PIN, OUTPUT);
    // Asegurar que empiecen apagados
  apagarNeoPixel(); // <--- CAMBIO: Apagar NeoPixel

// Configurar pines como salidas digitales
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT); // Asegurar salida del Buzzer

  // Configurar el canal PWM en el ESP32 y asignarlo al pin ENA
  ledcAttachChannel(MOTOR_ENA, freqMotor, resolucionPWM, canalMotor);

  // 2. Configurar y anclar el canal PWM del Buzzer
  ledcAttachChannel(BUZZER_PIN, 2000, resolucionPWM, canalBuzzer);
  ledcWriteTone(BUZZER_PIN, 0); // Asegurar que inicie en silencio

  // Inicializar el motor detenido
  detenerMotor();

    // Configurar pin del interruptor con pull-up interno
  pinMode(INTERRUPTOR_PIN, INPUT_PULLUP);
  // Toca un tono de frecuencia media (ej. 1500 Hz) durante 100 ms
 // tone(BUZZER_PIN, 1500, 100);
  Serial.begin(115200);
  Serial.setTimeout(1);

   
  Serial.setTimeout(1);
  /// Iniciar ambos buses I2C
  Wire.begin(); // Pines por defecto para BNO055 y ToFs
  I2CSegundo.begin(I2C2_SDA, I2C2_SCL); // Bus secundario para Color

  Wire.setClock(400000);       // Acelera el bus 1 a 400kHz
  I2CSegundo.setClock(400000); // Acelera el bus 2 a 400kHz
  // ------------------------------------
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

  // 3. Iniciar Sensor de Color en Bus 2
  Serial.print(F("Iniciando Color TCS34725 (Bus 2)... "));
  if (tcs.begin(TCS34725_ADDRESS, &I2CSegundo)) {
    Serial.println(F("OK"));
  } else {
    Serial.println(F("FALLO sensor de color. Verifica los pines 32 y 33."));
    while (1);
  }

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
  
  resetearPIDCentrado(&pidCentrado);

  // Inicializar variables de memoria de color
  ultimoColorDetectado = 'N';
  tiempoUltimoColor = 0;
  bufferIndex = 0;
  lastOrientation = 'C';
  

    Serial.println("centrao");
  direccion.write(SERVO_CENTRADO);

  // Verificar estado inicial del interruptor
  if (digitalRead(INTERRUPTOR_PIN) == LOW) {
    unsigned long tiempoEspera = millis();
    while (millis() - tiempoEspera < 100) {}
    iniciarSistema();
    direccion.write(SERVO_CENTRADO);
  } else {
    esperarInterruptor();
    direccion.write(SERVO_CENTRADO);
  }
  
  //Serial.print("centrao");
  ////ln(F("Sistema listo"));
  lastTime = millis();
  tiempoUltimaDeteccion = millis();
  ultimoTiempoCentrado = millis();

  // -segundo nucleo tarea permanente
  xTaskCreatePinnedToCore(
      TareaDeteccionColor, // Función de la tarea
      "TareaColor",        // Nombre para depuración
      4096,                // Tamaño de pila (stack)
      NULL,                // Parámetros
      1,                   // Prioridad (1 es normal)
      &TareaColorHandle,   // Referencia (handle)
      0                    // ANCLAR AL NÚCLEO 0 (el loop principal usa el 1)
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

  // --- SEGURO ANTI-CHOQUE DE NÚCLEOS ---
  // Si el Núcleo 0 (Color) está ejecutando un giro, obligamos al Núcleo 1 (Loop) a 
  // pausar. Así evitamos que ambos peleen por el sensor BNO055 y el Bluetooth.
  if (giroEnProceso) {
    delay(10);
    return; 
  }
  // -------------------------------------
   if (contadorLineas>= limiteLineas){
    estadoActual = ESTADO_DETENCION_FINAL;
  }
    // EJECUTAR SIEMPRE (independiente de comandos seriales)
  actualizarGiro();
  // detectarColor();
 /*if (estadoActual != ESTADO_DETENCION_FINAL){
    detectarColor();
  }*/
   // MÁQUINA DE ESTADOS - Solo ejecutar si no hay procesos de alta prioridad
  switch(estadoActual) {
    case ESTADO_NORMAL:
      //centrara vehiculo despues del primer giro
       if (!enGiro && 
          prioridadActual == PRIORIDAD_NORMAL && 
          !esquiveDiagonalActivo && 
          !retrocesoPorEmergencia && !primeraDeteccion) {
        centrarVehiculo();
        //Serial.println("normal");
          }
      break;
      
    case SIGUIENDO_OBJETO:
      if (prioridadActual == PRIORIDAD_NORMAL && 
          !esquiveDiagonalActivo && 
          !retrocesoPorEmergencia) {
          //  Serial.println("siguiendo");
        orientarServoSeguimiento();
        
      }
      break;
      
    case ESQUIVANDO:
      if (prioridadActual == PRIORIDAD_NORMAL && 
          !esquiveDiagonalActivo && 
          !retrocesoPorEmergencia) {
         //   Serial.println("esquive");
        procesarEsquive();
      }
      break;
   
     case ESTADO_DETENCION_FINAL:
      { 
        // Esta lógica se ejecuta una sola vez.
        Serial.println("--- ESTADO DE DETENCION FINAL INICIADO ---");
        Serial.print("La distancia trasera inicial medida fue: ");
        Serial.println(distanciaTraseraInicial, 1);

        // Poner la dirección recta para el avance final
        direccion.write(SERVO_CENTRADO);
        delay(200); // Pausa para que el servo se estabilice

        // Condición basada en la distancia inicial
        if (distanciaTraseraInicial > 125.0) {
          // Si la distancia fue grande, avanzar 2 segundos
          Serial.println("Distancia > 125cm. Avanzando recto por 2 segundos...");
          avanzarMotor(); // Activar motor de avance
          delay(1000);

        } else {
          // Si la distancia fue corta o falló la medición, avanzar 1 segundo
          Serial.println("Distancia <= 125cm. Avanzando recto por 1 segundo...");
          avanzarMotor(); // Activar motor de avance
          delay(1000);
        }
        
        // Después del avance cronometrado, detener el robot permanentemente
        Serial.println("Avance final completado. Deteniendo robot.");
        detenerRobot(); // Llama a la función que detiene todo y entra en modo monitor
      }
      break;
  }  
  
  
  // MÁXIMA PRIORIDAD: Retroceso de emergencia (SOLO si NO está girando)
  if (prioridadActual == PRIORIDAD_RETROCESO && !giroEnProceso && estadoActual !=ESTADO_DETENCION_FINAL){
    if (retrocesoPorEmergencia) {
      procesarRetrocesoEmergencia();
    }
    return;
  }

  // Verificar y activar retroceso de emergencia (SOLO si NO está girando)
  if (!giroEnProceso && verificarRetrocesoEmergencia() && estadoActual != ESTADO_DETENCION_FINAL) {
    procesarRetrocesoEmergencia();
    return;
  }
  
 
   if (!primeraDeteccion) {
    
     // Verificar obstáculos diagonales SOLO si NO está girando
      if (!esquiveDiagonalActivo && !giroEnProceso && verificarObstaculoDiagonal()) {
         // Se activó esquive diagonal
       }
       // Procesar esquive diagonal si está activo Y NO está girando
      if (esquiveDiagonalActivo && !giroEnProceso) {
            procesarEsquiveDiagonal();
       }
  
   }

  // Mostrar el valor cada 300 ms
  // Mostrar el valor cada 300 ms
  if (millis() - lastYawPrint >= YAW_INTERVAL) {
        // Leer los sensores Sharp en este instante para el monitor
        float sharpIzq = leerSensorSharp(SHARP_DIAGONAL_IZQ_PIN);
        float sharpDer = leerSensorSharp(SHARP_DIAGONAL_DER_PIN);
        
        // --- NUEVO: Leer el ToF Frontal ---
        float tofFrontal = leerToF('F');

        Serial.print("Yaw: ");
        Serial.print(anguloZ);
        Serial.print("° | Servo: ");
        Serial.print(direccion.read());
        Serial.print("° | ");
       
        // --- NUEVO: Imprimir la lectura del ToF Frontal ---
        Serial.print("ToF Frontal: ");
        if (tofFrontal >= 999) {
            Serial.print("Fuera de rango");
        } else {
            Serial.print(tofFrontal);
            Serial.print("cm");
        }
        Serial.print(" | ");

        Serial.print("ToF Der: ");
        Serial.print(distanciaDerecha);
        Serial.print("  ToF Izq: ");
        Serial.print(distanciaIzquierda);

        // --- Agregar impresión de los Sharp ---
        Serial.print(" | Sharp Der: ");
        if (sharpDer >= 999) Serial.print("Fuera de rango");
        else Serial.print(sharpDer);
        
        Serial.print("  Sharp Izq: ");
        if (sharpIzq >= 999) Serial.println("Fuera de rango");
        else Serial.println(sharpIzq);

        lastYawPrint = millis();
  }
 
  //delay(20);
}
