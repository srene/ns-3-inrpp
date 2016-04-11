
#ifdef NS3_MODULE_COMPILATION
# error "Do not include ns3 module aggregator headers from other modules; these are meant only for end user scripts."
#endif

#ifndef NS3_MODULE_INRPP
    

// Module headers:
#include "inrpp-cache.h"
#include "inrpp-global-route-manager-impl.h"
#include "inrpp-global-route-manager.h"
#include "inrpp-global-routing-helper.h"
#include "inrpp-header.h"
#include "inrpp-interface.h"
#include "inrpp-l3-protocol.h"
#include "inrpp-route.h"
#include "inrpp-stack-helper.h"
#include "inrpp-tag.h"
#include "inrpp-tail-queue.h"
#include "tcp-inrpp.h"
#endif
