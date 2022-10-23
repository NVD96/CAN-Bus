
#include <Timer.h>
#include <SoftwareSerial.h>

//bluetooth com pins
#define rxPin 19               //pin connected to Tx of HC-05
#define txPin 18               //pin connected to Rx of HC-05

//rpm led variables
#define latchPin 15
#define clockPin 16
#define dataPin 14
int count = 0;
byte leds = 0;

//display variables
#define pinA 2
#define pinB 3
#define pinC 4
#define pinD 5
#define pinE 6
#define pinF 7
#define pinG 8
#define D1 9
#define D2 10
#define D3 11
#define D4 12
int D[] = {9, 10, 11, 12};
int digitNum = 4;
int checkPin = 13;
#define obdCmdRetries 3      //Retries for OBD command if not receiving > char
#define rpmCmdRetries 5      //Retries for RPM command
#define tempCmdRetries 5     //Retries for Temp command

//OBD readings variables
int addr = 0;                //EEPROM address for storing Shift Light RPM
unsigned int rpm;            //Variables for RPM
unsigned int temp;           //Bariable for Temperature
boolean obdErrorFlag;        //Variable for OBD connection error
boolean rpmErrorFlag;        //Variable for RPM error
boolean rpmRetries;          //Variable for RPM cmd retries
boolean tempRetries;
boolean tempErrorFlag;

SoftwareSerial blueToothSerial(rxPin, txPin);
Timer tRpm;
Timer tTemp;


//-----------------------------------------------------//
void setup()
{
  //Setup bt pins
  pinMode(rxPin, INPUT);
  pinMode(txPin, OUTPUT);

  //Setup 74HC595 pins
  pinMode(checkPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);

  ledDemo();

  //Setup Display pins
  pinMode(pinA, OUTPUT);
  pinMode(pinB, OUTPUT);
  pinMode(pinC, OUTPUT);
  pinMode(pinD, OUTPUT);
  pinMode(pinE, OUTPUT);
  pinMode(pinF, OUTPUT);
  pinMode(pinG, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);
  digitalWrite(D1, LOW);
  digitalWrite(D2, LOW);
  digitalWrite(D3, LOW);
  digitalWrite(D4, LOW);

  //Start Serial and Bluetooth connection
  Serial.begin(9600);
  Serial.println("Arduino is ready");
  blueToothSerial.begin(9600);
  Serial.println("BTSerial started");


  rpmRetries = 0;
  tRpm.every(200, rpmCalc); //RPM read period
  tTemp.every(1900, tempCalc); //Temperature read period

  //OBD Initialization
  obdInit();

  //in case of OBDII connection error
  if (obdErrorFlag) {
    obdErrorFlash();
  }
}

//Led control

void updateShiftRegister()
{
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, leds);
  digitalWrite(latchPin, HIGH);
}

void ledDemo() {
  for (int i = 0; i < 5; i++) {
    bitSet(leds, i);
    updateShiftRegister();
    delay(100);
  }

  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, B00000000);
  digitalWrite(latchPin, HIGH);
  delay(100);
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, B00011111);
  digitalWrite(latchPin, HIGH);
  delay(100);
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, B00000000);
  digitalWrite(latchPin, HIGH);
  delay(100);
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, B00011111);
  digitalWrite(latchPin, HIGH);
  delay(300);

  for (int i = 4; i > 0; i--) {
    bitClear(leds, i);
    updateShiftRegister();
    delay(100);
  }
}

//Number of LED switched on by RPM
void led(int rpm)
{
  leds = 0;
  int i = 0;
  int m = rpm / 1000;
  int c = (rpm - (m * 1000)) / 100;
  int d = (rpm - (m * 1000 + c * 100)) / 10;
  int u = (rpm - (m * 1000 + c * 100 + d * 10) );
  if (rpm <= 1000) {
    count = 0;
  }
  if (rpm <= 2000 && rpm > 1000) {
    count = 1;
  }
  if (rpm > 2000 && rpm <= 2600) {
    count = 2;
  }
  if (rpm > 2600 && rpm <= 3000) {
    count = 3;
  }
  if (rpm > 3000 && rpm <= 3500) {
    count = 4;
  }
  if (rpm > 3500) {
    count = 5;
  }
  if (count < 5) {
    for (int i = 0; i <= count; i++) {
      bitSet(leds, i);
      updateShiftRegister();
    }
  } else {
    while (i < 10) {
      digitalWrite(latchPin, LOW);
      shiftOut(dataPin, clockPin, MSBFIRST, B00011111);
      digitalWrite(latchPin, HIGH);
      number(1, m);
      number(2, c);
      number(3, d);
      number(4, u);
      delay(1);
      i++;
    }
    i = 0;
    while (i < 10) {
      digitalWrite(latchPin, LOW);
      shiftOut(dataPin, clockPin, MSBFIRST, B00000000);
      digitalWrite(latchPin, HIGH);
      number(1, m);
      number(2, c);
      number(3, d);
      number(4, u);
      i++;
      delay(1);
    }
  }
}

//Slow RED light flash in case of error while connecting to OBD2
//Restart ARDUINO

void obdErrorFlash() {
  while (1) {
    digitalWrite(checkPin, HIGH);
    delay(1000);
    digitalWrite(checkPin, LOW);
    delay(1000);
  }
}


//Read RPM from OBD and decode it into readable number

void rpmCalc() {
  boolean prompt, valid;
  char recvChar;
  char bufin[15];
  int i;

  if (!(obdErrorFlag)) {                                  //if connected to OBD

    valid = false;
    prompt = false;
    blueToothSerial.print("010C1");                        //send to OBD PID command 010C for ROM and 1 to wait for an answer
    blueToothSerial.print("\r");                           //send to OBD cariage return char
    while (blueToothSerial.available() <= 0);              //wait until some data comes from ELM
    i = 0;
    while ((blueToothSerial.available() > 0) && (!prompt)) {
      recvChar = blueToothSerial.read();                   //read from ELM
      if ((i < 15) && (!(recvChar == 32))) {               //the normal respond to previus command is 010C1/r41 0C ?? ??>, so count 15 chars and ignore char 32 which is space
        bufin[i] = recvChar;
        i = i + 1;
      }
      if (recvChar == 62) prompt = true;                   //if received char is 62 which is '>' then prompt is true, which means that ELM response is finished
    }

    if ((bufin[6] == '4') && (bufin[7] == '1') && (bufin[8] == '0') && (bufin[9] == 'C')) { //if first four chars after our command is 410C->correct response
      valid = true;
    } else {
      valid = false;
    }
    if (valid) {
      rpmRetries = 0;                                                            //reset tries
      rpmErrorFlag = false;                                                      //set rpm error flag to false

      //start calculation of real RPM value
      //RPM is coming from OBD in two 8bit(bytes) hex numbers for example A=0B and B=6C
      //the equation is ((A * 256) + B) / 4, so 0B=11 and 6C=108
      //so rpm=((11 * 256) + 108) / 4 = 731 a normal idle car engine rpm
      rpm = 0;
      for (i = 10; i < 14; i++) {                       //in that 4 chars of bufin array which is the RPM value
        if ((bufin[i] >= 'A') && (bufin[i] <= 'F')) {   //if char is between 'A' and 'F'
          bufin[i] -= 55;                               //'A' is int 65 minus 55 gives 10 which is int value for hex A
        }

        if ((bufin[i] >= '0') && (bufin[i] <= '9')) {   //if char is between '0' and '9'
          bufin[i] -= 48;                               //'0' is int 48 minus 48 gives 0 same as hex
        }

        rpm = (rpm << 4) | (bufin[i] & 0xf);            //shift left rpm 4 bits and add the 4 bits of new char

      }
      rpm = rpm >> 2;                                   //finaly shift right rpm 2 bits, rpm=rpm/4
    }
    Serial.println(rpm);
  }
  if (!valid) {                                            //in case of incorrect RPM response
    rpmErrorFlag = true;                                   //set rpm error flag to true
    rpmRetries += 1;                                       //add 1 retry
    rpm = 0;                                               //set rpm to 0
    Serial.println("RPM_ERROR");
    if (rpmRetries >= rpmCmdRetries) obdErrorFlag = true; //if retries reached rpmCmdRetries limit then set obd error flag to true
  }
}


void tempCalc() {
  boolean prompt, valid;
  char recvChar;
  char bufin[15];
  int i;

  if (!(obdErrorFlag)) {                                  //if connected to OBD

    valid = false;
    prompt = false;
    blueToothSerial.print("01051");                        //send to OBD PID command 0105 for temp and 1 to wait for an answer
    blueToothSerial.print("\r");                           //send to OBD cariage return char
    while (blueToothSerial.available() <= 0);              //wait until some data comes from ELM
    i = 0;
    while ((blueToothSerial.available() > 0) && (!prompt)) {
      recvChar = blueToothSerial.read();                   //read from ELM
      if ((i < 13) && (!(recvChar == 32))) {               //the normal respond to previus command is 01051/r41 0C ?? >, so count 13 chars and ignore char 32 which is space
        bufin[i] = recvChar;
        i = i + 1;
      }
      if (recvChar == 62) prompt = true;                   //if received char is 62 which is '>' then prompt is true, which means that ELM response is finished
    }

    if ((bufin[6] == '4') && (bufin[7] == '1') && (bufin[8] == '0') && (bufin[9] == '5')) { //if first four chars after our command is 4105->correct response
      valid = true;
    } else {
      valid = false;
    }
    if (valid) {
      tempRetries = 0;                                                            //reset tries
      tempErrorFlag = false;                                                      //set temp error flag to false

      //start calculation of real temp value

      temp = 0;
      for (i = 10; i < 12; i++) {                       //in that 4 chars of bufin array which is the RPM value
        if ((bufin[i] >= 'A') && (bufin[i] <= 'F')) {   //if char is between 'A' and 'F'
          bufin[i] -= 55;                               //'A' is int 65 minus 55 gives 10 which is int value for hex A
        }

        if ((bufin[i] >= '0') && (bufin[i] <= '9')) {   //if char is between '0' and '9'
          bufin[i] -= 48;                               //'0' is int 48 minus 48 gives 0 same as hex
        }
      }
      temp = bufin[10] * 16 + bufin[11] - 40;
    }
    Serial.println(temp);
  }
  if (!valid) {                                            //in case of incorrect temp response
    tempErrorFlag = true;                                   //set temp error flag to true
    tempRetries += 1;                                       //add 1 retry
    temp = 0;                                               //set temp to 0
    Serial.println("TEMP ERROR");
    if (tempRetries >= tempCmdRetries) obdErrorFlag = true; //if retries reached tempCmdRetries limit then set obd error flag to true
  }
}

//---------------------Send OBD Command---------------------//
//--------------------for initialitation--------------------//

void send_OBD_cmd(char *obd_cmd) {
  char recvChar;
  boolean prompt;
  int retries;

  if (!(obdErrorFlag)) {                                       //if no OBD connection error

    prompt = false;
    retries = 0;
    while ((!prompt) && (retries < obdCmdRetries)) {            //while no prompt and not reached OBD cmd retries
      blueToothSerial.print(obd_cmd);                             //send OBD cmd
      blueToothSerial.print("\r");                                //send cariage return

      while (blueToothSerial.available() <= 0);                   //wait while no data from ELM

      while ((blueToothSerial.available() > 0) && (!prompt)) {    //while there is data and not prompt
        recvChar = blueToothSerial.read();                        //read from elm
        if (recvChar == 62) prompt = true;                        //if received char is '>' then prompt is true
      }
      retries = retries + 1;                                      //increase retries
      delay(2000);
    }
    if (retries >= obdCmdRetries) {                             // if OBD cmd retries reached
      obdErrorFlag = true;                                      // obd error flag is true
    }
  }
}

//----------------initialitation of OBDII-------------------//
void obdInit() {

  obdErrorFlag = false;   // obd error flag is false

  send_OBD_cmd("ATZ");      //send to OBD ATZ, reset
  delay(1000);
  send_OBD_cmd("ATSP0");    //send ATSP0, protocol auto

  send_OBD_cmd("0100");     //send 0100, retrieve available pid's 00-19
  delay(1000);
  send_OBD_cmd("0120");     //send 0120, retrieve available pid's 20-39
  delay(1000);
  send_OBD_cmd("0140");     //send 0140, retrieve available pid's 40-??
  delay(1000);
  send_OBD_cmd("010C1");    //send 010C1, RPM cmd
  delay(1000);
  send_OBD_cmd("01051");    //send 01051, Temp cmd
  delay(1000);
}


void loop() {

  while (!(obdErrorFlag)) {           //while no OBD comunication error
    if ((rpm >= 0) && (rpm < 6000)) {  //if rpm value is between 0 and 6000
      led(rpm);
    }

    tRpm.update();  //update of timer for calling rpmCalc
    tTemp.update();  //update of timer for calling tempCalc
  }
  if (obdErrorFlag) obdErrorFlash();    //if OBD error flag is true

}


//Numbers mapping
void number(int digit, int number) {
  for (int i = 0; i < digitNum; i++) {
    if (digit - 1 == i) {
      digitalWrite(D[i], LOW);
    } else {
      digitalWrite(D[i], HIGH);
    }
  }

  switch (number) {
    case 1: {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, LOW);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, LOW);
        digitalWrite(pinG, LOW);

        digitalWrite(pinA, LOW);
        digitalWrite(pinB, HIGH);
        digitalWrite(pinC, HIGH);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, LOW);
        digitalWrite(pinG, LOW);
        delay(1);
        break;
      }

    case 2: {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, LOW);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, LOW);
        digitalWrite(pinG, LOW);

        digitalWrite(pinA, HIGH);
        digitalWrite(pinB, HIGH);
        digitalWrite(pinC, LOW);
        digitalWrite(pinD, HIGH);
        digitalWrite(pinE, HIGH);
        digitalWrite(pinF, LOW);
        digitalWrite(pinG, HIGH);
        delay(1);
        break;
      }

    case 3: {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, LOW);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, LOW);
        digitalWrite(pinG, LOW);

        digitalWrite(pinA, HIGH);
        digitalWrite(pinB, HIGH);
        digitalWrite(pinC, HIGH);
        digitalWrite(pinD, HIGH);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, LOW);
        digitalWrite(pinG, HIGH);
        delay(1);
        break;
      }

    case 4: {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, LOW);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, LOW);
        digitalWrite(pinG, LOW);

        digitalWrite(pinA, LOW);
        digitalWrite(pinB, HIGH);
        digitalWrite(pinC, HIGH);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, HIGH);
        digitalWrite(pinG, HIGH);
        delay(1);
        break;
      }
    case 5: {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, LOW);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, LOW);
        digitalWrite(pinG, LOW);

        digitalWrite(pinA, HIGH);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, HIGH);
        digitalWrite(pinD, HIGH);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, HIGH);
        digitalWrite(pinG, HIGH);
        delay(1);
        break;
      }

    case 6: {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, LOW);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, LOW);
        digitalWrite(pinG, LOW);

        digitalWrite(pinA, HIGH);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, HIGH);
        digitalWrite(pinD, HIGH);
        digitalWrite(pinE, HIGH);
        digitalWrite(pinF, HIGH);
        digitalWrite(pinG, HIGH);
        delay(1);
        break;
      }

    case 7: {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, LOW);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, LOW);
        digitalWrite(pinG, LOW);

        digitalWrite(pinA, HIGH);
        digitalWrite(pinB, HIGH);
        digitalWrite(pinC, HIGH);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, LOW);
        digitalWrite(pinG, LOW);
        delay(1);
        break;
      }

    case 8: {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, LOW);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, LOW);
        digitalWrite(pinG, LOW);

        digitalWrite(pinA, HIGH);
        digitalWrite(pinB, HIGH);
        digitalWrite(pinC, HIGH);
        digitalWrite(pinD, HIGH);
        digitalWrite(pinE, HIGH);
        digitalWrite(pinF, HIGH);
        digitalWrite(pinG, HIGH);
        delay(1);
        break;
      }

    case 9: {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, LOW);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, LOW);
        digitalWrite(pinG, LOW);

        digitalWrite(pinA, HIGH);
        digitalWrite(pinB, HIGH);
        digitalWrite(pinC, HIGH);
        digitalWrite(pinD, HIGH);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, HIGH);
        digitalWrite(pinG, HIGH);
        delay(1);
        break;
      }

    case 0: {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, LOW);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, LOW);
        digitalWrite(pinG, LOW);

        digitalWrite(pinA, HIGH);
        digitalWrite(pinB, HIGH);
        digitalWrite(pinC, HIGH);
        digitalWrite(pinD, HIGH);
        digitalWrite(pinE, HIGH);
        digitalWrite(pinF, HIGH);
        digitalWrite(pinG, LOW);
        delay(1);
        break;
      }
  }
}

//Letter mapping
void lettera(int digit, char lettera) {
  for (int i = 0; i < digitNum; i++) {
    if (digit - 1 == i) {
      digitalWrite(D[i], LOW);
    } else {
      digitalWrite(D[i], HIGH);
    }
  }

  switch (lettera) {
    case 'a': {
        digitalWrite(pinA, HIGH);
        digitalWrite(pinB, HIGH);
        digitalWrite(pinC, HIGH);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, HIGH);
        digitalWrite(pinF, HIGH);
        digitalWrite(pinG, HIGH);
        delay(1);
        break;
      }

    case 'b': {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, HIGH);
        digitalWrite(pinD, HIGH);
        digitalWrite(pinE, HIGH);
        digitalWrite(pinF, HIGH);
        digitalWrite(pinG, HIGH);
        delay(1);
        break;
      }

    case 'c': {
        digitalWrite(pinA, HIGH);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, LOW);
        digitalWrite(pinD, HIGH);
        digitalWrite(pinE, HIGH);
        digitalWrite(pinF, HIGH);
        digitalWrite(pinG, LOW);
        delay(1);
        break;
      }

    case 'f': {
        digitalWrite(pinA, HIGH);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, LOW);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, HIGH);
        digitalWrite(pinF, HIGH);
        digitalWrite(pinG, HIGH);
        delay(1);
        break;
      }
    case 'h': {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, HIGH);
        digitalWrite(pinC, HIGH);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, HIGH);
        digitalWrite(pinF, HIGH);
        digitalWrite(pinG, HIGH);
        delay(1);
        break;
      }

    case 'o': {
        digitalWrite(pinA, HIGH);
        digitalWrite(pinB, HIGH);
        digitalWrite(pinC, HIGH);
        digitalWrite(pinD, HIGH);
        digitalWrite(pinE, HIGH);
        digitalWrite(pinF, HIGH);
        digitalWrite(pinG, LOW);
        delay(1);
        break;
      }


    case 'r': {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, LOW);
        digitalWrite(pinD, LOW);
        digitalWrite(pinE, HIGH);
        digitalWrite(pinF, LOW);
        digitalWrite(pinG, HIGH);
        delay(1);
        break;
      }
    case 's': {
        digitalWrite(pinA, HIGH);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, HIGH);
        digitalWrite(pinD, HIGH);
        digitalWrite(pinE, LOW);
        digitalWrite(pinF, HIGH);
        digitalWrite(pinG, HIGH);
        delay(1);
        break;
      }

    case 't': {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, LOW);
        digitalWrite(pinC, LOW);
        digitalWrite(pinD, HIGH);
        digitalWrite(pinE, HIGH);
        digitalWrite(pinF, HIGH);
        digitalWrite(pinG, HIGH);
        delay(1);
        break;
      }
  }
}



