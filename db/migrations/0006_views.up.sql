-- 0006_views: reporting views
CREATE OR REPLACE VIEW v_course_progress AS
SELECT e.student_id,
       e.course_id,
       count(l.id)                                       AS total_levels,
       count(lp.level_id) FILTER (WHERE lp.status = 'completed') AS completed_levels,
       coalesce(sum(lp.stars), 0)                        AS total_stars,
       coalesce(sum(lp.xp_earned), 0)                    AS level_xp,
       CASE WHEN count(l.id) = 0 THEN 0
            ELSE round(100.0 * count(lp.level_id) FILTER (WHERE lp.status = 'completed')
                       / count(l.id), 2) END             AS progress_pct
FROM course_enrollments e
JOIN levels l ON l.course_id = e.course_id
LEFT JOIN level_progress lp
       ON lp.level_id = l.id AND lp.student_id = e.student_id
GROUP BY e.student_id, e.course_id;

CREATE OR REPLACE VIEW v_topic_stats AS
SELECT a.student_id,
       t.topic_id,
       count(*)                                          AS attempts,
       count(*) FILTER (WHERE a.result = 'correct')      AS correct,
       round(avg(a.score), 4)                            AS avg_score,
       round(avg(a.time_spent_ms))                       AS avg_time_ms,
       max(a.created_at)                                 AS last_attempt_at
FROM attempts a
JOIN tasks t ON t.id = a.task_id
GROUP BY a.student_id, t.topic_id;

CREATE OR REPLACE VIEW v_daily_xp AS
SELECT student_id,
       (created_at AT TIME ZONE 'Asia/Qyzylorda')::date AS day,
       sum(amount)                                       AS xp,
       count(*)                                          AS events
FROM xp_events
GROUP BY student_id, day;

CREATE OR REPLACE VIEW v_weekly_leaderboard AS
SELECT student_id,
       date_trunc('week', created_at AT TIME ZONE 'Asia/Qyzylorda')::date AS week_start,
       sum(amount) AS xp
FROM xp_events
WHERE amount > 0
GROUP BY student_id, week_start;

CREATE OR REPLACE VIEW v_teacher_course AS
SELECT c.id AS course_id,
       c.title,
       count(DISTINCT e.student_id)                      AS students,
       count(DISTINCT t.id)                              AS tasks,
       round(avg(a.score), 3)                            AS avg_score,
       count(a.id)                                       AS attempts
FROM courses c
LEFT JOIN course_enrollments e ON e.course_id = c.id
LEFT JOIN levels l ON l.course_id = c.id
LEFT JOIN tasks t ON t.level_id = l.id
LEFT JOIN attempts a ON a.task_id = t.id
GROUP BY c.id, c.title;
