#include "relative_paths.h"

#include "kythe/cxx/common/path_utils.h"
#include "kythe/proto/analysis.pb.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"

namespace kythe {

namespace {
void AddPathSubstitutions(
    absl::string_view path_argument,
    std::vector<std::pair<std::string, std::string>>* substitutions) {
  std::vector<std::string> parts =
      absl::StrSplit(path_argument, ':', absl::SkipEmpty());
  for (const std::string& path_or_substitution : parts) {
    std::string::size_type equals_pos = path_or_substitution.find_first_of('=');
    if (equals_pos == std::string::npos) {
      substitutions->push_back(
          std::make_pair("", CleanPath(path_or_substitution)));
    } else {
      substitutions->push_back(std::make_pair(
          CleanPath(path_or_substitution.substr(0, equals_pos)),
          CleanPath(path_or_substitution.substr(equals_pos + 1))));
    }
  }
}
}


void ParsePathSubstitutions(
    const proto::CompilationUnit& unit,
    std::vector<std::pair<std::string, std::string>>* substitutions) {
  bool have_paths = false;
  bool expecting_path_arg = false;
  for (const std::string& argument : unit.argument()) {
    if (expecting_path_arg) {
      expecting_path_arg = false;
      AddPathSubstitutions(argument, substitutions);
      have_paths = true;
    } else if (argument == "-I" || argument == "--proto_path") {
      expecting_path_arg = true;
    } else {
      absl::string_view argument_value = argument;
      if (absl::ConsumePrefix(&argument_value, "-I")) {
        AddPathSubstitutions(argument_value, substitutions);
        have_paths = true;
      } else if (absl::ConsumePrefix(&argument_value, "--proto_path=")) {
        AddPathSubstitutions(argument_value, substitutions);
        have_paths = true;
      }
    }
  }
  if (!have_paths && !unit.working_directory().empty()) {
    substitutions->push_back(
        std::make_pair("", CleanPath(unit.working_directory())));
  }
}

}