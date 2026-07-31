/*
 * MSPM0G3507 line-tracking car application.
 */

#include "ti_msp_dl_config.h"
#include "car_control.h"
/* #include "debug_uart.h" */
#include "competition_tasks.h"
#include "ec11_encoder.h"
#include "line_tracker.h"
#include "mpu6050.h"
#include "odometer.h"
#include "oled_ssd1306.h"
/* #include "problem_menu.h" */

int main(void)
{
    SYSCFG_DL_init();

    LineTracker_Init();
    EC11_Init();
    Car_Init();
    OLED_Init();
    MPU6050_Init();
    /* ProblemMenu_Init(); */
    CompetitionTasks_Init();
    Odometer_Init();
    /* Debug_UART_Init(); */

    g_car.left.pid.kp = 180;
    g_car.left.pid.ki = 0.35f;
    g_car.left.pid.kd = 26;
    g_car.right.pid.kp = 180;
    g_car.right.pid.ki = 0.28f;
    g_car.right.pid.kd = 18;

    g_car.left.invert_motor = 0U;
    g_car.left.invert_encoder = 0U;
    g_car.right.invert_motor = 1U;
    g_car.right.invert_encoder = 1U;


    while (1) {
        MPU6050_Task();
        CompetitionTasks_Service();
        Odometer_Service();
        OLED_Task(g_car.control_tick);
        EC11_Task();
        /* ProblemMenu_Task(); */
        /* UART motor commands stay disabled; KEY2 owns start and stop. */
        /* Debug_UART_Task(); */
        __WFI();
    }
}

void TIMER_CONTROL_INST_IRQHandler(void)
{
    switch (DL_TimerG_getPendingInterrupt(TIMER_CONTROL_INST)) {
        case DL_TIMER_IIDX_ZERO:
            Car_ControlStep();
            CompetitionTasks_ControlStep();
            Odometer_ControlStep();
            break;
        default:
            break;
    }
}
