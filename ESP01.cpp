/**********************************************************************
  ) 2021 Carlos P

  ; * FileName:        ESP01.cpp
  ; * Dependencies:    Header
  ; * Processor:       ATMEGA328p
  ; * Compiler:        Arduino IDE
  ; *
   REVISION HISTORY:
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   Author           Date      Comments on this revision
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   Carlos Piccolini           10/11/21      Creation
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   Code Description:
   El objetivo de esta librería es la de poder ser de utilidad para la 
   configuración y manjeo de los módulos ESP01 basados en el mcu 
   ESP8266
 *************************************************************************/

/******************** Includes ********************/
#include "ESP01.h"

/******************** Methods ********************/

/*!
 * Object constructor
 * @param : rx: Serial port RX, tx: Serial port tx
 * By defect, this library use the pins 2 and 3 from Arduino UNO
 * @return :-
 */
ESP01::ESP01(int rx, int tx) 
:serialPort(rx, tx)
{
}

/*!
 * Begin the serial communication
 * @param : baud rate of Serial Communicatiom between ESP01 and Arduino UNO
 * By defect, the Serial port is inicializated at 115200
 * @return : 
 */
void ESP01::begin(int baud_rate) {
   serialPort.begin(baud_rate);  // Inicialize
   while(!serialPort) {          // Wait to connection to establishes
   }
   sendCommand("AT+RST");
   delay(10);
 }

/*!
 * Configura al ESP01 como AP
 * @param : el ssid y pass que tendrán la red, y la ip de la misma
 * Por defecto usand los valores default en ESP01.h
 * @return :  
 */
void ESP01::setupAsAP(const char *ssid, const char *pass, const char *ip) {
  char command [50];
  
  // Comando AT para poner al ESP01 como AP
  sendCommand("AT+CWMODE=2"); 

  // Configuración de parámetros de la red
  strcpy(command, "AT+CWSAP=\"");
  strcat(command, ssid);
  strcat(command, "\",\"");
  strcat(command, pass);
  
  if(strcmp(pass, PASS_DEF) == 0) 
    strcat(command, "\",6,0");
  else
    strcat(command, "\",6,4");

  sendCommand(command);

  // Configuración de la ip del ESP01 en la red
  strcpy(command, "AT+CIPAP=\"");
  strcat(command, ip);
  strcat(command, "\"");
  sendCommand(command);
}

/*!
 * Configura al ESP01 en modo Servidor en el puerto 80
 * @param : 
 * @return :  
 */
void ESP01::setupServer() {
  // Habilito multiples conexiones
  sendCommand("AT+CIPMUX=1"); 

  // Creo Servidor en puerto 80
  sendCommand("AT+CIPSERVER=1,80"); 

  // Seteo Time out del server en 0
  sendCommand("AT+CIPSTO=0");  
}

/*!
 * Se envía un mensaje desde el Servidor a algún usuario conectado
 * @param : command es el mensaje a envíar y id es el id del usuario el cual por defecto es el 0
 * en el servidor, teniendo en cuenta que el mismo permite hasta 4
 * usuarios en simultaneo
 * @return :  
 */
void ESP01::serverResponse(const char *command, const char *id){
  char message[60];
  char command_len[10];
  
  // Configuro el id y longitud del mensaje
  strcpy(message, "AT+CIPSEND=");
  strcat(message, id);
  strcat(message, ",");
  strcat(message, itoa(strlen(command) + 1, command_len, 10));  // El +1 es para que en el command pueda agregar el \n, sino no lo toma como parte del mensaje
  sendCommand(message);

  // Envío el mensaje
  strcpy(message, command);
  strcat(message, "\n");
  sendCommand(message);
}

/*!
 * Cierra el socket de comunciación TCP
 * @param : 
 * @return :
 */
void ESP01::closeConnection(const char *id) {
  char command [20];
  
  strcpy(command, "AT+CIPCLOSE=");
  strcat(command, id);
  sendCommand(command);
}

/*!
 * Envía un comando AT al ESP01 por medio de puerto serie
 * @param : el comando a enviar
 * @return :
 */
void ESP01::sendCommand(const char *command) {
  
  // Envío el comando AT al ESP01
  serialPort.print(command);
  serialPort.print(F("\r\n"));
  serialPort.flush();

  // Limpio el buffer de salida del ESP01 y descarto todo lo que entre
  readESP();
  delay(1);
}

/*!
 * Envía un comando AT al ESP01 por medio de puerto serie
 * @param : el comando a enviar y un c-string de tamaño 50 que tomará lo leido en el buffer
 * @return : la respuesta al comando enviado
 */
void ESP01::sendCommand(const char *command, char *data_returned) {
  char data_read[20];
  
  // Envío el comando AT al ESP01
  serialPort.print(command);
  serialPort.print(F("\r\n"));
  serialPort.flush();

  // Limpio el buffer de salida del ESP01 y lo copio a una variable externa
  readESP(data_read);
  strcpy(data_returned, data_read);
  delay(1);
}

/*!
 * Lee el puerto tx del ESP01 hasta un máximo de 59 bytes almacenados
 * Descarta todos los valores leidos, este método sirve para limpiar el buffer
 * @param : un c-string de tamaño 60 que tomará lo leido en el buffer
 * @return : 
 */
void ESP01::readESP() {
  int count {0};

  // Espero un Time Out para determinar que no van a entrar datos
  while(!serialPort.available()) {
    count += 1;
    if(count >= READ_TIMEOUT)
      break;
  }

  // Vacio el resto del buffer
  while(serialPort.available() > 0) {
    serialPort.read();
    delay(1);
  }
}

/*!
 * Lee el puerto tx del ESP01 hasta un máximo de 59 bytes en buffer
 * Devuelve los bytes leidos en data_read
 * @param : un c-string de tamaño 60 que tomará lo leido en el buffer
 * @return : 
 */
void ESP01::readESP(char *data_read) {
  int count {0};

  // Espero un Time Out para determinar que no van a entrar datos
  while(!serialPort.available()) {
    count += 1;
    if(count >= READ_TIMEOUT)
      break;
  }

  // Leo los datos recibidos desde el ESP01
  count = 0;
  while(serialPort.available() > 0 and count < 19) {
    data_read[count] = serialPort.read();
    count++;
    delay(1);
  }

  // Agrego al final el NULL para que quede como un c-string
  // Esta será la información devuelta
  data_read[count] = "\0";

  // Vacio el resto del buffer
  while(serialPort.available() > 0) {
    serialPort.read();
    delay(1);
  }
}

/*!
 * Verifica si existe data en el buffer tx del ESP01
 * @param : 
 * @return :  Devuelve true si existe data en el buffer de tx del ESP01
 */
bool ESP01::dataAvailable() {
  return serialPort.available();
}

/*!
 * Object destructor
 * @param :-
 * @return :-
 */
ESP01::~ESP01() {
  // TODO Auto-generated destructor stub
}
