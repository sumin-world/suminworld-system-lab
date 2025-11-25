#!/bin/bash
# ClamAV 실시간 모니터링 스크립트

WATCH_DIR="$1"
LOG_FILE="$HOME/clamav_realtime.log"

# 사용법 확인
if [ -z "$WATCH_DIR" ]; then
    echo "Usage: $0 <directory_to_watch>"
    echo "Example: $0 ~/test_monitor"
    exit 1
fi

echo "=== ClamAV Real-time Monitoring ==="
echo "Watching: $WATCH_DIR"
echo "Log file: $LOG_FILE"
echo "Press Ctrl+C to stop"
echo ""

# inotifywait로 파일 변경 감지 → ClamAV 검사
inotifywait -m -r -e create,close_write "$WATCH_DIR" --format '%w%f' | while read FILE
do
    TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
    echo "[$TIMESTAMP] Detected: $FILE" | tee -a "$LOG_FILE"
    
    # ClamAV 검사 실행
    SCAN_RESULT=$(sudo clamdscan --fdpass --no-summary "$FILE" 2>&1)
    
    if echo "$SCAN_RESULT" | grep -q "FOUND"; then
        echo "[$TIMESTAMP] ⚠️  MALWARE DETECTED: $FILE" | tee -a "$LOG_FILE"
        echo "$SCAN_RESULT" | tee -a "$LOG_FILE"
        
        # 감염된 파일 격리
        QUARANTINE_DIR="$HOME/clamav_quarantine"
        mkdir -p "$QUARANTINE_DIR"
        mv "$FILE" "$QUARANTINE_DIR/" 2>/dev/null
        echo "[$TIMESTAMP] 🔒 Quarantined to: $QUARANTINE_DIR" | tee -a "$LOG_FILE"
    else
        echo "[$TIMESTAMP] ✅ Clean: $FILE" | tee -a "$LOG_FILE"
    fi
done
