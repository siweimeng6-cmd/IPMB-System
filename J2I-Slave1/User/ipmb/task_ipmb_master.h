#ifndef __TASK_IPMB_MASTER_H
#define __TASK_IPMB_MASTER_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ipmb_protocol.h"

/* =================== UART -> Master 共享请求 =================== */
/* UART4 中断在解析到 hex 裸帧后写入下面这 3 个变量,
 * 主机任务在每次循环开始检查 pending 标志.            */

#define IPMB_REQ_BUF_SIZE   128    /* 需容纳 OEM 模块信息帧 (~116 字节) */
typedef struct {
    volatile uint8_t  pending;                    /* 1=有请求待处理 */
    volatile uint8_t  len;                        /* 帧有效长度 */
    uint8_t           buf[IPMB_REQ_BUF_SIZE];     /* 帧内容 (含 cs) */
} IPMB_Master_Request_t;

extern IPMB_Master_Request_t g_ipmb_req;

/* =================== Master 任务 =================== */
extern TaskHandle_t IPMB_Master_TaskHandle;

void IPMB_Master_Task(void* parameter);

#endif /* __TASK_IPMB_MASTER_H */


