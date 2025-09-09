#!/usr/bin/env python3
import subprocess, csv, re, sys

PCAP = "captures/first_capture.pcap"
OUT  = "captures/ap_summary_safe.csv"

def is_hex(s):
    return bool(re.fullmatch(r"[0-9a-f]+", s))

def hex_to_ascii(h):
    try:
        return bytes.fromhex(h).decode("utf-8", errors="replace")
    except Exception:
        return h

def channel_from_freq(freq):
    try:
        f = int(freq)
    except:
        return "?"
    # 2.4GHz
    ch_map = {
        2412:1,2417:2,2422:3,2427:4,2432:5,2437:6,
        2442:7,2447:8,2452:9,2457:10,2462:11,2467:12,2472:13,2484:14
    }
    if f in ch_map: return ch_map[f]
    # 5GHz: 대략적인 매핑
    if 5000 <= f <= 5900:
        return int((f - 5000) / 5)
    return "?"

def mask_bssid(bssid):
    # 예: 08:5d:dd:9e:83:d5 -> 08:5d:dd:**:**:**
    if re.fullmatch(r"([0-9a-f]{2}:){5}[0-9a-f]{2}", bssid.lower()):
        return bssid[:8] + ":**:**:**"
    return bssid

def mask_ssid(ssid):
    if ssid == "" or ssid == "<MISSING>":
        return ssid
    s = ssid
    if is_hex(ssid):
        s = hex_to_ascii(ssid)
    # 앞 4글자만 노출
    vis = s[:4] if len(s) >= 4 else s
    return f"{vis}_****"

def run_tshark():
    cmd = [
        "tshark", "-r", PCAP, "-Y", "wlan.fc.type_subtype==8",
        "-T", "fields",
        "-e", "wlan.bssid", "-e", "wlan.ssid", "-e", "radiotap.channel.freq"
    ]
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, check=True)
    except FileNotFoundError:
        print("[-] tshark가 설치되어 있지 않습니다. sudo apt install tshark", file=sys.stderr)
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        print(e.stderr, file=sys.stderr)
        sys.exit(1)
    lines = [ln for ln in res.stdout.splitlines() if ln.strip()]
    rows = []
    for ln in lines:
        parts = ln.split("\t")
        if len(parts) < 3: 
            continue
        bssid, ssid, freq = parts[0], parts[1], parts[2]
        rows.append((bssid, ssid, freq))
    return rows

def main():
    rows = run_tshark()
    seen = set()
    kept = []
    for bssid, ssid, freq in rows:
        mb = mask_bssid(bssid)
        ms = mask_ssid(ssid)
        ch = channel_from_freq(freq)
        key = (mb, ms, freq, ch)
        if key not in seen:
            seen.add(key)
            kept.append(key)
    with open(OUT, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["bssid_masked","ssid_masked","freq_mhz","channel"])
        w.writerows(kept)
    print(f"[+] 생성: {OUT} (고유 AP {len(kept)}행)")

if __name__ == "__main__":
    main()
