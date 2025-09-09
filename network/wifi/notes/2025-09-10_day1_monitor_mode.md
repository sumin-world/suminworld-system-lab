# 📝 Day 1: 모니터 모드 실습 기록 (2025-09-10)

> **Wi-Fi 패킷 캡처 첫 실습**  
> ALFA AWUS036ACM을 사용한 모니터 모드 전환 및 802.11 프레임 분석

---

## 🔧 1. 장비 인식 확인

**하드웨어**: ALFA AWUS036ACM (MediaTek MT7612U) USB 무선랜카드

```bash
# USB 장치 확인
lsusb
# → MediaTek MT7612U 장치 표시됨

# 커널 드라이버 로딩 확인  
dmesg | grep mt76
# → mt76x2u 드라이버 정상 로드됨
```

**결과**: Linux에서 별도 드라이버 설치 없이 바로 인식 성공

---

## 📡 2. 모니터 모드 전환

```bash
# 인터페이스 비활성화
sudo ip link set wlx00c0cab766e5 down

# 모니터 모드로 변경
sudo iw dev wlx00c0cab766e5 set type monitor

# 인터페이스 활성화
sudo ip link set wlx00c0cab766e5 up

# 모드 확인
iwconfig
```

**결과**: `Mode:Monitor` 출력 확인

**✅ 성과**: 주변 무선 패킷 캡처 준비 완료

---

## 📊 3. 패킷 캡처

```bash
# 실시간 패킷 캡처
sudo tcpdump -i wlx00c0cab766e5 -w first_capture.pcap
```

**캡처 결과**:
- **총 320개 프레임** 수집
- **프레임 유형**: Beacon, Probe Request/Response
- **제약사항**: GUI Wireshark 실행 실패 (VM 환경 한계)
- **해결책**: CLI `tshark`로 분석 진행

---

## 🔍 4. 패킷 분석

### 프레임 유형별 분석

| 프레임 유형 | 설명 | 분석 명령어 |
|------------|------|------------|
| **Beacon** | AP가 자신을 알리는 광고 신호 | `tshark -r first_capture.pcap -Y "wlan.fc.type_subtype==8"` |
| **Probe Request** | 단말이 특정 SSID를 탐색 | `tshark -r first_capture.pcap -Y "wlan.fc.type_subtype==4"` |
| **Probe Response** | AP가 Probe Request에 응답 | `tshark -r first_capture.pcap -Y "wlan.fc.type_subtype==5"` |

### 추가 처리
- **SSID 변환**: 일부 SSID가 hex로 표시되어 `xxd` 도구로 ASCII 변환 시도
- **필드 추출**: BSSID, SSID, 주파수 정보 구조적 추출

---

## 📋 5. CSV 데이터 정리

```bash
# Beacon 프레임에서 AP 정보 추출
tshark -r first_capture.pcap -Y "wlan.fc.type_subtype==8" \
  -T fields -e wlan.bssid -e wlan.ssid -e radiotap.channel.freq \
  > ap_summary.csv
```

**생성 파일**: `ap_summary.csv` (AP 요약 정보)

**발견된 한계점**:
- SSID가 hex 형태로 출력되어 가독성 부족
- 보안 필드(`wlan.rsn`, `wlan.wfa.ie.wpa`) 일부 추출 불가

---

## 🛡️ 6. 민감정보 마스킹

### 보안 처리 스크립트
```python
# sanitize.py 작성
# - BSSID 뒷부분 마스킹: xx:xx:xx:**:**:**
# - SSID 일부 마스킹: 앞 4글자 + ****
```

### 최종 결과 (`ap_summary_safe.csv`)
```csv
bssid_masked,ssid_masked,freq_mhz,channel
08:5d:dd:**:**:**,olle_****,2412,1
12:f4:5e:**:**:**,<MISSING>,2412,1
38:f4:5e:**:**:**,SK_B_****,2412,1
```

**✅ 성과**: 민감 데이터 노출 없이 결과 공유 가능

---

## 📂 7. Git 저장소 정리

### 디렉토리 구조
```
suminworld-system-lab/network/wifi/
├── README.md
├── .gitignore
├── captures/
│   ├── ap_summary_safe.csv      # 마스킹된 결과
│   └── sanitize.py              # 마스킹 스크립트
└── notes/
    └── 2025-09-10_day1_monitor_mode.md
```

### 보안 설정
- `.gitignore`에 원본 `.pcap`, `ap_summary.csv` 추가
- 안전한 파일만 커밋: `*_safe.csv`, `sanitize.py`, 문서류

---

## 결과

- 하드웨어: 무선랜카드 정상 인식 및 모니터 모드 전환
- 캡처: 첫 Wi-Fi 패킷 캡처 및 분석 성공  
- Beacon/Probe 프레임 동작 원리 파악
- 보안: 민감정보 마스킹 자동화 구현

---

## 🚀 향후 계획

| 단계 | 목표 | 상세 내용 |
|-----|------|---------|
| **Day 2** | 특정 채널 고정 후 AP별 트래픽 분석 | 채널별 상세 분석, 트래픽 패턴 관찰 |
| **Day 3** | Deauth 실습 (자체 네트워크에서만) | 연결 해제 공격 메커니즘 학습 |
| **Day 4** | WPA 핸드셰이크 캡처 | 암호화 통신 과정 분석 |

---

## 📖 학습 노트

### 핵심 개념
- **모니터 모드**: 모든 무선 트래픽을 수동적으로 관찰하는 모드
- **Beacon 프레임**: AP가 주기적으로 전송하는 존재 알림 신호
- **Probe 메커니즘**: 클라이언트와 AP 간의 발견 과정

### 기술적 인사이트  
- MT7612U 칩셋의 Linux 호환성
- tshark의 CLI 분석 능력이 생각보다 강력
- 민감정보 처리의 중요성 재확인