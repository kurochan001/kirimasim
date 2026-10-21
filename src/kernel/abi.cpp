// The C function table of abi.h. Every entry forwards to the C++ API, so
// the table and the API have one meaning (RUNTIME_ABI.md 4.2).

#include "kirimasim/kernel/abi.h"

#include <span>
#include <vector>

#include "kirimasim/kernel/context.h"

namespace kirimasim::kernel {

namespace {

static_assert(static_cast<int>(Status::Waiting) == KIRIMA_WAITING);
static_assert(static_cast<int>(Status::Done) == KIRIMA_DONE);
static_assert(static_cast<int>(Status::Killed) == KIRIMA_KILLED);
static_assert(static_cast<int>(Status::Error) == KIRIMA_ERROR);
static_assert(static_cast<int>(Status::Stop) == KIRIMA_STOP);
static_assert(static_cast<int>(Edge::Both) == KIRIMA_EDGE_BOTH);
static_assert(static_cast<int>(Group::Reactive) == KIRIMA_GROUP_REACTIVE);
static_assert(static_cast<int>(Domain::Four) == KIRIMA_DOMAIN_FOUR);
static_assert(sizeof(kirima_slot_part) == sizeof(SlotPart));

Scheduler& sched(kirima_context* c) { return fromC(c).scheduler(); }

SlotPart partOf(const kirima_slot_part& p) {
    return {p.slot, p.first_elem, p.num_elems, p.lsb, p.width};
}

std::vector<WaitItem> itemsOf(const kirima_wait_item* items, std::uint32_t n) {
    std::vector<WaitItem> v;
    v.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i)
        v.push_back({partOf(items[i].part), static_cast<Edge>(items[i].edge), items[i].guard,
                     items[i].guard_frame});
    return v;
}

MonitorSpec specOf(const kirima_monitor& m) {
    MonitorSpec spec;
    for (std::uint32_t i = 0; i < m.num_slots; ++i)
        spec.slots.push_back(partOf(m.slots[i]));
    for (std::uint32_t i = 0; i < m.num_events; ++i)
        spec.events.push_back(EventId{m.events[i]});
    spec.eval = m.eval;
    spec.frame = m.frame;
    spec.domain = static_cast<Domain>(m.domain);
    spec.width = m.width;
    spec.edge = static_cast<Edge>(m.edge);
    spec.untilTrue = m.until_true != 0;
    return spec;
}

constexpr kirima_kernel_api kApi = {
    KIRIMA_ABI_MAJOR,
    KIRIMA_ABI_MINOR,
    [](kirima_context* c, kirima_resume_fn fn, void* frame, std::uint32_t group) {
        return sched(c).addProcess(fn, frame, static_cast<Group>(group));
    },
    [](kirima_context* c, std::uint32_t p) { sched(c).start(p); },
    [](kirima_context* c, std::uint32_t p, std::uint64_t ticks) { sched(c).waitDelay(p, ticks); },
    [](kirima_context* c, std::uint32_t p, const kirima_wait_item* items, std::uint32_t n) {
        sched(c).waitEvent(p, itemsOf(items, n));
    },
    [](kirima_context* c, std::uint32_t p, const kirima_monitor* m) {
        sched(c).waitMonitor(p, specOf(*m));
    },
    [](kirima_context* c, std::uint32_t p, std::uint32_t e) { sched(c).waitNamed(p, EventId{e}); },
    [](kirima_context* c, std::uint32_t s, std::uint32_t elem, std::uint32_t lsb,
       std::uint32_t width,
       const std::uint64_t* value) { return sched(c).write(s, elem, lsb, width, value) ? 1 : 0; },
    [](kirima_context* c, std::uint32_t p, std::uint32_t s, std::uint32_t elem, std::uint32_t lsb,
       std::uint32_t width, const std::uint64_t* value,
       std::uint64_t delay) { sched(c).scheduleNba(p, s, elem, lsb, width, value, delay); },
    [](kirima_context* c, std::uint32_t e) { sched(c).trigger(EventId{e}); },
    [](kirima_context* c, std::uint32_t p, std::uint32_t e, std::uint64_t delay) {
        sched(c).triggerNb(p, EventId{e}, delay);
    },
    [](kirima_context* c, std::uint32_t e) { return sched(c).triggered(EventId{e}) ? 1 : 0; },
    [](kirima_context* c) { return sched(c).time(); },
    [](kirima_context* c, std::uint32_t reason, std::int32_t level) {
        sched(c).requestEnd(static_cast<EndReason>(reason), level);
    },
    [](kirima_context* c) { return sched(c).ending() ? 1 : 0; },
    [](kirima_context* c, const char* message) { sched(c).fail(message); },
    [](kirima_context* c, std::uint32_t s, std::uint32_t elem) {
        return fromC(c).store().element(s, elem);
    },
    [](kirima_context* c, std::uint32_t p, const kirima_wait_item* items, std::uint32_t n,
       const kirima_monitor_item* monitors, std::uint32_t m) {
        std::vector<MonitorSpec> specs;
        specs.reserve(m);
        for (std::uint32_t i = 0; i < m; ++i) {
            specs.push_back(specOf(monitors[i].monitor));
            specs.back().guard = monitors[i].guard;
            specs.back().guardFrame = monitors[i].guard_frame;
        }
        sched(c).waitAny(p, itemsOf(items, n), specs);
    },
};

}  // namespace

}  // namespace kirimasim::kernel

extern "C" const kirima_kernel_api* kirima_get_kernel_api(void) { return &kirimasim::kernel::kApi; }
