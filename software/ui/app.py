from flask import Flask, render_template, jsonify

app = Flask(__name__)

nodes = [
    {"id": "A1", "quality": 85, "last_seen": "2s ago"},
    {"id": "B4", "quality": 60, "last_seen": "15s ago"}
]

packets = [
    {"type": "RX", "from": "A1", "to": "ME", "status": "ok"},
    {"type": "TX", "from": "ME", "to": "B4", "status": "sent"},
]


@app.route("/dashboard")
def dashboard():
    return render_template("index.html")

@app.route("/")
def landing():
    return render_template("landing.html")


@app.route("/api/nodes")
def get_nodes():
    return jsonify(nodes)

@app.route("/api/packets")
def get_packets():
    return jsonify(packets)

if __name__ == "__main__":
    app.run(debug=True)
