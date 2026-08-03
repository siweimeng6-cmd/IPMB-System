#include "bsp_usart.h"
#include <stdarg.h>
#include <string.h>
#include "bsp_init.h"
#include "bsp_i2c.h"                   /* IPMB_A_AS_MASTER (决定 #if 条件编译) */
#include ".\gpio\bsp_gpio.h"
#include "bsp_adc.h"
#include "bsp_mo_i2c.h"
#include ".\timer\bsp_pwm.h"
#include ".\ipmb\ipmb_protocol.h"      /* IPMB_Calc_Checksum */
#include ".\ipmb\task_ipmb_master.h"   /* g_ipmb_req (仅在 IPMB_A_AS_MASTER 时被引用) */

// �ⲿ��������
extern float fan1_duty,fan2_duty,fan3_duty,fan4_duty;
extern float fan1_freq,fan2_freq,fan3_freq,fan4_freq;
extern stPRINTF_BUF_t stPrintf_Buf;

// UART4���ջ�����
#define UART4_RX_BUF_SIZE 64
uint8_t UART4_RxBuf[UART4_RX_BUF_SIZE];
uint8_t UART4_RxCnt = 0;

// UART5���ջ����������ڽ���CPU���ݣ�
#define UART5_RX_BUF_SIZE 64
uint8_t UART5_RxBuf[UART5_RX_BUF_SIZE];
uint8_t UART5_RxCnt = 0;

uint8_t poweron_flag = 0;// poweron�����־λ
uint8_t poweroff_flag = 0;// poweroff�����־λ
uint8_t powerrst_flag = 0;// powerrst�����־λ

volatile uint32_t g_uart4_rx_byte_cnt = 0;   /* 诊断: UART4 收到的字节总数 */
volatile uint32_t g_uart4_isr_cnt    = 0;   /* 诊断: UART4 ISR 调用次数 */
volatile uint32_t g_uart4_line_cnt   = 0;   /* 诊断: UART4 收到完整行次数 */

void USART_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // �򿪴���GPIO��ʱ��
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    // �򿪴��������ʱ��
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);

    // ��USART Tx��GPIO����Ϊ���츴��ģʽ
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // ��USART Rx��GPIO����Ϊ��������ģʽ
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // ���ô��ڵĹ�������
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(UART4, &USART_InitStructure);

    // ʹ�ܴ���
    USART_Cmd(UART4, ENABLE);
 /* ********************************************************************************************* */   
    // ��ʼ��UART5�����Ŀ�ͨѶ��
    // ��GPIOC��GPIOD��ʱ��
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);
    // ��UART5�����ʱ��
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART5, ENABLE);
    
    // PC12 = UART5 TX�����츴�������
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    
    // PD2 = UART5 RX���������룩
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    
    // ����UART5�Ĺ�������
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(UART5, &USART_InitStructure);
    
    // ʹ��UART5
    USART_Cmd(UART5, ENABLE);
    
    // ʹ��UART5�����жϣ����ڽ���CPU���ݣ�
    USART_ITConfig(UART5, USART_IT_RXNE, ENABLE);
    
    // ʹ��USART4�����ж�
    USART_ITConfig(UART4, USART_IT_RXNE, ENABLE);
    
    // ����NVIC
    NVIC_InitTypeDef NVIC_InitStructure;
    
    // ����UART5 NVIC
    NVIC_InitStructure.NVIC_IRQChannel = UART5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x03;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    // ����USART4 NVIC
    NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x02;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

}

int fputc(int ch, FILE *f)
{
    // �ȴ����ͻ�����Ϊ��
    while (USART_GetFlagStatus(UART4, USART_FLAG_TXE) == RESET);
    
    // ��������
    USART_SendData(UART4, (uint8_t)ch);
    
    return (ch);
}


void Usart_SendByte(USART_TypeDef * pUSARTx, uint8_t ch)
{
    USART_SendData(pUSARTx, ch);
        
    while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);	
}

void Usart_SendArray(USART_TypeDef * pUSARTx, uint8_t *array, uint16_t num)
{
    uint8_t i;
	
    for(i=0; i<num; i++)
    {
        Usart_SendByte(pUSARTx, array[i]);	
    }
    while(USART_GetFlagStatus(pUSARTx, USART_FLAG_TC) == RESET);
}


void Usart_SendString(USART_TypeDef * pUSARTx, char *str)
{
    unsigned int k=0;
    do 
    {
        Usart_SendByte(pUSARTx, *(str + k));
        k++;
    } while(*(str + k) != '\0');
  
    while(USART_GetFlagStatus(pUSARTx, USART_FLAG_TC) == RESET)
    {}
}

void Usart_SendHalfWord(USART_TypeDef * pUSARTx, uint16_t ch)
{
    uint8_t temp_h, temp_l;
	

    temp_h = (ch & 0XFF00) >> 8;

    temp_l = ch & 0XFF;
	

    USART_SendData(pUSARTx, temp_h);	
    while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);	
    USART_SendData(pUSARTx, temp_l);	
    while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);	
}

/**
 * @brief  USART5�������ݺ���
 * @param  data: Ҫ���͵�����
 * @param  len: ���ݳ���
 * @retval ��
 */
void USART5_SendData(uint8_t *data, uint16_t len)
{
    uint16_t i;
    for(i = 0; i < len; i++)
    {
        USART_SendData(UART5, data[i]);
        while (USART_GetFlagStatus(UART5, USART_FLAG_TXE) == RESET);
    }
    // �ȴ��������
    while(USART_GetFlagStatus(UART5, USART_FLAG_TC) == RESET);
}


void UART4_IRQHandler(void)
{
    uint8_t res;

    if (USART_GetITStatus(UART4, USART_IT_RXNE) != RESET)
    {
        g_uart4_isr_cnt++;                                  /* 诊断 */
        res = USART_ReceiveData(UART4);
        g_uart4_rx_byte_cnt++;                               /* 诊断 */

        // put into ring buffer
        if (UART4_RxCnt < UART4_RX_BUF_SIZE - 1)
        {
            UART4_RxBuf[UART4_RxCnt] = res;
            UART4_RxCnt++;
            UART4_RxBuf[UART4_RxCnt] = '\0';
        }

        // end of line: parse
        if (res == '\r' || res == '\n')
        {
            g_uart4_line_cnt++;                              /* 诊断 */
            int line_is_ipmb = 0;
#if IPMB_A_AS_MASTER
            /* 诊断: 打印收到的原始内容 */
            printf("[UART4] raw[%u]=\"%.*s\" hex=", UART4_RxCnt, UART4_RxCnt, UART4_RxBuf);
            { uint8_t _j; for(_j=0;_j<UART4_RxCnt;_j++) printf("%02X ", UART4_RxBuf[_j]); }
            printf("\r\n");
            // ---- (2) IPMB hex raw frame  (only in master mode) ----
            // 整行扫描: 只要包含 >= 6 字节 hex (含空白分隔), 就当作 IPMB 命令.
            // 这种宽松策略可以容忍用户使用任意触发符 (>, #, », 等) 或没有触发符.
            // 与原 poweron/poweroff 文本命令共存: 当 hex 解析成功时不再走文本分支.
            {
                uint8_t  tmp[IPMB_REQ_BUF_SIZE];
                uint8_t  tlen = 0;
                uint8_t  i    = 0;
                uint8_t  hi   = 0xFF;
                int      ok   = 1;

                while (i < UART4_RxCnt && tlen < IPMB_REQ_BUF_SIZE)
                {
                    char c = (char)UART4_RxBuf[i++];
                    if (c == ' ' || c == '\t' || c == ',' || c == '\r' || c == '\n') continue;
                    uint8_t nib;
                    if      (c >= '0' && c <= '9') nib = (uint8_t)(c - '0');
                    else if (c >= 'a' && c <= 'f') nib = (uint8_t)(c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') nib = (uint8_t)(c - 'A' + 10);
                    else { ok = 0; break; }

                    if (hi == 0xFF) {
                        hi = nib;
                    } else {
                        tmp[tlen++] = (uint8_t)((hi << 4) | nib);
                        hi = 0xFF;
                    }
                }
                if (hi != 0xFF) ok = 0;     // odd nibble

                printf("[UART4] parse: ok=%d tlen=%u\r\n", ok, tlen);
                /* 如果整行只有合法 hex 且 >= 6 字节, 当作 IPMB 帧 */
                if (ok && tlen >= 6 && tlen <= IPMB_MAX_FRAME_LEN)
                {
                    // Auto-fill cs1 and cs2 if the user omitted them.
                    // 输入 hex 字节数说明:
                    //   6 字节: rsSA netFn rqSA rqSeq cmd data   → 自动补 cs1/cs2, data 填 0
                    //   7 字节: rsSA netFn rqSA rqSeq cmd cs2     → 自动补 cs1; 若第 7 字节
                    //            是 data 则会被 cs2 覆盖! 要发 1 字节数据请发 ≥8 字节
                    //  ≥8 字节: 完整帧                              → 仅重算 cs1/cs2
                    uint8_t frame[IPMB_MAX_FRAME_LEN];
                    uint8_t flen;
                    if (tlen == 6) {
                        memcpy(frame, tmp, 6);
                        frame[6] = 0x00;           // 1 byte data placeholder
                        frame[7] = 0x00;           // temp cs2 placeholder
                        flen = 8;
                    } else if (tlen == 7) {
                        /* 7 字节: 视为完整 0-data 帧 (含 cs2).
                         * 若第 7 字节本意是 data, 需改为发 8 字节 (含 cs2). */
                        memcpy(frame, tmp, 7);
                        flen = 7;
                    } else {
                        memcpy(frame, tmp, tlen);
                        flen = tlen;
                    }
                    if (flen >= 3) {
                        frame[2] = IPMB_Calc_Checksum(frame, 2);
                    }
                    if (flen >= 4) {
                        frame[flen - 1] = IPMB_Calc_Checksum(&frame[3], (uint8_t)(flen - 4));
                    }
                    g_ipmb_req.len = flen;
                    memcpy((void*)g_ipmb_req.buf, frame, flen);
                    g_ipmb_req.pending = 1;
                    printf("[UART4] IPMB frame queued, len=%u\r\n", flen);
                    line_is_ipmb = 1;
                } else if (ok) {
                    /* ok 但 tlen < 6: 用户发的是不完整的 hex, 忽略 */
                    line_is_ipmb = 0;
                } else {
                    /* 包含非 hex 字符, 走文本命令分支 */
                    line_is_ipmb = 0;
                }
            }
#endif
            if (!line_is_ipmb)
            {
                // ---- (1) Legacy text commands: poweron / poweroff ----
                if (strstr((const char*)UART4_RxBuf, "poweron") != NULL)
                {
                    poweron_flag = 1;
                }
                if (strstr((const char*)UART4_RxBuf, "poweroff") != NULL)
                {
                    poweroff_flag = 1;
                }
                if (strstr((const char*)UART4_RxBuf, "powerrst") != NULL)
                {
                    powerrst_flag = 1;
                }
            }

            // clear buffer
            UART4_RxCnt = 0;
            memset(UART4_RxBuf, 0, sizeof(UART4_RxBuf));
        }

        USART_ClearITPendingBit(UART4, USART_IT_RXNE);
    }
}



/**
 * @brief  UART5�жϴ�������������CPU���ݣ�
 * @param  ��
 * @retval ��
 */
void UART5_IRQHandler(void)
{
    if(USART_GetITStatus(UART5, USART_IT_RXNE) != RESET)
    {
        uint8_t ch = USART_ReceiveData(UART5);
        
        // �������յ�������
        if(ch == '?')
        {
            // ���ɽ�����Ϣ
            char health_info[512] = {0};
            char temp_buf[128] = {0};
            
            // ��ջ�����
            memset(stPrintf_Buf.buf, 0, sizeof(stPrintf_Buf.buf));
            
            // 1. �����ѹֵ
            ADC_Calculation();
            
            // 2. �ɼ��¶�
            Board_ADDR90_temp();
            Board_ADDR92_temp();
            
            // 3. ��ȡPWMռ�ձ�
            pwm_func();
            
            // ����������Ϣ
            sprintf(health_info, "\r\n=================== ϵͳ״̬ ===================\r\n");
            strcat(health_info, stPrintf_Buf.buf);           
            strcat(health_info, "================================================\r\n");           
            // ͨ��USART5���ͽ�����Ϣ
            USART5_SendData((uint8_t *)health_info, strlen(health_info));
        }
        
        USART_ClearITPendingBit(UART5, USART_IT_RXNE);
    }
}







