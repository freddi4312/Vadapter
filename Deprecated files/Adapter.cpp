#include "Adapter.h"


ADAPTER_INIT ADAPTER::Init(_In_ NDIS_HANDLE NdisMiniportHandle, _In_ PMAC_ADDR MacAddr)
{
  _NdisMiniportHandle = NdisMiniportHandle;

  if (_ReceiveBuffer.Init(NdisMiniportHandle, MacAddr) == InitRes::BadAlloc)
    return ADAPTER_INIT::BadAlloc;

  _SendBuffer.Init(NdisMiniportHandle);

  return ADAPTER_INIT::Success;
}


NDIS_STATUS ADAPTER::Pause(_In_ PNDIS_MINIPORT_PAUSE_PARAMETERS PauseParameters)
{
  UNREFERENCED_PARAMETER(PauseParameters);

  //_SendBuffer.DropPaused();
  _ReceiveBuffer.SetUnready();

  return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS ADAPTER::Restart(_In_ PNDIS_MINIPORT_RESTART_PARAMETERS RestartParameters)
{
  UNREFERENCED_PARAMETER(RestartParameters);

  _ReceiveBuffer.SetReady();

  return NDIS_STATUS_SUCCESS;
}

void ADAPTER::CancelSend(_In_ PVOID CancelId)
{
  _SendBuffer.CancelSend(CancelId);
}

void ADAPTER::ReturnNetBufferLists(_Inout_ PNET_BUFFER_LIST NetBufferList)
{
  _ReceiveBuffer.ReturnBuffer(NetBufferList);
}

void ADAPTER::SendNetBufferLists(_Inout_ PNET_BUFFER_LIST NetBufferList)
{
  _SendBuffer.AddNetBufferList(NetBufferList);
}
