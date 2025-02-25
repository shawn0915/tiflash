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

#include <Common/Exception.h>
#include <Flash/Pipeline/Schedule/Tasks/EventTask.h>

namespace DB
{
EventTask::EventTask(
    PipelineExecutorContext & exec_context_,
    const EventPtr & event_)
    : Task(exec_context_)
    , event(event_)
{
    RUNTIME_CHECK(event);
}

EventTask::EventTask(
    PipelineExecutorContext & exec_context_,
    const String & req_id,
    const EventPtr & event_,
    ExecTaskStatus init_status)
    : Task(exec_context_, req_id, init_status)
    , event(event_)
{
    RUNTIME_CHECK(event);
}

void EventTask::finalizeImpl()
{
    doFinalizeImpl();
    event->onTaskFinish(profile_info);
    event.reset();
}

UInt64 EventTask::getScheduleDuration() const
{
    return event->getScheduleDuration();
}
} // namespace DB
