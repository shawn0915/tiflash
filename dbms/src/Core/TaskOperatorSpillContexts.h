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

#include <Core/OperatorSpillContext.h>

namespace DB
{
class TaskOperatorSpillContexts
{
public:
    Int64 triggerAutoSpill(Int64 expected_released_memories)
    {
        if (isFinished())
            return expected_released_memories;
        appendAdditionalOperatorSpillContexts();
        bool has_finished_operator_spill_contexts = false;
        for (auto & operator_spill_context : operator_spill_contexts)
        {
            assert(operator_spill_context->supportAutoTriggerSpill());
            if (operator_spill_context->spillableStageFinished())
            {
                has_finished_operator_spill_contexts = true;
                continue;
            }
            expected_released_memories = operator_spill_context->triggerSpill(expected_released_memories);
            if (expected_released_memories <= 0)
                break;
        }
        if (has_finished_operator_spill_contexts)
        {
            /// clean finished spill context
            operator_spill_contexts.erase(
                std::remove_if(
                    operator_spill_contexts.begin(),
                    operator_spill_contexts.end(),
                    [](const auto & context) { return context->spillableStageFinished(); }),
                operator_spill_contexts.end());
        }
        return expected_released_memories;
    }
    void registerOperatorSpillContext(const OperatorSpillContextPtr & operator_spill_context)
    {
        if (operator_spill_context->supportAutoTriggerSpill())
        {
            std::unique_lock lock(mutex);
            additional_operator_spill_contexts.push_back(operator_spill_context);
            has_additional_operator_spill_contexts = true;
        }
    }
    /// for tests
    size_t operatorSpillContextCount()
    {
        appendAdditionalOperatorSpillContexts();
        return operator_spill_contexts.size();
    }
    /// for tests
    size_t additionalOperatorSpillContextCount() const
    {
        std::unique_lock lock(mutex);
        return additional_operator_spill_contexts.size();
    }

    Int64 totalRevocableMemories()
    {
        if unlikely (isFinished())
            return 0;
        appendAdditionalOperatorSpillContexts();
        Int64 ret = 0;
        for (const auto & operator_spill_context : operator_spill_contexts)
            ret += operator_spill_context->getTotalRevocableMemory();
        return ret;
    }

    bool isFinished() const { return is_task_finished; }

    void finish() { is_task_finished = true; }

private:
    void appendAdditionalOperatorSpillContexts()
    {
        if (has_additional_operator_spill_contexts)
        {
            std::unique_lock lock(mutex);
            operator_spill_contexts.splice(operator_spill_contexts.end(), additional_operator_spill_contexts);
            has_additional_operator_spill_contexts = false;
            additional_operator_spill_contexts.clear();
        }
    }
    /// access to operator_spill_contexts is thread safe
    std::list<OperatorSpillContextPtr> operator_spill_contexts;
    mutable std::mutex mutex;
    /// access to additional_operator_spill_contexts need acquire lock first
    std::list<OperatorSpillContextPtr> additional_operator_spill_contexts;
    std::atomic<bool> has_additional_operator_spill_contexts{false};
    std::atomic<bool> is_task_finished{false};
};

} // namespace DB
