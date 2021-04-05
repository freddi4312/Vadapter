#pragma once

#include "NdisWithParams.h"

enum class GetNbRes
{
  Success,
  Nothing
};

class SEND_BUFFER
{
private:
  NDIS_RW_LOCK _RwLock;
  PNET_BUFFER_LIST _ListHead;
  PNET_BUFFER_LIST *_ListTail;
  PNET_BUFFER _CurrentReadyNetBuffer;
  PNET_BUFFER_LIST _CurrentReadyNetBufferList;
  PNET_BUFFER _CurrentPendingNetBuffer;
  NDIS_HANDLE _NdisMiniportHandle;
  NDIS_SPIN_LOCK _SendOkLock;
  ULONG _CountSendOk;

public:
  SEND_BUFFER() = delete; 
  void Init(_In_ NDIS_HANDLE NdisMiniportHandle);
  void AddNetBufferList(_Inout_ PNET_BUFFER_LIST NetBufferList);
  GetNbRes GetNetBuffer(_Outptr_result_maybenull_ PNET_BUFFER *NetBuffer);
  void NetBufferConfirmSent(_Inout_ PNET_BUFFER NetBuffer);
  void DropPaused();
  void DropReset();
  //void CancelSend(_In_ PVOID CancelId);
  ULONG CountSendOk()
  {
    return _CountSendOk;
  }
  ~SEND_BUFFER() {}

private:
  void DropWithStatus(_In_ NDIS_STATUS Status);
  void MoveNextCurrentReady();
  //void RemoveNetBufferListToCancelList(_Inout_ PNET_BUFFER_LIST *ListPtr, _Inout_ PNET_BUFFER_LIST *CanceledList);
};