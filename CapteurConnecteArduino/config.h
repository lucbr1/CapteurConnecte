#pragma once

#include <stdint.h>

static const uint8_t kGatewayMacAddress[6] = {0x90,0xDA,0x72,0x49,0x03,0x6C}; // Adresse de la passerelle (écriture hexadécimale)
constexpr uint8_t kWifiChannel = 3; // Canal wifi de la passerelle

#define DEVICE_NAME "sensor_l"