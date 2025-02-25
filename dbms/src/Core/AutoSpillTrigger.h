// Copyright 2023 PingCAP, Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <Common/Exception.h>
#include <Common/MemoryTracker.h>
#include <Core/QueryOperatorSpillContexts.h>

namespace DB
{
class AutoSpillTrigger
{
public:
    AutoSpillTrigger(
        const MemoryTrackerPtr & memory_tracker_,
        const std::shared_ptr<QueryOperatorSpillContexts> & query_operator_spill_contexts_,
        float auto_memory_revoke_trigger_threshold,
        float auto_memory_revoke_target_threshold)
        : memory_tracker(memory_tracker_)
        , query_operator_spill_contexts(query_operator_spill_contexts_)
    {
        RUNTIME_CHECK_MSG(memory_tracker->getLimit() > 0, "Memory limit must be set for auto spill trigger");
        RUNTIME_CHECK_MSG(
            auto_memory_revoke_target_threshold >= 0 && auto_memory_revoke_trigger_threshold > 0,
            "Invalid auto trigger threshold {} or invalid auto target threshold {}",
            auto_memory_revoke_trigger_threshold,
            auto_memory_revoke_target_threshold);
        if (unlikely(auto_memory_revoke_trigger_threshold < auto_memory_revoke_target_threshold))
        {
            LOG_WARNING(
                query_operator_spill_contexts->getLogger(),
                "Auto trigger threshold {} less than auto target threshold {}, not valid, use default value instead",
                auto_memory_revoke_trigger_threshold,
                auto_memory_revoke_target_threshold);
            /// invalid value, set the value to default value
            auto_memory_revoke_trigger_threshold = 0.7;
            auto_memory_revoke_target_threshold = 0.5;
        }
        trigger_threshold = static_cast<Int64>(memory_tracker->getLimit() * auto_memory_revoke_trigger_threshold);
        target_threshold = static_cast<Int64>(memory_tracker->getLimit() * auto_memory_revoke_target_threshold);
    }

    void triggerAutoSpill()
    {
        auto current_memory_usage = memory_tracker->get();
        if (current_memory_usage > trigger_threshold)
        {
            query_operator_spill_contexts->triggerAutoSpill(current_memory_usage - target_threshold);
        }
    }

private:
    MemoryTrackerPtr memory_tracker;
    std::shared_ptr<QueryOperatorSpillContexts> query_operator_spill_contexts;
    Int64 trigger_threshold;
    Int64 target_threshold;
};
} // namespace DB
