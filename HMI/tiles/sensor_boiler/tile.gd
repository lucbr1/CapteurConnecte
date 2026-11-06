extends Tile

# ---- CONSTANTES ----
const DEVICE_NAME = "sensor_l"
const SUB_HB = "hb_" + DEVICE_NAME
const SUB_STATUS_COUNTER = "status_" + DEVICE_NAME + "_counter"
const SUB_STATUS_BUTTON = "status_" + DEVICE_NAME + "_button"
const SUB_STATUS_DHT = "status_" + DEVICE_NAME + "_dht"
const SUB_CMD_RESET = "cmd_" + DEVICE_NAME + "_reset_counter"
const SUB_CMD_LED_STATE = "set_" + DEVICE_NAME + "_state_led"
const SUB_CMD_LED_COLOR = "set_" + DEVICE_NAME + "_color_led"
const COUNTER_PAYLOAD_SIZE = 4
const BUTTON_PAYLOAD_SIZE = 1
const DHT_PAYLOAD_SIZE = 3
const LED_COLOR_PAYLOAD_SIZE = 3
const COUNTER_VALUE_OFFSET = 0
const BUTTON_STATE_OFFSET = 0
const DHT_TEMPERATURE_OFFSET = 0
const DHT_HUMIDITY_OFFSET = 2
## Délai sans heartbeat au-delà duquel le capteur est considéré comme bloqué (3,5 périodes de 1 s).
const HB_TIMEOUT_S := 3.5
## Durée d'allumage de l'icône de heartbeat à chaque réception.
const HB_BLINK_S := 0.5

# ---- VARIABLES ----
var _node_id: int = -1
var _seconds_since_hb := INF
# Variable "_connected" avec setter personnalisé pour appeler la MAJ de l'UI au changement d'état
var _connected: bool = false:
	set(value):
		if value == _connected: return
		_connected = value
		if _connected:
			_on_sensor_connected()
		else:
			_on_sensor_disconnected()

func _ready() -> void:
	super() # abonne la tuile
	%ResetButton.pressed.connect(func(): publish(SUB_CMD_RESET, PackedByteArray(), true))
	# Envoie la nouvelle consigne d'état de la LED
	%ButtonLedOnOff.toggled.connect(func(toggled_on: bool):
		var payload := PackedByteArray()
		payload.append(1 if toggled_on else 0)
		publish(SUB_CMD_LED_STATE, payload, true)
	)
	# envoie un payload des trois couleurs quand on appuie sur le bouton de validaton
	# pour modifier la couleur de la led
	%ButtonValidationColor.pressed.connect(func():
		var color = %ColorPickerLed.color
		var payload := PackedByteArray()
		payload.resize(LED_COLOR_PAYLOAD_SIZE)
		payload.encode_u8(0, int(color.r * 255))
		payload.encode_u8(1, int(color.g * 255))
		payload.encode_u8(2, int(color.b * 255))
		publish(SUB_CMD_LED_COLOR, payload, true)
	)
func get_subscriptions() -> PackedStringArray:
	var subscriptions: PackedStringArray = []
	subscriptions.append(SUB_HB)
	subscriptions.append(SUB_STATUS_COUNTER)
	subscriptions.append(SUB_STATUS_BUTTON)
	subscriptions.append(SUB_STATUS_DHT)
	return subscriptions


## Source principale du node_id : la passerelle annonce le capteur par son nom dès son enregistrement,
## avant ses premières publications (dont l'état republié dans onSessionOpened côté Arduino).
func on_node_online(node_id: int, _mac_address: String, device_name: String) -> void:
	if device_name != DEVICE_NAME: return
	_node_id = node_id  # nouvel identifiant possible après un redémarrage du capteur
	%IDLabel.text = "node_id: %d" % _node_id


func on_node_offline(node_id: int, _reason_code: int) -> void:
	if node_id == _node_id: _connected = false


func on_gateway_reachability_changed(is_gateway_reachable: bool) -> void:
	if is_gateway_reachable: return
	_connected = false
	_node_id = -1  # la passerelle peut redistribuer les identifiants en redémarrant


func on_message(subject_name: String, source_id: int, payload: PackedByteArray) -> void:
	if subject_name == SUB_HB: _on_hb(source_id)
	if source_id != _node_id: return # ne traitez que les messages dédiés à votre capteur
	# Ajoutez vos messages à la suite
	if subject_name == SUB_STATUS_COUNTER:
		if payload.size() != COUNTER_PAYLOAD_SIZE: return # le contrat : 4 octets, sinon on ignore
		%CounterLabel.text = "counter: %d" % payload.decode_u32(COUNTER_VALUE_OFFSET)
	if subject_name == SUB_STATUS_BUTTON:
		if payload.size() != BUTTON_PAYLOAD_SIZE: return # le contrat : 1 octet, sinon on ignore
		# le message envoie :
		# quand le bouton est appuye : 0
		# quand le bouton n'est pas appuye : 1
		%ButtonStateIcon.state = !payload.decode_u8(BUTTON_STATE_OFFSET)
	if subject_name == SUB_STATUS_DHT:
		if payload.size() != DHT_PAYLOAD_SIZE: return # le contrat : 3 octets, sinon on ignore
		%TemperatureLabel.text = "temperature: %.2f °C" % (payload.decode_s16(DHT_TEMPERATURE_OFFSET) / 100.0)
		%HumidityLabel.text = "humidity: %d %%" % payload.decode_u8(DHT_HUMIDITY_OFFSET)


func _process(delta: float) -> void:
	_seconds_since_hb += delta
	%HeartbeatIcon.state = _seconds_since_hb < HB_BLINK_S
	if _connected and _seconds_since_hb >= HB_TIMEOUT_S:
		_connected = false  # plus de heartbeat : code du capteur bloqué ou capteur hors de portée


func _on_sensor_connected():
	%IDLabel.text = "node_id: %d" % _node_id
	%ResetButton.disabled = false
	%ConnectedIcon.state = true
	%ButtonLedOnOff.disabled = false
	%ButtonValidationColor.disabled = false
	%ColorPickerLed.color = Color(1,1,1,1)


func _on_sensor_disconnected():
	%IDLabel.text = "node_id: ∅"
	%ConnectedIcon.state = false
	%CounterLabel.text = "counter: ∅"
	%ResetButton.disabled = true
	%ButtonStateIcon.state = false
	%ButtonLedOnOff.set_pressed_no_signal(false)
	%ButtonLedOnOff.disabled = true
	%ButtonValidationColor.disabled = true
	%TemperatureLabel.text = "temperature: ∅"
	%HumidityLabel.text = "humidity: ∅"
	
## @brief Traite un heartbeat : (re)connecte le capteur et relance la temporisation.
## @param source_id  Identifiant de l'appareil émetteur.
func _on_hb(source_id: int):
	# Secours si la tuile n'a pas vu on_node_online (IHM lancée après le capteur, par exemple)
	if _node_id == -1: _node_id = source_id
	if source_id != _node_id: return
	_seconds_since_hb = 0.0
	_connected = true
