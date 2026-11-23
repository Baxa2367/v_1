-- 0003_gamification: xp journal, achievements, seasons, leaderboard snapshots
CREATE TABLE IF NOT EXISTS xp_events (
    id            bigserial PRIMARY KEY,
    student_id    uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    amount        int NOT NULL,
    reason        text NOT NULL,
    source_type   text NOT NULL DEFAULT 'task',
    source_id     text NOT NULL DEFAULT '',
    balance_after bigint NOT NULL,
    season_id     uuid,
    course_id     uuid,
    created_at    timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS xp_events_student_idx ON xp_events (student_id, created_at DESC);
CREATE INDEX IF NOT EXISTS xp_events_created_idx ON xp_events (created_at DESC);
CREATE INDEX IF NOT EXISTS xp_events_course_idx ON xp_events (course_id, created_at DESC);
CREATE INDEX IF NOT EXISTS xp_events_season_idx ON xp_events (season_id, student_id);

CREATE TABLE IF NOT EXISTS achievements (
    id           uuid PRIMARY KEY,
    code         text NOT NULL UNIQUE,
    category     text NOT NULL,
    rarity       text NOT NULL DEFAULT 'common'
                 CHECK (rarity IN ('common', 'rare', 'epic', 'legendary')),
    rule         jsonb NOT NULL,
    title        jsonb NOT NULL,
    description  jsonb NOT NULL,
    icon         text NOT NULL DEFAULT 'emoji_events',
    points       int NOT NULL DEFAULT 10,
    reward_coins int NOT NULL DEFAULT 0,
    is_secret    boolean NOT NULL DEFAULT false,
    sort_order   int NOT NULL DEFAULT 0,
    created_at   timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS achievements_category_idx ON achievements (category, sort_order);

CREATE TABLE IF NOT EXISTS student_achievements (
    student_id     uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    achievement_id uuid NOT NULL REFERENCES achievements(id) ON DELETE CASCADE,
    awarded_at     timestamptz NOT NULL DEFAULT now(),
    progress       numeric(10, 2) NOT NULL DEFAULT 0,
    PRIMARY KEY (student_id, achievement_id)
);
CREATE INDEX IF NOT EXISTS student_achievements_student_idx
    ON student_achievements (student_id, awarded_at DESC);

CREATE TABLE IF NOT EXISTS seasons (
    id          uuid PRIMARY KEY,
    code        text NOT NULL UNIQUE,
    title       jsonb NOT NULL,
    theme       text NOT NULL DEFAULT '',
    description jsonb NOT NULL DEFAULT '{}'::jsonb,
    starts_at   timestamptz NOT NULL,
    ends_at     timestamptz NOT NULL,
    status      text NOT NULL DEFAULT 'upcoming'
                CHECK (status IN ('upcoming', 'active', 'closed')),
    rewards     jsonb NOT NULL DEFAULT '[]'::jsonb,
    created_at  timestamptz NOT NULL DEFAULT now(),
    CHECK (ends_at > starts_at)
);
CREATE INDEX IF NOT EXISTS seasons_status_idx ON seasons (status, starts_at);

CREATE TABLE IF NOT EXISTS season_rewards (
    id           uuid PRIMARY KEY,
    season_id    uuid NOT NULL REFERENCES seasons(id) ON DELETE CASCADE,
    tier         int NOT NULL CHECK (tier >= 1),
    title        jsonb NOT NULL,
    description  jsonb NOT NULL DEFAULT '{}'::jsonb,
    required_xp  int NOT NULL DEFAULT 0,
    payload      jsonb NOT NULL DEFAULT '{}'::jsonb,
    UNIQUE (season_id, tier)
);

CREATE TABLE IF NOT EXISTS season_progress (
    season_id  uuid NOT NULL REFERENCES seasons(id) ON DELETE CASCADE,
    student_id uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    season_xp  bigint NOT NULL DEFAULT 0,
    tier       int NOT NULL DEFAULT 0,
    claimed_tiers jsonb NOT NULL DEFAULT '[]'::jsonb,
    updated_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (season_id, student_id)
);
CREATE INDEX IF NOT EXISTS season_progress_xp_idx ON season_progress (season_id, season_xp DESC);

CREATE TABLE IF NOT EXISTS leaderboards (
    id           bigserial PRIMARY KEY,
    scope        text NOT NULL,
    period       text NOT NULL DEFAULT 'all',
    season_id    uuid,
    course_id    uuid,
    entity_type  text NOT NULL DEFAULT 'student'
                 CHECK (entity_type IN ('student', 'team')),
    entity_id    uuid NOT NULL,
    score        numeric(14, 2) NOT NULL DEFAULT 0,
    rank         int NOT NULL DEFAULT 0,
    snapshot_at  timestamptz NOT NULL DEFAULT now(),
    meta         jsonb NOT NULL DEFAULT '{}'::jsonb
);
CREATE INDEX IF NOT EXISTS leaderboards_scope_idx
    ON leaderboards (scope, period, snapshot_at DESC);
CREATE INDEX IF NOT EXISTS leaderboards_entity_idx ON leaderboards (entity_id);
