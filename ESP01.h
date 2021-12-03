/**********************************************************************
  ) 2021 Carlos P

  ; * FileName:        ESP01.h
  ; * Dependencies:    
  ; * Processor:       ATMEGA328p
  ; * Compiler:        Arduino IDE
  ; *
   REVISION HISTORY:
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   Author           Date      Comments on this revision
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   Carlos Piccolini         10/11/21        Creation
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   Code Description:
   El objetivo de esta librería es la de poder ser de utilidad para la 
   configuración y manjeo de los módulos ESP01 basados en el mcu 
   ESP8266
 *************************************************************************/

#ifndef ESP01_H_
#define ESP01_H_

/******************** Includes ********************/
#include <SoftwareSerial.h> // Librería para crear Puertos Series en pines de Arduino que no sean el 0 y 1
#include <Arduino.h>        // Librería conmuchas funciones de Arduino

/******************** Constants ********************/
#define SSID_DEF "ESP01_wifi"
#define PASS_DEF ""
#define IP_DEF "192.168.1.5"
#define READ_TIMEOUT 1000000


/******************** Class ********************/

class ESP01 {
private:
  SoftwareSerial serialPort;  
public:
  ESP01(int rx=2, int tx=3);  
          
  void begin(int baud_rate=115200); 
    
  void setupAsAP(const char *ssid=SSID_DEF, const char *pass=PASS_DEF, const char *ip=IP_DEF);
  void setupServer();
  void serverResponse(const char *command, const char *id="0");
  void closeConnection(const char *id="0");

  void sendCommand(const char *command);
  void sendCommand(const char *command, char *data_returned); // Overloading
  void readESP(); 
  void readESP(char *data_read);  // Overloading

  bool dataAvailable();
  
  virtual ~ESP01();

  
};

#endif /* ESP01_H_ */
