#pragma once
#include <string>

namespace lumina::jobs {

class Worker {
public:
    static void start(int intervalSeconds = 60);
    static void stop();
    static void runOnce();
};

} // namespace lumina::jobs
