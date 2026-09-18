/// @file Registry.cpp
/// Publish credentials and package upload for `sere login` / `sere logout` /
/// `sere publish`.
///
/// Wire format follows https://sere-lang.com/docs/publishing-a-package:
/// `POST <registry>/api/packages` with `Authorization: Bearer <token>` and a
/// multipart body carrying the manifest, README, and packed `.slib`.

#include "sere/driver/Registry.h"

#include "sere/Version.h"
#include "sere/driver/Project.h"

#include <llvm/Support/JSON.h>
#include <llvm/Support/Program.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace sere {
namespace {

constexpr std::string_view TokenScheme = "sere_";
constexpr std::size_t MinPrefixLength = 4;
constexpr std::size_t MinSecretLength = 8;
constexpr std::size_t MaxResponseBytes = 4u * 1024u * 1024u;

// ---------------------------------------------------------------- text helpers

[[nodiscard]] bool isSpace(char ch) { return std::isspace(static_cast<unsigned char>(ch)) != 0; }

[[nodiscard]] std::string trimCopy(std::string text) {
  text.erase(text.begin(), std::find_if_not(text.begin(), text.end(), isSpace));
  text.erase(std::find_if_not(text.rbegin(), text.rend(), isSpace).base(), text.end());
  return text;
}

[[nodiscard]] std::string readFileBytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return {};
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

[[nodiscard]] std::string uniqueSuffix() {
  const auto ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count();
  return std::to_string(static_cast<long long>(ticks));
}

// ------------------------------------------------------------------ token shape

[[nodiscard]] bool isPrefixChar(char ch) {
  const unsigned char value = static_cast<unsigned char>(ch);
  return std::isalnum(value) != 0 || ch == '-' || ch == '_';
}

[[nodiscard]] bool isSecretChar(char ch) {
  const unsigned char value = static_cast<unsigned char>(ch);
  return std::isalnum(value) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == '+' ||
         ch == '/' || ch == '=';
}

/// Splits a token into prefix and secret when it has the documented shape.
[[nodiscard]] std::optional<std::pair<std::string_view, std::string_view>>
splitPublishToken(std::string_view token) {
  if (token.size() < TokenScheme.size() + MinPrefixLength + MinSecretLength + 1) {
    return std::nullopt;
  }
  if (token.compare(0, TokenScheme.size(), TokenScheme) != 0) {
    return std::nullopt;
  }
  const std::size_t prefixEnd = token.find('_', TokenScheme.size());
  if (prefixEnd == std::string_view::npos) {
    return std::nullopt;
  }
  const std::string_view prefix = token.substr(TokenScheme.size(), prefixEnd - TokenScheme.size());
  const std::string_view secret = token.substr(prefixEnd + 1);
  if (prefix.size() < MinPrefixLength || secret.size() < MinSecretLength) {
    return std::nullopt;
  }
  for (const char ch : prefix) {
    if (!isPrefixChar(ch)) {
      return std::nullopt;
    }
  }
  for (const char ch : secret) {
    if (!isSecretChar(ch)) {
      return std::nullopt;
    }
  }
  return std::pair<std::string_view, std::string_view>(prefix, secret);
}

// ----------------------------------------------------------------- credentials

struct StoredCredentials {
  std::string token;
  bool fromEnvironment = false;
  std::filesystem::path file;
};

[[nodiscard]] std::string tokenFromCredentialsText(const std::string& text) {
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    const std::string trimmed = trimCopy(line);
    if (trimmed.empty() || trimmed.front() == '#') {
      continue;
    }
    const std::size_t equals = trimmed.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    if (trimCopy(trimmed.substr(0, equals)) != "token") {
      continue;
    }
    std::string value = trimCopy(trimmed.substr(equals + 1));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
      value = value.substr(1, value.size() - 2);
    }
    return value;
  }
  return {};
}

[[nodiscard]] bool writeCredentials(const std::string& token, std::string& error) {
  const std::filesystem::path path = credentialsPath();
  std::error_code fsError;
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path(), fsError);
  }
  {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
      error = "cannot write '" + path.string() + "'";
      return false;
    }
    output << "# Sere registry credentials, written by `sere login`.\n"
           << "# Run `sere logout` to remove this token.\n"
           << "token = \"" << token << "\"\n";
    if (!output) {
      error = "cannot write '" + path.string() + "'";
      return false;
    }
  }
#ifndef _WIN32
  std::filesystem::permissions(path,
                               std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::replace, fsError);
#endif
  return true;
}

/// `SERE_TOKEN` when set, otherwise the stored credentials file.
[[nodiscard]] std::optional<StoredCredentials> resolveCredentials(std::string& error) {
  if (const char* fromEnv = std::getenv(RegistryTokenEnv);
      fromEnv != nullptr && *fromEnv != '\0') {
    StoredCredentials stored;
    stored.token = trimCopy(fromEnv);
    if (!splitPublishToken(stored.token).has_value()) {
      error = std::string(RegistryTokenEnv) +
              " is not shaped like a Sere publish token (sere_<prefix>_<secret>)";
      return std::nullopt;
    }
    stored.fromEnvironment = true;
    return stored;
  }
  const std::filesystem::path path = credentialsPath();
  std::error_code fsError;
  if (!std::filesystem::exists(path, fsError)) {
    error = "not signed in; run 'sere login <token>' first";
    return std::nullopt;
  }
  const std::string text = readFileBytes(path);
  StoredCredentials stored;
  stored.token = tokenFromCredentialsText(text);
  stored.file = path;
  if (stored.token.empty()) {
    error = "'" + path.string() + "' has no token; run 'sere login <token>'";
    return std::nullopt;
  }
  if (!splitPublishToken(stored.token).has_value()) {
    error = "'" + path.string() + "' holds a token that is not shaped like sere_<prefix>_<secret>";
    return std::nullopt;
  }
  return stored;
}

// ------------------------------------------------------------------- transport

struct HttpRequest {
  std::string method;
  std::string url;
  std::vector<std::string> headers;
  std::string body;
};

struct HttpResponse {
  bool reached = false;
  int status = 0;
  std::string body;
  std::string error;
};

#ifdef _WIN32

[[nodiscard]] std::wstring widen(const std::string& text) {
  if (text.empty()) {
    return std::wstring();
  }
  const int size = static_cast<int>(text.size());
  const int needed = MultiByteToWideChar(CP_UTF8, 0, text.data(), size, nullptr, 0);
  if (needed <= 0) {
    return std::wstring();
  }
  std::wstring wide(static_cast<std::size_t>(needed), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), size, wide.data(), needed);
  return wide;
}

[[nodiscard]] std::string narrow(const wchar_t* text, int length) {
  if (text == nullptr || length <= 0) {
    return std::string();
  }
  const int needed =
      WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
  if (needed <= 0) {
    return std::string();
  }
  std::string narrowed(static_cast<std::size_t>(needed), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, length, narrowed.data(), needed, nullptr, nullptr);
  return narrowed;
}

[[nodiscard]] std::string winErrorMessage(unsigned long code) {
  LPWSTR buffer = nullptr;
  const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                          FORMAT_MESSAGE_FROM_SYSTEM |
                                          FORMAT_MESSAGE_IGNORE_INSERTS,
                                      nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0,
                                      nullptr);
  if (buffer == nullptr) {
    return "Windows error " + std::to_string(code);
  }
  std::string message = trimCopy(narrow(buffer, static_cast<int>(length)));
  LocalFree(buffer);
  return message.empty() ? ("Windows error " + std::to_string(code)) : message;
}

/// Closes a WinHTTP handle exactly once, including on early returns.
class WinHttpHandle {
public:
  WinHttpHandle() = default;
  explicit WinHttpHandle(HINTERNET handle) : handle_(handle) {}
  ~WinHttpHandle() {
    if (handle_ != nullptr) {
      WinHttpCloseHandle(handle_);
    }
  }
  WinHttpHandle(const WinHttpHandle&) = delete;
  WinHttpHandle& operator=(const WinHttpHandle&) = delete;
  WinHttpHandle(WinHttpHandle&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }
  WinHttpHandle& operator=(WinHttpHandle&& other) noexcept {
    if (this != &other) {
      if (handle_ != nullptr) {
        WinHttpCloseHandle(handle_);
      }
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }
  [[nodiscard]] HINTERNET get() const { return handle_; }
  [[nodiscard]] bool valid() const { return handle_ != nullptr; }

private:
  HINTERNET handle_ = nullptr;
};

[[nodiscard]] HttpResponse sendWithWinHttp(const HttpRequest& request) {
  constexpr DWORD HostLength = 256;
  constexpr DWORD PathLength = 2048;
  constexpr DWORD ExtraLength = 1024;
  HttpResponse response;
  const std::wstring wideUrl = widen(request.url);
  const std::wstring wideMethod = widen(request.method);
  const std::wstring userAgent = widen("Sere/" SERE_VERSION_STRING);
  if (wideUrl.empty() || wideMethod.empty()) {
    response.error = "invalid request URL";
    return response;
  }
  wchar_t host[HostLength] = {};
  wchar_t path[PathLength] = {};
  wchar_t extra[ExtraLength] = {};
  URL_COMPONENTS parts;
  ZeroMemory(&parts, sizeof(parts));
  parts.dwStructSize = sizeof(parts);
  parts.lpszHostName = host;
  parts.dwHostNameLength = HostLength;
  parts.lpszUrlPath = path;
  parts.dwUrlPathLength = PathLength;
  parts.lpszExtraInfo = extra;
  parts.dwExtraInfoLength = ExtraLength;
  if (WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &parts) == FALSE) {
    response.error = "cannot parse registry URL '" + request.url + "': " + winErrorMessage(GetLastError());
    return response;
  }
  WinHttpHandle session(WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
  if (!session.valid()) {
    response.error = "cannot open an HTTP session: " + winErrorMessage(GetLastError());
    return response;
  }
  constexpr int TimeoutMs = 180000;
  WinHttpSetTimeouts(session.get(), TimeoutMs, TimeoutMs, TimeoutMs, TimeoutMs);
  WinHttpHandle connect(WinHttpConnect(session.get(), host, parts.nPort, 0));
  if (!connect.valid()) {
    response.error = "cannot connect to " + narrow(host, static_cast<int>(parts.dwHostNameLength)) +
                     ": " + winErrorMessage(GetLastError());
    return response;
  }
  DWORD flags = 0;
  if (parts.nScheme == INTERNET_SCHEME_HTTPS) {
    flags = WINHTTP_FLAG_SECURE;
  }
  wchar_t fullPath[3072] = {};
  if (parts.dwUrlPathLength > 0) {
    wcsncat_s(fullPath, std::size(fullPath), path, parts.dwUrlPathLength);
  }  if (parts.dwExtraInfoLength > 0) {
    wcsncat_s(fullPath, std::size(fullPath), extra, parts.dwExtraInfoLength);
  }
  WinHttpHandle handle(WinHttpOpenRequest(connect.get(), wideMethod.c_str(), fullPath, nullptr,
                                         WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
  if (!handle.valid()) {
    response.error = "cannot build the HTTP request: " + winErrorMessage(GetLastError());
    return response;
  }
  std::string joined;
  for (const std::string& header : request.headers) {
    joined += header + "\r\n";
  }
  const std::wstring wideHeaders = widen(joined);
  const DWORD bodyLength = static_cast<DWORD>(request.body.size());
  const BOOL sent =
      WinHttpSendRequest(handle.get(),
                         wideHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : wideHeaders.c_str(),
                         wideHeaders.empty() ? 0 : static_cast<DWORD>(-1),
                         request.body.empty() ? WINHTTP_NO_REQUEST_DATA
                                              : const_cast<char*>(request.body.data()),
                         bodyLength, bodyLength, 0);
  if (sent == FALSE || WinHttpReceiveResponse(handle.get(), nullptr) == FALSE) {
    response.error = "the request to " + request.url + " failed: " + winErrorMessage(GetLastError());
    return response;
  }
  DWORD status = 0;
  DWORD statusSize = sizeof(status);
  if (WinHttpQueryHeaders(handle.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                          WINHTTP_NO_HEADER_INDEX) == FALSE) {
    response.error = "cannot read the response status: " + winErrorMessage(GetLastError());
    return response;
  }
  response.reached = true;
  response.status = static_cast<int>(status);
  while (response.body.size() < MaxResponseBytes) {
    DWORD available = 0;
    if (WinHttpQueryDataAvailable(handle.get(), &available) == FALSE || available == 0) {
      break;
    }
    const std::size_t offset = response.body.size();
    response.body.resize(offset + available);
    DWORD read = 0;
    if (WinHttpReadData(handle.get(), response.body.data() + offset, available, &read) == FALSE ||
        read == 0) {
      response.body.resize(offset);
      break;
    }
    response.body.resize(offset + read);
  }
  return response;
}

#else

[[nodiscard]] HttpResponse sendWithCurl(const HttpRequest& request) {
  HttpResponse response;
  const std::optional<std::string> curl = llvm::sys::findProgramByName("curl");
  if (!curl.has_value()) {
    response.error = "curl is required to reach the registry on this platform";
    return response;
  }
  std::error_code fsError;
  const std::filesystem::path tempDir =
      std::filesystem::temp_directory_path(fsError) / ("sere-publish-" + uniqueSuffix());
  std::filesystem::create_directories(tempDir, fsError);
  if (fsError) {
    response.error = "cannot create a temporary directory";
    return response;
  }
  const std::filesystem::path bodyPath = tempDir / "body.bin";
  const std::filesystem::path responsePath = tempDir / "response.json";
  const std::filesystem::path statusPath = tempDir / "status.txt";
  const std::string statusFile = statusPath.string();
  std::vector<std::string> owned{*curl, "-sS", "--max-time", "180", "-X", request.method,
                                 "-H", "Expect:"};
  for (const std::string& header : request.headers) {
    owned.push_back("-H");
    owned.push_back(header);
  }
  if (!request.body.empty()) {
    std::ofstream body(bodyPath, std::ios::binary);
    body << request.body;
    if (!body) {
      std::filesystem::remove_all(tempDir, fsError);
      response.error = "cannot write the upload body";
      return response;
    }
    owned.push_back("--data-binary");
    owned.push_back("@" + bodyPath.string());
  }
  owned.push_back("-o");
  owned.push_back(responsePath.string());
  owned.push_back("-w");
  owned.push_back("%{http_code}");
  owned.push_back(request.url);
  std::vector<llvm::StringRef> args;
  args.reserve(owned.size());
  for (const std::string& item : owned) {
    args.push_back(item);
  }
  const std::vector<std::optional<llvm::StringRef>> redirects{std::nullopt,
                                                              llvm::StringRef(statusFile),
                                                              std::nullopt};
  std::string execError;
  const int code = llvm::sys::ExecuteAndWait(*curl, args, std::nullopt, redirects, 0, 0,
                                            &execError);
  if (code != 0) {
    std::filesystem::remove_all(tempDir, fsError);
    response.error = execError.empty() ? "curl failed to reach " + request.url : execError;
    return response;
  }
  response.body = readFileBytes(responsePath);
  const std::string status = trimCopy(readFileBytes(statusPath));
  std::filesystem::remove_all(tempDir, fsError);
  if (status.empty()) {
    response.error = "curl did not report an HTTP status";
    return response;
  }
  response.reached = true;
  response.status = std::atoi(status.c_str());
  return response;
}

#endif

[[nodiscard]] HttpResponse httpRequest(const HttpRequest& request) {
#ifdef _WIN32
  return sendWithWinHttp(request);
#else
  return sendWithCurl(request);
#endif
}

// ------------------------------------------------------------------ multipart

struct MultipartPart {
  std::string name;
  std::string filename;
  std::string contentType;
  std::string data;
};

[[nodiscard]] MultipartPart textPart(std::string name, const std::string& value) {
  MultipartPart part;
  part.name = std::move(name);
  part.data = value;
  return part;
}

[[nodiscard]] MultipartPart filePart(std::string name,
                                     const std::filesystem::path& path,
                                     std::string contentType) {
  MultipartPart part;
  part.name = std::move(name);
  part.filename = path.filename().string();
  part.contentType = std::move(contentType);
  part.data = readFileBytes(path);
  return part;
}

[[nodiscard]] std::string buildMultipart(const std::string& boundary,
                                         const std::vector<MultipartPart>& parts) {
  std::string body;
  for (const MultipartPart& part : parts) {
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"" + part.name + "\"";
    if (!part.filename.empty()) {
      body += "; filename=\"" + part.filename + "\"";
    }
    body += "\r\n";
    if (!part.contentType.empty()) {
      body += "Content-Type: " + part.contentType + "\r\n";
    }
    body += "\r\n";
    body += part.data;
    body += "\r\n";
  }
  body += "--" + boundary + "--\r\n";
  return body;
}

// ---------------------------------------------------------------- json replies

[[nodiscard]] std::string jsonStringField(const llvm::json::Object& object,
                                          std::string_view key) {
  if (const std::optional<llvm::StringRef> value = object.getString(key)) {
    return value->str();
  }
  return {};
}

[[nodiscard]] std::string firstJsonString(const llvm::json::Object& object,
                                          std::initializer_list<std::string_view> keys) {
  for (const std::string_view key : keys) {
    const std::string value = jsonStringField(object, key);
    if (!value.empty()) {
      return value;
    }
  }
  return {};
}

/// Human-readable reason for a rejected publish.
[[nodiscard]] std::string publishFailureReason(int status) {
  switch (status) {
    case 400: return "the registry rejected the package metadata";
    case 401: return "the publish token is missing, malformed, revoked, or expired";
    case 403: return "this token is not allowed to publish";
    case 409: return "that version already exists; bump the version in sere.toml";
    case 413: return "the archive is larger than the registry allows";
    case 415: return "the upload was not multipart/form-data";
    case 429: return "the registry rate limited this token; retry later";
    case 503: return "the registry is not configured yet; try again later";
    default: return "the registry returned HTTP " + std::to_string(status);
  }
}

/// Server-provided detail (`error`, `message`, or `detail`), or the raw body.
[[nodiscard]] std::string publishFailureDetail(const std::string& body) {
  llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(body);
  if (parsed) {
    if (const llvm::json::Object* object = parsed->getAsObject()) {
      const std::string detail = firstJsonString(*object, {"error", "message", "detail"});
      if (!detail.empty()) {
        return detail;
      }
    }
  }
  return trimCopy(body);
}

/// Registry replies may carry a relative link; make it clickable.
[[nodiscard]] std::string resolveRegistryLink(const std::string& baseUrl,
                                             const std::string& link) {
  if (link.empty() || link.starts_with("http://") || link.starts_with("https://")) {
    return link;
  }
  if (link.front() == '/') {
    return baseUrl + link;
  }
  return baseUrl + "/" + link;
}

} // namespace

std::filesystem::path credentialsPath() {
  if (const char* override = std::getenv(RegistryCredentialsEnv);
      override != nullptr && *override != '\0') {
    return std::filesystem::path(override);
  }
#ifdef _WIN32
  if (const char* local = std::getenv("LOCALAPPDATA"); local != nullptr && *local != '\0') {
    return std::filesystem::path(local) / "sere" / "credentials.toml";
  }
  if (const char* profile = std::getenv("USERPROFILE"); profile != nullptr && *profile != '\0') {
    return std::filesystem::path(profile) / ".sere" / "credentials.toml";
  }
#else
  if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0') {
    return std::filesystem::path(xdg) / "sere" / "credentials.toml";
  }
  if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
    return std::filesystem::path(home) / ".config" / "sere" / "credentials.toml";
  }
#endif
  return std::filesystem::current_path() / "sere-credentials.toml";
}

std::string registryBaseUrl() {
  std::string base;
  if (const char* override = std::getenv(RegistryUrlEnv);
      override != nullptr && *override != '\0') {
    base = trimCopy(override);
  }
  if (base.empty()) {
    base = std::string(DefaultRegistryUrl);
  }
  while (base.size() > 1 && base.back() == '/') {
    base.pop_back();
  }
  return base;
}

bool isPublishTokenShape(std::string_view token) {
  return splitPublishToken(token).has_value();
}

std::string maskPublishToken(std::string_view token) {
  const std::optional<std::pair<std::string_view, std::string_view>> parts =
      splitPublishToken(token);
  if (!parts.has_value()) {
    return std::string(TokenScheme) + "...";
  }
  return std::string(TokenScheme) + std::string(parts->first) + "_...";
}

int loginCommand(const CompilerOptions& options) {
  if (options.authToken.empty()) {
    std::string error;
    const std::optional<StoredCredentials> stored = resolveCredentials(error);
    if (!stored.has_value()) {
      std::cout << "not signed in to the Sere registry\n";
      std::cout << "  create a token: https://sere-lang.com/developers\n";
      std::cout << "  then run:       sere login <token>\n";
      return 0;
    }
    std::cout << "signed in to the Sere registry\n";
    std::cout << "  token: " << maskPublishToken(stored->token) << '\n';
    std::cout << "  from:  "
              << (stored->fromEnvironment ? std::string(RegistryTokenEnv)
                                          : stored->file.string())
              << '\n';
    return 0;
  }
  const std::string token = trimCopy(options.authToken);
  if (!isPublishTokenShape(token)) {
    llvm::errs() << "error: that does not look like a Sere publish token\n";
    llvm::errs() << "note: tokens look like sere_<prefix>_<secret>; create one at "
                    "https://sere-lang.com/developers\n";
    return 1;
  }
  std::string error;
  if (!writeCredentials(token, error)) {
    llvm::errs() << "error: " << error << '\n';
    return 1;
  }
  std::cout << "signed in to the Sere registry\n";
  std::cout << "  token:  " << maskPublishToken(token) << '\n';
  std::cout << "  stored: " << credentialsPath().string() << '\n';
  std::cout << "  publish a library with: sere publish\n";
  return 0;
}

int logoutCommand() {
  const std::filesystem::path path = credentialsPath();
  std::error_code fsError;
  if (std::filesystem::remove(path, fsError)) {
    std::cout << "signed out of the Sere registry (" << path.string() << " removed)\n";
  } else {
    std::cout << "no stored Sere registry token at " << path.string() << '\n';
  }
  const char* fromEnv = std::getenv(RegistryTokenEnv);
  if (fromEnv != nullptr && *fromEnv != '\0') {
    std::cout << "note: " << RegistryTokenEnv << " is still set in this environment\n";
  }
  return 0;
}

int publishCommand(const CompilerOptions& options) {
  const std::string baseUrl = registryBaseUrl();
  const std::string url = baseUrl + "/api/packages";
  std::string error;
  std::error_code fsError;
  const std::optional<std::filesystem::path> root =
      findProjectRoot(std::filesystem::current_path(fsError));
  if (!root.has_value()) {
    llvm::errs() << "error: no sere.toml found; run this from a library project "
                    "(sere init-lib <name>)\n";
    return 1;
  }
  ProjectManifest manifest;
  if (!loadProjectManifest(*root, manifest, error)) {
    llvm::errs() << "error: " << error << '\n';
    return 1;
  }
  if (manifest.kind != ProjectKind::Lib) {
    llvm::errs() << "error: '" << manifest.name
                 << "' is an application; only library projects publish "
                    "(sere init-lib <name>)\n";
    return 1;
  }
  const std::optional<StoredCredentials> credentials = resolveCredentials(error);
  if (!credentials.has_value()) {
    llvm::errs() << "error: " << error << '\n';
    llvm::errs() << "note: create a token at https://sere-lang.com/developers, then run "
                    "'sere login <token>'\n";
    return 1;
  }
  const int packed = packLibrary(options);
  if (packed != 0) {
    return packed;
  }
  const std::filesystem::path archive =
      options.outputPath.empty() ? manifest.output : options.outputPath;
  const std::uintmax_t archiveBytes = std::filesystem::file_size(archive, fsError);
  if (fsError) {
    llvm::errs() << "error: cannot read packed library '" << archive.string() << "'\n";
    return 1;
  }
  std::vector<MultipartPart> parts;
  parts.push_back(textPart("name", manifest.name));
  parts.push_back(textPart("version", manifest.version));
  const std::filesystem::path manifestFile = manifest.root / "sere.toml";
  if (std::filesystem::is_regular_file(manifestFile, fsError)) {
    parts.push_back(filePart("manifest", manifestFile, "application/toml"));
  }
  const std::filesystem::path readmeFile = manifest.root / "README.md";
  if (std::filesystem::is_regular_file(readmeFile, fsError)) {
    parts.push_back(filePart("readme", readmeFile, "text/markdown"));
  }
  parts.push_back(filePart("tarball", archive, "application/octet-stream"));
  const std::string boundary = "----SereFormBoundary" + uniqueSuffix();
  const std::string body = buildMultipart(boundary, parts);
  if (options.dryRun) {
    std::cout << "sere publish --dry-run " << manifest.name << " " << manifest.version << " -> "
              << url << '\n';
    for (const MultipartPart& part : parts) {
      std::cout << "  " << part.name << ": "
                << (part.filename.empty() ? part.data : part.filename) << " (" << part.data.size()
                << " bytes)\n";
    }
    std::cout << "  token: " << maskPublishToken(credentials->token) << '\n';
    std::cout << "nothing uploaded; drop --dry-run to publish\n";
    return 0;
  }
  std::cout << "sere publish " << manifest.name << " " << manifest.version << " (" << archiveBytes
            << " bytes) -> " << url << '\n';
  HttpRequest request;
  request.method = "POST";
  request.url = url;
  request.body = body;
  request.headers = {"Authorization: Bearer " + credentials->token,
                     "Accept: application/json",
                     "User-Agent: sere-cli",
                     "Content-Type: multipart/form-data; boundary=" + boundary};
  const HttpResponse response = httpRequest(request);
  if (!response.reached) {
    llvm::errs() << "error: " << response.error << '\n';
    return 1;
  }
  const std::string detail = publishFailureDetail(response.body);
  if (response.status != 200 && response.status != 201) {
    llvm::errs() << "error: " << publishFailureReason(response.status) << '\n';
    if (!detail.empty()) {
      llvm::errs() << "  " << detail << '\n';
    }
    if (response.status == 401) {
      llvm::errs() << "note: tokens are shown once; create a new one and run "
                      "'sere login <token>'\n";
    }
    return 1;
  }
  std::string name = manifest.name;
  std::string version = manifest.version;
  std::string install;
  std::string page;
  std::string checksum;
  if (const llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(response.body)) {
    if (const llvm::json::Object* object = parsed->getAsObject()) {
      const llvm::json::Object* published = nullptr;
      if (const llvm::json::Value* value = object->get("published")) {
        published = value->getAsObject();
      }
      const llvm::json::Object& source = published != nullptr ? *published : *object;
      name = jsonStringField(source, "name").empty() ? name : jsonStringField(source, "name");
      version = jsonStringField(source, "version").empty() ? version
                                                          : jsonStringField(source, "version");
      install = jsonStringField(source, "install");
      page = firstJsonString(source, {"url", "page"});
      checksum = jsonStringField(source, "checksumSha256");
    }
  }
  if (install.empty()) {
    install = "sere add " + name + "@" + version;
  }
  std::cout << "published " << name << " " << version << '\n';
  std::cout << "  install: " << install << '\n';
  if (!page.empty()) {
    std::cout << "  page:    " << resolveRegistryLink(baseUrl, page) << '\n';
  }
  if (!checksum.empty()) {
    std::cout << "  sha256:  " << checksum << '\n';
  }
  return 0;
}

} // namespace sere
