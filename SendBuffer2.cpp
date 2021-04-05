#include "SendBuffer2.h"

#define NET_BUFFER_STATUS(x) NET_BUFFER_MINIPORT_RESERVED(x)[0]
#define NB_READY (PVOID)0
#define NB_PENDING (PVOID)1
#define NB_SENT (PVOID)2

#define NET_BUFFER_LIST_NB_COUNT(x) *((ULONG *)&(NET_BUFFER_LIST_MINIPORT_RESERVED(x)[0]))

void SEND_BUFFER2::Init(_In_ NDIS_HANDLE NdisMiniportHandle)
{
  KdPrint(("v SEND_BUFFER2::Init start\n"));

  _NdisMiniportHandle = NdisMiniportHandle;
  _ReadyHead = nullptr;
  _ReadyTail = nullptr;
  _PendingHead = nullptr;
  _PendingTail = nullptr;
  _NbReady = nullptr;
  _NbPending = nullptr;
  _SendOkCount = 0;
  _IsSmthSent = false;
  NdisInitializeReadWriteLock(&_ReadyRwLock);
  NdisInitializeReadWriteLock(&_PendingRwLock);
  NdisAllocateSpinLock(&_SendOkLock);

  KdPrint(("v SEND_BUFFER2::Init end\n"));
}

void MarkNblReadyAndGetLast(_Inout_ PNET_BUFFER_LIST NetBufferList, _Outptr_ PNET_BUFFER_LIST *pLastNbl)
{
  KdPrint(("v SEND_BUFFER2::MarkNblReadyAndgetLast start\n"));
  ASSERTMSG("v SEND_BUFFER2::MarkNblReadyAndGetLast NetBufferList == nullptr\n", NetBufferList != nullptr);

  while (NET_BUFFER_LIST_NEXT_NBL(NetBufferList) != nullptr)
  {
    PNET_BUFFER NetBuffer = NET_BUFFER_LIST_FIRST_NB(NetBufferList);
    ULONG Count = 0;

    while (NetBuffer != nullptr)
    {
      NET_BUFFER_STATUS(NetBuffer) = NB_READY; 
      Count++;
      NetBuffer = NET_BUFFER_NEXT_NB(NetBuffer);
    }

    NET_BUFFER_LIST_NB_COUNT(NetBufferList) = Count;
    NetBufferList = NET_BUFFER_LIST_NEXT_NBL(NetBufferList);
  }

  ASSERTMSG("v SEND_BUFFER2::MarkNblReadyAndGetLast NetBufferList(in end) == nullptr\n", NetBufferList != nullptr);
  *pLastNbl = NetBufferList;
}

void SEND_BUFFER2::AddNetBufferList(_Inout_ PNET_BUFFER_LIST NetBufferList)
{
  KdPrint(("v SEND_BUFFER2::AddNetBufferList start\n"));
  ASSERTMSG("v SEND_BUFFER2::AddNetBufferList NetBufferList == nullptr\n", NetBufferList != nullptr);

  PNET_BUFFER_LIST LastNbl;
  MarkNblReadyAndGetLast(NetBufferList, &LastNbl);

  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&_ReadyRwLock, true, &LockState);
  {
    if (_ReadyHead == nullptr)
      _ReadyHead = NetBufferList;

    if (_ReadyTail != nullptr)
      NET_BUFFER_LIST_NEXT_NBL(_ReadyTail) = NetBufferList;

    _ReadyTail = LastNbl;
  }
  NdisReleaseReadWriteLock(&_ReadyRwLock, &LockState);
}

GET_NB_RES SEND_BUFFER2::GetNetBuffer(_Outptr_result_maybenull_ PNET_BUFFER *pNetBuffer)
{
  KdPrint(("v SEND_BUFFER2::GetNetBuffer start\n"));
  ASSERTMSG("v SEND_BUFFER2::GetNetBuffer pNetBuffer == nullptr\n", pNetBuffer != nullptr);

  GET_NB_RES Result = GET_NB_RES::Success;

  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&_PendingRwLock, true, &LockState);
  {
    _IsSmthSent = true;

    if (_NbReady == nullptr)
    {
      PNET_BUFFER_LIST Tmp = nullptr;
      LOCK_STATE LockState2;
      NdisAcquireReadWriteLock(&_ReadyRwLock, true, &LockState2);
      {
        if (_ReadyHead == nullptr)
        {
          Result = GET_NB_RES::Nothing;
        }
        else
        {
          Tmp = _ReadyHead;
          _ReadyHead = NET_BUFFER_LIST_NEXT_NBL(_ReadyHead);
          if (_ReadyHead == nullptr)
            _ReadyTail = nullptr;
        }
      }
      NdisReleaseReadWriteLock(&_ReadyRwLock, &LockState2);

      if (Result != GET_NB_RES::Nothing)
      {
        NET_BUFFER_LIST_NEXT_NBL(Tmp) = nullptr;
        if (_PendingTail != nullptr)
          NET_BUFFER_LIST_NEXT_NBL(_PendingTail) = Tmp;
        _PendingTail = Tmp;
        if (_PendingHead == nullptr)
          _PendingHead = Tmp;

        _NbReady = NET_BUFFER_LIST_FIRST_NB(_PendingTail);
      }
    }

    if (Result != GET_NB_RES::Nothing)
    {
      NET_BUFFER_STATUS(_NbReady) = NB_PENDING;
      *pNetBuffer = _NbReady;
      if (_NbPending == nullptr)
        _NbPending = _NbReady;

      _NbReady = NET_BUFFER_NEXT_NB(_NbReady);

    }
    else
    {
      *pNetBuffer = nullptr;
    }
  }
  NdisReleaseReadWriteLock(&_PendingRwLock, &LockState);

  return Result;
}

void SEND_BUFFER2::NetBufferConfirmSent(_Inout_ PNET_BUFFER NetBuffer)
{
  KdPrint(("v SEND_BUFFER2::NetBufferConfirmSent start\n"));
  ASSERTMSG("v SEND_BUFFER2::NetBufferConfirmSent NetBuffer == nullptr\n", NetBuffer != nullptr);

  PNET_BUFFER_LIST SendList = nullptr;
  ULONG Count = 0;

  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&_PendingRwLock, true, &LockState);
  {
    if (_PendingHead != nullptr)
    {
      NET_BUFFER_STATUS(NetBuffer) = NB_SENT;
      while (_NbPending != nullptr && NET_BUFFER_STATUS(_NbPending) != NB_PENDING)
      {
        if (NET_BUFFER_NEXT_NB(_NbPending) != nullptr)
          _NbPending = NET_BUFFER_NEXT_NB(_NbPending);
        else
        {
          if (SendList != nullptr)
            NET_BUFFER_LIST_NEXT_NBL(SendList) = _PendingHead;
          SendList = _PendingHead;
          NET_BUFFER_LIST_STATUS(SendList) = NDIS_STATUS_SUCCESS;
          Count += NET_BUFFER_LIST_NB_COUNT(SendList);
          _PendingHead = NET_BUFFER_LIST_NEXT_NBL(_PendingHead);         
          NET_BUFFER_LIST_NEXT_NBL(SendList) = nullptr;

          if (_PendingHead == nullptr)
          {
            _PendingTail = nullptr;
            _NbPending = nullptr;
          }
          else
          {
            _NbPending = NET_BUFFER_LIST_FIRST_NB(_PendingHead);
          }
        }
      }
    }
  }
  NdisReleaseReadWriteLock(&_PendingRwLock, &LockState);

  NdisInterlockedAddUlong(&_SendOkCount, Count, &_SendOkLock);

  if (SendList != nullptr)
    NdisMSendNetBufferListsComplete(_NdisMiniportHandle, SendList, 0);
}

void SEND_BUFFER2::CancelSend(_In_ PVOID CancelId)
{
  KdPrint(("v SEND_BUFFER2::CancelSend start\n"));

  PNET_BUFFER_LIST CancelList = nullptr;

  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&_ReadyRwLock, true, &LockState);
  {
    PNET_BUFFER_LIST Nbl = _ReadyHead;

    while (Nbl != nullptr)
    {
      if (NDIS_GET_NET_BUFFER_LIST_CANCEL_ID(Nbl) == CancelId)
      {
        NET_BUFFER_LIST_STATUS(Nbl) = NDIS_STATUS_SEND_ABORTED;

        if (CancelList != nullptr)
          NET_BUFFER_LIST_NEXT_NBL(CancelList) = Nbl;
        CancelList = Nbl;

        if (Nbl == _ReadyHead)
          _ReadyHead = NET_BUFFER_LIST_NEXT_NBL(_ReadyHead);

        Nbl = NET_BUFFER_LIST_NEXT_NBL(Nbl);
        NET_BUFFER_LIST_NEXT_NBL(CancelList) = nullptr;
      }
    }

    if (_ReadyHead == nullptr)
      _ReadyTail = nullptr;
  }
  NdisReleaseReadWriteLock(&_ReadyRwLock, &LockState);
  
  if (CancelList != nullptr)
    NdisMSendNetBufferListsComplete(_NdisMiniportHandle, CancelList, 0);
}

void NblsSetStatus(_Inout_ PNET_BUFFER_LIST Nbl, _In_ NDIS_STATUS Status)
{
  while (Nbl != nullptr)
  {
    NET_BUFFER_LIST_STATUS(Nbl) = Status;
    Nbl = NET_BUFFER_LIST_NEXT_NBL(Nbl);
  }
}

void SEND_BUFFER2::DropIfNotSent()
{
  KdPrint(("v SEND_BUFFER2::DropIfNotSent\n"));

  if (_IsSmthSent)
  {
    _IsSmthSent = false;
    return;
  }

  PNET_BUFFER_LIST DropList = nullptr;

  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&_ReadyRwLock, true, &LockState);
  {
    DropList = _ReadyHead;
    _ReadyHead = nullptr;
    _ReadyTail = nullptr;
  }
  NdisReleaseReadWriteLock(&_ReadyRwLock, &LockState);

  if (DropList != nullptr)
  {
    NblsSetStatus(DropList, NDIS_STATUS_PAUSED);
    NdisMSendNetBufferListsComplete(_NdisMiniportHandle, DropList, 0);
  }

  NdisAcquireReadWriteLock(&_PendingRwLock, true, &LockState);
  {
    DropList = _PendingHead;
    _PendingHead = nullptr;
    _PendingTail = nullptr;
    _NbReady = nullptr;
    _NbPending = nullptr;
  }
  NdisReleaseReadWriteLock(&_PendingRwLock, &LockState);

  if (DropList != nullptr)
  {
    NblsSetStatus(DropList, NDIS_STATUS_PAUSED);
    NdisMSendNetBufferListsComplete(_NdisMiniportHandle, DropList, 0);
  }
}
/*
void *SEND_BUFFER2::operator new(_In_ SIZE_T Size)
{
  if (Size > (SIZE_T)MAXUINT)
    return nullptr;

  return NdisAllocateMemoryWithTagPriority(NdisMiniportDriverHandle, (ULONG)Size, 'NPVp', NormalPoolPriority);
}

void SEND_BUFFER2::operator delete(_Inout_ void *Ptr, _In_ SIZE_T Size)
{
  if (Ptr == nullptr)
    return;

  if (Size > (SIZE_T)MAXUINT)
    return;

  NdisFreeMemory(Ptr, (ULONG)Size, NormalPoolPriority); 
}
*/