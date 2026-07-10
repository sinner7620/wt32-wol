#include <Arduino.h>
#include <ETH.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "HomeSpan.h"
#include "config.h"

#ifndef PC_WOL_IP_ADDRESS
#define PC_WOL_IP_ADDRESS PC_IP_ADDRESS
#endif

// WT32-ETH01 uses LAN8720 wired Ethernet.
#define WT32_ETH_PHY_TYPE ETH_PHY_LAN8720
#define WT32_ETH_PHY_ADDR 1
#define WT32_ETH_PHY_MDC 23
#define WT32_ETH_PHY_MDIO 18
#define WT32_ETH_PHY_POWER 16
#define WT32_ETH_CLK_MODE ETH_CLOCK_GPIO0_IN

static bool ethStarted = false;
static bool ethGotIp = false;
static bool ethLinkUp = false;

static IPAddress pcIp;
static IPAddress ethIp;
static IPAddress ethGateway;
static IPAddress ethSubnet;
static IPAddress ethDns;

static void onNetworkEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ethStarted = true;
      ETH.setHostname("wt32-homekit-wol");
      Serial.println("[eth] started");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      ethLinkUp = true;
      Serial.println("[eth] link up");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      ethGotIp = true;
      Serial.print("[eth] ip ");
      Serial.println(ETH.localIP());
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      ethLinkUp = false;
      ethGotIp = false;
      Serial.println("[eth] link down");
      break;
    case ARDUINO_EVENT_ETH_STOP:
      ethStarted = false;
      ethLinkUp = false;
      ethGotIp = false;
      Serial.println("[eth] stopped");
      break;
    default:
      break;
  }
}

static bool parseMac(const char *text, uint8_t mac[6]) {
  int values[6];
  if (sscanf(text, "%x:%x:%x:%x:%x:%x",
             &values[0], &values[1], &values[2],
             &values[3], &values[4], &values[5]) != 6) {
    return false;
  }

  for (int i = 0; i < 6; i++) {
    if (values[i] < 0 || values[i] > 255) {
      return false;
    }
    mac[i] = static_cast<uint8_t>(values[i]);
  }

  return true;
}

static bool sendWakeOnLan() {
  uint8_t mac[6];
  if (!parseMac(PC_MAC_ADDRESS, mac)) {
    Serial.println("[wol] invalid PC_MAC_ADDRESS");
    return false;
  }

  uint8_t packet[102];
  memset(packet, 0xFF, 6);
  for (int i = 1; i <= 16; i++) {
    memcpy(packet + i * 6, mac, 6);
  }

  IPAddress broadcast(
      pcIp[0],
      pcIp[1],
      pcIp[2],
      static_cast<uint8_t>(pcIp[3] | ~ethSubnet[3]));

  WiFiUDP udp;
  if (!udp.begin(9)) {
    Serial.println("[wol] udp begin failed");
    return false;
  }

  udp.beginPacket(broadcast, 9);
  udp.write(packet, sizeof(packet));
  const bool ok = udp.endPacket() == 1;
  udp.stop();

  Serial.printf("[wol] sent to %s:9, ok=%d\n", broadcast.toString().c_str(), ok);
  return ok;
}

#ifndef STATUS_CONNECT_TIMEOUT_MS
#define STATUS_CONNECT_TIMEOUT_MS 400
#endif

#ifndef STATUS_HTTP_TIMEOUT_MS
#define STATUS_HTTP_TIMEOUT_MS 800
#endif

#ifndef SHUTDOWN_CONNECT_TIMEOUT_MS
#define SHUTDOWN_CONNECT_TIMEOUT_MS 2500
#endif

#ifndef SHUTDOWN_HTTP_TIMEOUT_MS
#define SHUTDOWN_HTTP_TIMEOUT_MS 5000
#endif

#ifndef HOMEKIT_WIFI_TX_POWER
#define HOMEKIT_WIFI_TX_POWER WIFI_POWER_8_5dBm
#endif

static void configureWifiForHomeKit() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  const bool txPowerOk = WiFi.setTxPower(HOMEKIT_WIFI_TX_POWER);
  Serial.printf("[wifi] sleep disabled, tx power configured=%d\n", txPowerOk);
}

static bool httpGetOk(const char *url, uint16_t connectTimeoutMs, uint16_t timeoutMs,
                      uint16_t *statusCode = nullptr) {
  HTTPClient http;
  http.setConnectTimeout(connectTimeoutMs);
  http.setTimeout(timeoutMs);

  if (!http.begin(url)) {
    return false;
  }

  const int code = http.GET();
  if (statusCode) {
    *statusCode = code > 0 ? static_cast<uint16_t>(code) : 0;
  }
  http.end();

  return code >= 200 && code < 300;
}

static bool requestShutdown() {
  uint16_t status = 0;
  const bool ok = httpGetOk(SHUTDOWN_URL, SHUTDOWN_CONNECT_TIMEOUT_MS,
                            SHUTDOWN_HTTP_TIMEOUT_MS, &status);
  Serial.printf("[shutdown] GET %s -> %u\n", SHUTDOWN_URL, status);
  return ok;
}

static bool queryPcOnline() {
  uint16_t status = 0;
  const bool ok = httpGetOk(STATUS_URL, STATUS_CONNECT_TIMEOUT_MS,
                            STATUS_HTTP_TIMEOUT_MS, &status);
  Serial.printf("[status] GET %s -> %u\n", STATUS_URL, status);
  return ok;
}

static void startEthernet() {
  pcIp.fromString(PC_WOL_IP_ADDRESS);
  ethIp.fromString(WT32_ETH_IP);
  ethGateway.fromString(WT32_ETH_GATEWAY);
  ethSubnet.fromString(WT32_ETH_SUBNET);
  ethDns.fromString(WT32_ETH_DNS);

  WiFi.onEvent(onNetworkEvent);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ETH.begin(WT32_ETH_PHY_TYPE, WT32_ETH_PHY_ADDR, WT32_ETH_PHY_MDC,
            WT32_ETH_PHY_MDIO, WT32_ETH_PHY_POWER, WT32_ETH_CLK_MODE);
#else
  ETH.begin(WT32_ETH_PHY_ADDR, WT32_ETH_PHY_POWER, WT32_ETH_PHY_MDC,
            WT32_ETH_PHY_MDIO, WT32_ETH_PHY_TYPE, WT32_ETH_CLK_MODE);
#endif

  ETH.config(ethIp, ethGateway, ethSubnet, ethDns);
}

struct PcSwitch : Service::Switch {
  SpanCharacteristic *power;
  uint32_t lastPoll = 0;
  uint32_t wakeGraceUntil = 0;
  bool lastOnline = false;

  PcSwitch() : Service::Switch() {
    power = new Characteristic::On(false);
  }

  boolean update() override {
    const bool desiredOn = power->getNewVal();

    if (desiredOn) {
      const bool ok = sendWakeOnLan();
      if (ok) {
        wakeGraceUntil = millis() + WAKE_GRACE_MS;
        lastOnline = true;
      }
      return ok;
    }

    wakeGraceUntil = 0;
    return requestShutdown();
  }

  void loop() override {
    if (millis() - lastPoll < STATUS_POLL_MS) {
      return;
    }
    lastPoll = millis();

    const bool online = queryPcOnline();
    if (!online && wakeGraceUntil != 0 && static_cast<int32_t>(wakeGraceUntil - millis()) > 0) {
      return;
    }
    wakeGraceUntil = 0;

    if (online != lastOnline) {
      lastOnline = online;
      power->setVal(online);
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("[boot] WT32-ETH01 HomeKit WOL");
  startEthernet();
  configureWifiForHomeKit();

  homeSpan.setPairingCode(HOMEKIT_PAIRING_CODE);
  homeSpan.setQRID(HOMEKIT_QR_ID);
  homeSpan.begin(Category::Switches, HOMEKIT_DEVICE_NAME);

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Manufacturer("Wireless-Tag / ESP32");
      new Characteristic::SerialNumber("WT32-ETH01-WOL");
      new Characteristic::Model("WT32-ETH01");
      new Characteristic::FirmwareRevision("1.0.0");
      new Characteristic::Name(HOMEKIT_DEVICE_NAME);
    new PcSwitch();
}

void loop() {
  homeSpan.poll();
}
