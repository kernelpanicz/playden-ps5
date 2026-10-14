// Safe import catalog for Windows games already present on a PS5 volume.
// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
namespace playden::runtime {
struct FolderGame {
  std::string id, name;
  std::filesystem::path root;
  std::string executable, working_directory, arguments;
};
struct FolderImportResult { bool ok=false; std::string message; std::optional<FolderGame> game; };
// Validates the selected executable and ensures resolved paths remain inside root.
FolderImportResult ValidateFolderGame(const std::string& id, const std::string& name,
 const std::filesystem::path& root, const std::string& executable,
 const std::string& arguments="", const std::string& working_directory="");
std::vector<std::string> DiscoverFolderExecutables(const std::filesystem::path& root,
 std::size_t max_results=256);
bool LoadFolderGameCatalog(const std::filesystem::path& catalog, std::vector<FolderGame>* games, std::string* error);
bool SaveFolderGameCatalog(const std::filesystem::path& catalog, const std::vector<FolderGame>& games, std::string* error);
}
