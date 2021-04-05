#pragma once

#define NDIS_MINIPORT_DRIVER 1
#define NDIS60_MINIPORT 1
#define NDIS_WDM

#include <ndis.h>

#define DEVICE_NAME L"\\Device\\Vadapter"
#define SYM_LINK L"\\??\\Vadapter"
  
#define BUFFER_SIZE 1432 //1518 1432 508
#define SEND_BUFFERS_COUNT 1024   //TODO: set limit to send buffers
#define RECEIVE_BUFFERS_COUNT 1024

#define MULTICAST_LIST_SIZE 32


#define SUPPORTED_PACKET_FILTERS (NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_MULTICAST | NDIS_PACKET_TYPE_BROADCAST | NDIS_PACKET_TYPE_PROMISCUOUS)

void *operator new(_In_ SIZE_T Size);
void *operator new(SIZE_T, PVOID _In_ Ptr);
void operator delete(_Inout_ PVOID Ptr);
void operator delete(_Inout_ PVOID Ptr, SIZE_T);
