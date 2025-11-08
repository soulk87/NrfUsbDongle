#include "qmk.h"
#include "qmk/port/port.h"


static void cliQmk(cli_args_t *args);
static void idle_task(void);

static bool is_suspended = false;
// 스크롤 모드 상태
static bool scroll_mode = false;



bool qmkInit(void)
{
  eeprom_init();
  via_hid_init();

  keyboard_setup();
  keyboard_init();

  #ifdef RF_DONGLE_MODE_ENABLE
  key_protocol_init();
  #endif

  is_suspended = usbIsSuspended();

  // logPrintf("[  ] qmkInit()\n");
  // logPrintf("     MATRIX_ROWS : %d\n", MATRIX_ROWS);
  // logPrintf("     MATRIX_COLS : %d\n", MATRIX_COLS);
  // logPrintf("     DEBOUNCE    : %d\n", DEBOUNCE);

  cliAdd("qmk", cliQmk);

  return true;
}

void qmkUpdate(void)
{
  #ifdef RF_DONGLE_MODE_ENABLE
  key_protocol_update();
  #endif
  keyboard_task();
  eeprom_task();
  idle_task();
}

void keyboard_post_init_user(void)
{
#ifdef KILL_SWITCH_ENABLE
  kill_switch_init();
#endif
#ifdef KKUK_ENABLE
  kkuk_init();
#endif
}

bool process_record_user(uint16_t keycode, keyrecord_t *record)
{
#ifdef KILL_SWITCH_ENABLE
  kill_switch_process(keycode, record);
#endif
#ifdef KKUK_ENABLE
  kkuk_process(keycode, record);
#endif

    // KC_LCTL을 스크롤 모드 토글 키로 사용 (원하는 키로 변경 가능)
    // 예: KC_LCTL, KC_RCTL, KC_LGUI, MO(1) 등
    if (keycode == KC_MS_BTN8) {
        scroll_mode = record->event.pressed;
        return true;
    }
    
    return true;
    
  return true;
}

void idle_task(void)
{
  bool is_suspended_cur;

  is_suspended_cur = usbIsSuspended();
  if (is_suspended_cur != is_suspended)
  {
    if (is_suspended_cur)
    {
      suspend_power_down();
      logPrintf("Enter Suspend\n");
    }
    else
    {
      logPrintf("Exit Suspend\n");
      suspend_wakeup_init();
    }

    is_suspended = is_suspended_cur;
  }

#ifdef KKUK_ENABLE
  kkuk_idle();
#endif
}
/**
 * @brief 마우스 리포트 처리 (스크롤 모드 적용)
 */
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) 
{
    if (scroll_mode) {
        // 마우스 움직임을 스크롤로 변환
        // x, y 값을 h(horizontal), v(vertical) 스크롤로 매핑
        mouse_report.h = mouse_report.x/10;
        mouse_report.v = -mouse_report.y/10;  // Y축 반전 (마우스 위로 = 스크롤 아래로)
        
        // 마우스 커서 움직임 제거
        mouse_report.x = 0;
        mouse_report.y = 0;
    }
    
    return mouse_report;
}

/**
 * @brief 레이어 상태 변경 콜백
 * 레이어가 변경될 때마다 LVGL 디스플레이를 업데이트합니다.
 */
layer_state_t layer_state_set_user(layer_state_t state)
{
    uint8_t layer = get_highest_layer(state);
    apLvglUpdateLayer(layer);

    return state;   
}

void cliQmk(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 2 && args->isStr(0, "clear") && args->isStr(1, "eeprom"))
  {
    eeconfig_init();
    cliPrintf("Clearing EEPROM\n");
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("qmk info\n");
    cliPrintf("qmk clear eeprom\n");
  }
}