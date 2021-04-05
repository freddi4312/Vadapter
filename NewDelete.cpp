#include "NdisWithParams.h"


void *operator new(_In_ SIZE_T Size)
{
  KdPrint(("v New\n"));

  if (Size > (SIZE_T)MAXUINT)
    return nullptr;

  return ExAllocatePool2(POOL_FLAG_NON_PAGED, (ULONG)Size, 'NPVp');
}

void operator delete(_Inout_ PVOID Ptr)
{
  KdPrint(("v Delete\n"));

  if (Ptr == nullptr)
    return;

  ExFreePoolWithTag(Ptr, 'NPVp');
}

void *operator new(SIZE_T, PVOID _In_ Ptr)
{
  KdPrint(("v New w\\size\n"));

  return Ptr;
}

void operator delete(_Inout_ PVOID Ptr, SIZE_T)
{
  KdPrint(("v New w\\size\n"));

  if (Ptr == nullptr)
    return;

  ExFreePoolWithTag(Ptr, 'NPVp');
}