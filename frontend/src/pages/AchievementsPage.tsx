import { useMemo, useState } from 'react';
import { useQuery } from '@tanstack/react-query';
import { useTranslation } from 'react-i18next';
import clsx from 'clsx';
import { api } from '../api/client';
import { ErrorState, Icon, Progress, SectionHeader, Spinner, Tabs } from '../components/ui';

const RARITY: Record<string, string> = {
  common: 'bg-slate-100 text-slate-600 dark:bg-slate-500/20 dark:text-slate-300',
  rare: 'bg-sky-100 text-sky-700 dark:bg-sky-500/20 dark:text-sky-300',
  epic: 'bg-violet-100 text-violet-700 dark:bg-violet-500/20 dark:text-violet-300',
  legendary: 'bg-amber-100 text-amber-700 dark:bg-amber-500/20 dark:text-amber-300',
};

const CATEGORIES = [
  { id: '', label: 'Барлығы', icon: 'apps' },
  { id: 'start', label: 'Старт', icon: 'flag' },
  { id: 'progress', label: 'Прогресс', icon: 'trending_up' },
  { id: 'accuracy', label: 'Дәлдік', icon: 'target' },
  { id: 'speed', label: 'Жылдамдық', icon: 'speed' },
  { id: 'streaks', label: 'Сериялар', icon: 'local_fire_department' },
  { id: 'social', label: 'Әлеуметтік', icon: 'groups' },
  { id: 'seasonal', label: 'Маусым', icon: 'calendar_month' },
  { id: 'mastery', label: 'Шеберлік', icon: 'psychology' },
];

export default function AchievementsPage() {
  const { t } = useTranslation();
  const [category, setCategory] = useState('');
  const [rarity, setRarity] = useState('');
  const collection = useQuery({ queryKey: ['achievements'], queryFn: () => api('/me/achievements') });

  const items = useMemo(() => {
    const all = collection.data?.achievements ?? [];
    return all.filter((a: any) => (!category || a.category === category) && (!rarity || a.rarity === rarity));
  }, [collection.data, category, rarity]);

  if (collection.isLoading) return <Spinner />;
  if (collection.error) return <ErrorState error={collection.error} onRetry={() => collection.refetch()} />;

  const unlocked = collection.data?.unlocked ?? 0;
  const total = collection.data?.total ?? 0;

  return (
    <div className="space-y-5">
      <section className="flex flex-col gap-3 lg:flex-row lg:items-end lg:justify-between">
        <div>
          <div className="label">{unlocked}/{total} · COLLECTION</div>
          <h1 className="text-3xl font-extrabold tracking-tight">{t('ach.title')}</h1>
          <p className="mt-1 text-sm text-muted">{t('auth.feature3')}</p>
        </div>
        <div className="flex items-center gap-3">
          <div className="w-40">
            <Progress value={(unlocked / Math.max(1, total)) * 100} tone="amber" />
          </div>
          <span className="font-mono text-sm font-semibold text-amber-600">{Math.round((unlocked / Math.max(1, total)) * 100)}%</span>
        </div>
      </section>

      <div className="flex flex-col gap-3 lg:flex-row lg:items-center lg:justify-between">
        <div className="flex flex-wrap gap-1 rounded-xl bg-panel2 p-1">
          {CATEGORIES.map((c) => (
            <button
              key={c.id}
              onClick={() => setCategory(c.id)}
              className={clsx('flex items-center gap-1 rounded-lg px-2.5 py-1.5 text-xs font-medium', category === c.id ? 'bg-panel shadow-sm' : 'text-muted hover:text-ink')}
            >
              <Icon name={c.icon} className="text-[15px]" /> {c.label}
            </button>
          ))}
        </div>
        <Tabs
          tabs={[
            { id: '', label: t('common.all') },
            { id: 'common', label: t('ach.rarity.common') },
            { id: 'rare', label: t('ach.rarity.rare') },
            { id: 'epic', label: t('ach.rarity.epic') },
            { id: 'legendary', label: t('ach.rarity.legendary') },
          ]}
          value={rarity}
          onChange={setRarity}
        />
      </div>

      <section className="grid grid-cols-1 gap-3 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4">
        {items.map((a: any) => (
          <div
            key={a.id}
            className={clsx('card relative overflow-hidden p-4 transition-all', a.awarded ? 'border-amber-200 dark:border-amber-500/30' : 'opacity-90 hover:opacity-100')}
          >
            <div className="flex items-start gap-3">
              <span
                className={clsx(
                  'flex h-12 w-12 shrink-0 items-center justify-center rounded-2xl',
                  a.awarded ? 'bg-amber-50 text-amber-600 dark:bg-amber-500/10 dark:text-amber-300' : 'bg-panel2 text-muted',
                )}
              >
                <Icon name={a.icon} className="text-[26px]" fill={a.awarded} />
              </span>
              <div className="min-w-0 flex-1">
                <div className="flex items-center gap-1.5">
                  <span className="truncate text-sm font-bold">{a.title}</span>
                  {a.awarded && <Icon name="verified" className="text-amber text-[15px]" fill />}
                </div>
                <span className={clsx('chip mt-1 text-[9px] uppercase', RARITY[a.rarity])}>{t(`ach.rarity.${a.rarity}`)}</span>
              </div>
            </div>
            <p className="mt-2 line-clamp-2 text-xs leading-relaxed text-muted">{a.description}</p>
            <div className="mt-3">
              <div className="mb-1 flex items-center justify-between font-mono text-[10px] text-muted">
                <span>{a.awarded ? t('common.completed') : `${Math.round((a.progress ?? 0) * 100)}%`}</span>
                <span>{a.awarded ? '✓' : `${a.progress_value ?? 0}/${a.target_value ?? 1}`}</span>
              </div>
              <Progress value={a.awarded ? 100 : (a.progress ?? 0) * 100} tone={a.awarded ? 'amber' : 'brand'} />
            </div>
            {a.reward_coins > 0 && (
              <div className="mt-2 flex items-center gap-1 font-mono text-[10px] text-amber-600">
                <Icon name="monetization_on" className="text-[12px]" /> {a.reward_coins}
              </div>
            )}
          </div>
        ))}
      </section>
      {items.length === 0 && <div className="card p-10 text-center text-sm text-muted">{t('common.empty')}</div>}
    </div>
  );
}
