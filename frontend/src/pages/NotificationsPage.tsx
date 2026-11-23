import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { useTranslation } from 'react-i18next';
import clsx from 'clsx';
import { api } from '../api/client';
import { ErrorState, Icon, Spinner } from '../components/ui';

const KIND: Record<string, { icon: string; tone: string }> = {
  achievement: { icon: 'emoji_events', tone: 'bg-amber-50 text-amber-600 dark:bg-amber-500/10' },
  level_up: { icon: 'military_tech', tone: 'bg-brand-50 text-brand-600 dark:bg-brand-500/10' },
  season_reward: { icon: 'redeem', tone: 'bg-emerald-50 text-emerald-600 dark:bg-emerald-500/10' },
  season_ended: { icon: 'event', tone: 'bg-sky-50 text-sky-600 dark:bg-sky-500/10' },
  info: { icon: 'info', tone: 'bg-panel2 text-muted' },
};

export default function NotificationsPage() {
  const { t } = useTranslation();
  const queryClient = useQueryClient();
  const list = useQuery({ queryKey: ['notifications'], queryFn: () => api('/notifications', { params: { limit: 50 } }) });

  const readAll = useMutation({
    mutationFn: () => api('/notifications/read-all', { method: 'POST', body: {} }),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['notifications'] }),
  });
  const readOne = useMutation({
    mutationFn: (id: string) => api(`/notifications/${id}/read`, { method: 'POST', body: {} }),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['notifications'] }),
  });

  if (list.isLoading) return <Spinner />;
  if (list.error) return <ErrorState error={list.error} onRetry={() => list.refetch()} />;

  const items = list.data?.notifications ?? [];

  return (
    <div className="space-y-4">
      <section className="flex items-end justify-between">
        <div>
          <div className="label">{list.data?.unread ?? 0} unread</div>
          <h1 className="text-3xl font-extrabold tracking-tight">{t('notif.title')}</h1>
        </div>
        <button className="btn-ghost" onClick={() => readAll.mutate()} disabled={readAll.isPending || (list.data?.unread ?? 0) === 0}>
          <Icon name="done_all" /> {t('notif.read_all')}
        </button>
      </section>

      <section className="space-y-2">
        {items.map((n: any) => {
          const meta = KIND[n.kind] ?? KIND.info;
          return (
            <button
              key={n.id}
              onClick={() => !n.read_at && readOne.mutate(n.id)}
              className={clsx('card flex w-full items-start gap-3 p-4 text-left transition-colors', !n.read_at && 'border-brand-200 bg-brand-50/30 dark:border-brand-500/30 dark:bg-brand-500/5')}
            >
              <span className={clsx('flex h-10 w-10 shrink-0 items-center justify-center rounded-xl', meta.tone)}>
                <Icon name={meta.icon} fill />
              </span>
              <div className="min-w-0 flex-1">
                <div className="flex items-center gap-2">
                  <span className="text-sm font-semibold">{n.title}</span>
                  {!n.read_at && <span className="h-2 w-2 rounded-full bg-brand-600" />}
                </div>
                <div className="mt-0.5 text-xs text-muted">{n.body}</div>
                <div className="mt-1 font-mono text-[10px] text-muted">{new Date(n.created_at).toLocaleString()}</div>
              </div>
            </button>
          );
        })}
        {items.length === 0 && <div className="card p-10 text-center text-sm text-muted">{t('notif.empty')}</div>}
      </section>
    </div>
  );
}
