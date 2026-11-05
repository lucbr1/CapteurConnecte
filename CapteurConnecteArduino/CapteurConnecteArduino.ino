#include <DHT.h>
#include <DHT_U.h>
#include <MPS.h>
#include "config.h"

// Broche du bouton poussoir
#define BUTTON_PIN 3
// Broche du capteur
#define DHT_PIN 0

// Type de capteur
#define DHTTYPE DHT11

// ---- Structures de payload ----
// placer ici vos structures de payloads :
/// Valeur du compteur incrémenté toutes les 2s.
struct __attribute__((packed)) CounterPayload {
  uint32_t value;
};

/// État du bouton poussoir.
struct __attribute__((packed)) ButtonPayload {
  bool state;
};

/// Température et humidité mesurées par le DHT11.
struct __attribute__((packed)) DhtPayload {
  uint16_t temperatureCentiDegrees; // Température, en centièmes de degré Celsius
  uint8_t humidityPercents; //< Humidité relative, en %
};


static_assert(sizeof(CounterPayload) == 4, "CounterPayload = 4 octets"); // Filet de sécurité à la compilation
static_assert(sizeof(ButtonPayload) == 1, "ButtonPayload = 1 octets"); // Filet de sécurité à la compilation
static_assert(sizeof(DhtPayload) == 3, "DhtPayload = 3 octets"); // Filet de sécurité à la compilation

// ---- Variables globales ----
// placez ici vos variables à partager entre plusieurs contextes (ex : setup, loop, callbacks)
static CounterPayload counter = {0};
static ButtonPayload button = {false};
static DhtPayload dht = {0, 0};
// Declare un objet de type DHT
DHT dhtObject(DHT_PIN, DHTTYPE);

// ---- Subjects ----
constexpr mps::Subject kSubHb = mps::makeSubject("hb_" DEVICE_NAME);
// ... ajoutez vos sujets ici sur le même schéma que le heartbeat, mais en précisant la donnée envoyée,
constexpr mps::Subject kSubStatusCounter = mps::makeSubject("status_" DEVICE_NAME "_counter");
constexpr mps::Subject kSubStatusButton = mps::makeSubject("status_" DEVICE_NAME "_button");
constexpr mps::Subject kSubValuesDht = mps::makeSubject("status_" DEVICE_NAME "_dht");
constexpr mps::Subject kSubCmdResetCounter = mps::makeSubject("cmd_" DEVICE_NAME "_reset_counter");


// ---- Callbacks ----
// Appelé lors de l'ouverture de la session (callback)
static void onSessionOpened() {
  Serial.println("Session opened!");
  // republiez ici l’état initial, la passerelle ne garde rien
  MPS.publishValue(kSubStatusCounter, counter);
  MPS.publishValue(kSubStatusButton, button);
}

// Appelé en cas d'échec d'envoi (callback)
static void onSendFailed(uint16_t sequenceNumber, uint32_t subjectId, mps::SendFailureReason, mps::AcknowledgementStatus) {
  Serial.printf("/!\\ envoi %u (subject 0x%08lX) perdu\n", sequenceNumber, (unsigned long)subjectId);
}

// ... ajoutez vos callbacks ici, ex : 
static void onCmdResetCounter(const mps::FrameView& message, void*) {
  if (message.payloadLength != 0) return; // ce format n'a pas de payload
  counter.value = 0;
  MPS.publishValue(kSubStatusCounter, counter); // l'IHM voit le résultat réel
}

// ---- Fonctions appelées par setup/loop ----
// envoi de heartbeats réguliers sans bloquer (comparaison au temps écoulé sans delay())
void publishHeartbeat(){
  static uint32_t lastHeartbeatTimeMs = 0;
  static const uint16_t kHbPeriod = 1000;
  if(millis() - lastHeartbeatTimeMs >= kHbPeriod) {
    lastHeartbeatTimeMs = millis();
    
    // L'envoi de heartbeats depuis loop() assure au récepteur (IHM) que le code "métier" n'est pas planté
    MPS.publishHeartbeat(kSubHb);
  }
}

// envoi et mise à jour du counter régulière sans bloquer (comparaison au temps écoulé sans delay())
void publishUpdateCounter(){
  static uint32_t lastCounterTimeMs = 0;
  static const uint16_t kcounterPeriod = 2000;
  if(millis() - lastCounterTimeMs >= kcounterPeriod) {
    lastCounterTimeMs = millis();
    counter.value++;

    // envoi du counter
    MPS.publishValue(kSubStatusCounter, counter);
  }
}

// envoie la valeur du bouton si elle a changé 
void publishStateButton(){
  if (digitalRead(BUTTON_PIN) == button.state) return;
  button.state = digitalRead(BUTTON_PIN);
  // le message envoie :
	// quand le bouton est appuye : 0
	// quand le bouton n'est pas appuye : 1
  MPS.publishValue(kSubStatusButton, button);
}

// envoi des valeurs lues par le capteur régulier sans bloquer (comparaison au temps écoulé sans delay())
void publishValuesDht(){
  static uint32_t lastDhtTimeMs = 0;
  static const uint16_t kDhtPeriod = 2000;
  if(millis() - lastDhtTimeMs >= kDhtPeriod) {
    lastDhtTimeMs = millis();

    dht.temperatureCentiDegrees = uint16_t(dhtObject.readTemperature() * 100);// on multiplie par 100 pour récupérer les centièmes
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
  // ... ajoutez vos callbacks à l'aide de MPS.on(const mps::Subject& subject, mps::MessageHandler handler, void* userContext = nullptr)
  MPS.on(kSubCmdResetCounter, onCmdResetCounter);

  // constante d'adresse vide pour vérifier que l'adresse a été définie dans config.h
  static const uint8_t kUnsetMacAddress[6] = {0, 0, 0, 0, 0, 0};
  if(memcmp(kGatewayMacAddress, kUnsetMacAddress, 6) == 0) {
    Serial.println("MPS: renseignez kGatewayMacAddress dans config.h (adresse affichee par l'IHM)");
    return;
  }

  // Configuration de la broche en entrée avec résistance de pull-up interne 
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // démarrage du client MPS
  if(!MPS.begin(kGatewayMacAddress, DEVICE_NAME, kWifiChannel)) Serial.println("MPS: demarrage impossible");

  // Initialise la capteur DHT11
  dhtObject.begin();
  // Vérification du bon fonctionnement du capteur
  if (isnan(dhtObject.readHumidity()) && isnan(dhtObject.readTemperature())) {
    Serial.println("DHT: demarrage impossible (capteur introuvable ou defectueux)");
  }
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
