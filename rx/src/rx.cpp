//
// Created by Gxin on 2026/1/4.
//

#include "rx/rx.h"
#include "rx/leak_observer.h"

namespace rx
{
#if GX_DEBUG
std::unordered_map<std::string, int64_t> LeakObserver::sObjectMap = {};
GMutex LeakObserver::sLock;
#endif
}
