import { useState } from 'react';
import { useQuery } from '@tanstack/react-query';
import { useTranslation } from 'react-i18next';
import clsx from 'clsx';
import { api } from '../api/client';
import { Avatar, ErrorState, Icon, Spinner, Tabs } from '../components/ui';

export default function LeaderboardPage() {
  const { t } = useTranslation();
  const [scope, setScope] = useState('global');
  const [period, setPeriod] = useState<'all' | 'week'>('all');

  const seasons = useQuery({ queryKey: ['seasons', 'current'], queryFn: () => api('/seasons/current') });
  const courses = useQuery({ queryKey: ['courses'], queryFn: () => api('/courses') });
  const seasonId = seasons.data?.season?.id;
  const courseId = courses.data?.[0]?.id;

  const board = useQuery({
    queryKey: ['leaderboard', scope, period, seasonId, courseId],
    queryFn: () =>
      api('/leaderboards', {
        params: {
          scope,
          period: scope === 'weekly' ? 'week' : period,
          season_id: scope === 'season' ? seasonId : undefined,
          course_id: scope === 'course' ? courseId : undefined,
          limit: 50,
        },
      }),
    enabled: scope !== 'season' || !!seasonId,
  });

  const entries = board.data?.entries ?? [];
  const me = board.data?.me;
  const inList = entries.some((e: any) => e.is_me);
  const podium = entries.slice(0, 3);

  const tabs = [
    { id: 'global', label: t('lb.global'), icon: 'public' },
    { id: 'weekly', label: t('lb.weekly'), icon: 'date_range' },
    { id: 'course', label: t('lb.course'), icon: 'auto_stories' },
    { id: 'season', label: t('lb.season'), icon: 'calendar_month' },
    { id: 'team', label: t('lb.team'), icon: 'groups' },
  ];

  return (
    <div className="space-y-5">
      <section className="flex flex-col gap-3 lg:flex-row lg:items-end lg:justify-between">
        <div>
          <div className="label">LEAGUE · TOP {entries.length}</div>
          <h1 className="text-3xl font-extrabold tracking-tight">{t('nav.leaderboard')}</h1>
          <p className="mt-1 text-sm text-muted">
            {seasons.data?.season?.title ? `${t('season.title')}: ${seasons.data.season.title}` : t('dash.mini_board')}
          </p>
        </div>
        <Tabs tabs={tabs} value={scope} onChange={setScope} />
      </section>

      {me && (
        <section className="card flex flex-wrap items-center gap-4 p-4">
          <span className="flex h-11 w-11 items-center justify-center rounded-xl bg-brand-50 text-brand-600 dark:bg-brand-500/10 dark:text-brand-300">
            <Icon name="person_pin" />
          </span>
          <div>
            <div className="label">{t('lb.your_place')}</div>
            <div className="font-mono text-xl font-bold">#{me.rank}</div>
          </div>
          <div className="ml-auto text-right">
            <div className="label">{t('common.xp')}</div>
            <div className="font-mono text-xl font-bold text-brand-600 dark:text-brand-300">{Math.round(me.score)}</div>
          </div>
          {!inList && <span className="chip bg-amber-50 text-amber-700 dark:bg-amber-500/10 dark:text-amber-300">{t('common.you')}</span>}
        </section>
      )}

      {board.isLoading && <Spinner />}
      {board.error && <ErrorState error={board.error} onRetry={() => board.refetch()} />}

      {podium.length === 3 && (
        <section className="grid grid-cols-1 gap-3 sm:grid-cols-3">
          {[podium[1], podium[0], podium[2]].map((e: any, i: number) => (
            <div
              key={e.student_id}
              className={clsx(
                'card flex flex-col items-center gap-2 p-5 text-center',
                i === 1 && 'border-amber-300 ring-2 ring-amber-200/50 dark:border-amber-500/40',
                e.is_me && 'bg-brand-50/40 dark:bg-brand-500/5',
              )}
            >
              <div className="relative">
                <Avatar name={e.nickname} size={i === 1 ? 64 : 52} />
                <span
                  className={clsx(
                    'absolute -bottom-1 -right-1 flex h-6 w-6 items-center justify-center rounded-full text-[11px] font-bold text-white',
                    i === 1 ? 'bg-amber' : i === 0 ? 'bg-slate-400' : 'bg-orange-400',
                  )}
                >
                  {e.rank}
                </span>
              </div>
              <div className="font-semibold">{e.nickname}</div>
              <div className="text-[11px] text-muted">{e.level} {t('common.level')} · {e.title}</div>
              <div className="font-mono text-lg font-bold">{Math.round(e.score)} <span className="text-[10px] text-muted">XP</span></div>
            </div>
          ))}
        </section>
      )}

      <section className="card overflow-hidden">
        <div className="hidden grid-cols-[60px,1fr,90px,90px,90px] gap-3 border-b border-line px-4 py-2.5 text-[10px] uppercase tracking-wider text-muted sm:grid">
          <span>{t('common.rank')}</span>
          <span>{t('auth.nickname')}</span>
          <span className="text-center">{t('common.level')}</span>
          <span className="text-right">{t('common.xp')}</span>
          <span className="text-right">Δ</span>
        </div>
        {entries.map((e: any) => (
          <div
            key={e.student_id}
            className={clsx(
              'grid grid-cols-[44px,1fr,80px] items-center gap-3 border-b border-line/60 px-4 py-2.5 last:border-0 sm:grid-cols-[60px,1fr,90px,90px,90px]',
              e.is_me && 'bg-brand-50/60 dark:bg-brand-500/5',
            )}
          >
            <span className="font-mono text-sm font-semibold text-muted">{e.rank}</span>
            <div className="flex min-w-0 items-center gap-2.5">
              <Avatar name={e.nickname} size={30} />
              <div className="min-w-0">
                <div className="truncate text-sm font-semibold">
                  {e.nickname} {e.is_me && <span className="chip bg-brand-100 text-[9px] text-brand-700 dark:bg-brand-500/20 dark:text-brand-300">{t('common.you')}</span>}
                </div>
                <div className="text-[10px] text-muted">{e.title} · {e.league}</div>
              </div>
            </div>
            <span className="hidden text-center font-mono text-xs sm:block">{e.level}</span>
            <span className="text-right font-mono text-sm font-semibold">{Math.round(e.score)}</span>
            <span
              className={clsx(
                'hidden items-center justify-end gap-0.5 text-xs font-semibold sm:flex',
                e.delta > 0 ? 'text-success' : e.delta < 0 ? 'text-red-500' : 'text-muted',
              )}
            >
              {e.delta > 0 ? <Icon name="arrow_upward" className="text-[13px]" /> : e.delta < 0 ? <Icon name="arrow_downward" className="text-[13px]" /> : null}
              {e.delta !== 0 ? Math.abs(e.delta) : '—'}
            </span>
          </div>
        ))}
        {!board.isLoading && entries.length === 0 && <div className="p-10 text-center text-sm text-muted">{t('common.empty')}</div>}
      </section>
    </div>
  );
}
