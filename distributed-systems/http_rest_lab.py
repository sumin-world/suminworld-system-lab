#!/usr/bin/env python3
"""
Simple REST API client lab using https://httpbin.org

- GET 요청 + 쿼리 파라미터
- POST 요청 + JSON 바디
- 응답 JSON 파싱 및 출력
"""

import json
import textwrap

import requests


def main() -> None:
    # 1) GET /get 요청 (쿼리 파라미터 포함)
    get_url = "https://httpbin.org/get"
    get_params = {
        "user": "sumin",
        "lab": "system",
    }

    print(f"[INFO] GET {get_url} params={get_params}")
    resp_get = requests.get(get_url, params=get_params, timeout=5)

    print(f"[INFO] status code = {resp_get.status_code}")
    data_get = resp_get.json()
    print("[INFO] response JSON (GET):")
    print(json.dumps(data_get, indent=2, ensure_ascii=False))

    print("\n" + "=" * 60 + "\n")

    # 2) POST /post 요청 (JSON 바디)
    post_url = "https://httpbin.org/post"
    post_payload = {
        "message": "hello from suminworld-system-lab",
        "tags": ["rest", "json", "lab"],
    }

    print(f"[INFO] POST {post_url} json={post_payload}")
    resp_post = requests.post(post_url, json=post_payload, timeout=5)

    print(f"[INFO] status code = {resp_post.status_code}")
    data_post = resp_post.json()
    print("[INFO] response JSON (POST):")
    print(json.dumps(data_post, indent=2, ensure_ascii=False))

    print("\n" + "=" * 60 + "\n")

    # 3) 요약
    summary = textwrap.dedent(
        f"""
        === Summary ===
        - GET  {get_url} -> {resp_get.status_code}
        - POST {post_url} -> {resp_post.status_code}

        Check "args" field in GET response for query params.
        Check "json" field in POST response for JSON payload.
        """
    ).strip()
    print(summary)


if __name__ == "__main__":
    main()
