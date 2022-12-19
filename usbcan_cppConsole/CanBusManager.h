#ifndef __CAN_BUS_MANAGER_H__
#define __CAN_BUS_MANAGER_H__

#include "vcisdk.h"

#include <process.h>
#include <stdio.h>
#include <conio.h>
#include <iostream>
#include <functional>
#include <string>
#include <exception>

enum class ECAN_BAUD {
	CANBAUD_5K,
	CANBAUD_10K,
	CANBAUD_50K,
	CANBAUD_100K,
	CANBAUD_125K,
	CANBAUD_250K,
	CANBAUD_500K,
	CANBAUD_800K,
	CANBAUD_1000K
};
class CanBusError : public std::runtime_error
{
public:
	CanBusError(const char* what) : runtime_error(what) {}
};

#pragma once
class CanBusManager
{
public:
	LONG			lMustQuit;      // quit flag for the receive thread
	HANDLE			hEventReader;
	PFIFOREADER		pReader;

	CanBusManager();

	std::function<void(PCANMSG)> m_cbNewCanMsg;		// Callback function to be called on new New CAN message
	void addNewCanMsgHandler(std::function<void(PCANMSG)> callback) {
		m_cbNewCanMsg = callback;
	}

	HRESULT SelectDevice(LONG BusControlNo);
	HRESULT ProcessMessages(WORD wLimit);
	HRESULT InitSocket(LONG lCtrlNo, ECAN_BAUD baud);
	void TransmitViaPutDataEntry();
	void TransmitViaWriter();
	void Write(CANMSG *canMsg);
	void PrintMessage(PCANMSG pCanMsg);
	void Finalize();

private:
	IBalObject*		pBalObject;     // bus access object
	LONG			lSocketNo;		// socket number
	LONG			lBusCtlNo;		// controller number
	ICanControl*	pCanControl;    // control interface
	ICanChannel*	pCanChn;        // channel interface
	PFIFOWRITER		pWriter;
	HRESULT			CheckBalFeatures(LONG lCtrlNo);
	HANDLE			receiveTrheadHandle;
	char error_msg[256];
};

#endif