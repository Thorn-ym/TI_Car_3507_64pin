/*
 * Application-level feature selection.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define APP_DISPLAY_MODE_PROBLEM_SELECT 0U
#define APP_DISPLAY_MODE_BLUETOOTH_RX   1U

#ifndef APP_DISPLAY_MODE
#define APP_DISPLAY_MODE APP_DISPLAY_MODE_BLUETOOTH_RX
#endif

#if ((APP_DISPLAY_MODE != APP_DISPLAY_MODE_PROBLEM_SELECT) && \
     (APP_DISPLAY_MODE != APP_DISPLAY_MODE_BLUETOOTH_RX))
#error "APP_DISPLAY_MODE must select the problem menu or Bluetooth RX display"
#endif

#endif /* APP_CONFIG_H */