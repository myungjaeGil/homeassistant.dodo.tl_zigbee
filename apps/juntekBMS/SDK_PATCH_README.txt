[SDK 수정 필요] platform/services/b85m/irq_handler.c
=====================================================

porterDoorLock / juntekBMS 공통 패턴:
app_cfg.h 의 IR_RECV_ENABLE 값으로 IR 기능 조건부 컴파일

--- 수정 위치 1: 상단 include ---

변경 전:
    #include "tl_common.h"
    #include "ir_recv.h"

변경 후:
    #include "tl_common.h"
    #if defined(IR_RECV_ENABLE) && IR_RECV_ENABLE
    #include "ir_recv.h"
    #endif

--- 수정 위치 2: gpio_irq 핸들러 내부 ---

변경 전:
        ir_recv_gpio_irq();

변경 후:
    #if defined(IR_RECV_ENABLE) && IR_RECV_ENABLE
        ir_recv_gpio_irq();
    #endif

=====================================================
프로젝트별 app_cfg.h 설정:
  juntekBMS  → IR_RECV_ENABLE 0  (미사용)
  pwm5ch     → IR_RECV_ENABLE 1  (PA0 NEC 수신)
