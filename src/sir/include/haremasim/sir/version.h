// SIR (slot-indexed IR): the intermediate representation shared by all tiers.
//
// Only a version number exists at this stage.

#pragma once

#include <cstdint>

namespace haremasim::sir {

/// Version of the SIR definition. Bumped whenever the IR changes shape.
std::uint32_t formatVersion();

}  // namespace haremasim::sir
