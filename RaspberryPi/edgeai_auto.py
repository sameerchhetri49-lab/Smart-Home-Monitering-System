#logs", "sensor_log.jsonl")

# BLE write throttling
MIN_ACTION_SEND_GAP_SECONDS = 0.5
# INTERNAL STATE
last_telegram_at = 0
last_thingspeak_at = 0
last_action_sent_at = 0

interpreter = None
in_details = None
out_details = None
labels = ["NORMAL", "WARNING", "CRITICAL"]

# UTILITIES
def ensure_folders():
    os.makedirs(BASE_DIR, exist_ok=True)
    os.makedirs(os.path.join(BASE_DIR, "model"), exist_ok=True)
    os.makedirs(os.path.join(BASE_DIR, "logs"), exist_ok=True)

def log(msg):
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    line = f"[{ts}] {msg}"
    print(line)
    try:
        with open(LOG_PATH, "a", encoding="utf-8") as f:
            f.write(line + "\n")
    except Exception:
        pass

def append_sensor_jsonl(obj):
    try:
        with open(DATA_LOG_PATH, "a", encoding="utf-8") as f:
            f.write(json.dumps(obj) + "\n")
    except Exception:
        pass

def safe_float(x, default=0.0):
    try:
        return float(x)
    except Exception:
        return default

def now_s():
    return int(time.time())

# TELEGRAM
def send_telegram(text: str):
    global last_telegram_at
    if not TELEGRAM_ENABLED:
        return
    t = now_s()
    if t - last_telegram_at < TELEGRAM_RATE_LIMIT_SECONDS:
        return
    last_telegram_at = t

    try:
        url = f"https://api.telegram.org/bot{TELEGRAM_BOT_TOKEN}/sendMessage"
        r = requests.post(url, json={"chat_id": TELEGRAM_CHAT_ID, "text": text}, timeout=8)
        if r.status_code != 200:
            log(f"Telegram failed: {r.status_code} {r.text[:120]}")
    except Exception as e:
        log(f"Telegram error: {e}")

# THINGSPEAK (your field mapping)
# field1 gas, field2 flame, field3 temperature, field4 humidity, field5 motion, field6 rain
def update_thingspeak(sensor: dict, action: dict):
    global last_thingspeak_at
    if not THINGSPEAK_ENABLED:
        return

    t = now_s()
    if t - last_thingspeak_at < THINGSPEAK_RATE_LIMIT_SECONDS:
        return
    last_thingspeak_at = t

    try:
        params = {
            "api_key": THINGSPEAK_WRITE_KEY,
            "field1": sensor.get("gas", ""),
            "field2": sensor.get("flame", ""),
            "field3": sensor.get("temp", ""),
            "field4": sensor.get("hum", ""),
            "field5": sensor.get("motion", ""),
            "field6": sensor.get("rainAnalog", ""),  # rain analog value
        }

        url = "https://api.thingspeak.com/update"
        r = requests.get(url, params=params, timeout=8)

        if r.status_code != 200:
            log(f"ThingSpeak HTTP {r.status_code}: {r.text[:120]}")
            return

        entry_id = r.text.strip()
        if entry_id == "0":
            log("ThingSpeak rejected update (returned 0). Check rate limit or API key.")
        else:
            log(f"ThingSpeak updated entry_id={entry_id}")

    except Exception as e:
        log(f"ThingSpeak error: {e}")

# EDGE AI (TFLite) + FALLBACK
def load_tflite():
    global interpreter, in_details, out_details

    if not TFLITE_OK:
        log("tflite-runtime not available. Using rule-based fallback.")
        return False

    if not os.path.exists(MODEL_PATH):
        log(f"Model not found at: {MODEL_PATH}. Using rule-based fallback.")
        return False

    try:
        interpreter = Interpreter(model_path=MODEL_PATH)
        interpreter.allocate_tensors()
        in_details = interpreter.get_input_details()
        out_details = interpreter.get_output_details()
        log(f"Loaded TFLite model: {MODEL_PATH}")
        return True
    except Exception as e:
        log(f"Failed to load model. Fallback rules. Error: {e}")
        return False

def fallback_rules(sensor: dict):
    gas = int(sensor.get("gas", 0))
    flame = int(sensor.get("flame", 0))

    if flame == 1:
        return "CRITICAL", 1.0

    if gas < 170:
        return "NORMAL", 0.7
    elif gas < 400:
        return "WARNING", 0.75
    else:
        return "CRITICAL", 0.85

def policy(state: str, conf: float):
    action = {"state": state, "conf": round(conf, 3)}

    if state == "CRITICAL":
        action.update({
            "buzzer": 1,
            "led": "R",
            "doorServo": 0,
            "clothesServo": 90,
        })
    elif state == "WARNING":
        action.update({
            "buzzer": 0,
            "led": "Y",
        })
    else:
        action.update({
            "buzzer": 0,
            "led": "G",
        })

    return action

def infer_state(sensor: dict):
    flame = int(sensor.get("flame", 0))

    feats = [
        safe_float(sensor.get("gas", 0)),
        safe_float(flame),
        safe_float(sensor.get("temp", -1)),
        safe_float(sensor.get("hum", -1)),
        safe_float(sensor.get("rainAnalog", 0)),
        safe_float(sensor.get("distance", 999)),
        safe_float(sensor.get("motion", 0)),
    ]

    if interpreter is not None and in_details is not None:
        try:
            x = np.array([feats], dtype=np.float32)
            interpreter.set_tensor(in_details[0]["index"], x)
            interpreter.invoke()
            y = interpreter.get_tensor(out_details[0]["index"])[0]
            idx = int(np.argmax(y))
            conf = float(np.max(y))
            state = labels[idx]
        except Exception as e:
            log(f"Inference error, fallback rules: {e}")
            state, conf = fallback_rules(sensor)
    else:
        state, conf = fallback_rules(sensor)

    if flame == 1:
        state = "CRITICAL"
        conf = 1.0

    return policy(state, conf)

# BLE PIPELINE
async def find_arduino():
    log("Scanning for Arduino BLE device...")
    devices = await BleakScanner.discover(timeout=8.0)
    for d in devices:
        if (d.name or "").strip() == ARDUINO_NAME:
            log(f"Found {ARDUINO_NAME}: {d.address}")
            return d.address
    return None

async def run_pipeline():
    ensure_folders()
    model_loaded = load_tflite()
    if model_loaded:
        log("Edge AI: TFLite inference enabled.")
    else:
        log("Edge AI: using fallback rules.")

    while True:
        address = await find_arduino()
        if not address:
            log("Arduino not found. Retrying in 3 seconds...")
            await asyncio.sleep(3)
            continue

        try:
            async with BleakClient(address) as client:
                log("✅ Connected to Arduino BLE")

                def on_sensor_notify(_, data: bytearray):
                    global last_action_sent_at
                    try:
                        text = data.decode("utf-8", errors="ignore").strip()
                        if not text:
                            return

                        sensor = json.loads(text)
                        append_sensor_jsonl(sensor)

                        action = infer_state(sensor)

                        now_t = time.time()
                        if now_t - last_action_sent_at >= MIN_ACTION_SEND_GAP_SECONDS:
                            payload = (json.dumps(action) + "\n").encode("utf-8")
                            asyncio.create_task(
                                client.write_gatt_char(ACTION_CHAR_UUID, payload, response=False)
                            )
                            last_action_sent_at = now_t

                        # ThingSpeak update using YOUR field map
                        update_thingspeak(sensor, action)

                        # Telegram optional
                        if TELEGRAM_ENABLED and action.get("state") in ("WARNING", "CRITICAL"):
                            msg = (
                                f"🏠 SHM Alert: {action.get('state')}\n"
                                f"Gas={sensor.get('gas')} Flame={sensor.get('flame')} Temp={sensor.get('temp')} Hum={sensor.get('hum')}\n"
                                f"Motion={sensor.get('motion')} Rain={sensor.get('rainAnalog')}\n"
                                f"Conf={action.get('conf')}"
                            )
                            send_telegram(msg)

                        log(f"SENSOR -> ACTION: {sensor} -> {action}")

                    except Exception as e:
                        log(f"Notify handler error: {e}")
                        log(traceback.format_exc())

                await client.start_notify(SENSOR_CHAR_UUID, on_sensor_notify)
                log("✅ Subscribed to sensor notifications. Running...")

                while True:
                    await asyncio.sleep(1)

        except Exception as e:
            log("Disconnected / BLE error. Reconnecting soon...")
            log(str(e))
            log(traceback.format_exc())
            await asyncio.sleep(2)

# MAIN
if __name__ == "__main__":
    try:
        asyncio.run(run_pipeline())
    except KeyboardInterrupt:
        log("Stopped by user.")