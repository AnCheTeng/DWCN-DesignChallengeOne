#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "autonet.h"

#define TABLE_LENGTH 24
#define Device_ID 0x0809

void DesignChallengeOneProtocol(void);
void lightLED(uint8_t);


int main(void)
{
	DesignChallengeOneProtocol();
}

void DesignChallengeOneProtocol(void){
	uint8_t Rx_msg[TABLE_LENGTH]={0};
	uint8_t Tx_msg[TABLE_LENGTH]={0};
	uint8_t Tx_length = TABLE_LENGTH;

  uint8_t rcvd_length;
	uint8_t rcvd_rssi;
	uint8_t payload[TABLE_LENGTH]={0};
	uint8_t payload_length = TABLE_LENGTH;

	uint8_t ID = 1;
	uint8_t Table[TABLE_LENGTH]={0};
	uint8_t IDindex;
	uint8_t State = 0;
	uint8_t TableFullCheck;
	uint8_t i;
	uint8_t messageCount=0;
	
	uint8_t Type;
	uint16_t Addr;
	uint8_t radio_channel;
	uint16_t radio_panID;
	
	Type = Type_Light;
	Addr = Device_ID;
	radio_channel = 25;
	radio_panID = 0x0008;

	Initial(Addr,Type, radio_channel, radio_panID);
	setTimer(1,100,UNIT_MS);
	setTimer(2,3000,UNIT_MS);

	//===================================Network COnfiguration===================================
	// Sensing for other's information
	while(1){

		// If sense some ID greater than itself
		if(RF_Rx(Rx_msg,&rcvd_length,&rcvd_rssi)){
			getPayloadLength(&payload_length,Rx_msg);
			getPayload(payload,Rx_msg,payload_length);
			if(payload[0]+1>ID){
				ID = payload[0]+1;
			}
		}
		
		// If timeout reached, stop sensing and close the timer 1 and 2
		if(checkTimer(2)) {
			setTimer(1,0,UNIT_MS);
			setTimer(2,0,UNIT_MS);
			break;
		}
	}
	
	// Light the LED for ID times
	lightLED(ID);
	
	// If ID is not 8, broadcast self-ID
	if(ID!=8){
		
		
		setTimer(1,100,UNIT_MS);
		while(1){
			
			// Broadcast self-ID
			if(checkTimer(1)){
				Tx_msg[0] = ID;
				RF_Tx(0xffff,Tx_msg,Tx_length);
			}
		
			// If received greater ID number, stop broadcasting
			if(RF_Rx(Rx_msg,&rcvd_length,&rcvd_rssi)){
				getPayloadLength(&payload_length,Rx_msg);
				getPayload(payload,Rx_msg,payload_length);
				if(payload[0]>ID){
					break;
				}
			}
		}	
	}
	
	// Stop broadcasting and turn on the empty Table flag
	setGPIO(1,1);
	
	//===================================Network COnfiguration===================================

	
	//====================================Many-to-One Routing====================================

	// Initiate the Table
	IDindex = (ID-1)*3;
	Table[IDindex]=ID;
	Table[IDindex+1]=Addr>>8;
	Table[IDindex+2]=Addr;

	Delay(5000);
	
	// If ID=8, send "Starting Message" to UART and change state to State-1
	if(ID==8){
		sprintf((char *)Tx_msg,"%s\n","====Routing Start====");
		COM1_Tx(Tx_msg,23);
		RF_Tx(0xffff,Table,Tx_length);
		State = 1;
	}
	
	setTimer(1,200,UNIT_MS);

	while(1){
		
		// Count the received packet
		if(RF_Rx(Rx_msg,&rcvd_length,&rcvd_rssi)){
			messageCount++;
			getPayloadLength(&payload_length,Rx_msg);
			getPayload(payload,Rx_msg,payload_length);		
		}

		
		if(State==0){
			// StateOneHandler
			// If received packet doesn't contain self-ID
			
			if(payload[IDindex+2]==0){
				for(i=ID; i<8; i++){
					// If received trigger message, broadcast Table and change to State-1
					if(payload[i*3]!=0){
						RF_Tx(0xffff,Table,Tx_length);
						State = 1;
						break;
					}
				}
			}
			
		} else {
			//StateTwoHandler
			
			// If Table is not full and timeout is reached, broadcast the Table
			if(checkTimer(1)){
				RF_Tx(0xffff,Table,Tx_length);
			}
			
			// Check received Table and store the message received
			TableFullCheck = 0;
			for(i=0; i<ID-1; i++){
				if(payload[i*3+1]!=0 || payload[i*3+2]!=0){
					Table[i*3]=payload[i*3];
					Table[i*3+1]=payload[i*3+1];
					Table[i*3+2]=payload[i*3+2];
				}
				
				//Check if Table is full or not
				if(Table[i*3]==0){
					TableFullCheck++;
				}
			}
			
			// Retransmit Table when Table is full
			if(TableFullCheck==0){
				setGPIO(1,0);
				
				if(ID==8){
					
					// Print the Table result to UART
					for(i=0; i<8; i++){
						sprintf((char *)Tx_msg, "ID:%d, MAC:0x0%d0%d\n", Table[i*3], Table[i*3+1], Table[i*3+2]);
						COM1_Tx(Tx_msg,17);
					}
					// Print the total packet ID-8 received
					sprintf((char *)Tx_msg, "Message Count:%d\n", messageCount);
					COM1_Tx(Tx_msg,17);
				}
				
				// Clean up the Table
				RF_Tx(0xffff,Table,Tx_length);
				for(i=0; i<TABLE_LENGTH; i++){
					Table[i]=0;
				}
				
				// Initiate the Table
				Table[IDindex]=ID;
				Table[IDindex+1]=Addr>>8;
				Table[IDindex+2]=Addr;
				
				State = 0;
			}
		}
	}

	//====================================Many-to-One Routing====================================

}


// Light the LED for ID times
void lightLED(uint8_t ID){
	uint8_t i=0;
	uint8_t Tx_msg[TABLE_LENGTH]={0};
	uint8_t Tx_length = TABLE_LENGTH;
	
	for(i=0; i<ID; i++){
		setGPIO(1,1);
		Delay(200);
		
		Tx_msg[0] = ID;
		RF_Tx(0xffff,Tx_msg,Tx_length);
		
		setGPIO(1,0);
		Delay(200);
		
		RF_Tx(0xffff,Tx_msg,Tx_length);
	}
}
