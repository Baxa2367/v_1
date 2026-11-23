import { useEffect, useMemo, useRef, useState } from 'react';
import { Link, useNavigate, useParams, useSearchParams } from 'react-router-dom';
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { useTranslation } from 'react-i18next';
import clsx from 'clsx';
import { api } from '../api/client';
import type { AttemptResult, Task } from '../api/types';
import { useAuth } from '../store/auth';
import { ErrorState, Icon, Progress, Spinner, useToasts } from '../components/ui';

function newToken() {
  if (typeof crypto !== 'undefined' && 'randomUUID' in crypto) return crypto.randomUUID();
  return `att-${Date.now()}-${Math.random().toString(36).slice(2, 10)}`;
}

function shuffle<T>(arr: T[]): T[] {
  const a = [...arr];
  for (let i = a.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [a[i], a[j]] = [a[j], a[i]];
  }
  return a;
}

export default function TaskPage() {
  const { id } = useParams();
  const [params] = useSearchParams();
  const levelId = params.get('level') ?? '';
  const { t } = useTranslation();
  const navigate = useNavigate();
  const queryClient = useQueryClient();
  const push = useToasts((s) => s.push);
  const refreshMe = useAuth((s) => s.refreshMe);

  const task = useQuery({ queryKey: ['task', id], queryFn: () => api<Task>(`/tasks/${id}`) });
  const [answer, setAnswer] = useState<any>(null);
  const [hintsUsed, setHintsUsed] = useState(0);
  const [shownHints, setShownHints] = useState<string[]>([]);
  const [elapsed, setElapsed] = useState(0);
  const [result, setResult] = useState<AttemptResult | null>(null);
  const started = useRef(Date.now());

  const data = task.data;
  const limit = data?.time_limit_sec ?? 120;

  useEffect(() => {
    if (!data || result) return;
    started.current = Date.now();
    const timer = setInterval(() => setElapsed(Math.floor((Date.now() - started.current) / 1000)), 1000);
    return () => clearInterval(timer);
  }, [data?.id, result]);

  useEffect(() => {
    if (!data) return;
    const p = data.payload;
    setResult(null);
    setHintsUsed(0);
    setShownHints([]);
    setElapsed(0);
    started.current = Date.now();
    if (data.type === 'multiple_choice') setAnswer({ options: [] });
    else if (data.type === 'matching') setAnswer({ pairs: {} });
    else if (data.type === 'ordering') setAnswer({ order: (p.items ?? []).map((x) => x.id) });
    else if (data.type === 'cloze') setAnswer({ blanks: {} });
    else if (data.type === 'code') setAnswer({ code: p.starter_code ?? '' });
    else if (data.type === 'numeric') setAnswer({ text: '' });
    else if (data.type === 'short_answer') setAnswer({ text: '' });
    else setAnswer({});
  }, [data?.id]);

  const submit = useMutation({
    mutationFn: async () => {
      const timeSpent = Math.min(limit * 3000, Date.now() - started.current);
      return api<AttemptResult>(`/tasks/${id}/attempts`, {
        method: 'POST',
        body: {
          attempt_token: newToken(),
          answer,
          time_spent_ms: timeSpent,
          hints_used: hintsUsed,
          context: 'normal',
        },
      });
    },
    onSuccess: (res) => {
      setResult(res);
      refreshMe();
      queryClient.invalidateQueries({ queryKey: ['dashboard'] });
      queryClient.invalidateQueries({ queryKey: ['level', levelId] });
      queryClient.invalidateQueries({ queryKey: ['map'] });
      if (res.xp.level_up) push({ title: t('task.level_up'), body: `${res.xp.level} · ${res.xp.title}`, icon: 'military_tech', tone: 'amber' });
    },
    onError: (e: any) => push({ title: e.message, icon: 'error', tone: 'danger' }),
  });

  const revealHint = () => {
    if (!data || hintsUsed >= data.hints.length) return;
    const hint = data.hints[hintsUsed];
    setShownHints((h) => [...h, hint.text]);
    setHintsUsed((n) => n + 1);
  };

  const canSubmit = useMemo(() => {
    if (!data || !answer) return false;
    switch (data.type) {
      case 'single_choice':
      case 'timed_quiz':
        return !!answer.option;
      case 'multiple_choice':
        return (answer.options ?? []).length > 0;
      case 'true_false':
        return typeof answer.value === 'boolean';
      case 'short_answer':
        return (answer.text ?? '').trim().length > 0;
      case 'numeric':
        return (answer.text ?? '').trim().length > 0;
      case 'matching':
        return Object.keys(answer.pairs ?? {}).length === (data.payload.left ?? []).length;
      case 'ordering':
        return (answer.order ?? []).length === (data.payload.items ?? []).length;
      case 'cloze':
        return Object.keys(answer.blanks ?? {}).length === (data.payload.blanks ?? []).length;
      case 'code':
        return (answer.code ?? '').trim().length > 10;
      default:
        return true;
    }
  }, [answer, data]);

  if (task.isLoading) return <Spinner />;
  if (task.error) return <ErrorState error={task.error} onRetry={() => task.refetch()} />;
  if (!data) return null;

  const p = data.payload;
  const timeRatio = Math.min(1, elapsed / limit);
  const timeLeft = Math.max(0, limit - elapsed);
  const fmt = (s: number) => `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;

  return (
    <div className="space-y-4">
      <div className="flex flex-wrap items-center justify-between gap-2">
        <div className="flex items-center gap-2 text-xs text-muted">
          <Link to={levelId ? `/levels/${levelId}` : '/courses'} className="hover:text-ink">
            <Icon name="arrow_back" className="text-[16px]" /> {t('task.back_to_level')}
          </Link>
          <span className="hidden sm:inline">·</span>
          <span className="hidden font-mono uppercase tracking-wider sm:inline">{data.topic?.name}</span>
        </div>
        <div className="flex items-center gap-2">
          <span className="chip bg-panel2 text-muted">{t('common.difficulty')} {data.difficulty}/10</span>
          <span className={clsx('chip font-mono', timeRatio > 0.85 ? 'bg-red-50 text-red-600 dark:bg-red-500/10' : 'bg-brand-50 text-brand-700 dark:bg-brand-500/10 dark:text-brand-300')}>
            <Icon name="timer" className="text-[14px]" /> {fmt(timeLeft)}
          </span>
        </div>
      </div>

      <div className="grid grid-cols-1 gap-4 lg:grid-cols-[1.15fr,1fr]">
        {/* Prompt */}
        <section className="card flex flex-col p-5">
          <div className="flex flex-wrap items-center gap-2">
            <span className="chip bg-brand-50 uppercase tracking-wider text-brand-700 dark:bg-brand-500/10 dark:text-brand-300">
              {t('task.question')}
            </span>
            <span className="chip bg-panel2 text-muted">{data.type.replace('_', ' ')}</span>
            <span className="ml-auto font-mono text-[11px] text-amber">+{data.base_xp} XP</span>
          </div>

          <h1 className="mt-3 text-xl font-bold leading-snug whitespace-pre-wrap">{data.prompt}</h1>

          {data.type === 'true_false' && p.statement && (
            <div className="mt-4 rounded-xl bg-panel2 p-4 text-sm font-medium">{p.statement}</div>
          )}

          {p.visible_tests && p.visible_tests.length > 0 && (
            <div className="mt-4 rounded-xl bg-slate-900 p-3 font-mono text-[11px] text-slate-200">
              <div className="mb-1 text-slate-400">// visible tests</div>
              {p.visible_tests.map((tc, i) => (
                <div key={i} className="mt-1">
                  <span className="text-emerald-300">input</span>: {tc.input || '(empty)'} → <span className="text-sky-300">out</span>: {tc.expected_output}
                </div>
              ))}
            </div>
          )}

          <div className="mt-auto pt-5">
            <div className="mb-2 flex items-center justify-between">
              <span className="label">{t('task.hint')}</span>
              <span className="font-mono text-[10px] text-muted">{hintsUsed}/{data.hints.length}</span>
            </div>
            <div className="space-y-1.5">
              {shownHints.map((h, i) => (
                <div key={i} className="flex gap-2 rounded-xl bg-amber-50 p-2.5 text-xs text-amber-900 dark:bg-amber-500/10 dark:text-amber-200">
                  <Icon name="lightbulb" className="text-[16px]" fill /> {h}
                </div>
              ))}
            </div>
            <button className="btn-ghost mt-2 w-full" onClick={revealHint} disabled={hintsUsed >= data.hints.length || !!result}>
              <Icon name="lightbulb" /> {t('task.hint')} · {t('task.hint_cost')}
            </button>
          </div>
        </section>

        {/* Answer */}
        <section className="card flex flex-col p-5">
          {!result ? (
            <>
              <div className="mb-3 flex items-center justify-between">
                <div className="label">ANSWER</div>
                <span className="font-mono text-[10px] text-muted">{data.type}</span>
              </div>

              {data.type === 'single_choice' || data.type === 'timed_quiz' ? (
                <div className="space-y-2">
                  {(p.options ?? []).map((o) => (
                    <button
                      key={o.id}
                      onClick={() => setAnswer({ option: o.id })}
                      className={clsx(
                        'flex w-full items-center gap-3 rounded-xl border p-3 text-left text-sm transition-all',
                        answer?.option === o.id ? 'border-brand-500 bg-brand-50/60 dark:bg-brand-500/10' : 'border-line hover:border-brand-300',
                      )}
                    >
                      <span className={clsx('flex h-7 w-7 items-center justify-center rounded-full border font-mono text-xs font-bold', answer?.option === o.id ? 'border-brand-600 bg-brand-600 text-white' : 'border-line text-muted')}>
                        {o.id.toUpperCase()}
                      </span>
                      <span className="flex-1">{o.text}</span>
                      {answer?.option === o.id && <Icon name="check_circle" className="text-brand-600" fill />}
                    </button>
                  ))}
                </div>
              ) : null}

              {data.type === 'multiple_choice' ? (
                <div className="space-y-2">
                  {(p.options ?? []).map((o) => {
                    const checked = (answer?.options ?? []).includes(o.id);
                    return (
                      <button
                        key={o.id}
                        onClick={() => {
                          const cur = answer?.options ?? [];
                          setAnswer({ options: checked ? cur.filter((x: string) => x !== o.id) : [...cur, o.id] });
                        }}
                        className={clsx(
                          'flex w-full items-center gap-3 rounded-xl border p-3 text-left text-sm transition-all',
                          checked ? 'border-brand-500 bg-brand-50/60 dark:bg-brand-500/10' : 'border-line hover:border-brand-300',
                        )}
                      >
                        <span className={clsx('flex h-7 w-7 items-center justify-center rounded-md border', checked ? 'border-brand-600 bg-brand-600 text-white' : 'border-line')}>
                          {checked && <Icon name="check" className="text-[16px]" />}
                        </span>
                        <span className="flex-1">{o.text}</span>
                      </button>
                    );
                  })}
                </div>
              ) : null}

              {data.type === 'true_false' ? (
                <div className="grid grid-cols-2 gap-3">
                  {[true, false].map((v) => (
                    <button
                      key={String(v)}
                      onClick={() => setAnswer({ value: v })}
                      className={clsx(
                        'flex flex-col items-center gap-2 rounded-2xl border p-6 transition-all',
                        answer?.value === v ? 'border-brand-500 bg-brand-50/60 dark:bg-brand-500/10' : 'border-line hover:border-brand-300',
                      )}
                    >
                      <Icon name={v ? 'check_circle' : 'cancel'} className={v ? 'text-success text-[30px]' : 'text-red-500 text-[30px]'} />
                      <span className="font-semibold">{v ? 'TRUE' : 'FALSE'}</span>
                    </button>
                  ))}
                </div>
              ) : null}

              {data.type === 'short_answer' ? (
                <input
                  className="input"
                  autoFocus
                  placeholder={t('task.type_answer')}
                  value={answer?.text ?? ''}
                  onChange={(e) => setAnswer({ text: e.target.value })}
                />
              ) : null}

              {data.type === 'numeric' ? (
                <div className="flex items-center gap-2">
                  <input
                    className="input font-mono text-lg"
                    autoFocus
                    inputMode="decimal"
                    placeholder="0"
                    value={answer?.text ?? ''}
                    onChange={(e) => setAnswer({ text: e.target.value })}
                  />
                  {p.unit && <span className="text-sm text-muted">{p.unit}</span>}
                </div>
              ) : null}

              {data.type === 'matching' ? (
                <div className="space-y-2">
                  {(p.left ?? []).map((l) => (
                    <div key={l.id} className="rounded-xl border border-line p-2.5">
                      <div className="mb-1.5 text-sm font-medium">{l.text}</div>
                      <select
                        className="input py-1.5 text-xs"
                        value={answer?.pairs?.[l.id] ?? ''}
                        onChange={(e) => setAnswer({ pairs: { ...(answer?.pairs ?? {}), [l.id]: e.target.value } })}
                      >
                        <option value="">—</option>
                        {(p.right ?? []).map((r) => (
                          <option key={r.id} value={r.id}>{r.text}</option>
                        ))}
                      </select>
                    </div>
                  ))}
                </div>
              ) : null}

              {data.type === 'ordering' ? (
                <div className="space-y-2">
                  {(answer?.order ?? []).map((itemId: string, idx: number) => {
                    const item = (p.items ?? []).find((x) => x.id === itemId);
                    return (
                      <div key={itemId} className="flex items-center gap-2 rounded-xl border border-line p-2.5">
                        <span className="flex h-6 w-6 items-center justify-center rounded-md bg-panel2 font-mono text-[10px]">{idx + 1}</span>
                        <span className="flex-1 text-sm">{item?.text}</span>
                        <div className="flex flex-col">
                          <button
                            className="text-muted hover:text-ink disabled:opacity-30"
                            disabled={idx === 0}
                            onClick={() => {
                              const next = [...answer.order];
                              [next[idx - 1], next[idx]] = [next[idx], next[idx - 1]];
                              setAnswer({ order: next });
                            }}
                          >
                            <Icon name="keyboard_arrow_up" className="text-[18px]" />
                          </button>
                          <button
                            className="text-muted hover:text-ink disabled:opacity-30"
                            disabled={idx === (answer?.order ?? []).length - 1}
                            onClick={() => {
                              const next = [...answer.order];
                              [next[idx + 1], next[idx]] = [next[idx], next[idx + 1]];
                              setAnswer({ order: next });
                            }}
                          >
                            <Icon name="keyboard_arrow_down" className="text-[18px]" />
                          </button>
                        </div>
                      </div>
                    );
                  })}
                </div>
              ) : null}

              {data.type === 'cloze' ? (
                <div className="space-y-3">
                  <div className="whitespace-pre-wrap text-sm leading-8">
                    {(p.text ?? '').split(/(\{\{\d+\}\})/g).map((part, i) => {
                      const m = part.match(/^\{\{(\d+)\}\}$/);
                      if (!m) return <span key={i}>{part}</span>;
                      const blankId = m[1];
                      const blank = (p.blanks ?? []).find((b) => b.id === blankId);
                      return (
                        <select
                          key={i}
                          className="mx-1 inline-block rounded-lg border border-brand-300 bg-brand-50/60 px-2 py-1 text-xs dark:bg-brand-500/10"
                          value={answer?.blanks?.[blankId] ?? ''}
                          onChange={(e) => setAnswer({ blanks: { ...(answer?.blanks ?? {}), [blankId]: e.target.value } })}
                        >
                          <option value="">—</option>
                          {(blank?.options ?? []).map((o) => (
                            <option key={o.id} value={o.id}>{o.text}</option>
                          ))}
                        </select>
                      );
                    })}
                  </div>
                  <p className="text-xs text-muted">{t('task.fill')}</p>
                </div>
              ) : null}

              {data.type === 'code' ? (
                <div className="flex flex-1 flex-col">
                  <div className="mb-2 flex items-center justify-between">
                    <span className="font-mono text-[10px] uppercase text-muted">C++20 · stdin/stdout</span>
                  </div>
                  <textarea
                    className="min-h-[260px] w-full flex-1 rounded-xl border border-line bg-slate-950 p-3 font-mono text-[12px] leading-relaxed text-slate-100 outline-none focus:border-brand-500"
                    spellCheck={false}
                    value={answer?.code ?? ''}
                    onChange={(e) => setAnswer({ code: e.target.value })}
                  />
                </div>
              ) : null}

              <div className="mt-auto pt-4">
                <Progress className="mb-3" value={timeRatio * 100} tone={timeRatio > 0.85 ? 'amber' : 'brand'} />
                <button className="btn-primary w-full" disabled={!canSubmit || submit.isPending} onClick={() => submit.mutate()}>
                  {submit.isPending ? t('common.loading') : t('task.check')} <Icon name="check" />
                </button>
              </div>
            </>
          ) : (
            <Feedback result={result} data={data} levelId={levelId} onNext={(next) => {
              if (next) navigate(`/tasks/${next}?level=${levelId}`);
              else navigate(`/levels/${levelId || result.level.level_id}`);
            }} />
          )}
        </section>
      </div>
    </div>
  );
}

function Feedback({ result, data, levelId, onNext }: { result: AttemptResult; data: Task; levelId: string; onNext: (next?: string) => void }) {
  const { t } = useTranslation();
  const good = result.result === 'correct';
  const partial = result.result === 'partial';
  const correctLabel =
    data.type === 'single_choice' || data.type === 'timed_quiz'
      ? (data.payload.options ?? []).find((o) => o.id === result.answer_key?.correct?.[0])?.text
      : data.type === 'true_false'
        ? String(result.answer_key?.correct)
        : data.type === 'numeric'
          ? String(result.answer_key?.value)
          : data.type === 'short_answer'
            ? (result.answer_key?.accepted ?? []).slice(0, 3).join(' / ')
            : null;

  return (
    <div className="flex h-full flex-col">
      <div
        className={clsx(
          'rounded-2xl p-4',
          good ? 'bg-emerald-50 dark:bg-emerald-500/10' : partial ? 'bg-amber-50 dark:bg-amber-500/10' : 'bg-red-50 dark:bg-red-500/10',
        )}
      >
        <div className="flex items-center gap-3">
          <span
            className={clsx(
              'flex h-11 w-11 items-center justify-center rounded-2xl text-white',
              good ? 'bg-success' : partial ? 'bg-amber' : 'bg-red-500',
            )}
          >
            <Icon name={good ? 'check' : partial ? 'percent' : 'close'} className="text-[24px]" />
          </span>
          <div>
            <div className="text-lg font-bold">{good ? t('task.correct') : partial ? t('task.partial') : t('task.wrong')}</div>
            <div className="font-mono text-xs text-muted">
              {Math.round(result.score * 100)}% · {t('task.xp_gained', { xp: result.xp.amount })}
              {result.combo > 1 && ` · ${t('task.combo')} x${result.combo}`}
            </div>
          </div>
          <div className="ml-auto text-right">
            <div className="font-mono text-xl font-bold text-brand-600 dark:text-brand-300">{result.xp.total_xp}</div>
            <div className="text-[10px] uppercase text-muted">total XP</div>
          </div>
        </div>

        {result.suspicious && (
          <div className="mt-2 rounded-lg bg-amber-100 px-3 py-2 text-xs text-amber-900 dark:bg-amber-500/20 dark:text-amber-200">
            <Icon name="shield" className="text-[14px]" /> Anti-cheat: XP reduced
          </div>
        )}
      </div>

      {result.level.completed && (
        <div className="mt-3 rounded-2xl border border-emerald-200 bg-emerald-50/60 p-4 dark:border-emerald-500/30 dark:bg-emerald-500/10">
          <div className="flex items-center justify-between">
            <div>
              <div className="text-sm font-bold text-emerald-800 dark:text-emerald-200">{t('level.completed')}</div>
              <div className="mt-0.5 font-mono text-[11px] text-emerald-700 dark:text-emerald-300">
                {t('level.bonus')}: +{result.level.bonus_xp} XP · {Math.round(result.level.accuracy * 100)}%
              </div>
            </div>
            <div className="flex items-center gap-0.5">
              {[1, 2, 3].map((s) => (
                <Icon key={s} name="star" className={clsx('text-[24px]', s <= result.level.stars ? 'text-amber' : 'text-emerald-200/60')} fill={s <= result.level.stars} />
              ))}
            </div>
          </div>
        </div>
      )}

      {result.achievements?.length > 0 && (
        <div className="mt-3 space-y-1.5">
          {result.achievements.map((a) => (
            <div key={a.code} className="flex items-center gap-2 rounded-xl bg-amber-50 px-3 py-2 dark:bg-amber-500/10">
              <Icon name={a.icon} className="text-amber" fill />
              <div className="min-w-0 flex-1">
                <div className="truncate text-xs font-semibold">{a.title}</div>
                <div className="text-[10px] uppercase text-muted">{a.rarity}</div>
              </div>
              <Icon name="emoji_events" className="text-amber" />
            </div>
          ))}
        </div>
      )}

      <div className="mt-3 rounded-xl border border-line p-3">
        <div className="label mb-1">{t('task.explanation')}</div>
        <p className="text-sm leading-relaxed">{result.explanation}</p>
        {correctLabel && (
          <div className="mt-2 text-xs text-muted">
            <span className="font-semibold text-success">✓</span> {correctLabel}
          </div>
        )}
        {result.grading?.compile_error && (
          <pre className="mt-2 max-h-40 overflow-auto rounded-lg bg-slate-950 p-2 font-mono text-[10px] text-red-300">
            {result.grading.compile_error}
          </pre>
        )}
        {result.grading?.cases?.length > 0 && (
          <div className="mt-2 font-mono text-[10px] text-muted">
            {t('task.tests_passed', { passed: result.grading.passed, total: result.grading.total })}
          </div>
        )}
      </div>

      <div className="mt-auto flex flex-col gap-2 pt-4">
        <button className="btn-primary w-full" onClick={() => onNext(result.next_task_id ?? undefined)}>
          {result.next_task_id ? t('task.next_task') : t('task.finish')} <Icon name="arrow_forward" />
        </button>
        <button className="btn-ghost w-full" onClick={() => onNext(undefined)}>
          {t('task.back_to_level')}
        </button>
      </div>
    </div>
  );
}
