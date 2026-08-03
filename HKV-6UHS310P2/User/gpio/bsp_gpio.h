
#ifndef __BSP_GPIO_H
#define	__BSP_GPIO_H

#include "bsp_init.h"
#define MCU_CONTROL_POWER              1

/**********************************PA***********************************************/
// PA0-WKUP - SELF_RST (自复位)
#define SELF_RST_GPIO_PORT                      GPIOA
#define SELF_RST_GPIO_PIN                       GPIO_Pin_0

// PA1 - ALERT_JX 告警灯控制输出信号  输出低电平电量等  输出高电平熄灭灯
#define ALERT_JX_GPIO_PORT                       GPIOA
#define ALERT_JX_GPIO_PIN                        GPIO_Pin_1

// PA2 - IPMB_A_EN  NCA9511 使能 (IPMB-A 总线 buffer), 高电平使能
#define IPMB_A_EN_GPIO_PORT                       GPIOA
#define IPMB_A_EN_GPIO_PIN                        GPIO_Pin_2

// PA3 - IPMB_B_EN  NCA9511 使能 (IPMB-B 总线 buffer), 高电平使能
#define IPMB_B_EN_GPIO_PORT                       GPIOA
#define IPMB_B_EN_GPIO_PIN                        GPIO_Pin_3

// PA4 - PWR_CTL    
#define PWR_CTL_GPIO_PORT                   GPIOA
#define PWR_CTL_GPIO_PIN                    GPIO_Pin_4

// PA5 - PWR_ON 软关机POWER_OFF为MCU发送给CPU的软关机标志信号，为脉冲信号，低电平有效，默认高电平上拉
#define PWR_ON_GPIO_PORT                    GPIOA
#define PWR_ON_GPIO_PIN                     GPIO_Pin_5

// PA6 - ADC1 (P12V，系数0.1) 电压采集
#define ADC1_GPIO_PORT                GPIOA
#define ADC1_GPIO_PIN                  GPIO_Pin_6

// PA7 - ADC2 (P5V，系数0.2) 电压采集
#define ADC2_GPIO_PORT                 GPIOA
#define ADC2_GPIO_PIN                  GPIO_Pin_7

// PA8 - GA0  
#define GA0_GPIO_PORT                     GPIOA
#define GA0_GPIO_PIN                      GPIO_Pin_8

// PA9 - GA1
#define GA1_GPIO_PORT                 GPIOA
#define GA1_GPIO_PIN                  GPIO_Pin_9

// PA10 - GA2 
#define GA2_GPIO_PORT                    GPIOA
#define GA2_GPIO_PIN                     GPIO_Pin_10

// PA11 - GA3 
#define GA3_GPIO_PORT                   GPIOA
#define GA3_GPIO_PIN                    GPIO_Pin_11

// PA12 - BMC_CTRL_SYSRST 
#define BMC_CTRL_SYSRST_GPIO_PORT                   GPIOA
#define BMC_CTRL_SYSRST_GPIO_PIN                    GPIO_Pin_12

// PA13 - SWDIO
#define SWDIO_GPIO_PORT                         GPIOA
#define SWDIO_GPIO_PIN                          GPIO_Pin_13

// PA14 - SWCLK
#define SWCLK_GPIO_PORT                         GPIOA
#define SWCLK_GPIO_PIN                          GPIO_Pin_14

// PA15 - LED_BMC_STA 状态灯--闪烁频率2hz
#define LED_BMC_STA_GPIO_PORT                        GPIOA
#define LED_BMC_STA_GPIO_PIN                         GPIO_Pin_15
/******************************************PB*****************************************/
// PB0 - ADC3 (P3V3，系数0.3) 电压采集
#define ADC3_GPIO_PORT                          GPIOB
#define ADC3_GPIO_PIN                           GPIO_Pin_0

// PB1 - ADC4 (DDR5_VDD_P1V，系数1) 电压采集
#define ADC4_GPIO_PORT                          GPIOB
#define ADC4_GPIO_PIN                           GPIO_Pin_1

// PB2 - BOOT1
#define BOOT1_GPIO_PORT                         GPIOB
#define BOOT1_GPIO_PIN                          GPIO_Pin_2

//// PB3 - BMC_CPU_CRU_I2C_SCL 单片机模拟IIC  链接CPU可读取CPU温度
//#define BMC_CPU_CRU_I2C_SCL_GPIO_PORT                       GPIOB
//#define BMC_CPU_CRU_I2C_SCL_GPIO_PIN                        GPIO_Pin_3

//// PB4 - BMC_CPU_CRU_I2C_SD 单片机模拟IIC  链接CPU可读取CPU温度
//#define BMC_CPU_CRU_I2C_SD_GPIO_PORT                       GPIOB
//#define BMC_CPU_CRU_I2C_SD_GPIO_PIN                     GPIO_Pin_4

//// PB5 - PAYLD_SYS_IN 默认低电平  PAYLD_IN:开关机状态输入信号 此信号为低电平时，表示主板上电
////                                                           此信号为高电平时，表示主板下电
//#define PAYLD_SYS_IN_GPIO_PORT                       GPIOB
//#define PAYLD_SYS_IN_GPIO_PIN                        GPIO_Pin_5

// PB6 - BMC_IPMBA_SCL 
#define BMC_IPMBA_SCL_GPIO_PORT                     GPIOB
#define BMC_IPMBA_SCL_GPIO_PIN                      GPIO_Pin_6

// PB7 - BMC_IPMBA_SDA 
#define BMC_IPMBA_SDA_GPIO_PORT                     GPIOB
#define BMC_IPMBA_SDA_GPIO_PIN                      GPIO_Pin_7

// PB8 - BMC_M_I2C_SCL (采集温度)参考以往的设计
#define BMC_M_I2C_SCL_GPIO_PORT                        GPIOB
#define BMC_M_I2C_SCL_GPIO_PIN                         GPIO_Pin_8

// PB9 - BMC_M_I2C_SDA (采集温度)参考以往的设计
#define BMC_M_I2C_SDA_GPIO_PORT                        GPIOB
#define BMC_M_I2C_SDA_GPIO_PIN                         GPIO_Pin_9

// PB10 - BMC_IPMBB_SCL 
#define BMC_IPMBB_SCL_GPIO_PORT                     GPIOB
#define BMC_IPMBB_SCL_GPIO_PIN                      GPIO_Pin_10

// PB11 - BMC_IPMBB_SDA 
#define BMC_IPMBB_SDA_GPIO_PORT                     GPIOB
#define BMC_IPMBB_SDA_GPIO_PIN                      GPIO_Pin_11

// PB12 -
#define PB12_GPIO_PORT             GPIOB
#define PB12_GPIO_PIN              GPIO_Pin_12

// PB13 - 
#define PB13_GPIO_PORT          GPIOB
#define PB13_GPIO_PIN           GPIO_Pin_13

// PB14 - 
#define PB14_GPIO_PORT             GPIOB
#define PB14_GPIO_PIN              GPIO_Pin_14

// PB15 - 
#define PB15_GPIO_PORT                 GPIOB
#define PB15_GPIO_PIN                  GPIO_Pin_15

/******************************************PC*****************************************/
// PC0 - ADC5 (CPU0_VDD_CORE   系数1) 电压采集
#define ADC5_GPIO_PORT                          GPIOC
#define ADC5_GPIO_PIN                           GPIO_Pin_0

//// PC1 - ADC6 (VDD_GPU  系数1)电压采集
//#define ADC6_GPIO_PORT                    GPIOC
//#define ADC6_GPIO_PIN                     GPIO_Pin_1

// PC2 - 
#define PC2_GPIO_PORT                 GPIOC
#define PC2_GPIO_PIN                  GPIO_Pin_2

// PC3 - 
#define PC3_GPIO_PORT                 GPIOC
#define PC3_GPIO_PIN                  GPIO_Pin_3

// PC4 - 
#define LED_BMC_RESERVE_GPIO_PORT                    GPIOC
#define LED_BMC_RESERVE_GPIO_PIN                     GPIO_Pin_4

// PC5 - VP0_SYSRESET#   
#define VP0_SYSRESET_GPIO_PORT                       GPIOC
#define VP0_SYSRESET_GPIO_PIN                        GPIO_Pin_5

// PC6 - GA4  
#define GA4_GPIO_PORT                           GPIOC
#define GA4_GPIO_PIN                            GPIO_Pin_6

// PC7 - GAP
#define GAP_GPIO_PORT                           GPIOC
#define GAP_GPIO_PIN                            GPIO_Pin_7
    
// PC8 - BMC_FAN_PWM1
#define PC8_GPIO_PORT                  GPIOC
#define PC8_GPIO_PIN                   GPIO_Pin_8

// PC9 - 
#define PC9_GPIO_PORT                     GPIOC
#define PC9_GPIO_PIN                      GPIO_Pin_9

// PC10 - DEBUG_TX (DEBUG调试串口)
#define DEBUG_TX_GPIO_PORT                      GPIOC
#define DEBUG_TX_GPIO_PIN                       GPIO_Pin_10

// PC11 - DEBUG_RX
#define DEBUG_RX_GPIO_PORT                      GPIOC
#define DEBUG_RX_GPIO_PIN                       GPIO_Pin_11

// PC12 - Uart5 TX 
#define Uart5_TX_GPIO_PORT                     GPIOC
#define Uart5_TX_GPIO_PIN                      GPIO_Pin_12

//// PC13-TAMPER-RTC
//#define RTC_TAMPER_GPIO_PORT                    GPIOC
//#define RTC_TAMPER_GPIO_PIN                     GPIO_Pin_13

//// PC14-OSC32_IN
//#define OSC32_IN_GPIO_PORT                      GPIOC
//#define OSC32_IN_GPIO_PIN                       GPIO_Pin_14

//// PC15-OSC32_OUT
//#define OSC32_OUT_GPIO_PORT                     GPIOC
//#define OSC32_OUT_GPIO_PIN                      GPIO_Pin_15

// PD2 - Uart5-RX   核心卡PCIE复位信号    输入，预留暂不使用
#define Uart5_RX_GPIO_PORT                     GPIOD
#define Uart5_RX_GPIO_PIN                      GPIO_Pin_2

/***********************************input***********************************************/

extern uint8_t line0_low ;

extern xTaskHandle xExitLine0Semaphore; 

extern TaskHandle_t GPIO_Task_Handle;

void bsp_gpio_init(void);
void exit_gpio_init(void);
void GPIO_Task(void* parameter);

#endif 


