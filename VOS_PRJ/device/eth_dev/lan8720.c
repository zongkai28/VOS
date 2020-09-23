#include "lan8720.h"
#include "stm32f4x7_eth.h"
#include "usart.h"  

void VOSDelayUs(u32 us);

void Lan8720ResetPinInit()
{
  GPIO_InitTypeDef  GPIO_InitStructure;

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  GPIO_SetBits(GPIOB, GPIO_Pin_0);
}

void Lan8720Reset()
{
	GPIO_ResetBits(GPIOB,GPIO_Pin_0);
	VOSDelayUs(50*1000);
	GPIO_SetBits(GPIOB, GPIO_Pin_0);
}

//LAN8720��ʼ��
//����ֵ:0,�ɹ�;
//    ����,ʧ��
u8 LAN8720_Init(void)
{
	u8 rval=0;
	//ETH IO�ӿڳ�ʼ��
 	RCC->AHB1ENR|=1<<0;     //ʹ��PORTAʱ�� 
 	RCC->AHB1ENR|=1<<1;     //ʹ��PORTBʱ�� 
 	RCC->AHB1ENR|=1<<2;     //ʹ��PORTCʱ��  
	RCC->AHB1ENR|=1<<6;     //ʹ��PORTGʱ�� 
 	RCC->APB2ENR|=1<<14;   	//ʹ��SYSCFGʱ��
	SYSCFG->PMC|=1<<23;		//ʹ��RMII PHY�ӿ�.

	GPIO_Set(GPIOA, GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_7, GPIO_Mode_AF,GPIO_OType_PP, GPIO_Speed_100MHz, GPIO_PuPd_UP);		//PA1,2,7�������
	GPIO_Set(GPIOC, GPIO_Pin_1|GPIO_Pin_4|GPIO_Pin_5, GPIO_Mode_AF,GPIO_OType_PP, GPIO_Speed_100MHz, GPIO_PuPd_UP);		//PC1,4,5�������
	GPIO_Set(GPIOG, GPIO_Pin_13|GPIO_Pin_14, GPIO_Mode_AF,GPIO_OType_PP, GPIO_Speed_100MHz, GPIO_PuPd_UP);		//PG13,14�������
	GPIO_Set(GPIOB, GPIO_Pin_11, GPIO_Mode_AF, GPIO_OType_PP, GPIO_Speed_100MHz, GPIO_PuPd_UP);				//PB11�������
 
	GPIO_AF_Set(GPIOA,1,11);	//PA1,AF11
 	GPIO_AF_Set(GPIOA,2,11);	//PA2,AF11
 	GPIO_AF_Set(GPIOA,7,11);	//PA7,AF11
	
  	GPIO_AF_Set(GPIOB,11,11);	//PB11,AF11
 
	GPIO_AF_Set(GPIOC,1,11);	//PC1,AF11
 	GPIO_AF_Set(GPIOC,4,11);	//PC4,AF11
 	GPIO_AF_Set(GPIOC,5,11);	//PC5,AF11
 
  	GPIO_AF_Set(GPIOG,13,11);	//PG13,AF11
 	GPIO_AF_Set(GPIOG,14,11);	//PG14,AF11  
	
 	Lan8720ResetPinInit();
 	Lan8720Reset();

 	MY_NVIC_Init(0,0,ETH_IRQn,2);//����ETH�еķ���
	rval=ETH_MACDMA_Config();
	return !rval;				//ETH�Ĺ���Ϊ:0,ʧ��;1,�ɹ�;����Ҫȡ��һ�� 
}
//�õ�8720���ٶ�ģʽ
//����ֵ:
//001:10M��˫��
//101:10Mȫ˫��
//010:100M��˫��
//110:100Mȫ˫��
//����:����.
u8 LAN8720_Get_Speed(void)
{
	u8 speed;
	speed=((ETH_ReadPHYRegister(0x00,31)&0x1C)>>2); //��LAN8720��31�żĴ����ж�ȡ�����ٶȺ�˫��ģʽ
	return speed;
}

//��ʼ��ETH MAC�㼰DMA����
//����ֵ:ETH_ERROR,����ʧ��(0)
//		ETH_SUCCESS,���ͳɹ�(1)
u8 ETH_MACDMA_Config(void)
{
	u8 rval;
	ETH_InitTypeDef ETH_InitStructure; 
	
	//ʹ����̫��ʱ��
	RCC->AHB1ENR|=7<<25;//ʹ��ETH MAC/MAC_Tx/MAC_Rxʱ��
	
	ETH_DeInit();  								//AHB����������̫��
	ETH_SoftwareReset();  						//������������
	while (ETH_GetSoftwareResetStatus() == SET);//�ȴ���������������� 
	ETH_StructInit(&ETH_InitStructure); 	 	//��ʼ������ΪĬ��ֵ

	///����MAC��������
	ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Enable;   			//������������Ӧ����
	ETH_InitStructure.ETH_LoopbackMode = ETH_LoopbackMode_Disable;					//�رշ���
	ETH_InitStructure.ETH_RetryTransmission = ETH_RetryTransmission_Enable; 		//�ر��ش�����kp
	ETH_InitStructure.ETH_AutomaticPadCRCStrip = ETH_AutomaticPadCRCStrip_Disable; 	//�ر��Զ�ȥ��PDA/CRC����
	ETH_InitStructure.ETH_ReceiveAll = ETH_ReceiveAll_Disable;						//�رս������е�֡
	ETH_InitStructure.ETH_BroadcastFramesReception = ETH_BroadcastFramesReception_Enable;//�����������й㲥֡
	ETH_InitStructure.ETH_PromiscuousMode = ETH_PromiscuousMode_Disable;			//�رջ��ģʽ�ĵ�ַ����
	ETH_InitStructure.ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_Perfect;//�����鲥��ַʹ��������ַ����
	ETH_InitStructure.ETH_UnicastFramesFilter = ETH_UnicastFramesFilter_Perfect;	//�Ե�����ַʹ��������ַ����
#ifdef CHECKSUM_BY_HARDWARE
	ETH_InitStructure.ETH_ChecksumOffload = ETH_ChecksumOffload_Enable; 			//����ipv4��TCP/UDP/ICMP��֡У���ж��
#endif
	//������ʹ��֡У���ж�ع��ܵ�ʱ��һ��Ҫʹ�ܴ洢ת��ģʽ,�洢ת��ģʽ��Ҫ��֤����֡�洢��FIFO��,
	//����MAC�ܲ���/ʶ���֡У��ֵ,����У����ȷ��ʱ��DMA�Ϳ��Դ���֡,����Ͷ�������֡
	ETH_InitStructure.ETH_DropTCPIPChecksumErrorFrame = ETH_DropTCPIPChecksumErrorFrame_Enable; //��������TCP/IP����֡
	ETH_InitStructure.ETH_ReceiveStoreForward = ETH_ReceiveStoreForward_Enable;     //�����������ݵĴ洢ת��ģʽ
	ETH_InitStructure.ETH_TransmitStoreForward = ETH_TransmitStoreForward_Enable;   //�����������ݵĴ洢ת��ģʽ

	ETH_InitStructure.ETH_ForwardErrorFrames = ETH_ForwardErrorFrames_Disable;     	//��ֹת������֡
	ETH_InitStructure.ETH_ForwardUndersizedGoodFrames = ETH_ForwardUndersizedGoodFrames_Disable;	//��ת����С�ĺ�֡
	ETH_InitStructure.ETH_SecondFrameOperate = ETH_SecondFrameOperate_Enable;  		//�򿪴����ڶ�֡����
	ETH_InitStructure.ETH_AddressAlignedBeats = ETH_AddressAlignedBeats_Enable;  	//����DMA����ĵ�ַ���빦��
	ETH_InitStructure.ETH_FixedBurst = ETH_FixedBurst_Enable;            			//�����̶�ͻ������
	ETH_InitStructure.ETH_RxDMABurstLength = ETH_RxDMABurstLength_32Beat;     		//DMA���͵����ͻ������Ϊ32������
	ETH_InitStructure.ETH_TxDMABurstLength = ETH_TxDMABurstLength_32Beat;			//DMA���յ����ͻ������Ϊ32������
	ETH_InitStructure.ETH_DMAArbitration = ETH_DMAArbitration_RoundRobin_RxTx_2_1;
	ETH_InitStructure.Sys_Clock_Freq = 168;//ϵͳʱ��Ƶ��Ϊ168Mhz
	rval=ETH_Init(&ETH_InitStructure, LAN8720_PHY_ADDRESS);//����ETH
	if(rval==ETH_SUCCESS)//���óɹ�
	{
		ETH_DMAITConfig(ETH_DMA_IT_NIS|ETH_DMA_IT_R,ENABLE);//ʹ����̫�������ж�	
	}
	return rval;
}
extern void lwip_packet_handler(void);		//��lwip_comm.c���涨��

int dumphex(const unsigned char *buf, int size)
{
	int i;
	for(i=0; i<size; i++)
		kprintf("%02x,%s", buf[i], (i+1)%16?"":"\r\n");
	return 0;
}


void lwip_packet_handler();

void ETH_IRQHandler(void)
{ 
	int len = 0;
	VOSIntEnter();
	while((len=ETH_GetRxPktSize(DMARxDescToGet))!=0) 	//����Ƿ��յ����ݰ�
	{ 		
//		kprintf("------------------------------------\r\n");
//		dumphex((unsigned char*)(DMARxDescToGet->Buffer1Addr), len);
		lwip_packet_handler();
	} 
	ETH_DMAClearITPendingBit(ETH_DMA_IT_R);
	ETH_DMAClearITPendingBit(ETH_DMA_IT_NIS); 
	VOSIntExit ();
}  
//����һ���������ݰ�
//����ֵ:�������ݰ�֡�ṹ��
FrameTypeDef ETH_Rx_Packet(void)
{ 
	u32 framelength=0;
	FrameTypeDef frame={0,0};   
	//��鵱ǰ������,�Ƿ�����ETHERNET DMA(���õ�ʱ��)/CPU(��λ��ʱ��)
	if((DMARxDescToGet->Status&ETH_DMARxDesc_OWN)!=(u32)RESET)
	{	
		frame.length=ETH_ERROR; 
		if ((ETH->DMASR&ETH_DMASR_RBUS)!=(u32)RESET)  
		{ 
			ETH->DMASR = ETH_DMASR_RBUS;//���ETH DMA��RBUSλ 
			ETH->DMARPDR=0;//�ָ�DMA����
		}
		return frame;//����,OWNλ��������
	}  
	if(((DMARxDescToGet->Status&ETH_DMARxDesc_ES)==(u32)RESET)&& 
	((DMARxDescToGet->Status & ETH_DMARxDesc_LS)!=(u32)RESET)&&  
	((DMARxDescToGet->Status & ETH_DMARxDesc_FS)!=(u32)RESET))  
	{       
		framelength=((DMARxDescToGet->Status&ETH_DMARxDesc_FL)>>ETH_DMARxDesc_FrameLengthShift)-4;//�õ����հ�֡����(������4�ֽ�CRC)
 		frame.buffer = DMARxDescToGet->Buffer1Addr;//�õ����������ڵ�λ��
	}else framelength=ETH_ERROR;//����  
	frame.length=framelength; 
	frame.descriptor=DMARxDescToGet;  
	//����ETH DMAȫ��Rx������Ϊ��һ��Rx������
	//Ϊ��һ��buffer��ȡ������һ��DMA Rx������
	DMARxDescToGet=(ETH_DMADESCTypeDef*)(DMARxDescToGet->Buffer2NextDescAddr);   
	return frame;  
}
//����һ���������ݰ�
//FrameLength:���ݰ�����
//����ֵ:ETH_ERROR,����ʧ��(0)
//		ETH_SUCCESS,���ͳɹ�(1)
u8 ETH_Tx_Packet(u16 FrameLength)
{   
	//��鵱ǰ������,�Ƿ�����ETHERNET DMA(���õ�ʱ��)/CPU(��λ��ʱ��)
	if((DMATxDescToSet->Status&ETH_DMATxDesc_OWN)!=(u32)RESET)return ETH_ERROR;//����,OWNλ�������� 
 	DMATxDescToSet->ControlBufferSize=(FrameLength&ETH_DMATxDesc_TBS1);//����֡����,bits[12:0]
	DMATxDescToSet->Status|=ETH_DMATxDesc_LS|ETH_DMATxDesc_FS;//�������һ���͵�һ��λ����λ(1������������һ֡)
  	DMATxDescToSet->Status|=ETH_DMATxDesc_OWN;//����Tx��������OWNλ,buffer�ع�ETH DMA
	if((ETH->DMASR&ETH_DMASR_TBUS)!=(u32)RESET)//��Tx Buffer������λ(TBUS)�����õ�ʱ��,������.�ָ�����
	{ 
		ETH->DMASR=ETH_DMASR_TBUS;//����ETH DMA TBUSλ 
		ETH->DMATPDR=0;//�ָ�DMA����
	} 
	//����ETH DMAȫ��Tx������Ϊ��һ��Tx������
	//Ϊ��һ��buffer����������һ��DMA Tx������ 
	DMATxDescToSet=(ETH_DMADESCTypeDef*)(DMATxDescToSet->Buffer2NextDescAddr);    
	return ETH_SUCCESS;   
}

u32 ETH_GetCurrentTxBuffer(void)
{  
  return DMATxDescToSet->Buffer1Addr;
}





















