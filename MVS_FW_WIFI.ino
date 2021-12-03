//************ DESCRIPCIÓN ************//

/* Código que permite elegir que modo de operación ejecutar:
 * Modo 1: Modulación
 * Se Tx el pulso modulado cada 100mseg para poder observar las señales en las distintas etapas del Rx

 * Modo 2: Mediciones
 * Se envía una portadora de 40KHz y se activa una interrupción si el Rx recibe la portadora o si se alcanza el TIME OUT
 * Se agrega un tiempo limite de Tx, para que se envíe una cantidad fija de ciclos de portadora (modulación)
 * Se modificaron las variables que se utilizan dentro de la ISR para que sean volatiles 
 * Se desactivo el digitalWrite dentro de la interrupción
 * Se cambio como se trabaja con los contadores
 * Ahora la interrupcción de señal recibida comienza a observarse en el momento que se deja de transmitir
 * Se modificó el loop para realizar una cantidad determinada de mediciones en vez de solo una
 * Se cambio la interrupción de un FALLING a un LOW, por temas que no funcionaba muy bien con FALLING
 * Compatible con Tx v8 y Rx v8
 * Se modificó el guardado SD para que solo almacene todos los valores tomados, sin palabras ni valores promedios
 * 
 * El código está implementado en base al "Prueba_modulacion_v4.ino" y "Sistema_Completo_v3.ino"
 * 
 * A partir de la v2.0 se pretende dejar de usar la pantalla LCD y los botones para reemplazarlos por un modulo WiFI,
 * sí se conservará la memoria SD
 */
 
//************ INCLUDES ************//

#include "ESP01.h"              // Libreria para el manejo del módulo WiFi
#include <SPI.h>                // Libreria para el manejo de comunciaciones SPI (tarjeta SD)
#include <SD.h>                 // Libreria para el manejo de la tarjeta SD

//************ DEFINICIONES ************//

#define SSpin 10                  // Pin SS del módulo de la tarjeta SD
#define CS 9                      // Señal de salida que controla la modulación mediante la NAND
#define RX 4                      // Señal proveniente de la etapa final del Rx
#define RX_W 2                    // Rx de la comunicación Serie con el módulo WiFi  
#define TX_W 3                    // Tx de la comunicación Serie con el módulo WiFi  

//************ CONSTANTES ************//

const PROGMEM int timeOut = 65000;                          // Después de este tiempo se tomará como que no hubo eco (está en ticks)
const PROGMEM int transmitionTime = 3200;                   // Tiempo en useg que se transmite (está en ticks) 3200 ticks = 200useg
const PROGMEM int maxMeasureQuantity = 20;                  // Indica la cantidad máxima que acepta el programa por el ancho del array creado
const PROGMEM int measureValues[] = {5, 10, 20};            // Almacena los valores válidos de cantidad de muestras a elegir que se muestra en pantalla
const char SSID_MVS[] = "ESP01_wifi3";              // SSID de la red
const char PASS_MVS[] = "matraca96";                // Pass de la red

//************ VARIABLES GLOBALES ************//

volatile bool received = false;               // Indica si el eco ya fue recibido 
volatile unsigned long overflowCount = 0;     // Se encarga de aumentar en +1 cada vez que el contador del Timer1 llega a un overflow 
unsigned long startTime = 0;                  // El momento en el que se comenzó a transmitir
volatile unsigned long finishTime = 0;        // El momento en que llegó la señal 
File archivo;                                 // Creo objeto del tipo File para la escritura de archivos
ESP01 wifi(RX_W, TX_W);                       // Inicializo el módulo ESP01 WiFi
char wifiData[60];                            // Datos a enviar al ESP01
char intToChar[10];                           // c-string que almacena algún int convertido a c-string

//************ DECLARACION FUNCIONES ************//

void initTimer1();
void initInterrupt();
void timeOutReached();
void eco();

//************ SETUP ************// 
 
void setup() {
  
  //************ INICIALIZACION COMUNICACION SERIE ************//
  
  Serial.begin(9600);
  Serial.println(F("Comenzando"));
  Serial.flush();
  
  //************ INICIALIZACION ESP01 ************//
  
  wifi.begin(19200);                  // Inicializo comunicación con el módulo WiFi
  wifi.setupAsAP(SSID_MVS, PASS_MVS); // Creo una red WiFi con los valores default
  wifi.setupServer();                 // Levanto un Servidor en la dirección 192.168.1.5/80
  Serial.println(F("Red y servidor levantados"));
  Serial.flush();

  // Espero a que alguien se conecte para continuar
  while(!wifi.dataAvailable()) {}
  Serial.println(F("Persona conectada"));
  Serial.flush();
  wifi.readESP();
  Serial.println(F("Buffer limpiado"));
  Serial.flush();
  wifi.serverResponse("Inicia MVS");
  Serial.println(F("Iniciando MVS"));
  Serial.flush();
  delay(1000);
  
  //************ INICIALIZACION SD ************//

  wifi.serverResponse("Iniciando SD");
  Serial.println(F("Iniciando SD"));
  Serial.flush();
  if (!SD.begin(SSpin)) {
    wifi.serverResponse("Fallo en SD");
    wifi.serverResponse("Reinicie programa");
    while(true) {} // Si falla la inicialización se termina el programa
  }
  wifi.serverResponse("SD OK");
  
  //************ PINES ************//

  Serial.println(F("Configurando pines"));
  Serial.flush();
  pinMode(CS, OUTPUT);            // Pin para el control de Tx
  pinMode(RX, INPUT);             // Pin para recibir la interrupción de llegada de señal
  digitalWrite(CS, LOW);          // Desahabilito Tx
  wifi.serverResponse("Pines configurados");
  Serial.println(F("Pines configurados"));
  Serial.flush();
}

void loop() {
  //************ VARIABLES LOCALES ************//
  
  bool transmited = false;  // Indica si ya se terminó de transmitir
  bool chosed = false;      // Indica si ya se eligió la cantidad de muestras o el modo
  int chosedValue = 0;      // Lugar de vector measureValues[]
  int modValue = 1;         // Modo de operación (1:Modulación 2:Medición)

  //************ MODO DE OPERACION ************//
  
  wifi.serverResponse("Modo funcionamiento");
  wifi.serverResponse("M1 - Modulacion");
  wifi.serverResponse("M2 - Mediciones");
  wifi.serverResponse("Esperando...");
  
  while (!chosed){
    // Espero que envían algún comando al ESP01
    while(!wifi.dataAvailable()) {
    }
    
    // Leo la data recibida por el ESP01
    wifi.readESP(wifiData);
      
    // Logica a partir dela información recibida
    if(strstr(wifiData, "/D?=1&")) {
      modValue = 1;
      chosed = true;
      wifi.serverResponse("Modo Modulacion elegido");
    }
    else if(strstr(wifiData, "/D?=2&")) {
      modValue = 2;
      chosed = true;
      wifi.serverResponse("Modo Medicion elegido");
    }
  }
  
  switch(modValue) {
    
      //************ MODULACION ************//
      
      case 1:
        while(true){              // Se habilita la Tx durante un tiempo de 200useg, a intervalos de 100mseg
          digitalWrite(CS, HIGH);
          delayMicroseconds(200);
          digitalWrite(CS, LOW);
          delay(100);
        }
      break;

      //************ MEDICION ************//
      
      case 2:
      
        int measureQuantity = 0;                                          // Indica la cantidad de mediciones a realizar
        double measureAverage = 0;                                        // Indica el promedio en las mediciones 
        double measureTime[pgm_read_word_near(maxMeasureQuantity)] = {};  // Arreglo que engloba a todas las mediciones realizadas
        
        //************ INDICO CANTIDAD DE MUESTRAS ************//
        chosed = false;                                       // Determinación de cantidad de mediciones 
        wifi.serverResponse("Ingrese cantidad de muestras:");       // El máximo se debe a que está limtiado por el ancho máximo del array

        while (!chosed){
           // Espero que ingrese data por el ESP01
           while(!wifi.dataAvailable()) {}
          
           // Leo la data recibida por el ESP01
           wifi.readESP(wifiData);
              
           // Logica a partir dela información recibida
           if(strstr(wifiData, "/D?=1&")) {
             chosedValue = 0;
             chosed = true;
             measureQuantity = pgm_read_word_near(measureValues + chosedValue);
           }
           else if(strstr(wifiData, "/D?=2")) {
             chosedValue = 1;
             chosed = true;
             measureQuantity = pgm_read_word_near(measureValues + chosedValue);
           }
        }

        strcpy(wifiData, "Se eligieron ");
        strcat(wifiData, itoa(pgm_read_word_near(measureValues + chosedValue), intToChar, 10));
        strcat(wifiData, " cantidad de muestras");
        wifi.serverResponse(wifiData);
        wifi.serverResponse("Comienza la medicion");
        delay(2000);
  
        //************ TIMER ************//
  
        initTimer1(); //Inicializo el Timer1
  
        //************ COMIENZO TX ************//
  
        for (int i = 0 ; i < measureQuantity ; i++) {       // Iteración para cada medición
           initInterrupt();                                  // NO BORRAR
           startTime = (overflowCount << 16) + TCNT1;        // Almaceno la cantidad de ticks del contador, esta manera de hacerlo es para obtener un valor preciso, teniendo en cuenta la cantidad de overflows y los ticks actuales del contador
           digitalWrite(CS, HIGH);                           // Activo el pulso modulante
  
          //************ ESPERANDO INTERRUPCIÓN ************//
    
          while (!received){
            finishTime = (overflowCount << 16) + TCNT1; // Almacena el tiempo actual del contador del Timer1
      
            if (finishTime - startTime >= pgm_read_word_near(timeOut)){     // Salta el timeOut
              timeOutReached();
            }
            
            if ((finishTime - startTime >= pgm_read_word_near(transmitionTime)) && !transmited){  // Dejo de transmitir y solo espero que la señal llegue o salte el timeOut
              digitalWrite(CS, LOW);    // Dejo de transmitir
              transmited = true;        // Dejo en claro que deje de transmitir        
            }  
          }  

          //************ ALMACENAMIENTO ************//
    
          measureTime[i] = (finishTime - startTime) * 62.5 / 1000; // El tiempo medido fue en ticks del contador del Timer1, eso se lo multiplica por 62.5 porque cada tick equivalen a 62.5ns, y se lo divide por 1000 para pasarlo a useg
          received = false; 
          transmited = false;   
    
          //************ VISUALIZACIÓN ************// 

          strcpy(wifiData, "Medicion actual: ");
          strcat(wifiData, itoa(measureTime[i], intToChar, 10));
          strcat(wifiData, " us");
          wifi.serverResponse(wifiData);
    
          if(measureTime[i] > 1000) { // Por si surge un error doy tiempo a que el sistema se acomode
            digitalWrite(CS, LOW);
            delay(5000);
          }
      
          digitalWrite(CS, LOW);  // Dejo de transmitir
          delay(2000);            // Cuando volvemos a medir, si justo quedó un rebote en el canal el circuito lo agarra antes de dejar de transmitir y ahí entra en un loop
        }

        //************ CALCULOS ************//
  
        for (int i=0 ; i < measureQuantity ; i++)
           measureAverage += measureTime[i];
        measureAverage = measureAverage/measureQuantity;
        strcpy(wifiData, "Valor promedio:");
        strcat(wifiData, itoa(measureAverage, intToChar, 10));
        strcat(wifiData, " us");
        wifi.serverResponse(wifiData);
        delay(5000);

        //************ ESCRITURA SD ************//
  
        archivo = SD.open("prueba1.txt", FILE_WRITE);             // A partir del objeto file creado, se crea un archivo en la memoria SD
        wifi.serverResponse("Escribiendo en SD...");
        if (archivo) {                                            // De abrirse correctamente, comienza a escribirse un archivo en la tarjeta SD
          archivo.print(F("Mediciones: ["));
          archivo.print(measureQuantity);
          archivo.println(F("]"));
          for (int i=0 ; i < measureQuantity ; i++)
            archivo.println(measureTime[i]); 
          archivo.println();
          archivo.close();                                        // Al terminar la escritura se cierra el archivo
          wifi.serverResponse("Escritura finalizada");
        }
        else {
          wifi.serverResponse("Error al abrir archivo");
        }

        delay(2500);
        wifi.serverResponse("********FIN*********");
        wifi.closeConnection();

        //************ FIN ************//
  
        while(true){}
  
      break;
 }
}


//************ FUNCIONES ************//

void initTimer1(){                                       // Función encargada de reiniciar e inicializar el Timer1
  TCCR1A = 0;           // Reinicio Timer1, dejando sus registros de control A y B en 0
  TCCR1B = 0;
  TIMSK1 = bit (TOIE1); // Configuro el Timer1 para que cree una interrupción al overflow el contador
  TCNT1 = 0;            // Reinicio el contador del Timer1  
  overflowCount = 0;  
  TCCR1B =  bit (CS10); // Inicio el conteo de trailer a la frecuencia del clock de 16MHz sin preescalares  
}

void initInterrupt(){                                    // Función encargada de inicializar la interrupción que indica que la señal llegó a Rx  
  EIFR = bit (INTF0);                                    // Limpia el flag que indica que hubo una interrupción
  attachInterrupt(digitalPinToInterrupt(RX), eco, LOW);  // Inicializa la interrupción de la señal en Rx     
}

void timeOutReached(){                        // Funcion encargada de ejecutarse cuando se alcanza el time out
  detachInterrupt(digitalPinToInterrupt(RX)); // Desactivo la interrupción
  received = true;                            // Modifico para salir del loop
  finishTime = startTime;                        
}

//************ ISR ************//

void eco(){
  finishTime = (overflowCount << 16) + TCNT1; // Almaceno el tiempo actual del contador del Timer1
  received = true;
  detachInterrupt(digitalPinToInterrupt(RX)); // Desactivo la interrupción
}

ISR (TIMER1_OVF_vect){  // ISR de arduino que ingresa cada vez que el contador del Timer1 TCNT1 sufre un overflow y se reinicia
  overflowCount++;
} 
