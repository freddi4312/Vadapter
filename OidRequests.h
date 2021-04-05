#pragma once

#include "NdisWithParams.h"

NDIS_STATUS OidQuery(_In_ PADAPTER_CONTEXT AdapterContext, _Inout_ PNDIS_OID_REQUEST OidRequest);
NDIS_STATUS OidSet(_In_ PADAPTER_CONTEXT AdapterContext, _Inout_ PNDIS_OID_REQUEST OidRequest);