#include "Miniport.h"
#include "OidRequests.h"

ULONG VendorId = 0xFF'FF'FF;
WCHAR VendorDescription[] = L"Project VPN vendor";
ULONG VendorDriverVersion = 0x0001'0001;

NDIS_STATUS CompleteQuery(_Inout_ PNDIS_OID_REQUEST OidRequest, _In_ PVOID Buffer, _In_ ULONG Size)
{
  if (Size > OidRequest->DATA.QUERY_INFORMATION.InformationBufferLength)
  {
    KdPrint(("v Oid buffer too short\n"));
    OidRequest->DATA.QUERY_INFORMATION.BytesNeeded = Size;
    return NDIS_STATUS_BUFFER_TOO_SHORT;
  }

  OidRequest->DATA.QUERY_INFORMATION.BytesWritten = Size;
  memcpy(OidRequest->DATA.QUERY_INFORMATION.InformationBuffer, Buffer, Size);

  return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS OidQuery(_In_ PADAPTER_CONTEXT AdapterContext, _Inout_ PNDIS_OID_REQUEST OidRequest)
{
  ULONG Buffer = 0;
  NDIS_STATISTICS_INFO StatInfo = { 0 };
  NDIS_INTERRUPT_MODERATION_PARAMETERS InterruptModParams = { 0 };
  NDIS_STATUS Result;

  switch (OidRequest->DATA.QUERY_INFORMATION.Oid)
  {
    case OID_GEN_TRANSMIT_BUFFER_SPACE:
      KdPrint(("v OID_GEN_TRANSMIT_BUFFER_SPACE\n"));
      Buffer = BUFFER_SIZE * SEND_BUFFERS_COUNT;
      Result = CompleteQuery(OidRequest, &Buffer, sizeof(ULONG));
      break;

    case OID_GEN_RECEIVE_BUFFER_SPACE:
      KdPrint(("v OID_GEN_RECEIVE_BUFFER_SPACE\n"));
      Buffer = BUFFER_SIZE * RECEIVE_BUFFERS_COUNT;
      Result = CompleteQuery(OidRequest, &Buffer, sizeof(ULONG));
      break;

    case OID_GEN_TRANSMIT_BLOCK_SIZE:
      KdPrint(("v OID_GEN_TRANSMIT_BLOCK_SIZE\n"));
      Buffer = BUFFER_SIZE;
      Result = CompleteQuery(OidRequest, &Buffer, sizeof(ULONG));
      break;

    case OID_GEN_RECEIVE_BLOCK_SIZE:
      KdPrint(("v OID_GEN_RECEIVE_BLOCK_SIZE\n"));
      Buffer = BUFFER_SIZE;
      Result = CompleteQuery(OidRequest, &Buffer, sizeof(ULONG));
      break;

    case OID_GEN_VENDOR_ID:
      KdPrint(("v OID_GEN_VENDOR_ID\n"));
      Result = CompleteQuery(OidRequest, &VendorId, sizeof(VendorId));
      break;

    case OID_GEN_VENDOR_DESCRIPTION:
      KdPrint(("v OID_GEN_VENDOR_DESCRIPTION\n"));
      Result = CompleteQuery(OidRequest, VendorDescription, sizeof(VendorDescription));
      break;

    case OID_GEN_VENDOR_DRIVER_VERSION:
      KdPrint(("v OID_GEN_VENDOR_DRIVER_VERSION\n"));
      Result = CompleteQuery(OidRequest, &VendorDriverVersion, sizeof(VendorDriverVersion));
      break;

    case OID_GEN_MAXIMUM_TOTAL_SIZE:
      KdPrint(("v OID_GEN_MAXIMUM_TOTAL_SIZE\n"));
      Buffer = BUFFER_SIZE;
      Result = CompleteQuery(OidRequest, &Buffer, sizeof(ULONG));
      break;

    case OID_GEN_STATISTICS:
      KdPrint(("v OID_GEN_STATISTICS\n"));
      NdisZeroMemory(&StatInfo, sizeof(NDIS_STATISTICS_INFO));
      StatInfo.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
      StatInfo.Header.Revision = NDIS_STATISTICS_INFO_REVISION_1;
      StatInfo.Header.Size = NDIS_SIZEOF_STATISTICS_INFO_REVISION_1;
      StatInfo.SupportedStatistics = 0;
      Result = CompleteQuery(OidRequest, &StatInfo, NDIS_SIZEOF_STATISTICS_INFO_REVISION_1);
      break;

    case OID_GEN_XMIT_OK:
      KdPrint(("v OID_GEN_XMIT_OK\n"));
      Buffer = AdapterContext->Send.CountSendOk();
      Result = CompleteQuery(OidRequest, &Buffer, sizeof(ULONG));
      break;


    case OID_GEN_RCV_OK:
      KdPrint(("v OID_GEN_RCV_OK\n"));
      Buffer = AdapterContext->Receive.CountReceiveOk();
      Result = CompleteQuery(OidRequest, &Buffer, sizeof(ULONG));
      break;

    case OID_GEN_INTERRUPT_MODERATION:
      KdPrint(("v OID_GEN_INTERRUPT_MODERATION\n"));
      InterruptModParams.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
      InterruptModParams.Header.Revision = NDIS_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
      InterruptModParams.Header.Size = NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
      InterruptModParams.InterruptModeration = NdisInterruptModerationNotSupported;
      Result = CompleteQuery(OidRequest, &InterruptModParams, NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1);
      break;

    default:
      Result = NDIS_STATUS_NOT_SUPPORTED;
  }

  switch (Result)
  {
    case NDIS_STATUS_SUCCESS:
      KdPrint(("v Oid query success\n"));
      break;
    case NDIS_STATUS_NOT_SUPPORTED:
      KdPrint(("v Oid query #%i not supported\n", OidRequest->DATA.SET_INFORMATION.Oid));
      break;
    case NDIS_STATUS_INVALID_DATA:
      KdPrint(("v Oid query invalid data\n"));
      break;
    case NDIS_STATUS_BUFFER_TOO_SHORT:
      KdPrint(("v Oid query buffer too short\n"));
      break;
    default:
      KdPrint(("v Oid query unknown result\n"));
  }

  return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS CompleteSet(_Inout_ PNDIS_OID_REQUEST OidRequest, _In_ ULONG Size)
{
  if (OidRequest->DATA.SET_INFORMATION.InformationBufferLength != Size)
  {
    OidRequest->DATA.SET_INFORMATION.BytesNeeded = Size;
    return NDIS_STATUS_INVALID_LENGTH;
  }
  
  OidRequest->DATA.SET_INFORMATION.BytesRead = Size;

  return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS OidSet(_In_ PADAPTER_CONTEXT AdapterContext, _Inout_ PNDIS_OID_REQUEST OidRequest)
{

  NDIS_STATUS Result;

  switch (OidRequest->DATA.SET_INFORMATION.Oid)
  {
    case OID_GEN_CURRENT_LOOKAHEAD:
      KdPrint(("v OID_GEN_CURRENT_LOOKAHEAD\n"));
      Result = CompleteSet(OidRequest, sizeof(ULONG));
      if (Result == NDIS_STATUS_SUCCESS)
      {
        if (*(PULONG)OidRequest->DATA.SET_INFORMATION.InformationBuffer > BUFFER_SIZE)
        {
          KdPrint(("v Warning! required lookahead > BUFFER_SIZE\n"));
        }
      }
      break;

    case OID_GEN_INTERRUPT_MODERATION:
      KdPrint(("v OID_GEN_INTERRUPT_MODERATION\n"));
      Result = NDIS_STATUS_INVALID_DATA;
      break;

    case OID_GEN_CURRENT_PACKET_FILTER:
      KdPrint(("v OID_GEN_CURRENT_PACKET_FILTER\n"));
      Result = CompleteSet(OidRequest, sizeof(ULONG));
      if (Result == NDIS_STATUS_SUCCESS)
      {
        ULONG Filter = *(PULONG)OidRequest->DATA.SET_INFORMATION.InformationBuffer;
        if (Filter & ~SUPPORTED_PACKET_FILTERS)
        {
          KdPrint(("v Warning! packet type filter #%i not supported\n", Filter));
        }
        else
        {
          AdapterContext->Receive.SetFilter(Filter);
        }

      }
      break;

    case OID_802_3_MULTICAST_LIST:
      KdPrint(("v OID_802_3_MULTICAST_LIST\n"));     

      OidRequest->DATA.SET_INFORMATION.BytesRead = OidRequest->DATA.SET_INFORMATION.InformationBufferLength;
      AdapterContext->Receive.SetMulticast(OidRequest->DATA.SET_INFORMATION.InformationBuffer, OidRequest->DATA.SET_INFORMATION.InformationBufferLength / 6);
      Result = NDIS_STATUS_SUCCESS;
      break;

    default:
      Result = NDIS_STATUS_NOT_SUPPORTED;
  }

  switch (Result)
  {
    case NDIS_STATUS_SUCCESS:
      KdPrint(("v Oid set success\n"));
      break;
    case NDIS_STATUS_NOT_SUPPORTED:
      KdPrint(("v Oid set #%i not supported\n", OidRequest->DATA.SET_INFORMATION.Oid));
      break;
    case NDIS_STATUS_INVALID_DATA:
      KdPrint(("v Oid set invalid data\n"));
      break;
    case NDIS_STATUS_INVALID_LENGTH:
      KdPrint(("v Oid set invalid length\n"));
      break;
    default:
      KdPrint(("v Oid set unknown result\n"));
  }

  return Result;
}
