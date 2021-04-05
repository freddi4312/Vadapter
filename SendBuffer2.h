#pragma once

#include "NdisWithParams.h"

enum class GET_NB_RES
{
  Success,
  Nothing
};

class SEND_BUFFER2
{
private:
  NDIS_HANDLE _NdisMiniportHandle;
  PNET_BUFFER_LIST _ReadyHead;
  PNET_BUFFER_LIST _ReadyTail;
  PNET_BUFFER_LIST _PendingHead;
  PNET_BUFFER_LIST _PendingTail;
  PNET_BUFFER _NbReady;
  PNET_BUFFER _NbPending;
  NDIS_RW_LOCK _ReadyRwLock;
  NDIS_RW_LOCK _PendingRwLock;
  ULONG _SendOkCount;
  NDIS_SPIN_LOCK _SendOkLock;
  bool _IsSmthSent;

public:
  SEND_BUFFER2() { NdisZeroMemory(this, sizeof(SEND_BUFFER2)); }
  void Init(_In_ NDIS_HANDLE NdisMiniportHandle);
  void AddNetBufferList(_Inout_ PNET_BUFFER_LIST NetBufferList);
  GET_NB_RES GetNetBuffer(_Outptr_result_maybenull_ PNET_BUFFER *NetBuffer);
  void NetBufferConfirmSent(_Inout_ PNET_BUFFER NetBuffer);
  void DropIfNotSent();
  void CancelSend(_In_ PVOID CancelId);
  ULONG CountSendOk()
  {
    return _SendOkCount;
  }
  ~SEND_BUFFER2() {}
  /*
private:
  void *operator new(_In_ SIZE_T Size);
  void operator delete(_Inout_ void *Ptr, _In_ SIZE_T Size);
  */
};