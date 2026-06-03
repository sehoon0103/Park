## 📌 Ultrasonic Parking Detection System

초음파 센서를 이용해 물체와의 거리를 측정하고,  
기준 위치와 비교하여 차량 존재 여부를 판단하는 임베디드 시스템입니다.

**회로 설계 → PCB 제작 → 펌웨어 구현 → 시스템 통합까지 수행한  
MCU 기반 임베디드 시스템 전주기 개발 프로젝트입니다.**

---

## 📌 Project Objective

이 프로젝트의 목적은 단순한 주차 감지 기능 구현이 아니라,

**Timer, Interrupt, UART 통신, 프로토콜 설계 등  
MCU 저수준 제어 기반의 임베디드 시스템 구현 역량을 확보하는 것**입니다.

---

## ⚙️ System Overview

초음파 센서를 이용해 거리 측정을 수행하고,  
기준 거리와 비교하여 차량 존재 여부를 판단합니다.

### 동작 흐름
1. 초음파 센서 TRIG 신호 출력  
2. ECHO 펄스 폭 측정  
3. 거리 계산  
4. 기준 거리와 비교  
5. 차량 존재 여부 판단  
6. LED 상태 표시  

---

## 🔧 Hardware Development

이 프로젝트에서는 센서 동작 확인 수준을 넘어  
**회로 설계부터 실제 제작까지 직접 수행했습니다.**

### Circuit Design
- 초음파 센서 인터페이스 회로 설계
- MCU 핀 매핑 설계
- LED 및 스위치 회로 구성

### PCB Design
- CAD 기반 PCB 레이아웃 설계
- footprint 설정 및 배선 설계

### BOM 작성
- 부품 리스트 및 규격 정리

### Board Assembly
- PCB 제작 및 납땜
- 실제 동작 테스트 및 디버깅

📁 `PARKcad` 폴더 포함 자료
- 회로 설계 PDF
- BOM 문서
- footprint 및 제작 자료

---

## 💻 Firmware Development

펌웨어는 **ATmega328P** (AVR 8-bit MCU, 16 MHz) 기반에서 C 언어로 구현했습니다.

- 개발 환경: Microchip Studio (AVR-GCC, `-mmcu=atmega328p -Os`)
- 타겟 보드: ATmega328P DFP 1.7.374

---

### 1️⃣ Timer 기반 시간 시스템 구현
- Timer0 Overflow Interrupt 사용
- `millis()`, `micros()`, `delay()` 함수 직접 구현  

👉 라이브러리에 의존하지 않고 시간 관리 기능을 직접 구현

---

### 2️⃣ 초음파 센서 거리 측정 (Interrupt 기반)
- TRIG 신호로 초음파 송신  
- ECHO 핀을 외부 인터럽트(INT0)로 처리  
- Rising/Falling edge를 이용한 펄스폭 측정  

👉 폴링이 아닌 인터럽트 기반 측정 구조 구현

---

### 3️⃣ UART + RS-485 통신 구현
- UART 인터럽트 기반 송수신  
- Ring Buffer 구조 적용  
- RS-485 TR 핀 제어로 송수신 방향 전환  

👉 비동기 통신 및 데이터 충돌 방지 구조 구현

---

### 4️⃣ 커스텀 프로토콜 설계
- ASCII 기반 데이터 프레임 설계  
- Master-Slave 구조 통신 구현  

---

## 🖥️ 실시간 주차현황 모니터링 (C#)

RS-485 멀티드롭 네트워크를 통해 수집된 데이터를 시각화하는 모니터링 소프트웨어입니다.

- **개발 환경:** C# WinForms (.NET)
- **통신:** `System.IO.Ports.SerialPort` (9600 baud, 8N1)
- **기능:** 25개 주차 슬롯 실시간 현황 표시 (빈 자리 / 주차중)
- **구조:** 슬레이브 1~25 순차 폴링 → 응답 파싱 → UI 갱신
- **구현 파일:** `PARKMicrochipstudio/MainForm.cs`

> 원본 소스는 유실되어 펌웨어 프로토콜 명세를 기반으로 재구현했습니다.
