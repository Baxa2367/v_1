# Architecture decisions

Each decision records context, choice and consequences. All deviations from the
original specification are explicit.

## D1. Drogon on MSYS2 (with direct library linkage)

**Context.** The backend must be C++20 and the spec prefers Drogon. The build
host is Windows 11 without vcpkg/Visual Studio; only Node.js and Docker CLI were
present initially.
**Decision.** Install an MSYS2 UCRT64 toolchain (GCC 16, CMake, Ninja) and use
the packaged `mingw-w64-ucrt-x86_64-drogon` 1.9.14. Drogon's exported CMake
package unconditionally calls `find_dependency(MySQL)`, which fails on a host
without MySQL even though we only use PostgreSQL. `backend/CMakeLists.txt`
therefore links `drogon`, `trantor`, `jsoncpp`, `uuid`, `brotli*` and `cares`
directly and exposes an imported `Drogon::Drogon` target.
**Consequences.** Reproducible on MSYS2 and in Debian (`libdrogon-dev`), no
vcpkg bootstrap; the CMake file must list Drogon's transitive libraries
explicitly.

## D2. PostgreSQL 16+ via libpqxx instead of Drogon ORM

**Context.** Drogon's ORM adds a second schema layer and hides SQL; the project
has many reporting queries, window functions and JSONB fields.
**Decision.** Use **libpqxx** with a small connection pool (`db/db.{h,cpp}`),
parameterised `exec_params`, explicit transactions with `FOR UPDATE` row locks,
and hand-written SQL migrations. Compose runs PostgreSQL 16; the development
host currently runs PostgreSQL 18.6 (compatible superset).
**Consequences.** Full SQL control, explicit locking, easy JSONB; no ORM
migrations. All queries are parameterised (no string interpolation of user
input).

## D3. Redis is optional, SQL is the fallback

**Context.** The spec asks for Redis sorted sets with a SQL fallback, but no
Redis server is available on the development host (MSYS2 ships only clients).
**Decision.** Leaderboards are computed from PostgreSQL with window functions
and materialised into the `leaderboards` snapshot table by the worker. The
architecture keeps a Redis adapter point (`LeaderboardService`), and
`docker-compose.yml` includes Redis for environments that want it. The system
is fully functional without Redis.
**Consequences.** Slightly heavier leaderboard queries at very large scale;
correct results and ranking tie-breaks (`score DESC, user_id`) everywhere.

## D4. JWT and Argon2id implemented with OpenSSL + libsodium

**Context.** `jwt-cpp` is not packaged for MSYS2; libsodium is.
**Decision.** HS256 JWTs are signed/verified in `core/crypto.cpp` with OpenSSL
HMAC (constant-time comparison via libsodium). Passwords use libsodium
`crypto_pwhash_str` (Argon2id, interactive presets). Refresh tokens are random
256-bit values stored as SHA-256 hashes and rotated on every refresh.
**Consequences.** No extra dependency; the JWT implementation is small and
unit-testable. Tokens carry `sub`, `role`, `locale`, `name`, `typ`, `exp`, `jti`.

## D5. Server-authoritative game logic

**Decision.** The client never receives `answer_key` before an attempt; grading,
XP, achievements, streaks, quests and ranks are computed in PostgreSQL
transactions. `attempt_token` is unique and the full graded response is stored
in `attempts.response`, so network retries are idempotent.
**Consequences.** Replays return the exact original response; the anti-cheat
layer can flag suspicious attempts without losing progress.

## D6. XP formula and level curve

```
XP = base_xp × (0.8 + 0.12·difficulty) × (0.4 + 0.6·score) × speed
     × (1 + 0.05·min(streak,10)) × (1 − 0.15·hints) × repeat_decay
speed ∈ [0.85, 1.15] (fast answers get a bonus)
xp_to_next(level) = round(100 × level^1.5)
```

Bonuses: first daily-goal completion (+25 XP), level completion (level reward,
×1.5 for bosses, ÷3 for replays). Coins = floor(XP/10). All XP mutations are
append-only rows in `xp_events` with `balance_after`.

## D7. Declarative achievements

**Decision.** Achievement rules are JSON: `{event, metric, op, value}`. The
engine builds a metrics snapshot (one set of aggregate queries) and evaluates
all rules against it; awarding is protected by a unique key, so it is
idempotent. 45 achievements across 10 categories (start, progress, accuracy,
speed, streaks, social, seasonal, secret, mastery).
**Consequences.** New achievements are seed data, not code; progress bars reuse
the same metrics.

## D8. Adaptive engine (`libadaptive`)

**Decision.** Per (student, topic) we store a BKT state (mastery, confidence,
streaks, counters). Updates use a Bayesian posterior with slip/guess and a
learning rate, plus an exponential forgetting curve (half-life 21 days).
The next-task policy mixes 70 % ZPD (expected success 60–80 %), 20 % weak-topic
review and 10 % exploration; ≥3 errors or ≥60 % errors in 10 attempts triggers
support mode (difficulty −1), ≥3 correct answers triggers challenge mode.
Every decision is stored in `ai_decisions` with features and a tri-lingual
explanation.
**Consequences.** Explainability is a first-class API (`/ai/explain/{id}`), and
the policy is unit-tested without a database.

## D9. Optional LLM integration

**Decision.** `ai::LlmClient` calls the Anthropic Messages API over libcurl only
when `ANTHROPIC_API_KEY` is set. Otherwise the task bank, parameterised content
and 60 seeded tip templates are used. Generated tasks are returned as drafts
(`requires_moderation: true`) and never inserted automatically.
**Consequences.** The product works fully offline; enabling the LLM adds
personalised tips and draft generation without changing the API.

## D10. Code sandbox

**Decision.** C++ code tasks compile with `g++ -std=c++20 -O2` in a temporary
directory and run with stdin/stdout redirection, a per-test timeout and a 256 MB
memory cap (Windows Job Objects; POSIX `setrlimit`). Compilation and execution
happen outside the database transaction.
**Limitations.** The Windows runner cannot provide a filesystem/network
namespace. Production deployments must run the API inside a container with
`nsjail`/`isolate` (documented in `docs/SECURITY.md`); the interface
(`sandbox/sandbox.h`) is ready for such an adapter.

## D11. Time and locale

**Decision.** All timestamps are `timestamptz` in UTC; "today", weekly resets,
streaks and season boundaries use `Asia/Qyzylorda` (`APP_TIMEZONE`). Localised
JSON fields are `{kk, ru, en}` objects resolved server-side by
`Accept-Language` / `X-Locale`; tasks and explanations are guaranteed in kk+ru.

## D12. Static frontend served by the C++ server

**Decision.** Vite builds `frontend/dist`; Drogon serves it from
`PUBLIC_DIR`, with a custom 404 page that returns `index.html` for SPA routes.
`/api/v1` is the only data surface, so the same binary works behind nginx or
standalone.

## D13. Testing strategy

**Decision.** GoogleTest unit tests for the XP engine, grading of all task
types, BKT model and adaptive policy. Integration checks are performed by the
demo script (`scripts/demo.mjs`) against a live server and PostgreSQL. k6
script for leaderboard load is provided but not run in CI.
**Consequences.** The pure logic is covered without a database; end-to-end
behaviour is verified by the scripted defence scenario.

## D14. Seed content

**Decision.** Four courses × 8 levels × 10 tasks = **320 tasks**, all with kk+ru
prompts, explanations and 2–3 hints. The C++ seeder loads JSON content,
creates 40 demo students with realistic histories (four behavioural profiles),
6 teams, 3 seasons and an active tournament, then evaluates achievements so the
demo is alive on first launch.
**Consequences.** `lumina_seeder --reset` rebuilds a deterministic demo dataset.
