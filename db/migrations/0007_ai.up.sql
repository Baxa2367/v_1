-- 0007_ai: advice templates used by the adaptive engine (60+ seeded rows)
CREATE TABLE IF NOT EXISTS ai_tip_templates (
    id         uuid PRIMARY KEY,
    code       text NOT NULL UNIQUE,
    kind       text NOT NULL
               CHECK (kind IN ('motivational', 'methodical', 'warning', 'congratulation')),
    text       jsonb NOT NULL,
    tags       jsonb NOT NULL DEFAULT '[]'::jsonb,
    priority   int NOT NULL DEFAULT 0,
    active     boolean NOT NULL DEFAULT true
);
CREATE INDEX IF NOT EXISTS ai_tip_templates_kind_idx ON ai_tip_templates (kind, priority DESC);
