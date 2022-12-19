#include "CanBusManager.h"


unsigned int __stdcall ReceiveThread(void* p_this);

CanBusManager::CanBusManager()
{
    this->pBalObject = 0;
    this->lSocketNo = 0;
    this->lBusCtlNo = 0;
    this->pCanControl = 0;
    this->pCanChn = 0;
    this->lMustQuit = 0;
    this->hEventReader = 0;
    this->pReader = 0;
    this->pWriter = 0;
	this->receiveTrheadHandle = 0;
	memset(this->error_msg, '\0', sizeof(this->error_msg));
}

/**
  Selects the first CAN adapter.
  @param fUserSelect
    If this parameter is set to TRUE the functions display a dialog box which
    allows the user to select the device.
  @return
    VCI_OK on success, otherwise an Error code
*/
HRESULT CanBusManager::SelectDevice(LONG BusControlNo)
{
    HRESULT hResult =	VCI_OK;			// error code
    IVciDeviceManager*	pDevMgr = 0;	// device manager
    IVciEnumDevice*		pEnum = 0;		// enumerator handle
    VCIDEVICEINFO       sInfo;			// device info
    hResult = VciGetDeviceManager(&pDevMgr);
    if (hResult == VCI_OK) {
        hResult = pDevMgr->EnumDevices(&pEnum);
	}
	else {
		sprintf_s(this->error_msg, "\n%X : Enumerating device failed!", hResult);
		throw CanBusError(this->error_msg);
	}
    // retrieve information about the first
    // device within the device list
    if (hResult == VCI_OK) {
        hResult = pEnum->Next(1, &sInfo, NULL);
        // close the device list (no longer needed)
        pEnum->Release();
        pEnum = NULL;
    }
    if (hResult == VCI_OK) {    // open the first device via device manager and get the bal object
        IVciDevice* pDevice;
        hResult = pDevMgr->OpenDevice(sInfo.VciObjectId, &pDevice);
        if (hResult == VCI_OK) {
            hResult = pDevice->OpenComponent(CLSID_VCIBAL, IID_IBalObject, (void**)&pBalObject);
            pDevice->Release();
		}
		else {
			sprintf_s(this->error_msg, "\n%X : Cannot open device!", hResult);
			throw CanBusError(this->error_msg);
		}
	}
	else {
		sprintf_s(this->error_msg, "\n%X : No available device!", hResult);
		throw CanBusError(this->error_msg);
	}
    lBusCtlNo = BusControlNo;
    if (pDevMgr) { /// close device manager
        pDevMgr->Release();
        pDevMgr = NULL;
    }
    // This step is not necessary but shows how to get/check BAL features
    hResult = this->CheckBalFeatures(lBusCtlNo);

    return hResult;
}


/**
    Checks BAL features
    @param lCtrlNo
    controller number to check the features
    @return
    VCI_OK on success, otherwise an Error code
*/
HRESULT CanBusManager::CheckBalFeatures(LONG lCtrlNo)
{
    HRESULT hResult = E_FAIL;

    if (pBalObject) {
        // check if controller supports CANFD
        BALFEATURES features = { 0 };
        hResult = pBalObject->GetFeatures(&features);
        if (VCI_OK == hResult) {
            // check if controller number is valid
            if (lCtrlNo >= features.BusSocketCount) {
                // As we select the controller via the selection dialog, we should never get here.
				sprintf_s(this->error_msg, "\n%X : Invalid controller number. !", lCtrlNo);
				throw CanBusError(this->error_msg);
                //printf("\n Invalid controller number. !");
                return VCI_E_UNEXPECTED;
            }
            // check for the expected controller type
            if (VCI_BUS_TYPE(features.BusSocketType[lCtrlNo]) != VCI_BUS_CAN) {
                // Invalid controller type selected
				throw(CanBusError("Invalid controller type selected !"));
                //printf("\n Invalid controller type selected !");
                return VCI_E_UNEXPECTED;
            }
        } else {
			sprintf_s(this->error_msg, "\n pBalObject->GetFeatures failed: 0x%08lX !", hResult);
			throw CanBusError(this->error_msg);
        }
    }
    return hResult;
}

/**
    Opens the specified socket, creates a message channel, initializes
    and starts the CAN controller.
    @param dwCanNo
    Number of the CAN controller to open.
    @return
    VCI_OK on success, otherwise an Error code
    @note
    If <dwCanNo> is set to 0xFFFFFFFF, the function shows a dialog box
    which allows the user to select the VCI device and CAN controller.
*/
HRESULT CanBusManager::InitSocket(LONG lCtrlNo, ECAN_BAUD baud)
{
    HRESULT hResult = E_FAIL;
    byte CAN_BT0 = 0;
    byte CAN_BT1 = 0;
    switch (baud)
    {
    case ECAN_BAUD::CANBAUD_5K:
        CAN_BT0 = CAN_BT0_5KB; CAN_BT1 = CAN_BT1_5KB; break;
    case ECAN_BAUD::CANBAUD_10K:
        CAN_BT0 = CAN_BT0_10KB; CAN_BT1 = CAN_BT1_10KB; break;
    case ECAN_BAUD::CANBAUD_50K:
        CAN_BT0 = CAN_BT0_50KB; CAN_BT1 = CAN_BT1_50KB; break;
    case ECAN_BAUD::CANBAUD_100K:
        CAN_BT0 = CAN_BT0_100KB; CAN_BT1 = CAN_BT1_100KB; break;
    case ECAN_BAUD::CANBAUD_125K:
        CAN_BT0 = CAN_BT0_125KB; CAN_BT1 = CAN_BT1_125KB; break;
    case ECAN_BAUD::CANBAUD_250K:
        CAN_BT0 = CAN_BT0_250KB; CAN_BT1 = CAN_BT1_250KB; break;
    case ECAN_BAUD::CANBAUD_500K:
        CAN_BT0 = CAN_BT0_500KB; CAN_BT1 = CAN_BT1_500KB; break;
    case ECAN_BAUD::CANBAUD_800K:
        CAN_BT0 = CAN_BT0_800KB; CAN_BT1 = CAN_BT1_800KB; break;
    case ECAN_BAUD::CANBAUD_1000K:
        CAN_BT0 = CAN_BT0_1000KB; CAN_BT1 = CAN_BT1_1000KB; break;
    default:
        break;
    }
    if (pBalObject != NULL) {
        // check controller capabilities create a message channel
        ICanSocket* pCanSocket = 0;
        hResult = pBalObject->OpenSocket(lCtrlNo, IID_ICanSocket, (void**)&pCanSocket);
        if (hResult == VCI_OK) {
            // check capabilities
            CANCAPABILITIES capabilities = { 0 };
            hResult = pCanSocket->GetCapabilities(&capabilities);
            if (VCI_OK == hResult) {
                // This sample expects that standard and extended mode are
                // supported simultaneously. See use of
                // CAN_OPMODE_STANDARD | CAN_OPMODE_EXTENDED in InitLine() below
                if (capabilities.dwFeatures & CAN_FEATURE_STDANDEXT) {
                    // supports simultaneous standard and extended -> ok
                } else {
					hResult = VCI_E_NOT_SUPPORTED;
                    sprintf_s(this->error_msg, "\n Simultaneous standard and extended mode feature not supported !:%X", hResult);
					throw CanBusError(this->error_msg);
                }
            } else {
                // should not occurr
                sprintf_s(this->error_msg, "\n pCanSocket->GetCapabilities failed: 0x%08lX !", hResult);
				throw CanBusError(this->error_msg);
            }
            if (VCI_OK == hResult) { // create a message channel
                hResult = pCanSocket->CreateChannel(FALSE, &pCanChn);
            }
            pCanSocket->Release();
        }
        if (hResult == VCI_OK) { // initialize the message channel
            UINT16 wRxFifoSize = 1024;
            UINT16 wRxThreshold = 1;
            UINT16 wTxFifoSize = 128;
            UINT16 wTxThreshold = 1;

            hResult = pCanChn->Initialize(wRxFifoSize, wTxFifoSize);
            if (hResult == VCI_OK) {
                hResult = pCanChn->GetReader(&pReader);
                if (hResult == VCI_OK) {
                    pReader->SetThreshold(wRxThreshold);
                    hEventReader = CreateEvent(NULL, FALSE, FALSE, NULL);
                    pReader->AssignEvent(hEventReader);
                }
            }

            if (hResult == VCI_OK) {
                hResult = pCanChn->GetWriter(&pWriter);
                if (hResult == VCI_OK) {
                    pWriter->SetThreshold(wTxThreshold);
                }
            }
        }
        if (hResult == VCI_OK) { // activate the CAN channel
            hResult = pCanChn->Activate();
        }
        // Open the CAN control interface
        // During the programs lifetime we have multiple options:
        // 1) Open the control interface and keep it open
        //     -> No other programm is able to get the control interface and change the line settings
        // 2) Try to get the control interface and change the settings only when we get it
        //     -> Other programs can change the settings by getting the control interface
        if (hResult == VCI_OK) {
            hResult = pBalObject->OpenSocket(lCtrlNo, IID_ICanControl, (void**)&pCanControl);
            if (hResult == VCI_OK) { // initialize the CAN controller
                CANINITLINE init = {
                    CAN_OPMODE_STANDARD |
                    CAN_OPMODE_EXTENDED | CAN_OPMODE_ERRFRAME,      // opmode
                    0,                                              // bReserved
                    CAN_BT0, CAN_BT1                    // bt0, bt1
                };
                hResult = pCanControl->InitLine(&init);
                if (hResult != VCI_OK) {
					sprintf_s(this->error_msg, "\n pCanControl->InitLine failed: 0x%08lX !", hResult);
					throw CanBusError(this->error_msg);
                }
                if (hResult == VCI_OK) { // set the acceptance filter
                    hResult = pCanControl->SetAccFilter(CAN_FILTER_STD, CAN_ACC_CODE_ALL, CAN_ACC_MASK_ALL);
                    if (hResult == VCI_OK) { // set the acceptance filter
                        hResult = pCanControl->SetAccFilter(CAN_FILTER_EXT, CAN_ACC_CODE_ALL, CAN_ACC_MASK_ALL);
                    }
                    if (VCI_OK != hResult) {
						sprintf_s(this->error_msg, "\n pCanControl->SetAccFilter failed: 0x%08lX !", hResult);
						throw CanBusError(this->error_msg);
                    }
                    // SetAccFilter() returns VCI_E_INVALID_STATE if already controller is started. 
                    // We ignore this because the controller could already be started
                    // by another application.
                    if (VCI_E_INVALID_STATE == hResult) {
                        hResult = VCI_OK;
                    }
                }
                if (hResult == VCI_OK) { /// start the CAN controller
                    hResult = pCanControl->StartLine();
                    if (hResult != VCI_OK) {
						sprintf_s(this->error_msg, "\n pCanControl->StartLine failed: 0x%08lX !", hResult);
						throw CanBusError(this->error_msg);
                    }
                }
                //printf("\n Got Control interface. Settings applied !");
                receiveTrheadHandle = (HANDLE)_beginthreadex(0,0, &ReceiveThread, this, 0, 0);     /// start the receive thread
            } else {
                // If we can't get the control interface it is occupied by another application.
                // This means the application is in charge of the controller parameters.
                // We live with it and move on.
				sprintf_s(this->error_msg, "\n Control interface occupied. Settings not applied: 0x%08lX !", hResult);
				throw CanBusError(this->error_msg);
                hResult = VCI_OK;
            }
        }
    } else {
        hResult = VCI_E_INVHANDLE;
    }
    //DisplayError(NULL, hResult);
    return hResult;
}

/**
    Transmit message via PutDataEntry
*/
void CanBusManager::TransmitViaPutDataEntry()
{
    CANMSG  sCanMsg = { 0 };
    UINT payloadLen = 8;        // length of message payload
    sCanMsg.dwTime = 0;
    sCanMsg.dwMsgId = 0x100;    // CAN message identifier
    sCanMsg.uMsgInfo.Bytes.bType = CAN_MSGTYPE_DATA;
    // Flags:
    // srr = 1
    sCanMsg.uMsgInfo.Bytes.bFlags = CAN_MAKE_MSGFLAGS(CAN_LEN_TO_SDLC(payloadLen), 0, 1, 0, 0);
    // Flags2:
    // Set bFlags2 to 0
    sCanMsg.uMsgInfo.Bytes.bFlags2 = CAN_MAKE_MSGFLAGS2(0, 0, 0, 0, 0);
    for (UINT i = 0; i < payloadLen; i++) {
        sCanMsg.abData[i] = i;
    }
    // write a single CAN message into the transmit FIFO
    while (VCI_E_TXQUEUE_FULL == this->pWriter->PutDataEntry(&sCanMsg))
    {
        Sleep(1);
    }
}
/**
    Transmit CAN via Writer API (AcquireWrite/ReleaseWrite)
    with ID 0x100.
*/
void CanBusManager::TransmitViaWriter()
{
    // use the FIFO interface 
    // to write multiple messages
    UINT16  count = 0;
    PCANMSG pMsg;    
    UINT payloadLen = 8;        // length of message payload
    // aquire write access to FIFO
    HRESULT hr = pWriter->AcquireWrite((void**)&pMsg, &count);
    if (VCI_OK == hr) {
        // number of written messages needed for ReleaseWrite
        UINT16 written = 0;
        if (count > 0) {
            pMsg->dwTime = 0;
            pMsg->dwMsgId = 0x200;
            pMsg->uMsgInfo.Bytes.bType = CAN_MSGTYPE_DATA;
            // Flags:
            // srr = 1
            pMsg->uMsgInfo.Bytes.bFlags = CAN_MAKE_MSGFLAGS(CAN_LEN_TO_SDLC(payloadLen), 0, 1, 0, 0);
            // Flags2:
            // Set bFlags2 to 0 because FIFO memory will not be initialized by AquireWrite
            pMsg->uMsgInfo.Bytes.bFlags2 = CAN_MAKE_MSGFLAGS2(0, 0, 0, 0, 0);

            for (UINT i = 0; i < payloadLen; i++) {
                pMsg->abData[i] = i;
            }
            written = 1;
        }
        // release write access to FIFO
        hr = pWriter->ReleaseWrite(written);
        if (VCI_OK != hr) {
			sprintf_s(this->error_msg, "\n ReleaseWrite failed: 0x%08lX", hr);
			throw CanBusError(this->error_msg);
        }
    } else {
		sprintf_s(this->error_msg, "\n AcquireWrite failed: 0x%08lX", hr);
		throw CanBusError(this->error_msg);
    }
}

void CanBusManager::Write(CANMSG *canMsg) {
    // write a single CAN message into the transmit FIFO
    while (VCI_E_TXQUEUE_FULL == this->pWriter->PutDataEntry(canMsg))
    {
        Sleep(1);
    }
}

void CanBusManager::Finalize()
{
    if (pReader) {           /// release reader
        pReader->Release();
        pReader = 0;
    }
    if (pWriter) {            /// release writer
        pWriter->Release();
        pWriter = 0;
    }
    if (pCanChn) {           /// release channel interface
        pCanChn->Release();
        pCanChn = 0;
    }
    if (pCanControl) {       /// release CAN control object
        HRESULT hResult = pCanControl->StopLine();
        if (hResult != VCI_OK) {
			sprintf_s(this->error_msg, "\n pCanControl->StopLine failed: 0x%08lX !", hResult);
			throw CanBusError(this->error_msg);
        }
        hResult = pCanControl->ResetLine();
        if (hResult != VCI_OK) {
			sprintf_s(this->error_msg, "\n pCanControl->ResetLine failed: 0x%08lX !", hResult);
			throw CanBusError(this->error_msg);
        }
        pCanControl->Release();
        pCanControl = NULL;
    }
    if (pBalObject) {        /// release bal object
        pBalObject->Release();
        pBalObject = NULL;
    }
}

/**
    Print a message
*/
void CanBusManager::PrintMessage(PCANMSG pCanMsg)
{
    if (pCanMsg->uMsgInfo.Bytes.bType == CAN_MSGTYPE_DATA) {
        if (pCanMsg->uMsgInfo.Bits.rtr == 0) {   /// show data frames
            UINT j;
            // number of bytes in message payload
            UINT payloadLen = CAN_SDLC_TO_LEN(pCanMsg->uMsgInfo.Bits.dlc);
            printf("\nTime: %10u  ID: %3X %s  Len: %1u  Data:",
                pCanMsg->dwTime,
                pCanMsg->dwMsgId,
                (pCanMsg->uMsgInfo.Bits.ext == 1) ? "Ext" : "Std",
                payloadLen);
            // print payload bytes
            for (j = 0; j < payloadLen; j++) {
                printf(" %.2X", pCanMsg->abData[j]);
            }
        } else {
            printf("\nTime: %10u ID: %3X  DLC: %1u  Remote Frame",
                pCanMsg->dwTime,
                pCanMsg->dwMsgId,
                pCanMsg->uMsgInfo.Bits.dlc);
        }
    } else if (pCanMsg->uMsgInfo.Bytes.bType == CAN_MSGTYPE_INFO) {
        switch (pCanMsg->abData[0]) // show informational frames
        {
        case CAN_INFO_START: printf("\nCAN started..."); break;
        case CAN_INFO_STOP: printf("\nCAN stopped..."); break;
        case CAN_INFO_RESET: printf("\nCAN reseted..."); break;
        }
    } else if (pCanMsg->uMsgInfo.Bytes.bType == CAN_MSGTYPE_ERROR) {
        switch (pCanMsg->abData[0])// show error frames
        {
        case CAN_ERROR_STUFF: printf("\nstuff error...");          break;
        case CAN_ERROR_FORM: printf("\nform error...");           break;
        case CAN_ERROR_ACK: printf("\nacknowledgment error..."); break;
        case CAN_ERROR_BIT: printf("\nbit error...");            break;
        case CAN_ERROR_CRC: printf("\nCRC error...");            break;
        case CAN_ERROR_OTHER:
        default: printf("\nother error...");          break;
        }
    }
}

/**
    Process messages
    @param wLimit  max number of messages to process
    @return VCI_OK if more messages (may be) available
*/
HRESULT CanBusManager::ProcessMessages(WORD wLimit)
{
    // parameter checking
    if (!pReader) return E_UNEXPECTED;
    PCANMSG pCanMsg;
    // check if messages available
    UINT16  wCount = 0;
    HRESULT hr = pReader->AcquireRead((PVOID*)&pCanMsg, &wCount);
    if (VCI_OK == hr) {
        // limit number of messages to read
        if (wCount > wLimit) {
            wCount = wLimit;
        }
        UINT16 iter = wCount;
        while (iter) {
            //PrintMessage(pCanMsg);
            // Send callback
            this->m_cbNewCanMsg(pCanMsg);
            // process next VCI message
            iter--;
            pCanMsg++;
        }
        pReader->ReleaseRead(wCount);
    } else if (VCI_E_RXQUEUE_EMPTY == hr) {
        
    } else {        // return error code
        hr = VCI_OK; // ignore all other errors
    }
    return hr;
}

/**
    Receive thread.
    Note:
    Here console output in the receive thread is used for demonstration purposes.
    Using console outout in the receive thread involves Asynchronous
    Local Procedure Calls (ALPC) with the console host application (conhost.exe).
    So expect console output to be slow.
    Slow output can stall receive queue handling and finally lead
    to controller overruns on some CAN interfaces, even with moderate busloads
    (moderate = 1000 kBit/s, dlc=8, busload >= 30%).
    @param Param
    ptr on a user defined information
*/
unsigned int __stdcall ReceiveThread(void *p_this)
{
    CanBusManager* pBus = static_cast<CanBusManager*>(p_this);
    //UNREFERENCED_PARAMETER(Param);
    BOOL receiveSignaled = FALSE;
    BOOL moreMsgMayAvail = FALSE;
    WORD wLimit = 100;
    while (pBus->lMustQuit == 0) 
	{
        // if no more messages available wait 100msec for reader event
        if (!moreMsgMayAvail) {
            receiveSignaled = (WAIT_OBJECT_0 == WaitForSingleObject(pBus->hEventReader, wLimit));
        }
        // process messages while messages are available
        if (receiveSignaled || moreMsgMayAvail) {
            // try to process next chunk of messages (with max 100 msgs)
            moreMsgMayAvail = (VCI_OK == pBus->ProcessMessages(wLimit));
        }
    }
    _endthreadex(0);
    return 0;

}