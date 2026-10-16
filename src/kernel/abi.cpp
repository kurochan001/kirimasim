// The C function table of abi.h. Every entry forwards to the C++ API, so
// the table and the API have one meaning (RUNTIME_ABI.md 4.2).

#include "kirimasim/kernel/abi.h"

#include <span>
#include <vector>

#include "kirimasim/kernel/context.h"

namespace kirimasim::kernel {

namespace {

static_assert(static_cast<int>(Status::Waiting) == HS_WAITING);
static_assert(static_cast<int>(Status::Done) == HS_DONE);
static_assert(static_cast<int>(Status::Killed) == HS_KILLED);
static_assert(static_cast<int>(Status::Error) == HS_ERROR);
static_assert(static_cast<int>(Status::Stop) == HS_STOP);
static_assert(static_cast<int>(Edge::Both) == HS_EDGE_BOTH);
static_assert(static_cast<int>(Group::Reactive) == HS_GROUP_REACTIVE);
static_assert(static_cast<int>(Domain::Four) == HS_DOMAIN_FOUR);
static_assert(sizeof(hs_slot_part) == sizeof(SlotPart));

Scheduler& sched(hs_context* c) { return fromC(c).scheduler(); }

SlotPart partOf(const hs_slot_part& p) {
    return {p.slot, p.first_elem, p.num_elems, p.lsb, p.width};
}

constexpr hs_kernel_api kApi = {
    HS_ABI_MAJOR,
    HS_ABI_MINOR,
    [](hs_context* c, hs_resume_fn fn, void* frame, std::uint32_t group) {
        return sched(c).addProcess(fn, frame, static_cast<Group>(group));
    },
    [](hs_context* c, std::uint32_t p) { sched(c).start(p); },
    [](hs_context* c, std::uint32_t p, std::uint64_t ticks) { sched(c).waitDelay(p, ticks); },
    [](hs_context* c, std::uint32_t p, const hs_wait_item* items, std::uint32_t n) {
        std::vector<WaitItem> v;
        v.reserve(n);
        for (std::uint32_t i = 0; i < n; ++i)
            v.push_back({partOf(items[i].part), static_cast<Edge>(items[i].edge), items[i].guard,
                         items[i].guard_frame});
        sched(c).waitEvent(p, v);
    },
    [](hs_context* c, std::uint32_t p, const hs_monitor* m) {
        MonitorSpec spec;
        for (std::uint32_t i = 0; i < m->num_slots; ++i)
            spec.slots.push_back(partOf(m->slots[i]));
        spec.events.assign(m->events, m->events + m->num_events);
        spec.eval = m->eval;
        spec.frame = m->frame;
        spec.domain = static_cast<Domain>(m->domain);
        spec.width = m->width;
        spec.edge = static_cast<Edge>(m->edge);
        spec.untilTrue = m->until_true != 0;
        sched(c).waitMonitor(p, spec);
    },
    [](hs_context* c, std::uint32_t p, std::uint32_t e) { sched(c).waitNamed(p, e); },
    [](hs_context* c, std::uint32_t s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width,
       const std::uint64_t* value) { return sched(c).write(s, elem, lsb, width, value) ? 1 : 0; },
    [](hs_context* c, std::uint32_t p, std::uint32_t s, std::uint32_t elem, std::uint32_t lsb,
       std::uint32_t width, const std::uint64_t* value,
       std::uint64_t delay) { sched(c).scheduleNba(p, s, elem, lsb, width, value, delay); },
    [](hs_context* c, std::uint32_t e) { sched(c).trigger(e); },
    [](hs_context* c, std::uint32_t p, std::uint32_t e, std::uint64_t delay) {
        sched(c).triggerNb(p, e, delay);
    },
    [](hs_context* c, std::uint32_t e) { return sched(c).triggered(e) ? 1 : 0; },
    [](hs_context* c) { return sched(c).time(); },
    [](hs_context* c, std::uint32_t reason, std::int32_t level) {
        sched(c).requestEnd(static_cast<EndReason>(reason), level);
    },
    [](hs_context* c) { return sched(c).ending() ? 1 : 0; },
    [](hs_context* c, const char* message) { sched(c).fail(message); },
    [](hs_context* c, std::uint32_t s, std::uint32_t elem) {
        return fromC(c).store().element(s, elem);
    },
};

}  // namespace

}  // namespace kirimasim::kernel

extern "C" const hs_kernel_api* hs_get_kernel_api(void) { return &kirimasim::kernel::kApi; }
