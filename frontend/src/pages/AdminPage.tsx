import { useState } from 'react';
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { useTranslation } from 'react-i18next';
import clsx from 'clsx';
import { api } from '../api/client';
import { ErrorState, Icon, Spinner, Tabs, useToasts } from '../components/ui';

export default function AdminPage() {
  const { t } = useTranslation();
  const queryClient = useQueryClient();
  const push = useToasts((s) => s.push);
  const [tab, setTab] = useState('metrics');

  const metrics = useQuery({ queryKey: ['admin', 'metrics'], queryFn: () => api('/admin/metrics') });
  const users = useQuery({ queryKey: ['admin', 'users'], queryFn: () => api('/admin/users', { params: { limit: 60 } }), enabled: tab === 'users' });
  const flags = useQuery({ queryKey: ['admin', 'anticheat'], queryFn: () => api('/admin/anticheat', { params: { status: 'open' } }), enabled: tab === 'anticheat' });
  const seasons = useQuery({ queryKey: ['admin', 'seasons'], queryFn: () => api('/admin/seasons'), enabled: tab === 'seasons' });

  const updateUser = useMutation({
    mutationFn: ({ id, body }: { id: string; body: any }) => api(`/admin/users/${id}`, { method: 'PATCH', body }),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['admin', 'users'] }),
    onError: (e: any) => push({ title: e.message, icon: 'error', tone: 'danger' }),
  });

  const resolve = useMutation({
    mutationFn: (id: string) => api(`/admin/anticheat/${id}`, { method: 'PATCH', body: { status: 'reviewed' } }),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['admin', 'anticheat'] }),
  });

  const closeSeason = useMutation({
    mutationFn: (id: string) => api(`/admin/seasons/${id}/close`, { method: 'POST', body: {} }),
    onSuccess: () => {
      push({ title: 'Season closed', icon: 'event_available', tone: 'success' });
      queryClient.invalidateQueries({ queryKey: ['admin', 'seasons'] });
      queryClient.invalidateQueries({ queryKey: ['admin', 'metrics'] });
    },
  });

  if (metrics.isLoading) return <Spinner />;
  if (metrics.error) return <ErrorState error={metrics.error} onRetry={() => metrics.refetch()} />;

  const m = metrics.data;

  return (
    <div className="space-y-5">
      <section className="flex flex-col gap-3 sm:flex-row sm:items-end sm:justify-between">
        <div>
          <div className="label">ADMIN · SYSTEM</div>
          <h1 className="text-3xl font-extrabold tracking-tight">{t('admin.title')}</h1>
        </div>
        <Tabs
          tabs={[
            { id: 'metrics', label: t('admin.metrics'), icon: 'monitoring' },
            { id: 'users', label: t('admin.users'), icon: 'group' },
            { id: 'anticheat', label: t('admin.anticheat'), icon: 'shield' },
            { id: 'seasons', label: t('admin.seasons'), icon: 'calendar_month' },
          ]}
          value={tab}
          onChange={setTab}
        />
      </section>

      {tab === 'metrics' && (
        <>
          <section className="grid grid-cols-2 gap-3 lg:grid-cols-4">
            {[
              ['group', t('admin.users'), m.users, 'brand'],
              ['school', t('nav.courses'), m.courses, 'sky'],
              ['task_alt', t('common.tasks'), m.tasks, 'success'],
              ['bolt', `XP ${t('common.today')}`, m.xp_today, 'amber'],
              ['insights', 'Attempts', m.attempts, 'brand'],
              ['local_fire_department', 'Active 7d', m.active_7d, 'amber'],
              ['shield', t('admin.open_flags'), m.open_flags, 'success'],
              ['calendar_month', t('nav.season'), m.season?.status ?? '—', 'sky'],
            ].map(([icon, label, value, tone]: any) => (
              <div key={label} className="card p-4">
                <div className="flex items-start justify-between">
                  <div>
                    <div className="label">{label}</div>
                    <div className="stat-value mt-1 text-xl">{value}</div>
                  </div>
                  <span className={clsx('flex h-9 w-9 items-center justify-center rounded-xl', tone === 'amber' ? 'bg-amber-50 text-amber-600' : tone === 'sky' ? 'bg-sky-50 text-sky-600' : tone === 'success' ? 'bg-emerald-50 text-emerald-600' : 'bg-brand-50 text-brand-600')}>
                    <Icon name={icon} />
                  </span>
                </div>
              </div>
            ))}
          </section>
          <section className="card p-5">
            <div className="label mb-2">SEASON</div>
            <div className="text-lg font-bold">{m.season?.title?.kk ?? m.season?.title ?? '—'}</div>
            <div className="mt-1 font-mono text-xs text-muted">{m.season?.code} · {m.season?.status}</div>
          </section>
        </>
      )}

      {tab === 'users' && (
        <section className="card overflow-hidden">
          {users.isLoading && <Spinner />}
          <table className="w-full text-left text-sm">
            <thead className="bg-panel2 text-[10px] uppercase tracking-wider text-muted">
              <tr>
                <th className="px-4 py-2.5 font-medium">Email</th>
                <th className="px-3 py-2.5 font-medium">{t('auth.nickname')}</th>
                <th className="px-3 py-2.5 font-medium">{t('admin.role')}</th>
                <th className="px-3 py-2.5 font-medium">{t('common.level')}</th>
                <th className="px-3 py-2.5 font-medium">{t('admin.status')}</th>
              </tr>
            </thead>
            <tbody>
              {(users.data?.users ?? []).map((u: any) => (
                <tr key={u.id} className="border-t border-line/60">
                  <td className="px-4 py-2.5">
                    <div className="text-xs font-medium">{u.email}</div>
                    <div className="text-[10px] text-muted">{u.display_name}</div>
                  </td>
                  <td className="px-3 py-2.5 text-xs">{u.nickname ?? '—'}</td>
                  <td className="px-3 py-2.5">
                    <select
                      className="rounded-lg border border-line bg-panel px-2 py-1 text-xs"
                      value={u.role}
                      onChange={(e) => updateUser.mutate({ id: u.id, body: { role: e.target.value } })}
                    >
                      <option value="student">student</option>
                      <option value="teacher">teacher</option>
                      <option value="admin">admin</option>
                    </select>
                  </td>
                  <td className="px-3 py-2.5 font-mono text-xs">{u.level ?? '—'}</td>
                  <td className="px-3 py-2.5">
                    <button
                      className={clsx('chip text-[10px]', u.status === 'active' ? 'bg-emerald-50 text-emerald-700 dark:bg-emerald-500/10 dark:text-emerald-300' : 'bg-red-50 text-red-600 dark:bg-red-500/10')}
                      onClick={() => updateUser.mutate({ id: u.id, body: { status: u.status === 'active' ? 'blocked' : 'active' } })}
                    >
                      {u.status}
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </section>
      )}

      {tab === 'anticheat' && (
        <section className="space-y-2">
          {flags.isLoading && <Spinner />}
          {(flags.data?.flags ?? []).map((f: any) => (
            <div key={f.id} className="card flex flex-wrap items-center gap-3 p-4">
              <span className="flex h-10 w-10 items-center justify-center rounded-xl bg-red-50 text-red-600 dark:bg-red-500/10">
                <Icon name="shield" />
              </span>
              <div className="min-w-0 flex-1">
                <div className="text-sm font-semibold">{f.nickname} · {f.kind}</div>
                <div className="truncate text-xs text-muted">{f.prompt}</div>
                <div className="mt-0.5 font-mono text-[10px] text-muted">{new Date(f.created_at).toLocaleString()} · severity {f.severity}</div>
              </div>
              <button className="btn-ghost text-xs" onClick={() => resolve.mutate(f.id)}>
                {t('admin.resolve')}
              </button>
            </div>
          ))}
          {(flags.data?.flags ?? []).length === 0 && <div className="card p-10 text-center text-sm text-muted">{t('common.empty')}</div>}
        </section>
      )}

      {tab === 'seasons' && (
        <section className="space-y-2">
          {seasons.isLoading && <Spinner />}
          {(seasons.data ?? []).map((s: any) => (
            <div key={s.id} className="card flex flex-wrap items-center gap-3 p-4">
              <span className="flex h-10 w-10 items-center justify-center rounded-xl bg-brand-50 text-brand-600 dark:bg-brand-500/10">
                <Icon name="calendar_month" />
              </span>
              <div className="min-w-0 flex-1">
                <div className="text-sm font-semibold">{s.title}</div>
                <div className="font-mono text-[10px] text-muted">
                  {s.code} · {new Date(s.starts_at).toLocaleDateString()} → {new Date(s.ends_at).toLocaleDateString()}
                </div>
              </div>
              <span className={clsx('chip text-[10px]', s.status === 'active' ? 'bg-emerald-50 text-emerald-700' : 'bg-panel2 text-muted')}>{s.status}</span>
              {s.status === 'active' && (
                <button className="btn-ghost text-xs" onClick={() => closeSeason.mutate(s.id)} disabled={closeSeason.isPending}>
                  {t('common.close')}
                </button>
              )}
            </div>
          ))}
        </section>
      )}
    </div>
  );
}
