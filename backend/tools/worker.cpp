#include "core/config.h"
#include "core/crypto.h"
#include "core/log.h"
#include "db/db.h"
#include "jobs/worker.h"
#include <chrono>
#include <thread>

// Standalone background worker: seasons, snapshots, mastery decay, streaks.
int main() {
    using namespace lumina;
    Config::instance().load();
    Log::init(Config::instance().get("LOG_LEVEL", "info"), "logs/worker.log");
    Crypto::init();
    Db::instance().init(Config::instance().databaseUrl(), 4);
    Log::get()->info("lumina_worker started");
    while (true) {
        jobs::Worker::runOnce();
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
    return 0;
}
