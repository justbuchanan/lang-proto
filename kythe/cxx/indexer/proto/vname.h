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

#ifndef KYTHE_CXX_INDEXER_PROTO_VNAME_H_
#define KYTHE_CXX_INDEXER_PROTO_VNAME_H_

namespace kythe {

// Returns a VName for the given protobuf descriptor. Descriptors share
// various member names but do not participate in any sort of inheritance
// hierarchy, so we're stuck with a template.
template <typename SomeDescriptor>
proto::VName VNameForDescriptor(
    const SomeDescriptor* descriptor,
    const std::function<proto::VName(const std::string&)>& vname_for_rel_path) {
  proto::VName vname;
  class PathSink : public ::google::protobuf::io::AnnotationCollector {
   public:
    PathSink(const std::function<proto::VName(const std::string&)>&
                 vname_for_rel_path,
             proto::VName* vname)
        : vname_for_rel_path_(vname_for_rel_path), vname_(vname) {}

    void AddAnnotation(size_t begin_offset, size_t end_offset,
                       const std::string& file_path,
                       const std::vector<int>& path) override {
      *vname_ = VNameForProtoPath(vname_for_rel_path_(file_path), path);
    }

   private:
    const std::function<proto::VName(const std::string&)>& vname_for_rel_path_;
    proto::VName* vname_;
  } path_sink(vname_for_rel_path, &vname);
  // We'd really like to use GetLocationPath here, but it's private, so
  // we have to go through some contortions. On the plus side, this is the
  // *exact* same code that protoc backends use for writing out annotations,
  // so if AddAnnotation ever changes we'll know.
  std::string s;
  ::google::protobuf::io::StringOutputStream stream(&s);
  ::google::protobuf::io::Printer printer(&stream, '$', &path_sink);
  printer.Print("$0$", "0", "0");
  printer.Annotate("0", descriptor);
  return vname;
}

}  // namespace kythe

#endif  // KYTHE_CXX_INDEXER_PROTO_VNAME_H_
