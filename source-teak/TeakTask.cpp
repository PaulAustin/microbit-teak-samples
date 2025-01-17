/*
Copyright (c) 2019 Trashbots, Inc. - SDG
Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include <MicroBit.h>
#include <ctype.h>
#include <algorithm>
#include "MicroBitUARTService.h"
#include "TeakTask.h"
#include "TBCDriver.h"
extern MicroBit uBit;

const int kEmojiHouse = PBMAP(
    PBMAP_ROW(0, 0, 1, 0, 0),
    PBMAP_ROW(0, 1, 0, 1, 0),
    PBMAP_ROW(1, 1, 0, 1, 1),
    PBMAP_ROW(1, 1, 1, 1, 1),
    PBMAP_ROW(1, 1, 1, 1, 1),
    PBMAP_FRAME_COUNT(1));

short corrections[101] = {};
short motor_correction;
//short counter = 0;
extern short versionNumber;
extern bool connected;
//------------------------------------------------------------------------------
// Set up the intial task to be the boot task, this will
// run the startup screen

/*
Menu state machines.
1. On power up a short animation and initial display of m_name
2. Top level menu show small set of optipons
    A - Outputs ( motors)
    B - Sensors ( three sub option Accelerator, Gyro, Temp)
    C - User save programs
3. A Special mode for helping establish BT, also shows m_name
The Taskmanager manages all transistion by watching for globally detectable
events.
  A-LONG -> Go into/outof BT monitor mode.
  B-LONG -> Run (from top) or Reset to top
  A Scroll left in Task
  B Scroll right in Task
*/

// Images are unpacked or merged in one buffer before
// being transfered to the main display.
//ZZZextern TeakTask *gTasks[];
TeakTaskManager gTaskManager;

ManagedString eom(";");

MicroBitUARTService *uart;

//------------------------------------------------------------------------------
TeakTask::TeakTask() {
    m_image = 0;
    m_asyncImage = false;
    m_leftTask = NULL;
    m_rightTask = NULL;
    // uBit.serial.send('\r');
    // uBit.serial.send('\n');
    // uBit.serial.send(-1);
    // uBit.serial.send(' ');
    // uBit.serial.send(-1);
}

//------------------------------------------------------------------------------
void TeakTask::SetAdjacentTasks(TeakTask* leftTask, TeakTask* rightTask)
{
    m_leftTask = leftTask;
    m_rightTask = rightTask;
}

//------------------------------------------------------------------------------
TeakTaskManager::TeakTaskManager()
{
  m_currentTask = NULL;
  m_animating = false;
}

//------------------------------------------------------------------------------
void TeakTaskManager::Setup()
{
    //gpEmojiTask->SetAdjacentTasks(gpMotorTask, gpSensorTask);
    //gpEmojiTask->SetAdjacentTasks(gpEmojiTask, gpEmojiTask);
    //m_currentTask->Event(event);
    //gpMotorTask->SetAdjacentTasks(gpEmojiTask, gpEmojiTask);
    //gpMotorTask->SetAdjacentTasks(gpBlueToothTask, gpEmojiTask);
    //gpBlueToothTask->SetAdjacentTasks(gpSensorTask, gpMotorTask);
    //gpSensorTask->SetAdjacentTasks(gpEmojiTask, gpBlueToothTask);

    // Start with the boot task.
    SwitchTo(gpBootTask);

    // Connect event listeners
    uBit.messageBus.listen(MICROBIT_ID_BLE, MICROBIT_EVT_ANY, this, &TeakTaskManager::MicrobitDalEvent);
    uBit.messageBus.listen(MICROBIT_ID_BLE_UART, MICROBIT_UART_S_EVT_DELIM_MATCH, this, &TeakTaskManager::MicrobitBtEvent);
    uBit.messageBus.listen(MICROBIT_ID_BUTTON_A, MICROBIT_EVT_ANY, this, &TeakTaskManager::MicrobitDalEvent);
    uBit.messageBus.listen(MICROBIT_ID_BUTTON_B, MICROBIT_EVT_ANY, this, &TeakTaskManager::MicrobitDalEvent);
    uBit.messageBus.listen(MICROBIT_ID_BUTTON_AB, MICROBIT_EVT_ANY, this, &TeakTaskManager::MicrobitDalEvent);
    uBit.messageBus.listen(MICROBIT_ID_DISPLAY, MICROBIT_EVT_ANY, this, &TeakTaskManager::MicrobitDalEvent);

    // Get the micro:bit name and make it upper case for legibility on LEDs
    strncpy(m_name, microbit_friendly_name(), sizeof(m_name));
    char* upperName = m_name;
    for (int i = strlen(m_name); i ; i--) {
       *upperName = toupper(*upperName);
       upperName++;
    }

    // Note GATT table size increased from default in MicroBitConfig.h
    // #define MICROBIT_SD_GATT_TABLE_SIZE             0x500

    uart = new MicroBitUARTService(*uBit.ble, 61, 60);
    uart->eventOn(eom, ASYNC);
}

//------------------------------------------------------------------------------
void TeakTaskManager::SwitchTo(TeakTask* task)
{
  m_currentTask = task;
}

#define FLASH_STR_DEFINE(_name, _value) const char _name[] = _value
#define FLASH_STRU(_name) ((const uint8_t*) _name)
#define FLASH_STR(_name) (_name)
#define FLASH_STR_LEN(_name) (sizeof(_name))

FLASH_STR_DEFINE(gStrA, "(a)");
FLASH_STR_DEFINE(gStrB, "(b)");
FLASH_STR_DEFINE(gStrAB, "(ab)");
FLASH_STR_DEFINE(gStrCS, "(cs)");
FLASH_STR_DEFINE(gStrCF, "(cf)");

void TeakTaskManager::calibrate()
{
  uart->send(FLASH_STRU(gStrCS), FLASH_STR_LEN(gStrCS));
  //uBit.serial.send(buffer);

  const int FIRST_VALUE = 0xDEADBEEF;
  int prevEncod1 = FIRST_VALUE;
  int prevEncod2 = FIRST_VALUE;
  short test_power = 40;
  const short THRESHOLD = 40;
  const unsigned short INTERVAL = 30;
  short one_values[THRESHOLD];
  short two_values[THRESHOLD];
  short revolutions = 0;
  //uBit.serial.send('\r');
  //uBit.serial.send('\n');
  while (revolutions != 100)
  {
    if (revolutions == 0)
    {
        SetMotorPower(2, test_power);
        //fiber_sleep(10);
        SetMotorPower(1, -test_power);

        //fiber_sleep(200);
    }
    if (revolutions < THRESHOLD)
    {
        int current1 = ReadEncoder1();
        int current2 = -1*ReadEncoder2();
        int change1 = prevEncod1 == FIRST_VALUE ? 0 : current1-prevEncod1;
        int change2 = prevEncod2 == FIRST_VALUE ? 0 : current2-prevEncod2;
        one_values[revolutions] = change1;
        two_values[revolutions] = change2;
        //uBit.display.scroll('S');
        //uBit.display.scroll(change1);
        //uBit.display.scroll(change2);
        prevEncod1 = current1;
        prevEncod2 = current2;
        revolutions++;
        fiber_sleep(INTERVAL);
    }
    if (revolutions == THRESHOLD)
    {

        std::sort(one_values, one_values+THRESHOLD);
        std::sort(two_values, two_values+THRESHOLD);


        //int temp = 1.0 * (sum2-sum1) / sum2 * test_power;
        int median1 = one_values[THRESHOLD/2-1] + one_values[THRESHOLD/2]+one_values[THRESHOLD/2+1];
        int median2 = two_values[THRESHOLD/2-1] +two_values[THRESHOLD/2]+two_values[THRESHOLD/2+1];
        int median_average = 1.0 * (median1 + median2) / 2;
        int median_temp = 1.0 * (median2-median1) / median_average * test_power;

        // uBit.display.scroll('S');
        // uBit.display.scroll(sum1);
        // uBit.display.scroll(sum2);
        //uBit.display.scroll(median1);
        //uBit.display.scroll(median2);
        //uBit.display.scroll(median_temp);
        //uBit.serial.send('\r');
        //uBit.serial.send('\n');
        //uBit.serial.send(median1);
        //uBit.serial.send(' ');
        //uBit.serial.send(median2);
        //uBit.serial.send(' ');
        //uBit.serial.send(median_temp);
        // uBit.serial.send(' ');
        // uBit.serial.send(counter);
        corrections[test_power] = -median_temp;
        SetMotorPower(1, 0);
        SetMotorPower(2, 0);
        if (test_power != 100)
        {
          revolutions = 0;
          test_power += 10;
          prevEncod1 = FIRST_VALUE;
          prevEncod2 = FIRST_VALUE;
          fiber_sleep(250);
        }
        else
        {
          motor_correction = -median_temp;
          revolutions = 100;
        }
    }
  }
  uart->send(FLASH_STRU(gStrCF), FLASH_STR_LEN(gStrCF));
  //uBit.serial.send(buffer);
}

void TeakTaskManager::disconnect() {
    m_btConnected = false;
    connected = false;
    uBit.display.print('D');
    versionNumber = -10;

}

//------------------------------------------------------------------------------
void TeakTaskManager::MicrobitDalEvent(MicroBitEvent event)
{
    if (event.source == MICROBIT_ID_BLE) {
        if (event.value == MICROBIT_BLE_EVT_CONNECTED) {
            m_btConnected = true;
      			connected = true;
            uBit.display.print('C');
      			versionNumber = 10;
        } else if (event.value == MICROBIT_BLE_EVT_DISCONNECTED) {
            disconnect();
        }
    //} else if (event.value == MICROBIT_BUTTON_EVT_HOLD) {
        //if (event.source == MICROBIT_ID_BUTTON_B) {
          // uBit.display.print('G');
        //}
    } else if (event.value == MICROBIT_BUTTON_EVT_CLICK || event.value == MICROBIT_BUTTON_EVT_HOLD) {
         if (event.source == MICROBIT_ID_BUTTON_A) {
           uart->send(FLASH_STRU(gStrA), FLASH_STR_LEN(gStrA));
         } else if (event.source == MICROBIT_ID_BUTTON_B) {
           uart->send(FLASH_STRU(gStrB), FLASH_STR_LEN(gStrB));
         } else if (event.source == MICROBIT_ID_BUTTON_AB) {
           uart->send(FLASH_STRU(gStrAB), FLASH_STR_LEN(gStrAB));
         }
    } else if (event.source == MICROBIT_ID_DISPLAY) {
      if (event.value == MICROBIT_DISPLAY_EVT_ANIMATION_COMPLETE) {
        m_animating = false;
        m_currentImage = 0;
      }
    }
    else if (event.source == MICROBIT_ID_TIMER) {
        // uBit.serial.send('\r');
        // uBit.serial.send('\n');
        // uBit.serial.send(revolutions);
        // uBit.serial.send(' ');
        // uBit.serial.send(counter);
        // uBit.serial.send(' ');
        // uBit.serial.send(THRESHOLD);
      // if (counter < 100)
      // {
      //   counter++;
      // }
      // if (counter == 100)
      // {
      //   calibrate();
      //   counter++;
      // }
    }


    if (m_currentTask != NULL) {
        if (event.source == MICROBIT_ID_BUTTON_B && event.value == MICROBIT_BUTTON_EVT_HOLD) {
            // The Task swap key is treated as a special event
            event.source = MICROBIT_ID_TASK_SWAP;
            m_currentTask->Event(event);
            event.source = MICROBIT_ID_BUTTON_B;
          }

        m_currentTask->Event(event);
    }

    if (!m_currentTask->AsyncImage()) {
        int newImage = m_currentTask->PackedImage();
        if ((newImage != m_currentImage) && (!m_animating)) {
            // If task image has changed the push it to the LEDs
            PBmapUnpack(newImage, uBit.display.image.getBitmap(), uBit.display.image.getWidth());
            m_currentImage = newImage;
        }
    }
}

//------------------------------------------------------------------------------
int PBmapUnpack(int pbmap, uint8_t* bytes, int width)
{
    int bits = pbmap;
    uint8_t* rowBytes = bytes;
    for (int i=0; i < 5; i++) {
        for (int j=0; j < 5; j++) {
            rowBytes[j] = (bits & (0x01 << (4-j))) ? 128 : 0;
        }
        rowBytes += width;
        bits = bits >> 5;
    }
    return 0;
}

#if 0
// Just for reference
#define MICROBIT_BUTTON_EVT_DOWN                1
#define MICROBIT_BUTTON_EVT_UP                  2
#define MICROBIT_BUTTON_EVT_CLICK               3
#define MICROBIT_BUTTON_EVT_LONG_CLICK          4
#define MICROBIT_BUTTON_EVT_HOLD                5
#define MICROBIT_BUTTON_EVT_DOUBLE_CLICK        6

#define MICROBIT_BUTTON_LONG_CLICK_TIME         1000
#define MICROBIT_BUTTON_HOLD_TIME               1500

#define MICROBIT_BUTTON_STATE                   1
#define MICROBIT_BUTTON_STATE_HOLD_TRIGGERED    2
#define MICROBIT_BUTTON_STATE_CLICK             4
#define MICROBIT_BUTTON_STATE_LONG_CLICK        8
#endif

//------------------------------------------------------------------------------
void setAdvertising(bool state)
{
    if (state) {
        uBit.ble->setTransmitPower(6);
        // TODO??? still needed? no so easy with codal
        // uBit.ble->setAdvertisingInterval(200);
        // uBit.ble->gap().setAdvertisingTimeout(0);
        uBit.ble->advertise();
    } else {
        uBit.ble->stopAdvertising();
    }
}

//------------------------------------------------------------------------------
// Could the code tap into the lower layer, and look for complete expression
// that would be better. It will be helpful to have sctatter string support.
SPI spi(MOSI, MISO, SCK);

int hexCharToInt(char c) {
  if ((c >= '0') && (c <= '9')) {
    return c - '0';
  } else if ((c >= 'a') && (c <= 'f')) {
    return c - 'a' + 10;
  }  else if ((c >= 'A') && (c <= 'F')) {
    return c - 'A' + 10;
  }
  return 0;
}

MicroBitImage image(5,5);
int j = 0;
int pixVal = 1;

// Called when a delimiter is found.
void TeakTaskManager::MicrobitBtEvent(MicroBitEvent)
{
  // If the event was called there should be a message
  ManagedString buff = uart->readUntil(eom, ASYNC);
  StringData *s = buff.leakData();
  // Internal buffer is null terminated as well.
  //teak::tstring command(s->data);
  char* str = s->data;
  int len = strlen(str);
  int value = 0;
  // one command to start with, the picture display
  // P0123456789:
  // the numebers are hex, the five bits are packed into two hex digits.
  if ((strncmp(str, "(px:", 4) == 0) && len >= 14) {
      str += 4;
      for (int i = 0; i < 5; i++) {
          char c1 = hexCharToInt(*str);
          str++;
          char c2 = hexCharToInt(*str);
          str++;
          image.setPixelValue(0, i, (c1 & 0x01) ? 255 : 0);
          image.setPixelValue(1, i, (c2 & 0x08) ? 255 : 0);
          image.setPixelValue(2, i, (c2 & 0x04) ? 255 : 0);
          image.setPixelValue(3, i, (c2 & 0x02) ? 255 : 0);
          image.setPixelValue(4, i, (c2 & 0x01) ? 255 : 0);
      }
      uBit.display.print(image);
  } else if ((strncmp(str, "(sr:", 4) == 0) && len >= 5) {
      str += 4;
      value = atoi(str);
      uBit.io.P1.setServoValue(value);
      uBit.display.scroll("S");
  } else if ((strncmp(str, "(m:", 3) == 0) && len >= 4) {
      // uBit.display.print('M');
      str += 3;
      if(strncmp(str, "(1 2)", 5) == 0) {
          if(strncmp(str + 6, "d", 1) == 0) {
              value = atoi(str + 8);

                int index = (-value+5)/10*10;
                int correction = corrections[index];
                correction = 1.0 * correction / index * (-value);
                if (correction > 0)
                {
                  correction = 0;
                }
                SetMotorPower(1, value);
                SetMotorPower(2, -value+correction);

          }
      } else if(strncmp(str, "1", 1) == 0) {
          if(strncmp(str + 2, "d", 1) == 0) {
              value = atoi(str + 4);
              SetMotorPower(1, value);
          }
      } else if(strncmp(str, "2", 1) == 0) {
          if(strncmp(str + 2, "d", 1) == 0) {
              value = atoi(str + 4);

                int index = (-value+5)/10*10;
                int correction = corrections[index];
                correction = 1.0 * correction / index * (-value);
                if (correction > 0)
                {
                  correction = 0;
                }
                SetMotorPower(2, -value+correction);

          }
      }
  } else if ((strncmp(str, "(nt:", 4) == 0) && len >= 5) {
      // Notes come in the form nn where n is the
      // piano key number
      value = atoi(str + 4);
      if (value <= 13) {
        // early version pay in register0 ( notes 1-12 ( + next C))
        // bump to C4 (key number 40)
        value += 39;
      }
      PlayNote(value, 64);
  } else if ((strncmp(str, "(pr:", 4) == 0) && len >= 5) {
      value = atoi(str + 4);
      m_animating = true;
      uBit.display.scroll(value);
	//   versionNumber = -10;
	//   MicroBitEvent tick(MICROBIT_ID_TIMER, 0, CREATE_ONLY);
	//   	  MicrobitDalEvent(tick);
	//   	  	// uBit.display.scroll("1");
	// 	char buffer [20];
	// 	// uBit.display.scroll("2");
	// 	const char* versionMessage = "(vs:%d)";
	// 	// uBit.display.scroll("3");

	// 	snprintf(buffer, sizeof(buffer), versionMessage, versionNumber);
	// 	// uBit.display.scroll("4");

	// 	uBit.serial.send(buffer);
	// 	// uBit.display.scroll("5");

	// 	uart->send((uint8_t *)buffer, strlen(buffer));
		// uBit.display.scroll("6");
  } else if ((strncmp(str, "(st)", 4) == 0)) {
      stopAll();


  } else if((strncmp(str, "(vr)", 4) == 0)) {
	// uBit.display.scroll("1");
    // char buffer [20];
	// uBit.display.scroll("2");
	// const char* versionMessage = "(vs:%d)";
	// uBit.display.scroll("3");

	// snprintf(buffer, sizeof(buffer), versionMessage, versionNumber);
	// uBit.display.scroll("4");

	// uBit.serial.send(buffer);
	// uBit.display.scroll("5");

	// uart->send((uint8_t *)buffer, strlen(buffer));
	// uBit.display.scroll("6");
	versionNumber = -10;

  } else if ((strncmp(str, "(cl)", 4) == 0)) {
    calibrate();
    //uBit.display.print(kEmojiHouse);
  } else if ((strncmp(str, "(dc)", 4) == 0)) {
    disconnect();
  } else {
      // Debug option, if its not understood show the message.
      // uBit.display.scroll(str);
  }
  s->decr();
}
