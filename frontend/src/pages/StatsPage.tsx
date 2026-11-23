import { useState } from 'react';
import { useQuery } from '@tanstack/react-query';
import { useTranslation } from 'react-i18next';
import {
  Area, AreaChart, CartesianGrid, PolarAngleAxis, PolarGrid, Radar, RadarChart, ResponsiveContainer, Tooltip, XAxis, YAxis,
} from 'recharts';
import clsx from 'clsx';
import { api, download } from '../api/client';
import { download as _unused } from '../api/client';
import { ErrorState, Icon, Progress, SectionHeader, Spinner, Tabs } from '../components/ui';

function heatClass(xp: number) {
  if (xp <= 0) return 'heat-0';
  if (xp < 30) return 'heat-1';
  if (xp < 80) return 'heat-2';
  if (xp < 150) return 'heat-3';
  return 'heat-4';
}

export default function StatsPage() {
  const { t } = useTranslation();
  const [days, setDays] = useState(30);
  const [exporting, setExporting] = useState<string | null>(null);

  const progress = useQuery({ queryKey: ['reports', 'progress'], queryFn: () => api('/reports/progress') });
  const xp = useQuery({ queryKey: ['reports', 'xp', days], queryFn: () => api('/reports/xp', { params: { days } }) });
  const topics = useQuery({ queryKey: ['reports', 'topics'], queryFn: () => api('/reports/topics') });
  const heatmap = useQuery({ queryKey: ['reports', 'heatmap'], queryFn: () => api('/reports/heatmap', { params: { days: 120 } }) });
  const achievements = useQuery({ queryKey: ['reports', 'achievements'], queryFn: () => api('/reports/achievements') });

  if (progress.isLoading || xp.isLoading) return <Spinner />;
  if (progress.error || xp.error) return <ErrorState error={progress.error ?? xp.error} />;

  const p = progress.data;
  const x = xp.data;
  const daily = (x.daily ?? []).map((d: any) => ({ day: d.day.slice(5), xp: Number(d.xp) }));
  const topicData = (topics.data?.topics ?? []).slice(0, 8).map((tp: any) => ({
    topic: tp.name.length > 14 ? tp.name.slice(0, 14) + '…' : tp.name,
    mastery: Math.round((tp.mastery ?? 0) * 100),
    accuracy: Math.round(tp.accuracy ?? 0),
  }));
  const heat = heatmap.data?.days ?? [];
  const hours = (p.total_time_ms ?? 0) / 3_600_000;

  const exportReport = async (type: string, format: string) => {
    setExporting(`${type}-${format}`);
    try {
      await download('/reports/export', { type, format });
    } finally {
      setExporting(null);
    }
  };

  return (
    <div className="space-y-5">
      <section className="flex flex-col gap-3 lg:flex-row lg:items-end lg:justify-between">
        <div>
          <div className="label">REPORTS · {x.progress?.total_xp ?? 0} XP</div>
          <h1 className="text-3xl font-extrabold tracking-tight">{t('stats.title')}</h1>
        </div>
        <div className="flex flex-wrap items-center gap-2">
          <Tabs
            tabs={[
              { id: '7', label: '7d' },
              { id: '30', label: '30d' },
              { id: '90', label: '90d' },
            ]}
            value={String(days)}
            onChange={(v) => setDays(Number(v))}
          />
          {['csv', 'xlsx', 'pdf'].map((f) => (
            <button key={f} className="btn-ghost px-3 py-1.5 text-xs uppercase" onClick={() => exportReport('xp', f)} disabled={exporting !== null}>
              <Icon name="download" className="text-[16px]" /> {f}
            </button>
          ))}
        </div>
      </section>

      <section className="grid grid-cols-2 gap-3 lg:grid-cols-4">
        {[
          ['bolt', t('common.xp'), x.progress?.total_xp ?? 0, `${x.progress?.level ?? 1} ${t('common.level')}`, 'brand'],
          ['local_fire_department', t('common.streak'), `${x.progress?.today ?? 0} XP`, t('common.today'), 'amber'],
          ['schedule', t('stats.hours'), `${hours.toFixed(1)}h`, t('stats.levels_done'), 'sky'],
          ['emoji_events', t('stats.achievements'), `${achievements.data?.unlocked ?? 0}/${achievements.data?.total ?? 0}`, '', 'success'],
        ].map(([icon, label, value, sub, tone]: any) => (
          <div key={label} className="card p-4">
            <div className="flex items-start justify-between">
              <div>
                <div className="label">{label}</div>
                <div className="stat-value mt-1">{value}</div>
                {sub && <div className="mt-0.5 text-[11px] text-muted">{sub}</div>}
              </div>
              <span className={clsx('flex h-9 w-9 items-center justify-center rounded-xl', tone === 'amber' ? 'bg-amber-50 text-amber-600' : tone === 'sky' ? 'bg-sky-50 text-sky-600' : tone === 'success' ? 'bg-emerald-50 text-emerald-600' : 'bg-brand-50 text-brand-600')}>
                <Icon name={icon} />
              </span>
            </div>
          </div>
        ))}
      </section>

      <div className="grid grid-cols-1 gap-5 xl:grid-cols-12">
        <section className="card p-5 xl:col-span-8">
          <SectionHeader title={t('stats.xp_dynamics')} subtitle={`${days} days`} />
          <div className="h-64">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={daily} margin={{ top: 10, right: 10, left: -10, bottom: 0 }}>
                <defs>
                  <linearGradient id="xpFill" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="0%" stopColor="#4f46e5" stopOpacity={0.35} />
                    <stop offset="100%" stopColor="#4f46e5" stopOpacity={0} />
                  </linearGradient>
                </defs>
                <CartesianGrid strokeDasharray="3 3" stroke="rgba(148,163,184,.18)" vertical={false} />
                <XAxis dataKey="day" tickLine={false} axisLine={false} tick={{ fontSize: 10, fill: '#94a3b8' }} />
                <YAxis tickLine={false} axisLine={false} tick={{ fontSize: 10, fill: '#94a3b8' }} width={36} />
                <Tooltip contentStyle={{ borderRadius: 12, border: '1px solid #e2e8f0', fontSize: 12 }} />
                <Area type="monotone" dataKey="xp" stroke="#4f46e5" strokeWidth={2.5} fill="url(#xpFill)" />
              </AreaChart>
            </ResponsiveContainer>
          </div>
        </section>

        <section className="card p-5 xl:col-span-4">
          <SectionHeader title={t('stats.topics')} subtitle="mastery" />
          <div className="h-64">
            <ResponsiveContainer width="100%" height="100%">
              <RadarChart data={topicData} outerRadius="72%">
                <PolarGrid stroke="rgba(148,163,184,.25)" />
                <PolarAngleAxis dataKey="topic" tick={{ fontSize: 9, fill: '#94a3b8' }} />
                <Radar dataKey="mastery" stroke="#10b981" fill="#10b981" fillOpacity={0.25} />
                <Radar dataKey="accuracy" stroke="#4f46e5" fill="#4f46e5" fillOpacity={0.12} />
                <Tooltip contentStyle={{ borderRadius: 12, border: '1px solid #e2e8f0', fontSize: 12 }} />
              </RadarChart>
            </ResponsiveContainer>
          </div>
        </section>

        <section className="card p-5 xl:col-span-5">
          <SectionHeader title={t('stats.progress')} />
          <div className="space-y-3">
            {(p.courses ?? []).map((c: any) => (
              <div key={c.id}>
                <div className="flex items-center justify-between text-xs">
                  <span className="truncate pr-2 font-medium">{c.title}</span>
                  <span className="font-mono text-muted">{Math.round(c.progress_pct ?? 0)}%</span>
                </div>
                <Progress className="mt-1.5" value={c.progress_pct ?? 0} />
                <div className="mt-1 flex gap-3 font-mono text-[10px] text-muted">
                  <span>{c.levels_done}/{c.levels_total} {t('course.levels').toLowerCase()}</span>
                  <span>★ {c.stars}</span>
                </div>
              </div>
            ))}
          </div>
        </section>

        <section className="card p-5 xl:col-span-7">
          <SectionHeader title={t('stats.activity')} subtitle={`${heat.length} days`} />
          <div className="flex flex-wrap gap-1">
            {heat.map((d: any) => (
              <div
                key={d.day}
                title={`${d.day}: ${d.xp} XP`}
                className={clsx('h-3.5 w-3.5 rounded-[3px]', heatClass(Number(d.xp)))}
              />
            ))}
          </div>
          <div className="mt-3 flex items-center gap-2 text-[10px] text-muted">
            <span>Less</span>
            {[0, 1, 2, 3, 4].map((i) => <span key={i} className={clsx('h-3 w-3 rounded-[3px]', `heat-${i}`)} />)}
            <span>More</span>
          </div>
        </section>

        <section className="card p-5 xl:col-span-5">
          <SectionHeader title={t('stats.weak')} />
          <div className="space-y-2">
            {(topics.data?.topics ?? []).slice(0, 5).map((tp: any) => (
              <div key={tp.code} className="flex items-center gap-3 rounded-xl border border-line p-2.5">
                <span className="flex h-8 w-8 items-center justify-center rounded-lg bg-panel2 text-muted">
                  <Icon name="psychology" className="text-[18px]" />
                </span>
                <div className="min-w-0 flex-1">
                  <div className="truncate text-xs font-semibold">{tp.name}</div>
                  <div className="mt-1 flex items-center gap-2">
                    <Progress className="h-1" value={(tp.mastery ?? 0) * 100} tone={tp.mastery > 0.7 ? 'success' : 'amber'} />
                    <span className="font-mono text-[10px] text-muted">{Math.round((tp.mastery ?? 0) * 100)}%</span>
                  </div>
                </div>
              </div>
            ))}
          </div>
        </section>
      </div>
    </div>
  );
}
