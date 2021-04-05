#include "MacAddr.h"

MAC_ADDR MacAddrBroadcast = { {0xff, 0xff, 0xff, 0xff, 0xff, 0xff} };
MAC_ADDR MacAddrAdapter = { {0xff, 0xff, 0xff, 0x30, 0x30, 0x30} };

bool MAC_ADDR::operator==(MAC_ADDR &other)
{
  bool Result = true;

  for (ULONG i = 0; i < 6; i++)
    if (Byte[i] != other.Byte[i])
      Result = false;

  return Result;
}

void MAC_ADDR::zero()
{
  for (ULONG i = 0; i < 6; i++)
    Byte[i] = 0;
}

void MAC_ADDR::operator=(MAC_ADDR &other)
{
  for (ULONG i = 0; i < 6; i++)
    Byte[i] = other.Byte[i];
}
