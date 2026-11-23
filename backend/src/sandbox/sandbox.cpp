#include "sandbox/sandbox.h"
#include "core/log.h"
#include "core/util.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <processthreadsapi.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace lumina {
namespace fs = std::filesystem;

std::string Sandbox::compiler_ = "g++";

void Sandbox::setCompiler(const std::string& path) { compiler_ = path; }

namespace {

std::string readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void writeFile(const fs::path& p, const std::string& data) {
    std::ofstream out(p, std::ios::binary);
    out << data;
}

std::string trimOutput(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    while (!s.empty() && (s.front() == '\n' || s.front() == '\r')) s.erase(s.begin());
    return s;
}

#ifdef _WIN32
std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

struct ProcessResult {
    bool started = false;
    bool timedOut = false;
    unsigned long exitCode = 0;
};

ProcessResult runProcess(const std::string& command, const fs::path& stdinPath,
                         const fs::path& stdoutPath, const fs::path& stderrPath,
                         int timeoutMs, int memoryLimitMb) {
    ProcessResult result;
    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};

    HANDLE hIn = CreateFileW(widen(stdinPath.string()).c_str(), GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    HANDLE hOut = CreateFileW(widen(stdoutPath.string()).c_str(), GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, 0, nullptr);
    HANDLE hErr = CreateFileW(widen(stderrPath.string()).c_str(), GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, 0, nullptr);
    if (hIn == INVALID_HANDLE_VALUE || hOut == INVALID_HANDLE_VALUE || hErr == INVALID_HANDLE_VALUE) {
        if (hIn != INVALID_HANDLE_VALUE) CloseHandle(hIn);
        if (hOut != INVALID_HANDLE_VALUE) CloseHandle(hOut);
        if (hErr != INVALID_HANDLE_VALUE) CloseHandle(hErr);
        return result;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hIn;
    si.hStdOutput = hOut;
    si.hStdError = hErr;
    PROCESS_INFORMATION pi{};

    std::wstring cmd = widen(command);
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    const BOOL created = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr,
                                        &si, &pi);
    CloseHandle(hIn);
    CloseHandle(hOut);
    CloseHandle(hErr);
    if (!created) {
        return result;
    }
    result.started = true;

    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (memoryLimitMb > 0) {
            info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
            info.ProcessMemoryLimit = (SIZE_T)memoryLimitMb * 1024ull * 1024ull;
        }
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info));
        AssignProcessToJobObject(job, pi.hProcess);
    }
    ResumeThread(pi.hThread);

    const DWORD wait = WaitForSingleObject(pi.hProcess, (DWORD)timeoutMs);
    if (wait == WAIT_TIMEOUT) {
        result.timedOut = true;
        if (job) TerminateJobObject(job, 124);
        TerminateProcess(pi.hProcess, 124);
    }
    WaitForSingleObject(pi.hProcess, 2000);
    GetExitCodeProcess(pi.hProcess, &result.exitCode);

    if (job) CloseHandle(job);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return result;
}
#else
struct ProcessResult {
    bool started = false;
    bool timedOut = false;
    int exitCode = 0;
};

ProcessResult runProcess(const std::string& command, const fs::path& stdinPath,
                         const fs::path& stdoutPath, const fs::path& stderrPath,
                         int timeoutMs, int memoryLimitMb) {
    ProcessResult result;
    pid_t pid = fork();
    if (pid < 0) return result;
    if (pid == 0) {
        int in = open(stdinPath.c_str(), O_RDONLY);
        int out = open(stdoutPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        int err = open(stderrPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (in >= 0) dup2(in, STDIN_FILENO);
        if (out >= 0) dup2(out, STDOUT_FILENO);
        if (err >= 0) dup2(err, STDERR_FILENO);
        if (memoryLimitMb > 0) {
            struct rlimit mem;
            mem.rlim_cur = mem.rlim_max = (rlim_t)memoryLimitMb * 1024 * 1024;
            setrlimit(RLIMIT_AS, &mem);
        }
        struct rlimit cpu;
        cpu.rlim_cur = cpu.rlim_max = (rlim_t)std::max(1, timeoutMs / 1000 + 1);
        setrlimit(RLIMIT_CPU, &cpu);
        setpgid(0, 0);
        execl("/bin/sh", "sh", "-c", command.c_str(), (char*)nullptr);
        _exit(127);
    }
    result.started = true;
    const int deadlineMs = timeoutMs;
    int waited = 0;
    int status = 0;
    while (waited < deadlineMs) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
            return result;
        }
        usleep(20000);
        waited += 20;
    }
    result.timedOut = true;
    kill(-pid, SIGKILL);
    waitpid(pid, &status, 0);
    result.exitCode = 124;
    return result;
}
#endif

std::string shellQuote(const std::string& s) {
#ifdef _WIN32
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else out += c;
    }
    out += "\"";
    return out;
#else
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
#endif
}

} // namespace

SandboxResult Sandbox::runCpp(const std::string& source, const std::vector<SandboxTest>& tests,
                              int perTestTimeoutMs, int memoryLimitMb) {
    SandboxResult result;
    result.total = static_cast<int>(tests.size());

    const fs::path dir = fs::temp_directory_path() / ("lumina_sbx_" + randomHex(6));
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        result.sandboxUnavailable = true;
        result.compileError = "cannot create sandbox directory";
        return result;
    }

    struct Cleanup {
        fs::path p;
        ~Cleanup() {
            std::error_code e;
            fs::remove_all(p, e);
        }
    } cleanup{dir};

    writeFile(dir / "main.cpp", source);
    writeFile(dir / "stdin.txt", "");

#ifdef _WIN32
    const fs::path exe = dir / "main.exe";
    const std::string compileCmd = shellQuote(compiler_) +
                                   " -std=c++20 -O2 -pipe -o " + shellQuote(exe.string()) +
                                   " " + shellQuote((dir / "main.cpp").string());
#else
    const fs::path exe = dir / "main";
    const std::string compileCmd = shellQuote(compiler_) +
                                   " -std=c++20 -O2 -pipe -o " + shellQuote(exe.string()) +
                                   " " + shellQuote((dir / "main.cpp").string());
#endif

    auto compile = runProcess(compileCmd, dir / "stdin.txt", dir / "compile.out", dir / "compile.err",
                              30000, 1024);
    const std::string compileOut = readFile(dir / "compile.out") + readFile(dir / "compile.err");
    if (!compile.started) {
        result.sandboxUnavailable = true;
        result.compileError = "compiler is not available (" + compiler_ + ")";
        return result;
    }
    if (compile.timedOut || compile.exitCode != 0 || !fs::exists(exe)) {
        result.compileError = trimOutput(compileOut);
        if (compile.timedOut) result.compileError = "compilation timed out";
        return result;
    }
    result.compiled = true;

    for (size_t i = 0; i < tests.size(); ++i) {
        const fs::path inPath = dir / ("in_" + std::to_string(i) + ".txt");
        writeFile(inPath, tests[i].input);
        const fs::path outPath = dir / ("out_" + std::to_string(i) + ".txt");
        const fs::path errPath = dir / ("err_" + std::to_string(i) + ".txt");
        auto run = runProcess(shellQuote(exe.string()), inPath, outPath, errPath,
                              perTestTimeoutMs, memoryLimitMb);
        SandboxCase c;
        c.index = static_cast<int>(i);
        c.timedOut = run.timedOut;
        c.output = trimOutput(readFile(outPath));
        c.error = trimOutput(readFile(errPath));
        c.passed = run.started && !run.timedOut && run.exitCode == 0 &&
                   c.output == trimOutput(tests[i].expectedOutput);
        if (c.passed) ++result.passed;
        result.cases.push_back(std::move(c));
    }
    return result;
}

} // namespace lumina


