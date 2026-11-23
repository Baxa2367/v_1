-- 0001_core: users, refresh tokens, student profiles, audit log
CREATE TABLE IF NOT EXISTS users (
    id              uuid PRIMARY KEY,
    email           text NOT NULL,
    password_hash   text NOT NULL,
    role            text NOT NULL DEFAULT 'student'
                    CHECK (role IN ('student', 'teacher', 'admin')),
    locale          text NOT NULL DEFAULT 'kk'
                    CHECK (locale IN ('kk', 'ru', 'en')),
    status          text NOT NULL DEFAULT 'active'
                    CHECK (status IN ('active', 'blocked', 'deleted')),
    display_name    text NOT NULL DEFAULT '',
    created_at      timestamptz NOT NULL DEFAULT now(),
    updated_at      timestamptz NOT NULL DEFAULT now(),
    deleted_at      timestamptz
);
CREATE UNIQUE INDEX IF NOT EXISTS users_email_unique
    ON users (lower(email)) WHERE deleted_at IS NULL;
CREATE INDEX IF NOT EXISTS users_role_idx ON users (role) WHERE deleted_at IS NULL;

CREATE TABLE IF NOT EXISTS refresh_tokens (
    id              uuid PRIMARY KEY,
    user_id         uuid NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    token_hash      text NOT NULL UNIQUE,
    expires_at      timestamptz NOT NULL,
    revoked_at      timestamptz,
    user_agent      text NOT NULL DEFAULT '',
    ip              text NOT NULL DEFAULT '',
    created_at      timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS refresh_tokens_user_idx ON refresh_tokens (user_id);

CREATE TABLE IF NOT EXISTS students (
    user_id          uuid PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    nickname         text NOT NULL,
    avatar           text NOT NULL DEFAULT 'face',
    bio              text NOT NULL DEFAULT '',
    total_xp         bigint NOT NULL DEFAULT 0 CHECK (total_xp >= 0),
    level            int NOT NULL DEFAULT 1 CHECK (level >= 1),
    coins            int NOT NULL DEFAULT 0 CHECK (coins >= 0),
    gems             int NOT NULL DEFAULT 0 CHECK (gems >= 0),
    current_streak   int NOT NULL DEFAULT 0 CHECK (current_streak >= 0),
    longest_streak   int NOT NULL DEFAULT 0 CHECK (longest_streak >= 0),
    last_activity_date date,
    daily_goal_xp    int NOT NULL DEFAULT 50 CHECK (daily_goal_xp > 0),
    league           text NOT NULL DEFAULT 'bronze'
                     CHECK (league IN ('bronze', 'silver', 'gold', 'platinum', 'diamond')),
    privacy_flags    jsonb NOT NULL DEFAULT '{"public_leaderboard": true}'::jsonb,
    showcase         jsonb NOT NULL DEFAULT '[]'::jsonb,
    last_active_at   timestamptz,
    created_at       timestamptz NOT NULL DEFAULT now(),
    updated_at       timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS students_total_xp_idx ON students (total_xp DESC);
CREATE INDEX IF NOT EXISTS students_league_idx ON students (league, total_xp DESC);
CREATE INDEX IF NOT EXISTS students_streak_idx ON students (current_streak DESC);

CREATE TABLE IF NOT EXISTS audit_log (
    id           bigserial PRIMARY KEY,
    actor_id     uuid REFERENCES users(id) ON DELETE SET NULL,
    action       text NOT NULL,
    entity_type  text NOT NULL DEFAULT '',
    entity_id    text NOT NULL DEFAULT '',
    payload      jsonb NOT NULL DEFAULT '{}'::jsonb,
    ip           text NOT NULL DEFAULT '',
    created_at   timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS audit_log_created_idx ON audit_log (created_at DESC);
CREATE INDEX IF NOT EXISTS audit_log_actor_idx ON audit_log (actor_id, created_at DESC);
