// Reading the range of the calling thread's stack (stack.h), as the
// kernel's twin does.

#include "stack.h"

#include <pthread.h>

namespace kirimasim::front {

StackRange StackRange::ofThisThread() {
#if defined(__APPLE__)
    // The address is the top of the stack, its highest end.
    const pthread_t self = pthread_self();
    const auto high = reinterpret_cast<std::uintptr_t>(pthread_get_stackaddr_np(self));
    const std::size_t size = pthread_get_stacksize_np(self);
    if (high == 0 || size == 0 || size > high) return {};
    return {high - size, high};
#elif defined(__linux__)
    pthread_attr_t attr;
    if (pthread_getattr_np(pthread_self(), &attr) != 0) return {};
    void* low = nullptr;
    std::size_t size = 0;
    const bool read = pthread_attr_getstack(&attr, &low, &size) == 0;
    pthread_attr_destroy(&attr);
    if (!read || low == nullptr || size == 0) return {};
    const auto from = reinterpret_cast<std::uintptr_t>(low);
    return {from, from + size};
#else
    return {};
#endif
}

}  // namespace kirimasim::front
