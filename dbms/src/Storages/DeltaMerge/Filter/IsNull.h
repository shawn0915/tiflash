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

#include <Storages/DeltaMerge/Filter/RSOperator.h>

namespace DB::DM
{

class IsNull : public RSOperator
{
    Attr attr;

public:
    explicit IsNull(const Attr & attr_)
        : attr(attr_)
    {
    }

    String name() override { return "isnull"; }

    Attrs getAttrs() override { return {attr}; }

    String toDebugString() override
    {
        return fmt::format(R"({{"op":"{}","col":"{}"}})", name(), attr.col_name);
    }

    RSResults roughCheck(size_t start_pack, size_t pack_count, const RSCheckParam & param) override
    {
        RSResults results(pack_count, RSResult::Some);
        GET_RSINDEX_FROM_PARAM_NOT_FOUND_RETURN_DIRECTLY(param, attr, rsindex, results);
        return rsindex.minmax->checkIsNull(start_pack, pack_count);
    }
};

} // namespace DB::DM
