#include <DHT.h>
#include <DHT_U.h>
#include <MPS.h>
#include "config.h"



// ---- Structures de payload ----
/// Valeur du compteur incrémenté toutes les 2s.
struct __attribute__((packed)) CounterPayload {
  uint32_t value;
};

/// État du bouton poussoir.
struct __attribute__((packed)) ButtonPayload {
  uint8_t state;
};

/// Température et humidité mesurées par le DHT11.
struct __attribute__((packed)) DhtPayload {
  int16_t temperatureCentiDegrees; // Température, en centièmes de degré Celsius
  uint8_t humidityPercents; //< Humidité relative, en %
};

// État de la led
struct __attribute__((packed)) LedStatePayload {
  uint8_t state;
};

//Couleur RGB de la led
struct __attribute__((packed)) LedColorPayload {
  uint8_t colorR;
  uint8_t colorG;
  uint8_t colorB;
};

static_assert(sizeof(CounterPayload) == 4, "CounterPayload = 4 octets"); // Filet de sécurité à la compilation
static_assert(sizeof(ButtonPayload) == 1, "ButtonPayload = 1 octets"); // Filet de sécurité à la compilation
static_assert(sizeof(DhtPayload) == 3, "DhtPayload = 3 octets"); // Filet de sécurité à la compilation
static_assert(sizeof(LedStatePayload) == 1, "LedStatePayload = 1 octets"); // Filet de sécurité à la compilation
static_assert(sizeof(LedColorPayload) == 3, "LedColorPayload = 3 octets"); // Filet de sécurité à la compilation

// ---- Variables globales ----
static CounterPayload counter = {0};
static ButtonPayload button = {false};
static DhtPayload dht = {0, 0};
static LedStatePayload ledState = {0};
static LedColorPayload ledColor = {255,255,255};

constexpr uint8_t kButtonPin = 3; // Broche du bouton poussoir
constexpr uint8_t kDhtPin = 0; // Broche du capteur
constexpr uint8_t kLedPin = 2; // Broche de la led 
constexpr uint8_t kDhtType = DHT11; // Type de capteur

// Declare un objet de type DHT
DHT dhtObject(kDhtPin, kDhtType);

// ---- Subjects ----
constexpr mps::Subject kSubHb = mps::makeSubject("hb_" DEVICE_NAME);
constexpr mps::Subject kSubStatusCounter = mps::makeSubject("status_" DEVICE_NAME "_counter");
constexpr mps::Subject kSubStatusButton = mps::makeSubject("status_" DEVICE_NAME "_button");
constexpr mps::Subject kSubValuesDht = mps::makeSubject("status_" DEVICE_NAME "_dht");
constexpr mps::Subject kSubCmdResetCounter = mps::makeSubject("cmd_" DEVICE_NAME "_reset_counter");
constexpr mps::Subject kSubSetStateLed = mps::makeSubject("set_" DEVICE_NAME "_state_led");
constexpr mps::Subject kSubSetColorLed = mps::makeSubject("set_" DEVICE_NAME "_color_led");


// ---- Callbacks ----
// Appelé lors de l'ouverture de la session (callback)
static void onSessionOpened() {
  Serial.println("Session opened!");

  MPS.publishValue(kSubStatusCounter, counter);
  MPS.publishValue(kSubStatusButton, button);
  if (dhtIsWorking())
    MPS.publishValue(kSubValuesDht, dht);
}

// Appelé en cas d'échec d'envoi (callback)
static void onSendFailed(uint16_t sequenceNumber, uint32_t subjectId, mps::SendFailureReason, mps::AcknowledgementStatus) {
  Serial.printf("/!\\ envoi %u (subject 0x%08lX) perdu\n", sequenceNumber, (unsigned long)subjectId);
}

// Remet à 0 la valeur du counter
static void onCmdResetCounter(const mps::FrameView& message, void*) {
  if (message.payloadLength != 0) return; // ce format n'a pas de payload
  counter.value = 0;
  MPS.publishValue(kSubStatusCounter, counter); // l'IHM voit le résultat réel
}

// Allume ou éteint la led en gradant la dernière couleur utilisé si elle doit s'allumer
static void onSetLedState(const mps::FrameView& message, void*) {
  if (message.payloadLength != sizeof(LedStatePayload)) return;

  memcpy(&ledState, message.payload, sizeof(LedStatePayload));
  rgbLedWrite(kLedPin, ledState.state *ledColor.colorR, ledState.state *ledColor.colorG, ledState.state *ledColor.colorB );
}

//change la couleur de la led si elle est allumé sinon elle est juste sauvegardé
static void onSetLedColor(const mps::FrameView& message, void*) {
  if (message.payloadLength != sizeof(LedColorPayload)) return;

  memcpy(&ledColor, message.payload, sizeof(LedColorPayload));
  rgbLedWrite(kLedPin, ledState.state *ledColor.colorR, ledState.state *ledColor.colorG, ledState.state *ledColor.colorB );
}

// ---- Fonctions appelées par setup/loop ----
// vérifie si le capteur dht fonctionne correctement
static bool dhtIsWorking() {
  // Vérification du bon fonctionnement du capteur
  static float humidity = 0;
  static float temperature = 0;
  humidity = dhtObject.readHumidity();
  temperature = dhtObject.readTemperature();
  // Si le câble est débranché, h et t ne seront pas des nombres valides (NaN)
  // ou renverront des valeurs hors normes.
  return !(isnan(humidity) || isnan(temperature) || humidity > 100 || temperature > 100 || temperature < -100);
}

/// Publie périodiquement un heartbeat pour signaler que le capteur fonctionne.
static void publishHeartbeat(){
  static uint32_t lastHeartbeatTimeMs = 0;
  static const uint16_t kHbPeriodMs = 1000;
  if(millis() - lastHeartbeatTimeMs >= kHbPeriodMs) {
    lastHeartbeatTimeMs = millis();
    
    // L'envoi de heartbeats depuis loop() assure au récepteur (IHM) que le code "métier" n'est pas planté
    MPS.publishHeartbeat(kSubHb);
  }
}

// envoi et mise à jour du counter régulière sans bloquer (comparaison au temps écoulé sans delay())
static void publishUpdateCounter(){
  static uint32_t lastCounterTimeMs = 0;
  static const uint16_t kCounterPeriodMs = 2000;
  if(millis() - lastCounterTimeMs >= kCounterPeriodMs) {
    lastCounterTimeMs = millis();
    counter.value++;

    // envoi du counter
    MPS.publishValue(kSubStatusCounter, counter);
  }
}

// envoie la valeur du bouton si elle a changé 
static void publishStateButton(){
  if (digitalRead(kButtonPin) == button.state) return;
  button.state = digitalRead(kButtonPin);
  // le message envoie :
	// quand le bouton est appuye : 0
	// quand le bouton n'est pas appuye : 1
  MPS.publishValue(kSubStatusButton, button);
}

// envoi des valeurs lues par le capteur régulier sans bloquer (comparaison au temps écoulé sans delay())
static void publishValuesDht(){
  static uint32_t lastDhtTimeMs = 0;
  static const uint16_t kDhtPeriodMs = 2000;
  if(millis() - lastDhtTimeMs >= kDhtPeriodMs) {
    lastDhtTimeMs = millis();

    if (!dhtIsWorking()) return;

    dht.temperatureCentiDegrees = int16_t(dhtObject.readTemperature() * 100);// on multiplie par 100 pour récupérer les centièmes
    dht.humidityPercents = uint8_t(dhtObject.readHumidity());
    
    // envoi des valeurs
    MPS.publishValue(kSubValuesDht, dht);
  }
}

// ---- Fonctions Arduino ----
// Exécutée une fois au démarrage
void setup() {
  Serial.begin(115200); // Démarrage de la communication série à 115200 bauds (sortie des Serial.print.. par exemple)
  MPS.setLogOutput(&Serial); // Définition de la sortie des messages de log de la lib MPS

  // enregistrement des callbacks
  MPS.setRegisteredCallback(onSessionOpened);
  MPS.setSendFailedCallback(onSendFailed);
  MPS.on(kSubCmdResetCounter, onCmdResetCounter);
  MPS.on(kSubSetStateLed, onSetLedState);
  MPS.on(kSubSetColorLed, onSetLedColor);

  // constante d'adresse vide pour vérifier que l'adresse a été définie dans config.h
  static const uint8_t kUnsetMacAddress[6] = {0, 0, 0, 0, 0, 0};
  if(memcmp(kGatewayMacAddress, kUnsetMacAddress, 6) == 0) {
    Serial.println("MPS: renseignez kGatewayMacAddress dans config.h (adresse affichee par l'IHM)");
    return;
  }

  // Configuration de la broche en entrée avec résistance de pull-up interne 
  pinMode(kButtonPin, INPUT_PULLUP);

  // Configuration de la broche en sortie
  pinMode(kLedPin, OUTPUT);
  digitalWrite(kLedPin, LOW);

  // démarrage du client MPS
  if(!MPS.begin(kGatewayMacAddress, DEVICE_NAME, kWifiChannel)) Serial.println("MPS: demarrage impossible");

  // Initialise la capteur DHT11
  dhtObject.begin();
}

// Exécutée en boucle (= while(true))
void loop() {
  // appel de la boucle MPS (/!\ du code bloquant dans votre boucle loop() pourrait occasionner des problèmes de connexion)
  MPS.loop();

  // Appel non bloquant de l'envoi d'HB (polling)
  publishHeartbeat();
  
  // ... ajoutez vos envois de données et autres comportements **non bloquants**
  // Appel non bloquant de l'envoi du counter
  publishUpdateCounter();

  // Appel non bloquant de l'envoi de l'état du bouton
  publishStateButton();

  // Appel non bloquant de l'envoi des valeurs lues par le dht
  publishValuesDht();
}
