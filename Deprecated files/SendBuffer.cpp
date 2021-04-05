#include "SendBuffer.h"


#define NET_BUFFER_STATUS(x) NET_BUFFER_MINIPORT_RESERVED(x)[0]
#define NB_READY (PVOID)0
#define NB_SENDING (PVOID)1
#define NB_SENT (PVOID)2

#define NET_BUFFER_LIST_NB_COUNT(x) *((ULONG *)&(NET_BUFFER_LIST_MINIPORT_RESERVED(x)[0]))

void SEND_BUFFER::Init(_In_ NDIS_HANDLE NdisMiniportHandle)
{
  KdPrint(("v SEND_BUFFER::Init start\n"));

  NdisInitializeReadWriteLock(&_RwLock);
  NdisAllocateSpinLock(&_SendOkLock);
  _ListHead = nullptr;
  _ListTail = nullptr;
  _CurrentReadyNetBuffer = nullptr;
  _CurrentReadyNetBufferList = nullptr;
  _CurrentPendingNetBuffer = nullptr;
  _NdisMiniportHandle = NdisMiniportHandle;
  _CountSendOk = 0;

  KdPrint(("v SEND_BUFFER::Init end\n"));
}

void GetNetBufferListLastAndSetListReady(_In_ PNET_BUFFER_LIST NetBufferList, _Outptr_ PNET_BUFFER_LIST **Last)
{
  ASSERTMSG("v SEND_BUFFER::GetNetBufferListLast NetBufferList == nullptr\n", NetBufferList != nullptr);
  ASSERTMSG("v SEND_BUFFER::GetNetBufferListLast Last == nullptr\n", Last != nullptr);

  ULONG CountNb = 0;
  PNET_BUFFER_LIST *LastNetBufferList = &NET_BUFFER_LIST_NEXT_NBL(NetBufferList);
  while (*LastNetBufferList != nullptr)
  {
    PNET_BUFFER NetBuffer = NET_BUFFER_LIST_FIRST_NB(*LastNetBufferList);
    CountNb = 0;
    while (NetBuffer != nullptr)
    {
      NET_BUFFER_STATUS(NetBuffer) = NB_READY;
      CountNb++;
      NetBuffer = NET_BUFFER_NEXT_NB(NetBuffer);
    }

    NET_BUFFER_LIST_NB_COUNT(*LastNetBufferList) = CountNb;
    LastNetBufferList = &NET_BUFFER_LIST_NEXT_NBL(*LastNetBufferList);
  }
  KdPrint(("v SEND_BUFFER::AddNetBufferList Found list end\n"));

  *Last = LastNetBufferList;
  ASSERTMSG("v SEND_BUFFER::GetNetBufferListLast *Last == nullptr\n", *Last != nullptr);
}

void SEND_BUFFER::AddNetBufferList(_Inout_ PNET_BUFFER_LIST NetBufferList)
{
  KdPrint(("v SEND_BUFFER::AddNetBufferList start\n"));
  ASSERTMSG("v SEND_BUFFER::AddNetBufferList NetBufferList == nullptr\n", NetBufferList != nullptr);

  PNET_BUFFER_LIST *LastNetBufferList = nullptr;
  GetNetBufferListLastAndSetListReady(NetBufferList, &LastNetBufferList);

  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&_RwLock, true, &LockState);
  {
    KdPrint(("v SEND_BUFFER::AddNetBufferList Locked\n"));
    if (_ListHead == nullptr)
    {
      _ListHead = NetBufferList;
    }
    
    if (_ListTail != nullptr)
    {
      *_ListTail = NetBufferList;
    }
    _ListTail = LastNetBufferList;

    if (_CurrentReadyNetBufferList == nullptr)
    {
      _CurrentReadyNetBufferList = NetBufferList;
      _CurrentReadyNetBuffer = NET_BUFFER_LIST_FIRST_NB(NetBufferList);
    }

    if (_CurrentPendingNetBuffer == nullptr)
    {
      _CurrentPendingNetBuffer = NET_BUFFER_LIST_FIRST_NB(NetBufferList);
    }

  }
  NdisReleaseReadWriteLock(&_RwLock, &LockState);

  KdPrint(("v SEND_BUFFER::AddNetBufferList Unlocked\n"));
  KdPrint(("v SEND_BUFFER::AddNetBufferList end\n"));
}

void SEND_BUFFER::MoveNextCurrentReady()
{
  ASSERTMSG("v SEND_BUFFER::MoveNextCurrentReady _CurrentReadyNetBufferList == nullptr\n", _CurrentReadyNetBufferList != nullptr);

  if (NET_BUFFER_NEXT_NB(_CurrentReadyNetBuffer) != nullptr)
  {
    _CurrentReadyNetBuffer = NET_BUFFER_NEXT_NB(_CurrentReadyNetBuffer);
  }
  else if (NET_BUFFER_LIST_NEXT_NBL(_CurrentReadyNetBufferList) != nullptr)
  {
    _CurrentReadyNetBufferList = NET_BUFFER_LIST_NEXT_NBL(_CurrentReadyNetBufferList);
    _CurrentReadyNetBuffer = NET_BUFFER_LIST_FIRST_NB(_CurrentReadyNetBufferList);
  }
  else
  {
    _CurrentReadyNetBufferList = nullptr;
    _CurrentReadyNetBuffer = nullptr;
  }
}

GetNbRes SEND_BUFFER::GetNetBuffer(_Outptr_result_maybenull_ PNET_BUFFER *pNetBuffer)
{
  KdPrint(("v SEND_BUFFER::GetNetBuffer start\n"));
  ASSERTMSG("v SEND_BUFFER::GetNetBuffer pNetBuffer == nullptr\n", pNetBuffer != nullptr);

  GetNbRes Result;

  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&_RwLock, true, &LockState);
  {
    KdPrint(("v SEND_BUFFER::GetNetBuffer Locked\n"));
    if (_CurrentReadyNetBuffer == nullptr)
    {
      KdPrint(("v SEND_BUFFER::GetNetBuffer no net buffer to send\n"));
      *pNetBuffer = nullptr;
      Result = GetNbRes::Nothing;
    }
    else
    {
      NET_BUFFER_STATUS(_CurrentReadyNetBuffer) = NB_SENDING;
      *pNetBuffer = _CurrentReadyNetBuffer;

      MoveNextCurrentReady();

      Result = GetNbRes::Success;
    }
  }
  NdisReleaseReadWriteLock(&_RwLock, &LockState);

  KdPrint(("v SEND_BUFFER::GetNetBuffer Unlocked\n"));
  KdPrint(("v SEND_BUFFER::GetNetBuffer end\n"));

  return Result;
}

void SEND_BUFFER::NetBufferConfirmSent(_Inout_ PNET_BUFFER NetBuffer)
{
  KdPrint(("v SEND_BUFFER::NetBufferConfirmSent start\n"));
  ASSERTMSG("v SEND_BUFFER::NetBufferConfirmSent NetBuffer == nullptr\n", NetBuffer != nullptr);

  NET_BUFFER_STATUS(NetBuffer) = NB_SENT;

  PNET_BUFFER_LIST ListTmp = nullptr;
  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&_RwLock, true, &LockState);
  {
    KdPrint(("v SEND_BUFFER::NetBufferConfirmSent locked\n"));
    PNET_BUFFER_LIST *ListPtr = &_ListHead;

    while (*ListPtr != nullptr)
    {
      while (_CurrentPendingNetBuffer != nullptr && NET_BUFFER_STATUS(_CurrentPendingNetBuffer) == NB_SENT)
      {
        _CurrentPendingNetBuffer = NET_BUFFER_NEXT_NB(_CurrentPendingNetBuffer);
      }

      if (_CurrentPendingNetBuffer != nullptr)
      {
        break;
      }

      NET_BUFFER_LIST_STATUS(*ListPtr) = NDIS_STATUS_SUCCESS;
      ListPtr = &NET_BUFFER_LIST_NEXT_NBL((*ListPtr));
      _CurrentPendingNetBuffer = NET_BUFFER_LIST_FIRST_NB(*ListPtr);
    }
    KdPrint(("v SEND_BUFFER::NetBufferConfirmSent while end\n"));

    if (ListPtr != &_ListHead)
    {
      ListTmp = _ListHead;

      _ListHead = *ListPtr;
      if (_ListHead == nullptr)
      {
        _ListTail = nullptr;
      }

      *ListPtr = nullptr;
    }
  }
  NdisReleaseReadWriteLock(&_RwLock, &LockState);
  KdPrint(("v SEND_BUFFER::NetBufferConfirmSent unlocked\n"));

  if (ListTmp != nullptr)
  {
    NdisInterlockedAddUlong(&_CountSendOk, NET_BUFFER_LIST_NB_COUNT(ListTmp), &_SendOkLock);
    NdisMSendNetBufferListsComplete(_NdisMiniportHandle, ListTmp, 0);
  }

  KdPrint(("v SEND_BUFFER::NetBufferConfirmSent end\n"));
}

void SEND_BUFFER::DropWithStatus(_In_ NDIS_STATUS Status)
{
  KdPrint(("v SEND_BUFFER::DropWithStatus start\n"));
  ASSERTMSG("v SEND_BUFFER::DropWithStatus invalid status\n", Status == NDIS_STATUS_PAUSED || Status == NDIS_STATUS_RESET_IN_PROGRESS);

  PNET_BUFFER_LIST DropList;

  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&_RwLock, true, &LockState);
  {
    KdPrint(("v SEND_BUFFER::DropWithStatus locked\n"));
    PNET_BUFFER_LIST ListPtr = _ListHead;

    while (ListPtr != nullptr)
    {
      NET_BUFFER_LIST_STATUS(ListPtr) = Status;
      ListPtr = NET_BUFFER_LIST_NEXT_NBL(ListPtr);
    }

    KdPrint(("v SEND_BUFFER::DropWithStatus statuses setted\n"));

    DropList = _ListHead;
    
  }
  NdisReleaseReadWriteLock(&_RwLock, &LockState);
  KdPrint(("v SEND_BUFFER::DropWithStatus unlocked\n"));

  NdisMSendNetBufferListsComplete(_NdisMiniportHandle, DropList, 0);

  KdPrint(("v SEND_BUFFER::DropWithStatus end\n"));
}

void SEND_BUFFER::DropPaused()
{
  DropWithStatus(NDIS_STATUS_PAUSED);
}

void SEND_BUFFER::DropReset()
{
  DropWithStatus(NDIS_STATUS_RESET_IN_PROGRESS);
}

/*
void SEND_BUFFER::RemoveNetBufferListToCancelList(_Inout_ PNET_BUFFER_LIST *ListPtr, _Inout_ PNET_BUFFER_LIST *CanceledList)
{
  ASSERTMSG("v SEND_BUFFER::RemoveNetBufferListToCancelList ListPtr == nullptr", ListPtr != nullptr);

  if (_CurrentReadyNetBufferList == *ListPtr)
  {
    _CurrentReadyNetBufferList = NET_BUFFER_LIST_NEXT_NBL(*ListPtr);
    if (_CurrentReadyNetBufferList != nullptr)
      _CurrentReadyNetBuffer = NET_BUFFER_LIST_FIRST_NB(_CurrentReadyNetBufferList);
    else
      _CurrentReadyNetBuffer = nullptr;
  }

  PNET_BUFFER_LIST Tmp = *ListPtr;
  *ListPtr = NET_BUFFER_LIST_NEXT_NBL(*ListPtr);
  if (_ListTail == ListPtr)
    *_ListTail = *ListPtr;

  NET_BUFFER_LIST_NEXT_NBL(Tmp) = *CanceledList;
  *CanceledList = Tmp;
}
*/
 
/*
void SEND_BUFFER::CancelSend(_In_ PVOID CancelId)
{
  KdPrint(("v SEND_BUFFER::CancelSend start\n"));

  PNET_BUFFER_LIST CanceledList = nullptr;

  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&_RwLock, true, &LockState);
  {
    KdPrint(("v SEND_BUFFER::CancelSend locked\n"));
    PNET_BUFFER_LIST *ListPtr = &_ListHead;
    bool IsPendingNetBufferValid = true;

    if (_ListHead != nullptr && NDIS_GET_NET_BUFFER_LIST_CANCEL_ID(_ListHead) == CancelId)
    {
      KdPrint(("v SEND_BUFFER::CancelSend _ListHead == CancelId\n"));
      IsPendingNetBufferValid = false;
    }

    while (*ListPtr != nullptr)
    {
      if (NDIS_GET_NET_BUFFER_LIST_CANCEL_ID(*ListPtr) == CancelId)
      {
        NET_BUFFER_LIST_STATUS(*ListPtr) = NDIS_STATUS_SEND_ABORTED;
        this->RemoveNetBufferListToCancelList(ListPtr, &CanceledList);
      }

      ListPtr = &NET_BUFFER_LIST_NEXT_NBL(*ListPtr);
    }

    if (!IsPendingNetBufferValid)
    {
      if (_ListHead != nullptr)
        _CurrentPendingNetBuffer = NET_BUFFER_LIST_FIRST_NB(_ListHead);
      else
        _CurrentPendingNetBuffer = nullptr;
    }
  }
  NdisReleaseReadWriteLock(&_RwLock, &LockState);
  KdPrint(("v SEND_BUFFER::CancelSend unlocked\n"));

  if (CanceledList != nullptr)
  {
    KdPrint(("v SEND_BUFFER::CancelSend completing send\n"));
    NdisMSendNetBufferListsComplete(_NdisMiniportHandle, CanceledList, 0);
  }

  KdPrint(("v SEND_BUFFER::CancelSend end\n"));
}
*/
