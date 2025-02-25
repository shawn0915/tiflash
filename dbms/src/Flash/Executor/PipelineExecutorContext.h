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

#include <Common/Logger.h>
#include <Common/MemoryTracker.h>
#include <Core/AutoSpillTrigger.h>
#include <Flash/Executor/ExecutionResult.h>
#include <Flash/Executor/ResultHandler.h>
#include <Flash/Executor/ResultQueue.h>
#include <Flash/Pipeline/Schedule/Tasks/TaskProfileInfo.h>

#include <atomic>
#include <exception>
#include <mutex>

namespace DB
{
class OperatorSpillContext;
using RegisterOperatorSpillContext = std::function<void(const std::shared_ptr<OperatorSpillContext> & ptr)>;
class PipelineExecutorContext : private boost::noncopyable
{
public:
    static constexpr auto timeout_err_msg = "error with timeout";

    // Only used for unit test.
    PipelineExecutorContext()
        : log(Logger::get())
        , mem_tracker(nullptr)
    {}

    PipelineExecutorContext(
        const String & query_id_,
        const String & req_id,
        const MemoryTrackerPtr & mem_tracker_,
        AutoSpillTrigger * auto_spill_trigger_ = nullptr,
        const RegisterOperatorSpillContext & register_operator_spill_context_ = nullptr)
        : query_id(query_id_)
        , log(Logger::get(req_id))
        , mem_tracker(mem_tracker_)
        , auto_spill_trigger(auto_spill_trigger_)
        , register_operator_spill_context(register_operator_spill_context_)
    {}

    ExecutionResult toExecutionResult();

    std::exception_ptr getExceptionPtr();
    String getExceptionMsg();

    void incActiveRefCount();

    void decActiveRefCount();

    void onErrorOccurred(const String & err_msg);
    void onErrorOccurred(const std::exception_ptr & exception_ptr_);

    void wait();

    template <typename Duration>
    void waitFor(const Duration & timeout_duration)
    {
        bool is_timeout = false;
        {
            std::unique_lock lock(mu);
            RUNTIME_ASSERT(isWaitMode());
            is_timeout = !cv.wait_for(lock, timeout_duration, [&] { return 0 == active_ref_count; });
        }
        if (is_timeout)
        {
            LOG_WARNING(log, "wait timeout");
            onErrorOccurred(timeout_err_msg);
            throw Exception(timeout_err_msg);
        }
        LOG_DEBUG(log, "query finished and wait done");
    }

    void consume(ResultHandler & result_handler);

    template <typename Duration>
    void consumeFor(ResultHandler & result_handler, const Duration & timeout_duration)
    {
        RUNTIME_ASSERT(result_handler);
        auto consumed_result_queue = getConsumedResultQueue();
        bool is_timeout = false;
        try
        {
            Block ret;
            while (true)
            {
                auto res = consumed_result_queue->popTimeout(ret, timeout_duration);
                if (res == MPMCQueueResult::TIMEOUT)
                {
                    is_timeout = true;
                    break;
                }
                else if (res == MPMCQueueResult::OK)
                {
                    result_handler(ret);
                }
                else
                {
                    break;
                }
            }
        }
        catch (...)
        {
            // If result_handler throws an error, here should notify the query to terminate, and wait for the end of the query.
            onErrorOccurred(std::current_exception());
        }
        if (is_timeout)
        {
            LOG_WARNING(log, "wait timeout");
            onErrorOccurred(timeout_err_msg);
            throw Exception(timeout_err_msg);
        }
        else
        {
            // In order to ensure that `decActiveRefCount` has finished calling at this point
            // and avoid referencing the already destructed `mu` in `decActiveRefCount`.
            std::unique_lock lock(mu);
            cv.wait(lock, [&] { return 0 == active_ref_count; });
        }
        LOG_DEBUG(log, "query finished and consume done");
    }

    void cancel();

    ALWAYS_INLINE bool isCancelled() { return is_cancelled.load(std::memory_order_acquire); }

    ResultQueuePtr toConsumeMode(size_t queue_size);

    void update(const TaskProfileInfo & task_profile_info) { query_profile_info.merge(task_profile_info); }

    const QueryProfileInfo & getQueryProfileInfo() const { return query_profile_info; }

    const String & getQueryId() const { return query_id; }

    const MemoryTrackerPtr & getMemoryTracker() const { return mem_tracker; }

    void triggerAutoSpill() const
    {
        if (auto_spill_trigger != nullptr)
            auto_spill_trigger->triggerAutoSpill();
    }

    void registerOperatorSpillContext(const std::shared_ptr<OperatorSpillContext> & operator_spill_context)
    {
        if (register_operator_spill_context != nullptr)
            register_operator_spill_context(operator_spill_context);
    }

    const RegisterOperatorSpillContext & getRegisterOperatorSpillContext() const
    {
        return register_operator_spill_context;
    }

private:
    bool setExceptionPtr(const std::exception_ptr & exception_ptr_);

    // Need to be called under lock.
    bool isWaitMode();

    ResultQueuePtr getConsumedResultQueue();

private:
    const String query_id;

    LoggerPtr log;

    MemoryTrackerPtr mem_tracker;

    std::mutex mu;
    std::condition_variable cv;
    std::exception_ptr exception_ptr;
    UInt32 active_ref_count{0};

    std::atomic_bool is_cancelled{false};

    bool is_finished{false};

    // `result_queue.finish` can only be called in `decActiveRefCount` because `result_queue.pop` cannot end until events end.
    std::optional<ResultQueuePtr> result_queue;

    QueryProfileInfo query_profile_info;

    AutoSpillTrigger * auto_spill_trigger;

    RegisterOperatorSpillContext register_operator_spill_context;
};
} // namespace DB
