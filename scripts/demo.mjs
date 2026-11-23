#!/usr/bin/env node
// End-to-end defence scenario. Requires a running server (default :8080) and
// PostgreSQL access to read the answer key for the chosen task (demo only).
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';

const BASE = process.env.LUMINA_API ?? 'http://127.0.0.1:8080/api/v1';
const envFile = path.join(process.cwd(), '.env');
const env = Object.fromEntries(
  fs.existsSync(envFile)
    ? fs.readFileSync(envFile, 'utf8').split(/\r?\n/).filter((l) => l && !l.startsWith('#') && l.includes('=')).map((l) => {
        const i = l.indexOf('=');
        return [l.slice(0, i).trim(), l.slice(i + 1).trim()];
      })
    : [],
);
const DATABASE_URL = process.env.DATABASE_URL ?? env.DATABASE_URL ?? 'postgresql://lumina:lumina@127.0.0.1:5433/lumina';
const PSQL = process.env.PSQL ?? 'C:/Users/Lenovo/msys64/ucrt64/bin/psql.exe';

let passed = 0;
const check = (name, cond, extra = '') => {
  if (cond) {
    passed++;
    console.log(`  ✓ ${name}`);
  } else {
    console.error(`  ✗ ${name} ${extra}`);
    process.exitCode = 1;
    throw new Error(name);
  }
};

async function api(path, { method = 'GET', body, token } = {}) {
  const res = await fetch(BASE + path, {
    method,
    headers: { 'content-type': 'application/json', ...(token ? { authorization: `Bearer ${token}` } : {}) },
    body: body ? JSON.stringify(body) : undefined,
  });
  const json = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(`${method} ${path} -> ${res.status}: ${json?.error?.message ?? JSON.stringify(json)}`);
  return json.data;
}

function query(sql) {
  return execFileSync(PSQL, [DATABASE_URL, '-t', '-A', '-c', sql], { encoding: 'utf8' }).trim();
}

function answerFor(task) {
  const key = JSON.parse(query(`SELECT answer_key::text FROM tasks WHERE id = '${task.id}'`));
  switch (task.type) {
    case 'single_choice':
    case 'timed_quiz':
      return { option: key.correct[0] };
    case 'multiple_choice':
      return { options: key.correct };
    case 'true_false':
      return { value: key.correct };
    case 'short_answer':
      return { text: key.accepted[0] };
    case 'numeric':
      return { value: key.value };
    case 'matching':
      return { pairs: key.pairs };
    case 'ordering':
      return { order: key.order };
    case 'cloze':
      return { blanks: key.blanks };
    case 'code':
      return { code: '// demo submission\n' };
    default:
      throw new Error(`unknown type ${task.type}`);
  }
}

(async () => {
  console.log('Lumina demo scenario\n');
  const stamp = Date.now();
  const email = `demo${stamp}@lumina.kz`;

  console.log('1. Registration & onboarding');
  const reg = await api('/auth/register', {
    method: 'POST',
    body: {
      email,
      password: 'Lumina2026!',
      display_name: 'Демо Студент',
      nickname: `demo${stamp % 10000}`,
      locale: 'kk',
    },
  });
  check('student registered', !!reg.access_token);
  const token = reg.access_token;

  const courses = await api('/courses', { token });
  check('course catalogue loaded', Array.isArray(courses) && courses.length >= 4, `got ${courses?.length}`);
  const course = courses[0];
  const enrolled = await api(`/courses/${course.id}/enroll`, { method: 'POST', body: {}, token });
  check('enrolled in course', enrolled.enrolled === true);

  await api('/me', { method: 'PATCH', body: { daily_goal_xp: 50 }, token });
  const map = await api(`/courses/${course.id}/map`, { token });
  const level = map.levels.find((l) => l.state !== 'locked');
  check('first level unlocked', !!level, JSON.stringify(map.levels?.[0]));

  console.log('2. First task & grading');
  const task = await api(`/levels/${level.id}/next-task`, { token });
  check('task delivered without answer key', task.id && !('answer_key' in task));
  const answer = answerFor(task);
  const submit = await api(`/tasks/${task.id}/attempts`, {
    method: 'POST',
    body: { attempt_token: `demo-${stamp}-${task.id.slice(0, 8)}`, answer, time_spent_ms: 24000, hints_used: 0, context: 'normal' },
    token,
  });
  check('answer graded correct', submit.result === 'correct', submit.result);
  check('XP awarded server-side', submit.xp.amount > 0, `amount=${submit.xp.amount}`);
  check('XP journal updated', submit.xp.total_xp > 0);

  console.log('3. Achievements & idempotency');
  check('«Первый шаг» unlocked', submit.achievements.some((a) => a.code === 'first_step'), JSON.stringify(submit.achievements));
  const replay = await api(`/tasks/${task.id}/attempts`, {
    method: 'POST',
    body: { attempt_token: `demo-${stamp}-${task.id.slice(0, 8)}`, answer, time_spent_ms: 24000, hints_used: 0 },
    token,
  });
  check('replay did not double XP', replay.xp.total_xp === submit.xp.total_xp);

  console.log('4. AI next task with explanation');
  const ai = await api(`/ai/next-task?course_id=${course.id}`, { token });
  check('AI returned a task', !!ai.task?.id);
  check('decision is explained', typeof ai.decision?.explanation === 'string' && ai.decision.explanation.length > 10);
  check('reason code present', ['zpd', 'challenge', 'support', 'review', 'new_topic', 'daily_goal'].includes(ai.decision.reason_code));

  console.log('5. Leaderboard & reports');
  const board = await api('/leaderboards?scope=global&limit=10', { token });
  check('leaderboard has entries', board.entries.length > 0);
  check('my rank is visible', board.me.rank > 0);
  const progress = await api('/reports/progress', { token });
  check('progress report', Array.isArray(progress.courses));
  const xpReport = await api('/reports/xp?days=7', { token });
  check('XP report has daily series', Array.isArray(xpReport.daily) && xpReport.daily.length === 7);
  const topics = await api('/reports/topics', { token });
  check('topic report', Array.isArray(topics.topics));
  const achievements = await api('/me/achievements', { token });
  check('achievement collection', achievements.total >= 45 && achievements.unlocked >= 1, `${achievements.unlocked}/${achievements.total}`);
  const quests = await api('/me/quests', { token });
  check('daily quests assigned', (quests.quests ?? []).filter((q) => q.kind === 'daily').length === 3);

  console.log(`\nScenario completed: ${passed} checks passed.`);
})().catch((e) => {
  console.error('\nDEMO FAILED:', e.message);
  process.exit(1);
});
