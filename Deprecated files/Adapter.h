#pragma once


#include "NdisWithParams.h"
#include "SendBuffer2.h"
#include "ReceiveBuffer.h"


enum class ADAPTER_INIT
{
  Success,
  BadAlloc
};

class ADAPTER
{
public:
  NDIS_HANDLE _NdisMiniportHandle;
  SEND_BUFFER2 _SendBuffer;
  RECEIVE_BUFFER _ReceiveBuffer;


public:
  ADAPTER_INIT Init(_In_ NDIS_HANDLE NdisMiniportHandle, _In_ PMAC_ADDR MacAddr);
  NDIS_STATUS Pause(_In_ PNDIS_MINIPORT_PAUSE_PARAMETERS PauseParameters);
  NDIS_STATUS Restart(_In_ PNDIS_MINIPORT_RESTART_PARAMETERS RestartParameters);
  void CancelSend(_In_ PVOID CancelId);
  void ReturnNetBufferLists(_Inout_ PNET_BUFFER_LIST NetBufferList);
  void SendNetBufferLists(_Inout_ PNET_BUFFER_LIST NetBufferList);
  ULONG CountSendOk()
  {
    return _SendBuffer.CountSendOk();
  }
  ULONG CountReceiveOk()
  {
    return _ReceiveBuffer.CountReceiveOk();
  }
  void SetFilter(_In_ ULONG Filter)
  {
    _ReceiveBuffer.SetFilter(Filter);
  }
  void free()
  {
    _SendBuffer.free();
    _ReceiveBuffer.free();
  }
  void SetMulticast(_In_ PVOID MacAddrs, _In_ ULONG Size)
  {
    _ReceiveBuffer.SetMulticast(MacAddrs, Size);
  }
  void DropIfNotSent()
  {
    _SendBuffer.DropIfNotSent();
  }


};