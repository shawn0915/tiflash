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

#include <Common/Exception.h>
#include <Storages/Transaction/Types.h>

#include <magic_enum.hpp>

namespace DB
{

class RegionException : public Exception
{
public:
    // - region not found : This region does not exist or has been removed.
    // - region epoch not match : This region may has executed split/merge/change-peer command.
    enum class RegionReadStatus : UInt8
    {
        OK,
        NOT_FOUND, // reported by KVStore
        EPOCH_NOT_MATCH,
        NOT_LEADER,
        NOT_FOUND_TIKV, // reported by Proxy/TiKV
        OTHER,
    };

    using UnavailableRegions = std::unordered_set<RegionID>;

public:
    RegionException(UnavailableRegions && unavailable_region_, RegionReadStatus status_)
        : Exception(fmt::format(
            "Region error {}",
            magic_enum::enum_name(status_)))
        , unavailable_region(std::move(unavailable_region_))
        , status(status_)
    {}

    /// Region could be found with correct epoch, but unavailable (e.g. its lease in proxy has not been built with leader).
    UnavailableRegions unavailable_region;
    RegionReadStatus status;
};

} // namespace DB
