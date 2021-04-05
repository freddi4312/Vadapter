#pragma once

#include "NdisWithParams.h"

struct MAC_ADDR
{
  UCHAR Byte[6];

  bool operator==(MAC_ADDR &other);
  void zero();
  void operator=(MAC_ADDR &other);
};

typedef MAC_ADDR *PMAC_ADDR;

typedef struct MAC_HEADER
{
  MAC_ADDR Dest;
  MAC_ADDR Source;
  UCHAR EtherType[2];
} *PMAC_HEADER;

extern MAC_ADDR MacAddrBroadcast;
extern MAC_ADDR MacAddrAdapter;
