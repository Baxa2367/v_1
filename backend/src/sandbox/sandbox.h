#pragma once
#include <string>
#include <vector>

namespace lumina {

struct SandboxTest {
    std::string input;
    std::string expectedOutput;
};

struct SandboxCase {
    int index = 0;
    bool passed = false;
    std::string output;
    std::string error;
    bool timedOut = false;
};

struct SandboxResult {
    bool compiled = false;
    std::string compileError;
    std::vector<SandboxCase> cases;
    int passed = 0;
    int total = 0;
    bool sandboxUnavailable = false;
};

// Compiles and runs a C++20 program in an isolated child process with
// time and memory limits. No network access is granted by the runner itself;
// production deployments should additionally use a network namespace / nsjail.
class Sandbox {
public:
    static void setCompiler(const std::string& path);
    static SandboxResult runCpp(const std::string& source,
                                const std::vector<SandboxTest>& tests,
                                int perTestTimeoutMs = 3000,
                                int memoryLimitMb = 256);

private:
    static std::string compiler_;
};

} // namespace lumina
