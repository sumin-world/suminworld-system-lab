# Wi-Fi Security Lab

> **Wi-Fi 보안 연구를 위한 실습 프로젝트**  
> ALFA AWUS036ACM USB 무선랜카드를 사용한 **모니터 모드 + 패킷 캡처** 실습  
> 전체 프로젝트: [`suminworld-system-lab/network/wifi`](https://github.com/sumin-world/suminworld-system-lab/tree/main/network/wifi)

---

## 실습 환경

| 구성요소 | 세부사항 |
|---------|---------|
| **Host OS** | macOS (UTM 가상머신) |
| **Guest OS** | Ubuntu 24.04 LTS |
| **무선랜카드** | ALFA AWUS036ACM |
| **칩셋** | MediaTek MT7612U (mt76x2u) |
| **분석도구** | tcpdump, tshark, awk |

---

## 실습 과정

### 모니터 모드 활성화

```bash
# 인터페이스 비활성화
sudo ip link set wlx00c0cab766e5 down

# 모니터 모드로 변경
sudo iw dev wlx00c0cab766e5 set type monitor

# 인터페이스 활성화
sudo ip link set wlx00c0cab766e5 up

# 모드 확인
iwconfig  # → Mode:Monitor 표시 확인
```

### 802.11 프레임 캡처

```bash
# 실시간 패킷 캡처
sudo tcpdump -i wlx00c0cab766e5 -w first_capture.pcap
```

**캡처된 프레임 유형:**
- **Beacon 프레임**: AP 광고 신호
- **Probe Request/Response**: 클라이언트 스캔 신호

### 패킷 분석 및 데이터 추출

#### Beacon 프레임 분석
```bash
# AP 정보 추출 (BSSID, SSID, 주파수)
tshark -r first_capture.pcap -Y "wlan.fc.type_subtype==8" \
  -T fields -e wlan.bssid -e wlan.ssid -e radiotap.channel.freq | head
```

#### Probe Request 분석
```bash
# 클라이언트 스캔 활동 분석
tshark -r first_capture.pcap -Y "wlan.fc.type_subtype==4" \
  -T fields -e wlan.sa -e wlan.da -e wlan.ssid \
| awk '{
    if (NF >= 3 && $3 != "") {
        gsub(/\\x([0-9a-fA-F]{2})/, "", $3)  # 특수문자 제거
        print $1 " → " $2 " [" $3 "]"
    }
}' | head
```

---

## 데이터 보안 처리

### 민감정보 마스킹 스크립트

```bash
# 자동 데이터 sanitization
./scripts/sanitize.py
```

**출력 예시:**
```csv
bssid_masked,ssid_masked,freq_mhz,channel
08:5d:dd:**:**:**,olle_****,2412,1
12:f4:5e:**:**:**,<MISSING>,2412,1
38:f4:5e:**:**:**,SK_B_****,2412,1
```

---

## 프로젝트 구조

```
network/wifi/
├── README.md
├── .gitignore                         # 민감 데이터 제외
├── captures/
│   ├── ap_summary_safe.csv              # 마스킹된 AP 정보
│   └── sanitize.py                      # 데이터 마스킹 스크립트
└── notes/
    └── 2025-09-10_day1_monitor_mode.md  # 상세 실습 노트
```

---

## 결과

- ✅ **모니터 모드 성공**: MT7612U 칩셋 정상 동작 확인
- ✅ **프레임 캡처**: Beacon, Probe 프레임 정상 수집
- ✅ **데이터 분석**: tshark를 활용한 구조적 분석
- ✅ **보안 처리**: 민감정보 마스킹 자동화

---

## 향후 계획

| 단계 | 목표 | 예상 일정 |
|-----|------|---------|
| **Day 2** | 특정 채널 고정 + AP별 트래픽 수집 | 진행 예정 |
| **Day 3** | Deauthentication 테스트 (격리 환경) | 계획 중 |
| **Day 4** | WPA/WPA2 핸드셰이크 캡처 | 계획 중 |

---

## 주의사항

> **이 프로젝트는 교육 및 연구 목적으로만 사용됩니다.**  
> 모든 실습은 본인 소유 네트워크 또는 명시적 허가를 받은 환경에서만 진행합니다.

---

## 참고 자료

- [IEEE 802.11 Standard](https://standards.ieee.org/ieee/802.11/7028/)
- [Wireshark 802.11 Analysis](https://wiki.wireshark.org/802.11)
- [Linux Wireless Documentation](https://wireless.wiki.kernel.org/)