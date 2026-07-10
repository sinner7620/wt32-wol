from flask import Flask
import os

app = Flask(__name__)

@app.route("/status")
def status():
    return "online", 200

@app.route("/shutdown")
def shutdown():
    os.system("shutdown /s /t 0")
    return "shutdown", 200

if __name__ == "__main__":
    app.run(
        host="0.0.0.0",
        port=8765,
        debug=False
    )