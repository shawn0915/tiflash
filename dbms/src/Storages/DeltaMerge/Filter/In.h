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

class In : public RSOperator
{
    Attr attr;
    Fields values;

public:
    In(const Attr & attr_, const Fields & values_)
        : attr(attr_)
        , values(values_)
    {
    }

    String name() override { return "in"; }

    Attrs getAttrs() override { return {attr}; }

    String toDebugString() override
    {
        String s = R"({"op":")" + name() + R"(","col":")" + attr.col_name + R"(","value":"[)";
        if (!values.empty())
        {
            for (auto & v : values)
                s += "\"" + applyVisitor(FieldVisitorToDebugString(), v) + "\",";
            s.pop_back();
        }
        return s + "]}";
    };

    RSResults roughCheck(size_t start_pack, size_t pack_count, const RSCheckParam & param) override
    {
        // If values is empty (for example where a in ()), all packs will not match.
        // So return none directly.
        if (values.empty())
            return RSResults(pack_count, RSResult::None);
        RSResults results(pack_count, RSResult::Some);
        GET_RSINDEX_FROM_PARAM_NOT_FOUND_RETURN_DIRECTLY(param, attr, rsindex, results);
        return rsindex.minmax->checkIn(start_pack, pack_count, values, rsindex.type);
    }
};

} // namespace DB::DM
