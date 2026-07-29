/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)



#define CPUCLK_FREQ                                                     32000000



/* Defines for PWM_0 */
#define PWM_0_INST                                                         TIMA0
#define PWM_0_INST_IRQHandler                                   TIMA0_IRQHandler
#define PWM_0_INST_INT_IRQN                                     (TIMA0_INT_IRQn)
#define PWM_0_INST_CLK_FREQ                                             32000000
/* GPIO defines for channel 0 */
#define GPIO_PWM_0_C0_PORT                                                 GPIOB
#define GPIO_PWM_0_C0_PIN                                          DL_GPIO_PIN_8
#define GPIO_PWM_0_C0_IOMUX                                      (IOMUX_PINCM25)
#define GPIO_PWM_0_C0_IOMUX_FUNC                     IOMUX_PINCM25_PF_TIMA0_CCP0
#define GPIO_PWM_0_C0_IDX                                    DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 */
#define GPIO_PWM_0_C1_PORT                                                 GPIOB
#define GPIO_PWM_0_C1_PIN                                          DL_GPIO_PIN_9
#define GPIO_PWM_0_C1_IOMUX                                      (IOMUX_PINCM26)
#define GPIO_PWM_0_C1_IOMUX_FUNC                     IOMUX_PINCM26_PF_TIMA0_CCP1
#define GPIO_PWM_0_C1_IDX                                    DL_TIMER_CC_1_INDEX




/* Defines for QEI_0 */
#define QEI_0_INST                                                         TIMG8
#define QEI_0_INST_IRQHandler                                   TIMG8_IRQHandler
#define QEI_0_INST_INT_IRQN                                     (TIMG8_INT_IRQn)
/* Pin configuration defines for QEI_0 PHA Pin */
#define GPIO_QEI_0_PHA_PORT                                                GPIOB
#define GPIO_QEI_0_PHA_PIN                                         DL_GPIO_PIN_6
#define GPIO_QEI_0_PHA_IOMUX                                     (IOMUX_PINCM23)
#define GPIO_QEI_0_PHA_IOMUX_FUNC                    IOMUX_PINCM23_PF_TIMG8_CCP0
/* Pin configuration defines for QEI_0 PHB Pin */
#define GPIO_QEI_0_PHB_PORT                                                GPIOB
#define GPIO_QEI_0_PHB_PIN                                         DL_GPIO_PIN_7
#define GPIO_QEI_0_PHB_IOMUX                                     (IOMUX_PINCM24)
#define GPIO_QEI_0_PHB_IOMUX_FUNC                    IOMUX_PINCM24_PF_TIMG8_CCP1


/* Defines for TIMER_CONTROL */
#define TIMER_CONTROL_INST                                              (TIMG12)
#define TIMER_CONTROL_INST_IRQHandler                          TIMG12_IRQHandler
#define TIMER_CONTROL_INST_INT_IRQN                            (TIMG12_INT_IRQn)
#define TIMER_CONTROL_INST_LOAD_VALUE                                  (319999U)




/* Defines for I2C_MPU */
#define I2C_MPU_INST                                                        I2C1
#define I2C_MPU_INST_IRQHandler                                  I2C1_IRQHandler
#define I2C_MPU_INST_INT_IRQN                                      I2C1_INT_IRQn
#define I2C_MPU_BUS_SPEED_HZ                                              100000
#define GPIO_I2C_MPU_SDA_PORT                                              GPIOB
#define GPIO_I2C_MPU_SDA_PIN                                       DL_GPIO_PIN_3
#define GPIO_I2C_MPU_IOMUX_SDA                                   (IOMUX_PINCM16)
#define GPIO_I2C_MPU_IOMUX_SDA_FUNC                    IOMUX_PINCM16_PF_I2C1_SDA
#define GPIO_I2C_MPU_SCL_PORT                                              GPIOB
#define GPIO_I2C_MPU_SCL_PIN                                       DL_GPIO_PIN_2
#define GPIO_I2C_MPU_IOMUX_SCL                                   (IOMUX_PINCM15)
#define GPIO_I2C_MPU_IOMUX_SCL_FUNC                    IOMUX_PINCM15_PF_I2C1_SCL


/* Defines for UART_DEBUG */
#define UART_DEBUG_INST                                                    UART1
#define UART_DEBUG_INST_FREQUENCY                                       32000000
#define UART_DEBUG_INST_IRQHandler                              UART1_IRQHandler
#define UART_DEBUG_INST_INT_IRQN                                  UART1_INT_IRQn
#define GPIO_UART_DEBUG_RX_PORT                                            GPIOA
#define GPIO_UART_DEBUG_TX_PORT                                            GPIOA
#define GPIO_UART_DEBUG_RX_PIN                                     DL_GPIO_PIN_9
#define GPIO_UART_DEBUG_TX_PIN                                     DL_GPIO_PIN_8
#define GPIO_UART_DEBUG_IOMUX_RX                                 (IOMUX_PINCM20)
#define GPIO_UART_DEBUG_IOMUX_TX                                 (IOMUX_PINCM19)
#define GPIO_UART_DEBUG_IOMUX_RX_FUNC                  IOMUX_PINCM20_PF_UART1_RX
#define GPIO_UART_DEBUG_IOMUX_TX_FUNC                  IOMUX_PINCM19_PF_UART1_TX
#define UART_DEBUG_BAUD_RATE                                              (9600)
#define UART_DEBUG_IBRD_32_MHZ_9600_BAUD                                   (208)
#define UART_DEBUG_FBRD_32_MHZ_9600_BAUD                                    (21)





/* Port definition for Pin Group GPIO_OLED */
#define GPIO_OLED_PORT                                                   (GPIOA)

/* Defines for SCL: GPIOA.0 with pinCMx 1 on package pin 33 */
#define GPIO_OLED_SCL_PIN                                        (DL_GPIO_PIN_0)
#define GPIO_OLED_SCL_IOMUX                                       (IOMUX_PINCM1)
/* Defines for SDA: GPIOA.1 with pinCMx 2 on package pin 34 */
#define GPIO_OLED_SDA_PIN                                        (DL_GPIO_PIN_1)
#define GPIO_OLED_SDA_IOMUX                                       (IOMUX_PINCM2)
/* Port definition for Pin Group GPIO_KEY */
#define GPIO_KEY_PORT                                                    (GPIOB)

/* Defines for START: GPIOB.23 with pinCMx 51 on package pin 22 */
#define GPIO_KEY_START_PIN                                      (DL_GPIO_PIN_23)
#define GPIO_KEY_START_IOMUX                                     (IOMUX_PINCM51)
/* Port definition for Pin Group GPIO_MOTOR */
#define GPIO_MOTOR_PORT                                                  (GPIOA)

/* Defines for AIN1: GPIOA.7 with pinCMx 14 on package pin 49 */
#define GPIO_MOTOR_AIN1_PIN                                      (DL_GPIO_PIN_7)
#define GPIO_MOTOR_AIN1_IOMUX                                    (IOMUX_PINCM14)
/* Defines for AIN2: GPIOA.28 with pinCMx 3 on package pin 35 */
#define GPIO_MOTOR_AIN2_PIN                                     (DL_GPIO_PIN_28)
#define GPIO_MOTOR_AIN2_IOMUX                                     (IOMUX_PINCM3)
/* Defines for STBY: GPIOA.22 with pinCMx 47 on package pin 18 */
#define GPIO_MOTOR_STBY_PIN                                     (DL_GPIO_PIN_22)
#define GPIO_MOTOR_STBY_IOMUX                                    (IOMUX_PINCM47)
/* Defines for BIN1: GPIOA.31 with pinCMx 6 on package pin 39 */
#define GPIO_MOTOR_BIN1_PIN                                     (DL_GPIO_PIN_31)
#define GPIO_MOTOR_BIN1_IOMUX                                     (IOMUX_PINCM6)
/* Defines for BIN2: GPIOA.21 with pinCMx 46 on package pin 17 */
#define GPIO_MOTOR_BIN2_PIN                                     (DL_GPIO_PIN_21)
#define GPIO_MOTOR_BIN2_IOMUX                                    (IOMUX_PINCM46)
/* Port definition for Pin Group GPIO_LINE */
#define GPIO_LINE_PORT                                                   (GPIOB)

/* Defines for S1: GPIOB.12 with pinCMx 29 on package pin 64 */
#define GPIO_LINE_S1_PIN                                        (DL_GPIO_PIN_12)
#define GPIO_LINE_S1_IOMUX                                       (IOMUX_PINCM29)
/* Defines for S2: GPIOB.13 with pinCMx 30 on package pin 1 */
#define GPIO_LINE_S2_PIN                                        (DL_GPIO_PIN_13)
#define GPIO_LINE_S2_IOMUX                                       (IOMUX_PINCM30)
/* Defines for S3: GPIOB.14 with pinCMx 31 on package pin 2 */
#define GPIO_LINE_S3_PIN                                        (DL_GPIO_PIN_14)
#define GPIO_LINE_S3_IOMUX                                       (IOMUX_PINCM31)
/* Defines for S4: GPIOB.15 with pinCMx 32 on package pin 3 */
#define GPIO_LINE_S4_PIN                                        (DL_GPIO_PIN_15)
#define GPIO_LINE_S4_IOMUX                                       (IOMUX_PINCM32)
/* Defines for S5: GPIOB.16 with pinCMx 33 on package pin 4 */
#define GPIO_LINE_S5_PIN                                        (DL_GPIO_PIN_16)
#define GPIO_LINE_S5_IOMUX                                       (IOMUX_PINCM33)
/* Defines for S6: GPIOB.17 with pinCMx 43 on package pin 14 */
#define GPIO_LINE_S6_PIN                                        (DL_GPIO_PIN_17)
#define GPIO_LINE_S6_IOMUX                                       (IOMUX_PINCM43)
/* Defines for S7: GPIOB.18 with pinCMx 44 on package pin 15 */
#define GPIO_LINE_S7_PIN                                        (DL_GPIO_PIN_18)
#define GPIO_LINE_S7_IOMUX                                       (IOMUX_PINCM44)
/* Port definition for Pin Group GPIO_ENCODER */
#define GPIO_ENCODER_PORT                                                (GPIOB)

/* Defines for E2A: GPIOB.10 with pinCMx 27 on package pin 62 */
// pins affected by this interrupt request:["E2A","E2B"]
#define GPIO_ENCODER_INT_IRQN                                   (GPIOB_INT_IRQn)
#define GPIO_ENCODER_INT_IIDX                   (DL_INTERRUPT_GROUP1_IIDX_GPIOB)
#define GPIO_ENCODER_E2A_IIDX                               (DL_GPIO_IIDX_DIO10)
#define GPIO_ENCODER_E2A_PIN                                    (DL_GPIO_PIN_10)
#define GPIO_ENCODER_E2A_IOMUX                                   (IOMUX_PINCM27)
/* Defines for E2B: GPIOB.11 with pinCMx 28 on package pin 63 */
#define GPIO_ENCODER_E2B_IIDX                               (DL_GPIO_IIDX_DIO11)
#define GPIO_ENCODER_E2B_PIN                                    (DL_GPIO_PIN_11)
#define GPIO_ENCODER_E2B_IOMUX                                   (IOMUX_PINCM28)
/* Port definition for Pin Group GPIO_EC11 */
#define GPIO_EC11_PORT                                                   (GPIOA)

/* Defines for A: GPIOA.14 with pinCMx 36 on package pin 7 */
#define GPIO_EC11_A_PIN                                         (DL_GPIO_PIN_14)
#define GPIO_EC11_A_IOMUX                                        (IOMUX_PINCM36)
/* Defines for B: GPIOA.15 with pinCMx 37 on package pin 8 */
#define GPIO_EC11_B_PIN                                         (DL_GPIO_PIN_15)
#define GPIO_EC11_B_IOMUX                                        (IOMUX_PINCM37)
/* Defines for SW: GPIOA.16 with pinCMx 38 on package pin 9 */
#define GPIO_EC11_SW_PIN                                        (DL_GPIO_PIN_16)
#define GPIO_EC11_SW_IOMUX                                       (IOMUX_PINCM38)


/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_PWM_0_init(void);
void SYSCFG_DL_QEI_0_init(void);
void SYSCFG_DL_TIMER_CONTROL_init(void);
void SYSCFG_DL_I2C_MPU_init(void);
void SYSCFG_DL_UART_DEBUG_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
