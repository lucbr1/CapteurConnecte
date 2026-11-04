#pragma once

#include <stdint.h>

static const uint8_t kGatewayMacAddress[6] = {0x90,0xDA,0x72,0x53,0x5D,0x08}; // Adresse de la passerelle (écriture hexadécimale)
constexpr uint8_t kWifiChannel = 1; // Canal wifi de la passerelle

#define DEVICE_NAME "sensor_l"

//drive link : https://drive.google.com/drive/folders/19Gj54nOkQezYLFFQazH_TBjEtd2X_TSx?usp=sharing