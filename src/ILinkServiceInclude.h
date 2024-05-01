// ILinkServiceInclude.h

#ifndef _I_LINK_SERVICE_INCLUDE_h
#define _I_LINK_SERVICE_INCLUDE_h

#include <LoLaDefinitions.h>

#include "Link\ILoLaLink.h"
#include "Link\LoLaLinkDefinition.h"
#include "Link\LoLaPacketDefinition.h"

/// Base Link Services for extension.
#include "Services\Template\TemplateLinkService.h"
#include "Services\Discovery\DiscoveryDefinitions.h"
#include "Services\Discovery\AbstractDiscoveryService.h"
///

/// Ready-to-use Link Services
#include <ISurfaceInclude.h>
#include "Services\Surface\SurfaceReader.h"
#include "Services\Surface\SurfaceWriter.h"
///
#endif