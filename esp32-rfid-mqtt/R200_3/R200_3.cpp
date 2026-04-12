#include <Arduino.h>
#include "R200_3.h"

// Buzzer-Pin (ggf. anpassen)
#define R200_BUZZER_PIN 4

// --- kleine Hilfsfunktionen fürs Debugging ---

static void printHexByte(const char* name, uint8_t value){
  Serial.print(name);
  Serial.print(":");
  Serial.print(value < 0x10 ? "0x0" : "0x");
  Serial.println(value, HEX);
}

static void printHexBytes(const char* name, uint8_t *value, uint8_t len){
  Serial.print(name);
  Serial.print(":0x");
  for(int i=0; i<len; i++){
    Serial.print(value[i] < 0x10 ? "0" : "");
    Serial.print(value[i], HEX);
  }
  Serial.println();
}

static void printHexWord(const char* name, uint8_t MSB, uint8_t LSB){
  Serial.print(name);
  Serial.print(":");
  Serial.print(MSB < 0x10 ? "0x0" : "0x");
  Serial.println(MSB, HEX);
  Serial.print(LSB < 0x10 ? "0" : "");
  Serial.println(LSB, HEX);
}

// --- Klasse R200_3 ---

R200_3::R200_3(){}

bool R200_3::begin(HardwareSerial *serial, int baud, uint8_t RxPin, uint8_t TxPin){
  _serial = serial;
  _serial->begin(baud, SERIAL_8N1, RxPin, TxPin);

  pinMode(R200_BUZZER_PIN, OUTPUT);
  digitalWrite(R200_BUZZER_PIN, LOW);

  Serial.println("R200_3::begin() bereit");
  return true;
}

void R200_3::beep() {
  digitalWrite(R200_BUZZER_PIN, HIGH);
  delay(150);
  digitalWrite(R200_BUZZER_PIN, LOW);
}

// Hat der Reader irgendwas geschickt?
bool R200_3::dataAvailable(){
  return _serial->available() > 0;
}

void R200_3::loop(){
  // Neue Daten vom Reader?
  if(dataAvailable()){
    // Versuchen, einen kompletten Frame einzulesen
    if(receiveData()){
      if(dataIsValid()){
        switch(_buffer[R200_CommandPos]){

          case CMD_GetModuleInfo:
            // Datenbereich als String ausgeben
            for (uint8_t i=0; i<RX_BUFFER_LENGTH-8; i++) {
              Serial.print((char)_buffer[6 + i]);
              // Stop, wenn nur noch CRC + FrameEnd kommen
              if (_buffer[8 + i] == R200_FrameEnd) {
                break;
              }
            }
            Serial.println();
            break;

          case CMD_SinglePollInstruction:
            // Beispiel erfolgreicher Response:
            // AA 02 22 00 11 C7 30 00 E2 80 68 90 00 00 50 0E 88 C6 A4 A7 11 9B 29 DD 

            #ifdef DEBUG
              printHexByte("RSSI", _buffer[6]);
              printHexWord("PC", _buffer[7], _buffer[8]);
              printHexBytes("EPC", &_buffer[9], 12);
            #endif

            // Prüfen, ob der empfangene EPC != gespeicherter EPC ist (neuer Tag)
            if(memcmp(uid, &_buffer[9], 12) != 0) {
              // Neuen Tag in uid speichern
              memcpy(uid, &_buffer[9], 12);

              // Ausgabe im Serial Monitor
              Serial.print("Neuer Tag erkannt: ");
              dumpUIDToSerial();
              Serial.println();

              // Einmaliger Piepton
              beep();
            }
            else {
              #ifdef DEBUG
                Serial.print("Gleicher Tag noch vorhanden: ");
                dumpUIDToSerial();
                Serial.println();
              #endif
            }

            #ifdef DEBUG
              printHexWord("CRC", _buffer[20], _buffer[21]);
            #endif
            break;

          case CMD_ExecutionFailure:
            switch(_buffer[R200_ParamPos]){
              case ERR_CommandError:
                #ifdef DEBUG
                  Serial.println("Command error");
                #endif
                break;

              case ERR_InventoryFail:
                // Kein Tag in Reichweite
                // Wenn vorher einer da war → als entfernt melden
                if(memcmp(uid, blankUid, sizeof uid) != 0) {
                  Serial.print("Tag entfernt: ");
                  dumpUIDToSerial();
                  Serial.println();

                  memset(uid, 0, sizeof uid);
                }
                break;

              case ERR_AccessFail:
              case ERR_ReadFail:
              case ERR_WriteFail:
              default:
                #ifdef DEBUG
                  Serial.print("Execution failure, code: 0x");
                  Serial.println(_buffer[R200_ParamPos], HEX);
                #endif
                break;
            }
            break;

          default:
            #ifdef DEBUG
              Serial.print("Unbekannter Command: 0x");
              Serial.println(_buffer[R200_CommandPos], HEX);
            #endif
            break;
        }
      }
      else {
        #ifdef DEBUG
          Serial.println("Ungültige CRC / Frame verworfen");
        #endif
      }
    }
  }
}

// Checksumme prüfen
bool R200_3::dataIsValid(){
  uint8_t CRC = calculateCheckSum(_buffer);

  uint16_t paramLength = _buffer[3];
  paramLength <<= 8;
  paramLength += _buffer[4];
  uint8_t CRCpos = 5 + paramLength;

  return (CRC == _buffer[CRCpos]);
}

/*
 * Letzte UID im Serial Monitor ausgeben
 */ 
void R200_3::dumpUIDToSerial(){
  Serial.print("0x");
  for (uint8_t i=0; i<12; i++){
    Serial.print(uid[i] < 0x10 ? "0" : "");
    Serial.print(uid[i], HEX);
  }
}

void R200_3::dumpReceiveBufferToSerial(){
  Serial.print("RX-Buffer: 0x");
  for (uint8_t i=0; i<RX_BUFFER_LENGTH; i++){
    Serial.print(_buffer[i] < 0x10 ? "0" : "");
    Serial.print(_buffer[i], HEX);
  }
  Serial.println(". Done.");
}

// (optional, falls du sie doch benutzen willst)
bool R200_3::parseReceivedData() {
  switch(_buffer[R200_CommandPos]){
    case CMD_GetModuleInfo:
      // aktuell nicht weiter ausgewertet
      break;

    case CMD_SinglePollInstruction:
    case CMD_MultiplePollInstruction:
      for(uint8_t i=8; i<20; i++) {
        uid[i-8] = _buffer[i];
      }
      break;

    case CMD_ExecutionFailure:
    default:
      break;
  }
  return true;
}

/*
 * Achtung: Arduino Serial.flush() leert nicht den Eingangsbuffer!
 */
uint8_t R200_3::flush(){
  uint8_t bytesDiscarded = 0;
  while(_serial->available()){
    _serial->read();
    bytesDiscarded++;
  }
  return bytesDiscarded;
}

// Liest einen kompletten Frame vom Reader
bool R200_3::receiveData(unsigned long timeOut){
  unsigned long startTime = millis();
  uint8_t bytesReceived = 0;

  // Buffer leeren
  for (int i = 0; i < RX_BUFFER_LENGTH; i++) {
    _buffer[i] = 0;
  }

  while ((millis() - startTime) < timeOut) {
    while (_serial->available()) {
      uint8_t b = _serial->read();

      if(bytesReceived > RX_BUFFER_LENGTH - 1) {
        Serial.println("Error: Max Buffer Length Exceeded!");
        flush();
        return false;
      } else {
        _buffer[bytesReceived] = b;
      }

      bytesReceived++;

      if (b == R200_FrameEnd) {
        // Ende des Frames erreicht
        break;
      }
    }

    // Wenn wir schon ein Ende-Zeichen gesehen haben, können wir raus
    if (bytesReceived > 0 && _buffer[bytesReceived - 1] == R200_FrameEnd) {
      break;
    }
  }

  if (bytesReceived > 1 && 
      _buffer[0] == R200_FrameHeader && 
      _buffer[bytesReceived - 1] == R200_FrameEnd) {
    return true;
  } else {
    return false;
  }
}

void R200_3::dumpModuleInfo(){
  uint8_t commandFrame[8] = {0};
  commandFrame[0] = R200_FrameHeader;
  commandFrame[1] = FrameType_Command;
  commandFrame[2] = CMD_GetModuleInfo;
  commandFrame[3] = 0x00; // ParamLen MSB
  commandFrame[4] = 0x01; // ParamLen LSB
  commandFrame[5] = 0x00; // Param
  commandFrame[6] = 0x04; // Checksum (laut Datenblatt)
  commandFrame[7] = R200_FrameEnd;
  _serial->write(commandFrame, 8);
}

/**
 * Single Poll Command senden
 */
void R200_3::poll(){
  uint8_t commandFrame[7] = {0};
  commandFrame[0] = R200_FrameHeader;
  commandFrame[1] = FrameType_Command;
  commandFrame[2] = CMD_SinglePollInstruction;
  commandFrame[3] = 0x00; // ParamLen MSB
  commandFrame[4] = 0x00; // ParamLen LSB
  commandFrame[5] = 0x22; // Checksum
  commandFrame[6] = R200_FrameEnd;
  _serial->write(commandFrame, 7);
}

void R200_3::setTransmitPowerDbm(uint8_t dbm){
  // viele Protokolle: Pow = dBm * 100 (z.B. 20dBm -> 2000 -> 0x07D0)
  uint16_t pow = (uint16_t)dbm * 100;

  uint8_t commandFrame[9] = {0};
  commandFrame[0] = R200_FrameHeader;   // 0xAA
  commandFrame[1] = FrameType_Command;  // 0x00
  commandFrame[2] = CMD_SetTransmitPower; // 0xB6
  commandFrame[3] = 0x00;              // ParamLen MSB
  commandFrame[4] = 0x02;              // ParamLen LSB
  commandFrame[5] = (pow >> 8) & 0xFF;  // Pow MSB
  commandFrame[6] = pow & 0xFF;         // Pow LSB

  // Checksum dynamisch berechnen (statt hart codieren)
  commandFrame[7] = calculateCheckSum(commandFrame);

  commandFrame[8] = R200_FrameEnd;      // 0xDD
  _serial->write(commandFrame, 9);
}

void R200_3::setMultiplePollingMode(bool enable){
  if(enable){
    uint8_t commandFrame[10] = {0};
    commandFrame[0] = R200_FrameHeader;
    commandFrame[1] = FrameType_Command; //(0x00)
    commandFrame[2] = CMD_MultiplePollInstruction; //0x27
    commandFrame[3] = 0x00; // ParamLen MSB
    commandFrame[4] = 0x03; // ParamLen LSB
    commandFrame[5] = 0x22; // Param
    commandFrame[6] = 0xFF; // Count of polls MSB
    commandFrame[7] = 0xFF; // Count of polls LSB
    commandFrame[8] = 0x4A; // Checksum
    commandFrame[9] = R200_FrameEnd;
    _serial->write(commandFrame, 10);
  }
  else {
    uint8_t commandFrame[7] = {0};
    commandFrame[0] = R200_FrameHeader;
    commandFrame[1] = FrameType_Command;
    commandFrame[2] = CMD_StopMultiplePoll; //0x28
    commandFrame[3] = 0x00; // ParamLen MSB
    commandFrame[4] = 0x00; // ParamLen LSB
    commandFrame[5] = 0x28; // Checksum
    commandFrame[6] = R200_FrameEnd;
    _serial->write(commandFrame, 7);
  }
}

uint8_t R200_3::calculateCheckSum(uint8_t *buffer){
  // Parameterlänge auslesen
  uint16_t paramLength = buffer[3];
  paramLength <<= 8;
  paramLength += buffer[4];

  // Checksum = Summe aller Bytes von Type bis letztem Parameter
  uint16_t check = 0;
  for(uint16_t i = 1; i < paramLength + 4 + 1; i++) {
    check += buffer[i];
  }
  return (check & 0xFF);
}

uint16_t R200_3::arrayToUint16(uint8_t *array){
  uint16_t value = *array;
  value <<= 8;
  value += *(array+1);
  return value;
}
