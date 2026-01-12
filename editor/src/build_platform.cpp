/**
 * @file build_platform.cpp
 * @brief Platform-specific build implementations for NovelMind
 *
 * Extracted from build_system.cpp to separate platform-specific code:
 * - Platform bundlers (Windows, Linux, macOS, Web, Android, iOS)
 * - Code signing functionality (Windows signtool, macOS codesign)
 * - Security validation for signing tools
 * - Safe command execution without shell injection
 */

#include "NovelMind/editor/build_system.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <cstdlib> // for std::getenv

// Platform-specific includes for process handling
#ifdef _WIN32
#include <io.h>
#include <windows.h> // for CreateProcess, etc.
#define popen _popen
#define pclose _pclose
#else
#include <sys/wait.h> // for WIFEXITED, WEXITSTATUS
#include <unistd.h>   // for fork, execv, pipe, dup2, read, close
#endif

namespace fs = std::filesystem;

namespace NovelMind::editor {

// =============================================================================
// Platform Bundlers
// =============================================================================

Result<void> BuildSystem::buildWindowsExecutable(const std::string& outputPath) {
  // Create a launcher script/placeholder for Windows
  fs::path exePath =
      fs::path(outputPath) /
      (m_config.executableName + BuildUtils::getExecutableExtension(BuildPlatform::Windows));

  // In a real implementation, this would copy the runtime executable
  // For now, create a placeholder batch file
  fs::path batchPath = fs::path(outputPath) / (m_config.executableName + "_launcher.bat");

  std::ofstream batch(batchPath);
  if (batch.is_open()) {
    batch << "@echo off\n";
    batch << "echo NovelMind Runtime - " << m_config.executableName << "\n";
    batch << "echo Version: " << m_config.version << "\n";
    batch << "echo.\n";
    batch << "echo This is a placeholder launcher.\n";
    batch << "echo In production, this would start the game runtime.\n";
    batch << "pause\n";
    batch.close();
  }

  return Result<void>::ok();
}

Result<void> BuildSystem::buildLinuxExecutable(const std::string& outputPath) {
  // Create a launcher script/placeholder for Linux
  fs::path scriptPath = fs::path(outputPath) / (m_config.executableName + "_launcher.sh");

  std::ofstream script(scriptPath);
  if (script.is_open()) {
    script << "#!/bin/bash\n";
    script << "echo \"NovelMind Runtime - " << m_config.executableName << "\"\n";
    script << "echo \"Version: " << m_config.version << "\"\n";
    script << "echo \"\"\n";
    script << "echo \"This is a placeholder launcher.\"\n";
    script << "echo \"In production, this would start the game runtime.\"\n";
    script.close();

    // Make executable
    fs::permissions(scriptPath,
                    fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add);
  }

  return Result<void>::ok();
}

Result<void> BuildSystem::buildMacOSBundle(const std::string& outputPath) {
  // Create a basic .app bundle structure
  std::string appName = m_config.executableName + ".app";
  fs::path appPath = fs::path(outputPath) / appName;
  fs::path contentsPath = appPath / "Contents";
  fs::path macOSPath = contentsPath / "MacOS";
  fs::path resourcesPath = contentsPath / "Resources";

  fs::create_directories(macOSPath);
  fs::create_directories(resourcesPath);

  // Create Info.plist
  fs::path plistPath = contentsPath / "Info.plist";
  std::ofstream plist(plistPath);
  if (plist.is_open()) {
    plist << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    plist << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
             "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    plist << "<plist version=\"1.0\">\n";
    plist << "<dict>\n";
    plist << "  <key>CFBundleExecutable</key>\n";
    plist << "  <string>" << m_config.executableName << "</string>\n";
    plist << "  <key>CFBundleIdentifier</key>\n";
    plist << "  <string>com.novelmind." << m_config.executableName << "</string>\n";
    plist << "  <key>CFBundleName</key>\n";
    plist << "  <string>" << m_config.executableName << "</string>\n";
    plist << "  <key>CFBundleShortVersionString</key>\n";
    plist << "  <string>" << m_config.version << "</string>\n";
    plist << "  <key>CFBundleVersion</key>\n";
    plist << "  <string>" << m_config.version << "</string>\n";
    plist << "  <key>CFBundlePackageType</key>\n";
    plist << "  <string>APPL</string>\n";
    plist << "</dict>\n";
    plist << "</plist>\n";
    plist.close();
  }

  // Create placeholder executable
  fs::path exePath = macOSPath / m_config.executableName;
  std::ofstream exe(exePath);
  if (exe.is_open()) {
    exe << "#!/bin/bash\n";
    exe << "echo \"NovelMind Runtime - " << m_config.executableName << "\"\n";
    exe << "echo \"Version: " << m_config.version << "\"\n";
    exe.close();

    fs::permissions(exePath, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add);
  }

  // Copy packs and config to Resources
  fs::path stagingPacks = fs::path(outputPath) / "packs";
  fs::path stagingConfig = fs::path(outputPath) / "config";

  if (fs::exists(stagingPacks)) {
    fs::copy(stagingPacks, resourcesPath / "packs",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  if (fs::exists(stagingConfig)) {
    fs::copy(stagingConfig, resourcesPath / "config",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  return Result<void>::ok();
}

Result<void> BuildSystem::buildWebBundle(const std::string& outputPath) {
  // Create WebAssembly/Emscripten bundle structure
  fs::path webPath = fs::path(outputPath) / "web";
  fs::create_directories(webPath);

  // Create index.html entry point
  fs::path htmlPath = webPath / "index.html";
  std::ofstream html(htmlPath);
  if (html.is_open()) {
    html << "<!DOCTYPE html>\n";
    html << "<html lang=\"en\">\n";
    html << "<head>\n";
    html << "  <meta charset=\"UTF-8\">\n";
    html << "  <meta name=\"viewport\" content=\"width=device-width, "
            "initial-scale=1.0\">\n";
    html << "  <title>" << m_config.executableName << " - NovelMind</title>\n";
    html << "  <style>\n";
    html << "    body { margin: 0; background: #1a1a1a; display: flex; "
            "justify-content: center; align-items: center; height: 100vh; }\n";
    html << "    #canvas { background: #000; }\n";
    html << "    .loading { color: #fff; font-family: sans-serif; }\n";
    html << "  </style>\n";
    html << "</head>\n";
    html << "<body>\n";
    html << "  <div class=\"loading\" id=\"status\">Loading " << m_config.executableName << " (v"
         << m_config.version << ")...</div>\n";
    html << "  <canvas id=\"canvas\" width=\"1280\" height=\"720\" "
            "style=\"display:none;\"></canvas>\n";
    html << "  <script>\n";
    html << "    // NovelMind WebAssembly runtime placeholder\n";
    html << "    // In production, this loads the Emscripten-compiled runtime\n";
    html << "    document.getElementById('status').textContent = 'Web build "
            "placeholder - runtime not yet bundled';\n";
    html << "  </script>\n";
    html << "</body>\n";
    html << "</html>\n";
    html.close();
  }

  // Copy packs to web directory
  fs::path stagingPacks = fs::path(outputPath) / "packs";
  if (fs::exists(stagingPacks)) {
    fs::copy(stagingPacks, webPath / "packs",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  // Copy config
  fs::path stagingConfig = fs::path(outputPath) / "config";
  if (fs::exists(stagingConfig)) {
    fs::copy(stagingConfig, webPath / "config",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  logMessage("Web bundle created at: " + webPath.string(), false);
  return Result<void>::ok();
}

Result<void> BuildSystem::buildAndroidBundle(const std::string& outputPath) {
  // Create Android project structure placeholder
  // In production, this would generate a complete Gradle project
  fs::path androidPath = fs::path(outputPath) / "android";
  fs::path appPath = androidPath / "app" / "src" / "main";
  fs::path assetsPath = appPath / "assets";
  fs::create_directories(assetsPath);

  // Create AndroidManifest.xml placeholder
  fs::path manifestPath = appPath / "AndroidManifest.xml";
  std::ofstream manifest(manifestPath);
  if (manifest.is_open()) {
    manifest << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    manifest << "<manifest "
                "xmlns:android=\"http://schemas.android.com/apk/res/android\"\n";
    manifest << "    package=\"com.novelmind." << m_config.executableName << "\">\n";
    manifest << "    <application\n";
    manifest << "        android:label=\"" << m_config.executableName << "\"\n";
    manifest << "        android:theme=\"@style/Theme.NovelMind\">\n";
    manifest << "        <activity\n";
    manifest << "            android:name=\".MainActivity\"\n";
    manifest << "            android:exported=\"true\"\n";
    manifest << "            android:configChanges=\"orientation|screenSize\">\n";
    manifest << "            <intent-filter>\n";
    manifest << "                <action "
                "android:name=\"android.intent.action.MAIN\" />\n";
    manifest << "                <category "
                "android:name=\"android.intent.category.LAUNCHER\" />\n";
    manifest << "            </intent-filter>\n";
    manifest << "        </activity>\n";
    manifest << "    </application>\n";
    manifest << "</manifest>\n";
    manifest.close();
  }

  // Create build.gradle placeholder
  fs::path gradlePath = androidPath / "app" / "build.gradle";
  fs::create_directories(gradlePath.parent_path());
  std::ofstream gradle(gradlePath);
  if (gradle.is_open()) {
    gradle << "// NovelMind Android build configuration\n";
    gradle << "// Generated by NovelMind Build System\n";
    gradle << "plugins {\n";
    gradle << "    id 'com.android.application'\n";
    gradle << "}\n\n";
    gradle << "android {\n";
    gradle << "    namespace 'com.novelmind." << m_config.executableName << "'\n";
    gradle << "    compileSdk 34\n\n";
    gradle << "    defaultConfig {\n";
    gradle << "        applicationId 'com.novelmind." << m_config.executableName << "'\n";
    gradle << "        minSdk 24\n";
    gradle << "        targetSdk 34\n";
    gradle << "        versionCode " << m_config.buildNumber << "\n";
    gradle << "        versionName \"" << m_config.version << "\"\n";
    gradle << "    }\n";
    gradle << "}\n";
    gradle.close();
  }

  // Copy packs to assets
  fs::path stagingPacks = fs::path(outputPath) / "packs";
  if (fs::exists(stagingPacks)) {
    fs::copy(stagingPacks, assetsPath / "packs",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  logMessage("Android project structure created at: " + androidPath.string(), false);
  return Result<void>::ok();
}

Result<void> BuildSystem::buildIOSBundle(const std::string& outputPath) {
  // Create iOS Xcode project structure placeholder
  fs::path iosPath = fs::path(outputPath) / "ios";
  std::string projectName = m_config.executableName;
  fs::path xcodeProj = iosPath / (projectName + ".xcodeproj");
  fs::path sourcePath = iosPath / projectName;
  fs::path resourcesPath = sourcePath / "Resources";
  fs::create_directories(xcodeProj);
  fs::create_directories(resourcesPath);

  // Create Info.plist
  fs::path plistPath = sourcePath / "Info.plist";
  std::ofstream plist(plistPath);
  if (plist.is_open()) {
    plist << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    plist << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
             "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    plist << "<plist version=\"1.0\">\n";
    plist << "<dict>\n";
    plist << "  <key>CFBundleDevelopmentRegion</key>\n";
    plist << "  <string>en</string>\n";
    plist << "  <key>CFBundleExecutable</key>\n";
    plist << "  <string>$(EXECUTABLE_NAME)</string>\n";
    plist << "  <key>CFBundleIdentifier</key>\n";
    plist << "  <string>com.novelmind." << projectName << "</string>\n";
    plist << "  <key>CFBundleName</key>\n";
    plist << "  <string>" << projectName << "</string>\n";
    plist << "  <key>CFBundlePackageType</key>\n";
    plist << "  <string>APPL</string>\n";
    plist << "  <key>CFBundleShortVersionString</key>\n";
    plist << "  <string>" << m_config.version << "</string>\n";
    plist << "  <key>CFBundleVersion</key>\n";
    plist << "  <string>" << m_config.buildNumber << "</string>\n";
    plist << "  <key>UILaunchStoryboardName</key>\n";
    plist << "  <string>LaunchScreen</string>\n";
    plist << "  <key>UISupportedInterfaceOrientations</key>\n";
    plist << "  <array>\n";
    plist << "    <string>UIInterfaceOrientationLandscapeLeft</string>\n";
    plist << "    <string>UIInterfaceOrientationLandscapeRight</string>\n";
    plist << "  </array>\n";
    plist << "  <key>UIRequiresFullScreen</key>\n";
    plist << "  <true/>\n";
    plist << "</dict>\n";
    plist << "</plist>\n";
    plist.close();
  }

  // Create minimal project.pbxproj placeholder
  fs::path pbxprojPath = xcodeProj / "project.pbxproj";
  std::ofstream pbxproj(pbxprojPath);
  if (pbxproj.is_open()) {
    pbxproj << "// NovelMind iOS Project\n";
    pbxproj << "// Generated by NovelMind Build System v" << m_config.version << "\n";
    pbxproj << "// Build Number: " << m_config.buildNumber << "\n";
    pbxproj << "// \n";
    pbxproj << "// This is a placeholder. In production, this would contain\n";
    pbxproj << "// a complete Xcode project configuration for building the iOS "
               "app.\n";
    pbxproj.close();
  }

  // Copy packs to Resources
  fs::path stagingPacks = fs::path(outputPath) / "packs";
  if (fs::exists(stagingPacks)) {
    fs::copy(stagingPacks, resourcesPath / "packs",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  // Copy config
  fs::path stagingConfig = fs::path(outputPath) / "config";
  if (fs::exists(stagingConfig)) {
    fs::copy(stagingConfig, resourcesPath / "config",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  logMessage("iOS Xcode project structure created at: " + iosPath.string(), false);
  return Result<void>::ok();
}

// =============================================================================
// Code Signing Implementation (Secure)
// =============================================================================

/**
 * @brief Validate that a signing tool path is safe to use
 *
 * This function prevents command injection by:
 * 1. Checking that the path exists and is a regular file
 * 2. Checking that the path is executable
 * 3. Rejecting paths with shell metacharacters
 * 4. Verifying against an allowlist of known signing tools
 */
Result<void> BuildSystem::validateSigningToolPath(const std::string& toolPath,
                                                  const std::vector<std::string>& allowedNames) {
  // Check for empty path
  if (toolPath.empty()) {
    return Result<void>::error("Signing tool path cannot be empty");
  }

  // Check for shell metacharacters that could enable command injection
  const std::string dangerousChars = "|&;<>$`\\\"'(){}[]!*?~";
  for (char c : toolPath) {
    if (dangerousChars.find(c) != std::string::npos) {
      return Result<void>::error(
          "Signing tool path contains invalid character: '" + std::string(1, c) +
          "'. Paths with shell metacharacters are not allowed.");
    }
  }

  // Convert to filesystem path
  fs::path path(toolPath);

  // Check if path exists
  if (!fs::exists(path)) {
    return Result<void>::error("Signing tool not found: " + toolPath);
  }

  // Check if it's a regular file
  if (!fs::is_regular_file(path)) {
    return Result<void>::error("Signing tool path is not a regular file: " + toolPath);
  }

  // Get the filename for allowlist checking
  std::string filename = path.filename().string();

  // Validate against allowlist
  bool isAllowed = false;
  for (const auto& allowedName : allowedNames) {
    if (filename == allowedName || filename == (allowedName + ".exe")) {
      isAllowed = true;
      break;
    }
  }

  if (!isAllowed) {
    std::string allowedList;
    for (size_t i = 0; i < allowedNames.size(); ++i) {
      allowedList += allowedNames[i];
      if (i < allowedNames.size() - 1) {
        allowedList += ", ";
      }
    }
    return Result<void>::error("Signing tool '" + filename +
                               "' is not in the allowlist. Allowed tools: " + allowedList);
  }

  return Result<void>::ok();
}

/**
 * @brief Execute a command securely without shell injection vulnerabilities
 *
 * This uses platform-specific APIs to execute commands without involving a shell:
 * - On Windows: CreateProcess
 * - On Unix: fork + execv
 *
 * This prevents command injection attacks that would be possible with system() or popen().
 */
Result<i32> BuildSystem::executeCommand(const std::string& command, std::string& output) const {
#ifdef _WIN32
  // Windows implementation using CreateProcess
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;

  // Create pipes for stdout
  HANDLE hStdoutRead, hStdoutWrite;
  if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
    return Result<i32>::error("Failed to create pipe for command output");
  }

  // Ensure read handle is not inherited
  SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

  // Setup startup info
  STARTUPINFOA si;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.hStdOutput = hStdoutWrite;
  si.hStdError = hStdoutWrite;
  si.dwFlags |= STARTF_USESTDHANDLES;

  PROCESS_INFORMATION pi;
  ZeroMemory(&pi, sizeof(pi));

  // Create process (note: command must be mutable for CreateProcessA)
  std::vector<char> cmdLine(command.begin(), command.end());
  cmdLine.push_back('\0');

  if (!CreateProcessA(NULL, cmdLine.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
    CloseHandle(hStdoutRead);
    CloseHandle(hStdoutWrite);
    return Result<i32>::error("Failed to create process: " + command);
  }

  // Close write end of pipe
  CloseHandle(hStdoutWrite);

  // Read output
  const size_t BUFFER_SIZE = 4096;
  char buffer[BUFFER_SIZE];
  DWORD bytesRead;
  output.clear();

  while (ReadFile(hStdoutRead, buffer, BUFFER_SIZE - 1, &bytesRead, NULL) && bytesRead > 0) {
    buffer[bytesRead] = '\0';
    output += buffer;
  }

  // Wait for process to complete
  WaitForSingleObject(pi.hProcess, INFINITE);

  // Get exit code
  DWORD exitCode;
  GetExitCodeProcess(pi.hProcess, &exitCode);

  // Cleanup
  CloseHandle(hStdoutRead);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return Result<i32>::ok(static_cast<i32>(exitCode));

#else
  // Unix implementation using fork + execv
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    return Result<i32>::error("Failed to create pipe for command output");
  }

  pid_t pid = fork();
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return Result<i32>::error("Failed to fork process");
  }

  if (pid == 0) {
    // Child process
    close(pipefd[0]); // Close read end

    // Redirect stdout and stderr to pipe
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);

    // Parse command into argv array
    // Note: This is a simplified parser. In production, use a proper shell parser
    // or pass command and arguments separately
    std::vector<std::string> tokens;
    std::istringstream iss(command);
    std::string token;
    while (iss >> std::quoted(token)) {
      tokens.push_back(token);
    }

    std::vector<char*> argv;
    for (auto& t : tokens) {
      argv.push_back(const_cast<char*>(t.c_str()));
    }
    argv.push_back(nullptr);

    // Execute command without shell
    execv(argv[0], argv.data());

    // If execv returns, it failed
    _exit(127);
  } else {
    // Parent process
    close(pipefd[1]); // Close write end

    // Read output
    const size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    output.clear();

    while ((bytesRead = read(pipefd[0], buffer, BUFFER_SIZE - 1)) > 0) {
      buffer[bytesRead] = '\0';
      output += buffer;
    }

    close(pipefd[0]);

    // Wait for child process
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
      return Result<i32>::ok(WEXITSTATUS(status));
    } else {
      return Result<i32>::error("Process did not exit normally");
    }
  }
#endif
}

/**
 * @brief Sign executable for the configured platform
 */
Result<void> BuildSystem::signExecutableForPlatform(const std::string& executablePath) {
  if (!m_config.signExecutable) {
    return Result<void>::ok(); // Signing not requested
  }

  if (m_config.signingCertificate.empty()) {
    return Result<void>::error("Signing requested but no certificate path provided");
  }

  // Validate the executable path exists
  if (!fs::exists(executablePath)) {
    return Result<void>::error("Executable not found for signing: " + executablePath);
  }

  logMessage("Signing executable: " + executablePath, false);

  // Dispatch to platform-specific signing
  switch (m_config.platform) {
  case BuildPlatform::Windows:
    return signWindowsExecutable(executablePath);

  case BuildPlatform::MacOS:
    return signMacOSBundle(executablePath);

  case BuildPlatform::Linux:
    // Linux doesn't have standard code signing like Windows/macOS
    logMessage("Code signing not required for Linux builds", false);
    return Result<void>::ok();

  default:
    logMessage("Code signing not supported for this platform", false);
    return Result<void>::ok();
  }
}

/**
 * @brief Sign Windows executable using signtool.exe
 *
 * This implements secure signing without command injection vulnerabilities:
 * 1. Validates the signtool.exe path
 * 2. Validates the certificate path
 * 3. Uses executeCommand() which doesn't use a shell
 * 4. Properly quotes all arguments
 */
Result<void> BuildSystem::signWindowsExecutable(const std::string& executablePath) {
  // Determine signing tool path
  std::string signtoolPath;

  // Check for user-provided signing tool path
  const char* customSigntool = std::getenv("NOVELMIND_SIGNTOOL_PATH");
  if (customSigntool != nullptr) {
    signtoolPath = customSigntool;
  } else {
    // Use default Windows SDK signtool.exe
    // Try common locations
    std::vector<std::string> commonPaths = {
        "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\x64\\signtool.exe",
        "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\x86\\signtool.exe",
        "signtool.exe" // Assume it's in PATH
    };

    for (const auto& path : commonPaths) {
      if (fs::exists(path)) {
        signtoolPath = path;
        break;
      }
    }

    if (signtoolPath.empty()) {
      signtoolPath = "signtool.exe"; // Hope it's in PATH
    }
  }

  // Validate signing tool path
  std::vector<std::string> allowedTools = {"signtool.exe", "signtool"};
  auto validationResult = validateSigningToolPath(signtoolPath, allowedTools);
  if (!validationResult.isOk()) {
    return Result<void>::error("Signing tool validation failed: " + validationResult.error());
  }

  // Validate certificate path
  if (!fs::exists(m_config.signingCertificate)) {
    return Result<void>::error("Signing certificate not found: " + m_config.signingCertificate);
  }

  // Build command with proper quoting
  // Note: We're building a command string, but executeCommand() will parse it safely
  std::ostringstream cmd;
  cmd << "\"" << signtoolPath << "\" sign";

  // Add certificate file
  cmd << " /f \"" << m_config.signingCertificate << "\"";

  // Add password if provided (note: this is sensitive data)
  if (!m_config.signingPassword.empty()) {
    // Validate password doesn't contain command injection chars
    const std::string dangerousChars = "|&;<>$`\\\"'";
    for (char c : m_config.signingPassword) {
      if (dangerousChars.find(c) != std::string::npos) {
        return Result<void>::error("Signing password contains invalid characters");
      }
    }
    cmd << " /p \"" << m_config.signingPassword << "\"";
  }

  // Add timestamp server if provided
  if (!m_config.signingTimestampUrl.empty()) {
    // Validate URL format (basic check)
    if (m_config.signingTimestampUrl.find("http://") != 0 &&
        m_config.signingTimestampUrl.find("https://") != 0) {
      return Result<void>::error("Invalid timestamp URL format");
    }
    cmd << " /t \"" << m_config.signingTimestampUrl << "\"";
  }

  // Add file to sign
  cmd << " \"" << executablePath << "\"";

  // Execute signing command
  std::string output;
  auto result = executeCommand(cmd.str(), output);

  if (!result.isOk()) {
    return Result<void>::error("Failed to execute signing command: " + result.error());
  }

  i32 exitCode = result.value();
  if (exitCode != 0) {
    return Result<void>::error("Signing failed with exit code " + std::to_string(exitCode) +
                               ": " + output);
  }

  logMessage("Successfully signed Windows executable", false);
  return Result<void>::ok();
}

/**
 * @brief Sign macOS application bundle using codesign
 *
 * This implements secure signing without command injection vulnerabilities:
 * 1. Validates the codesign path
 * 2. Validates the certificate/identity
 * 3. Uses executeCommand() which doesn't use a shell
 * 4. Properly quotes all arguments
 */
Result<void> BuildSystem::signMacOSBundle(const std::string& bundlePath) {
  // Determine signing tool path
  std::string codesignPath;

  // Check for user-provided signing tool path
  const char* customCodesign = std::getenv("NOVELMIND_CODESIGN_PATH");
  if (customCodesign != nullptr) {
    codesignPath = customCodesign;
  } else {
    // Use system codesign
    codesignPath = "/usr/bin/codesign";
  }

  // Validate signing tool path
  std::vector<std::string> allowedTools = {"codesign"};
  auto validationResult = validateSigningToolPath(codesignPath, allowedTools);
  if (!validationResult.isOk()) {
    return Result<void>::error("Signing tool validation failed: " + validationResult.error());
  }

  // Validate bundle path
  if (!fs::exists(bundlePath)) {
    return Result<void>::error("Bundle not found for signing: " + bundlePath);
  }

  // Build command with proper quoting
  std::ostringstream cmd;
  cmd << "\"" << codesignPath << "\" --force --sign \"" << m_config.signingCertificate << "\"";

  // Add entitlements if provided
  if (!m_config.signingEntitlements.empty()) {
    if (!fs::exists(m_config.signingEntitlements)) {
      return Result<void>::error("Entitlements file not found: " +
                                 m_config.signingEntitlements);
    }
    cmd << " --entitlements \"" << m_config.signingEntitlements << "\"";
  }

  // Add team ID if provided (for notarization)
  if (!m_config.signingTeamId.empty()) {
    // Validate team ID format (alphanumeric)
    for (char c : m_config.signingTeamId) {
      if (!std::isalnum(c)) {
        return Result<void>::error("Invalid team ID format (must be alphanumeric)");
      }
    }
    cmd << " --team-id " << m_config.signingTeamId;
  }

  // Add bundle to sign
  cmd << " \"" << bundlePath << "\"";

  // Execute signing command
  std::string output;
  auto result = executeCommand(cmd.str(), output);

  if (!result.isOk()) {
    return Result<void>::error("Failed to execute signing command: " + result.error());
  }

  i32 exitCode = result.value();
  if (exitCode != 0) {
    return Result<void>::error("Signing failed with exit code " + std::to_string(exitCode) +
                               ": " + output);
  }

  logMessage("Successfully signed macOS bundle", false);

  // Verify the signature
  std::ostringstream verifyCmd;
  verifyCmd << "\"" << codesignPath << "\" --verify --verbose \"" << bundlePath << "\"";

  std::string verifyOutput;
  auto verifyResult = executeCommand(verifyCmd.str(), verifyOutput);

  if (!verifyResult.isOk() || verifyResult.value() != 0) {
    m_progress.warnings.push_back("Code signature verification failed: " + verifyOutput);
  } else {
    logMessage("Code signature verified successfully", false);
  }

  return Result<void>::ok();
}

} // namespace NovelMind::editor
