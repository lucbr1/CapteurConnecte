# Projet - Création d'un capteur connecté

## Informations du Groupe

### Groupe L

### Membres :
  - Lucas Bieszczad
  - Lucas Brun


## Matériel et Câblage

### Matériel utilisé
- **Carte :** M5Stack Stamp-C3 (ESP32-C3, RISC-V)
- **Capteurs / Actionneurs :** 
  - Capteur de température et d'humidité DHT11
  - LED RGB intégrée (SK6812)
  - Bouton-poussoir intégré

### Configuration du Câblage (Pinout)
| Composant | Pin GPIO (Arduino) | Configuration / Remarques |
| :--- | :--- | :--- |
| **Bouton** | GPIO 3 (`G3`) | Câblé à GND → Mode `INPUT_PULLUP` obligatoire |
| **LED RGB (SK6812)** | GPIO 2 (`G2`) | Pilotée via `rgbLedWrite()` |
| **Capteur DHT11** | GPIO 0 (`G0`) | Signal de données (bibliothèque DHT) |
| **Lecture Analogique** | GPIO 4 (`G4`) / GPIO 5 (`G5`) | Test de lecture flottante via `analogRead()` |

## Structures des données (Subjects)

### `status_DEVICE_NAME_counter`

| Offset | Champ | Type | Unité | Taille |
| ----- | ----- | ----- | ----- | ----- |
| 0 | value | uint32_t | sans unité | 4 octets |

### `status_DEVICE_NAME_button`

| Offset | Champ | Type | Unité | Taille |
| ----- | ----- | ----- | ----- | ----- |
| 0 | state | uint8_t | sans unité | 1 octet |

### `status_DEVICE_NAME_dht`

| Offset | Champ | Type | Unité | Taille |
| ----- | ----- | ----- | ----- | ----- |
| 0 | temperatureCentiDegrees | int16_t | Centième de degré | 2 octets |
| 2 | humidityPercents | uint8_t | Pourcentage (%) | 1 octet |

### `cmd_DEVICE_NAME_reset_counter`

| Offset | Champ | Type | Unité | Taille |
| ----- | ----- | ----- | ----- | ----- |
| - | aucun | - | - | 0 octet |

### `set_DEVICE_NAME_state_led`

| Offset | Champ | Type | Unité | Taille |
| ----- | ----- | ----- | ----- | ----- |
| 0 | state | uint8_t | sans unité | 1 octet |

### `set_DEVICE_NAME_color_led`

| Offset | Champ | Type | Unité | Taille |
| ----- | ----- | ----- | ----- | ----- |
| 0 | colorR | uint8_t | sans unité | 1 octet |
| 1 | colorG | uint8_t | sans unité | 1 octet |
| 2 | colorB | uint8_t | sans unité | 1 octet |
