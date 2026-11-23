import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { Link } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { Bar, BarChart, Cell, ResponsiveContainer, Tooltip, XAxis } from 'recharts';
import clsx from 'clsx';
import { api } from '../api/client';
import { useAuth } from '../store/auth';
import { Avatar, Coin, ErrorState, Icon, Progress, SectionHeader, Spinner, useToasts } from '../components/ui';

function Ring({ value, size = 72, stroke = 6, children }: { value: number; size?: number; stroke?: number; children?: any }) {
  const r = (size - stroke) / 2;
  const c = 2 * Math.PI * r;
  return (
    <div className="relative flex items-center justify-center" style={{ width: size, height: size }}>
      <svg width={size} height={size} className="-rotate-90">
        <circle cx={size / 2} cy={size / 2} r={r} fill="none" stroke="rgb(var(--line))" strokeWidth={stroke} />
        <circle
          cx={size / 2}
          cy={size / 2}
          r={r}
          fill="none"
          stroke="#4f46e5"
          strokeWidth={stroke}
          strokeLinecap="round"
          strokeDasharray={`${c * Math.min(1, value)} ${c}`}
        />
      </svg>
      <div className="absolute inset-0 flex items-center justify-center">{children}</div>
    </div>
  );
}

export default function DashboardPage() {
  const { t } = useTranslation();
  const { profile } = useAuth();
  const queryClient = useQueryClient();
  const push = useToasts((s) => s.push);
  const isStudent = !profile?.role || profile.role === 'student';

  const dash = useQuery({
    queryKey: ['dashboard'],
    queryFn: () => api('/me/dashboard'),
    enabled: isStudent,
  });
  const courses = useQuery({
    queryKey: ['courses', 'enrolled'],
    queryFn: () => api('/courses', { params: { enrolled: 1 } }),
    enabled: isStudent,
  });
  const board = useQuery({
    queryKey: ['leaderboard', 'global', 'mini'],
    queryFn: () => api('/leaderboards', { params: { scope: 'global', limit: 5 } }),
    enabled: isStudent,
  });
  const xp = useQuery({
    queryKey: ['reports', 'xp', 14],
    queryFn: () => api('/reports/xp', { params: { days: 14 } }),
    enabled: isStudent,
  });

  const claim = useMutation({
    mutationFn: (id: string) => api(`/quests/${id}/claim`, { method: 'POST', body: {} }),
    onSuccess: (res: any) => {
      push({ title: `+${res.reward_xp} XP`, body: `+${res.reward_coins} coins`, icon: 'task_alt', tone: 'success' });
      queryClient.invalidateQueries({ queryKey: ['dashboard'] });
      queryClient.invalidateQueries({ queryKey: ['quests'] });
    },
  });

  if (!isStudent) {
    return (
      <div className="card mx-auto max-w-xl p-8 text-center">
        <div className="mx-auto flex h-14 w-14 items-center justify-center rounded-2xl bg-brand-50 text-brand-600">
          <Icon name="school" />
        </div>
        <h1 className="mt-4 text-xl font-bold">{t('teacher.title')}</h1>
        <p className="mt-1 text-sm text-muted"> {t('teacher.analytics')}</p>
        <Link to="/teacher" className="btn-primary mt-5">
          {t('teacher.title')} <Icon name="arrow_forward" />
        </Link>
      </div>
    );
  }

  if (dash.isLoading) return <Spinner />;
  if (dash.error) return <ErrorState error={dash.error} onRetry={() => dash.refetch()} />;

  const d = dash.data;
  const p = d.profile;
  const goalPct = d.today.goal_xp ? Math.min(1, d.today.xp_earned / d.today.goal_xp) : 0;
  const daily = xp.data?.daily ?? [];
  const chart = daily.map((x: any) => ({ day: x.day.slice(5), xp: Number(x.xp) }));
  const quests = d.quests?.quests ?? [];
  const dailyQuests = quests.filter((q: any) => q.kind === 'daily').slice(0, 3);
  const continueItem = d.continue;
  const enrolled = courses.data ?? [];

  return (
    <div className="space-y-5">
      <section className="flex flex-col gap-3 lg:flex-row lg:items-end lg:justify-between">
        <div>
          <div className="flex flex-wrap items-center gap-2">
            <span className="chip bg-panel2 uppercase tracking-wider text-muted">
              {t('common.today')} · {new Date().toLocaleDateString()}
            </span>
            <span className="h-1.5 w-1.5 rounded-full bg-success" />
            <span className="text-xs font-semibold text-success">Live</span>
          </div>
          <h1 className="mt-2 text-3xl font-extrabold tracking-tight">
            {t('dash.greeting', { name: p.nickname || p.display_name })}
          </h1>
          <p className="mt-1 max-w-2xl text-sm text-muted">
            {d.streak.current > 0
              ? `${d.streak.current} ${t('common.streak')} · ${t('dash.subtitle')}`
              : t('dash.subtitle')}
          </p>
        </div>
        <div className="flex items-center gap-2">
          <Link to="/ai" className="btn-soft">
            <Icon name="auto_awesome" /> {t('ai.title')}
          </Link>
          <Link to="/courses" className="btn-primary">
            <Icon name="add_task" /> {t('dash.start_learning')}
          </Link>
        </div>
      </section>

      {/* Metric nodes */}
      <section className="grid grid-cols-1 gap-3 sm:grid-cols-2 xl:grid-cols-4">
        <div className="card p-4">
          <div className="flex items-start justify-between">
            <div>
              <div className="label">{t('common.level')} · {p.title}</div>
              <div className="mt-1 text-lg font-bold">{p.level}-{t('common.level')}</div>
            </div>
            <span className="flex h-10 w-10 items-center justify-center rounded-xl bg-brand-50 text-brand-600 dark:bg-brand-500/10 dark:text-brand-300">
              <Icon name="terminal" fill />
            </span>
          </div>
          <div className="mt-3 flex items-center justify-between font-mono text-[11px]">
            <span className="text-muted">{p.xp_in_level} / {p.xp_to_next} XP</span>
            <span className="font-semibold text-success">+{d.today.xp_earned} {t('common.today')}</span>
          </div>
          <Progress className="mt-1.5" value={(p.level_progress ?? 0) * 100} />
        </div>

        <div className="card p-4">
          <div className="flex items-start justify-between">
            <div>
              <div className="label">{t('dash.streak_title')}</div>
              <div className="mt-1 flex items-center gap-1.5 text-lg font-bold">
                {d.streak.current} {t('common.streak')} <span className="text-amber">🔥</span>
              </div>
            </div>
            <span className="flex h-10 w-10 items-center justify-center rounded-xl bg-amber-50 text-amber-600 dark:bg-amber-500/10 dark:text-amber-300">
              <Icon name="local_fire_department" fill />
            </span>
          </div>
          <div className="mt-3 flex items-center justify-between">
            {Array.from({ length: 7 }).map((_, i) => (
              <div key={i} className="flex flex-col items-center gap-1">
                <span
                  className={clsx(
                    'flex h-6 w-6 items-center justify-center rounded-full font-mono text-[10px] font-bold',
                    i < Math.min(6, d.streak.current)
                      ? 'bg-emerald-100 text-emerald-700 dark:bg-emerald-500/20 dark:text-emerald-300'
                      : 'bg-panel2 text-muted',
                  )}
                >
                  {i < Math.min(6, d.streak.current) ? '✓' : '·'}
                </span>
              </div>
            ))}
          </div>
        </div>

        <div className="card flex items-center justify-between p-4">
          <div>
            <div className="label">{t('dash.daily_goal')}</div>
            <div className="mt-1 text-lg font-bold">{d.today.xp_earned} / {d.today.goal_xp} XP</div>
            <div className="mt-1 text-xs text-muted">
              {d.today.completed ? t('dash.goal_done') : t('dash.goal_left', { xp: Math.max(0, d.today.goal_xp - d.today.xp_earned) })}
            </div>
          </div>
          <Ring value={goalPct}>
            <span className="font-mono text-sm font-bold">{Math.round(goalPct * 100)}%</span>
          </Ring>
        </div>

        <div className="card p-4">
          <div className="flex items-start justify-between">
            <div>
              <div className="label">{t('nav.leaderboard')} · {t('dash.league')}</div>
              <div className="mt-1 text-lg font-bold">#{d.rank.global} {p.league}</div>
            </div>
            <span className="flex h-10 w-10 items-center justify-center rounded-xl bg-panel2 text-brand-600">
              <Icon name="workspace_premium" fill />
            </span>
          </div>
          <div className="mt-3 flex items-center justify-between text-xs">
            <span className="inline-flex items-center gap-1 font-semibold text-success">
              <Icon name="arrow_upward" className="text-[14px]" /> {t('common.week')}: #{d.rank.weekly}
            </span>
            <span className="chip bg-emerald-100 text-emerald-700 dark:bg-emerald-500/20 dark:text-emerald-300">
              {d.rank.weekly_xp} XP
            </span>
          </div>
        </div>
      </section>

      {/* Continue hero */}
      <section className="relative overflow-hidden rounded-2xl hero-gradient p-6 text-white shadow-lg lg:p-8">
        <div className="absolute -bottom-24 -right-16 h-80 w-80 rounded-full bg-emerald-400/20 blur-3xl" />
        <div className="relative flex flex-col justify-between gap-6 lg:flex-row lg:items-center">
          <div className="max-w-2xl">
            <div className="flex flex-wrap items-center gap-2">
              <span className="chip bg-white/15 uppercase tracking-wider text-white backdrop-blur">
                {t('dash.continue_course')}
              </span>
              <span className="chip bg-emerald-300 text-emerald-950">
                <Icon name="bolt" className="text-[14px]" fill /> 120 XP
              </span>
            </div>
            <h2 className="mt-3 text-2xl font-extrabold tracking-tight lg:text-3xl">
              {continueItem?.level_title ?? continueItem?.course_title ?? t('dash.no_course')}
            </h2>
            <p className="mt-1.5 text-sm text-white/70">{continueItem?.course_title ?? t('courses_hint')}</p>
            {continueItem && (
              <div className="mt-4 max-w-md">
                <div className="flex items-center justify-between font-mono text-[11px] text-white/80">
                  <span>{t('course.progress')} · {Math.round(continueItem.progress_pct ?? 0)}%</span>
                  <span>{continueItem.order_no} {t('common.level')}</span>
                </div>
                <div className="mt-1.5 h-2 overflow-hidden rounded-full bg-white/20">
                  <div className="h-full rounded-full bg-emerald-300" style={{ width: `${Math.min(100, continueItem.progress_pct ?? 0)}%` }} />
                </div>
              </div>
            )}
          </div>
          <div className="flex flex-col gap-2 sm:flex-row lg:flex-col">
            <Link
              to={continueItem ? `/levels/${continueItem.level_id}` : '/courses'}
              className="btn bg-success text-white hover:brightness-95"
            >
              {t('common.continue')} <Icon name="arrow_forward" />
            </Link>
            <Link to="/ai" className="btn bg-white/10 text-white backdrop-blur hover:bg-white/20">
              <Icon name="auto_awesome" /> {t('ai.start')}
            </Link>
          </div>
        </div>
      </section>

      <div className="grid grid-cols-1 gap-5 xl:grid-cols-12">
        <div className="space-y-5 xl:col-span-8">
          <section className="card p-5">
            <SectionHeader
              title={t('nav.courses')}
              subtitle={t('course.all_courses')}
              action={
                <Link to="/courses" className="text-sm font-semibold text-brand-600 hover:underline dark:text-brand-300">
                  {t('common.view_all')}
                </Link>
              }
            />
            <div className="space-y-2.5">
              {enrolled.length === 0 && <div className="p-6 text-center text-sm text-muted">{t('dash.no_course')}</div>}
              {enrolled.slice(0, 4).map((c: any) => (
                <Link
                  key={c.id}
                  to={`/courses/${c.id}`}
                  className="flex items-center gap-3 rounded-xl bg-panel2/60 p-3 transition-colors hover:bg-panel2"
                >
                  <span className="flex h-11 w-11 items-center justify-center rounded-xl bg-brand-50 text-brand-600 dark:bg-brand-500/10 dark:text-brand-300">
                    <Icon name={c.cover || 'school'} />
                  </span>
                  <div className="min-w-0 flex-1">
                    <div className="flex items-center gap-2">
                      <span className="truncate font-semibold">{c.title}</span>
                      <span className="chip bg-panel text-[9px] uppercase text-muted">{c.category}</span>
                    </div>
                    <div className="mt-0.5 text-xs text-muted">
                      {c.completed_levels} / {c.levels_count} {t('course.levels').toLowerCase()}
                    </div>
                    <Progress className="mt-2 h-1.5" value={c.progress_pct} />
                  </div>
                  <Icon name="chevron_right" className="text-muted" />
                </Link>
              ))}
            </div>
          </section>

          <section className="card p-5">
            <SectionHeader
              title={t('dash.activity')}
              subtitle={`${xp.data?.progress?.total_xp ?? p.total_xp} XP`}
              action={<span className="chip bg-brand-50 text-brand-700 dark:bg-brand-500/10 dark:text-brand-300">{t('common.week')}</span>}
            />
            <div className="h-48 w-full">
              <ResponsiveContainer width="100%" height="100%">
                <BarChart data={chart} margin={{ top: 6, right: 4, left: 0, bottom: 0 }}>
                  <XAxis dataKey="day" tickLine={false} axisLine={false} tick={{ fontSize: 10, fill: '#94a3b8' }} />
                  <Tooltip
                    cursor={{ fill: 'rgba(79,70,229,.06)' }}
                    contentStyle={{ borderRadius: 12, border: '1px solid #e2e8f0', fontSize: 12 }}
                    formatter={(v: any) => [`${v} XP`, '']}
                  />
                  <Bar dataKey="xp" radius={[6, 6, 6, 6]} barSize={16}>
                    {chart.map((_: any, i: number) => (
                      <Cell key={i} fill={i === chart.length - 1 ? '#10b981' : '#6366f1'} />
                    ))}
                  </Bar>
                </BarChart>
              </ResponsiveContainer>
            </div>
          </section>
        </div>

        <div className="space-y-5 xl:col-span-4">
          <section className="card overflow-hidden">
            <div className="border-b border-line bg-emerald-50/60 p-4 dark:bg-emerald-500/5">
              <div className="label flex items-center gap-1.5 text-emerald-700 dark:text-emerald-300">
                <Icon name="auto_awesome" className="text-[14px]" fill /> {t('dash.ai_tip')}
              </div>
              <p className="mt-2 text-sm leading-relaxed">{d.tip?.text ?? '—'}</p>
              <Link to="/ai" className="btn-soft mt-3 w-full">
                {t('ai.title')} <Icon name="arrow_forward" />
              </Link>
            </div>
            <div className="p-4">
              <div className="flex items-center justify-between">
                <div className="label">{t('dash.quests')}</div>
                <span className="font-mono text-[10px] text-muted">
                  {d.quests?.daily_completed ?? 0}/{d.quests?.daily_total ?? 3}
                </span>
              </div>
              <div className="mt-3 space-y-2.5">
                {dailyQuests.map((q: any) => {
                  const done = q.progress >= q.target;
                  return (
                    <div key={q.id} className="flex items-center gap-3 rounded-xl border border-line p-2.5">
                      <span className={clsx('flex h-8 w-8 items-center justify-center rounded-lg', done ? 'bg-emerald-50 text-emerald-600' : 'bg-panel2 text-muted')}>
                        <Icon name={q.icon || 'task_alt'} className="text-[18px]" />
                      </span>
                      <div className="min-w-0 flex-1">
                        <div className="truncate text-xs font-semibold">{q.title}</div>
                        <div className="mt-1 flex items-center gap-2">
                          <Progress className="h-1" value={(q.progress / q.target) * 100} tone={done ? 'success' : 'brand'} />
                          <span className="font-mono text-[10px] text-muted">{q.progress}/{q.target}</span>
                        </div>
                      </div>
                      {q.status === 'completed' && (
                        <button className="btn-success px-2 py-1 text-[10px]" onClick={() => claim.mutate(q.id)} disabled={claim.isPending}>
                          {t('season.claim')}
                        </button>
                      )}
                      {q.status === 'claimed' && <Icon name="check_circle" className="text-success" />}
                    </div>
                  );
                })}
                {dailyQuests.length === 0 && <div className="py-4 text-center text-xs text-muted">{t('common.empty')}</div>}
              </div>
            </div>
          </section>

          <section className="card p-4">
            <SectionHeader
              title={t('dash.mini_board')}
              action={<Link to="/leaderboard" className="text-xs font-semibold text-brand-600 dark:text-brand-300">{t('common.view_all')}</Link>}
            />
            <div className="space-y-1">
              {(board.data?.entries ?? []).map((e: any) => (
                <div key={e.student_id} className={clsx('flex items-center gap-2.5 rounded-xl px-2 py-1.5', e.is_me && 'bg-brand-50 dark:bg-brand-500/10')}>
                  <span className="w-4 font-mono text-xs text-muted">{e.rank}</span>
                  <Avatar name={e.nickname} size={28} />
                  <div className="min-w-0 flex-1">
                    <div className="truncate text-xs font-semibold">{e.nickname}</div>
                    <div className="text-[10px] text-muted">{e.level} {t('common.level')}</div>
                  </div>
                  <span className="font-mono text-xs font-semibold">{Math.round(e.score)}</span>
                </div>
              ))}
            </div>
          </section>

          <section className="card p-4">
            <SectionHeader
              title={t('dash.recent_ach')}
              action={<Link to="/achievements" className="text-xs font-semibold text-brand-600 dark:text-brand-300">{t('common.view_all')}</Link>}
            />
            <div className="grid grid-cols-3 gap-2">
              {(d.recent_achievements ?? []).map((a: any) => (
                <div key={a.code} className="flex flex-col items-center gap-1 rounded-xl border border-line p-2 text-center">
                  <span className="flex h-9 w-9 items-center justify-center rounded-full bg-amber-50 text-amber-600 dark:bg-amber-500/10">
                    <Icon name={a.icon} fill />
                  </span>
                  <span className="line-clamp-2 text-[10px] font-medium">{a.title}</span>
                </div>
              ))}
              {(d.recent_achievements ?? []).length === 0 && (
                <div className="col-span-3 py-4 text-center text-xs text-muted">{t('common.empty')}</div>
              )}
            </div>
          </section>
        </div>
      </div>
    </div>
  );
}
