#include "bsp_i2c.h"


stIIC_SEND_DATA_t i2c1_send_data_struct;
stIIC_RECV_DATA_t i2c1_recv_data_struct;
stIIC_SEND_DATA_t i2c2_send_data_struct;
stIIC_RECV_DATA_t i2c2_recv_data_struct;

/* IPMB 总线速率(control_req 0x10/0x11 可运行期切换,原来是#define常量) */
volatile uint32_t I2C_Speed = 100000;
static volatile uint8_t s_i2c1_speed_pending = 0;
static volatile uint8_t s_i2c2_speed_pending = 0;



/*																						
*********************************************************************************************************
*	�� �� ��: I2C_GPIO_Config
*	����˵��: I2C GPIO�ܽų�ʼ��
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void I2C_GPIO_Config(void)
{
  GPIO_InitTypeDef  GPIO_InitStructure; 
   
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
  RCC_AHB1PeriphClockCmd(COM_I2C1_SCL_GPIO_CLK | COM_I2C1_SDA_GPIO_CLK, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
  RCC_AHB1PeriphClockCmd(COM_I2C2_SCL_GPIO_CLK | COM_I2C2_SDA_GPIO_CLK, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C3, ENABLE);
  RCC_AHB1PeriphClockCmd(COM_I2C3_SCL_GPIO_CLK | COM_I2C3_SDA_GPIO_CLK, ENABLE);
	

  /*!< GPIO configuration */
  GPIO_PinAFConfig(COM_I2C1_SCL_GPIO_PORT, COM_I2C1_SCL_SOURCE, COM_I2C1_SCL_AF);  //Connect PXx to I2C1_SCL
  GPIO_PinAFConfig(COM_I2C1_SDA_GPIO_PORT, COM_I2C1_SDA_SOURCE, COM_I2C1_SDA_AF);  //Connect PXx to I2C1_SDA
	GPIO_PinAFConfig(COM_I2C2_SCL_GPIO_PORT, COM_I2C2_SCL_SOURCE, COM_I2C2_SCL_AF);  //Connect PXx to I2C2_SCL
  GPIO_PinAFConfig(COM_I2C2_SDA_GPIO_PORT, COM_I2C2_SDA_SOURCE, COM_I2C2_SDA_AF);  //Connect PXx to I2C2_SDA
	GPIO_PinAFConfig(COM_I2C3_SCL_GPIO_PORT, COM_I2C3_SCL_SOURCE, COM_I2C3_SCL_AF);  //Connect PXx to I2C2_SCL
  GPIO_PinAFConfig(COM_I2C3_SDA_GPIO_PORT, COM_I2C3_SDA_SOURCE, COM_I2C3_SDA_AF);  //Connect PXx to I2C2_SDA

  
  /*!< Configure I2C1 pins: SCL */   
  GPIO_InitStructure.GPIO_Pin 	= COM_I2C1_SCL_PIN;
  GPIO_InitStructure.GPIO_Mode 	= GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;	
  GPIO_Init(COM_I2C1_SCL_GPIO_PORT, &GPIO_InitStructure);

  /*!< Configure I2C1 pins: SDA */
  GPIO_InitStructure.GPIO_Pin = COM_I2C1_SDA_PIN;
  GPIO_Init(COM_I2C1_SDA_GPIO_PORT, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin 	= COM_I2C2_SCL_PIN;
	GPIO_Init(COM_I2C2_SCL_GPIO_PORT, &GPIO_InitStructure);	
  GPIO_InitStructure.GPIO_Pin = COM_I2C2_SDA_PIN;
  GPIO_Init(COM_I2C2_SDA_GPIO_PORT, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin 	= COM_I2C3_SCL_PIN;
	GPIO_Init(COM_I2C3_SCL_GPIO_PORT, &GPIO_InitStructure);	
  GPIO_InitStructure.GPIO_Pin = COM_I2C3_SDA_PIN;
  GPIO_Init(COM_I2C3_SDA_GPIO_PORT, &GPIO_InitStructure);

 
}


/*																						
*********************************************************************************************************
*	�� �� ��: I2C_Mode_Config
*	����˵��: I2C ����ģʽ����
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void I2C_Mode_Config(void)
{
  I2C_InitTypeDef  I2C_InitStructure;

  /* I2C ���� */
  I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
  I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;		                    /* �ߵ�ƽ�����ȶ����͵�ƽ���ݱ仯 SCL ʱ���ߵ�ռ�ձ� */
  I2C_InitStructure.I2C_Ack = I2C_Ack_Enable ;
  I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;	/* I2C��Ѱַģʽ */
  I2C_InitStructure.I2C_ClockSpeed = I2C_Speed;	                            /* ͨ������ */

  /* I2C1 — 角色由 IPMB_A_ROLE 决定:从机模式(默认)用于本地 IPMI 自测;主机模式用于业务。
   * 主机模式下 OwnAddress1 也必须设成 IPMB 主机地址(0x20):业务上 I2C1 除了发起请求,
   * 还要能被从机(F103)主动 write 回来的响应帧寻址到,不能再用旧的占位地址 COM_I2C1_ADDRESS。 */
  I2C_InitStructure.I2C_OwnAddress1 = IPMB_HOST_I2C_ADDR;
  I2C_Init(COM_I2C1, &I2C_InitStructure);

#if (IPMB_A_ROLE == IPMB_A_SLAVE)
  /* 从机模式:OAR2 也设为 0x10(主控板 IPMB 地址 7bit),双地址匹配 */
  I2C_OwnAddress2Config(COM_I2C1, COM_I2C1_OAR2_ADDRESS);
  I2C_DualAddressCmd(COM_I2C1, ENABLE);
  I2C_GeneralCallCmd(COM_I2C1, DISABLE);
  I2C_Cmd(COM_I2C1, ENABLE);
  /* 从机需要 EVT + ERR + BUF 中断全部打开,接收与响应都依赖 */
  I2C_ITConfig(COM_I2C1, I2C_IT_EVT | I2C_IT_ERR | I2C_IT_BUF, ENABLE);
  I2C_AcknowledgeConfig(COM_I2C1, ENABLE);
#else
  /* 主机模式:双角色并存——既要能发起请求(主机),又要能随时接收从机主动 write
   * 回来的响应帧(从机),EVT/ERR/BUF 中断必须从启动开始就常开,不能只在
   * SendRequest 期间临时打开,否则会错过从机主动推送响应的那一刻。 */
  I2C_Cmd(COM_I2C1, ENABLE);
  I2C_ITConfig(COM_I2C1,I2C_IT_BUF | I2C_IT_EVT | I2C_IT_ERR, ENABLE);
  I2C_AcknowledgeConfig(COM_I2C1, ENABLE);
#endif

  /* I2C2(IPMB-B)始终是主机+从机双角色,同样监听 IPMB 主机地址 0x20 */
  I2C_InitStructure.I2C_OwnAddress1 = IPMB_HOST_I2C_ADDR;
  I2C_Init(COM_I2C2, &I2C_InitStructure);

  I2C_InitStructure.I2C_OwnAddress1 =COM_I2C3_ADDRESS;
  I2C_Init(COM_I2C3, &I2C_InitStructure);

  I2C_Cmd(COM_I2C2, ENABLE);  	                                                /* ʹ�� I2C2 */
  I2C_ITConfig(COM_I2C2,I2C_IT_BUF | I2C_IT_EVT | I2C_IT_ERR, ENABLE);//���ж�
  I2C_AcknowledgeConfig(COM_I2C2, ENABLE);

  I2C_Cmd(COM_I2C3, ENABLE);  	                                                /* ʹ�� I2C2 */
  I2C_ITConfig(COM_I2C3,I2C_IT_BUF | I2C_IT_EVT | I2C_IT_ERR, ENABLE);//���ж�
  I2C_AcknowledgeConfig(COM_I2C3, ENABLE);


}



/*																						
*********************************************************************************************************
*	�� �� ��: i2c_send_nvic
*	����˵��: I2C �жϷ��ͺ��������ж��е���
*	��    �Σ�I2C_TypeDef* I2Cx
*	�� �� ֵ: ��
*********************************************************************************************************
*/

void i2c_send_nvic(I2C_TypeDef* I2Cx,uint8_t slave_addr)
{

	
	 if(I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT))//EV5,SB=1,ͨ���ȶ�ȡSR1�Ĵ����ٽ���ַд��DR�Ĵ������㣨�û��ֲ�ע�ͣ�
	{
		I2C_Send7bitAddress(I2Cx, slave_addr, I2C_Direction_Transmitter);//Send slave address ��ģʽ,���ʹӵ�ַ 
	}
	else if(I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))//EV6,ADDR=1,ͨ���ȶ�ȡSR1�ٶ�ȡSR2�Ĵ��������㣨�û��ֲ�ע�ͣ�
	{
	}
	else if(I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTING))//EV8��TXE=1����λ�Ĵ����գ����ݼĴ����գ���DR��д��DATA1���û��ֲ�ע�ͣ�
	{
		if(i2c1_send_data_struct.send_data_len>0)
		{
			I2C_SendData(I2Cx,i2c1_send_data_struct.send_buf[i2c1_send_data_struct.send_data_cnt++]);
			i2c1_send_data_struct.send_data_len--;
		}
		else
		{
			i2c1_send_data_struct.send_data_cnt=0;
		}
	}
	else if(I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED))//EV8_2����������ֹͣ
	{
		I2C_GenerateSTOP(I2Cx, ENABLE);//����ֹͣ
	}
}
/*																						
*********************************************************************************************************
*	�� �� ��: i2c_recv_data
*	����˵��: I2C�жϽ��գ���ȡ�Ĵ�������
*	��    �Σ�I2C_TypeDef* I2Cx
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void i2c_recv_data(I2C_TypeDef* I2Cx)
{
	if(I2Cx == I2C1)
	{
		/* if reception data register is not empty ,I2C2 will read a data from I2C_DTR */       			
		i2c1_recv_data_struct.recv_buf [i2c1_recv_data_struct.recv_data_len++] = I2C_ReceiveData(I2C1);
		if(i2c1_recv_data_struct.recv_data_len  >= BUFFER_RX_SIZE)
		{
		  i2c1_recv_data_struct.recv_data_len = 0;
		}
	}
	else if(I2Cx == I2C2)
	{ 			
		i2c2_recv_data_struct.recv_buf [i2c2_recv_data_struct.recv_data_len++] = I2C_ReceiveData(I2C2);
		if(i2c2_recv_data_struct.recv_data_len  >= BUFFER_RX_SIZE)
		{
		  i2c2_recv_data_struct.recv_data_len = 0;
		}	
	}
}
/*																						
*********************************************************************************************************
*	�� �� ��: i2cSemaphoreGive
*	����˵��: I2C�жϽ��ն�ֵ����ֵ
*	��    �Σ�I2C_TypeDef* I2Cx
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void i2cSemaphoreGive(I2C_TypeDef* I2Cx)
{
	BaseType_t  xHigherPriorityTaskWoken;
							
	xHigherPriorityTaskWoken = pdFALSE;
	if(I2Cx == I2C1)
	{
		/* 注意:不能在这里清 recv_data_len——被唤醒的任务要等 ISR 返回后才能真正
		 * 运行,此刻清零的话任务读到的永远是 0。缓冲何时清空由调用方(SendRequest)
		 * 在下一次准备接收前自己决定。 */
		xSemaphoreGiveFromISR(I2C1ReceiveSemaphore, &xHigherPriorityTaskWoken);
	}
	else if(I2Cx == I2C2)
	{
		xSemaphoreGiveFromISR(I2C2ReceiveSemaphore, &xHigherPriorityTaskWoken);
	}
			
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);	//���xHigherPriorityTaskWoken = pdTRUE,��ô�˳��жϺ��е���ǰ������ȼ�����ִ�� */
}

/* ============================================================
 * 主机状态机类型与变量前向声明(i2c_recv_callback 中需要引用)
 * ============================================================ */
typedef enum {
	I2C2_M_IDLE = 0,
	I2C2_M_SEND_ADDR_W,
	I2C2_M_SEND_DATA,
	I2C2_M_RESTART,
	I2C2_M_SEND_ADDR_R,
	I2C2_M_RECV_DATA,
	I2C2_M_DONE,
} i2c2_master_state_t;

typedef enum {
	I2C1_M_IDLE = 0,
	I2C1_M_SEND_ADDR_W,
	I2C1_M_SEND_DATA,
	I2C1_M_RESTART,
	I2C1_M_SEND_ADDR_R,
	I2C1_M_RECV_DATA,
	I2C1_M_DONE,
} i2c1_master_state_t;

static volatile i2c1_master_state_t s_i2c1_master_state = I2C1_M_IDLE;
static volatile uint8_t s_i2c1_master_tx_idx = 0;
static volatile uint8_t s_i2c1_master_tx_target_addr = 0;
static volatile uint8_t s_i2c1_master_done = 0;   /* 一次事务(写)完成标志,供任务轮询 */

static volatile i2c2_master_state_t s_i2c2_master_state = I2C2_M_IDLE;
static volatile uint8_t s_i2c2_master_tx_idx = 0;
static volatile uint8_t s_i2c2_master_tx_target_addr = 0;
static volatile uint8_t s_i2c2_master_done = 0;   /* 一次事务(写)完成标志,供任务轮询 */

/* ============================================================
 * 从机接收状态(捕获从机主动 write 回来的响应帧)
 * 只在 s_i2cX_master_state == IDLE 时的地址匹配事件里进入,
 * 与"本机自己发起的主机事务"互不重叠(I2C 总线同一时刻只有一方能发起 START)。
 * ============================================================ */
typedef enum { I2C1_SRX_IDLE = 0, I2C1_SRX_RECEIVING } i2c1_slave_rx_state_t;
static volatile i2c1_slave_rx_state_t s_i2c1_srx_state = I2C1_SRX_IDLE;
static volatile TickType_t s_i2c1_srx_enter_tick = 0;   /* 进入 RECEIVING 的时刻,供总线卡死判定用 */

typedef enum { I2C2_SRX_IDLE = 0, I2C2_SRX_RECEIVING } i2c2_slave_rx_state_t;
static volatile i2c2_slave_rx_state_t s_i2c2_srx_state = I2C2_SRX_IDLE;
static volatile TickType_t s_i2c2_srx_enter_tick = 0;   /* 进入 RECEIVING 的时刻,供总线卡死判定用 */

/*
*********************************************************************************************************
*	函数名: i2c_master_busy_recover
*	功能说明: 总线卡死(BUSY 假死)恢复——物理插拔 I2C 线缆时 SDA 浮空瞬间可能出现
*	          类似起始条件的毛刺,让硬件误判总线一直忙,此后 GenerateSTART 永远
*	          不会真正发起。检测到卡死后做一次 SWRST 软复位并按 I2C_Mode_Config()
*	          同样的参数重新初始化该总线,恢复主机发起+从机被动接收的双角色能力。
*	          仿照从机侧(F103)bsp_i2c_master.c 里 i2c_master_swrst_recover() 的做法。
*	参    数: I2C_TypeDef* I2Cx
*	返 回 值: 无
*********************************************************************************************************
*/
static void i2c_master_busy_recover(I2C_TypeDef* I2Cx)
{
	I2C_InitTypeDef I2C_InitStructure;

	I2C_GenerateSTOP(I2Cx, ENABLE);
	I2C_SoftwareResetCmd(I2Cx, ENABLE);
	I2C_SoftwareResetCmd(I2Cx, DISABLE);

	/* SWRST 会清空 CCR/TRISE/CR2/OAR1 等全部寄存器,必须照 I2C_Mode_Config()
	 * 里同样的参数重新初始化,否则复位完总线还是用不了。 */
	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed = I2C_Speed;
	I2C_InitStructure.I2C_OwnAddress1 = IPMB_HOST_I2C_ADDR;
	I2C_Init(I2Cx, &I2C_InitStructure);
	I2C_Cmd(I2Cx, ENABLE);
	I2C_ITConfig(I2Cx, I2C_IT_BUF | I2C_IT_EVT | I2C_IT_ERR, ENABLE);
	I2C_AcknowledgeConfig(I2Cx, ENABLE);

	if (I2Cx == I2C1)
	{
		s_i2c1_master_state = I2C1_M_IDLE;
		s_i2c1_srx_state = I2C1_SRX_IDLE;
		i2c1_recv_data_struct.recv_data_len = 0;
	}
	else if (I2Cx == I2C2)
	{
		s_i2c2_master_state = I2C2_M_IDLE;
		s_i2c2_srx_state = I2C2_SRX_IDLE;
		i2c2_recv_data_struct.recv_data_len = 0;
	}
}

/*
*********************************************************************************************************
*	函 数 名: Ipmb_SetBusSpeed
*	功能说明: IPMB 总线速率切换(control_req 0x10/0x11)入口——只更新 I2C_Speed 变量+置两路
*	          "待重新init"标志,不在这里直接摸寄存器。真正的 I2C_Init() 重新配置延后到
*	          I2C1_Task/I2C2_Task 各自处理完当前命令、回到循环顶部时,由它们自己调用
*	          Ipmb_I2C1/2_ApplyPendingSpeed() 消费,避免跨任务直接碰同一个外设寄存器的竞态。
*	形    参: speed_hz: 100000 或 400000
*	返 回 值: 无
*********************************************************************************************************
*/
void Ipmb_SetBusSpeed(uint32_t speed_hz)
{
	I2C_Speed = speed_hz;
	s_i2c1_speed_pending = 1;
	s_i2c2_speed_pending = 1;
}

/*
*********************************************************************************************************
*	函 数 名: Ipmb_I2C1_ApplyPendingSpeed / Ipmb_I2C2_ApplyPendingSpeed
*	功能说明: 供 I2C1_Task/I2C2_Task 在每轮循环顶部无条件调用;没有待生效的速率变更时直接
*	          返回(几乎零开销),有则消费标志并复用总线卡死恢复用的 i2c_master_busy_recover()
*	          按当前 I2C_Speed 重新初始化对应这一路外设(只碰传入的这一路,不影响另一路/I2C3)。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void Ipmb_I2C1_ApplyPendingSpeed(void)
{
	if (s_i2c1_speed_pending) {
		s_i2c1_speed_pending = 0;
		i2c_master_busy_recover(I2C1);
		printf("[I2C1] speed reinit committed: %luHz\r\n", (unsigned long)I2C_Speed);
	}
}

void Ipmb_I2C2_ApplyPendingSpeed(void)
{
	if (s_i2c2_speed_pending) {
		s_i2c2_speed_pending = 0;
		i2c_master_busy_recover(I2C2);
		printf("[I2C2] speed reinit committed: %luHz\r\n", (unsigned long)I2C_Speed);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: i2c_recv_callback
*	����˵��: I2C�жϻص�����
*	��    �Σ�I2C_TypeDef* I2Cx
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void i2c_recv_callback(I2C_TypeDef* I2Cx)
{
	__IO uint16_t SR1Register =0;
	__IO uint16_t SR2Register =0;

	/* 先读 SR1 原始值,用于检测主机模式下可能发生的错误标志 */
	SR1Register = I2Cx->SR1;
	SR2Register = I2Cx->SR2;

	/* -------------------------------------------------------
	 * 主机 / 从机通用错误处理:AF / BERR / ARLO
	 * 这些标志如果不及时清除会导致中断反复触发,系统卡死
	 * ------------------------------------------------------- */
		if (SR1Register & I2C_FLAG_AF)
		{
			/* Acknowledge Failure: 从机未应答 --- 清 AF + 发 STOP 释放总线 */
			I2C_ClearFlag(I2Cx, I2C_FLAG_AF);
			I2C_GenerateSTOP(I2Cx, ENABLE);
			if (I2Cx == I2C1)
			{
				s_i2c1_master_state = I2C1_M_IDLE;
				s_i2c1_master_done = 1;   /* 通知 SendRequest 事务已终止 */
				s_i2c1_srx_state = I2C1_SRX_IDLE;
				i2c1_recv_data_struct.recv_data_len = 0;
			}
			else if (I2Cx == I2C2)
			{
				s_i2c2_master_state = I2C2_M_IDLE;
				s_i2c2_master_done = 1;   /* 通知 SendRequest 事务已终止 */
				s_i2c2_srx_state = I2C2_SRX_IDLE;
				i2c2_recv_data_struct.recv_data_len = 0;
			}
		}
		else if (SR1Register & I2C_FLAG_BERR)
		{
			/* Bus Error: 清 BERR + 发 STOP + 重置状态机 */
			I2C_ClearFlag(I2Cx, I2C_FLAG_BERR);
			I2C_GenerateSTOP(I2Cx, ENABLE);
			if (I2Cx == I2C1)
			{
				s_i2c1_master_state = I2C1_M_IDLE;
				s_i2c1_master_done = 1;   /* 通知 SendRequest 事务已终止 */
				s_i2c1_srx_state = I2C1_SRX_IDLE;
				i2c1_recv_data_struct.recv_data_len = 0;
			}
			else if (I2Cx == I2C2)
			{
				s_i2c2_master_state = I2C2_M_IDLE;
				s_i2c2_master_done = 1;   /* 通知 SendRequest 事务已终止 */
				s_i2c2_srx_state = I2C2_SRX_IDLE;
				i2c2_recv_data_struct.recv_data_len = 0;
			}
		}
		else if (SR1Register & I2C_FLAG_ARLO)
		{
			/* Arbitration Lost: 清 ARLO + 重置状态机 */
			I2C_ClearFlag(I2Cx, I2C_FLAG_ARLO);
			if (I2Cx == I2C1)
			{
				s_i2c1_master_state = I2C1_M_IDLE;
				s_i2c1_master_done = 1;   /* 通知 SendRequest 事务已终止 */
				s_i2c1_srx_state = I2C1_SRX_IDLE;
				i2c1_recv_data_struct.recv_data_len = 0;
			}
			else if (I2Cx == I2C2)
			{
				s_i2c2_master_state = I2C2_M_IDLE;
				s_i2c2_master_done = 1;   /* 通知 SendRequest 事务已终止 */
				s_i2c2_srx_state = I2C2_SRX_IDLE;
				i2c2_recv_data_struct.recv_data_len = 0;
			}
		}
		/* 从机接收(地址匹配/RXNE/STOPF)事件现在由 I2C1_EV_IRQHandler_Master /
		 * I2C2_EV_IRQHandler_Master 处理——BUF/EVT 事件走 EV 中断线,不会进到
		 * 这个挂在 ER 中断线上的函数里,原来这里的三个 SLAVE_* 分支从未真正
		 * 执行过,已删除,避免被误当成生效代码。这里只保留 AF/BERR/ARLO。 */
}


/*																						
*********************************************************************************************************
*	�� �� ��: i2c_init
*	����˵��: I2C��GPIO���ü�IICģʽ����
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void Bsp_I2C_Init(void)
{

	I2C_GPIO_Config();
	I2C_Mode_Config();

}

/*																						
*********************************************************************************************************
*	�� �� ��: I2C1_EV_IRQHandler
*	����˵��: I2C1�¼��ж�
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void I2C1_EV_IRQHandler(void)
{
#if (IPMB_A_ROLE == IPMB_A_SLAVE)
	I2C1_EV_IRQHandler_Slave();
#else
	I2C1_EV_IRQHandler_Master();
#endif
}

/*																						
*********************************************************************************************************
*	�� �� ��: I2C1_ER_IRQHandler
*	����˵��: I2C1�����ж�
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void I2C1_ER_IRQHandler(void)
{
	i2c_recv_callback(I2C1);
}

/*
*********************************************************************************************************
*	�� �� ��: I2C2_EV_IRQHandler
*	����˵��: I2C2�¼��ж�
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void I2C2_EV_IRQHandler(void)
{
	I2C2_EV_IRQHandler_Master();
}

/*																						
*********************************************************************************************************
*	�� �� ��: I2C2_ER_IRQHandler
*	����˵��: I2C2�����ж�
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void I2C2_ER_IRQHandler(void)
{
		i2c_recv_callback(I2C2);
}

/* 注:旧的"主机读方向 RXNE 搬字节"辅助函数 i2c1_recv_byte_mr()/i2c2_recv_byte_mr()
 * 已随着"主机发完请求再另起一次主机读事务"模型的废弃而不再被调用,已删除
 * (响应改由从机主动 write 回来,由 i2c_recv_data() 在从机接收分支里搬字节)。 */

void I2C2_EV_IRQHandler_Master(void)
{
	uint32_t ev;

	/* SB / ADDR / TXE / BTF / RXNE / STOPF */
	ev = I2C_GetLastEvent(I2C2);

	/* ★ 关键:地址 NACK 后 AF+TXE 同时置位,EV ISR 优先级可能高于 ER,
	 * 必须在此处检测 AF 并立即处理,否则 TXE 会导致 EV ISR 无限循环。
	 * 中断保持常开(不再 DISABLE):从机(F103)随时可能主动 write 响应帧过来,
	 * 关中断会把这个写入直接吞掉。 */
	if (I2C_GetFlagStatus(I2C2, I2C_FLAG_AF))
	{
		I2C_ClearFlag(I2C2, I2C_FLAG_AF);
		if (s_i2c2_master_state != I2C2_M_IDLE)
		{
			/* 本机发起的写请求被 NACK:发 STOP 中止事务,通知任务不必再等 */
			I2C_GenerateSTOP(I2C2, ENABLE);
			s_i2c2_master_state = I2C2_M_IDLE;
			s_i2c2_master_done = 1;   /* 事务因 NACK 中止,通知任务不必再等 */
		}
		s_i2c2_srx_state = I2C2_SRX_IDLE;   /* 丢弃可能在收的半帧,防止状态错乱 */
		return;
	}

	if (s_i2c2_master_state != I2C2_M_IDLE)
	{
		/* ============================================================
		 * 本机自己发起的主机事务(写请求帧)
		 * ============================================================ */
		switch (s_i2c2_master_state)
		{
		case I2C2_M_IDLE:
			break;

		case I2C2_M_SEND_ADDR_W:
			/* EV5: SB=1 → 写地址(写方向) */
			if ((ev & I2C_EVENT_MASTER_MODE_SELECT) == I2C_EVENT_MASTER_MODE_SELECT)
			{
				I2C_Send7bitAddress(I2C2, s_i2c2_master_tx_target_addr, I2C_Direction_Transmitter);
				s_i2c2_master_state = I2C2_M_SEND_DATA;
			}
			break;

		case I2C2_M_SEND_DATA:
			/* EV6: ADDR=1 → 清地址,继续写 */
			if ((ev & I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)
			{
				(void)I2C2->SR2;
			}
			/* EV8 / EV8_2: TXE / BTF */
			else if ((ev & I2C_EVENT_MASTER_BYTE_TRANSMITTED) == I2C_EVENT_MASTER_BYTE_TRANSMITTED)
			{
				/* 请求帧发完:分离式事务,发 STOP 结束写事务(不再 repeated-START)。
				 * F103 从机收到 STOP 后会在自己任务里处理请求,并主动切主机把
				 * 响应 write 回来——不再是"等被读",中断保持常开,交给下面的
				 * IDLE 分支接住这次主动写入。 */
				I2C_GenerateSTOP(I2C2, ENABLE);
				s_i2c2_master_state = I2C2_M_IDLE;
				s_i2c2_master_done = 1;
			}
			else if ((ev & I2C_EVENT_MASTER_BYTE_TRANSMITTING) == I2C_EVENT_MASTER_BYTE_TRANSMITTING)
			{
				if (s_i2c2_master_tx_idx < i2c2_send_data_struct.send_data_len)
				{
					I2C_SendData(I2C2, i2c2_send_data_struct.send_buf[s_i2c2_master_tx_idx++]);
				}
			}
			break;

		case I2C2_M_RESTART:
		case I2C2_M_SEND_ADDR_R:
		case I2C2_M_RECV_DATA:
		case I2C2_M_DONE:
		default:
			/* 旧的"主机发完请求再另起一次主机读事务"模型已废弃(响应改由从机
			 * 主动 write 回来,见下方 IDLE 分支),这些状态不再被进入。 */
			break;
		}

		/* STOPF:实际在我们 GenerateSTOP 后由硬件置位,清掉 */
		if ((ev & I2C_EVENT_SLAVE_STOP_DETECTED) == I2C_EVENT_SLAVE_STOP_DETECTED)
		{
			/* 清 STOPF:读 SR1 写 CR1 */
			(void)I2C2->SR1;
			I2C2->CR1 |= (uint16_t)0x0001; /* dummy write to clear STOPF */
		}
		return;
	}

	/* ============================================================
	 * s_i2c2_master_state == IDLE:不是本机发起的事务。
	 * 从机(F103)处理完请求后主动切主机、把响应帧 write 回本机地址
	 * (IPMB_HOST_I2C_ADDR = 0x20)的场景,就是在这里被动接住的。
	 * ============================================================ */
	if ((ev & I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED) == I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED)
	{
		(void)I2C2->SR2;                          /* 读 SR2 清 ADDR */
		i2c2_recv_data_struct.recv_data_len = 0;  /* 开始接收新的一帧 */
		s_i2c2_srx_state = I2C2_SRX_RECEIVING;
		s_i2c2_srx_enter_tick = xTaskGetTickCount();
		return;
	}
	if ((ev & I2C_EVENT_SLAVE_BYTE_RECEIVED) == I2C_EVENT_SLAVE_BYTE_RECEIVED)
	{
		if (s_i2c2_srx_state == I2C2_SRX_RECEIVING)
		{
			i2c_recv_data(I2C2);
		}
		else
		{
			(void)I2C_ReceiveData(I2C2);   /* 防御性丢弃:没有先看到地址匹配的意外 RXNE */
		}
		return;
	}
	if ((ev & I2C_EVENT_SLAVE_STOP_DETECTED) == I2C_EVENT_SLAVE_STOP_DETECTED)
	{
		(void)I2C2->SR1;
		I2C2->CR1 |= (uint16_t)0x0001;
		if (s_i2c2_srx_state == I2C2_SRX_RECEIVING)
		{
			s_i2c2_srx_state = I2C2_SRX_IDLE;
			i2cSemaphoreGive(I2C2);   /* 唤醒正在等待响应的 SendRequest */
		}
		return;
	}
	/* 其它事件:忽略,不关中断 */
}

/**
 * @brief  I2C1_SendRequest 内部两处"丢帧"点复用:清空 i2c1_recv_data_struct
 *         之前先判断它是不是从机主动推来的 PEM 帧(is_pem_push_frame,定义于
 *         task_i2c.c/.h),是则转发进 xIPMB_PemQueue 再清空,不再直接静默丢弃。
 */
static void i2c1_forward_pem_and_clear(void)
{
	if (xIPMB_PemQueue != NULL &&
	    is_pem_push_frame(i2c1_recv_data_struct.recv_buf, i2c1_recv_data_struct.recv_data_len))
	{
		ipmb_pem_pkt_t pem;
		uint8_t i;
		pem.len = (uint8_t)i2c1_recv_data_struct.recv_data_len;
		if (pem.len > IPMB_PEM_BUF_SIZE) pem.len = IPMB_PEM_BUF_SIZE;
		for (i = 0; i < pem.len; i++) pem.buf[i] = i2c1_recv_data_struct.recv_buf[i];
		xQueueSend(xIPMB_PemQueue, &pem, 0);
	}
	i2c1_recv_data_struct.recv_data_len = 0;
}

/* 同上,I2C2 版本 */
static void i2c2_forward_pem_and_clear(void)
{
	if (xIPMB_PemQueue2 != NULL &&
	    is_pem_push_frame(i2c2_recv_data_struct.recv_buf, i2c2_recv_data_struct.recv_data_len))
	{
		ipmb_pem_pkt_t pem;
		uint8_t i;
		pem.len = (uint8_t)i2c2_recv_data_struct.recv_data_len;
		if (pem.len > IPMB_PEM_BUF_SIZE) pem.len = IPMB_PEM_BUF_SIZE;
		for (i = 0; i < pem.len; i++) pem.buf[i] = i2c2_recv_data_struct.recv_buf[i];
		xQueueSend(xIPMB_PemQueue2, &pem, 0);
	}
	i2c2_recv_data_struct.recv_data_len = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: I2C2_SendRequest
*	����˵��: IPMB-B(I2C2)主机启动一次 IPMI 请求:写入请求帧,随后读出响应帧。
*	��    ��: tx_buf:请求字节(已含完整 IPMI 帧,从机地址字节 + 净荷,不含硬件 I2C 地址字节)
*	          tx_len:请求字节数
*	          rx_expect_len:期望接收字节数(响应净荷,不含硬件 I2C 帧头)
*	          target_addr:从机 7bit 地址(IPMB-A = 0x0A)
*********************************************************************************************************
*/
void I2C2_SendRequest(const uint8_t *tx_buf, uint8_t tx_len, uint8_t rx_expect_len, uint8_t target_addr)
{
	uint8_t i;
	uint16_t guard;
	if (tx_buf == NULL || tx_len == 0) return;
	if (tx_len < 6) return;   /* 下面按回显 cmd(偏移5)匹配响应,必须至少有这个字节 */
	if (tx_len > BUFFER_TX_SIZE) tx_len = BUFFER_TX_SIZE;
	if (rx_expect_len > BUFFER_RX_SIZE) rx_expect_len = BUFFER_RX_SIZE;

	/* ============ 事务 1:写请求帧 + STOP ============
	 * 与 I2C1 相同的分离式事务模型:F103 从机在收到 STOP 后才于任务里处理请求、
	 * 装入响应,故写完必须发 STOP,不能用 repeated-START 直接接读。 */
	for (i = 0; i < tx_len; i++)
	{
		i2c2_send_data_struct.send_buf[i] = tx_buf[i];
	}
	i2c2_send_data_struct.send_data_len = tx_len;
	i2c2_send_data_struct.send_data_cnt = 0;
	s_i2c2_master_tx_idx = 0;
	s_i2c2_master_tx_target_addr = target_addr;
	s_i2c2_master_done = 0;

	I2C_AcknowledgeConfig(I2C2, ENABLE);

	/* 总线卡死检测:物理插拔线缆可能让 BUSY/STOPF 假死,GenerateSTART 永远不会
	 * 真正发起。除非本机正合法接收从机推送(srx_state==RECEIVING 且未超时),
	 * 否则短等 5ms 仍卡死就做软复位恢复,解决"断开重连后再也连不上"的问题。 */
	{
		uint8_t legit_receiving = (s_i2c2_srx_state == I2C2_SRX_RECEIVING) &&
		                           ((xTaskGetTickCount() - s_i2c2_srx_enter_tick) < pdMS_TO_TICKS(50));
		if (!legit_receiving &&
		    (I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY) || (I2C2->SR1 & I2C_SR1_STOPF)))
		{
			TickType_t busy_t0 = xTaskGetTickCount();
			while (!legit_receiving &&
			       (I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY) || (I2C2->SR1 & I2C_SR1_STOPF)))
			{
				if ((xTaskGetTickCount() - busy_t0) > pdMS_TO_TICKS(5))
				{
					i2c_master_busy_recover(I2C2);
					break;
				}
				vTaskDelay(1);
				legit_receiving = (s_i2c2_srx_state == I2C2_SRX_RECEIVING) &&
				                   ((xTaskGetTickCount() - s_i2c2_srx_enter_tick) < pdMS_TO_TICKS(50));
			}
		}
	}

	s_i2c2_master_state = I2C2_M_SEND_ADDR_W;
	I2C_GenerateSTART(I2C2, ENABLE);

	/* 等写事务完成(ISR 发完 STOP 或 NACK 中止时置 done),最多 ~20ms */
	guard = 0;
	while (!s_i2c2_master_done && guard < 20) { vTaskDelay(1); guard++; }

	/* 写事务超时 (从机不存在/无应答):中断返回,不再等响应 */
	if (!s_i2c2_master_done) return;

	/* ============ 事务 2:等从机主动 write 回来的响应帧 ============
	 * F103 从机收到 STOP 后在自己任务里处理请求、主动切主机把响应帧 write 回本机
	 * (IPMB_HOST_I2C_ADDR),不再是"等被读"。这里改为在 I2C2ReceiveSemaphore 上
	 * 等待——由 I2C2_EV_IRQHandler_Master 的从机接收分支在收完一帧(STOPF)后
	 * xSemaphoreGiveFromISR。耗时不定,按"响应 byte5(cmd 回显)==本次请求 cmd"
	 * 匹配,不匹配当残留帧丢弃继续等,总预算 ~120ms(与旧的 12×8ms 轮询量级一致)。
	 * 【2026-07-28更正】同 I2C1_SendRequest 的更正:光比 cmd 字节不够,连续点
	 * 同一个控制按钮(同 cmd)时,上一条请求晚到的响应会被这里误收,导致这次
	 * 真正的响应错失、间歇性超时。补上 byte4(rqSeq<<2|lun)校验。 */
	{
		uint8_t expect_cmd   = tx_buf[5];
		uint8_t expect_byte4 = tx_buf[4];   /* rqSeq<<2|lun,协议要求原样回显 */
		TickType_t t_start = xTaskGetTickCount();
		const TickType_t budget = pdMS_TO_TICKS(120);

		(void)rx_expect_len;   /* 响应长度由请求方在任务层估算后仅用于超时兜底,这里靠 STOPF 天然定帧长 */

		xSemaphoreTake(I2C2ReceiveSemaphore, 0);   /* 清掉可能残留的旧 give */
		i2c2_forward_pem_and_clear();   /* 清空前先看一眼是不是从机主动推来的 PEM,是则转发不丢 */

		for (;;)
		{
			TickType_t elapsed = xTaskGetTickCount() - t_start;
			if (elapsed >= budget)
			{
				i2c2_recv_data_struct.recv_data_len = 0;
				break;                         /* 总预算耗尽,放弃 */
			}
			if (xSemaphoreTake(I2C2ReceiveSemaphore, budget - elapsed) != pdTRUE)
			{
				i2c2_recv_data_struct.recv_data_len = 0;
				break;                         /* 等不到从机推送,超时 */
			}
			if (i2c2_recv_data_struct.recv_data_len >= 6 &&
			    i2c2_recv_data_struct.recv_buf[5] == expect_cmd &&
			    i2c2_recv_data_struct.recv_buf[4] == expect_byte4)
			{
				break;                         /* 收到本次请求对应的响应 */
			}
			/* 残留/不匹配的旧帧:可能是从机主动推来的 PEM,转发后再丢弃,
			 * 在剩余预算内继续等 */
			i2c2_forward_pem_and_clear();
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: I2C1_EV_IRQHandler_Master
*	����˵��: I2C1 作为主机时的 EV 中断处理:负责 IPMI 请求的发送 + 响应的接收。
*********************************************************************************************************
*/
void I2C1_EV_IRQHandler_Master(void)
{
	uint32_t ev;

	/* SB / ADDR / TXE / BTF / RXNE / STOPF */
	ev = I2C_GetLastEvent(I2C1);

	/* ★ 关键:地址 NACK 后 AF+TXE 同时置位,EV ISR 优先级可能高于 ER,
	 * 必须在此处检测 AF 并立即处理,否则 TXE 会导致 EV ISR 无限循环。
	 * 中断保持常开(不再 DISABLE):从机(F103)随时可能主动 write 响应帧过来,
	 * 关中断会把这个写入直接吞掉。 */
	if (I2C_GetFlagStatus(I2C1, I2C_FLAG_AF))
	{
		I2C_ClearFlag(I2C1, I2C_FLAG_AF);
		if (s_i2c1_master_state != I2C1_M_IDLE)
		{
			/* 本机发起的写请求被 NACK:发 STOP 中止事务,通知任务不必再等 */
			I2C_GenerateSTOP(I2C1, ENABLE);
			s_i2c1_master_state = I2C1_M_IDLE;
			s_i2c1_master_done = 1;   /* 事务因 NACK 中止,通知任务不必再等 */
		}
		s_i2c1_srx_state = I2C1_SRX_IDLE;   /* 丢弃可能在收的半帧,防止状态错乱 */
		return;
	}

	if (s_i2c1_master_state != I2C1_M_IDLE)
	{
		/* ============================================================
		 * 本机自己发起的主机事务(写请求帧)
		 * ============================================================ */
		switch (s_i2c1_master_state)
		{
		case I2C1_M_IDLE:
			break;

		case I2C1_M_SEND_ADDR_W:
			/* EV5: SB=1 → 写地址(写方向) */
			if ((ev & I2C_EVENT_MASTER_MODE_SELECT) == I2C_EVENT_MASTER_MODE_SELECT)
			{
				I2C_Send7bitAddress(I2C1, s_i2c1_master_tx_target_addr, I2C_Direction_Transmitter);
				s_i2c1_master_state = I2C1_M_SEND_DATA;
			}
			break;

		case I2C1_M_SEND_DATA:
			/* EV6: ADDR=1 → 清地址,继续写 */
			if ((ev & I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)
			{
				(void)I2C1->SR2;
			}
			/* EV8 / EV8_2: TXE / BTF */
			else if ((ev & I2C_EVENT_MASTER_BYTE_TRANSMITTED) == I2C_EVENT_MASTER_BYTE_TRANSMITTED)
			{
				/* 请求帧发完:分离式事务,发 STOP 结束写事务(不再 repeated-START)。
				 * F103 从机(IPMB-B)收到 STOP 后会在自己任务里处理请求,并主动切
				 * 主机把响应 write 回来——不再是"等被读",中断保持常开,交给
				 * 下面的 IDLE 分支接住这次主动写入。 */
				I2C_GenerateSTOP(I2C1, ENABLE);
				s_i2c1_master_state = I2C1_M_IDLE;
				s_i2c1_master_done = 1;
			}
			else if ((ev & I2C_EVENT_MASTER_BYTE_TRANSMITTING) == I2C_EVENT_MASTER_BYTE_TRANSMITTING)
			{
				if (s_i2c1_master_tx_idx < i2c1_send_data_struct.send_data_len)
				{
					I2C_SendData(I2C1, i2c1_send_data_struct.send_buf[s_i2c1_master_tx_idx++]);
				}
			}
			break;

		case I2C1_M_RESTART:
		case I2C1_M_SEND_ADDR_R:
		case I2C1_M_RECV_DATA:
		case I2C1_M_DONE:
		default:
			/* 旧的"主机发完请求再另起一次主机读事务"模型已废弃(响应改由从机
			 * 主动 write 回来,见下方 IDLE 分支),这些状态不再被进入。 */
			break;
		}

		/* STOPF:实际在我们 GenerateSTOP 后由硬件置位,清掉 */
		if ((ev & I2C_EVENT_SLAVE_STOP_DETECTED) == I2C_EVENT_SLAVE_STOP_DETECTED)
		{
			/* 清 STOPF:读 SR1 写 CR1 */
			(void)I2C1->SR1;
			I2C1->CR1 |= (uint16_t)0x0001; /* dummy write to clear STOPF */
		}
		return;
	}

	/* ============================================================
	 * s_i2c1_master_state == IDLE:不是本机发起的事务。
	 * 从机(F103)处理完请求后主动切主机、把响应帧 write 回本机地址
	 * (IPMB_HOST_I2C_ADDR = 0x20)的场景,就是在这里被动接住的。
	 * ============================================================ */
	if ((ev & I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED) == I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED)
	{
		(void)I2C1->SR2;                          /* 读 SR2 清 ADDR */
		i2c1_recv_data_struct.recv_data_len = 0;  /* 开始接收新的一帧 */
		s_i2c1_srx_state = I2C1_SRX_RECEIVING;
		s_i2c1_srx_enter_tick = xTaskGetTickCount();
		return;
	}
	if ((ev & I2C_EVENT_SLAVE_BYTE_RECEIVED) == I2C_EVENT_SLAVE_BYTE_RECEIVED)
	{
		if (s_i2c1_srx_state == I2C1_SRX_RECEIVING)
		{
			i2c_recv_data(I2C1);
		}
		else
		{
			(void)I2C_ReceiveData(I2C1);   /* 防御性丢弃:没有先看到地址匹配的意外 RXNE */
		}
		return;
	}
	if ((ev & I2C_EVENT_SLAVE_STOP_DETECTED) == I2C_EVENT_SLAVE_STOP_DETECTED)
	{
		(void)I2C1->SR1;
		I2C1->CR1 |= (uint16_t)0x0001;
		if (s_i2c1_srx_state == I2C1_SRX_RECEIVING)
		{
			s_i2c1_srx_state = I2C1_SRX_IDLE;
			i2cSemaphoreGive(I2C1);   /* 唤醒正在等待响应的 SendRequest */
		}
		return;
	}
	/* 其它事件:忽略,不关中断 */
}

/*
*********************************************************************************************************
*	�� �� ��: I2C1_SendRequest
*	����˵��: IPMB-A(I2C1)主机启动一次 IPMI 请求:写入请求帧,随后读出响应帧。
*	��    ��: tx_buf:请求字节(已含完整 IPMI 帧,从机地址字节 + 净荷,不含硬件 I2C 地址字节)
*	          tx_len:请求字节数
*	          rx_expect_len:期望接收字节数(响应净荷,不含硬件 I2C 帧头)
*	          target_addr:从机 7bit 地址
*********************************************************************************************************
*/
void I2C1_SendRequest(const uint8_t *tx_buf, uint8_t tx_len, uint8_t rx_expect_len, uint8_t target_addr)
{
	uint8_t i;
	uint16_t guard;
	if (tx_buf == NULL || tx_len == 0) return;
	if (tx_len < 6) return;   /* 下面按回显 cmd(偏移5)匹配响应,必须至少有这个字节 */
	if (tx_len > BUFFER_TX_SIZE) tx_len = BUFFER_TX_SIZE;
	if (rx_expect_len > BUFFER_RX_SIZE) rx_expect_len = BUFFER_RX_SIZE;

	/* ============ 事务 1:写请求帧 + STOP ============
	 * F103 从机(IPMB-B)在收到 STOP 后才于任务里处理请求、装入响应,
	 * 所以这里写完必须发 STOP,不能用 repeated-START 直接接读。 */
	for (i = 0; i < tx_len; i++)
	{
		i2c1_send_data_struct.send_buf[i] = tx_buf[i];
	}
	i2c1_send_data_struct.send_data_len = tx_len;
	i2c1_send_data_struct.send_data_cnt = 0;
	s_i2c1_master_tx_idx = 0;
	s_i2c1_master_tx_target_addr = target_addr;
	s_i2c1_master_done = 0;

	I2C_AcknowledgeConfig(I2C1, ENABLE);

	/* 总线卡死检测:物理插拔线缆可能让 BUSY/STOPF 假死,GenerateSTART 永远不会
	 * 真正发起。除非本机正合法接收从机推送(srx_state==RECEIVING 且未超时),
	 * 否则短等 5ms 仍卡死就做软复位恢复,解决"断开重连后再也连不上"的问题。 */
	{
		uint8_t legit_receiving = (s_i2c1_srx_state == I2C1_SRX_RECEIVING) &&
		                           ((xTaskGetTickCount() - s_i2c1_srx_enter_tick) < pdMS_TO_TICKS(50));
		if (!legit_receiving &&
		    (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) || (I2C1->SR1 & I2C_SR1_STOPF)))
		{
			TickType_t busy_t0 = xTaskGetTickCount();
			while (!legit_receiving &&
			       (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) || (I2C1->SR1 & I2C_SR1_STOPF)))
			{
				if ((xTaskGetTickCount() - busy_t0) > pdMS_TO_TICKS(5))
				{
					i2c_master_busy_recover(I2C1);
					break;
				}
				vTaskDelay(1);
				legit_receiving = (s_i2c1_srx_state == I2C1_SRX_RECEIVING) &&
				                   ((xTaskGetTickCount() - s_i2c1_srx_enter_tick) < pdMS_TO_TICKS(50));
			}
		}
	}

	s_i2c1_master_state = I2C1_M_SEND_ADDR_W;
	I2C_GenerateSTART(I2C1, ENABLE);

	/* 等写事务完成(ISR 发完 STOP 或 NACK 中止时置 done),最多 ~20ms */
	guard = 0;
	while (!s_i2c1_master_done && guard < 20) { vTaskDelay(1); guard++; }

	/* 写事务超时 (从机不存在/无应答):中断返回,不再等响应 */
	if (!s_i2c1_master_done) return;

	/* ============ 事务 2:等从机主动 write 回来的响应帧 ============
	 * F103 从机(IPMB-A)收到 STOP 后在自己任务里处理请求、主动切主机把响应帧
	 * write 回本机(IPMB_HOST_I2C_ADDR),不再是"等被读"。这里改为在
	 * I2C1ReceiveSemaphore 上等待——由 I2C1_EV_IRQHandler_Master 的从机接收
	 * 分支在收完一帧(STOPF)后 xSemaphoreGiveFromISR。耗时不定,按"响应
	 * byte5(cmd 回显)==本次请求 cmd"匹配,不匹配当残留帧丢弃继续等,总预算
	 * ~120ms(与旧的 12×8ms 轮询量级一致)。
	 * 【2026-07-28更正】光比 cmd 字节不够:网页控制台的开机/关机/复位/软关机/
	 * 软复位这几个按钮全都是同一个 cmd(Set FRU Activation),连续点同一个按钮
	 * 测试时,如果上一条请求的响应因总线偶发延迟晚到,会被这里误判成"这次"的
	 * 响应收下——上层 task_ctrl_dispatch.c 虽然会靠 rqSeq 发现不对再拒绝,但
	 * 那时真正属于这次请求的响应已经把信号量"发过一次"的机会用掉、找不回来了,
	 * 表现为间歇性超时(实测复现:连续点"复位"，超时和成功交替出现)。这里补上
	 * byte4(rqSeq<<2|lun 回显)校验,从源头避免信号量被同 cmd 的旧响应误消耗。 */
	{
		uint8_t expect_cmd   = tx_buf[5];
		uint8_t expect_byte4 = tx_buf[4];   /* rqSeq<<2|lun,协议要求原样回显 */
		TickType_t t_start = xTaskGetTickCount();
		const TickType_t budget = pdMS_TO_TICKS(120);

		(void)rx_expect_len;   /* 响应长度由请求方在任务层估算后仅用于超时兜底,这里靠 STOPF 天然定帧长 */

		xSemaphoreTake(I2C1ReceiveSemaphore, 0);   /* 清掉可能残留的旧 give */
		i2c1_forward_pem_and_clear();   /* 清空前先看一眼是不是从机主动推来的 PEM,是则转发不丢 */

		for (;;)
		{
			TickType_t elapsed = xTaskGetTickCount() - t_start;
			if (elapsed >= budget)
			{
				i2c1_recv_data_struct.recv_data_len = 0;
				break;                         /* 总预算耗尽,放弃 */
			}
			if (xSemaphoreTake(I2C1ReceiveSemaphore, budget - elapsed) != pdTRUE)
			{
				i2c1_recv_data_struct.recv_data_len = 0;
				break;                         /* 等不到从机推送,超时 */
			}
			if (i2c1_recv_data_struct.recv_data_len >= 6 &&
			    i2c1_recv_data_struct.recv_buf[5] == expect_cmd &&
			    i2c1_recv_data_struct.recv_buf[4] == expect_byte4)
			{
				break;                         /* 收到本次请求对应的响应 */
			}
			/* 残留/不匹配的旧帧:可能是从机主动推来的 PEM,转发后再丢弃,
			 * 在剩余预算内继续等 */
			i2c1_forward_pem_and_clear();
		}
	}
}
/*
*********************************************************************************************************
*	           : I2C1_EV_IRQHandler_Slave
*	           : IPMB-A(I2C1) is slave,EV interrupt handler.
*	          - ADDR=matched,RXNE TXE
*********************************************************************************************************
*/
typedef enum {
	I2C1_S_IDLE = 0,
	I2C1_S_TXMATCHED,
	I2C1_S_TXSENDING,
	I2C1_S_TXSTOPPED,
} i2c1_slave_tx_state_t;

static volatile i2c1_slave_tx_state_t s_i2c1_tx_state = I2C1_S_IDLE;
static volatile uint8_t s_i2c1_tx_idx = 0;

void I2C1_EV_IRQHandler_Slave(void)
{
	uint32_t ev = I2C_GetLastEvent(I2C1);

	/* --- 主机切换到读方向,从机进入发送状态机 --- */
	if (ev & I2C_EVENT_SLAVE_TRANSMITTER_ADDRESS_MATCHED)
	{
		/* 写第 1 个字节(若已有待发响应) */
		s_i2c1_tx_state = I2C1_S_TXMATCHED;
		s_i2c1_tx_idx = 0;
		if (i2c1_send_data_struct.send_data_len > 0)
		{
			I2C_SendData(I2C1, i2c1_send_data_struct.send_buf[s_i2c1_tx_idx++]);
		}
		else
		{
			/* 没有待发数据,送 0xFF */
			I2C_SendData(I2C1, 0xFF);
			s_i2c1_tx_idx++;
		}
		s_i2c1_tx_state = I2C1_S_TXSENDING;
	}
	else if (ev & I2C_EVENT_SLAVE_BYTE_TRANSMITTING)
	{
		if (s_i2c1_tx_idx < i2c1_send_data_struct.send_data_len)
		{
			I2C_SendData(I2C1, i2c1_send_data_struct.send_buf[s_i2c1_tx_idx++]);
		}
		else
		{
			/* 数据已发完,送 0xFF 让主机收到 BTF/NACK */
			I2C_SendData(I2C1, 0xFF);
		}
	}
	else if (ev & I2C_EVENT_SLAVE_BYTE_TRANSMITTED)
	{
		/* BTF,准备收尾 */
	}
	else if ((ev & I2C_EVENT_SLAVE_STOP_DETECTED) == I2C_EVENT_SLAVE_STOP_DETECTED)
	{
		/* 清 STOPF:先读 SR1,再写 CR1 */
		(void)I2C1->SR1;
		I2C1->CR1 |= (uint16_t)0x0001;
		s_i2c1_tx_state = I2C1_S_TXSTOPPED;
		/* 重置 */
		s_i2c1_tx_state = I2C1_S_IDLE;
	}
	else if (ev & I2C_EVENT_SLAVE_ACK_FAILURE)
	{
		/* 主机 NACK,清 AF */
		I2C_ClearFlag(I2C1, I2C_FLAG_AF);
		s_i2c1_tx_state = I2C1_S_IDLE;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: I2C1_SlaveSendResponse
*	����˵��: 丛机模式下,把待发响应写入缓冲区,等待主机读。
*********************************************************************************************************
*/
void I2C1_SlaveSendResponse(const uint8_t *buf, uint8_t len)
{
	uint8_t i;
	if (buf == NULL || len == 0) return;
	if (len > BUFFER_TX_SIZE) len = BUFFER_TX_SIZE;
	for (i = 0; i < len; i++)
	{
		i2c1_send_data_struct.send_buf[i] = buf[i];
	}
	i2c1_send_data_struct.send_data_len = len;
	i2c1_send_data_struct.send_data_cnt = 0;
	s_i2c1_tx_idx = 0;
	s_i2c1_tx_state = I2C1_S_IDLE;
}

