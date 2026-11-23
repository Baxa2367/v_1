# Seed contract — course content

Every course is one file `seeds/courses/<slug>.json`. It is loaded by the C++
`lumina_seeder`. Strings must be JSON objects with `kk` and `ru` (required)
and `en` (optional, fallback ru). No HTML, plain text + `\n`. Max 600 chars
for prompts.

```jsonc
{
  "slug": "cpp-fundamentals",
  "title":        { "kk": "...", "ru": "...", "en": "..." },
  "description":  { "kk": "...", "ru": "...", "en": "..." },
  "category":     "programming",          // programming|math|database|language
  "difficulty":   4,                       // 1..10
  "tags":         ["cpp", "basics"],
  "cover":        "terminal",              // material symbol name
  "levels": [ /* exactly 8 */ ]
}
```

## Level

```jsonc
{
  "order_no": 1,
  "title":       { "kk": "...", "ru": "...", "en": "..." },
  "description": { "kk": "...", "ru": "...", "en": "..." },
  "icon": "terminal",
  "is_boss": false,                        // bosses: at least 2 per course
  "unlock_rules": { "min_xp": 0, "min_prev_accuracy": 0, "required_tasks": 0 },
  "topics": [ { "code": "cpp.variables", "name": { "kk": "...", "ru": "...", "en": "..." } } ],
  "tasks": [ /* exactly 10 */ ]
}
```

Topic `code` must be unique inside a course and referenced by tasks.
The first level has `min_xp: 0`; later levels use e.g. `min_xp: 120`,
`min_prev_accuracy: 0.6`.

## Task

Common fields:

```jsonc
{
  "type": "single_choice",
  "topic": "cpp.variables",
  "difficulty": 2,                 // 1..10
  "base_xp": 40,                   // 20..160
  "time_limit_sec": 120,           // 30..600
  "prompt": { "kk": "...", "ru": "..." },
  "explanation": { "kk": "...", "ru": "..." },
  "hints": [ { "kk": "...", "ru": "..." } ],   // 2..3 hints
  "payload": { },
  "answer_key": { }
}
```

### Types and payloads

1. `single_choice`
   `payload: {"options":[{"id":"a","text":{"kk":"..","ru":".."}}...]}`
   `answer_key: {"correct":["b"]}`
2. `multiple_choice` — same options, `answer_key: {"correct":["a","c"]}`
   (partial credit: correct picks / total correct − wrong picks / total wrong)
3. `true_false` — `payload: {"statement":{"kk":"..","ru":".."}}`,
   `answer_key: {"correct":true}`
4. `short_answer` — `payload: {"placeholder":{"kk":"..","ru":".."}}`,
   `answer_key: {"accepted":["std::vector","vector"],"case_sensitive":false}`
   (trim + collapse spaces is always applied; numbers compared numerically when
   both sides parse as number)
5. `matching` — `payload: {"left":[{"id":"l1","text":{...}}],"right":[{"id":"r1","text":{...}}]}`
   `answer_key: {"pairs":{"l1":"r2","l2":"r1"}}` (shuffle right on the client)
6. `ordering` — `payload: {"items":[{"id":"i1","text":{...}}...]}`
   `answer_key: {"order":["i2","i1","i3"]}` (display order is shuffled)
7. `cloze` — `payload: {"text":{"kk":"... {{1}} ... {{2}} ...","ru":"..."},"blanks":[{"id":"1","options":[{"id":"o1","text":{...}}]}]}`
   `answer_key: {"blanks":{"1":"o1","2":"o3"}}`
8. `numeric` — `payload: {"unit":{"kk":"..","ru":".."}}`,
   `answer_key: {"value": 42, "tolerance": 0.01}` (accepts comma decimals)
9. `timed_quiz` — like `single_choice` but short `time_limit_sec` (20..45),
   `payload: {"options":[...]}`, `answer_key: {"correct":["a"]}`.
10. `code` — C++ task:
    `payload: {"language":"cpp","starter_code":"...","visible_tests":[{"input":"3 4","expected_output":"7"}]}`
    `answer_key: {"tests":[{"input":"3 4","expected_output":"7"},{"input":"5 0","expected_output":"5"}]}`
    (hidden tests are in answer_key; the sandbox compiles with `g++ -std=c++20`
    and compares stdout trimmed, case-sensitive)

Every level must contain exactly 10 tasks. Use each of the 10 types at least
once across a course; within one level mix at least 6 distinct types.

Answer keys never reach the client. Write content in Kazakh and Russian,
correct and unambiguous, with an explanation for every task and 2–3 hints.
