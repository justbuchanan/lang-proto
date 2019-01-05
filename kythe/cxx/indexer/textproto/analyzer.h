#ifndef KYTHE_CXX_INDEXER_TEXTPROTO_ANALYZER_H_
#define KYTHE_CXX_INDEXER_TEXTPROTO_ANALYZER_H_

#include <cstdio>
#include <functional>
#include <string>
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "kythe/cxx/common/file_vname_generator.h"
#include "kythe/cxx/common/indexing/KytheGraphRecorder.h"
#include "kythe/cxx/common/protobuf_metadata_file.h"
#include "kythe/cxx/common/utf8_line_index.h"
#include "kythe/proto/analysis.pb.h"

namespace kythe {
namespace lang_textproto {

// The canonical name for the textproto language in Kythe.
static const char kLanguageName[] = "protobuf_textformat";

/// Processes the textproto file described by @unit and emits graph facts to
/// @recorder.
/// \param unit The compilation unit specifying the textproto and the
/// protos that define its schema.
/// \param file_data The file contents of the textproto and relevant protos.
/// \param The name of the message type that defines the schema for the
/// textproto file (including namespace).
// TODO: should message_name be part of @unit?
void AnalyzeCompilationUnit(const proto::CompilationUnit& unit,
                            const std::vector<proto::FileData>& file_data,
                            std::string message_name,
                            const FileVNameGenerator& file_vnames,
                            KytheGraphRecorder* recorder);

};  // namespace lang_textproto
};  // namespace kythe

#endif  // KYTHE_CXX_INDEXER_TEXTPROTO_ANALYZER_H_
