#include "Device.h"
#include "Miniport.h"

NTSTATUS CompleteIrp(_Inout_ PIRP Irp, _In_ NTSTATUS Status, _In_ ULONG_PTR Info)
{
  Irp->IoStatus.Status = Status;
  Irp->IoStatus.Information = Info;
  IoCompleteRequest(Irp, 0);

  return Status;
}

_Use_decl_annotations_
NTSTATUS VadapterCreateClose(PDEVICE_OBJECT, PIRP Irp)
{
  KdPrint(("v VadapterCreateClose\n"));

  return CompleteIrp(Irp, STATUS_SUCCESS, 0);
}

_Success_(GET_NB_RES::Success)
GET_NB_RES GetNetBufferSafe(_In_ PADAPTER_CONTEXT AdapterContext, _Outptr_ PNET_BUFFER *pNetBuffer)
{
  ASSERTMSG("v GetNetBufferSafe pNetBuffer == nullptr\n", pNetBuffer != nullptr);

  LOCK_STATE LockState;
  GET_NB_RES NbResult;

  NdisAcquireReadWriteLock(&AdapterContext->RwLock, false, &LockState);
  {
    if (AdapterContext->IsAdapterReady)
      NbResult = AdapterContext->Send.GetNetBuffer(pNetBuffer);
    else
    {
      *pNetBuffer = nullptr;
      NbResult = GET_NB_RES::Nothing;
    }
  }
  NdisReleaseReadWriteLock(&AdapterContext->RwLock, &LockState);
  
  return NbResult;
}


void ReturnNetBufferSafe(_In_ PADAPTER_CONTEXT AdapterContext, _Inout_ PNET_BUFFER NetBuffer)
{
  LOCK_STATE LockState;

  NdisAcquireReadWriteLock(&AdapterContext->RwLock, false, &LockState);
  {
    if (AdapterContext->IsAdapterReady)
      AdapterContext->Send.NetBufferConfirmSent(NetBuffer);
  }
  NdisReleaseReadWriteLock(&AdapterContext->RwLock, &LockState);
}

_Use_decl_annotations_
NTSTATUS VadapterRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
  KdPrint(("v VadapterRead\n"));

  PIO_STACK_LOCATION StackLocation = IoGetCurrentIrpStackLocation(Irp);
  ULONG Length = StackLocation->Parameters.Read.Length;

  if (Length < BUFFER_SIZE)
    return CompleteIrp(Irp, STATUS_INVALID_BUFFER_SIZE, 0);

  ASSERTMSG("v Read: Irp->MdlAddress == nullptr\n", Irp->MdlAddress != nullptr);
  PVOID Buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
  if (Buffer == nullptr)
    return CompleteIrp(Irp, STATUS_INSUFFICIENT_RESOURCES, 0);

  PNET_BUFFER NetBuffer;
  ULONG Info;

  PADAPTER_CONTEXT AdapterContext = ADAPTER_CONTEXT_FROM_DEVICE(DeviceObject);
  if (GetNetBufferSafe(AdapterContext, &NetBuffer) == GET_NB_RES::Success)
  {
    ASSERTMSG("v VadapterRead NetBuffer length > BUFFER_SIZE\n", NetBuffer->DataLength <= BUFFER_SIZE);
    Info = NetBuffer->DataLength;
    PVOID Data = MmGetMdlVirtualAddress(NetBuffer->CurrentMdl);
    memcpy(Buffer, Data, NetBuffer->DataLength);
    ReturnNetBufferSafe(AdapterContext, NetBuffer);
  }
  else
  {
    Info = 0;
  }

  return CompleteIrp(Irp, STATUS_SUCCESS, Info);
}


TRANSMIT_RES TransmitToNdisSafe(_In_ PADAPTER_CONTEXT AdapterContext, _In_ PVOID Buffer, _In_ ULONG BufferSize)
{
  TRANSMIT_RES Result;
  LOCK_STATE LockState;

  NdisAcquireReadWriteLock(&AdapterContext->RwLock, false, &LockState);
  {
    Result = AdapterContext->Receive.TransmitToNdis(Buffer, BufferSize);
  }
  NdisReleaseReadWriteLock(&AdapterContext->RwLock, &LockState);

  return Result;
}

_Use_decl_annotations_
NTSTATUS VadapterWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
  KdPrint(("v VadapterWrite\n"));

  PIO_STACK_LOCATION StackLocation = IoGetCurrentIrpStackLocation(Irp);
  ULONG Length = StackLocation->Parameters.Write.Length;

  if (Length > BUFFER_SIZE)
    return CompleteIrp(Irp, STATUS_INVALID_BUFFER_SIZE, 0);

  ASSERTMSG("v Write: Irp->MdlAddress == nullptr\n", Irp->MdlAddress != nullptr);
  PVOID Buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
  if (Buffer == nullptr)
    return CompleteIrp(Irp, STATUS_INSUFFICIENT_RESOURCES, 0);

  PADAPTER_CONTEXT AdapterContext = ADAPTER_CONTEXT_FROM_DEVICE(DeviceObject);
  if (TransmitToNdisSafe(AdapterContext, Buffer, Length) == TRANSMIT_RES::Success)
  {
    return CompleteIrp(Irp, STATUS_SUCCESS, Length);
  }

  return CompleteIrp(Irp, STATUS_SUCCESS, 0);
}