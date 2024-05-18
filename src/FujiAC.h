#ifndef FUJI_AC_H
#define FUJI_AC_H

#include <HardwareSerial.h>
#include <Arduino.h>

/*
Each 40 UNIT messages (40 RX + 40 TX) there are 
- 1 unknown message with content : FF 7F FF 7F FF FF FF FF
- 5 unknown messages with content: FF 7E FF ED ED 5F FE FF

Normal known messages example:
S01::| 32|:: <-- FF 5F FF ED ED 5F FE FF   mSrc:   0, mDst:  32, onOff: 0, mType: 0, temp: 18 | cTemp: 0, acmode: 1, fanmode: 1, write: 0, login: 1, unk: 1, cP:1, uMgc: 10,  acError:0, id:   3290
S01::| 32|:: --> DF 7E FF ED ED FF FE FF   mSrc:  32, mDst:   1, onOff: 0, mType: 0, temp: 18 | cTemp: 0, acmode: 1, fanmode: 1, write: 0, login: 0, unk: 1, cP:1, uMgc:  0,  acError:0, id:   3291
*/
#define MS_BETWEEN_UNIT_SENDS 400
#define MS_SENDING_UNIT 200
#define MAX_MS_BETWEEN_REPLY_UNIT MS_BETWEEN_UNIT_SENDS - MS_SENDING_UNIT
#define CONFIG_SERIAL_TIMEOUT 100
#define CONFIG_SERIAL_RX_TIMEOUT_SYMBOLS 3
#define CONFIG_SERIAL_RX_FIFOFULL_SYMBOLS 8
#define UART_BUFFER_RX 128
#define MAX_SEND_MS 250

#define RESTORE_STATE_ON_STARTUP 0
#define USE_CACHE
// #define MAX_MS_BETWEEN_RX_FRAMES 200

const byte kModeIndex = 3;
const byte kModeMask = 0b00001110;
const byte kModeOffset = 1;

const byte kFanIndex = 3;
const byte kFanMask = 0b01110000;
const byte kFanOffset = 4;

const byte kEnabledIndex = 3;
const byte kEnabledMask = 0b00000001;
const byte kEnabledOffset = 0;

const byte kErrorIndex = 3;
const byte kErrorMask = 0b10000000;
const byte kErrorOffset = 7;

// index 0: 0b11111111  source
// index 1: 0b11111111  unknownbit|destination ¿b2=login parte de dest?
// index 2: 0b00111000  ??|messagetype|writebit|???
// index 3: 0b11111111  error|fan|mode|enabled
// index 4: 0b11111111  economy|temperature
// index 5: 0b11110110  updatemagic|?|swing|swingstep|?
// index 6: 0b01111111  ?|controllerTemp|controllerPresent
// index 7: 0b00000000  ?|?|?|?|?|?|?|?

const byte kEconomyIndex = 4;
const byte kEconomyMask = 0b10000000;
const byte kEconomyOffset = 7;

const byte kTemperatureIndex = 4;
const byte kTemperatureMask = 0b01111111;
const byte kTemperatureOffset = 0;

const byte kUpdateMagicIndex = 5;
const byte kUpdateMagicMask = 0b11110000;
const byte kUpdateMagicOffset = 4;

const byte kSwingIndex = 5;
const byte kSwingMask = 0b00000100;
const byte kSwingOffset = 2;

const byte kSwingStepIndex = 5;
const byte kSwingStepMask = 0b00000010;
const byte kSwingStepOffset = 1;

const byte kControllerPresentIndex = 6;
const byte kControllerPresentMask = 0b00000001;
const byte kControllerPresentOffset = 0;

const byte kControllerTempIndex = 6;
const byte kControllerTempMask = 0b01111110;
const byte kControllerTempOffset = 1;

typedef struct FujiFrames
{
  byte onOff = 0;
  byte temperature = 16;
  byte acMode = 0;
  byte fanMode = 0;
  byte acError = 0;
  byte economyMode = 0;
  byte swingMode = 0;
  byte swingStep = 0;
  byte controllerPresent = 0;
  byte updateMagic = 0; // unsure what this value indicates
  byte controllerTemp = 16;

  bool writeBit = false;
  bool loginBit = false;
  bool unknownBit = false; // unsure what this bit indicates

  byte messageType = 0;
  byte messageSource = 0;
  byte messageDest = 0;
  unsigned long lastUpdate = 0;
  unsigned long msg_id = 0;
  byte unitAddress = 0;

} FujiFrame;
typedef struct
{
  int v1[5] = {0};
  int v2[5] = {0};
  String name[5] = {""};

} CompareBuffer;

#define CACHE_BUFFER_SIZE 10
typedef struct
{
  FujiFrame rx_frame[CACHE_BUFFER_SIZE];
  FujiFrame tx_frame[CACHE_BUFFER_SIZE];
  FujiFrame latest_rx_frame;
  FujiFrame latest_tx_frame;
  int latest_rx_frame_index = 0;
  int index = 0;
  bool fullcache = false;
} CacheBuffer;
typedef struct
{
  byte rx_frame[CACHE_BUFFER_SIZE][8];
  byte tx_frame[CACHE_BUFFER_SIZE][8];
  byte latest_rx_frame[8];
  byte latest_tx_frame[8];
  int latest_rx_frame_index = 0;
  int index = 0;
} CacheBuffer_raw;

enum class FujiControllerType : byte
{
  MASTER = 0,
  SLAVE_PRIMARY = 1,
  SLAVE_SECONDARY = 2,
  SNIFFER = 3
};

#define MAX_FIFO_SIZE 50

struct FIFO
{
  FujiFrames buffer[MAX_FIFO_SIZE];
  int front = -1;
  int rear = -1;
};

class FujiAC
{
private:
  CacheBuffer_raw cacheBuffer_raw;
  CacheBuffer cacheBuffer;
  FujiFrame lateststate;
  byte masterTemperature = 16;
  CompareBuffer compareBuffer;
  unsigned long latest_msg_id = 0;
  FujiFrame latest_rx_frame;
  bool firstmessage = true;
  bool latestcompareequal = false;
  FujiControllerType _fct;
  FujiFrames _parameters;
  // FujiFrames rxFrame;
  HardwareSerial *_serial;
  byte latestRXBuffer[8];
  byte writeBuf[8];

  byte controllerAddress;
  bool controllerIsPrimary = true;
  bool seenSecondaryController = false;
  bool controllerLoggedIn = false;
  unsigned long lastFrameReceived;
  unsigned long sendFrameTimer;
  unsigned long readFrameTimer;

  bool dequeue(FIFO *fifo, FujiFrames *frame);
  bool enqueue(FIFO *fifo, FujiFrames frame);
  bool isFIFOEmpty(FIFO *fifo);
  bool isFIFOFull(FIFO *fifo);
  void initFIFO(FIFO *fifo);

// SEND_FRAME_INTERVAL está limitado por el buffer de lectura de UART. A menos de 700ms se pierden paquetes.
// Implementar bufer FIFO para evitar pérdida de paquetes. O almacenar en memoria el último paquete, si el recibido es igual al anterior no hay que volver a generar el que hay que transmitir.
#define SEND_FRAME_INTERVAL 500
#define READ_FRAME_INTERVAL 50

  // FujiFrame updateState;
  // FujiFrame slaveState_rx;
  FujiFrame rx_state;
  FujiFrame rx_state_reply;
  FujiFrame masterState;

  FujiFrame wiFiState;
  FujiAC *_controllerLCD = NULL;

  FujiFrame pendingFrame;

  bool _sniffmode = false;
  bool _ismaster = false;
  // FujiAC *_slave = NULL;
  bool _controllerLCDavailable = false;
  bool _readonly = false;
  unsigned long sendtimer = 0;
  int _inh_rx = -1;

public:
  int corruptedFrames_counter = 0;
  unsigned long processtimer = millis();
  unsigned long timesincelastframeanalisys = 0;
  unsigned long latestuart = 0;
  unsigned long frameerrortimer = 0;
  byte txLastSend[8];
  bool read8BFramefromUART(FujiFrame *ff, String *hex = NULL);
  bool putCache(FujiFrame rx_frame, FujiFrame tx_frame);
  bool putCache(byte *rx_frame, byte *tx_frame);
  int isInCache(FujiFrame rx_frame, FujiFrame *tx_frame);
  int isInCache(byte *rx_frame, byte *tx_frame);
  bool UARTFlushTX(int timeout = 500);
  unsigned long getTimeMSSinceLastRX();
  unsigned long last_RX_time = 0;
  String _controllerName = "X";
  bool receivedack = false;
  unsigned long acktimer = 0;
  bool pendingModifications = false;
  FujiFrame pendingModificationsState;
  void connectLCDController(HardwareSerial *serial, int rxPin, int txPin);
  void ReadOnly(bool ro);
  FujiFrame getFrameFromReadArray(byte readBuf[8]);
  bool getFrameFromUARTBUFF(FujiFrame *ff);
  bool getFrameFromUARTBUFF_1(FujiFrame *ff);
  byte *FujiFrame2ByteArray(FujiFrame ff);
  void printFrame(FujiFrame ff, bool tx = false, bool printdirection = true);
  byte *encodeFrame(FujiFrame ff);
  void connect(HardwareSerial *serial, FujiControllerType fct = FujiControllerType::SLAVE_PRIMARY);
  void connect(HardwareSerial *serial, FujiControllerType fct = FujiControllerType::SLAVE_PRIMARY, int rxPin = -1, int txPin = -1, int inh_rx = -1);
  void SetName(String name);
  bool waitForFrame();
  void sendFujiFrame(FujiFrame ff, bool verbose = false);
  // bool isBound();
  // bool updatePending();
  int compareParameters(FujiFrame p, FujiFrame p2, bool compareDest = false, bool verbose = false);
  FujiFrames createParameters(byte onOff, byte temperature, byte acMode, byte fanMode, byte acError, byte economyMode, byte swingMode, byte swingStep, byte controllerPresent, byte updateMagic, byte controllerTemp);
  // FujiFrames getRXFrame();
  String GetName();
  void setSendAddressParams();
  FujiFrame getRXState();
  // void linkSlave(FujiAC *slave);
  byte getpendingModifications();
  void saveFrame2EEPROM(FujiFrame ff);
  FujiFrame getFrameFromEEPROM();
  FujiFrame get_offFrame();
  FujiFrame get_onFrame();
  void loopFujiAC();
  void setOnOff(bool o);
  void setTemp(byte t);
  void setMode(byte m);
  void setFanMode(byte fm);
  void setEconomyMode(byte em);
  void setSwingMode(byte sm);
  void setSwingStep(byte ss);
  bool getOnOff();
  byte getTemp();
  byte getMode();
  byte getFanMode();
  byte getEconomyMode();
  byte getSwingMode();
  byte getSwingStep();
  byte getControllerTemp();
  bool debugPrint = true;
  void commandsTerminal();
  void setSecondaryController(bool enable);
  bool _secondaryController = false;
};

enum class FujiMode : byte
{
  UNKNOWN = 0,
  FAN = 1,
  DRY = 2,
  COOL = 3,
  HEAT = 4,
  AUTO = 5
};

// p3.messageType = byte(FujiMessageType::ERROR); ERROR TYPE show "ERROR" on screen with hex changing number as error code equal to the value sent as temperature.
// once error is on the screen it will stay there until ???? message is sent. (sniff main unit behaviour)
// LOGIN: dont do nothing
// UNKNOWN: dont do nothing
// STATUS: show the temperature, mode, etc on the screen
enum class FujiMessageType : byte
{
  STATUS = 0,
  ERROR = 1,
  LOGIN = 2,
  UNKNOWN = 3,
};

enum class FujiAddress : byte
{
  START = 0,
  UNIT = 1,
  PRIMARY = 32,
  SECONDARY = 33,
};

enum class FujiFanMode : byte
{
  FAN_AUTO = 0,
  FAN_QUIET = 1,
  FAN_LOW = 2,
  FAN_MEDIUM = 3,
  FAN_HIGH = 4
};

const byte kOnOffUpdateMask = 0b10000000;
const byte kTempUpdateMask = 0b01000000;
const byte kModeUpdateMask = 0b00100000;
const byte kFanModeUpdateMask = 0b00010000;
const byte kEconomyModeUpdateMask = 0b00001000;
const byte kSwingModeUpdateMask = 0b00000100;
const byte kSwingStepUpdateMask = 0b00000010;

#endif