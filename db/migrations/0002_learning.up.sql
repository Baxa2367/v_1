-- 0002_learning: courses, topics, levels, tasks, attempts, level progress
CREATE TABLE IF NOT EXISTS courses (
    id           uuid PRIMARY KEY,
    slug         text NOT NULL UNIQUE,
    title        jsonb NOT NULL,
    description  jsonb NOT NULL,
    category     text NOT NULL DEFAULT 'general',
    tags         jsonb NOT NULL DEFAULT '[]'::jsonb,
    cover        text NOT NULL DEFAULT 'school',
    difficulty   int NOT NULL DEFAULT 1 CHECK (difficulty BETWEEN 1 AND 10),
    status       text NOT NULL DEFAULT 'published'
                 CHECK (status IN ('draft', 'published', 'archived')),
    author_id    uuid REFERENCES users(id) ON DELETE SET NULL,
    prerequisite_course_id uuid REFERENCES courses(id) ON DELETE SET NULL,
    sort_order   int NOT NULL DEFAULT 0,
    created_at   timestamptz NOT NULL DEFAULT now(),
    updated_at   timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS courses_status_idx ON courses (status, sort_order);

CREATE TABLE IF NOT EXISTS course_enrollments (
    student_id    uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    course_id     uuid NOT NULL REFERENCES courses(id) ON DELETE CASCADE,
    status        text NOT NULL DEFAULT 'active'
                  CHECK (status IN ('active', 'completed', 'left')),
    progress_pct  numeric(5, 2) NOT NULL DEFAULT 0 CHECK (progress_pct BETWEEN 0 AND 100),
    last_level_id uuid,
    enrolled_at   timestamptz NOT NULL DEFAULT now(),
    updated_at    timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (student_id, course_id)
);

CREATE TABLE IF NOT EXISTS topics (
    id         uuid PRIMARY KEY,
    course_id  uuid NOT NULL REFERENCES courses(id) ON DELETE CASCADE,
    parent_id  uuid REFERENCES topics(id) ON DELETE SET NULL,
    code       text NOT NULL,
    name       jsonb NOT NULL,
    UNIQUE (course_id, code)
);
CREATE INDEX IF NOT EXISTS topics_course_idx ON topics (course_id);

CREATE TABLE IF NOT EXISTS levels (
    id            uuid PRIMARY KEY,
    course_id     uuid NOT NULL REFERENCES courses(id) ON DELETE CASCADE,
    order_no      int NOT NULL CHECK (order_no >= 1),
    title         jsonb NOT NULL,
    description   jsonb NOT NULL,
    icon          text NOT NULL DEFAULT 'flag',
    unlock_rules  jsonb NOT NULL DEFAULT '{}'::jsonb,
    is_boss       boolean NOT NULL DEFAULT false,
    xp_reward     int NOT NULL DEFAULT 100 CHECK (xp_reward >= 0),
    created_at    timestamptz NOT NULL DEFAULT now(),
    UNIQUE (course_id, order_no)
);

CREATE TABLE IF NOT EXISTS tasks (
    id             uuid PRIMARY KEY,
    level_id       uuid NOT NULL REFERENCES levels(id) ON DELETE CASCADE,
    topic_id       uuid NOT NULL REFERENCES topics(id) ON DELETE RESTRICT,
    type           text NOT NULL CHECK (type IN
                   ('single_choice', 'multiple_choice', 'true_false', 'short_answer',
                    'matching', 'ordering', 'cloze', 'numeric', 'timed_quiz', 'code')),
    prompt         jsonb NOT NULL,
    payload        jsonb NOT NULL DEFAULT '{}'::jsonb,
    answer_key     jsonb NOT NULL,
    explanation    jsonb NOT NULL DEFAULT '{}'::jsonb,
    hints          jsonb NOT NULL DEFAULT '[]'::jsonb,
    difficulty     int NOT NULL DEFAULT 1 CHECK (difficulty BETWEEN 1 AND 10),
    base_xp        int NOT NULL DEFAULT 30 CHECK (base_xp BETWEEN 5 AND 500),
    time_limit_sec int NOT NULL DEFAULT 120 CHECK (time_limit_sec BETWEEN 10 AND 3600),
    version        int NOT NULL DEFAULT 1,
    generated      boolean NOT NULL DEFAULT false,
    status         text NOT NULL DEFAULT 'published'
                   CHECK (status IN ('draft', 'published', 'archived', 'moderation')),
    sort_order     int NOT NULL DEFAULT 0,
    created_at     timestamptz NOT NULL DEFAULT now(),
    updated_at     timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS tasks_level_idx ON tasks (level_id, sort_order);
CREATE INDEX IF NOT EXISTS tasks_topic_idx ON tasks (topic_id, difficulty);

CREATE TABLE IF NOT EXISTS attempts (
    id             uuid PRIMARY KEY,
    student_id     uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    task_id        uuid NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,
    attempt_token  text NOT NULL UNIQUE,
    answer         jsonb NOT NULL DEFAULT '{}'::jsonb,
    result         text NOT NULL CHECK (result IN ('correct', 'partial', 'wrong')),
    score          numeric(4, 3) NOT NULL DEFAULT 0 CHECK (score BETWEEN 0 AND 1),
    time_spent_ms  int NOT NULL DEFAULT 0 CHECK (time_spent_ms >= 0),
    hints_used     int NOT NULL DEFAULT 0 CHECK (hints_used >= 0),
    context        text NOT NULL DEFAULT 'normal'
                   CHECK (context IN ('normal', 'duel', 'tournament', 'season', 'daily')),
    suspicious     boolean NOT NULL DEFAULT false,
    ip             text NOT NULL DEFAULT '',
    response       jsonb NOT NULL DEFAULT '{}'::jsonb,
    created_at     timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS attempts_student_idx ON attempts (student_id, created_at DESC);
CREATE INDEX IF NOT EXISTS attempts_task_idx ON attempts (task_id, student_id);
CREATE INDEX IF NOT EXISTS attempts_created_idx ON attempts (created_at DESC);

CREATE TABLE IF NOT EXISTS level_progress (
    student_id    uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    level_id      uuid NOT NULL REFERENCES levels(id) ON DELETE CASCADE,
    status        text NOT NULL DEFAULT 'in_progress'
                  CHECK (status IN ('locked', 'available', 'in_progress', 'completed')),
    stars         int NOT NULL DEFAULT 0 CHECK (stars BETWEEN 0 AND 3),
    best_accuracy numeric(4, 3) NOT NULL DEFAULT 0,
    xp_earned     int NOT NULL DEFAULT 0,
    attempts_count int NOT NULL DEFAULT 0,
    completed_at  timestamptz,
    updated_at    timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (student_id, level_id)
);
CREATE INDEX IF NOT EXISTS level_progress_level_idx ON level_progress (level_id, status);
