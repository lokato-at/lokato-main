#include <Arduino.h>
#include "R200_3.h"

R200_3::R200_3() {}

bool R200_3::begin(HardwareSerial* serial, int baud, uint8_t RxPin, uint8_t TxPin) {
  _serial = serial;
  _serial->begin(baud, SERIAL_8N1, RxPin, TxPin);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  resetFrame();
  memset(uid, 0, sizeof(uid));

  Serial.println("R200_3::begin() ready");
  return true;
}

bool R200_3::dataAvailable() {
  return _serial != nullptr && _serial->available() > 0;
}

void R200_3::startBeep(uint16_t durationMs) {
  digitalWrite(BUZZER_PIN, HIGH);
  _beepActive = true;
  _beepUntilAt = millis() + durationMs;
}

void R200_3::updateBuzzer() {
  if (_beepActive && (long)(millis() - _beepUntilAt) >= 0) {
    digitalWrite(BUZZER_PIN, LOW);
    _beepActive = false;
  }
}

void R200_3::resetFrame() {
  _rxIndex = 0;
  _frameInProgress = false;
  _lastByteAt = 0;
  memset(_buffer, 0, sizeof(_buffer));
}

void R200_3::flushInput() {
  if (_serial == nullptr) {
    return;
  }
  while (_serial->available()) {
    _serial->read();
  }
}

void R200_3::loop() {
  updateBuzzer();

  if (_serial == nullptr) {
    return;
  }

  while (_serial->available()) {
    consumeByte((uint8_t)_serial->read());
  }

  if (_frameInProgress && (millis() - _lastByteAt > FRAME_TIMEOUT_MS)) {
#ifdef DEBUG
    Serial.println("RFID frame timeout -> reset parser");
#endif
    resetFrame();
    flushInput();
  }
}

void R200_3::consumeByte(uint8_t b) {
  if (!_frameInProgress) {
    if (b != R200_FrameHeader) {
      return;
    }

    resetFrame();
    _frameInProgress = true;
    _buffer[0] = b;
    _rxIndex = 1;
    _lastByteAt = millis();
    return;
  }

  _lastByteAt = millis();

  if (_rxIndex >= RX_BUFFER_LENGTH) {
#ifdef DEBUG
    Serial.println("RFID RX overflow -> reset parser");
#endif
    resetFrame();
    return;
  }

  _buffer[_rxIndex++] = b;

  if (b == R200_FrameEnd) {
    processFrame(_rxIndex);
    resetFrame();
  }
}

bool R200_3::dataIsValid(uint8_t frameLength) {
  if (frameLength < 7) {
    return false;
  }

  if (_buffer[0] != R200_FrameHeader || _buffer[frameLength - 1] != R200_FrameEnd) {
    return false;
  }

  uint16_t paramLength = arrayToUint16(&_buffer[R200_ParamLengthMSBPos]);
  const uint16_t crcPos = 5 + paramLength;
  const uint16_t expectedLength = crcPos + 2; // CRC + FrameEnd

  if (expectedLength != frameLength) {
#ifdef DEBUG
    Serial.print("RFID frame length mismatch. expected=");
    Serial.print(expectedLength);
    Serial.print(" got=");
    Serial.println(frameLength);
#endif
    return false;
  }

  return calculateCheckSum(_buffer) == _buffer[crcPos];
}

void R200_3::processFrame(uint8_t frameLength) {
  if (!dataIsValid(frameLength)) {
#ifdef DEBUG
    Serial.println("Invalid RFID frame / CRC");
    dumpReceiveBufferToSerial(frameLength);
#endif
    return;
  }

  switch (_buffer[R200_CommandPos]) {
    case CMD_GetModuleInfo: {
      Serial.print("Reader info: ");
      const uint16_t paramLength = arrayToUint16(&_buffer[R200_ParamLengthMSBPos]);
      for (uint16_t i = 1; i < paramLength; ++i) {
        const char c = (char)_buffer[R200_ParamPos + i];
        if (c == '\0' || c == (char)R200_FrameEnd) {
          break;
        }
        Serial.print(c);
      }
      Serial.println();
      break;
    }

    case CMD_SinglePollInstruction:
    case CMD_MultiplePollInstruction: {
      const uint16_t paramLength = arrayToUint16(&_buffer[R200_ParamLengthMSBPos]);
      if (paramLength < 0x11 || frameLength < 23) {
#ifdef DEBUG
        Serial.println("RFID poll response too short");
#endif
        return;
      }

      if (memcmp(uid, &_buffer[9], 12) != 0) {
        memcpy(uid, &_buffer[9], 12);
        startBeep(60);
      }
      break;
    }

    case CMD_ExecutionFailure:
      switch (_buffer[R200_ParamPos]) {
        case ERR_InventoryFail:
          if (memcmp(uid, blankUid, sizeof(uid)) != 0) {
            memset(uid, 0, sizeof(uid));
          }
          break;
        default:
#ifdef DEBUG
          Serial.print("RFID execution failure code: 0x");
          Serial.println(_buffer[R200_ParamPos], HEX);
#endif
          break;
      }
      break;

    default:
#ifdef DEBUG
      Serial.print("Unknown RFID command: 0x");
      Serial.println(_buffer[R200_CommandPos], HEX);
#endif
      break;
  }
}

void R200_3::dumpUIDToSerial() {
  Serial.print("0x");
  for (uint8_t i = 0; i < 12; ++i) {
    if (uid[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(uid[i], HEX);
  }
}

void R200_3::dumpReceiveBufferToSerial(uint8_t len) {
  Serial.print("RX-Buffer: 0x");
  for (uint8_t i = 0; i < len; ++i) {
    if (_buffer[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(_buffer[i], HEX);
  }
  Serial.println();
}

void R200_3::dumpModuleInfo() {
  uint8_t commandFrame[8] = {0};
  commandFrame[0] = R200_FrameHeader;
  commandFrame[1] = FrameType_Command;
  commandFrame[2] = CMD_GetModuleInfo;
  commandFrame[3] = 0x00;
  commandFrame[4] = 0x01;
  commandFrame[5] = 0x00;
  commandFrame[6] = calculateCheckSum(commandFrame);
  commandFrame[7] = R200_FrameEnd;
  _serial->write(commandFrame, sizeof(commandFrame));
}

void R200_3::poll() {
  if (_serial == nullptr) {
    return;
  }

  uint8_t commandFrame[7] = {0};
  commandFrame[0] = R200_FrameHeader;
  commandFrame[1] = FrameType_Command;
  commandFrame[2] = CMD_SinglePollInstruction;
  commandFrame[3] = 0x00;
  commandFrame[4] = 0x00;
  commandFrame[5] = calculateCheckSum(commandFrame);
  commandFrame[6] = R200_FrameEnd;
  _serial->write(commandFrame, sizeof(commandFrame));
}

void R200_3::setTransmitPowerDbm(uint8_t dbm) {
  if (_serial == nullptr) {
    return;
  }

  const uint16_t power = (uint16_t)dbm * 100;

  uint8_t commandFrame[9] = {0};
  commandFrame[0] = R200_FrameHeader;
  commandFrame[1] = FrameType_Command;
  commandFrame[2] = CMD_SetTransmitPower;
  commandFrame[3] = 0x00;
  commandFrame[4] = 0x02;
  commandFrame[5] = (power >> 8) & 0xFF;
  commandFrame[6] = power & 0xFF;
  commandFrame[7] = calculateCheckSum(commandFrame);
  commandFrame[8] = R200_FrameEnd;
  _serial->write(commandFrame, sizeof(commandFrame));
}

void R200_3::setMultiplePollingMode(bool enable) {
  if (_serial == nullptr) {
    return;
  }

  if (enable) {
    uint8_t commandFrame[10] = {0};
    commandFrame[0] = R200_FrameHeader;
    commandFrame[1] = FrameType_Command;
    commandFrame[2] = CMD_MultiplePollInstruction;
    commandFrame[3] = 0x00;
    commandFrame[4] = 0x03;
    commandFrame[5] = 0x22;
    commandFrame[6] = 0xFF;
    commandFrame[7] = 0xFF;
    commandFrame[8] = calculateCheckSum(commandFrame);
    commandFrame[9] = R200_FrameEnd;
    _serial->write(commandFrame, sizeof(commandFrame));
  } else {
    uint8_t commandFrame[7] = {0};
    commandFrame[0] = R200_FrameHeader;
    commandFrame[1] = FrameType_Command;
    commandFrame[2] = CMD_StopMultiplePoll;
    commandFrame[3] = 0x00;
    commandFrame[4] = 0x00;
    commandFrame[5] = calculateCheckSum(commandFrame);
    commandFrame[6] = R200_FrameEnd;
    _serial->write(commandFrame, sizeof(commandFrame));
  }
}

uint8_t R200_3::calculateCheckSum(uint8_t* buffer) {
  uint16_t paramLength = arrayToUint16(&buffer[3]);
  uint16_t check = 0;

  for (uint16_t i = 1; i < paramLength + 5; ++i) {
    check += buffer[i];
  }
  return (uint8_t)(check & 0xFF);
}

uint16_t R200_3::arrayToUint16(uint8_t* array) {
  uint16_t value = *array;
  value <<= 8;
  value += *(array + 1);
  return value;
}
