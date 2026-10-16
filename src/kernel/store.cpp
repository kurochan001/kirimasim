#include "kirimasim/kernel/store.h"

#include "kirimasim/kernel/bits.h"

namespace kirimasim::kernel {

SlotId Store::add(Domain domain, std::uint32_t width, std::uint32_t elements, Init init) {
    SlotLayout l;
    l.domain = domain;
    l.width = domain == Domain::Two || domain == Domain::Four ? width : 64;
    l.elements = elements;
    const std::uint32_t n = wordsFor(l.width);
    l.elemWords = domain == Domain::Four ? 2 * n : domain == Domain::Two ? n : 1;
    auto& w = words(domain);
    l.offset = w.size();
    w.resize(w.size() + std::uint64_t{l.elemWords} * elements, 0);
    if (domain == Domain::Four && init != Init::Zero) {
        // x is a = 1, b = 1; z is a = 0, b = 1. Bits above the width stay 0.
        for (std::uint32_t e = 0; e < elements; ++e) {
            std::uint64_t* p = w.data() + l.offset + std::uint64_t{e} * l.elemWords;
            for (std::size_t i = 0; i < n; ++i) {
                const auto bits = static_cast<unsigned>(i + 1 < n ? 64 : l.width - 64 * i);
                // 1 is a = 1, b = 0.
                p[2 * i] = init == Init::X || init == Init::One ? mask(bits) : 0;
                p[2 * i + 1] = init == Init::One ? 0 : mask(bits);
            }
        }
    }
    slots_.push_back(l);
    return static_cast<SlotId>(slots_.size() - 1);
}

}  // namespace kirimasim::kernel
