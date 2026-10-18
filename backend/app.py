import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from flask import Flask, request, jsonify
from flask_cors import CORS

from config import HOST, PORT
import locker

app = Flask(__name__)
CORS(app)


@app.get("/api/status")
def status():
    return jsonify({"running": True})


@app.get("/api/locked")
def locked():
    try:
        items = locker.list_locked()
        return jsonify({"success": True, "items": items})
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500


@app.post("/api/lock")
def lock():
    data = request.get_json(silent=True) or {}
    folder_path = (data.get("path") or "").strip()
    password = data.get("password", "")
    if not folder_path:
        return jsonify({"success": False, "error": "Specify folder path"}), 400
    if not password:
        return jsonify({"success": False, "error": "Specify password"}), 400
    result = locker.lock_folder(folder_path, password)
    status_code = 200 if result.get("success") else 400
    return jsonify(result), status_code


@app.post("/api/unlock")
def unlock():
    data = request.get_json(silent=True) or {}
    folder_id = data.get("id")
    original_path = data.get("path")
    password = data.get("password", "")
    if not folder_id and not original_path:
        return jsonify({"success": False, "error": "Specify id or path"}), 400
    if not password:
        return jsonify({"success": False, "error": "Specify password"}), 400
    result = locker.unlock_folder(
        folder_id=folder_id, original_path=original_path, password=password
    )
    status_code = 200 if result.get("success") else 400
    return jsonify(result), status_code


def main():
    from config import VAULT_DIR
    VAULT_DIR.mkdir(parents=True, exist_ok=True)
    locker.init_db()
    app.run(host=HOST, port=PORT, debug=False, use_reloader=False)


if __name__ == "__main__":
    main()
