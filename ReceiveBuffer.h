#pragma once

#include "NdisWithParams.h"
#include "MacAddr.h"


enum class TRANSMIT_RES
{
  Success,
  NoFreeBuffers,
  Restricted,
  ResetingOrPausing,
};

enum class INIT_RES
{
  Success,
  BadAlloc
};

class RECEIVE_BUFFER
{
private:
  NDIS_RW_LOCK _RwLock;
  PNET_BUFFER_LIST _FreeBuffers;
  LONG _CountOfFreeBuffers;
  NDIS_HANDLE _NdisMiniportHandle;
  bool _IsReadyToReceive;
  NDIS_RW_LOCK _RwReceiveStateLock;
  NDIS_HANDLE _Pool;
  NDIS_SPIN_LOCK _ReceiveOkLock;
  ULONG _CountReceiveOk;
  MAC_ADDR _MacAddr;
  ULONG _Filter;
  MAC_ADDR _Multicast[MULTICAST_LIST_SIZE];
  ULONG _MulticastSize;

public:
  RECEIVE_BUFFER() { NdisZeroMemory(this, sizeof(RECEIVE_BUFFER)); }
  INIT_RES Init(_In_ NDIS_HANDLE NdisMiniportHandle, _In_ PMAC_ADDR MacAddr);
  void ReturnBuffer(_Inout_ PNET_BUFFER_LIST NetBufferList);
  TRANSMIT_RES TransmitToNdis(_In_ PVOID Buffer, _In_ ULONG BufferSize);
  void SetReady();
  void SetUnready();
  ULONG CountReceiveOk()
  {
    return _CountReceiveOk;
  }
  void SetFilter(_In_ ULONG Filter)
  {
    _Filter = Filter;
  }
  ~RECEIVE_BUFFER();
  void SetMulticast(_In_ PVOID MacAddrs, _In_ ULONG Size);

private:
  void FreeBuffers();
  bool CanBeReceived(_In_ PMAC_HEADER MacHeader);
  /*
  void *operator new(_In_ SIZE_T Size);
  void operator delete(_Inout_ void *Ptr, _In_ SIZE_T Size);
  */
};