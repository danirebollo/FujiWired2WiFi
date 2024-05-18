#include "FujiAC.h"
#include <Preferences.h>
Preferences preferences;

// #define TIMER_MEASUREMENTS_ON

// TODO:
// if cache changes and new value is not stored in cache then store it and also process, but if it is already in cache... Maybe is not processing it again?
// todo: enable / disable LCD controller remote temperature sensor. If enabled, then send temperature from LCD remote sensor as sensor temperature, 
// if not, temperature needs to be updated by API. 

// Start with master halted, restore status from slave read, then send it to the master and unhalt. Unhalt also after a timeout.
// separate master and slave into RTOS tasks
// Hardware bus collision detection
// Not pulldown data lines if reset...

// ESP32 thermostat with ESPhome and hassio: https://esphome.io/components/climate/thermostat.html
// Home assistant integration, outside and inside temperature sensors
// Integrate heating system
// create altium designer project
// Create github public page with hardware and software

// index 0: 0b11111111  source
// index 1: 0b11111111  unknownbit|destination ¿b2=login parte de dest?
// index 2: 0b00111000  ??|messagetype|writebit|???
// index 3: 0b11111111  error|fan|mode|enabled
// index 4: 0b11111111  economy|temperature
// index 5: 0b11110110  updatemagic|?|swing|swingstep|?
// index 6: 0b01111111  ?|controllerTemp|controllerPresent
// index 7: 0b00000000  ?|?|?|?|?|?|?|? Show maintenance mode icon when value is between about 60 and about 131 and about 196 to about 1

// TODO: mensajes solapados enviando a unit

// ############################################################################################################################################################################

/* **************************************************************************************
*********************************** MAIN functions **************************************
*************************************************************************************** */

void FujiAC::loopFujiAC()
{

    // if is master then send frame to LCD controller at required time interval (SEND_FRAME_INTERVAL)
    if (_ismaster) // drive LCD slave controller
    {
        if (!_readonly)
        {
            if (millis() - sendFrameTimer > SEND_FRAME_INTERVAL) //(millis() - acktimer > 400 || (receivedack && millis() - acktimer > 100)) // receivedack || (millis() - sendFrameTimer > SEND_FRAME_INTERVAL)
            {
                // if (millis() - acktimer > 400)
                //{
                //     Serial.println(":: " + _controllerName + " :: Caution, ACK timeout");
                //     acktimeout = true;
                // }

                // Serial.println(_controllerName + " :: Sending MASTER frame to LCD controller");

                masterState.writeBit = 0;
                masterState.loginBit = 1;
                masterState.unknownBit = 1;
                masterState.updateMagic = 0;
                masterState.messageType = byte(FujiMessageType::STATUS);
                masterState.messageSource = byte(FujiAddress::START);
                masterState.messageDest = byte(FujiAddress::PRIMARY);

                // if there are modifications (in the pendingModificationsState) and master and slave aren't off, then send them to the masterState
                if (pendingModifications == true)
                {
                    if (((!(masterState.onOff == pendingModificationsState.onOff && pendingModificationsState.onOff == 0))))
                    {
                        masterState.onOff = pendingModificationsState.onOff;
                        masterState.temperature = pendingModificationsState.temperature;
                        masterState.acMode = pendingModificationsState.acMode;
                        masterState.fanMode = pendingModificationsState.fanMode;
                        masterState.swingMode = pendingModificationsState.swingMode;
                        masterState.swingStep = pendingModificationsState.swingStep;
                        masterState.economyMode = pendingModificationsState.economyMode;
                        masterState.controllerPresent = 1;

                        // don't set pendingModifications to false yet, because we want to wait until receive the modifications from the LCD controller
                        // pendingModifications = false;

                        Serial.println("Sending modifications from " + _controllerName + ", overwride received frame values to the pendingModificationsState.\n: onOff: " + String(masterState.onOff) + ", temperature: " + String(masterState.temperature) + ", acMode: " + String(masterState.acMode) + ", fanMode: " + String(masterState.fanMode) + ", swingMode: " + String(masterState.swingMode) + ", swingStep: " + String(masterState.swingStep) + ", economyMode: " + String(masterState.economyMode));
                        // printFrame(masterState, true);
                    }
                    else
                    {
                        Serial.println(":: " + _controllerName + " :: Unit is off. NOT SENDING UPDATE. B masterState.onOff: " + String(masterState.onOff) + ", pendingModificationsState.onOff: " + String(pendingModificationsState.onOff));
                    }
                }
                // send frame to LCD controller. Bypass reply if no modifications are needed and modificated frame if modifications are needed
                //Serial.println("## :: sendFujiFrame. Sending frame from " + _controllerName + " :: ##");
                sendFujiFrame(masterState, true); // master send

                receivedack = false;
                sendFrameTimer = millis();
                // Serial.println(":: " + _controllerName + " :: loopFujiAC() - end SEND_FRAME_INTERVAL\n\n");
            }
        }
    }
    // if is slave don't do nothing unless we have a controller attached (to talk with LCD)
    else
    {
        // if we are slave (talk with UNIT) but we have controller attached (to talk with LCD) then execute loopFujiAC on it to send frames to LCD controller
        if (!_readonly && _controllerLCDavailable && _controllerLCD != NULL)
            _controllerLCD->loopFujiAC();
    }

    if (millis() - readFrameTimer > READ_FRAME_INTERVAL)
    {
        // read incoming frames and reply to it. If we are master then reply to the LCD controller and if we are slave then reply to the UNIT
        if (waitForFrame())
        {
            // if we are slave with attached master (_controllerLCD), compare latest LCD frame with latest UNIT frame
            if (!_readonly && _controllerLCD != NULL && _controllerLCDavailable)
            {
                // compare parameters of LCD controller (master) and this controller (slave)
                FujiAC *compareEntity_1_Controller = this;
                FujiAC *compareEntity_2_Controller = _controllerLCD;

                Serial.println("\n####################### loopFujiAC: Comparing controller " + compareEntity_1_Controller->GetName() + " with " + compareEntity_2_Controller->GetName() + " #######################");

                int compareResult = compareParameters(compareEntity_1_Controller->getRXState(), compareEntity_2_Controller->getRXState(), false, true);
                Serial.println(":: " + _controllerName + " :: compareResult: " + String(compareResult) + " :: B");

                // pendingModificationsState vs rx_state
                if (compareResult == 1)
                {
                    Serial.println("## " + _controllerName + " loopFujiAC: Master and Slave are different, updating " + compareEntity_2_Controller->_controllerName + " to " + compareEntity_1_Controller->_controllerName + " values. id: " + String(compareEntity_1_Controller->getRXState().msg_id) + " (" + String(compareResult) + ")... ##");
                    compareEntity_2_Controller->setOnOff(compareEntity_1_Controller->getRXState().onOff);
                    compareEntity_2_Controller->setTemp(compareEntity_1_Controller->getRXState().temperature);
                    compareEntity_2_Controller->setMode(compareEntity_1_Controller->getRXState().acMode);
                    compareEntity_2_Controller->setFanMode(compareEntity_1_Controller->getRXState().fanMode);
                    compareEntity_2_Controller->setEconomyMode(compareEntity_1_Controller->getRXState().economyMode);
                    compareEntity_2_Controller->setSendAddressParams();
                    // Serial.println("### pf1");
                    // printFrame(compareEntity_1_Controller->getRXState(), true, false);
                    // printFrame(compareEntity_2_Controller->getRXState(), true, false);
                    // printFrame(compareEntity_2_Controller->pendingModificationsState, true, false);
                    Serial.println("#######################          ##          #######################\n");
                }
                else if (compareResult == 2)
                {
                    Serial.println("## " + _controllerName + " loopFujiAC: Master and Slave are different, updating " + compareEntity_1_Controller->_controllerName + " to " + compareEntity_2_Controller->_controllerName + " values. id: " + String(compareEntity_2_Controller->getRXState().msg_id) + " (" + String(compareResult) + ")... ##");
                    Serial.println("onOff: " + String(compareEntity_1_Controller->getRXState().onOff) + " != " + String(compareEntity_2_Controller->getRXState().onOff));
                    compareEntity_1_Controller->setOnOff(compareEntity_2_Controller->getRXState().onOff);
                    compareEntity_1_Controller->setTemp(compareEntity_2_Controller->getRXState().temperature);
                    compareEntity_1_Controller->setMode(compareEntity_2_Controller->getRXState().acMode);
                    compareEntity_1_Controller->setFanMode(compareEntity_2_Controller->getRXState().fanMode);
                    compareEntity_1_Controller->setEconomyMode(compareEntity_2_Controller->getRXState().economyMode);
                    compareEntity_1_Controller->setSendAddressParams();
                    Serial.println("### pf2");
                    printFrame(compareEntity_1_Controller->getRXState(), true, false);
                    printFrame(compareEntity_2_Controller->getRXState(), true, false);
                    printFrame(compareEntity_1_Controller->pendingModificationsState, true, false);
                    Serial.println("#######################          ##          #######################\n");
                }
                else
                {
                    Serial.print("Parameters are the same\n");
                }
            }

            readFrameTimer = millis();
            lateststate = rx_state;
            // Serial.println(":: " + _controllerName + " :: loopFujiAC() - end READ_FRAME_INTERVAL && waitForFrame()\n\n");
        }
    }
}

// TODO: if need to discard frame, (corrupt frame, incomplete...) not send anything and wait until new sincronization (100ms high) on UART line

bool FujiAC::waitForFrame()
{
    // print corruptedFrames_counter each 10s to Serial
    if (millis() - frameerrortimer > 10000)
    {
        frameerrortimer = millis();
        Serial.println("Corrupted frames received: " + String(corruptedFrames_counter));
    }

    // If data is available to read from the serial port
    if (_serial->available())
    {
        /*
                Serial.print("avail: " + String(_serial->available()) + " bytes: ");
                Serial.print(_serial->read(), HEX);
                Serial.println(" Current millis: " + String(millis()) + " dif: " + String(millis() - latestuart));

                if (millis() - latestuart > 100)
                {
                    pinMode(_inh_rx, OUTPUT);
                    digitalWrite(_inh_rx, HIGH);
                    delay(10);
                    _serial->write(0xAA);
                    delay(30);

                    pinMode(_inh_rx, INPUT);
                }

                latestuart = millis();
                return false;
        */
        processtimer = millis();
#ifdef TIMER_MEASUREMENTS_ON

        Serial.println("\n\n---------------------------------------------------------------------------------------\n\
## " + _controllerName +
                       " :: time 0: " + String(millis() - processtimer) + ", START");
#endif
        FujiFrame ff, ff_bk;
        if (!getFrameFromUARTBUFF(&ff))
            return false;

#ifdef TIMER_MEASUREMENTS_ON
        Serial.println("## " + _controllerName + " :: time A: " + String(millis() - processtimer) + ", READED FRAME");
#endif

#ifdef USE_CACHE
        if (!_ismaster)
        {
            if (!pendingModifications) // communication with UNIT  && _ismaster
            {
#ifdef TIMER_MEASUREMENTS_ON
                Serial.println("## " + _controllerName + " :: time CACHE A: " + String(millis() - processtimer) + ", READED CACHE");
#endif
                int cacheResult = isInCache(ff, &ff_bk);
#ifdef TIMER_MEASUREMENTS_ON
                Serial.println("## " + _controllerName + " :: time CACHE B: " + String(millis() - processtimer) + ", DONE");
#endif

                if (cacheResult > -1)
                {
                    if (1 || debugPrint)
                    {

                        // Serial.println("## " + _controllerName + " :: time CACHE C: " + String(millis() - processtimer) + ", PRINTFRAME");
                        // Serial.println(_controllerName + " ### pf_bk. Sending cached frame: " + String(cacheResult) + " ### pendingModifications: " + String(pendingModifications) + " ###");
                        // printFrame(ff_bk, true, false);
                    }
// delay(60);
//  if(!_ismaster)
#ifdef TIMER_MEASUREMENTS_ON
                    Serial.println("## " + _controllerName + " :: time CACHE D: " + String(millis() - processtimer) + ", SEND FRAME");
#endif

                    sendFujiFrame(ff_bk, true); // cache slave send

#ifdef TIMER_MEASUREMENTS_ON
                    Serial.println("## " + _controllerName + " :: time CACHE E: " + String(millis() - processtimer) + ", ");
                    // int msg_time = 20;
                    // delay(msg_time);

                    Serial.println("## " + _controllerName + " :: time CACHE F: " + String(millis() - processtimer) + ", ");
#endif
                    return true;
                }
                else
                {
                    if (debugPrint)
                    {
                        Serial.println(_controllerName + " ### pf. Not using cache. cacheResult: " + String(cacheResult) + " ###");
                    }
                }
            }
            else
            {
                if (debugPrint)
                {
                    Serial.println(_controllerName + " ### pf. Not using cache. pendingModifications: " + String(pendingModifications) + ". ismaster: " + String(_ismaster) + " ###");
                }
            }
        }
#endif
        //Serial.println("## " + _controllerName + " :: Processed frame:: ##");
        printFrame(ff, false, true);
        Serial.println("");

        ff_bk = ff;
        // we need to reply if we receive a message to our address
        ff.lastUpdate = millis() + 20000000;
        ff.unitAddress = controllerAddress;
#ifdef TIMER_MEASUREMENTS_ON
        Serial.println("## " + _controllerName + " :: time B: " + String(millis() - processtimer));
#endif
        if (debugPrint)
        {
            // print incoming frame
            // Serial.println("### pf6");
            // printFrame(ff, false);
        }

        if (_readonly)
            return true;

        // if message is for us, we need to reply
        if (ff.messageDest == controllerAddress)
        {
            // first time we don't have a message to compare, so we just save the first message
            if (firstmessage)
            {
                latest_rx_frame = ff;
                firstmessage = false;
                // return true;
            }

            // Serial.println("\n####################### " + _controllerName + " :: Comparing with latest frame [dest: " + String(ff.messageDest) + "/cAddr:" + String(controllerAddress) + "]:: #######################");
#ifdef TIMER_MEASUREMENTS_ON
            Serial.println("## " + _controllerName + " :: time C: " + String(millis() - processtimer) + ", COMPARE PARAMETERS");
#endif

            // save the frame with the newest timestamp
            int compareResult = compareParameters(latest_rx_frame, ff, false, true);
// Serial.println(":: " + _controllerName + " :: compareResult: " + String(compareResult) + " :: A");
#ifdef TIMER_MEASUREMENTS_ON
            Serial.println("## " + _controllerName + " :: time D: " + String(millis() - processtimer));
#endif

            if (compareResult == 0)
            {
                ff = latest_rx_frame;
            }
            else if (compareResult == 1)
            {
                ff = latest_rx_frame;
            }
            else if (compareResult == 2)
            {
                // ff = ff;
                // replacing the old frame since we will send the new one
                latest_rx_frame = ff;
            }

            if (_ismaster && compareResult != 0) // drive LCD slave controller
            {
                pendingModificationsState = ff;
                Serial.println("## :: " + _controllerName + " :: Received and updated parameters are different. Setting Pending modifications pendingModifications=1  A ##");
                pendingModifications = true;
            }
            // save newest frame to rx_state
            rx_state = ff;

#ifdef TIMER_MEASUREMENTS_ON
            Serial.println("## " + _controllerName + " :: time E: " + String(millis() - processtimer));
#endif

            // if we're waiting for modifications, check if the received frame is the same as the one we're waiting for
            if (pendingModifications)
            {
                Serial.println(_controllerName + "//-> pendingModifications: " + String(pendingModifications));

                if (ff.onOff == pendingModificationsState.onOff && ff.temperature == pendingModificationsState.temperature && ff.acMode == pendingModificationsState.acMode && ff.fanMode == pendingModificationsState.fanMode &&
                    ff.swingMode == pendingModificationsState.swingMode && ff.swingStep == pendingModificationsState.swingStep && ff.economyMode == pendingModificationsState.economyMode)
                {
                    Serial.println(":: " + _controllerName + " :: Received and updated parameters are the same. Done updating.");
                    pendingModifications = false;
                }
                else
                {
                    // getRXState().onOff == 1 ||
                    if ((!(pendingModificationsState.onOff == ff.onOff && ff.onOff == 0))) // apply pending modifications only if unit is on
                    {
                        ff.onOff = pendingModificationsState.onOff;
                        ff.temperature = pendingModificationsState.temperature;
                        ff.acMode = pendingModificationsState.acMode;
                        ff.fanMode = pendingModificationsState.fanMode;
                        ff.swingMode = pendingModificationsState.swingMode;
                        ff.swingStep = pendingModificationsState.swingStep;
                        ff.economyMode == pendingModificationsState.economyMode;
                        ff.messageSource = controllerAddress;
                        ff.messageDest = static_cast<byte>(FujiAddress::UNIT);
                        ff.writeBit = 1;
                        Serial.println("## :: sendFujiFrame. Sending again modifications from " + _controllerName + ", ismaster (drive LCD): " + String(_ismaster) + " :: ##");
                        sendFujiFrame(ff, true); // master send
#ifdef USE_CACHE
                        putCache(ff_bk, ff);
#endif
                        return true;
                    }
                    else
                    {
                        Serial.println(":: " + _controllerName + " :: Unit is off. NOT SENDING UPDATE. A. ff.onOff: " + String(ff.onOff) + ", pendingModificationsState.onOff: " + String(pendingModificationsState.onOff) + ", getRXState().onOff: " + String(getRXState().onOff));
                    }
                }
            }

#ifdef TIMER_MEASUREMENTS_ON
            Serial.println("## " + _controllerName + " :: time F: " + String(millis() - processtimer));
#endif

            // if is slave prepare reply to UNIT
            if (!_ismaster) // reply UNIT master controller
            {
                // we are slave and have received a STATUS type message, we need to reply
                if (ff.messageType == static_cast<byte>(FujiMessageType::STATUS))
                {
                    // we are slave and have received a controllerPresent == 1 flag
                    if (ff.controllerPresent == 1)
                    {
                        // Prepare STATUS type message to send from our address
                        ff.messageSource = controllerAddress;
                        ff.updateMagic = 0;
                        ff.unknownBit = true;
                        ff.writeBit = 0;
                        ff.messageType = static_cast<byte>(FujiMessageType::STATUS);

                        // if we have seen message to secondary controller, and secondary controller is enabled, then we need to send a message to the secondary controller
                        if (seenSecondaryController && _secondaryController)
                        {
                            ff.messageDest = static_cast<byte>(FujiAddress::SECONDARY);
                            ff.loginBit = true;
                            ff.controllerPresent = 0;
                        }
                        // if we haven't seen the secondary controller, then we need to send a message to the indoor unit
                        else
                        {
                            ff.messageDest = static_cast<byte>(FujiAddress::UNIT);
                            ff.loginBit = false;
                            ff.controllerPresent = 1;
                        }
                    }
                    // If we are slave and controllerPresent flag is 0, then we need register ourself as controller with a LOGIN type message
                    else
                    {
                        ff.onOff = 0;
                        ff.temperature = 0;
                        ff.acMode = 0;
                        ff.fanMode = 0;
                        ff.swingMode = 0;
                        ff.swingStep = 0;
                        ff.acError = 0;
                        ff.economyMode = 0;

                        // if we are slave and SLAVE_PRIMARY, then
                        if (_fct == FujiControllerType::SLAVE_PRIMARY)
                        {
                            // if this is the first message we have received, announce ourselves to the indoor unit
                            ff.messageSource = controllerAddress;
                            ff.messageDest = static_cast<byte>(FujiAddress::UNIT);
                            ff.loginBit = false;
                            ff.controllerPresent = 0;
                            ff.updateMagic = 0;
                            ff.unknownBit = true;
                            ff.writeBit = 0;
                            ff.messageType = static_cast<byte>(FujiMessageType::LOGIN);
                        }
                        else if (_fct == FujiControllerType::SLAVE_SECONDARY)
                        {
                            // secondary controller never seems to get any other message types, only status with controllerPresent == 0
                            // the secondary controller seems to send the same flags no matter which message type
                            ff.messageSource = controllerAddress;
                            ff.messageDest = static_cast<byte>(FujiAddress::UNIT);
                            ff.loginBit = false;
                            ff.controllerPresent = 1;
                            ff.updateMagic = 2;
                            ff.unknownBit = true;
                            ff.writeBit = 0;
                        }
                        // If we are SECONDARY but not PRIMARY not SECONDARY, then we are sniffing. No need to reply.
                    }
                }
                // If we receive a LOGIN type message, then we need to reply with a loginBit = 1
                else if (ff.messageType == static_cast<byte>(FujiMessageType::LOGIN))
                {
                    // received a login frame OK frame
                    // the primary will send packet to a secondary controller to see if it exists
                    ff.messageSource = controllerAddress;
                    ff.messageDest = static_cast<byte>(FujiAddress::SECONDARY);
                    ff.loginBit = true;
                    ff.controllerPresent = 1;
                    ff.updateMagic = 0;
                    ff.unknownBit = true;
                    ff.writeBit = 0;

                    // ff.onOff = pendingModificationsState.onOff;
                    // ff.temperature = pendingModificationsState.temperature;
                    // ff.acMode = pendingModificationsState.acMode;
                    // ff.fanMode = pendingModificationsState.fanMode;
                    // ff.swingMode = pendingModificationsState.swingMode;
                    // ff.swingStep = pendingModificationsState.swingStep;
                    // ff.acError = pendingModificationsState.acError;
                }
                // we have received an error frame, we don't need to reply so return
                else if (ff.messageType == static_cast<byte>(FujiMessageType::ERROR))
                {
                    Serial.print(_controllerName + ":: AC ERROR RECV: ");
                    // printFrame(ff, false);
                    latestcompareequal = false;
                    ff.lastUpdate = millis() + 40000000;
                    rx_state = ff;
                    return false;
                }

                //  we only need to reply to the indoor unit if we are not sniffing
                if (!_sniffmode)
                {
#ifdef TIMER_MEASUREMENTS_ON
                    Serial.println("## " + _controllerName + " :: time G: " + String(millis() - processtimer) + ", SEND FRAME");
                    Serial.println("## " + _controllerName + " :: time H: " + String(millis() - processtimer));
#endif
                    ff.unitAddress = controllerAddress;

#ifdef USE_CACHE
                    putCache(ff_bk, ff);
#else
                    // sending frame after processing is slow so there are a lot of collisions. It is not recommended to use this code without cache
                    // Serial.println(_controllerName + ": RX_STATE_REPLY. sendFujiFrame");
                    // sendFujiFrame(ff, true); // slave send
#endif
                    rx_state_reply = ff;
                }
                if (ff.writeBit == 1)
                {
                    Serial.println("Write: 1 detected as slave");
                }
            }
            // If is Master prepare reply to LCD controller
            else
            {
                // if we are master, receive a STATUS type message with writeBit = 1, we need to update our frame
                if (ff.messageType == static_cast<byte>(FujiMessageType::STATUS))
                {
                    if (ff.writeBit == 1)
                    {
                        if (compareParameters(masterState, ff, false, true) != 0)
                        {
                            Serial.println("## :: " + _controllerName + " :: Received and updated parameters are different. Setting Pending modifications pendingModifications=1  B ##");
                            pendingModifications = true;
                            masterState.onOff = ff.onOff;
                            masterState.temperature = ff.temperature;
                            masterState.acMode = ff.acMode;
                            masterState.fanMode = ff.fanMode;
                            masterState.swingMode = ff.swingMode;
                            masterState.swingStep = ff.swingStep;
                            masterState.economyMode = ff.economyMode;
                            Serial.println("\n#@ write: 1 detected: Master state ready to be updated to: onOff: " + String(masterState.onOff) + ", temperature: " + String(masterState.temperature) + ", acMode: " + String(masterState.acMode) + ", fanMode: " + String(masterState.fanMode) + ", swingMode: " + String(masterState.swingMode) + ", swingStep: " + String(masterState.swingStep) + ", economyMode: " + String(masterState.economyMode) + ", controllerPresent: " + String(masterState.controllerPresent) + ", unknownBit: " + String(masterState.unknownBit) + ", loginBit: " + String(masterState.loginBit) + ", writeBit: " + String(masterState.writeBit) + ", updateMagic: " + String(masterState.updateMagic) + ", messageDest: " + String(masterState.messageDest) + ", messageSource: " + String(masterState.messageSource));
                        }
                    }
                }
            }
        }
        // message is not for us, is for SECONDARY, so we don't need to reply but we can sniff the message
        else if (ff.messageDest == static_cast<byte>(FujiAddress::SECONDARY))
        {
            // if (!_ismaster)
            //{
            //  we have received a message from the secondary controller. No need to reply but we can update our temp reading

            // set seenSecondaryController to true so we know we have received a message from the secondary controller
            seenSecondaryController = true;

            // pendingModificationsState.controllerTemp = ff.controllerTemp; // we dont have a temp sensor, use the temp reading from the secondary controller
            if (debugPrint)
            {
                Serial.print(_controllerName + ":: Message sent to Secondary unit. [ ff.messageDest == static_cast<byte>(FujiAddress::SECONDARY) ]\n");
                Serial.print(_controllerName + ":: Received temp: " + String(ff.controllerTemp) + "\n");
            }
            //}
        }
        // message is not for us, is for UNKNOWN
        else
        {
            // if (!_ismaster)
            //{
            //  Unknown destiny, no reply
            if (debugPrint)
            {
                Serial.print(_controllerName + ":: Message sent to Unknown unit. We are controllerAddress: " + String(controllerAddress) + ", messageSrc: " + ff.messageSource + ", messageDest: " + String(ff.messageDest) + "\n");
                printFrame(ff, false, false);
            }
            //}
        }
#ifdef TIMER_MEASUREMENTS_ON
        Serial.println("## " + _controllerName + " :: time I: " + String(millis() - processtimer));
#endif

        // we already handled the message, return true
        return true;
    }
    delay(50);
    if (millis() - timesincelastframeanalisys > 1000)
    {
        timesincelastframeanalisys = millis();
        Serial.println("No data available");
    }
    // if there is no data available, return false
    return false;
}

// compare parameters with 3s priority to the first parameter
int FujiAC::compareParameters(FujiFrame p, FujiFrame p2, bool compareDest, bool verbose)
{
    int return_val = 0;
    bool result = true;
    int i = 0;
    int j = 0;
    bool found = false;
    for (i = 0; i < 5; i++)
    {
        if (compareBuffer.name[i] == _controllerName)
        {
            found = true;
            if (compareBuffer.v1[i] == p.msg_id && compareBuffer.v2[i] == p2.msg_id)
            {
                // Serial.println("Compare id's are the same\n");
                return -1;
            }
            // break;
        }
        if (compareBuffer.name[i] == "")
        {
            j = i;
            break;
        }
    }

    compareBuffer.v1[j] = p.msg_id;
    compareBuffer.v2[j] = p2.msg_id;
    compareBuffer.name[j] = _controllerName;

    String compareResult = "";
    if (p.onOff != p2.onOff)
    {
        compareResult += "onOff :\t" + String(p.onOff) + " != " + String(p2.onOff);
        result = false;
    }
    if (p.temperature != p2.temperature)
    {
        if (p.acMode != 1 && p2.acMode != 1) // if acMode is not 1 (fan only) then compare temperature
        {
            if (p.temperature > 30 || p2.temperature > 30)
            {
                compareResult += "- Temperature is >30: " + String(p.temperature) + "/" + String(p2.temperature) + ". Comparison is not reliable -";
            }
            else if (p.temperature < 16 || p2.temperature < 16)
            {
                compareResult += "- Temperature is <16: " + String(p.temperature) + "/" + String(p2.temperature) + ". Comparison is not reliable -";
            }
            else
            {
                compareResult += ", temperature: " + String(p.temperature) + " != " + String(p2.temperature);
                result = false;
            }
        }
        else
        {
            compareResult += ", temperature: " + String(p.temperature) + " != " + String(p2.temperature) + " but acmode=" + String(p.acMode) + " (ignoring)";
        }
    }
    if (p.acMode != p2.acMode)
    {
        compareResult += ", acMode: " + String(p.acMode) + " != " + String(p2.acMode);
        result = false;
    }
    if (p.fanMode != p2.fanMode)
    {
        compareResult += ", fanMode: " + String(p.fanMode) + " != " + String(p2.fanMode);
        result = false;
    }
    if (p.acError != p2.acError)
    {
        compareResult += ", acError: " + String(p.acError) + " != " + String(p2.acError);
        result = false;
    }
    if (p.economyMode != p2.economyMode)
    {
        compareResult += ", economyMode: " + String(p.economyMode) + " != " + String(p2.economyMode);
        result = false;
    }
    if (p.swingMode != p2.swingMode)
    {
        compareResult += ", swingMode: " + String(p.swingMode) + " != " + String(p2.swingMode);
        result = false;
    }
    if (p.swingStep != p2.swingStep)
    {
        compareResult += ", swingStep: " + String(p.swingStep) + " != " + String(p2.swingStep);
        result = false;
    }
    if (p.controllerPresent != p2.controllerPresent)
    {
        compareResult += ", controllerPresent: " + String(p.controllerPresent) + " != " + String(p2.controllerPresent);
        //result = false;
    }
    if (p.updateMagic != p2.updateMagic)
    {
        compareResult += ", updateMagic (no comparison is done): " + String(p.updateMagic) + " != " + String(p2.updateMagic);
        // result = false;
    }
    if (p.controllerTemp != p2.controllerTemp)
    {
        compareResult += ", controllerTemp (no comparison is done): " + String(p.controllerTemp) + " != " + String(p2.controllerTemp);
        // result = false;
    }
    // if(compareSrcAndDest && (p.messageSource != p2.messageSource))
    //{
    //     compareResult+=", messageSource:\t" + String(p.messageSource) + " != " + String(p2.messageSource));
    //     result = false;
    // }
    if (compareDest && (p.messageDest != p2.messageDest))
    {
        compareResult += ", messageDest: " + String(p.messageDest) + " != " + String(p2.messageDest);
        result = false;
    }

    if (!result && p.onOff == p2.onOff && p.onOff == 0)
    {
        compareResult += ", onOff: Both are off. Comparison is not reliable";
        // Serial.println("####################### ############## " + _controllerName + " ############## #######################\n");
        if (verbose)
            Serial.println(compareResult);
        return -1;
    }

    if (compareResult.length() > 0 || !result)
    {
        if (verbose)
        {
            Serial.println("####################### " + _controllerName + " :: Comparing parameters :: " + String(p.msg_id) + "|" + String(p2.msg_id) + " #######################");
            Serial.print("A ");
            printFrame(p, false, false);
            Serial.print("B ");
            printFrame(p2, false, false);
            Serial.println(compareResult);
        }
    }

    if (!result && (p.acMode == 0 || p2.acMode == 0 || p.acMode > 5 || p2.acMode > 5))
    {
        if (verbose)
            Serial.println("- AC mode is " + String(p.acMode) + "/" + String(p2.acMode) + ". Comparison is not reliable -");
        // Serial.println("####################### ############## " + _controllerName + " ############## #######################\n");
        return -1;
    }

    if (!result)
    {
        // return the FujiFrame with newest timestamp
        if (((p.lastUpdate + 3000 > p2.lastUpdate) && (p.lastUpdate != 0 && p2.lastUpdate != 0)) || (p.msg_id > p2.msg_id))
        {
            return_val = 1;
            if (verbose)
                Serial.println("New: first");
        }
        else
        {
            return_val = 2;
            if (verbose)
                Serial.println("New: second");
        }
    }
    else
    {
        // Serial.println("Parameters are the same");
        return_val = 0;
    }
    // Serial.println("####################### ############## " + _controllerName + " ############## #######################\n");
    return return_val;
}

void FujiAC::connect(HardwareSerial *serial, FujiControllerType fct)
{
    return this->connect(serial, fct, -1, -1, -1);
}

void FujiAC::connect(HardwareSerial *serial, FujiControllerType fct, int rxPin, int txPin, int inh_rx)
{
    bool secondary = false;
    bool sniffMode = false;
    bool ismaster = false;
    _fct = fct;
    _inh_rx = inh_rx;
    String controllerType_str = "UNKNOWN";
    if (fct == FujiControllerType::MASTER)
    {
        ismaster = true;
        secondary = false;
        sniffMode = false;
        masterState.onOff = 0;
        masterState.messageSource = byte(FujiAddress::START);
        masterState.messageDest = byte(FujiAddress::PRIMARY);
        masterState.controllerPresent = 1;
        masterState.updateMagic = 10; // no idea what this is
        masterState.controllerTemp = 0;
        masterState.writeBit = 0;
        masterState.loginBit = 1;
        masterState.unknownBit = 1;
        masterState.messageType = byte(FujiMessageType::STATUS);
        masterState.temperature = masterTemperature; // TODO

        controllerAddress = static_cast<byte>(FujiAddress::UNIT);
        masterState.unitAddress = controllerAddress;
        controllerType_str = "MASTER";
    }
    else if (fct == FujiControllerType::SLAVE_PRIMARY)
    {
        ismaster = false;
        secondary = false;
        sniffMode = false;
        controllerIsPrimary = true;
        controllerAddress = static_cast<byte>(FujiAddress::PRIMARY);
        controllerType_str = "SLAVE_PRIMARY";
    }
    else if (fct == FujiControllerType::SLAVE_SECONDARY)
    {
        ismaster = false;
        secondary = true;
        sniffMode = false;
        controllerIsPrimary = false;
        controllerAddress = static_cast<byte>(FujiAddress::SECONDARY);
        controllerType_str = "SLAVE_SECONDARY";
    }
    else if (fct == FujiControllerType::SNIFFER)
    {
        ismaster = false;
        secondary = false;
        sniffMode = true;
        controllerAddress = 99;
        controllerType_str = "SNIFFER";
    }

    _ismaster = ismaster;
    _sniffmode = sniffMode;
    _serial = serial;
    if (rxPin != -1 && txPin != -1)
    {
#ifdef ESP32
        Serial.print(_controllerName + ":: Setting RX/TX pin to: " + String(rxPin) + "/" + String(txPin) + " and controller type = " + String(controllerType_str) + "\n");
        _serial->begin(500, SERIAL_8E1, rxPin, txPin);
#else
        Serial.print(_controllerName + ":: Setting RX/TX pin unsupported, using defaults.\n");
        _serial->begin(500, SERIAL_8E1);
#endif
    }
    else
    {
        _serial->begin(500, SERIAL_8E1);
    }
    _serial->setTimeout(CONFIG_SERIAL_TIMEOUT);
    //_serial->setRxInvert(true);
    _serial->setRxTimeout(CONFIG_SERIAL_RX_TIMEOUT_SYMBOLS);
    _serial->setRxFIFOFull(CONFIG_SERIAL_RX_FIFOFULL_SYMBOLS);

    // set pins 22,21 as output
    if (_inh_rx != -1)
    {
        pinMode(_inh_rx, INPUT);
        delay(20);
        // digitalWrite(_inh_rx, LOW);
    }

    if (secondary)
    {
    }
    else
    {
    }

    if (RESTORE_STATE_ON_STARTUP == 1)
    {
        // TODO: restore state from eeprom
        FujiFrame ff = getFrameFromEEPROM();
        pendingModificationsState.unitAddress = controllerAddress;
        pendingModificationsState = ff;
    }
    else
    {
        pendingModificationsState = get_offFrame();
    }
    pendingModifications = false;
}

void FujiAC::connectLCDController(HardwareSerial *serial, int rxPin, int txPin)
{
    if (_ismaster) // drive LCD slave controller
    {
        Serial.print(_controllerName);
        Serial.printf("|%3d| :: ERROR. Trying to linking master controller (LCD) in master mode\n\n\n", controllerAddress);
        delay(5000);
        return;
    }
    Serial.print(_controllerName + " :: Connecting LCD controller 1\n");
    _controllerLCD = new FujiAC();
    Serial.print(_controllerName + " :: Connecting LCD controller 2\n");
    _controllerLCD->connect(serial, FujiControllerType::MASTER, rxPin, txPin);
    Serial.print(_controllerName + " :: Connecting LCD controller 3\n");
    _controllerLCD->SetName(("LCD"));
    Serial.print(_controllerName + " :: Connecting LCD controller 4\n");
    _controllerLCDavailable = true;
    if (_readonly)
        _controllerLCD->ReadOnly(true);
}

/* **************************************************************************************
****************************** Secondary functions **************************************
*************************************************************************************** */
// TODO: not send frame if time is more than 250ms and throw alert... but send to cache so next time can be sent
void FujiAC::sendFujiFrame(FujiFrame ff, bool verbose)
{
#ifdef TIMER_MEASUREMENTS_ON
    Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer A: " + String(millis() - processtimer));
#endif
    if (_readonly)
    {
        // Serial.print(_controllerName + " :: ERROR. Trying to send frame in read only mode\n");
        return;
    }

#ifdef TIMER_MEASUREMENTS_ON
    Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer B: " + String(millis() - processtimer));
#endif
    if (_sniffmode)
    {
        Serial.print(_controllerName + " :: ERROR. Trying to send frame in sniff mode\n");
        return;
    }
    // if we are slave, and millis() - processtimer > 250ms, then we need to abort the sending of the frame
    if (!_ismaster && millis() - processtimer > MAX_SEND_MS)
    {
        Serial.print(_controllerName + " :: ERROR. Trying to send frame while process time is more than 250ms\n");
        return;
    }

    if (_inh_rx != -1)
    {
#ifdef TIMER_MEASUREMENTS_ON
        Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer B0: " + String(millis() - processtimer) + ", HIGH INHIBIT");
#endif
        pinMode(_inh_rx, OUTPUT);
        digitalWrite(_inh_rx, HIGH);
        delay(20);
#ifdef TIMER_MEASUREMENTS_ON
        Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer B1: " + String(millis() - processtimer) + ", ");
#endif
    }

#ifdef TIMER_MEASUREMENTS_ON
    Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer C: " + String(millis() - processtimer));
#endif
    ff.msg_id = latest_msg_id;
    latest_msg_id++;
    pendingFrame = ff;
    unsigned long remainingTime = getTimeMSSinceLastRX();

#ifdef TIMER_MEASUREMENTS_ON
    Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer D: " + String(millis() - processtimer));
#endif

    // time after send a new frame from RX
    unsigned long timebetweenmsg = 250; // 50ms if master
    if (!_ismaster)
        timebetweenmsg = 50;

#ifdef TIMER_MEASUREMENTS_ON
    Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer F: " + String(millis() - processtimer) + ", COPY BUF INTO TXLASTSEND");
#endif
    byte *buf;
    buf = new byte[8];
    buf = encodeFrame(pendingFrame);
    // copy buf into txLastSend
    for (int i = 0; i < 8; i++)
    {
        txLastSend[i] = buf[i];
    }

#ifdef TIMER_MEASUREMENTS_ON
    Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer G: " + String(millis() - processtimer));
#endif

    // delay(100);
    //  if we are receiving data from the serial port, we need to wait until we can send a new frame
    if (_serial->available())
    {
        Serial.println(_controllerName + " :: ERROR. Trying to send frame while data is on RX buffer\n");
        return;
    }

#ifdef TIMER_MEASUREMENTS_ON
    Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer G0: " + String(millis() - processtimer) + ", WRITING UART");
#endif

    _serial->write(buf, 8);
#ifdef TIMER_MEASUREMENTS_ON
    Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer G1: " + String(millis() - processtimer) + ", DONE WRITING UART");
    //_serial->flush();

    Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer H: " + String(millis() - processtimer));
#endif
    if (debugPrint || verbose)
    {
        // Serial.print(_controllerName + ":: --> frame\n");
        Serial.println("### pf5");
        printFrame(pendingFrame, true);
    }
#ifdef TIMER_MEASUREMENTS_ON
    Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer I: " + String(millis() - processtimer));
    // delay(50);

    Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer J: " + String(millis() - processtimer));
#endif
    if (_inh_rx != -1)
    {
        // check if UART TX buffer is idle, wait until it is
        int availbytes = _serial->availableForWrite();
        unsigned long startms = millis();
        while (availbytes < UART_BUFFER_RX)
        {
            availbytes = _serial->availableForWrite();
            delay(10);

            if (millis() - startms > 1000)
            {
                Serial.println(_controllerName + " :: ERROR. Timeout waiting for UART TX buffer to be idle, pending bytes: " + String(128 - availbytes) + "\n");
                break;
            }
        }
        // 1 byte is left, wait until is transmitted
        delay(40);

        pinMode(_inh_rx, INPUT);
#ifdef TIMER_MEASUREMENTS_ON
        Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer B0: " + String(millis() - processtimer) + ", LOW INHIBIT");
#endif
    }

#ifdef TIMER_MEASUREMENTS_ON
    Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer K: " + String(millis() - processtimer));
#endif
    // pendingModifications = false;
    int timeout = 0;
    unsigned long startms = millis();
    // while(_serial->available()<8)
    //{
    //     Serial.print(_controllerName + " :: Waiting for readback frame\n");
    //     delay(50);
    //     timeout++;
    //     if (timeout > 40)
    //     {
    //         Serial.print(_controllerName + " :: ERROR. Timeout waiting for readback frame\n");
    //         break;
    //     }
    // }
    // Serial.print(_controllerName + " :: Readback frame received in " + String(millis()-startms) + "ms\n");
    //_serial->readBytes(buf, 8); // read back our own frame so we dont process it again
    // UARTFlushTX();

    // byte readBuf0[8];
    //  if (_serial->available())
    //{
    //      memset(readBuf0, 0, 8);
    //      int bytesRead = _serial->readBytes(readBuf0, 8);
    //      if (bytesRead < 8)
    //      {
    //          // skip incomplete frame
    //          return;
    //      }
    //      //for (int i = 0; i < 8; i++)
    //      //{
    //      //     readBuf0[i] ^= 0xFF;
    //      //}
    //      Serial.print(_controllerName + " READBACK FRAME: " + String(readBuf0[0], HEX) + " " + String(readBuf0[1], HEX) + " " + String(readBuf0[2], HEX) + " " + String(readBuf0[3], HEX) + " " + String(readBuf0[4], HEX) + " " + String(readBuf0[5], HEX) + " " + String(readBuf0[6], HEX) + " " + String(readBuf0[7], HEX) + "\n");
    //  }
    //  else
    //{
    //      // Serial.print(_controllerName + " NO READBACK FRAME\n");
    //  }

    delete[] buf;
#ifdef TIMER_MEASUREMENTS_ON
    Serial.println("## " + _controllerName + " :: sendFujiFrame() - timer L: " + String(millis() - processtimer));
#endif
}

bool FujiAC::read8BFramefromUART(FujiFrame *ff, String *hex)
{
    int availBytes = _serial->available();
    if (availBytes < 8)
    {
        // delay(200);
        //  skip incomplete frame
        Serial.println(_controllerName + " :: CAUTION!!! Incomplete frame received. bytes to read: " + String(availBytes));
        UARTFlushTX();
        return false;
    }

    if (availBytes > 0) // _ismaster availBytes >= 8)
    {
        byte data[8];
        memset(data, 0, 8);
        int bytesRead = _serial->readBytes(data, 8);

        if (bytesRead < 8)
        {
            String hex0 = "";
            for (int i = 0; i < bytesRead; i++)
            {
                hex0 += String(data[i], HEX) + " ";
            }
            if (hex != NULL)
                *hex = hex0;
            // skip incomplete frame
            Serial.println(_controllerName + " :: CAUTION!!! Incomplete frame received. bytes read: " + String(bytesRead) + ". HEX: " + hex0 + " - skipping frame\n");
            UARTFlushTX();
            return false;
        }

        if (txLastSend[0] == data[0] && txLastSend[1] == data[1] && txLastSend[2] == data[2] && txLastSend[3] == data[3] && txLastSend[4] == data[4] && txLastSend[5] == data[5] && txLastSend[6] == data[6] && txLastSend[7] == data[7])
        {
            // Serial.println(_controllerName + " :: CAUTION!!! Readback frame received. Skipping frame");
            return false;
        }

        for (int i = 0; i < 8; i++)
        {
            data[i] ^= 0xFF;
        }
        String hex1 = "";
        for (int i = 0; i < 8; i++)
        {
            hex1 += String(data[i], HEX) + " ";
        }
        if (hex != NULL)
        {
            *hex = hex1;
        }
        *ff = getFrameFromReadArray(data);
        ff->msg_id = latest_msg_id;
        latest_msg_id++;
        ff->unitAddress = controllerAddress;
        return true;
    }

    return false;
}
bool FujiAC::getFrameFromUARTBUFF(FujiFrame *ff)
{
    int count = 0;
    int availBytes = _serial->available();
    if (availBytes <= 0)
    {
        return false;
    }
    // Serial.println("▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬\n" + _controllerName + " :: getFrameFromUARTBUFF: availBytes: " + String(availBytes));

    bool readresult = false;
    do
    {
        String hex = "";
        readresult = (read8BFramefromUART(ff, &hex));
        // if(readresult)//availBytes!=8)
        {
            // Serial.print("- bytes: " + String(availBytes) + ", count: " + String(count) + ": ");
            printFrame(*ff, false);
        }
        availBytes = _serial->available();
        count++;
    } while ((availBytes > 0 && !readresult) && count < 3);
    if (count >= 3)
    {
        Serial.println(_controllerName + " :: CAUTION!!! TIMEOUT");
    }
    Serial.println("▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬\n");

    if (availBytes > 0) // _ismaster availBytes >= 8)
    {
        Serial.println(_controllerName + " :: CAUTION!!! Flushing" + String(availBytes) + " bytes.");
        last_RX_time = millis();
        UARTFlushTX();
    }
    if((ff->messageSource==0 && ff->messageDest==0)||(ff->messageSource==0 && ff->messageDest==1))
    {
        Serial.println(_controllerName + " :: CAUTION!!! CORRUPT FRAME RECEIVED. SRC: "+String(ff->messageSource)+", DEST: "+String(ff->messageDest));
        corruptedFrames_counter++;
        return false;
    }
    // Serial.println(_controllerName + " :: getFrameFromUARTBUFF: final frame RX:");
    // printFrame(*ff, false);
    //  delay(50);
    return true;
}

/* **************************************************************************************
****************************** GET / SET functions **************************************
*************************************************************************************** */

void FujiAC::setOnOff(bool o)
{
    byte bytestatus = 0;
    if (o == true)
        bytestatus = 1;
    else
        bytestatus = 0;

    if (bytestatus != pendingModificationsState.onOff)
    {
        pendingModificationsState.onOff = bytestatus;
        pendingModifications = true;
        Serial.println("Setting OnOff to " + String(pendingModificationsState.onOff));
    }
}

void FujiAC::setTemp(byte t)
{
    if (t < 16)
    {
        Serial.println("CAUTION. Casting (" + String(t) + "). Temperature must be more than 16");
        t = 16;
    }
    if (t > 30)
    {
        Serial.println("CAUTION. Casting (" + String(t) + "). Temperature must be less than 30");
        t = 30;
    }
    if (t != pendingModificationsState.temperature)
    {
        pendingModificationsState.temperature = t;
        pendingModifications = true;
        Serial.println("Setting Temp to " + String(pendingModificationsState.temperature));
    }
}

void FujiAC::setMode(byte m)
{
    if (m < 0)
    {
        m = 0;
    }
    if (m > 5)
    {
        Serial.println("CAUTION. Casting (" + String(m) + "). Mode must be less than 5");
        m = 5;
    }
    if (m != pendingModificationsState.acMode)
    {
        pendingModificationsState.acMode = m;
        pendingModifications = true;
        Serial.println("Setting Mode to " + String(pendingModificationsState.acMode));
    }
}

void FujiAC::setFanMode(byte fm)
{

    if (fm < 0)
    {
        fm = 0;
    }
    if (fm > 4)
    {
        Serial.println("CAUTION. Casting (" + String(fm) + "). Fan mode must be less than 5");
        fm = 4;
    }
    if (fm != pendingModificationsState.fanMode)
    {
        pendingModificationsState.fanMode = fm;
        pendingModifications = true;
        Serial.println("Setting FanMode to " + String(pendingModificationsState.fanMode));
    }
}

void FujiAC::setEconomyMode(byte em)
{

    if (em < 0)
    {
        em = 0;
    }
    if (em > 1)
    {
        Serial.println("CAUTION. Casting (" + String(em) + "). Economy mode must be 0 or 1");
        em = 1;
    }
    if (em != pendingModificationsState.economyMode)
    {
        pendingModificationsState.economyMode = em;
        pendingModifications = true;
        Serial.println("Setting EconomyMode to " + String(pendingModificationsState.economyMode));
    }
}

void FujiAC::setSwingMode(byte sm)
{

    if (sm < 0)
    {
        sm = 0;
    }
    if (sm > 1)
    {
        Serial.println("CAUTION. Casting (" + String(sm) + "). Swing mode must be 0 or 1");
        sm = 1;
    }
    if (sm != pendingModificationsState.swingMode)
    {
        pendingModificationsState.swingMode = sm;
        pendingModifications = true;
        Serial.println("Setting SwingMode to " + String(pendingModificationsState.swingMode));
    }
}

void FujiAC::setSwingStep(byte ss)
{

    if (ss < 0)
    {
        ss = 0;
    }
    if (ss > 1)
    {
        Serial.println("CAUTION. Casting (" + String(ss) + "). Swing step must be 0 or 1");
        ss = 1;
    }
    if (ss != pendingModificationsState.swingStep)
    {
        pendingModificationsState.swingStep = ss;
        pendingModifications = true;
        Serial.println("Setting SwingStep to " + String(pendingModificationsState.swingStep));
    }
}

void FujiAC::setSendAddressParams()
{
    if (_ismaster) // drive LCD slave controller
    {
        pendingModificationsState.messageDest = static_cast<byte>(FujiAddress::START);
        pendingModificationsState.messageSource = static_cast<byte>(FujiAddress::PRIMARY);
    }
    else
    {
        pendingModificationsState.messageDest = controllerAddress;
        pendingModificationsState.messageSource = static_cast<byte>(FujiAddress::UNIT);
    }
}

bool FujiAC::getOnOff()
{

    return pendingModificationsState.onOff == 1 ? true : false;
}

byte FujiAC::getTemp()
{
    return pendingModificationsState.temperature;
}

byte FujiAC::getMode()
{
    return pendingModificationsState.acMode;
}

byte FujiAC::getFanMode()
{
    return pendingModificationsState.fanMode;
}

byte FujiAC::getEconomyMode()
{
    return pendingModificationsState.economyMode;
}

byte FujiAC::getSwingMode()
{
    return pendingModificationsState.swingMode;
}

byte FujiAC::getSwingStep()
{
    return pendingModificationsState.swingStep;
}

byte FujiAC::getControllerTemp()
{
    return pendingModificationsState.controllerTemp;
}

void FujiAC::SetName(String name)
{
    _controllerName = String(name);
}

String FujiAC::GetName()
{
    return _controllerName;
}

void FujiAC::ReadOnly(bool ro)
{
    _readonly = ro;
    if (_controllerLCD != NULL)
        _controllerLCD->ReadOnly(ro);
}

void FujiAC::setSecondaryController(bool enable)
{
    _secondaryController = enable;
}

FujiFrame FujiAC::getRXState()
{
    rx_state.unitAddress = controllerAddress;
    return rx_state;
}

/* **************************************************************************************
******************************** AUX functions ******************************************
*************************************************************************************** */
bool FujiAC::UARTFlushTX(int timeout)
{
    unsigned long start = millis();
    while (_serial->available() > 0)
    {
        _serial->read();
        if (millis() - start > timeout)
        {
            return false;
        }
    }
    return true;
}
unsigned long FujiAC::getTimeMSSinceLastRX()
{
    return (millis() - last_RX_time);
}

FujiFrame FujiAC::getFrameFromReadArray(byte readArray[8])
{
    FujiFrame ff;

    ff.messageSource = readArray[0];
    ff.messageDest = readArray[1] & 0b01111111;
    ff.messageType = (readArray[2] & 0b00110000) >> 4;

    ff.acError = (readArray[kErrorIndex] & kErrorMask) >> kErrorOffset;
    ff.temperature = (readArray[kTemperatureIndex] & kTemperatureMask) >> kTemperatureOffset;
    ff.acMode = (readArray[kModeIndex] & kModeMask) >> kModeOffset;
    ff.fanMode = (readArray[kFanIndex] & kFanMask) >> kFanOffset;
    ff.economyMode = (readArray[kEconomyIndex] & kEconomyMask) >> kEconomyOffset;
    ff.swingMode = (readArray[kSwingIndex] & kSwingMask) >> kSwingOffset;
    ff.swingStep = (readArray[kSwingStepIndex] & kSwingStepMask) >> kSwingStepOffset;
    ff.controllerPresent = (readArray[kControllerPresentIndex] & kControllerPresentMask) >> kControllerPresentOffset;
    ff.updateMagic = (readArray[kUpdateMagicIndex] & kUpdateMagicMask) >> kUpdateMagicOffset;
    ff.onOff = (readArray[kEnabledIndex] & kEnabledMask) >> kEnabledOffset;
    ff.controllerTemp = (readArray[kControllerTempIndex] & kControllerTempMask) >> kControllerTempOffset; // there is one leading bit here that is unknown - probably a sign bit for negative temps?

    ff.writeBit = (readArray[2] & 0b00001000) != 0;
    ff.loginBit = (readArray[1] & 0b00100000) != 0;
    ff.unknownBit = (readArray[1] & 0b10000000) > 0;

    return ff;
}

// Función para guardar un objeto FujiFrame en la EEPROM
void FujiAC::saveFrame2EEPROM(FujiFrame ff)
{
    // Abre el espacio de preferencias con el nombre "fuji_frames" en modo escritura
    preferences.begin("fuji_frames", false);

    // Guarda cada campo de la estructura en la EEPROM
    preferences.putUChar("onOff", ff.onOff);
    preferences.putUChar("temperature", ff.temperature);
    preferences.putUChar("acMode", ff.acMode);
    preferences.putUChar("fanMode", ff.fanMode);
    preferences.putUChar("acError", ff.acError);
    preferences.putUChar("economyMode", ff.economyMode);
    preferences.putUChar("swingMode", ff.swingMode);
    preferences.putUChar("swingStep", ff.swingStep);
    preferences.putUChar("controllerPresent", ff.controllerPresent);
    preferences.putUChar("updateMagic", ff.updateMagic);
    preferences.putUChar("controllerTemp", ff.controllerTemp);
    preferences.putBool("writeBit", ff.writeBit);
    preferences.putBool("loginBit", ff.loginBit);
    preferences.putBool("unknownBit", ff.unknownBit);
    preferences.putUChar("messageType", ff.messageType);
    preferences.putUChar("messageSource", ff.messageSource);
    preferences.putUChar("messageDest", ff.messageDest);

    // Cierra el espacio de preferencias
    preferences.end();
}

// Función para restaurar un objeto FujiFrame desde la EEPROM
FujiFrame FujiAC::getFrameFromEEPROM()
{
    // Crea un nuevo objeto FujiFrame
    FujiFrame ff;

    // Abre el espacio de preferencias con el nombre "fuji_frames" en modo lectura
    preferences.begin("fuji_frames", true);

    // Lee cada campo de la EEPROM y lo asigna al objeto FujiFrame
    ff.onOff = preferences.getUChar("onOff", 0);
    ff.temperature = preferences.getUChar("temperature", 16);
    ff.acMode = preferences.getUChar("acMode", 0);
    ff.fanMode = preferences.getUChar("fanMode", 0);
    ff.acError = preferences.getUChar("acError", 0);
    ff.economyMode = preferences.getUChar("economyMode", 0);
    ff.swingMode = preferences.getUChar("swingMode", 0);
    ff.swingStep = preferences.getUChar("swingStep", 0);
    ff.controllerPresent = preferences.getUChar("controllerPresent", 0);
    ff.updateMagic = preferences.getUChar("updateMagic", 0);
    ff.controllerTemp = preferences.getUChar("controllerTemp", 16);
    ff.writeBit = preferences.getBool("writeBit", false);
    ff.loginBit = preferences.getBool("loginBit", false);
    ff.unknownBit = preferences.getBool("unknownBit", false);
    ff.messageType = preferences.getUChar("messageType", 0);
    ff.messageSource = preferences.getUChar("messageSource", 0);
    ff.messageDest = preferences.getUChar("messageDest", 0);

    // Cierra el espacio de preferencias
    preferences.end();

    // Devuelve el objeto FujiFrame restaurado
    return ff;
}

FujiFrame FujiAC::get_offFrame()
{
    FujiFrame ff;
    ff.onOff = 0;
    ff.temperature = 16;
    ff.acMode = 0;
    ff.fanMode = 0;
    ff.acError = 0;
    ff.economyMode = 0;
    ff.swingMode = 0;
    ff.swingStep = 0;
    ff.controllerPresent = 0;
    ff.updateMagic = 0;
    ff.controllerTemp = 16;
    return ff;
}

FujiFrame FujiAC::get_onFrame()
{
    FujiFrame ff;
    ff.onOff = 1;
    ff.temperature = 16;
    ff.acMode = 0;
    ff.fanMode = 0;
    ff.acError = 0;
    ff.economyMode = 0;
    ff.swingMode = 0;
    ff.swingStep = 0;
    ff.controllerPresent = 0;
    ff.updateMagic = 0;
    ff.controllerTemp = 16;
    return ff;
}

byte *FujiAC::encodeFrame(FujiFrame ff)
{
    byte *tempBuf;
    tempBuf = new byte[8];
    memset(tempBuf, 0, 8);

    tempBuf[0] = ff.messageSource;

    tempBuf[1] &= 0b10000000;
    tempBuf[1] |= ff.messageDest & 0b01111111;
    tempBuf[1] &= 0b01111111;
    if (ff.unknownBit)
    {
        tempBuf[1] |= 0b10000000;
    }

    if (ff.loginBit)
    {
        tempBuf[1] |= 0b00100000;
    }
    else
    {
        tempBuf[1] &= 0b11011111;
    }

    tempBuf[2] &= 0b11001111;
    tempBuf[2] |= ff.messageType << 4;

    if (ff.writeBit)
    {
        tempBuf[2] |= 0b00001000;
    }
    else
    {
        tempBuf[2] &= 0b11110111;
    }

    tempBuf[kModeIndex] = (tempBuf[kModeIndex] & ~kModeMask) | (ff.acMode << kModeOffset);
    tempBuf[kModeIndex] = (tempBuf[kEnabledIndex] & ~kEnabledMask) | (ff.onOff << kEnabledOffset);
    tempBuf[kFanIndex] = (tempBuf[kFanIndex] & ~kFanMask) | (ff.fanMode << kFanOffset);
    tempBuf[kErrorIndex] = (tempBuf[kErrorIndex] & ~kErrorMask) | (ff.acError << kErrorOffset);
    tempBuf[kEconomyIndex] = (tempBuf[kEconomyIndex] & ~kEconomyMask) | (ff.economyMode << kEconomyOffset);
    tempBuf[kTemperatureIndex] = (tempBuf[kTemperatureIndex] & ~kTemperatureMask) | (ff.temperature << kTemperatureOffset);
    tempBuf[kSwingIndex] = (tempBuf[kSwingIndex] & ~kSwingMask) | (ff.swingMode << kSwingOffset);
    tempBuf[kSwingStepIndex] = (tempBuf[kSwingStepIndex] & ~kSwingStepMask) | (ff.swingStep << kSwingStepOffset);
    tempBuf[kControllerPresentIndex] = (tempBuf[kControllerPresentIndex] & ~kControllerPresentMask) | (ff.controllerPresent << kControllerPresentOffset);
    tempBuf[kUpdateMagicIndex] = (tempBuf[kUpdateMagicIndex] & ~kUpdateMagicMask) | (ff.updateMagic << kUpdateMagicOffset);
    tempBuf[kControllerTempIndex] = (tempBuf[kControllerTempIndex] & ~kControllerTempMask) | (ff.controllerTemp << kControllerTempOffset);
    tempBuf[7] = 0b00000000;
    for (int i = 0; i < 8; i++)
    {
        tempBuf[i] ^= 0xFF;
    }
    return tempBuf;
}

void FujiAC::printFrame(FujiFrame ff, bool tx, bool printdirection)
{
    String notrecognized = "*";
    if ((tx == false && ff.messageDest == controllerAddress) || tx == true)
    {
        notrecognized = " ";
    }
    byte *buf = FujiFrame2ByteArray(ff);
    String direction = tx ? "-->" : "<--";
    String cname = _controllerName;
    byte caddress = controllerAddress;
    if (printdirection == false)
    {
        direction = "---";
        cname = "   ";
        caddress = ff.unitAddress;
    }
    Serial.print(cname + "::");
    Serial.printf("|%3d|::", controllerAddress);
    Serial.print(" " + direction + " ");
    Serial.printf("%2X %2X %2X %2X %2X %2X %2X %2X ", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
    Serial.print(notrecognized);
    Serial.printf(" mSrc: %3d, mDst: %3d, onOff: %1d, mType: %1d, temp: %2d | cTemp:%2d, acmode: %1d, fanmode: %1d, write: %1d, login: %1d, unk: %1d, cP:%1d, uMgc:%3d,  acError:%1d, id: %6d \n", ff.messageSource, ff.messageDest, ff.onOff, ff.messageType, ff.temperature, ff.controllerTemp, ff.acMode, ff.fanMode, ff.writeBit, ff.loginBit, ff.unknownBit, ff.controllerPresent, ff.updateMagic, ff.acError, ff.msg_id); // ff.lastUpdate
    delete[] buf;
}

FujiFrames FujiAC::createParameters(byte onOff, byte temperature, byte acMode, byte fanMode, byte acError, byte economyMode, byte swingMode, byte swingStep, byte controllerPresent, byte updateMagic, byte controllerTemp)
{
    FujiFrames p;
    p.onOff = onOff;
    p.temperature = temperature;
    p.acMode = acMode;
    p.fanMode = fanMode;
    p.acError = acError;
    p.economyMode = economyMode;
    p.swingMode = swingMode;
    p.swingStep = swingStep;
    p.controllerPresent = controllerPresent;
    p.updateMagic = updateMagic;
    p.controllerTemp = controllerTemp;
    return p;
}

byte *FujiAC::FujiFrame2ByteArray(FujiFrame ff)
{
    byte *buf;
    // buf = new byte[8];
    buf = encodeFrame(ff);
    return buf;
}

void copy(byte *src, byte *dst, int len)
{
    memcpy(dst, src, sizeof(src[0]) * len);
}

// Print a terminal with options to change temp, mode, fan mode, on/off
void FujiAC::commandsTerminal()
{
    if (_readonly)
    {
        return;
    }
    Serial.println("\n-------------------------------------------------------------------------------\nCommands:");
    Serial.println("t<temp> - Set temperature");
    Serial.println("m<mode> - Set mode");
    Serial.println("f<fan> - Set fan mode");
    Serial.println("o<onoff> - Set on/off");
    Serial.println("q - Quit\n-------------------------------------------------------------------------------\n");

    while (Serial.available() > 0)
    {
        char c = Serial.read();
        // hp.getCurrentState();
        if (c == 'q')
        {
            return;
        }
        if (c == 't')
        {
            int temp = Serial.parseInt();
            setTemp(temp);
            // weWantToUpdate = true;
        }
        if (c == 'm')
        {
            setMode(static_cast<byte>(Serial.parseInt()));
            // weWantToUpdate = true;
        }
        if (c == 'f')
        {
            setFanMode(Serial.parseInt());
            // weWantToUpdate = true;
        }
        if (c == 'o')
        {
            setOnOff(Serial.parseInt());
            // weWantToUpdate = true;
        }
    }

    delay(100);

    return;
}

/* **************************************************************************************
********************************** CACHE functions ***************************************
*************************************************************************************** */
bool FujiAC::putCache(FujiFrame rx_frame, FujiFrame tx_frame)
{
    FujiFrame rx_frame2;
    if (isInCache(rx_frame, &rx_frame2) != -1)
    {
        return true;
    }

    if (cacheBuffer.index >= CACHE_BUFFER_SIZE)
    {
        cacheBuffer.fullcache = true;
        cacheBuffer.index = 0;
        Serial.println("Cache is full. Switching to circular mode.");
    }
    if (cacheBuffer.index < CACHE_BUFFER_SIZE)
    {
        cacheBuffer.rx_frame[cacheBuffer.index] = rx_frame;
        cacheBuffer.tx_frame[cacheBuffer.index] = tx_frame;

        byte *buf_rx, *buf_tx;
        buf_rx = FujiFrame2ByteArray(rx_frame);
        buf_tx = FujiFrame2ByteArray(tx_frame);
        Serial.println(_controllerName + " :: Caching RX [" + String(cacheBuffer.index) + "]: " + String(buf_rx[0], HEX) + " " + String(buf_rx[1], HEX) + " " + String(buf_rx[2], HEX) + " " + String(buf_rx[3], HEX) + " " + String(buf_rx[4], HEX) + " " + String(buf_rx[5], HEX) + " " + String(buf_rx[6], HEX) + " " + String(buf_rx[7], HEX));
        Serial.println(_controllerName + " :: Caching TX [" + String(cacheBuffer.index) + "]: " + String(buf_tx[0], HEX) + " " + String(buf_tx[1], HEX) + " " + String(buf_tx[2], HEX) + " " + String(buf_tx[3], HEX) + " " + String(buf_tx[4], HEX) + " " + String(buf_tx[5], HEX) + " " + String(buf_tx[6], HEX) + " " + String(buf_tx[7], HEX));
        cacheBuffer.index++;
        return true;
    }
    return false;
}

bool FujiAC::putCache(byte *rx_frame, byte *tx_frame)
{
    if (cacheBuffer_raw.index < CACHE_BUFFER_SIZE)
    {
        cacheBuffer_raw.rx_frame[cacheBuffer.index][0] = rx_frame[0];
        cacheBuffer_raw.rx_frame[cacheBuffer.index][1] = rx_frame[1];
        cacheBuffer_raw.rx_frame[cacheBuffer.index][2] = rx_frame[2];
        cacheBuffer_raw.rx_frame[cacheBuffer.index][3] = rx_frame[3];
        cacheBuffer_raw.rx_frame[cacheBuffer.index][4] = rx_frame[4];
        cacheBuffer_raw.rx_frame[cacheBuffer.index][5] = rx_frame[5];
        cacheBuffer_raw.rx_frame[cacheBuffer.index][6] = rx_frame[6];
        cacheBuffer_raw.rx_frame[cacheBuffer.index][7] = rx_frame[7];

        cacheBuffer_raw.tx_frame[cacheBuffer.index][0] = tx_frame[0];
        cacheBuffer_raw.tx_frame[cacheBuffer.index][1] = tx_frame[1];
        cacheBuffer_raw.tx_frame[cacheBuffer.index][2] = tx_frame[2];
        cacheBuffer_raw.tx_frame[cacheBuffer.index][3] = tx_frame[3];
        cacheBuffer_raw.tx_frame[cacheBuffer.index][4] = tx_frame[4];
        cacheBuffer_raw.tx_frame[cacheBuffer.index][5] = tx_frame[5];
        cacheBuffer_raw.tx_frame[cacheBuffer.index][6] = tx_frame[6];
        cacheBuffer_raw.tx_frame[cacheBuffer.index][7] = tx_frame[7];

        cacheBuffer_raw.index++;
        return true;
    }

    Serial.println("Cache is full.");

    return false;
}
int FujiAC::isInCache(FujiFrame rx_frame, FujiFrame *tx_frame)
{
    int i = 0;
    if (compareParameters(cacheBuffer.latest_rx_frame, rx_frame, false, false) == 0)
    {
        *tx_frame = cacheBuffer.latest_tx_frame;
        return cacheBuffer.latest_rx_frame_index;
    }
    for (0; i < CACHE_BUFFER_SIZE; i++)
    {
        if (compareParameters(cacheBuffer.rx_frame[i], rx_frame, false, false) == 0)
        {
            *tx_frame = cacheBuffer.tx_frame[i];
            cacheBuffer.latest_rx_frame_index = i;
            cacheBuffer.latest_rx_frame = rx_frame;
            cacheBuffer.latest_tx_frame = *tx_frame;
            return i;
        }
    }
    return -1;
}

int FujiAC::isInCache(byte *rx_frame, byte *tx_frame)
{
    int i = 0;
    if (rx_frame[0] == cacheBuffer_raw.latest_rx_frame[0] && rx_frame[1] == cacheBuffer_raw.latest_rx_frame[1] && rx_frame[2] == cacheBuffer_raw.latest_rx_frame[2] && rx_frame[3] == cacheBuffer_raw.latest_rx_frame[3] && rx_frame[4] == cacheBuffer_raw.latest_rx_frame[4] && rx_frame[5] == cacheBuffer_raw.latest_rx_frame[5] && rx_frame[6] == cacheBuffer_raw.latest_rx_frame[6] && rx_frame[7] == cacheBuffer_raw.latest_rx_frame[7])
    {
        tx_frame[0] = cacheBuffer_raw.latest_tx_frame[0];
        tx_frame[1] = cacheBuffer_raw.latest_tx_frame[1];
        tx_frame[2] = cacheBuffer_raw.latest_tx_frame[2];
        tx_frame[3] = cacheBuffer_raw.latest_tx_frame[3];
        tx_frame[4] = cacheBuffer_raw.latest_tx_frame[4];
        tx_frame[5] = cacheBuffer_raw.latest_tx_frame[5];
        tx_frame[6] = cacheBuffer_raw.latest_tx_frame[6];
        tx_frame[7] = cacheBuffer_raw.latest_tx_frame[7];
        return cacheBuffer_raw.latest_rx_frame_index;
    }
    for (0; i < cacheBuffer_raw.index; i++)
    {
        if ((cacheBuffer_raw.rx_frame[i][0] == rx_frame[0] && cacheBuffer_raw.rx_frame[i][1] == rx_frame[1] && cacheBuffer_raw.rx_frame[i][2] == rx_frame[2] && cacheBuffer_raw.rx_frame[i][3] == rx_frame[3] && cacheBuffer_raw.rx_frame[i][4] == rx_frame[4] && cacheBuffer_raw.rx_frame[i][5] == rx_frame[5] && cacheBuffer_raw.rx_frame[i][6] == rx_frame[6] && cacheBuffer_raw.rx_frame[i][7] == rx_frame[7]))
        {
            tx_frame[0] = cacheBuffer_raw.tx_frame[i][0];
            tx_frame[1] = cacheBuffer_raw.tx_frame[i][1];
            tx_frame[2] = cacheBuffer_raw.tx_frame[i][2];
            tx_frame[3] = cacheBuffer_raw.tx_frame[i][3];
            tx_frame[4] = cacheBuffer_raw.tx_frame[i][4];
            tx_frame[5] = cacheBuffer_raw.tx_frame[i][5];
            tx_frame[6] = cacheBuffer_raw.tx_frame[i][6];
            tx_frame[7] = cacheBuffer_raw.tx_frame[i][7];
            cacheBuffer_raw.latest_tx_frame[0] = cacheBuffer_raw.tx_frame[i][0];
            cacheBuffer_raw.latest_tx_frame[1] = cacheBuffer_raw.tx_frame[i][1];
            cacheBuffer_raw.latest_tx_frame[2] = cacheBuffer_raw.tx_frame[i][2];
            cacheBuffer_raw.latest_tx_frame[3] = cacheBuffer_raw.tx_frame[i][3];
            cacheBuffer_raw.latest_tx_frame[4] = cacheBuffer_raw.tx_frame[i][4];
            cacheBuffer_raw.latest_tx_frame[5] = cacheBuffer_raw.tx_frame[i][5];
            cacheBuffer_raw.latest_tx_frame[6] = cacheBuffer_raw.tx_frame[i][6];
            cacheBuffer_raw.latest_tx_frame[7] = cacheBuffer_raw.tx_frame[i][7];
            cacheBuffer_raw.latest_rx_frame_index = i;
            return i;
        }
    }
    return -1;
}
