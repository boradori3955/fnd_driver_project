# RPi 5 FND Driver

Raspberry Pi 5에서 74HC595 시프트 레지스터를 이용해 4자리 FND(7-Segment Display)를 제어하는 Linux 커널 드라이버입니다.

---

## 하드웨어 구성

### 사용 부품
- Raspberry Pi 5
- 4자리 FND
- 74HC595 시프트 레지스터

### GPIO 핀 연결

| 신호 | GPIO 번호 | 기능 |
|------|-----------|------|
| DATA | GPIO 17 | 시프트 레지스터 데이터 입력 |
| CLOCK | GPIO 27 | 시프트 레지스터 클럭 |
| LATCH | GPIO 22 | 시프트 레지스터 래치 |
| COM1 | GPIO 5 | FND 1번째 자리 (최상위, 가장 왼쪽) |
| COM2 | GPIO 6 | FND 2번째 자리 |
| COM3 | GPIO 13 | FND 3번째 자리 |
| COM4 | GPIO 26 | FND 4번째 자리 (최하위, 가장 오른쪽) |

> GPIO 번호는 모두 `CHIP_START(571)` 기반 오프셋 값입니다.  
> 핀 번호 확인: `cat /sys/kernel/debug/gpio`

---

## 프로젝트 구조

```
fnd_driver_project
├── fnd_driver.c   # 커널 드라이버 소스
├── Makefile       # 커널 모듈 빌드 설정
└── test.c         # 유저스페이스 테스트 프로그램
```

---

## 빌드 및 설치

### 빌드 환경 준비

```bash
sudo apt update
sudo apt install raspberrypi-kernel-headers build-essential
```

### 빌드

```bash
make
```

### 모듈 로드

```bash
sudo insmod fnd_driver.ko
```

모듈이 로드되면 `/dev/fnd_driver` 디바이스 파일이 자동으로 생성됩니다.

### 모듈 언로드

```bash
sudo rmmod fnd_driver
```

---

## 테스트 프로그램

4자리 숫자를 표시하거나, `clear` 명령으로 전체 소등할 수 있는 테스트 프로그램이 포함되어 있습니다.

### 빌드

```bash
gcc -o test test.c
```

### 실행

```bash
# 숫자 표시
sudo ./test 1234
```

```bash
# 전체 소등
sudo ./test clear
```

4개의 자리를 빠르게 순차적으로 갱신하여 `1234`를 표시합니다. `clear`를 전달하면 드라이버에 소등 명령을 한 번 전송하고 종료합니다.

### 쓰기 명령 형식

```
echo "<자릿수> <숫자>" > /dev/fnd_driver
```

| 파라미터 | 범위 | 설명 |
|----------|------|------|
| 자릿수 | 1 ~ 4 | 표시할 자리 (1: 최하위(가장 오른쪽), 4: 최상위(가장 왼쪽)) |
| 숫자 | 0 ~ 9 | 표시할 숫자 |

```
echo "clear" > /dev/fnd_driver
```

예시:
```bash
# 3번째 자리에 숫자 5 표시
echo "3 5" > /dev/fnd_driver

# FND 초기화
echo "clear" > /dev/fnd_driver
```

---

## 모듈 파라미터

로드 시 GPIO 핀 번호를 변경할 수 있습니다.

```bash
sudo insmod fnd_driver.ko dataPin=588 clockPin=598 latchPin=593
```

| 파라미터 | 기본값 | 설명 |
|----------|--------|------|
| `dataPin` | 588 (GPIO 17) | 데이터 핀 |
| `clockPin` | 598 (GPIO 27) | 클럭 핀 |
| `latchPin` | 593 (GPIO 22) | 래치 핀 |
| `comPins` | 576,577,584,597 | COM 핀 배열 (4개) |

> 파라미터 확인: `cat /sys/module/fnd_driver/parameters/`

---

## 커널 로그 확인

```bash
dmesg | grep "FND Driver"
```

---

## 라이선스

GPL v2
