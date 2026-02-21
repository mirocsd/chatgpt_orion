from flask import Flask, render_template, request, redirect, url_for, jsonify
from flask_login import LoginManager, UserMixin, login_user, logout_user, login_required
from werkzeug.security import generate_password_hash, check_password_hash
from flask_sqlalchemy import SQLAlchemy
from dotenv import load_dotenv
import os
import datetime

load_dotenv()

app = Flask(__name__)
app.secret_key = os.getenv("SECRET_KEY")
app.config['SQLALCHEMY_DATABASE_URI'] = os.getenv("DATABASE_URL")

db = SQLAlchemy(app)
login_manager = LoginManager()
login_manager.init_app(app)
login_manager.login_view = "login"


# ── Models ────────────────────────────────────────────────

class User(UserMixin, db.Model):
    __tablename__ = "users"
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(80), unique=True, nullable=False)
    password_hash = db.Column(db.String(256), nullable=False)

class Node(db.Model):
    __tablename__ = "nodes"
    node_id = db.Column(db.String, primary_key=True)
    signal_strength = db.Column(db.Integer)
    last_seen = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    connection_quality = db.Column(db.String(50))
    packets = db.relationship('Packet', backref='node', lazy=True)

class Packet(db.Model):
    __tablename__ = "packets"
    packet_id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    sender_id = db.Column(db.String, db.ForeignKey('nodes.node_id'), nullable=False)
    recipient_id = db.Column(db.String, nullable=False)
    timestamp = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    payload = db.Column(db.Text)


# ── Auth ──────────────────────────────────────────────────

@login_manager.user_loader
def load_user(user_id):
    return User.query.get(int(user_id))

@app.route("/signup", methods=["GET", "POST"])
def signup():
    if request.method == "POST":
        username = request.form.get("username", "").strip()
        password = request.form.get("password", "")
        confirm = request.form.get("confirm", "")

        if not username or not password:
            return render_template("signup.html", error="Username and password are required.")

        if User.query.filter_by(username=username).first():
            return render_template("signup.html", error="That username is already taken.")

        if password != confirm:
            return render_template("signup.html", error="Passwords do not match.")

        if len(password) < 8:
            return render_template("signup.html", error="Password must be at least 8 characters.")

        user = User(username=username, password_hash=generate_password_hash(password))
        db.session.add(user)
        db.session.commit()

        login_user(user)
        return redirect(url_for("dashboard"))

    return render_template("signup.html", error=None)

@app.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        username = request.form.get("username", "").strip()
        password = request.form.get("password", "")
        user = User.query.filter_by(username=username).first()

        if user and check_password_hash(user.password_hash, password):
            login_user(user)
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


# ── API Routes ────────────────────────────────────────────

@app.route("/api/nodes", methods=["GET"])
@login_required
def get_nodes():
    nodes = Node.query.all()
    return jsonify([{
        "node_id": n.node_id,
        "signal_strength": n.signal_strength,
        "last_seen": n.last_seen,
        "connection_quality": n.connection_quality
    } for n in nodes])

@app.route("/api/nodes", methods=["POST"])
def receive_node():
    data = request.json
    node = Node.query.get(data['node_id'])
    if node:
        # update existing node
        node.signal_strength = data.get('signal_strength', node.signal_strength)
        node.connection_quality = data.get('connection_quality', node.connection_quality)
        node.last_seen = datetime.datetime.utcnow()
    else:
        # create new node
        node = Node(
            node_id=data['node_id'],
            signal_strength=data.get('signal_strength'),
            connection_quality=data.get('connection_quality')
        )
        db.session.add(node)
    db.session.commit()
    return jsonify({ "status": "ok" })

@app.route("/api/packets", methods=["GET"])
@login_required
def get_packets():
    packets = Packet.query.order_by(Packet.timestamp.desc()).limit(100).all()
    return jsonify([{
        "packet_id": p.packet_id,
        "sender_id": p.sender_id,
        "recipient_id": p.recipient_id,
        "timestamp": p.timestamp,
        "payload": p.payload
    } for p in packets])

@app.route("/api/packets", methods=["POST"])
def receive_packet():
    data = request.json
    packet = Packet(
        sender_id=data['sender_id'],
        recipient_id=data['recipient_id'],
        payload=data.get('payload')
    )
    db.session.add(packet)
    db.session.commit()
    return jsonify({ "status": "ok" })


# ── Init ──────────────────────────────────────────────────

if __name__ == "__main__":
    with app.app_context():
        db.create_all()
    app.run(debug=True)