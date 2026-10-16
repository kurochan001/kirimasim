#include "haremasim/kernel/strings.h"

namespace haremasim::kernel {

std::uint64_t Strings::intern(std::string_view s) {
    if (s.empty()) return 0;
    std::string key(s);
    if (auto it = handles_.find(key); it != handles_.end()) return it->second;
    texts_.push_back(key);
    const auto h = static_cast<std::uint64_t>(texts_.size());
    handles_.emplace(std::move(key), h);
    return h;
}

}  // namespace haremasim::kernel
