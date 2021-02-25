/*
 * Copyright (c) 2015-2016 Chris Howell
 *
 * -------------------------------------------------------------------
 *
 * Additional Adaptation of OpenEVSE ESP Wifi
 * by Trystan Lea, Glyn Hudson, OpenEnergyMonitor
 * All adaptation GNU General Public License as below.
 *
 * -------------------------------------------------------------------
 *
 * This file is part of Open EVSE.
 * Open EVSE is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * Open EVSE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with Open EVSE; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

//#define ENABLE_DEBUG

#include <Arduino.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>               // local OTA update from Arduino IDE
#include <MongooseCore.h>
#include <MicroTasks.h>

#include "emonesp.h"
#include "app_config.h"
#include "net_manager.h"
#include "web_server.h"
#include "ohm.h"
#include "input.h"
#include "emoncms.h"
#include "mqtt.h"
#include "divert.h"
#include "ota.h"
#include "lcd.h"
#include "openevse.h"
#include "root_ca.h"
#include "espal.h"
#include "time_man.h"
#include "tesla_client.h"
#include "event.h"
#include "rfid.h"
#include "sleep_timer.h"
#include "load_balancer.h"
#include "lcd_manager.h"

#include "LedManagerTask.h"

#include "RapiSender.h"

RapiSender rapiSender(&RAPI_PORT);

unsigned long Timer1; // Timer for events once every 30 seconds
unsigned long Timer3; // Timer for events once every 2 seconds
unsigned long Timer4; // Timer for events once every 5 minutes

boolean rapi_read = 0; //flag to indicate first read of RAPI status

static uint32_t start_mem = 0;
static uint32_t last_mem = 0;

static void hardware_setup();

// -------------------------------------------------------------------
// SETUP
// -------------------------------------------------------------------
void setup()
{
  hardware_setup();
  ESPAL.begin();

  DEBUG.println();
  DEBUG.printf("OpenEVSE WiFI %s\n", ESPAL.getShortId().c_str());
  DEBUG.printf("Firmware: %s\n", currentfirmware.c_str());
  DEBUG.printf("IDF version: %s\n", ESP.getSdkVersion());
  DEBUG.printf("Free: %d\n", ESPAL.getFreeHeap());

  // Read saved settings from the config
  config_load_settings();
  DBUGF("After config_load_settings: %d", ESPAL.getFreeHeap());

  MicroTask.startTask(ledManager);

  // Initialise the WiFi
  net_setup();
  DBUGF("After net_setup: %d", ESPAL.getFreeHeap());

  // Initialise Mongoose networking library
  Mongoose.begin();
  Mongoose.setRootCa(root_ca);

  // Bring up the web server
  web_server_setup();

  // Start printing logs to http and mqtt
  SerialDebug.onWrite([](const uint8_t *buffer, size_t size)
  {
    server.sendAll("/debug/console", WEBSOCKET_OP_TEXT, buffer, size);
    String str = (char*)buffer;
    mqtt_log(str);
  });

  DBUGF("After web_server_setup: %d", ESPAL.getFreeHeap());

  loadBalancer.begin();
  sleepTimer.begin();
  lcdManager.begin();
  rfid.begin();

#ifdef ENABLE_OTA
  ota_setup();
  DBUGF("After ota_setup: %d", ESPAL.getFreeHeap());
#endif

  input_setup();

  lcdManager.display("OpenEVSE WiFI", 0, 0, 0);
  lcdManager.display(currentfirmware, 0, 1, 5 * 1000);

  start_mem = last_mem = ESPAL.getFreeHeap();
} // end setup

// -------------------------------------------------------------------
// LOOP
// -------------------------------------------------------------------
void
loop() {
  Profile_Start(loop);

  Profile_Start(Mongoose);
  Mongoose.poll(0);
  Profile_End(Mongoose, 10);

  //lcd_loop();
  web_server_loop();
  net_loop();
#ifdef ENABLE_OTA
  ota_loop();
#endif
  rapiSender.loop();
  divert_current_loop();
  time_loop();

  MicroTask.update();

  if(OpenEVSE.isConnected())
  {
    if(OPENEVSE_STATE_STARTING != state &&
       OPENEVSE_STATE_INVALID != state)
    {
      // Read initial state from OpenEVSE
      if (rapi_read == 0)
      {
        DBUGLN("first read RAPI values");
        handleRapiRead(); //Read all RAPI values
        rapi_read=1;
      }

      // -------------------------------------------------------------------
      // Do these things once every 2s
      // -------------------------------------------------------------------
      if ((millis() - Timer3) >= 2000) {
        update_rapi_values();
        Timer3 = millis();
      }

      // -------------------------------------------------------------------
      // Do these things once every 5 minutes
      // -------------------------------------------------------------------
      if ((millis() - Timer4) >= 300 * 1000){
        uint32_t current = ESPAL.getFreeHeap();
        int32_t diff = (int32_t)(last_mem - current);
        if(diff != 0) {
          DEBUG.printf("%s: Free memory %u - diff %d %d\n", time_format_time(time(NULL)).c_str(), current, diff, start_mem - current);
          last_mem = current;
        }
        rfid.wakeup();
        Timer4 = millis();
      }
    }
  }
  else
  {
    // Check if we can talk to OpenEVSE
    if ((millis() - Timer3) >= 1000)
    {
      // Check state the OpenEVSE is in.
      OpenEVSE.begin(rapiSender, [](bool connected)
      {
        if(connected)
        {
          OpenEVSE.getStatus([](int ret, uint8_t evse_state, uint32_t session_time, uint8_t pilot_state, uint32_t vflags) {
            state = evse_state;
            ledManager.setEvseState(evse_state);
          });
        } else {
          DBUGLN("OpenEVSE not responding or not connected");
        }
      });
      Timer3 = millis();
    }
  }

  if(net_is_connected())
  {
    if (config_tesla_enabled()) {
      teslaClient.loop();
    }

    mqtt_loop();

    // -------------------------------------------------------------------
    // Do these things once every 30 seconds
    // -------------------------------------------------------------------
    if ((millis() - Timer1) >= 30000) {
      DBUGLN("Time1");

      if(!Update.isRunning())
      {
        DynamicJsonDocument data(4096);
        create_rapi_json(data); // create JSON Strings for EmonCMS and MQTT

        emoncms_publish(data);

        teslaClient.getChargeInfoJson(data);
        event_send(data);

        if(config_ohm_enabled()) {
          ohm_loop();
        }
      }

      Timer1 = millis();
    }

    if(emoncms_updated)
    {
      // Send the current state to check the config
      DynamicJsonDocument data(4096);
      create_rapi_json(data);
      emoncms_publish(data);
      emoncms_updated = false;
    }
  } // end WiFi connected

  Profile_End(loop, 10);
} // end loop


void event_send(String &json)
{
  StaticJsonDocument<512> event;
  deserializeJson(event, json);
  event_send(event);
}

void event_send(JsonDocument &event)
{
  #ifdef ENABLE_DEBUG
  serializeJson(event, DEBUG_PORT);
  DBUGLN("");
  #endif
  web_server_event(event);
  mqtt_publish(event);
}

void hardware_setup()
{
  debug_setup();

#ifdef SERIAL_RX_PULLUP_PIN
  // https://forums.adafruit.com/viewtopic.php?f=57&t=153553&p=759890&hilit=esp32+serial+pullup#p769168
  pinMode(SERIAL_RX_PULLUP_PIN, INPUT_PULLUP);
#endif

  enableLoopWDT();
}
