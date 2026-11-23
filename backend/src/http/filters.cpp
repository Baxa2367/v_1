#include "http/filters.h"
#include <chrono>

namespace lumina {

RateLimiter& RateLimiter::instance() {
    static RateLimiter limiter;
    return limiter;
}

bool RateLimiter::allow(const std::string& key, int perMinute) {
    if (perMinute <= 0) return true;
    std::lock_guard lock(mutex_);
    const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
    auto& b = buckets_[key];
    if (b.updatedMs == 0) {
        b.tokens = perMinute;
        b.updatedMs = now;
    }
    const double refill = (now - b.updatedMs) * perMinute / 60000.0;
    b.tokens = std::min<double>(perMinute, b.tokens + refill);
    b.updatedMs = now;
    if (b.tokens < 1.0) return false;
    b.tokens -= 1.0;
    if (buckets_.size() > 20000) buckets_.clear();
    return true;
}

} // namespace lumina

// Drogon registers filters through DrObject<T>'s static allocator. Header-only
// filters are otherwise not odr-used, so force the template instantiation.
template class drogon::HttpFilter<lumina::AuthFilter>;
template class drogon::HttpFilter<lumina::TeacherFilter>;
template class drogon::HttpFilter<lumina::AdminFilter>;
