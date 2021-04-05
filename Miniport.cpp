#include "Miniport.h"
#include "Device.h"
#include "OidRequests.h"


NDIS_OID SupportedOidList[] =
{
  //OID_GEN_HARDWARE_STATUS,
  OID_GEN_TRANSMIT_BUFFER_SPACE,
  OID_GEN_RECEIVE_BUFFER_SPACE,
  OID_GEN_TRANSMIT_BLOCK_SIZE,
  OID_GEN_RECEIVE_BLOCK_SIZE,
  OID_GEN_VENDOR_ID,
  OID_GEN_VENDOR_DESCRIPTION,
  OID_GEN_VENDOR_DRIVER_VERSION,
  OID_GEN_CURRENT_PACKET_FILTER,
  OID_GEN_CURRENT_LOOKAHEAD,
  //OID_GEN_DRIVER_VERSION,
  OID_GEN_MAXIMUM_TOTAL_SIZE,
  OID_GEN_XMIT_OK,
  OID_GEN_RCV_OK,
  OID_GEN_STATISTICS,
  //OID_GEN_LINK_PARAMETERS,
  OID_GEN_INTERRUPT_MODERATION,
  //OID_GEN_MEDIA_SUPPORTED,
  //OID_GEN_MEDIA_IN_USE,
  //OID_GEN_MAXIMUM_SEND_PACKETS,
  //OID_GEN_XMIT_ERROR,
  //OID_GEN_RCV_ERROR,
  //OID_GEN_RCV_NO_BUFFER,
  //OID_802_3_PERMANENT_ADDRESS,
  //OID_802_3_CURRENT_ADDRESS,
  OID_802_3_MULTICAST_LIST,
  //OID_802_3_MAXIMUM_LIST_SIZE,
  //OID_802_3_RCV_ERROR_ALIGNMENT,
  //OID_802_3_XMIT_ONE_COLLISION,
  //OID_802_3_XMIT_MORE_COLLISIONS
};

enum class MI_STAGE
{
  Success,
  AdapterInit,
  RegAttrs,
  GenAttrs,
  DpcReg
};

_Use_decl_annotations_
NDIS_STATUS MiniportInitialize(NDIS_HANDLE NdisMiniportHandle, NDIS_HANDLE MiniportDriverContext, PNDIS_MINIPORT_INIT_PARAMETERS)
{
  KdPrint(("v MiniportInitialize start\n"));

  PADAPTER_CONTEXT AdapterContext = (PADAPTER_CONTEXT)MiniportDriverContext;
  MI_STAGE Stage;
  NDIS_STATUS Status;

  bool CheckStatus;
  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&AdapterContext->RwLock, true, &LockState);
  {
    CheckStatus = AdapterContext->IsAdapterExist;
    if (!CheckStatus)
      AdapterContext->IsAdapterExist = true;
  }
  NdisReleaseReadWriteLock(&AdapterContext->RwLock, &LockState);
  if (CheckStatus)
    return NDIS_STATUS_FAILURE;

  do
  {
    Stage = MI_STAGE::AdapterInit; //Adapter context initialization
    AdapterContext->NdisMiniportHandle = NdisMiniportHandle;
    AdapterContext->DpcState = DPC_STATE::Stopped;
    AdapterContext->Send.Init(NdisMiniportHandle);
    INIT_RES InitResult = AdapterContext->Receive.Init(NdisMiniportHandle, &MacAddrAdapter);
    if (InitResult == INIT_RES::BadAlloc)
    {
      Status = NDIS_STATUS_FAILURE;
      break;
    }
    AdapterContext->IsAdapterReady = true;

    Stage = MI_STAGE::RegAttrs; //Setting adapter registration attributes;
    NDIS_MINIPORT_ADAPTER_ATTRIBUTES AdapterAttrs;
    NdisZeroMemory(&AdapterAttrs, sizeof(AdapterAttrs));
    AdapterAttrs.RegistrationAttributes.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
    AdapterAttrs.RegistrationAttributes.Header.Revision = NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
    AdapterAttrs.RegistrationAttributes.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
    AdapterAttrs.RegistrationAttributes.MiniportAdapterContext = AdapterContext;
    AdapterAttrs.RegistrationAttributes.AttributeFlags = NDIS_MINIPORT_ATTRIBUTES_SURPRISE_REMOVE_OK;
    AdapterAttrs.RegistrationAttributes.CheckForHangTimeInSeconds = 0;
    AdapterAttrs.RegistrationAttributes.InterfaceType = NdisInterfaceInternal;
    Status = NdisMSetMiniportAttributes(NdisMiniportHandle, &AdapterAttrs);
    if (Status != NDIS_STATUS_SUCCESS)
      break;

    Stage = MI_STAGE::GenAttrs; //Setting adapter general attributes;
    NdisZeroMemory(&AdapterAttrs, sizeof(AdapterAttrs));
    AdapterAttrs.GeneralAttributes.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES;
    AdapterAttrs.GeneralAttributes.Header.Revision = NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_1;
    AdapterAttrs.GeneralAttributes.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_1;
    AdapterAttrs.GeneralAttributes.Flags = 0;
    AdapterAttrs.GeneralAttributes.MediaType = NdisMedium802_3;
    AdapterAttrs.GeneralAttributes.PhysicalMediumType = NdisPhysicalMediumUnspecified;
    AdapterAttrs.GeneralAttributes.MtuSize = BUFFER_SIZE; //check
    AdapterAttrs.GeneralAttributes.MaxXmitLinkSpeed = 4'000'000;
    AdapterAttrs.GeneralAttributes.XmitLinkSpeed = 4'000'000;
    AdapterAttrs.GeneralAttributes.MaxRcvLinkSpeed = 4'000'000;
    AdapterAttrs.GeneralAttributes.RcvLinkSpeed = 4'000'000;
    AdapterAttrs.GeneralAttributes.MediaConnectState = MediaConnectStateConnected;
    AdapterAttrs.GeneralAttributes.MediaDuplexState = MediaDuplexStateFull;
    AdapterAttrs.GeneralAttributes.LookaheadSize = BUFFER_SIZE - 14; //check
    AdapterAttrs.GeneralAttributes.PowerManagementCapabilities = nullptr;
    AdapterAttrs.GeneralAttributes.MacOptions = NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA | NDIS_MAC_OPTION_TRANSFERS_NOT_PEND | NDIS_MAC_OPTION_NO_LOOPBACK; //check
    AdapterAttrs.GeneralAttributes.SupportedPacketFilters = SUPPORTED_PACKET_FILTERS;
    AdapterAttrs.GeneralAttributes.MaxMulticastListSize = MULTICAST_LIST_SIZE;
    AdapterAttrs.GeneralAttributes.MacAddressLength = 6;
    for (int i = 0; i < 6; i++)
      AdapterAttrs.GeneralAttributes.PermanentMacAddress[i] = MacAddrAdapter.Byte[i];
    for (int i = 0; i < 6; i++)
      AdapterAttrs.GeneralAttributes.CurrentMacAddress[i] = MacAddrAdapter.Byte[i];
    AdapterAttrs.GeneralAttributes.RecvScaleCapabilities = nullptr;
    AdapterAttrs.GeneralAttributes.AccessType = NET_IF_ACCESS_BROADCAST; //check
    AdapterAttrs.GeneralAttributes.DirectionType = NET_IF_DIRECTION_SENDRECEIVE;
    AdapterAttrs.GeneralAttributes.ConnectionType = NET_IF_CONNECTION_DEDICATED; //check
    AdapterAttrs.GeneralAttributes.IfType = IF_TYPE_ETHERNET_CSMACD;
    AdapterAttrs.GeneralAttributes.IfConnectorPresent = FALSE;
    AdapterAttrs.GeneralAttributes.SupportedStatistics = 0;
    AdapterAttrs.GeneralAttributes.SupportedPauseFunctions = NdisPauseFunctionsUnsupported;
    AdapterAttrs.GeneralAttributes.DataBackFillSize = 0;
    AdapterAttrs.GeneralAttributes.ContextBackFillSize = 0;
    AdapterAttrs.GeneralAttributes.SupportedOidList = SupportedOidList;
    AdapterAttrs.GeneralAttributes.SupportedOidListLength = sizeof(SupportedOidList);
    AdapterAttrs.GeneralAttributes.AutoNegotiationFlags = NDIS_LINK_STATE_DUPLEX_AUTO_NEGOTIATED; //check
    Status = NdisMSetMiniportAttributes(NdisMiniportHandle, &AdapterAttrs);
    if (Status != NDIS_STATUS_SUCCESS)
      break;
    
    Stage = MI_STAGE::DpcReg;
    PDPC_CONTEXT DpcContext = new DPC_CONTEXT;
    if (DpcContext == nullptr)
    {
      Status = NDIS_STATUS_FAILURE;
      break;
    }
    LARGE_INTEGER Interval;
    Interval.QuadPart = -2000LL * 10000LL;
    KeInitializeTimer(&DpcContext->Timer);
    DpcContext->AdapterContext = AdapterContext;
    KeInitializeDpc(&DpcContext->Dpc, DpcSendChecker, DpcContext);
    KeSetTimer(&DpcContext->Timer, Interval, &DpcContext->Dpc);
    AdapterContext->DpcState = DPC_STATE::Running;

    Stage = MI_STAGE::Success;
  } while (false);

  switch (Stage)
  {
    case MI_STAGE::DpcReg:
    case MI_STAGE::GenAttrs:
    case MI_STAGE::RegAttrs:
      NdisAcquireReadWriteLock(&AdapterContext->RwLock, true, &LockState);
      {
        AdapterContext->IsAdapterReady = false;
        AdapterContext->Receive.~RECEIVE_BUFFER();
        AdapterContext->Send.~SEND_BUFFER2();
      }
      NdisReleaseReadWriteLock(&AdapterContext->RwLock, &LockState);
    case MI_STAGE::AdapterInit:
      KdPrint(("v MiniportInit stage #%i error (Status = %i)\n", Stage, Status));
      break;

    case MI_STAGE::Success:
      KdPrint(("v MiniportInit success\n"));
      break;

    default:
      KdPrint(("v MiniportInit unknown stage #%i\n", Stage));
  }

  return Status;
}

void DpcSendChecker(PKDPC, _Inout_ PVOID Context, PVOID, PVOID)
{
  KdPrint(("v Dpc\n"));

  PDPC_CONTEXT DpcContext = (PDPC_CONTEXT)Context;

  if (DpcContext->AdapterContext->DpcState != DPC_STATE::Running)
  {
    KdPrint(("v Dpc stopped\n"));
    DpcContext->AdapterContext->DpcState = DPC_STATE::Stopped;
    return;
  }

  DpcContext->AdapterContext->Send.DropIfNotSent();

  LARGE_INTEGER Interval;
  Interval.QuadPart = -2000LL * 10000LL;
  KeSetTimer(&DpcContext->Timer, Interval, &DpcContext->Dpc);
}


// V V
_Use_decl_annotations_
void MiniportHalt(NDIS_HANDLE MiniportAdapterContext, NDIS_HALT_ACTION)
{
  KdPrint(("v MiniportHalt start\n"));

  PADAPTER_CONTEXT AdapterContext = (PADAPTER_CONTEXT)MiniportAdapterContext;
  AdapterContext->DpcState = DPC_STATE::Stopping;
  while (AdapterContext->DpcState != DPC_STATE::Stopped);
  KdPrint(("v Dpc stop confirm\n"));

  LOCK_STATE LockState;
  NdisAcquireReadWriteLock(&AdapterContext->RwLock, true, &LockState);
  {
    AdapterContext->IsAdapterReady = false;
    AdapterContext->Receive.~RECEIVE_BUFFER();
    AdapterContext->Send.~SEND_BUFFER2();
  }
  NdisReleaseReadWriteLock(&AdapterContext->RwLock, &LockState);


  KdPrint(("v MiniportHalt end\n"));
}


// V V
_Use_decl_annotations_
void MiniportUnload(PDRIVER_OBJECT DriverObject)
{
  KdPrint(("v MiniportUnload start\n"));

  PADAPTER_CONTEXT AdapterContext = ADAPTER_CONTEXT_FROM_DEVICE(DriverObject->DeviceObject);

  NdisDeregisterDeviceEx(AdapterContext->NdisDeviceObjectHandle);
  NdisMDeregisterMiniportDriver(AdapterContext->NdisMiniportDriverHandle);

  KdPrint(("v MiniportUnload end\n"));
}


// V V
_Use_decl_annotations_
NDIS_STATUS MiniportPause(NDIS_HANDLE MiniportAdapterContext, PNDIS_MINIPORT_PAUSE_PARAMETERS)
{
  KdPrint(("v MiniportPause start\n"));

  PADAPTER_CONTEXT AdapterContext = (PADAPTER_CONTEXT)MiniportAdapterContext;
  AdapterContext->Receive.SetUnready();

  KdPrint(("v MiniportPause end\n"));

  return NDIS_STATUS_SUCCESS;
}


// V V
_Use_decl_annotations_
NDIS_STATUS MiniportRestart(NDIS_HANDLE MiniportAdapterContext, PNDIS_MINIPORT_RESTART_PARAMETERS)
{
  KdPrint(("v MiniportRestart start\n"));

  PADAPTER_CONTEXT AdapterContext = (PADAPTER_CONTEXT)MiniportAdapterContext;
  AdapterContext->Receive.SetReady();

  KdPrint(("v MiniportRestart end\n"));

  return NDIS_STATUS_SUCCESS;
}


// V V
_Use_decl_annotations_
NDIS_STATUS MiniportOidRequest(NDIS_HANDLE MiniportAdapterContext, PNDIS_OID_REQUEST OidRequest)
{
  KdPrint(("v MiniportOidRequest start\n"));

  PADAPTER_CONTEXT AdapterContext = (PADAPTER_CONTEXT)MiniportAdapterContext;

  switch (OidRequest->RequestType)
  {
    case NdisRequestQueryInformation:
    case NdisRequestQueryStatistics:
      return OidQuery(AdapterContext, OidRequest);

    case NdisRequestSetInformation:
      return OidSet(AdapterContext,  OidRequest);
    
    default:
      KdPrint(("Not supported OidRequest type (%i)", OidRequest->RequestType));
      return NDIS_STATUS_NOT_SUPPORTED;
  }
}


// V V
_Use_decl_annotations_
void MiniportSendNetBufferLists(NDIS_HANDLE MiniportAdapterContext, PNET_BUFFER_LIST NetBufferList, NDIS_PORT_NUMBER, ULONG)
{
  KdPrint(("v MiniportSendNetBufferLists start\n"));

  PADAPTER_CONTEXT AdapterContext = (PADAPTER_CONTEXT)MiniportAdapterContext;
  AdapterContext->Send.AddNetBufferList(NetBufferList);

  KdPrint(("v MiniportSendNetBufferLists end\n"));
}


// V V
_Use_decl_annotations_
void MiniportReturnNetBufferLists(NDIS_HANDLE MiniportAdapterContext, PNET_BUFFER_LIST NetBufferLists, ULONG)
{
  KdPrint(("v MiniportReturnNetBufferLists start\n"));

  PADAPTER_CONTEXT AdapterContext = (PADAPTER_CONTEXT)MiniportAdapterContext;
  AdapterContext->Receive.ReturnBuffer(NetBufferLists);

  KdPrint(("v MiniportReturnNetBufferLists end\n"));
}


// V V
_Use_decl_annotations_
void MiniportCancelSend(NDIS_HANDLE MiniportAdapterContext, PVOID CancelId)
{
  KdPrint(("v MiniportCanselList start\n"));

  PADAPTER_CONTEXT AdapterContext = (PADAPTER_CONTEXT)MiniportAdapterContext;
  AdapterContext->Send.CancelSend(CancelId);

  KdPrint(("v MiniportCanselList end\n"));
}


// V V
_Use_decl_annotations_
void MiniportDevicePnPEventNotify(NDIS_HANDLE, PNET_DEVICE_PNP_EVENT)
{
  KdPrint(("v MiniportDevicePnPEventNotify\n"));

  // Just ingoring this event
}


// V V
_Use_decl_annotations_
void MiniportShutdown(NDIS_HANDLE, NDIS_SHUTDOWN_ACTION)
{
  KdPrint(("v MiniportShutdown\n"));

  //Nothing to do here
}


// V V
_Use_decl_annotations_
void MiniportCancelOidRequest(NDIS_HANDLE, PVOID)
{
  KdPrint(("v MiniportCancelOidRequest\n"));

  //No OIDs need to be canceled, because all completes immediately
}