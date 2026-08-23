/// @file ImportPath.cpp
/// Resolves dotted import paths and lists importable .sere modules.

#include "sere/driver/ImportPath.h"

#include "sere/driver/Library.h"

#include <algorithm>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace sere {
namespace {

[[nodiscard]] bool namesEqual(const std::filesystem::path& left, const std::filesystem::path& right) {
  std::error_code leftError;
  std::error_code rightError;
  const std::filesystem::path a = std::filesystem::weakly_canonical(left, leftError);
  const std::filesystem::path b = std::filesystem::weakly_canonical(right, rightError);
  return !leftError && !rightError && a == b;
}

[[nodiscard]] bool isHiddenName(std::string_view name) {
  return name.empty() || name.starts_with('.');
}

[[nodiscard]] bool isSkippedDirName(std::string_view name) {
  return name == "build" || name == "bin" || name == "dist" || name == "CMakeFiles" ||
         name == "node_modules" || name == "out" || name == "target" || name == ".git" ||
         name == ".sere-lib";
}

[[nodiscard]] bool isSereModuleFile(const std::filesystem::path& path) {
  const bool sere = path.extension() == ".sere";
  const bool slib = isSereLibraryFile(path);
  return (sere || slib) && path.stem() != "prelude" && !isHiddenName(path.stem().string());
}

[[nodiscard]] bool directoryHasSereFile(const std::filesystem::path& directory) {
  std::error_code error;
  const std::filesystem::directory_iterator end{};
  for (std::filesystem::directory_iterator it(directory, error);
       !error && it != end; it.increment(error)) {
    std::error_code inner;
    if (it->is_regular_file(inner) && isSereModuleFile(it->path())) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool pathIsUnder(const std::filesystem::path& file,
                               const std::filesystem::path& root) {
  if (root.empty() || file.empty()) {
    return false;
  }
  std::error_code error;
  const std::string fileText =
      std::filesystem::weakly_canonical(file, error).generic_string();
  const std::string rootText =
      std::filesystem::weakly_canonical(root, error).generic_string();
  if (error || rootText.empty() || fileText.size() < rootText.size()) {
    return false;
  }
  if (!fileText.starts_with(rootText)) {
    return false;
  }
  return fileText.size() == rootText.size() || fileText[rootText.size()] == '/';
}

[[nodiscard]] std::string sourceDetail(const std::filesystem::path& file,
                                       const std::filesystem::path& stdlibDir,
                                       bool isPackage) {
  const char* origin = pathIsUnder(file, stdlibDir) ? "stdlib" : "workspace";
  if (isPackage) {
    return std::string(origin) + " package";
  }
  if (isSereLibraryFile(file)) {
    return std::string(origin) + " library";
  }
  return std::string(origin) + " module";
}

void splitTypedPath(std::string_view typed,
                    std::vector<std::string>& parent,
                    std::string& last) {
  parent.clear();
  last.clear();
  if (typed.empty()) {
    return;
  }
  const bool trailingDot = typed.back() == '.';
  const std::string_view body = trailingDot ? typed.substr(0, typed.size() - 1) : typed;
  std::vector<std::string> parts = splitImportPath(body);
  if (trailingDot) {
    parent = std::move(parts);
    return;
  }
  if (parts.empty()) {
    return;
  }
  last = parts.back();
  parts.pop_back();
  parent = std::move(parts);
}

[[nodiscard]] std::filesystem::path nestedDir(const std::filesystem::path& root,
                                              const std::vector<std::string>& parent) {
  std::filesystem::path directory = root;
  for (const std::string& part : parent) {
    directory /= part;
  }
  return directory;
}

[[nodiscard]] std::string childImportPath(const std::vector<std::string>& parent,
                                         const std::string& name) {
  std::vector<std::string> parts = parent;
  parts.push_back(name);
  return joinImportPath(parts);
}

void considerEntry(std::vector<ImportModuleEntry>& out,
                   std::unordered_set<std::string>& seen,
                   const std::vector<std::string>& parent,
                   const std::string& name,
                   const std::filesystem::path& path,
                   const std::filesystem::path& stdlibDir,
                   const std::string& last,
                   bool isPackage) {
  if (!last.empty() && !name.starts_with(last)) {
    return;
  }
  const std::string dotted = childImportPath(parent, name);
  if (!seen.insert(dotted).second) {
    return;
  }
  ImportModuleEntry entry;
  entry.dottedName = dotted;
  entry.lastSegment = name;
  entry.detail = sourceDetail(path, stdlibDir, isPackage);
  entry.isPackage = isPackage;
  out.push_back(std::move(entry));
}

}  // namespace

std::vector<std::string> splitImportPath(std::string_view dotted) {
  std::vector<std::string> parts;
  std::string current;
  for (const char ch : dotted) {
    if (ch == '.') {
      if (!current.empty()) {
        parts.push_back(std::move(current));
        current.clear();
      }
      continue;
    }
    current.push_back(ch);
  }
  if (!current.empty()) {
    parts.push_back(std::move(current));
  }
  return parts;
}

std::string joinImportPath(const std::vector<std::string>& parts) {
  std::string text;
  for (const std::string& part : parts) {
    if (!text.empty()) {
      text += '.';
    }
    text += part;
  }
  return text;
}

void appendImportSearchDir(std::vector<std::filesystem::path>& dirs,
                           const std::filesystem::path& directory) {
  if (directory.empty()) {
    return;
  }
  for (const std::filesystem::path& existing : dirs) {
    if (namesEqual(existing, directory)) {
      return;
    }
  }
  dirs.push_back(directory);
}

void appendWorkspaceImportDirs(std::vector<std::filesystem::path>& dirs,
                               const std::filesystem::path& workspaceRoot) {
  if (workspaceRoot.empty()) {
    return;
  }
  appendImportSearchDir(dirs, workspaceRoot);
  appendImportSearchDir(dirs, workspaceRoot / "libs");
  appendImportSearchDir(dirs, workspaceRoot / "src");
}

std::vector<std::filesystem::path> importSearchDirs(const std::filesystem::path& originDir,
                                                    const std::filesystem::path& stdlibDir) {
  std::vector<std::filesystem::path> dirs;
  std::error_code error;
  const std::filesystem::path cwd = std::filesystem::current_path(error);
  std::filesystem::path origin = originDir;
  if (origin.empty()) {
    origin = cwd;
  }
  appendImportSearchDir(dirs, origin);
  if (!origin.empty()) {
    appendImportSearchDir(dirs, origin / "libs");
    appendImportSearchDir(dirs, origin.parent_path() / "libs");
  }
  appendImportSearchDir(dirs, cwd);
  appendImportSearchDir(dirs, cwd / "libs");
  appendImportSearchDir(dirs, cwd / "src");
  appendImportSearchDir(dirs, stdlibDir);
  return dirs;
}

std::filesystem::path resolveImportFile(const std::vector<std::filesystem::path>& searchDirs,
                                        const std::vector<std::string>& parts,
                                        const std::filesystem::path& skipFile,
                                        std::string* resolveError) {
  std::filesystem::path relative;
  for (const std::string& part : parts) {
    relative /= part;
  }
  const std::filesystem::path relativeSere = [&relative]() {
    std::filesystem::path path = relative;
    path += ".sere";
    return path;
  }();
  const std::filesystem::path relativeSlib = [&relative]() {
    std::filesystem::path path = relative;
    path += ".slib";
    return path;
  }();
  for (const std::filesystem::path& directory : searchDirs) {
    if (directory.empty()) {
      continue;
    }
    const std::filesystem::path candidate = directory / relativeSere;
    std::error_code error;
    if (std::filesystem::exists(candidate, error) && !error) {
      if (skipFile.empty() || !namesEqual(candidate, skipFile)) {
        return std::filesystem::weakly_canonical(candidate, error);
      }
    }
    const std::filesystem::path slib = directory / relativeSlib;
    if (std::filesystem::exists(slib, error) && !error) {
      if (skipFile.empty() || !namesEqual(slib, skipFile)) {
        std::string extractError;
        const std::filesystem::path entry = ensureLibraryExtracted(slib, extractError);
        if (!entry.empty()) {
          return entry;
        }
        if (resolveError != nullptr && resolveError->empty() && !extractError.empty()) {
          *resolveError = "cannot open library '" + slib.string() + "': " + extractError;
        }
      }
    }
    const std::filesystem::path folder = directory / relative;
    const std::filesystem::path folderEntry = folderLibraryEntry(folder);
    if (!folderEntry.empty() && (skipFile.empty() || !namesEqual(folderEntry, skipFile))) {
      return folderEntry;
    }
  }
  return {};
}

std::vector<ImportModuleEntry> listImportModules(
    const std::vector<std::filesystem::path>& searchDirs,
    const std::filesystem::path& stdlibDir,
    std::string_view typedPath,
    const std::filesystem::path& skipFile) {
  std::vector<std::string> parent;
  std::string last;
  splitTypedPath(typedPath, parent, last);
  std::vector<ImportModuleEntry> out;
  std::unordered_set<std::string> seen;
  for (const std::filesystem::path& searchDir : searchDirs) {
    if (searchDir.empty()) {
      continue;
    }
    const std::filesystem::path directory = nestedDir(searchDir, parent);
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error) || error) {
      continue;
    }
    const std::filesystem::directory_iterator end{};
    for (std::filesystem::directory_iterator it(directory, error);
         !error && it != end; it.increment(error)) {
      const std::filesystem::path path = it->path();
      const std::string name = path.filename().string();
      if (isHiddenName(name)) {
        continue;
      }
      std::error_code fileError;
      if (it->is_regular_file(fileError) && isSereModuleFile(path)) {
        if (!skipFile.empty() && namesEqual(path, skipFile)) {
          continue;
        }
        considerEntry(out, seen, parent, path.stem().string(), path, stdlibDir, last, false);
        continue;
      }
      std::error_code dirError;
      if (!it->is_directory(dirError) || isSkippedDirName(name)) {
        continue;
      }
      if (!stdlibDir.empty() && namesEqual(path, stdlibDir)) {
        continue;
      }
      if (!directoryHasSereFile(path) && folderLibraryEntry(path).empty()) {
        continue;
      }
      considerEntry(out, seen, parent, name, path, stdlibDir, last, true);
    }
  }
  std::sort(out.begin(), out.end(), [](const ImportModuleEntry& a, const ImportModuleEntry& b) {
    if (a.detail != b.detail) {
      return a.detail < b.detail;
    }
    return a.dottedName < b.dottedName;
  });
  return out;
}

}  // namespace sere
