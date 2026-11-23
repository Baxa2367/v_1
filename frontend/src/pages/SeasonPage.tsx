import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { useTranslation } from 'react-i18next';
import clsx from 'clsx';
import { api } from '../api/client';
import { Avatar, ErrorState, Icon, Progress, SectionHeader, Spinner, useToasts } from '../components/ui';

export default function SeasonPage() {
  const { t } = useTranslation();
  const queryClient = useQueryClient();
  const push = useToasts((s) => s.push);
  const season = useQuery({ queryKey: ['season', 'current'], queryFn: () => api('/seasons/current') });

  const claim = useMutation({
    mutationFn: (tier: number) => api(`/seasons/${season.data.season.id}/claim/${tier}`, { method: 'POST', body: {} }),
    onSuccess: () => {
      push({ title: t('season.claimed'), icon: 'redeem', tone: 'success' });
      queryClient.invalidateQueries({ queryKey: ['season'] });
      queryClient.invalidateQueries({ queryKey: ['shop'] });
    },
    onError: (e: any) => push({ title: e.message, icon: 'error', tone: 'danger' }),
  });

  if (season.isLoading) return <Spinner />;
  if (season.error) return <ErrorState error={season.error} onRetry={() => season.refetch()} />;
  if (!season.data?.active) {
    return <div className="card p-10 text-center text-muted">{t('common.empty')}</div>;
  }

  const s = season.data;
  const xp = s.progress?.season_xp ?? 0;
  const tier = s.progress?.tier ?? 0;
  const claimed: number[] = s.progress?.claimed_tiers ?? [];
  const nextReward = (s.rewards ?? []).find((r: any) => r.tier === tier + 1);
  const pct = nextReward ? Math.min(100, (xp / nextReward.required_xp) * 100) : 100;

  return (
    <div className="space-y-5">
      <section className="relative overflow-hidden rounded-2xl hero-gradient p-6 text-white lg:p-8">
        <div className="absolute -right-10 -bottom-20 h-72 w-72 rounded-full bg-amber-300/20 blur-3xl" />
        <div className="relative flex flex-wrap items-start justify-between gap-5">
          <div>
            <div className="flex items-center gap-2">
              <span className="chip bg-white/15 uppercase tracking-wider text-white">{t('season.title')}</span>
              <span className="chip bg-amber-300 text-amber-950">{s.days_left} {t('season.days_left')}</span>
            </div>
            <h1 className="mt-2 text-3xl font-extrabold tracking-tight">{s.season.title}</h1>
            <p className="mt-1 max-w-xl text-sm text-white/75">{s.season.description}</p>
            <div className="mt-4 flex flex-wrap items-center gap-5 font-mono text-[11px] text-white/80">
              <span>{xp} XP</span>
              <span>{t('season.tier')} {tier}</span>
              <span>#{s.rank}</span>
            </div>
          </div>
          <div className="rounded-2xl bg-white/10 p-4 backdrop-blur">
            <div className="label text-white/70">{t('shop.balance')}</div>
            <div className="font-mono text-2xl font-bold">{xp} XP</div>
            {nextReward && (
              <div className="mt-2 w-48">
                <div className="flex items-center justify-between font-mono text-[10px] text-white/70">
                  <span>→ {t('season.tier')} {nextReward.tier}</span>
                  <span>{nextReward.required_xp}</span>
                </div>
                <div className="mt-1 h-2 overflow-hidden rounded-full bg-white/20">
                  <div className="h-full rounded-full bg-amber-300" style={{ width: `${pct}%` }} />
                </div>
              </div>
            )}
          </div>
        </div>
      </section>

      <div className="grid grid-cols-1 gap-5 xl:grid-cols-12">
        <section className="card p-5 xl:col-span-8">
          <SectionHeader title={t('season.rewards')} subtitle={`${(s.rewards ?? []).length} tiers`} />
          <div className="space-y-2">
            {(s.rewards ?? []).map((r: any) => {
              const unlocked = tier >= r.tier;
              const isClaimed = claimed.includes(r.tier);
              return (
                <div
                  key={r.id}
                  className={clsx(
                    'flex items-center gap-3 rounded-xl border p-3',
                    unlocked ? 'border-amber-200 bg-amber-50/50 dark:border-amber-500/30 dark:bg-amber-500/5' : 'border-line',
                  )}
                >
                  <span className={clsx('flex h-10 w-10 items-center justify-center rounded-xl', unlocked ? 'bg-amber-100 text-amber-700 dark:bg-amber-500/20 dark:text-amber-300' : 'bg-panel2 text-muted')}>
                    <Icon name={unlocked ? 'card_giftcard' : 'lock'} />
                  </span>
                  <div className="min-w-0 flex-1">
                    <div className="flex items-center gap-2">
                      <span className="text-sm font-semibold">{r.title}</span>
                      <span className="chip bg-panel2 text-[9px] text-muted">{r.required_xp} XP</span>
                    </div>
                    <div className="mt-1 flex gap-3 font-mono text-[10px] text-muted">
                      {r.payload?.coins ? <span>+{r.payload.coins} coins</span> : null}
                      {r.payload?.xp ? <span>+{r.payload.xp} XP</span> : null}
                    </div>
                  </div>
                  {isClaimed ? (
                    <span className="chip bg-emerald-50 text-emerald-700 dark:bg-emerald-500/10 dark:text-emerald-300">{t('season.claimed')}</span>
                  ) : unlocked ? (
                    <button className="btn-success px-3 py-1.5 text-xs" onClick={() => claim.mutate(r.tier)} disabled={claim.isPending}>
                      {t('season.claim')}
                    </button>
                  ) : (
                    <Icon name="lock" className="text-muted" />
                  )}
                </div>
              );
            })}
          </div>
        </section>

        <section className="card p-5 xl:col-span-4">
          <SectionHeader title={t('season.top')} />
          <div className="space-y-2">
            {(s.top ?? []).map((e: any) => (
              <div key={e.student_id} className="flex items-center gap-2.5 rounded-xl px-2 py-1.5">
                <span className="w-4 font-mono text-xs text-muted">{e.rank}</span>
                <Avatar name={e.nickname} size={28} />
                <div className="min-w-0 flex-1">
                  <div className="truncate text-xs font-semibold">{e.nickname}</div>
                  <div className="text-[10px] text-muted">{e.level} {t('common.level')}</div>
                </div>
                <span className="font-mono text-xs font-semibold">{Math.round(e.score)}</span>
              </div>
            ))}
            {(s.top ?? []).length === 0 && <div className="py-6 text-center text-xs text-muted">{t('common.empty')}</div>}
          </div>
        </section>
      </div>
    </div>
  );
}
