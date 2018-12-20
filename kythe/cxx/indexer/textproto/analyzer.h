#ifndef KYTHE_CXX_INDEXER_TEXTPROTO_ANALYZER_H_
#define KYTHE_CXX_INDEXER_TEXTPROTO_ANALYZER_H_

#include <cstdio>
#include <string>
#include "google/protobuf/text_format.h"
#include "kythe/cxx/common/indexing/KytheGraphRecorder.h"
#include "kythe/cxx/common/utf8_line_index.h"
#include "kythe/proto/analysis.pb.h"

namespace kythe {
namespace lang_textproto {

// Analyzes a single textproto
class TextProtoAnalyzer {
 public:
  TextProtoAnalyzer(const proto::CompilationUnit* cu,
                    const std::vector<proto::FileData>* file_data,
                    std::string msgTypeName, KytheGraphRecorder* recorder)
      : compilation_unit_(cu),
        files_(file_data),
        msg_type_name_(msgTypeName),
        recorder_(recorder) {}

  void DoIt();

 private:
  void AddNode(const proto::VName& node_name, NodeKindID node_kind);
  proto::VName CreateAndAddAnchorNode(
      const proto::VName& file, const google::protobuf::FieldDescriptor* field,
      google::protobuf::TextFormat::ParseLocation loc);

  const proto::CompilationUnit* compilation_unit_;
  const std::vector<proto::FileData>* files_;
  const std::string msg_type_name_;
  KytheGraphRecorder* recorder_;
  std::unique_ptr<const UTF8LineIndex> line_index_;
};

// The canonical name for the textproto language in Kythe
static const char kLanguageName[] = "protobuf_textformat";

};  // namespace lang_textproto
};  // namespace kythe

#endif  // KYTHE_CXX_INDEXER_TEXTPROTO_ANALYZER_H_
