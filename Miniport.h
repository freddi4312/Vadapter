#pragma once

#include "NdisWithParams.h"
#include "SendBuffer2.h"
#include "ReceiveBuffer.h"

MINIPORT_INITIALIZE              MiniportInitialize;
MINIPORT_HALT                    MiniportHalt;
MINIPORT_UNLOAD                  MiniportUnload;
MINIPORT_PAUSE                   MiniportPause;
MINIPORT_RESTART                 MiniportRestart;
MINIPORT_OID_REQUEST             MiniportOidRequest;
MINIPORT_SEND_NET_BUFFER_LISTS   MiniportSendNetBufferLists;
MINIPORT_RETURN_NET_BUFFER_LISTS MiniportReturnNetBufferLists;
MINIPORT_CANCEL_SEND             MiniportCancelSend;
MINIPORT_DEVICE_PNP_EVENT_NOTIFY MiniportDevicePnPEventNotify;
MINIPORT_SHUTDOWN                MiniportShutdown;
MINIPORT_CANCEL_OID_REQUEST      MiniportCancelOidRequest;

enum class DPC_STATE
{
  Running,
  Stopping,
  Stopped
};

struct ADAPTER_CONTEXT
{
  bool IsAdapterExist;
  bool IsAdapterReady;
  NDIS_RW_LOCK RwLock;
  SEND_BUFFER2 Send;
  RECEIVE_BUFFER Receive;
  NDIS_HANDLE NdisMiniportHandle;
  NDIS_HANDLE NdisMiniportDriverHandle;
  NDIS_HANDLE NdisDeviceObjectHandle;
  DPC_STATE DpcState;
  PDEVICE_OBJECT Device;

  ADAPTER_CONTEXT()
  {
    memset(this, 0, sizeof(ADAPTER_CONTEXT));
  }
  ~ADAPTER_CONTEXT() {}

}; 

typedef ADAPTER_CONTEXT *PADAPTER_CONTEXT;

struct DPC_CONTEXT
{
  KTIMER Timer;
  KDPC Dpc;
  PADAPTER_CONTEXT AdapterContext;
};

typedef DPC_CONTEXT *PDPC_CONTEXT;


#define ADAPTER_CONTEXT_FROM_DEVICE(_X_) (*((PADAPTER_CONTEXT *)NdisGetDeviceReservedExtension(_X_)))

void DpcSendChecker(PKDPC, _Inout_ PVOID Context, PVOID, PVOID);

/*
#define SUPPOTED_STATISTICS (NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_RCV    | \
                             NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_RCV   | \
                             NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_RCV   | \
                             NDIS_STATISTICS_FLAGS_VALID_BYTES_RCV              | \
                             NDIS_STATISTICS_FLAGS_VALID_RCV_DISCARDS           | \
                             NDIS_STATISTICS_FLAGS_VALID_RCV_ERROR              | \
                             NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_XMIT   | \
                             NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_XMIT  | \
                             NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_XMIT  | \
                             NDIS_STATISTICS_FLAGS_VALID_BYTES_XMIT             | \
                             NDIS_STATISTICS_FLAGS_VALID_XMIT_ERROR             | \
                             NDIS_STATISTICS_FLAGS_VALID_XMIT_DISCARDS          | \
                             NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_RCV     | \
                             NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_RCV    | \
                             NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_RCV    | \
                             NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_XMIT    | \
                             NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_XMIT   | \
                             NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_XMIT)
*/