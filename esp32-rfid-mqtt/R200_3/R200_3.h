#ifndef R200_3_h
#define R200_3_h

// #define DEBUG   // enable for more debug prints

#include <Arduino.h>
#include <stdint.h>

// Receive buffer length
#define RX_BUFFER_LENGTH 128

class R200_3 {
  private:
    HardwareSerial *_serial;
    uint8_t _buffer[RX_BUFFER_LENGTH] = {0};

    uint8_t  calculateCheckSum(uint8_t *buffer);
    uint16_t arrayToUint16(uint8_t *array);
    bool     parseReceivedData();
    bool     dataIsValid();
    bool     receiveData(unsigned long timeOut = 500);
    void     dumpReceiveBufferToSerial();
    uint8_t  flush();
    void     beep();

    const uint8_t blankUid[12] = {0};

  public:
    R200_3();

    uint8_t uid[12] = {0};

    bool begin(HardwareSerial *serial = &Serial2,
               int baud = 115200,
               uint8_t RxPin = 16,
               uint8_t TxPin = 17);

    void loop();
    void poll();
    void setTransmitPowerDbm(uint8_t dbm);
    void setMultiplePollingMode(bool enable = true);
    void dumpModuleInfo();
    bool dataAvailable();
    void dumpUIDToSerial();

    enum R200_FrameStructure : byte {
      R200_HeaderPos         = 0x00,
      R200_TypePos           = 0x01,
      R200_CommandPos        = 0x02,
      R200_ParamLengthMSBPos = 0x03,
      R200_ParamLengthLSBPos = 0x04,
      R200_ParamPos          = 0x05,
    };

    enum R200_FrameControl : byte {
      R200_FrameHeader = 0xAA,
      R200_FrameEnd    = 0xDD,
    };

    enum R200_FrameType : byte {
      FrameType_Command      = 0x00,
      FrameType_Response     = 0x01,
      FrameType_Notification = 0x02,
    };

    enum R200_Command : byte {
      CMD_GetModuleInfo           = 0x03,
      CMD_SinglePollInstruction   = 0x22,
      CMD_MultiplePollInstruction = 0x27,
      CMD_StopMultiplePoll        = 0x28,
      CMD_SetTransmitPower        = 0xB6,
      CMD_ExecutionFailure        = 0xFF,
    };

    enum R200_ErrorCode : byte {
      ERR_CommandError  = 0x17,
      ERR_InventoryFail = 0x15,
      ERR_AccessFail    = 0x16,
      ERR_ReadFail      = 0x09,
      ERR_WriteFail     = 0x10,
    };
};

#endif
