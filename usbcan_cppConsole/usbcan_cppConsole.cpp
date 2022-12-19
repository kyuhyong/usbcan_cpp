/**

  Demo application and wrapper for the IXXAT interface based VCI-API.

  @note
	This demo demonstrates the following VCI features
	- controller selection
	- socket initialization with desired communication speed
	- creation of a can message and transmission
	- reception of CAN messages via callback funtion

*/
//////////////////////////////////////////////////////////////////////////
// Original code was writeen by HMS Technology Center Ravensburg GmbH
// Wrapped and modified by KHY (kyuhyong@gmail.com)
//////////////////////////////////////////////////////////////////////////

// usbcan_cppConsole.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <process.h>
#include <stdio.h>
#include <conio.h>
#include <iostream>
#include <functional>
#include "CanBusManager.h"

void CallbackNewCANMsg(PCANMSG pcanMsg);

CanBusManager can;

int main()
{
	// Attach callback function to the object
	can.addNewCanMsgHandler(CallbackNewCANMsg);
    printf(" >>>> VCI - C-API Example V2.1 <<<<\n");
	printf(" >>>> C++ CanBusManager library demo <<<<\n");
	try {
		printf("\n Select Adapter...");
		can.SelectDevice(0);
		printf("\n Initialize CAN with 1000 kBaud");
		can.InitSocket(0, ECAN_BAUD::CANBAUD_1000K);
		printf("\n Initialize CAN............ OK !");
		printf("\n 'w'-key sends message with ID 125H Extended frame via Write()");
		printf("\n 'Esc'-key quit the application\n");
		while (1)
		{
			// wait for the user to press a key
			int chKey = _getch();
			// when the key is 'w' or 'W' the send a CAN message
			if ((chKey == 'w') || (chKey == 'W')) {
				CANMSG canMsg = { 0 };
				canMsg.dwTime = 0;
				canMsg.dwMsgId = 0x125;
				canMsg.uMsgInfo.Bytes.bType = CAN_MSGTYPE_DATA;
				// Set bFlags DLC, Data Overrun = 0, Self Reception =1, Remote request= 0, Extended frame = 1
				canMsg.uMsgInfo.Bytes.bFlags = CAN_MAKE_MSGFLAGS(CAN_LEN_TO_SDLC(4), 0, 1, 0, 1);
				// Set bFlags2 to 0 because FIFO memory will not be initialized by AquireWrite
				canMsg.uMsgInfo.Bytes.bFlags2 = CAN_MAKE_MSGFLAGS2(0, 0, 0, 0, 0);
				canMsg.abData[0] = 0x00;
				canMsg.abData[1] = 0x01;
				canMsg.abData[2] = 0x02;
				canMsg.abData[3] = 0x03;
				can.Write(&canMsg);
			}
			// when the user press the ESC key then end the program
			if (chKey == VK_ESCAPE)
				break;
			Sleep(1);
		}
		can.Finalize();
	}
	catch (CanBusError err) {
		std::cout << err.what() << std::endl;
	}
}

/// <summary>
/// Whenever a new can message received
/// </summary>
/// <param name="pcanMsg">Pointer to CANMSG received</param>
void CallbackNewCANMsg(PCANMSG pcanMsg) 
{
	// Just print the received can message
	can.PrintMessage(pcanMsg);
}
