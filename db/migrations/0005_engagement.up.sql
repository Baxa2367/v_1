-- 0005_engagement: daily goals, quests, streaks, mastery, AI, anticheat
CREATE TABLE IF NOT EXISTS daily_goals (
    student_id  uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    goal_date   date NOT NULL,
    goal_xp     int NOT NULL DEFAULT 50 CHECK (goal_xp > 0),
    xp_earned   int NOT NULL DEFAULT 0,
    completed_at timestamptz,
    PRIMARY KEY (student_id, goal_date)
);

CREATE TABLE IF NOT EXISTS daily_quests (
    id           uuid PRIMARY KEY,
    code         text NOT NULL UNIQUE,
    title        jsonb NOT NULL,
    description  jsonb NOT NULL DEFAULT '{}'::jsonb,
    kind         text NOT NULL DEFAULT 'daily' CHECK (kind IN ('daily', 'weekly')),
    rule         jsonb NOT NULL,
    target       int NOT NULL DEFAULT 1 CHECK (target > 0),
    reward_xp    int NOT NULL DEFAULT 20,
    reward_coins int NOT NULL DEFAULT 5,
    icon         text NOT NULL DEFAULT 'task_alt',
    active       boolean NOT NULL DEFAULT true
);
CREATE INDEX IF NOT EXISTS daily_quests_kind_idx ON daily_quests (kind, active);

CREATE TABLE IF NOT EXISTS student_quests (
    id           uuid PRIMARY KEY,
    student_id   uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    quest_id     uuid NOT NULL REFERENCES daily_quests(id) ON DELETE CASCADE,
    quest_date   date NOT NULL,
    progress     int NOT NULL DEFAULT 0,
    target       int NOT NULL DEFAULT 1,
    status       text NOT NULL DEFAULT 'active'
                 CHECK (status IN ('active', 'completed', 'claimed')),
    completed_at timestamptz,
    claimed_at   timestamptz,
    UNIQUE (student_id, quest_id, quest_date)
);
CREATE INDEX IF NOT EXISTS student_quests_lookup_idx
    ON student_quests (student_id, quest_date DESC);

CREATE TABLE IF NOT EXISTS streak_log (
    student_id uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    log_date   date NOT NULL,
    xp_earned  int NOT NULL DEFAULT 0,
    goal_met   boolean NOT NULL DEFAULT false,
    freeze_used boolean NOT NULL DEFAULT false,
    PRIMARY KEY (student_id, log_date)
);

CREATE TABLE IF NOT EXISTS topic_mastery (
    student_id   uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    topic_id     uuid NOT NULL REFERENCES topics(id) ON DELETE CASCADE,
    mastery      numeric(5, 4) NOT NULL DEFAULT 0.3 CHECK (mastery BETWEEN 0 AND 1),
    confidence   numeric(5, 4) NOT NULL DEFAULT 0.1 CHECK (confidence BETWEEN 0 AND 1),
    last_seen_at timestamptz,
    streak_correct int NOT NULL DEFAULT 0,
    streak_wrong   int NOT NULL DEFAULT 0,
    attempts       int NOT NULL DEFAULT 0,
    correct        int NOT NULL DEFAULT 0,
    updated_at   timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (student_id, topic_id)
);
CREATE INDEX IF NOT EXISTS topic_mastery_student_idx ON topic_mastery (student_id, mastery);

CREATE TABLE IF NOT EXISTS ai_decisions (
    id           uuid PRIMARY KEY,
    student_id   uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    course_id    uuid REFERENCES courses(id) ON DELETE CASCADE,
    task_id      uuid REFERENCES tasks(id) ON DELETE SET NULL,
    reason_code  text NOT NULL DEFAULT 'zpd',
    features     jsonb NOT NULL DEFAULT '{}'::jsonb,
    explanation  jsonb NOT NULL DEFAULT '{}'::jsonb,
    created_at   timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS ai_decisions_student_idx ON ai_decisions (student_id, created_at DESC);

CREATE TABLE IF NOT EXISTS ai_tips (
    id         uuid PRIMARY KEY,
    student_id uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    kind       text NOT NULL DEFAULT 'methodical'
               CHECK (kind IN ('motivational', 'methodical', 'warning', 'congratulation')),
    text       jsonb NOT NULL,
    meta       jsonb NOT NULL DEFAULT '{}'::jsonb,
    created_at timestamptz NOT NULL DEFAULT now(),
    read_at    timestamptz
);
CREATE INDEX IF NOT EXISTS ai_tips_student_idx ON ai_tips (student_id, created_at DESC);

CREATE TABLE IF NOT EXISTS anticheat_flags (
    id          uuid PRIMARY KEY,
    student_id  uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    attempt_id  uuid REFERENCES attempts(id) ON DELETE SET NULL,
    kind        text NOT NULL,
    severity    int NOT NULL DEFAULT 1 CHECK (severity BETWEEN 1 AND 5),
    details     jsonb NOT NULL DEFAULT '{}'::jsonb,
    status      text NOT NULL DEFAULT 'open' CHECK (status IN ('open', 'reviewed', 'dismissed')),
    created_at  timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS anticheat_flags_status_idx ON anticheat_flags (status, created_at DESC);
