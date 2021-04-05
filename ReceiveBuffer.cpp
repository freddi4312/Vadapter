#include "ReceiveBuffer.h"


// A little bit too many debug prints, i know
void FillNetBuffer(_Inout_ PNET_BUFFER NetBuffer, _In_ PVOID Buffer, _In_ SIZE_T BufferSize)
{
  KdPrint(("v RECEIVE_BUFFER::FillNetBuffer start\n"));
  ASSERTMSG("v RECEIVE_BUFFER::FillNetBuffer NetBuffer == nullptr\n", NetBuffer != nullptr);
  ASSERTMSG("v RECEIVE_BUFFER::FillNetBuffer Buffer == nullptr\n", Buffer != nullptr);

  PMDL Mdl = NET_BUFFER_CURRENT_MDL(NetBuffer);
  ASSERTMSG("v RECEIVE_BUFFER::FillNetBuffer Mdl == nullptr\n", Mdl != nullptr);

  PCHAR MdlBuffer = (PCHAR)MmGetMdlVirtualAddress(Mdl);
  ASSERTMSG("v RECEIVE_BUFFER::FillNetBuffer MdlBuffer == nullptr\n", MdlBuffer != nullptr);
  MdlBuffer += NET_BUFFER_CURRENT_MDL_OFFSET(NetBuffer);

  ASSERTMSG("v RECEIVE_BUFFER::FillNetBuffer MdlBuffer too small\n", MmGetMdlByteCount(Mdl) - NET_BUFFER_CURRENT_MDL_OFFSET(NetBuffer) < BufferSize);

  memcpy(MdlBuffer, Buffer, BufferSize);

  KdPrint(("v RECEIVE_BUFFER::FillNetBuffer end\n"));
}

void RECEIVE_BUFFER::FreeBuffers()
{
  while (_FreeBuffers != nullptr)
  {
    PNET_BUFFER_LIST NetBufferList = _FreeBuffers;
    _FreeBuffers = NET_BUFFER_LIST_NEXT_NBL(_FreeBuffers);
    NdisFreeNetBufferList(NetBufferList);
  }
}

INIT_RES RECEIVE_BUFFER::Init(_In_ NDIS_HANDLE NdisMiniportHandle, _In_ PMAC_ADDR MacAddr)
{
  KdPrint(("v RECEIVE_BUFFER::Init start\n"));
  ASSERTMSG("v RECEIVE_BUFFER::Init MiniportAdapterhandle == NULL\n", NdisMiniportHandle != NULL);

  //======PoolAllocation=====================================
  NET_BUFFER_LIST_POOL_PARAMETERS PoolParams;
  PoolParams.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
  PoolParams.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
  PoolParams.Header.Size = NDIS_SIZEOF_NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
  PoolParams.ProtocolId = NDIS_PROTOCOL_ID_DEFAULT;
  PoolParams.fAllocateNetBuffer = true;
  PoolParams.ContextSize = 0;
  PoolParams.PoolTag = 'NPVp';
  PoolParams.DataSize = BUFFER_SIZE;
  _Pool = NdisAllocateNetBufferListPool(NdisMiniportHandle, &PoolParams);
  if (_Pool == NULL)
  {
    KdPrint(("v RECEIVE_BUFFER::Init pool alloc error\n"));
    return INIT_RES::BadAlloc;
  }
  KdPrint(("v RECEIVE_BUFFER::Init pool allocated\n"));

  //======BuffersAllocation==================================
  _FreeBuffers = nullptr;
  _CountOfFreeBuffers = 0;
  while (_CountOfFreeBuffers < RECEIVE_BUFFERS_COUNT)
  {
    PNET_BUFFER_LIST NetBufferList = NdisAllocateNetBufferList(_Pool, 0, 0);
    if (NetBufferList == nullptr)
    {
      KdPrint(("v RECEIVE_BUFFER::Init buffer #%lu alloc error\n", _CountOfFreeBuffers));     
      FreeBuffers();
      NdisFreeNetBufferListPool(_Pool);

      return INIT_RES::BadAlloc;
    }

    NDIS_STATUS Result = NdisRetreatNetBufferDataStart(NET_BUFFER_LIST_FIRST_NB(NetBufferList), BUFFER_SIZE, 0, nullptr);
    ASSERTMSG("v RECEIVE_BUFFER::Init Retreat error\n", Result == NDIS_STATUS_SUCCESS);

    NET_BUFFER_LIST_NEXT_NBL(NetBufferList) = _FreeBuffers;
    _FreeBuffers = NetBufferList;
    _CountOfFreeBuffers++;
  }
  KdPrint(("v RECEIVE_BUFFER::Init buffers allocated\n"));

  //======OtherInitialization===============================
  _NdisMiniportHandle = NdisMiniportHandle;
  NdisInitializeReadWriteLock(&_RwLock);
  NdisInitializeReadWriteLock(&_RwReceiveStateLock);
  NdisAllocateSpinLock(&_ReceiveOkLock);
  _CountReceiveOk = 0;
  _IsReadyToReceive = true;
  _MacAddr = *MacAddr;
  _Filter = 0;
  for (ULONG i = 0; i < MULTICAST_LIST_SIZE; i++)
    _Multicast[i].zero();
  _MulticastSize = 0;

  KdPrint(("v RECEIVE_BUFFER::Init end\n"));
  return INIT_RES::Success;
}

void RECEIVE_BUFFER::ReturnBuffer(_Inout_ PNET_BUFFER_LIST NetBufferList)
{
  KdPrint(("v RECEIVE_BUFFER::ReturnBuffer start\n"));

  NdisInterlockedAddUlong((PULONG)&_CountReceiveOk, 1, &_ReceiveOkLock);

  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&_RwLock, true, &LockState);
  {
    KdPrint(("v RECEIVE_BUFFER::ReturnBuffer locked\n"));
    NET_BUFFER_LIST_NEXT_NBL(NetBufferList) = _FreeBuffers;
    _FreeBuffers = NetBufferList;
    _CountOfFreeBuffers++;
    ASSERTMSG("v CountOfFreeBuffers > RECEIVE_BUFFERS_COUNT\n", _CountOfFreeBuffers <= RECEIVE_BUFFERS_COUNT);
  }
  NdisReleaseReadWriteLock(&_RwLock, &LockState);
  KdPrint(("v RECEIVE_BUFFER::ReturnBuffer unlocked\n"));

  KdPrint(("v RECEIVE_BUFFER::ReturnBuffer end\n"));
}

bool RECEIVE_BUFFER::CanBeReceived(_In_ PMAC_HEADER MacHeader)
{
  if (_Filter & NDIS_PACKET_TYPE_PROMISCUOUS)
    return true;

  if (_Filter & NDIS_PACKET_TYPE_DIRECTED)
    if (MacHeader->Dest == _MacAddr)
      return true;

  if (_Filter & NDIS_PACKET_TYPE_BROADCAST)
    if (MacHeader->Dest == MacAddrBroadcast)
      return true;

  if (_Filter & NDIS_PACKET_TYPE_MULTICAST)
  {
    for (SIZE_T i = 0; i < _MulticastSize; i++)
      if (MacHeader->Dest == _Multicast[i])
        return true;
  }

  KdPrint(("v RECEIVE_BUFFER::CanBeReceived unlocked\n"));

  return false;
}

TRANSMIT_RES RECEIVE_BUFFER::TransmitToNdis(_In_ PVOID Buffer, _In_ ULONG BufferSize)
{
  KdPrint(("v RECEIVE_BUFFER::TransmitToNdis start\n"));

  if (!CanBeReceived((PMAC_HEADER)Buffer))
    return TRANSMIT_RES::Restricted;

  bool IsReady = true;
  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&_RwReceiveStateLock, false, &LockState);
  {
    KdPrint(("v RECEIVE_BUFFER::TransmitToNdis locked #1\n"));
    if (_IsReadyToReceive == false)
      IsReady = false;
  }
  NdisReleaseReadWriteLock(&_RwReceiveStateLock, &LockState);
  KdPrint(("v RECEIVE_BUFFER::TransmitToNdis unlocked #1\n"));
  if (!IsReady)
    return TRANSMIT_RES::ResetingOrPausing;

  PNET_BUFFER_LIST NetBufferList = nullptr;
  TRANSMIT_RES Result = TRANSMIT_RES::Success;
  NdisAcquireReadWriteLock(&_RwLock, true, &LockState);
  {
    KdPrint(("v RECEIVE_BUFFER::TransmitToNdis locked #2\n"));
    if (_FreeBuffers == nullptr)
    {
      Result = TRANSMIT_RES::NoFreeBuffers;
    }
    else
    {
      NetBufferList = _FreeBuffers;
      _FreeBuffers = NET_BUFFER_LIST_NEXT_NBL(_FreeBuffers);
      _CountOfFreeBuffers--;
      ASSERTMSG("v CountOfFreeBuffers < 0\n", _CountOfFreeBuffers >= 0);
    }
  }
  NdisReleaseReadWriteLock(&_RwLock, &LockState);
  KdPrint(("v RECEIVE_BUFFER::TransmitToNdis unlocked #2\n"));

  if (Result == TRANSMIT_RES::NoFreeBuffers)
  {
    KdPrint(("v RECEIVE_BUFFER::TransmitToNdis no free net buffers\n"));
    return Result;
  }

  FillNetBuffer(NET_BUFFER_LIST_FIRST_NB(NetBufferList), Buffer, BufferSize);
  KdPrint(("v RECEIVE_BUFFER::TransmitToNdis inidicating receive\n"));
  NdisMIndicateReceiveNetBufferLists(_NdisMiniportHandle, NetBufferList, 0, 1, 0);

  KdPrint(("v RECEIVE_BUFFER::TransmitToNdis end\n"));
  return Result;
}

void RECEIVE_BUFFER::SetReady()
{
  KdPrint(("v RECEIVE_BUFFER::SetReady start\n"));

  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&_RwReceiveStateLock, true, &LockState);
  {
    KdPrint(("v RECEIVE_BUFFER::SetReady locked\n"));
    _IsReadyToReceive = true;
  }
  NdisReleaseReadWriteLock(&_RwReceiveStateLock, &LockState);
  KdPrint(("v RECEIVE_BUFFER::SetReady unlocked\n"));

  KdPrint(("v RECEIVE_BUFFER::SetReady end\n"));
}

void RECEIVE_BUFFER::SetUnready()
{
  KdPrint(("v RECEIVE_BUFFER::SetUnready start\n"));

  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&_RwReceiveStateLock, true, &LockState);
  {
    KdPrint(("v RECEIVE_BUFFER::SetUnready locked\n"));
    _IsReadyToReceive = false;
  }
  NdisReleaseReadWriteLock(&_RwReceiveStateLock, &LockState);
  KdPrint(("v RECEIVE_BUFFER::SetUnready unlocked\n"));

  KdPrint(("v RECEIVE_BUFFER::SetUnready end\n"));
}

RECEIVE_BUFFER::~RECEIVE_BUFFER()
{
  KdPrint(("v RECEIVE_BUFFER::free start\n"));

  PNET_BUFFER_LIST LocalFreeBuffers;

  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&_RwLock, true, &LockState);
  {
    KdPrint(("v RECEIVE_BUFFER::free locked\n"));

    LocalFreeBuffers = _FreeBuffers;
    _FreeBuffers = nullptr;
  }
  NdisReleaseReadWriteLock(&_RwLock, &LockState);
  KdPrint(("v RECEIVE_BUFFER::free unlocked\n"));

  PNET_BUFFER_LIST NetBufferList;
  while (LocalFreeBuffers != nullptr)
  {
    NetBufferList = LocalFreeBuffers;
    LocalFreeBuffers = NET_BUFFER_LIST_NEXT_NBL(LocalFreeBuffers);
    NdisFreeNetBufferList(NetBufferList);
  }

  NdisFreeNetBufferListPool(_Pool);

  KdPrint(("v RECEIVE_BUFFER::free end\n"));
}

void RECEIVE_BUFFER::SetMulticast(_In_ PVOID MacAddrs, _In_ ULONG Size)
{
  _MulticastSize = Size;
  for (SIZE_T i = 0; i < Size; i++)
    _Multicast[i] = *(PMAC_ADDR)((PCHAR)MacAddrs + 6 * i);
}
/*
void *RECEIVE_BUFFER::operator new(_In_ SIZE_T Size)
{
  if (Size > (SIZE_T)MAXUINT)
    return nullptr;

  return NdisAllocateMemoryWithTagPriority(NdisMiniportDriverHandle, (ULONG)Size, 'NPVp', NormalPoolPriority);
}

void RECEIVE_BUFFER::operator delete(_Inout_ void *Ptr, _In_ SIZE_T Size)
{
  if (Ptr == nullptr)
    return;

  if (Size > (SIZE_T)MAXUINT)
    return;

  NdisFreeMemory(Ptr, (ULONG)Size, NormalPoolPriority);
}
*/