-- 0004_social: teams, tournaments, shop, notifications
CREATE TABLE IF NOT EXISTS teams (
    id          uuid PRIMARY KEY,
    name        text NOT NULL,
    emblem      text NOT NULL DEFAULT 'groups',
    description text NOT NULL DEFAULT '',
    captain_id  uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    invite_code text NOT NULL UNIQUE,
    created_at  timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS team_members (
    team_id    uuid NOT NULL REFERENCES teams(id) ON DELETE CASCADE,
    student_id uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    role       text NOT NULL DEFAULT 'member' CHECK (role IN ('captain', 'member')),
    joined_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (team_id, student_id),
    UNIQUE (student_id)
);

CREATE TABLE IF NOT EXISTS tournaments (
    id          uuid PRIMARY KEY,
    title       jsonb NOT NULL,
    description jsonb NOT NULL DEFAULT '{}'::jsonb,
    metric      text NOT NULL DEFAULT 'xp'
                CHECK (metric IN ('xp', 'accuracy', 'tasks')),
    starts_at   timestamptz NOT NULL,
    ends_at     timestamptz NOT NULL,
    status      text NOT NULL DEFAULT 'upcoming'
                CHECK (status IN ('upcoming', 'active', 'closed')),
    rules       jsonb NOT NULL DEFAULT '{}'::jsonb,
    created_at  timestamptz NOT NULL DEFAULT now(),
    CHECK (ends_at > starts_at)
);

CREATE TABLE IF NOT EXISTS tournament_teams (
    tournament_id uuid NOT NULL REFERENCES tournaments(id) ON DELETE CASCADE,
    team_id       uuid NOT NULL REFERENCES teams(id) ON DELETE CASCADE,
    score         numeric(14, 2) NOT NULL DEFAULT 0,
    rank          int NOT NULL DEFAULT 0,
    updated_at    timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (tournament_id, team_id)
);

CREATE TABLE IF NOT EXISTS shop_items (
    id           uuid PRIMARY KEY,
    code         text NOT NULL UNIQUE,
    kind         text NOT NULL
                 CHECK (kind IN ('streak_freeze', 'hint_pack', 'avatar', 'frame', 'theme', 'booster')),
    title        jsonb NOT NULL,
    description  jsonb NOT NULL DEFAULT '{}'::jsonb,
    price_coins  int NOT NULL DEFAULT 0 CHECK (price_coins >= 0),
    price_gems   int NOT NULL DEFAULT 0 CHECK (price_gems >= 0),
    icon         text NOT NULL DEFAULT 'shopping_bag',
    payload      jsonb NOT NULL DEFAULT '{}'::jsonb,
    active       boolean NOT NULL DEFAULT true,
    sort_order   int NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS purchases (
    id          uuid PRIMARY KEY,
    student_id  uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    item_id     uuid NOT NULL REFERENCES shop_items(id) ON DELETE RESTRICT,
    price_coins int NOT NULL DEFAULT 0,
    price_gems  int NOT NULL DEFAULT 0,
    created_at  timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS purchases_student_idx ON purchases (student_id, created_at DESC);

CREATE TABLE IF NOT EXISTS inventory (
    student_id  uuid NOT NULL REFERENCES students(user_id) ON DELETE CASCADE,
    item_id     uuid NOT NULL REFERENCES shop_items(id) ON DELETE CASCADE,
    qty         int NOT NULL DEFAULT 1 CHECK (qty >= 0),
    equipped    boolean NOT NULL DEFAULT false,
    acquired_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (student_id, item_id)
);

CREATE TABLE IF NOT EXISTS notifications (
    id         uuid PRIMARY KEY,
    user_id    uuid NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    kind       text NOT NULL DEFAULT 'info',
    title      jsonb NOT NULL DEFAULT '{}'::jsonb,
    body       jsonb NOT NULL DEFAULT '{}'::jsonb,
    payload    jsonb NOT NULL DEFAULT '{}'::jsonb,
    read_at    timestamptz,
    created_at timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS notifications_user_idx ON notifications (user_id, created_at DESC);
