// ILinkServiceInclude.h

#ifndef _I_LINK_SERVICE_INCLUDE_h
#define _I_LINK_SERVICE_INCLUDE_h

#include <LoLaDefinitions.h>

#include "Link\ILoLaLink.h"
#include "Link\LoLaLinkDefinition.h"
#include "Link\LoLaPacketDefinition.h"

/// Base Link Services for extension.
#include "Service\TemplateLoLaService.h"
#include "LoLaServices\AbstractDiscovery\DiscoveryDefinitions.h"
#include "LoLaServices\AbstractDiscovery\AbstractDiscoveryService.h"
///

/// Ready-to-use Link Services
#include <ISurfaceInclude.h>
#include "LoLaServices\Surface\SurfaceReader.h"
#include "LoLaServices\Surface\SurfaceWriter.h"
///
#endif