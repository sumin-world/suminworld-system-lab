#!/usr/bin/env python3
"""
Simple REST API server using Flask.

Endpoints:
- GET  /ping        -> health check
- POST /echo-json   -> echo back JSON payload
"""

from flask import Flask, request, jsonify

app = Flask(__name__)


@app.get("/ping")
def ping():
    """
    Health check endpoint.

    curl http://127.0.0.1:5000/ping
    """
    return jsonify({"status": "ok", "message": "pong"})


@app.post("/echo-json")
def echo_json():
    """
    Echo back JSON payload.

    curl -X POST http://127.0.0.1:5000/echo-json \
         -H "Content-Type: application/json" \
         -d '{"msg": "hello"}'
    """
    try:
        data = request.get_json(force=True)
    except Exception:
        return jsonify({"error": "invalid json"}), 400

    return jsonify({
        "received": data,
        "info": "echo from simple_rest_server",
    })


if __name__ == "__main__":
    # 개발용 서버 실행 (로컬 테스트용)
    app.run(host="127.0.0.1", port=5000, debug=True)
