# heaterRC — ZT3L 무시동히터 리모컨

ZT3L(TLSR8258)로 무시동히터 컨트롤러의 버튼(ON / OFF / UP / DOWN) 라인을 직접 눌러주고, 같은 라인을 평소에는 입력으로 감시해서 원래 리모컨이나 히터 본체 버튼으로 조작한 것도 상태(전원, 레벨 0~9)에 반영하는 Zigbee 펌웨어입니다. PWM5CH 수정본의 Zigbee 처리(조인 재시도, reporting, OTA, LED, IR, OLED)를 그대로 가져왔습니다.

## 버튼 라인 배선

| 라인 | ZT3L 핀 | GPIO | 히터 버튼 |
|---|---|---|---|
| OUT1 | Pin 7  | PC2 | ON (단일 전원 버튼 모드: POWER) |
| OUT2 | Pin 6  | PC3 | OFF (단일 전원 버튼 모드: 미사용) |
| OUT3 | Pin 5  | PD2 | UP |
| OUT4 | Pin 14 | PB5 | DOWN |
| OUT5 | Pin 13 | PB4 | 예비 (입력 대기만) |

각 핀은 양방향입니다.

- **대기:** 입력 + 내부 풀다운(100k). 라인이 HIGH가 되면(외부에서 버튼이 눌림) 30ms 디바운스 후 1회 입력으로 처리합니다.
- **누름:** 그 핀만 HIGH 출력 → 펄스 시간이 지나면 출력을 끄고 다시 입력(Hi-Z). LOW를 직접 출력하지 않으므로 원래 리모컨과 동시에 눌려도 충돌하지 않습니다.
- 직접 누른 라인은 누르는 동안과 뗀 뒤 80ms 동안 감시에서 빠지므로 자기 펄스를 외부 입력으로 다시 세지 않습니다.

**배선 조건 (중요)**

- 히터 쪽 버튼 라인이 **눌림 = HIGH, 평소 = LOW**인 신호선이어야 합니다. 반대(눌림 = LOW)면 `HEATER_ACTIVE_LEVEL 0`으로 바꾸면 됩니다(풀업 대기, LOW로 누름).
- **HIGH 전압은 3.3V 이하**여야 합니다. TLSR8258은 5V 입력을 견디지 못합니다. 5V 라인이면 분압이나 레벨 변환이 필요합니다.
- ZT3L GND와 히터 컨트롤러 GND를 연결해야 합니다.
- 핀과 라인 사이에 직렬 저항 470Ω~1kΩ을 넣으면 실수로 단락돼도 핀이 보호됩니다.
- 옵토커플러나 MOSFET을 거치면 라인을 다시 읽을 수 없으므로, 이 방식은 **GPIO를 라인에 직접(저항 경유) 연결**할 때만 동작합니다. PWM5CH 보드의 MOSFET 출력단은 쓸 수 없습니다.

## 설정 — `app_cfg.h`

| 항목 | 기본값 | 설명 |
|---|---|---|
| `HEATER_ACTIVE_LEVEL` | 1 | 1 = 눌림 HIGH (풀다운 대기), 0 = 눌림 LOW (풀업 대기) |
| `HEATER_IN_MONITOR_ENABLE` | 1 | 외부 버튼 입력 감시 |
| `HEATER_IN_DEBOUNCE_MS` | 30 | 외부 입력 디바운스 |
| `HEATER_IN_BLANK_MS` | 80 | 직접 누른 뒤 그 라인 감시 제외 시간 |
| `HEATER_LEVEL_MIN` / `MAX` | 0 / 9 | 레벨 범위 |
| `HEATER_POWER_SINGLE_BUTTON` | 0 | 1 = 전원 버튼 하나로 켜고 끄는 히터 (OUT1 만 사용) |
| `HEATER_UPDN_REQUIRE_ON` | 1 | 히터가 꺼져 있으면 UP/DOWN 무시 |
| `HEATER_POWER_PULSE_MS_DEF` | 500 | ON/OFF 누름 시간 기본값 |
| `HEATER_BTN_PULSE_MS_DEF` | 200 | UP/DOWN 누름 시간 기본값 |
| `HEATER_GAP_MS_DEF` | 300 | 연속 명령 사이 간격 기본값 |
| `HEATER_QUEUE_SIZE` | 8 | 대기 명령 최대 개수 |
| `HEATER_OLED_ENABLE` | 1 | SSD1306 OLED 사용 |
| `HEATER_BUTTON_TOGGLE_ENABLE` | 1 | BUTTON1 짧게 누름 = 히터 토글 |

펄스 시간 세 가지는 Zigbee 속성으로 노출되어 HA에서 바로 바꿀 수 있고 NV에 저장됩니다. 위 값은 첫 부팅 기본값입니다.

## Zigbee

| EP | 클러스터 | 동작 |
|---|---|---|
| 1 | OnOff | On → ON 버튼, Off → OFF 버튼, Toggle → 현재 상태 반대 |
| 1 | OnOff 0xF000 / 0xF001 / 0xF002 | powerPulseMs / btnPulseMs / gapMs (uint16, R/W) |
| 1 | OnOff 0xF003 | 레벨 0~9 (uint8, 읽기·리포트) |
| 1 | OnOff 0xF004 | 마지막 입력 0=ON 1=OFF 2=UP 3=DOWN 0xFF=없음 (enum8, 읽기·리포트) |
| 1 | OnOff 0xF005 | 마지막 입력 출처 0=이 장치 1=외부 (enum8, 읽기·리포트) |
| 2 | OnOff | On → UP 1회, 펄스가 끝나면 자동 Off |
| 3 | OnOff | On → DOWN 1회, 펄스가 끝나면 자동 Off |

- 모델 ID `ZT3L-HEATER`, 제조사 `DODO`. OTA 이미지 타입은 PWM5CH와 다르게 `0x48`로 지정해서 다른 장치용 OTA 파일이 올라가지 않습니다.
- 상태는 이 장치가 누른 것과 라인에서 감지한 외부 입력을 모두 반영합니다. 다만 히터 본체 상태(실제 온도, 에러, 자동 꺼짐)를 읽는 것은 아니므로, 버튼 외의 이유로 히터 상태가 바뀌면 어긋날 수 있습니다.
- 레벨: UP +1, DOWN −1, 0~9에서 멈춤. 범위 끝에서도 버튼은 눌러 보내므로 실제 히터와 어긋났을 때 끝까지 누르면 다시 맞춰집니다.
- 히터가 꺼져 있을 때의 UP/DOWN은 레벨을 바꾸지 않습니다(`HEATER_UPDN_REQUIRE_ON`).
- 전원·레벨은 마지막 변경 2초 뒤에 NV에 저장합니다.
- 부팅 시 저장된 상태로 버튼을 자동으로 누르지 않습니다.
- 명령이 거부되면(히터 꺼짐 상태의 UP/DOWN, 대기열 가득) ZCL 실패 응답을 보냅니다.

## 로컬 조작

- **BUTTON1**: 짧게(1초 미만) = 히터 토글, 5초 = 공장 초기화
- **IR (NEC)**: `0x45` ON, `0x47` OFF, `0x40` UP, `0x43` DOWN (`ir_recv_cmd.c` 상단에서 변경). UP/DOWN은 길게 누르면 펄스가 끝날 때마다 한 단계씩 추가되고, 손을 떼면 바로 멈춥니다.
- **OLED**: Zigbee 상태, 전원, 레벨, 마지막 입력(RC = 이 장치 / EXT = 외부), 대기 명령 수, 최근 IR

## SDK 쪽 작업

- `irq_handler.c` IR 연결은 `ir_irq_patch.txt` 참고 (PWM5CH와 동일, `IR_RECV_ENABLE` 분기)
- 이 폴더를 SDK `apps/` 아래에 새 프로젝트로 추가하고, PWM5CH 빌드 설정을 복사해서 소스 목록만 바꾸면 됩니다.

## Z2M

`ZT3L-HEATER.js`를 `data/external_converters/`에 넣고 Z2M을 재시작합니다. HA에는 히터 전원 스위치, 레벨(0~9)·마지막 입력·입력 출처 센서, UP·DOWN 버튼, 펄스 시간 숫자 3개(설정 항목)가 생깁니다. 펌웨어 업데이트 후 Z2M에서 **Reconfigure**를 한 번 눌러야 레벨·마지막 입력 리포트가 설정됩니다.
