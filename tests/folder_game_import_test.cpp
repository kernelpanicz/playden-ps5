#include "runtime/folder_game_import.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
int main() {
  namespace fs = std::filesystem;
  const auto base = fs::temp_directory_path() / "playden-folder-import-test";
  std::error_code ec; fs::remove_all(base, ec); fs::create_directories(base / "Game" / "bin");
  { std::ofstream f(base / "Game" / "bin" / "game.exe"); f << "test"; }
  { std::ofstream f(base / "outside.exe"); f << "test"; }
  auto valid = playden::runtime::ValidateFolderGame("custom-1", "Test Game", base / "Game", "bin/game.exe");
  assert(valid.ok && valid.game.has_value());
  assert(!playden::runtime::ValidateFolderGame("bad/ID", "Test Game", base / "Game", "bin/game.exe").ok);
  assert(!playden::runtime::ValidateFolderGame("custom-2", "Test Game", base / "Game", "../outside.exe").ok);
  assert(!playden::runtime::ValidateFolderGame("custom-3", "Test Game", base / "Game", "missing.exe").ok);
  auto found = playden::runtime::DiscoverFolderExecutables(base / "Game");
  assert(found.size() == 1 && found.front() == "bin/game.exe");
  std::vector<playden::runtime::FolderGame> games{*valid.game};
  const auto catalog = base / "state" / "imported-games.json";
  std::string error;
  assert(playden::runtime::SaveFolderGameCatalog(catalog, games, &error));
  std::vector<playden::runtime::FolderGame> loaded;
  assert(playden::runtime::LoadFolderGameCatalog(catalog, &loaded, &error));
  assert(loaded.size() == 1 && loaded.front().id == "custom-1" && loaded.front().executable == "bin/game.exe");
  fs::remove_all(base, ec);
}
