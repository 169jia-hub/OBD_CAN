#ifndef __APP_CAN_H
#define __APP_CAN_H

void App_CAN_Init(void);
void CAN_Tx_Task(void *pvParameters);
void CAN_Rx_Task(void *pvParameters);

#endif
