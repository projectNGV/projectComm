# 🏎️ RC카 자율주행 프로젝트 (TC375 기반)

## 📌 프로젝트 개요
이 프로젝트는 **Infineon AURIX TC375 Lite Kit**을 기반으로 한 RC카 자율주행 시스템입니다.  
초음파 센서와 Bluetooth 통신을 활용하여 **후방 자율 주차**, **전방 추돌 경고**, **수동 조작 주행** 기능을 구현하였습니다.

---

## ⚙️ 주요 기능

| 기능            | 설명                                                                 |
|-----------------|----------------------------------------------------------------------|
| `autopark`      | 초음파 센서를 기반으로 주차 공간을 탐색하고 자동으로 후진 주차 수행       |
| `fcw`           | 전방 거리 감지를 통해 장애물과의 충돌을 방지하는 경고 시스템               |
| `control`       | Bluetooth를 통한 전진, 회전, 정지 등 수동 제어 기능                        |
| `motor`         | PWM 제어를 통해 모터의 속도 및 방향을 제어                                 |
| `bluetooth`     | UART 통신 기반 Bluetooth 입력 수신 처리                                   |
| `buzzer`, `led` | 주행 상태 또는 경고 상황을 사용자에게 알리는 출력 장치 제어                |

---


## 📁 프로젝트 구조
```
projectWon/
  ├── ASW/
  │    ├── auth/ # 암호키
  │    ├── autopark/ # 자율주차
  │    ├── drivecontrol/ # 원격주행
  │    ├── emergencystop/ # 긴급제동
  │    └── fsm # 상태전이
  └── BSW/
     ├── MCAL/ # Microcontroller Abstraction Layer
     └── Service
```


---

## 🔌 사용된 하드웨어

| 부품             | 설명                                       |
|------------------|--------------------------------------------|
| MCU              | Infineon AURIX TC375 Lite Kit             |
| 초음파 센서       | HC-SR04 (거리 측정용)        |
| Bluetooth 모듈   | HC-06 (UART 통신 기반)                    |
| 모터 드라이버     | L298N                       |
| 액추에이터        | 부저, LED                                   |
| 섀시 및 구동계    | 4WD RC카             |

---

## 🛠️ 빌드 방법

1. **IDE 설치**
   - Infineon AURIX Development Studio (ADS) 사용
2. **프로젝트 임포트**
   - `projectWon` 폴더를 ADS에서 Import
3. **Build Config**
   - `Debug` 또는 `Release` 선택 후 빌드
4. **보드에 업로드**
   - USB 연결 후 Flash 버튼 클릭

---

## ▶️ 실행 방법

- **전원 공급**  
  보드 및 모터 드라이버에 전원 공급

- **Bluetooth 연결**  
  PC에서 tools/drivecontrol_bluetooth/controller.py 실행  
  키 입력으로 제어 가능

- **Autopark 모드 진입**  
  'p' 입력 시 자동 주차 FSM 실행

- **AEB 모드 진입**
  't' 입력 시 AEB 모드 실행
---

## 📎 참고 사항

- 실시간성 확보를 위해 GPT12 타이머 인터럽트를 활용
- 거리 필터링에 이동 평균(Moving Average) 사용
- 상태 기반 FSM으로 시나리오 동작 구현
- 자율 주차 기능은 초음파 센서 기반으로 공간을 인식하며 후진 정렬까지 수행

---

## 👨‍💻 개발 환경

- MCU: Infineon AURIX TC375
- IDE: AURIX Development Studio
- 언어: C
- 디버깅: On-Chip Debugger via USB
- 디버깅툴: UDE Starterkit

---

## 📝 라이선스

이 프로젝트는 교육 목적으로 사용되며 별도의 상업적 이용을 허가하지 않습니다.
