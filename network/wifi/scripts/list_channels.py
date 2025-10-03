#!/usr/bin/env python3
import csv

CSV_PATH = "../captures/ap_summary_safe.csv"

def main():
    channels = set()
    with open(CSV_PATH, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            ch = row.get("channel", "")
            if ch:
                channels.add(ch)
    print("✅ 발견된 채널 목록:")
    for ch in sorted(channels, key=lambda x: int(x) if x.isdigit() else 9999):
        print(" - 채널", ch)

if __name__ == "__main__":
    main()
