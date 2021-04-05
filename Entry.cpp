#include "Device.h"
#include "Miniport.h"


enum class DE_STAGE
{
  Success,
  MemoryAlloc,
  MiniportDriverReg,
  DeviceReg,
};

extern "C"
NDIS_STATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
  KdPrint(("v DriverEntry start\n"));

  NDIS_STATUS NdisStatus = NDIS_STATUS_FAILURE;
  DE_STAGE Stage;
  UNICODE_STRING SymbolicName = RTL_CONSTANT_STRING(SYM_LINK);
  UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(DEVICE_NAME);
  PADAPTER_CONTEXT AdapterContext = nullptr;
  PDEVICE_OBJECT DeviceObject = nullptr;
  NDIS_HANDLE NdisMiniportDriverHandle = nullptr;
  NDIS_HANDLE NdisDeviceObjectHandle = nullptr;

  do
  {
    Stage = DE_STAGE::MemoryAlloc; //Memory allocation
    AdapterContext = new ADAPTER_CONTEXT;
    if (AdapterContext == nullptr)
    {
      NdisStatus = NDIS_STATUS_FAILURE;
      break;
    }
    NdisInitializeReadWriteLock(&AdapterContext->RwLock);
    AdapterContext->IsAdapterReady = false;
    AdapterContext->IsAdapterExist = false;
    

    Stage = DE_STAGE::MiniportDriverReg; //Miniport driver registration
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS DriverChars;
    NdisZeroMemory(&DriverChars, sizeof(DriverChars));
    DriverChars.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
    DriverChars.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1;
    DriverChars.Header.Size = NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1;
    DriverChars.MajorNdisVersion = 6;
    DriverChars.MinorNdisVersion = 0;
    DriverChars.MajorDriverVersion = 1;
    DriverChars.MinorDriverVersion = 1;
    DriverChars.Flags = 0;
    DriverChars.InitializeHandlerEx = MiniportInitialize;
    DriverChars.HaltHandlerEx = MiniportHalt;
    DriverChars.UnloadHandler = MiniportUnload;
    DriverChars.PauseHandler = MiniportPause;
    DriverChars.RestartHandler = MiniportRestart;
    DriverChars.OidRequestHandler = MiniportOidRequest;
    DriverChars.SendNetBufferListsHandler = MiniportSendNetBufferLists;
    DriverChars.ReturnNetBufferListsHandler = MiniportReturnNetBufferLists;
    DriverChars.CancelSendHandler = MiniportCancelSend;
    DriverChars.DevicePnPEventNotifyHandler = MiniportDevicePnPEventNotify;
    DriverChars.ShutdownHandlerEx = MiniportShutdown;
    DriverChars.CancelOidRequestHandler = MiniportCancelOidRequest;
    NdisStatus = NdisMRegisterMiniportDriver(DriverObject, RegistryPath, AdapterContext, &DriverChars, &NdisMiniportDriverHandle);
    if (NdisStatus != NDIS_STATUS_SUCCESS)
      break;
    AdapterContext->NdisMiniportDriverHandle = NdisMiniportDriverHandle;


    Stage = DE_STAGE::DeviceReg; //Device registration
    NTSTATUS (*MajorFunction[28])(PDEVICE_OBJECT, PIRP);
    NdisZeroMemory(MajorFunction, sizeof(MajorFunction));
    MajorFunction[IRP_MJ_CREATE] = VadapterCreateClose;
    MajorFunction[IRP_MJ_CLOSE] = VadapterCreateClose;
    MajorFunction[IRP_MJ_READ] = VadapterRead;
    MajorFunction[IRP_MJ_WRITE] = VadapterWrite;
    NDIS_DEVICE_OBJECT_ATTRIBUTES DeviceAttrs;
    NdisZeroMemory(&DeviceAttrs, sizeof(DeviceAttrs));
    DeviceAttrs.Header.Type = NDIS_OBJECT_TYPE_DEVICE_OBJECT_ATTRIBUTES;
    DeviceAttrs.Header.Revision = NDIS_DEVICE_OBJECT_ATTRIBUTES_REVISION_1;
    DeviceAttrs.Header.Size = NDIS_SIZEOF_DEVICE_OBJECT_ATTRIBUTES_REVISION_1;
    DeviceAttrs.DeviceName = &DeviceName;
    DeviceAttrs.SymbolicName = &SymbolicName;
    DeviceAttrs.MajorFunctions = MajorFunction;
    DeviceAttrs.ExtensionSize = sizeof(PADAPTER_CONTEXT);
    DeviceAttrs.DefaultSDDLString = nullptr;
    DeviceAttrs.DeviceClassGuid = nullptr;
    NdisStatus = NdisRegisterDeviceEx(NdisMiniportDriverHandle, &DeviceAttrs, &DeviceObject, &NdisDeviceObjectHandle);
    if (NdisStatus != NDIS_STATUS_SUCCESS)
      break;
    DeviceObject->Flags |= DO_DIRECT_IO;
    ADAPTER_CONTEXT_FROM_DEVICE(DeviceObject) = AdapterContext;
    AdapterContext->NdisDeviceObjectHandle = NdisDeviceObjectHandle;
    AdapterContext->Device = DeviceObject;
    

    Stage = DE_STAGE::Success;

  } while (false);


  switch (Stage)
  {
    case DE_STAGE::DeviceReg:
      NdisMDeregisterMiniportDriver(NdisMiniportDriverHandle);
    case DE_STAGE::MiniportDriverReg:
      delete AdapterContext;
    case DE_STAGE::MemoryAlloc:
      KdPrint(("v DriverEntry stage #%i error(NdisStatus = %i)\n", Stage, NdisStatus));
      break;

    case DE_STAGE::Success:
      KdPrint(("v DriverEntry success\n"));
      break;

    default:
      KdPrint(("v DriverEntry unknown stage (%i)\n", Stage));
  }

  
  return NdisStatus;
}