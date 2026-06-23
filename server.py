import os
import json
import time
import webbrowser as web
from sys import executable
from importlib.util import find_spec
from threading import Thread
from collections import deque

require = "flask paho-mqtt"
if not all(find_spec({"paho-mqtt": "paho"}.get(x, x)) for x in require.split()):
    os.system(
        f"{executable} -m pip install -i https://pypi.tuna.tsinghua.edu.cn/simple {require}"
    )
import flask as fl
import paho.mqtt.client as mqtt


class greenhouse(mqtt.Client):
    def __init__(self):
        self.begin = time.time()
        super().__init__(mqtt.CallbackAPIVersion.VERSION2, str(self.begin))
        super().connect("broker.hivemq.com")
        self.latest = {}
        self.history = deque(maxlen=100)

    def on_connect(self, client, userdata, flags, reason_code, properties=None):
        super().subscribe("team1/greenhouse/1/sensor/#")
        super().subscribe("team1/greenhouse/1/status/led")

    def on_message(self, client, userdata, msg: mqtt.MQTTMessage):
        payload = json.loads(msg.payload.decode("utf-8"))
        payload["timestamp"] = time.strftime(
            "%Y-%m-%d %H:%M:%S", time.localtime(self.begin + payload["timestamp"] / 1e3)
        )
        sensor = os.path.basename(msg.topic)
        self.latest[sensor] = payload
        if "value" in payload:
            self.history.append(payload)
        print(sensor, "更新:", payload)


topic_actuator = "team1/greenhouse/1/actuator/led"
client = greenhouse()
app = fl.Flask(__name__)


@app.route("/")
def index():
    return fl.render_template("index.html")


@app.route("/api/latest")
def latest():
    return fl.jsonify(client.latest)


@app.route("/api/history")
def history():
    return fl.jsonify(list(client.history)[::-1])


@app.route("/api/control", methods=["POST"])
def control():
    payload = json.dumps({"state": fl.request.get_json()["state"]})
    if mqtt.MQTT_ERR_SUCCESS == client.publish(topic_actuator, payload).rc:
        print(topic_actuator, "发送:", payload)
        return fl.jsonify({"status": "ok", "msg": payload})
    return fl.jsonify({"status": "error", "msg": ""}), 500


if __name__ == "__main__":
    port = 5000
    Thread(target=app.run, args=("0.0.0.0", port, False), daemon=True).start()
    if True:
        Thread(
            target=lambda: time.sleep(web.open(f"http://localhost:{port}")), daemon=True
        ).start()
    client.loop_forever()
