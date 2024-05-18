#include <Arduino.h>
#include "FujiAC.h"

// -- TODO --
//  Estado base apagado.
//  Interfaz web para modificar estado
//  Replicar estado a remote y leer modificaciones
//  Rele para que en estado apagado haya bypass.
//  Rele controlado por esp de forma que pueda activar el bypass.
// if cache changes and new value is not stored in cache then store it and also process, but if it is already in cache... Maybe is not processing it again?
// enable / disable LCD controller remote temperature sensor. If enabled, then send temperature from LCD remote sensor as sensor temperature, 
// if not, temperature needs to be updated by API. 

// Start with master halted, restore status from slave read, then send it to the master and unhalt. Unhalt also after a timeout.
// separate master and slave into RTOS tasks
// Hardware bus collision detection
// Not pulldown data lines if reset...

// ESP32 thermostat with ESPhome and hassio: https://esphome.io/components/climate/thermostat.html
// Home assistant integration, outside and inside temperature sensors
// Integrate heating system
// create altium designer project

unsigned long lastUpdate = millis();
unsigned long lastUpdate2 = millis();

int temp = 16;
int attempts = 0;
unsigned long lastUpdate3 = millis();
unsigned long timetemp = millis();
int temp2 = 23;
bool loginbit = false;
bool unknownbit = false;
bool blink = false;
byte error = 0;
unsigned long blinktime = millis();

bool weWantToUpdate = false;

//#define DIFFERENT_ENTITIES
//#define DIFFERENT_ENTITIES_DISABLE_COMPARE

FujiAC hp;
#ifdef DIFFERENT_ENTITIES
FujiAC hp2;
unsigned long readFrameTimer = millis();
#define FUJI_HEATPUMP_COMPARE_INTERVAL 4000
#endif

unsigned long startTime = 0;
unsigned long endTime = 0;
unsigned long actime = 33;
unsigned long actimeexecution = 0;
void ACTask(void *pvParameters)
{
  for (;;)
  {
    startTime = millis();
    // set error to true during 5s if blink is true
    if (blink && millis() - blinktime > 5000)
    {
      error = 0;
      blink = false;
    }

    if (millis() - timetemp > 5000)
    {
      // temp2++;
      // if (temp2 > 30)
      //   temp2 = 16;
      timetemp = millis();
      loginbit = !loginbit;
      unknownbit = !unknownbit;

      // hp.commandsTerminal();
    }

    hp.loopFujiAC();
#ifdef DIFFERENT_ENTITIES
    hp2.loopFujiAC();

#ifndef DIFFERENT_ENTITIES_DISABLE_COMPARE
    // compare parameters of LCD controller (master) and this controller (slave)
    FujiAC *compareEntity_1_Controller = &hp;
    FujiAC *compareEntity_2_Controller = &hp2;

    if (millis() - readFrameTimer > FUJI_HEATPUMP_COMPARE_INTERVAL)
    {
      Serial.println("\n&&&&&&&&&&&&&&&&&& loopFujiAC: Comparing controller " + compareEntity_1_Controller->GetName() + " with " + compareEntity_2_Controller->GetName() + " #######################");

      int compareResult = hp.compareParameters(compareEntity_1_Controller->getRXState(), compareEntity_2_Controller->getRXState());

      // pendingModificationsState vs rx_state
      if (compareResult == 1)
      {
        Serial.println("LCD::| ## Different, waiting for update. " + compareEntity_2_Controller->_controllerName + " to " + compareEntity_1_Controller->_controllerName + " values. id: " + String(compareEntity_1_Controller->getRXState().msg_id) + " (" + String(compareResult) + ")... ##");
        Serial.println("S01::| ## Different, sending update. " + compareEntity_2_Controller->_controllerName + " to " + compareEntity_1_Controller->_controllerName + " values. id: " + String(compareEntity_1_Controller->getRXState().msg_id) + " (" + String(compareResult) + ")... ##");
        compareEntity_2_Controller->setOnOff(compareEntity_1_Controller->getRXState().onOff);
        compareEntity_2_Controller->setTemp(compareEntity_1_Controller->getRXState().temperature);
        compareEntity_2_Controller->setMode(compareEntity_1_Controller->getRXState().acMode);
        compareEntity_2_Controller->setFanMode(compareEntity_1_Controller->getRXState().fanMode);
        compareEntity_2_Controller->setEconomyMode(compareEntity_1_Controller->getRXState().economyMode);
        compareEntity_2_Controller->setSendAddressParams();
        Serial.println("### pf1");
        hp.printFrame(compareEntity_1_Controller->getRXState(), true, false);
        hp.printFrame(compareEntity_2_Controller->getRXState(), true, false);
        hp.printFrame(compareEntity_2_Controller->pendingModificationsState, true, false);
        Serial.println("#######################          ##          #######################\n");
      }
      if (compareResult == 2)
      {
        Serial.println("LCD::| ## Different, sending update. " + compareEntity_1_Controller->_controllerName + " to " + compareEntity_2_Controller->_controllerName + " values. id: " + String(compareEntity_2_Controller->getRXState().msg_id) + " (" + String(compareResult) + ")... ##");
        Serial.println("S01::| ## Different, waiting for update. " + compareEntity_1_Controller->_controllerName + " to " + compareEntity_2_Controller->_controllerName + " values. id: " + String(compareEntity_2_Controller->getRXState().msg_id) + " (" + String(compareResult) + ")... ##");
        compareEntity_1_Controller->setOnOff(compareEntity_2_Controller->getRXState().onOff);
        compareEntity_1_Controller->setTemp(compareEntity_2_Controller->getRXState().temperature);
        compareEntity_1_Controller->setMode(compareEntity_2_Controller->getRXState().acMode);
        compareEntity_1_Controller->setFanMode(compareEntity_2_Controller->getRXState().fanMode);
        compareEntity_1_Controller->setEconomyMode(compareEntity_2_Controller->getRXState().economyMode);
        compareEntity_1_Controller->setSendAddressParams();
        Serial.println("### pf2");
        hp.printFrame(compareEntity_1_Controller->getRXState(), true, false);
        hp.printFrame(compareEntity_2_Controller->getRXState(), true, false);
        hp.printFrame(compareEntity_1_Controller->pendingModificationsState, true, false);
        Serial.println("#######################          ##          #######################\n");
      }
      else
      {
        // Serial.print("Parameters are the same\n");
      }
      readFrameTimer = millis();
    }
#endif

// lateststate = rx_state;
#endif

    // compare the state of the master and slave every FUJI_HEATPUMP_COMPARE_INTERVAL ms
    // if (millis() - comparetime > FUJI_HEATPUMP_COMPARE_INTERVAL)
    //{
    //  comparetime = millis();
    //
    //  FujiFrame slaveFrame = hp.getCurrentState();
    //  FujiFrame masterFrame = hp2.getCurrentState();
    //  bool equal = true;
    //
    //}

    /*
       if (millis() - lastUpdate3 > 200 + 60)
      {
         Serial.print(" - -----------------------------------------------------------------------------------------------------------------\n");
         // hp2.waitForFrame();

         FujiFrame p3;
         p3.onOff = 1;
         p3.temperature = byte(temp2);
         p3.acMode = byte(FujiMode::AUTO);
         p3.fanMode = byte(FujiFanMode::FAN_LOW);
         // if(error==0)
         p3.acError = 0;
         // else
         //   p3.acError = 1;
         p3.economyMode = 0;
         p3.swingMode = 0;
         p3.swingStep = 0;
         p3.controllerPresent = 1;
         p3.updateMagic = 10; // no idea what this is
         p3.controllerTemp = 0;
         p3.writeBit = 0;
         p3.loginBit = 1;
         p3.unknownBit = 1;
         p3.messageType = byte(FujiMessageType::STATUS);
         p3.messageSource = byte(FujiAddress::START);
         p3.messageDest = byte(FujiAddress::PRIMARY);

        hp2.sendFujiFrame(p3);
        lastUpdate3 = millis();
      }

    */
    actime = millis() - startTime;
    actimeexecution++;
  }
}

void controlTask(void *pvParameters)
{
  unsigned long latestacexecution;
  for (;;)
  {
    if(latestacexecution==actimeexecution)
    {
      delay(1000);
      continue;
    }
    // Calcula el uso de CPU por ACTask
    uint32_t stackUsage = uxTaskGetStackHighWaterMark(NULL); // En bytes
    uint32_t cpuUsage = 100 - (stackUsage * 100) / configMINIMAL_STACK_SIZE;

    // Calcula el tiempo libre para otras tareas (en milisegundos)
    uint32_t totalExecutionTime = millis();
    uint32_t ACTaskExecutionTime = actime;
    uint32_t freeTimeForOtherTasks = totalExecutionTime - ACTaskExecutionTime;

    // Imprime los resultados
    Serial.print("\
    \n▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬ \
    \n▮ Current ms:\t"+                     String(millis())+"\
    \n▮ CPU Usage by ACTask (%): \t"+       String(cpuUsage)+"\
    \n▮ ACTask Execution Time (ms): \t"+    String(actime)+"\
    \n▮ Free time for other tasks (ms): \t"+String(freeTimeForOtherTasks)+"\
    \n▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬\n\n");

    // Espera un tiempo antes de volver a calcular
    delay(3000);
    latestacexecution=actimeexecution;
  }
}

void wiFiTask(void *pvParameters)
{
  for (;;)
  {
    // create thermostat web interface

    // increase temperature 1 degree every 5 seconds
    if (millis() - lastUpdate > 10000)
    {
      temp++;
      if (temp > 23)
        temp = 20;
      lastUpdate = millis();
      weWantToUpdate = true;

      Serial.println("UPDATING TEMP TO " + String(temp));
      // compare parameters of LCD controller (master) and this controller (slave)
      FujiAC *compareEntity_1_Controller = &hp;

      compareEntity_1_Controller->setOnOff(1);
      compareEntity_1_Controller->setTemp(temp);
      compareEntity_1_Controller->setMode(5);
      compareEntity_1_Controller->setFanMode(2);
      compareEntity_1_Controller->setEconomyMode(0);
      compareEntity_1_Controller->setSendAddressParams();
      Serial.println("### pf1");
      hp.printFrame(compareEntity_1_Controller->getRXState(), true, false);
      hp.printFrame(compareEntity_1_Controller->pendingModificationsState, true, false);
      Serial.println("#######################          ##          #######################\n");

      if (temp == 22)
      {
        // disable task
        vTaskDelete(NULL);
      }
    }

    delay(1000);
  }
}
void setup()
{
  Serial.begin(921600); //115200
  Serial.println("Starting up");
  // setting up pin 23 as LED output
  pinMode(23, OUTPUT);
  digitalWrite(23, HIGH);

  // set pins 27, 26 as input
  pinMode(27, INPUT);
  pinMode(26, OUTPUT);
  //digitalWrite(26, HIGH);

#define INHIBIT_RX_1 21
#define INHIBIT_RX_2 22
  // Serial1.begin(500, SERIAL_8E1, 27, 12);
  hp.connect(&Serial2, FujiControllerType::SLAVE_PRIMARY, 27, 26, INHIBIT_RX_1); // use Serial2, and bind as a secondary controller
  hp.SetName("S01");


  // set INHIBIT_RX_2 as output
  //pinMode(INHIBIT_RX_1, OUTPUT);
  //digitalWrite(INHIBIT_RX_1, HIGH);

#ifdef DIFFERENT_ENTITIES
  hp2.connect(&Serial1, FujiControllerType::MASTER, 25, 33, INHIBIT_RX_2);
  hp2.SetName("LCD");
#else
  hp.connectLCDController(&Serial1, 25, 33); // use Serial1, and bind as a secondary controller
#endif
  // hp.ReadOnly(true);
  //  If restore flag is set to 1: Restore AC to the last known state
  //  hp.restoreState();

  xTaskCreatePinnedToCore(
      ACTask,   /* Task function. */
      "ACTask", /* name of task. */
      15000,    /* Stack size of task */
      NULL,     /* parameter of the task */
      1,        /* priority of the task */
      NULL,     /* Task handle to keep track of created task */
      0);       /* pin task to core 0 */

  // xTaskCreatePinnedToCore(
  //     wiFiTask,   /* Task function. */
  //     "wiFiTask", /* name of task. */
  //     10000,      /* Stack size of task */
  //     NULL,       /* parameter of the task */
  //     1,          /* priority of the task */
  //     NULL,       /* Task handle to keep track of created task */
  //     1);         /* pin task to core 1 */

  // create control task
  //xTaskCreatePinnedToCore(
  //    controlTask,   /* Task function. */
  //    "controlTask", /* name of task. */
  //    10000,         /* Stack size of task */
  //    NULL,          /* parameter of the task */
  //    1,             /* priority of the task */
  //    NULL,          /* Task handle to keep track of created task */
  //    1);            /* pin task to core 1 */
}

// unsigned long comparetime = millis();
void loop()
{
}
