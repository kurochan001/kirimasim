// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "service_tables.h"

#include "kirimasim/kernel/container_abi.h"
#include "kirimasim/kernel/event_abi.h"
#include "kirimasim/kernel/execution_abi.h"
#include "kirimasim/kernel/foreign_abi.h"
#include "kirimasim/kernel/io_abi.h"
#include "kirimasim/kernel/net_abi.h"
#include "kirimasim/kernel/object_abi.h"
#include "kirimasim/kernel/observation_abi.h"
#include "kirimasim/kernel/place_abi.h"
#include "kirimasim/kernel/presentation_abi.h"
#include "kirimasim/kernel/process_abi.h"
#include "kirimasim/kernel/reference_abi.h"
#include "kirimasim/kernel/run_policy_abi.h"
#include "kirimasim/kernel/scalar_task_abi.h"
#include "kirimasim/kernel/signal_abi.h"
#include "kirimasim/kernel/task_abi.h"
#include "kirimasim/kernel/text_abi.h"
#include "kirimasim/kernel/value_abi.h"

namespace kirimasim::kernel {
const void* queryService(std::uint32_t id, std::uint32_t version) noexcept {
    switch (id) {
        case KIRIMA_SERVICE_VALUE:
            return version == KIRIMA_VALUE_VERSION ? valueService() : nullptr;
        case KIRIMA_SERVICE_IO: return version == KIRIMA_IO_VERSION ? ioService() : nullptr;
        case KIRIMA_SERVICE_REFERENCE:
            return version == KIRIMA_REFERENCE_VERSION ? referenceService() : nullptr;
        case KIRIMA_SERVICE_PROCESS:
            return version == KIRIMA_PROCESS_VERSION ? processService() : nullptr;
        case KIRIMA_SERVICE_SCALAR_TASK:
            return version == KIRIMA_SCALAR_TASK_VERSION ? scalarTaskService() : nullptr;
        case KIRIMA_SERVICE_PRESENTATION:
            return version == KIRIMA_PRESENTATION_VERSION ? presentationService() : nullptr;
        case KIRIMA_SERVICE_CONTAINER:
            return version == KIRIMA_CONTAINER_VERSION ? containerService() : nullptr;
        case KIRIMA_SERVICE_OBJECT:
            return version == KIRIMA_OBJECT_VERSION ? objectService() : nullptr;
        case KIRIMA_SERVICE_EVENT:
            return version == KIRIMA_EVENT_VERSION ? eventService() : nullptr;
        case KIRIMA_SERVICE_SIGNAL: return signalService(version);
        case KIRIMA_SERVICE_TASK: return version == KIRIMA_TASK_VERSION ? taskService() : nullptr;
        case KIRIMA_SERVICE_OBSERVATION:
            return version == KIRIMA_OBSERVATION_VERSION ? observationService() : nullptr;
        case KIRIMA_SERVICE_NET: return version == KIRIMA_NET_VERSION ? netService() : nullptr;
        case KIRIMA_SERVICE_TEXT: return version == KIRIMA_TEXT_VERSION ? textService() : nullptr;
        case KIRIMA_SERVICE_FOREIGN:
            return version == KIRIMA_FOREIGN_VERSION ? foreignService() : nullptr;
        case KIRIMA_SERVICE_PLACE:
            return version == KIRIMA_PLACE_VERSION ? placeService() : nullptr;
        case KIRIMA_SERVICE_EXECUTION:
            return version == KIRIMA_EXECUTION_VERSION ? executionService() : nullptr;
        case KIRIMA_SERVICE_RUN_POLICY:
            return version == KIRIMA_RUN_POLICY_VERSION ? runPolicyService() : nullptr;
        default: return nullptr;
    }
}
}  // namespace kirimasim::kernel
