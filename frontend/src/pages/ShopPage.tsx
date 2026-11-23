import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { useTranslation } from 'react-i18next';
import { api } from '../api/client';
import { ErrorState, Icon, SectionHeader, Spinner, useToasts } from '../components/ui';

export default function ShopPage() {
  const { t } = useTranslation();
  const queryClient = useQueryClient();
  const push = useToasts((s) => s.push);
  const shop = useQuery({ queryKey: ['shop'], queryFn: () => api('/shop') });
  const inventory = useQuery({ queryKey: ['inventory'], queryFn: () => api('/inventory') });

  const buy = useMutation({
    mutationFn: (id: string) => api(`/shop/${id}/buy`, { method: 'POST', body: {} }),
    onSuccess: (res: any) => {
      push({ title: res.item?.title ?? 'OK', body: t('shop.buy'), icon: 'shopping_bag', tone: 'success' });
      queryClient.invalidateQueries({ queryKey: ['shop'] });
      queryClient.invalidateQueries({ queryKey: ['inventory'] });
    },
    onError: (e: any) => push({ title: t('shop.not_enough'), body: e.message, icon: 'error', tone: 'danger' }),
  });

  if (shop.isLoading) return <Spinner />;
  if (shop.error) return <ErrorState error={shop.error} onRetry={() => shop.refetch()} />;

  const items = shop.data?.items ?? [];
  const coins = shop.data?.coins ?? 0;

  return (
    <div className="space-y-5">
      <section className="flex flex-col gap-3 sm:flex-row sm:items-end sm:justify-between">
        <div>
          <div className="label">SHOP · {items.length} ITEMS</div>
          <h1 className="text-3xl font-extrabold tracking-tight">{t('shop.title')}</h1>
        </div>
        <div className="flex items-center gap-2 rounded-2xl bg-amber-50 px-4 py-2.5 dark:bg-amber-500/10">
          <Icon name="monetization_on" className="text-amber" fill />
          <div>
            <div className="label">{t('shop.balance')}</div>
            <div className="font-mono text-lg font-bold text-amber-700 dark:text-amber-300">{coins}</div>
          </div>
        </div>
      </section>

      <section className="grid grid-cols-1 gap-3 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4">
        {items.map((item: any) => (
          <div key={item.id} className="card card-hover flex flex-col p-4">
            <div className="flex items-start justify-between">
              <span className="flex h-12 w-12 items-center justify-center rounded-2xl bg-brand-50 text-brand-600 dark:bg-brand-500/10 dark:text-brand-300">
                <Icon name={item.icon} className="text-[24px]" />
              </span>
              <span className="chip bg-panel2 text-[9px] uppercase text-muted">{item.kind}</span>
            </div>
            <div className="mt-3 font-semibold">{item.title}</div>
            <p className="mt-1 line-clamp-2 text-xs text-muted">{item.description}</p>
            <div className="mt-auto flex items-center justify-between pt-3">
              <div className="flex items-center gap-1 font-mono text-sm font-bold text-amber-600">
                <Icon name="monetization_on" className="text-[16px]" fill /> {item.price_coins}
              </div>
              {item.owned > 0 ? (
                <span className="chip bg-emerald-50 text-emerald-700 dark:bg-emerald-500/10 dark:text-emerald-300">
                  {t('shop.owned')} ×{item.owned}
                </span>
              ) : (
                <button className="btn-primary px-3 py-1.5 text-xs" disabled={coins < item.price_coins || buy.isPending} onClick={() => buy.mutate(item.id)}>
                  {t('shop.buy')}
                </button>
              )}
            </div>
          </div>
        ))}
      </section>

      <section className="card p-5">
        <SectionHeader title={t('shop.inventory')} subtitle={`${(inventory.data ?? []).length} items`} />
        <div className="grid grid-cols-2 gap-2 sm:grid-cols-3 lg:grid-cols-5">
          {(inventory.data ?? []).map((i: any) => (
            <div key={i.id} className="flex flex-col items-center gap-1 rounded-xl border border-line p-3 text-center">
              <Icon name={i.icon} className="text-brand-600" />
              <span className="line-clamp-2 text-[11px] font-medium">{i.title}</span>
              <span className="font-mono text-[10px] text-muted">×{i.qty}</span>
            </div>
          ))}
          {(inventory.data ?? []).length === 0 && <div className="col-span-full py-6 text-center text-sm text-muted">{t('common.empty')}</div>}
        </div>
      </section>
    </div>
  );
}
