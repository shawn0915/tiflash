// Copyright 2022 PingCAP, Ltd.
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

#include <DataStreams/IProfilingBlockInputStream.h>
#include <Interpreters/Join.h>

namespace DB
{
class HashJoinBuildBlockInputStream : public IProfilingBlockInputStream
{
    static constexpr auto NAME = "HashJoinBuild";

public:
    HashJoinBuildBlockInputStream(
        const BlockInputStreamPtr & input,
        JoinPtr join_,
        size_t stream_index_,
        const String & req_id)
        : stream_index(stream_index_)
        , log(Logger::get(req_id))
    {
        children.push_back(input);
        join = join_;
    }
    String getName() const override { return NAME; }
    Block getHeader() const override { return children.back()->getHeader(); }

protected:
    Block readImpl() override;
    void appendInfo(FmtBuffer & buffer) const override;

private:
    JoinPtr join;
    size_t stream_index;
    const LoggerPtr log;
};

} // namespace DB
