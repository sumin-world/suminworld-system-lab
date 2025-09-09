# Day 1 — Monitor 모드 & 첫 캡처
- 어댑터: ALFA AWUS036ACM (MT7612U)
- 모드: Monitor (2412 MHz = ch1)
- 캡처: `captures/first_capture.pcap` (320 pkts)
- 요약표: `captures/ap_summary_clean.csv`

## 관찰
- Beacon에서 SSID/BSSID/채널 확인
- 숨김 SSID 일부 존재 (`<MISSING>`)
- Probe Request에서 단말이 찾는 SSID 관찰(anytime2u_2G)

## 사용 명령어
\`\`\`bash
sudo tcpdump -i wlx00c0cab766e5 -w first_capture.pcap
tshark -r first_capture.pcap -Y "wlan.fc.type_subtype==8" -T fields \
  -e wlan.bssid -e wlan.ssid -e radiotap.channel.freq -E header=y -E separator=, > ap_summary.csv
\`\`\`
