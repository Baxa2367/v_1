#!/usr/bin/env node
// Structural validation of seeds/courses/*.json against seeds/schema/CONTRACT.md
import fs from 'node:fs';
import path from 'node:path';

const TYPES = new Set([
  'single_choice', 'multiple_choice', 'true_false', 'short_answer', 'matching',
  'ordering', 'cloze', 'numeric', 'timed_quiz', 'code',
]);
const errors = [];
const warns = [];

function hasText(v, where) {
  if (!v || typeof v !== 'object') { errors.push(`${where}: expected {kk,ru} object`); return; }
  for (const l of ['kk', 'ru']) {
    if (typeof v[l] !== 'string' || !v[l].trim()) errors.push(`${where}: missing "${l}"`);
  }
  if (v.en !== undefined && typeof v.en !== 'string') errors.push(`${where}.en must be string`);
}

function checkAnswerKey(t, where) {
  const k = t.answer_key;
  if (!k || typeof k !== 'object') { errors.push(`${where}: answer_key missing`); return; }
  const p = t.payload || {};
  const ids = (arr) => (arr || []).map((o) => o.id);
  switch (t.type) {
    case 'single_choice': case 'timed_quiz': {
      if (!Array.isArray(p.options) || p.options.length < 3) errors.push(`${where}: need >=3 options`);
      if (!Array.isArray(k.correct) || k.correct.length !== 1) errors.push(`${where}: correct must be [id]`);
      else if (!ids(p.options).includes(k.correct[0])) errors.push(`${where}: correct id not in options`);
      for (const [i, o] of (p.options || []).entries()) hasText(o.text, `${where}.options[${i}].text`);
      break;
    }
    case 'multiple_choice': {
      if (!Array.isArray(p.options) || p.options.length < 4) errors.push(`${where}: need >=4 options`);
      if (!Array.isArray(k.correct) || k.correct.length < 2) errors.push(`${where}: need >=2 correct`);
      for (const c of k.correct) if (!ids(p.options).includes(c)) errors.push(`${where}: correct ${c} not in options`);
      break;
    }
    case 'true_false':
      if (typeof k.correct !== 'boolean') errors.push(`${where}: correct must be boolean`);
      hasText(p.statement, `${where}.payload.statement`);
      break;
    case 'short_answer':
      if (!Array.isArray(k.accepted) || k.accepted.length < 1) errors.push(`${where}: accepted[] missing`);
      break;
    case 'matching': {
      const L = ids(p.left), R = ids(p.right);
      if (L.length < 3 || R.length !== L.length) errors.push(`${where}: need ${L.length} matching right items`);
      for (const [l, r] of Object.entries(k.pairs || {})) {
        if (!L.includes(l)) errors.push(`${where}: pair key ${l} unknown`);
        if (!R.includes(r)) errors.push(`${where}: pair value ${r} unknown`);
      }
      if (Object.keys(k.pairs || {}).length !== L.length) errors.push(`${where}: pairs must cover all left items`);
      break;
    }
    case 'ordering': {
      const I = ids(p.items);
      if (I.length < 3) errors.push(`${where}: need >=3 items`);
      if (!Array.isArray(k.order) || k.order.length !== I.length) errors.push(`${where}: order must cover all items`);
      else if (new Set(k.order).size !== I.length || k.order.some((x) => !I.includes(x))) errors.push(`${where}: order ids mismatch`);
      break;
    }
    case 'cloze': {
      const blanks = p.blanks || [];
      if (!blanks.length) errors.push(`${where}: blanks missing`);
      for (const b of blanks) {
        if (!Array.isArray(b.options) || b.options.length < 2) errors.push(`${where}: blank ${b.id} needs options`);
        if (!k.blanks || !(b.id in k.blanks)) errors.push(`${where}: answer for blank ${b.id} missing`);
      }
      hasText(p.text, `${where}.payload.text`);
      break;
    }
    case 'numeric':
      if (typeof k.value !== 'number') errors.push(`${where}: numeric value missing`);
      if (k.tolerance === undefined) warns.push(`${where}: tolerance default 0.01`);
      break;
    case 'code':
      if (!/^cpp|^c\+\+/.test(p.language || '')) errors.push(`${where}: language must be cpp`);
      if (typeof p.starter_code !== 'string' || p.starter_code.length < 10) errors.push(`${where}: starter_code too short`);
      if (!Array.isArray(p.visible_tests) || !p.visible_tests.length) errors.push(`${where}: visible_tests missing`);
      if (!Array.isArray(k.tests) || k.tests.length < 2) errors.push(`${where}: >=2 hidden tests required`);
      break;
    default:
      errors.push(`${where}: unknown type ${t.type}`);
  }
}

const files = process.argv.slice(2).length
  ? process.argv.slice(2)
  : fs.readdirSync('seeds/courses').filter((f) => f.endsWith('.json')).map((f) => path.join('seeds/courses', f));

if (!files.length) { console.error('no course files found'); process.exit(1); }

let totalTasks = 0;
for (const file of files) {
  const before = errors.length;
  const c = JSON.parse(fs.readFileSync(file, 'utf8'));
  const tag = path.basename(file);
  hasText(c.title, `${tag}.title`);
  hasText(c.description, `${tag}.description`);
  if (!c.slug) errors.push(`${tag}: slug missing`);
  if (!Array.isArray(c.levels) || c.levels.length < 8) errors.push(`${tag}: need >=8 levels`);
  const topicCodes = new Set();
  for (const lvl of c.levels || []) {
    for (const t of lvl.topics || []) { topicCodes.add(t.code); hasText(t.name, `${tag}.topic ${t.code}`); }
  }
  const seen = new Set();
  for (const lvl of c.levels || []) {
    const lw = `${tag}.L${lvl.order_no}`;
    hasText(lvl.title, `${lw}.title`);
    hasText(lvl.description, `${lw}.description`);
    if (!Array.isArray(lvl.tasks) || lvl.tasks.length !== 10) errors.push(`${lw}: exactly 10 tasks required, got ${(lvl.tasks || []).length}`);
    if (lvl.tasks) {
      for (const [i, t] of lvl.tasks.entries()) {
        totalTasks++;
        const where = `${lw}.T${i + 1}(${t.type})`;
        if (!TYPES.has(t.type)) errors.push(`${where}: bad type`);
        if (!topicCodes.has(t.topic)) errors.push(`${where}: unknown topic ${t.topic}`);
        if (!(t.difficulty >= 1 && t.difficulty <= 10)) errors.push(`${where}: difficulty 1..10`);
        if (!(t.base_xp >= 10 && t.base_xp <= 200)) errors.push(`${where}: base_xp 10..200`);
        if (!(t.time_limit_sec >= 15 && t.time_limit_sec <= 900)) errors.push(`${where}: time_limit_sec range`);
        hasText(t.prompt, `${where}.prompt`);
        hasText(t.explanation, `${where}.explanation`);
        if (!Array.isArray(t.hints) || t.hints.length < 2) errors.push(`${where}: need >=2 hints`);
        else t.hints.forEach((h, j) => hasText(h, `${where}.hints[${j}]`));
        seen.add(t.type);
        checkAnswerKey(t, where);
      }
    }
  }
  const bosses = (c.levels || []).filter((l) => l.is_boss).length;
  if (bosses < 2) warns.push(`${tag}: ${bosses} boss levels (>=2 recommended)`);
  if (seen.size < 6) warns.push(`${tag}: only ${seen.size} task types used`);
  console.log(`${tag}: ${(c.levels || []).length} levels, ${(c.levels || []).reduce((a, l) => a + (l.tasks || []).length, 0)} tasks, types=${[...seen].join(',')} ${errors.length === before ? 'OK' : 'FAIL'}`);
}

console.log(`\ntotal tasks: ${totalTasks}`);
for (const w of warns) console.warn('WARN', w);
if (errors.length) {
  console.error(`\n${errors.length} error(s):`);
  for (const e of errors.slice(0, 60)) console.error(' -', e);
  if (errors.length > 60) console.error(` ... +${errors.length - 60} more`);
  process.exit(1);
}
console.log('validation OK');
