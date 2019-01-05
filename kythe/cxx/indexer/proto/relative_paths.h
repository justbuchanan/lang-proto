/*
 * Copyright 2018 The Kythe Authors. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef KYTHE_CXX_INDEXER_PROTO_RELATIVE_PATHS_H_
#define KYTHE_CXX_INDEXER_PROTO_RELATIVE_PATHS_H_

#include <string>
#include <vector>

namespace kythe {
namespace proto {
class CompilationUnit;
}  // namespace proto

void ParsePathSubstitutions(
    const proto::CompilationUnit& unit,
    std::vector<std::pair<std::string, std::string>>* substitutions);


}

#endif // KYTHE_CXX_INDEXER_PROTO_RELATIVE_PATHS_H_
