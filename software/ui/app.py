import json
from flask import Flask, render_template, request, redirect, url_for, jsonify, Response, stream_with_context
from flask_login import LoginManager, UserMixin, login_user, logout_user, login_required
from werkzeug.security import generate_password_hash, check_password_hash
import serial.tools.list_ports
import serial_comm

app = Flask(__name__)
app.secret_key = "supersecretkey"

login_manager = LoginManager()
login_manager.init_app(app)
login_manager.login_view = "login"


USERS = {}  

class User(UserMixin):
    def __init__(self, username):
        self.id = username

@login_manager.user_loader
def load_user(user_id):
    if user_id in USERS:
        return User(user_id)
    return None

@app.route("/signup", methods=["GET", "POST"])
def signup():
    if request.method == "POST":
        username = request.form.get("username", "").strip()
        password = request.form.get("password", "")
        confirm = request.form.get("confirm", "")

        if not username or not password:
            return render_template("signup.html", error="Username and password are required.")

        if username in USERS:
            return render_template("signup.html", error="That username is already taken.")

        if password != confirm:
            return render_template("signup.html", error="Passwords do not match.")

        if len(password) < 8:
            return render_template("signup.html", error="Password must be at least 8 characters.")

        USERS[username] = generate_password_hash(password)


        login_user(User(username))
        return redirect(url_for("dashboard"))

    return render_template("signup.html", error=None)

@app.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        username = request.form.get("username", "").strip()
        password = request.form.get("password", "")

        if username in USERS and check_password_hash(USERS[username], password):
            login_user(User(username))
            return redirect(url_for("dashboard"))

        return render_template("login.html", error="Invalid username or password")

    return render_template("login.html", error=None)

@app.route("/logout")
@login_required
def logout():
    logout_user()
    return redirect(url_for("login"))

@app.route("/")
@login_required
def dashboard():
    return render_template("index.html")

@app.route("/dashboard")
@login_required
def dashboard_alias():
    return render_template("index.html")

@app.route("/api/ports")
@login_required
def list_ports():
    ports = serial.tools.list_ports.comports()
    return jsonify([
        {"device": p.device, "description": p.description}
        for p in sorted(ports, key=lambda p: p.device)
    ])

@app.route("/api/serial/connect", methods=["POST"])
@login_required
def connect_serial():
    port = request.json.get("port")
    baudrate = request.json.get("baudrate", 115200)
    if serial_comm.connect(port, baudrate):
        return jsonify({"message": "Serial connection established"})
    else:
        return jsonify({"message": "Failed to connect to serial port"}), 500

@app.route("/api/serial/disconnect", methods=["POST"])
@login_required
def disconnect_serial():
    if serial_comm.disconnect():
        return jsonify({"message": "Serial connection closed"})
    else:
        return jsonify({"message": "Failed to disconnect from serial port"}), 500

@app.route("/api/serial/stream")
@login_required
def stream_serial():
    def generate():
        for data in serial_comm.receive():
            yield f"data: {json.dumps(data)}\n\n"
    return Response(stream_with_context(generate()), mimetype="text/event-stream")

if __name__ == "__main__":
    app.run(debug=True)