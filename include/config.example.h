#pragma once

// Copy this file to include/config.h and adjust these values before flashing.

// HomeKit pairing code. HomeSpan requires exactly 8 digits, without dashes.
#define HOMEKIT_PAIRING_CODE "11122333"
#define HOMEKIT_QR_ID "WT32"
#define HOMEKIT_DEVICE_NAME "Study PC"

// PC wired Ethernet settings. The WT32-ETH01 Ethernet port is intended to be
// connected directly to the PC Ethernet port for Wake-on-LAN. Keep this direct
// cable on a different subnet from your home Wi-Fi LAN so HomeKit/mDNS stays on
// Wi-Fi and cannot be routed through the PC cable.
#define PC_MAC_ADDRESS "AA:BB:CC:DD:EE:FF"
#define PC_IP_ADDRESS "192.168.50.2"
#define PC_WOL_IP_ADDRESS PC_IP_ADDRESS
#define PC_SUBNET_MASK "255.255.255.0"
#define PC_GATEWAY "0.0.0.0"

// WT32-ETH01 wired Ethernet static address. Do not use DHCP when connected
// directly to a PC, because the PC normally will not assign an address.
#define WT32_ETH_IP "192.168.50.1"
#define WT32_ETH_GATEWAY "0.0.0.0"
#define WT32_ETH_SUBNET "255.255.255.0"
#define WT32_ETH_DNS "8.8.8.8"

// Shutdown helper running on the Windows PC over the direct wired link.
// See tools/windows-shutdown-server.ps1.
#define SHUTDOWN_URL "http://192.168.50.2:8765/shutdown"

// Status polling. ICMP ping needs an extra library on many Arduino setups, so
// this project treats the helper service as the status source.
#define STATUS_URL "http://192.168.50.2:8765/status"
#define STATUS_POLL_MS 15000
#define WAKE_GRACE_MS 120000

// Keep status checks short. When the PC is powered off, long blocking HTTP
// timeouts can starve HomeKit polling and make the accessory show "No Response".
#define STATUS_CONNECT_TIMEOUT_MS 400
#define STATUS_HTTP_TIMEOUT_MS 800
#define SHUTDOWN_CONNECT_TIMEOUT_MS 2500
#define SHUTDOWN_HTTP_TIMEOUT_MS 5000

// Lower Wi-Fi transmit power to reduce current spikes while connecting. If the
// router is far away, try WIFI_POWER_11dBm or WIFI_POWER_13dBm after fixing power.
#define HOMEKIT_WIFI_TX_POWER WIFI_POWER_8_5dBm
