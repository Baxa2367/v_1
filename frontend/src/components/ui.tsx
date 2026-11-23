import { Link } from 'react-router-dom';
import { create } from 'zustand';
import clsx from 'clsx';
import { Component, type ErrorInfo, type ReactNode } from 'react';

export function Icon({ name, className, fill }: { name: string; className?: string; fill?: boolean }) {
  return <span className={clsx('material-symbols-outlined', fill && 'fill', className)}>{name}</span>;
}

const AVATAR_COLORS = ['#4f46e5', '#0ea5e9', '#10b981', '#f59e0b', '#ec4899', '#8b5cf6'];

export function Avatar({ name, size = 40, src }: { name?: string; size?: number; src?: string }) {
  const initial = (name ?? '?').trim().charAt(0).toUpperCase();
  const idx = (name ?? '').split('').reduce((a, c) => a + c.charCodeAt(0), 0) % AVATAR_COLORS.length;
  if (src && src.startsWith('http')) {
    return <img src={src} alt={name} style={{ width: size, height: size }} className="rounded-full object-cover" />;
  }
  return (
    <span
      className="inline-flex items-center justify-center rounded-full font-semibold text-white shrink-0"
      style={{ width: size, height: size, background: AVATAR_COLORS[idx], fontSize: size * 0.42 }}
      aria-label={name}
    >
      {initial}
    </span>
  );
}

export function Progress({ value, className, tone = 'brand' }: { value: number; className?: string; tone?: 'brand' | 'success' | 'amber' }) {
  const pct = Math.max(0, Math.min(100, value));
  const bg = tone === 'success' ? 'bg-success' : tone === 'amber' ? 'bg-amber' : 'bg-brand-600';
  return (
    <div className={clsx('h-2 w-full overflow-hidden rounded-full bg-line/70', className)}>
      <div className={clsx('h-full rounded-full transition-all duration-700', bg)} style={{ width: `${pct}%` }} />
    </div>
  );
}

export function Stat({
  label, value, sub, icon, tone = 'brand', className,
}: { label: string; value: ReactNode; sub?: ReactNode; icon?: string; tone?: 'brand' | 'success' | 'amber' | 'sky'; className?: string }) {
  const tones: Record<string, string> = {
    brand: 'bg-brand-50 text-brand-600 dark:bg-brand-500/10 dark:text-brand-300',
    success: 'bg-emerald-50 text-emerald-600 dark:bg-emerald-500/10 dark:text-emerald-300',
    amber: 'bg-amber-50 text-amber-600 dark:bg-amber-500/10 dark:text-amber-300',
    sky: 'bg-sky-50 text-sky-600 dark:bg-sky-500/10 dark:text-sky-300',
  };
  return (
    <div className={clsx('card p-5', className)}>
      <div className="flex items-start justify-between gap-3">
        <div className="min-w-0">
          <div className="label">{label}</div>
          <div className="mt-1 truncate font-mono text-xl font-semibold">{value}</div>
          {sub && <div className="mt-1 text-xs text-muted">{sub}</div>}
        </div>
        {icon && (
          <span className={clsx('flex h-10 w-10 items-center justify-center rounded-xl', tones[tone])}>
            <Icon name={icon} />
          </span>
        )}
      </div>
    </div>
  );
}

export function SectionHeader({ title, subtitle, action }: { title: ReactNode; subtitle?: ReactNode; action?: ReactNode }) {
  return (
    <div className="mb-4 flex items-end justify-between gap-3">
      <div>
        {subtitle && <div className="label mb-1">{subtitle}</div>}
        <h2 className="text-xl font-bold tracking-tight">{title}</h2>
      </div>
      {action}
    </div>
  );
}

export function Spinner({ className }: { className?: string }) {
  return (
    <div className={clsx('flex items-center justify-center py-16', className)}>
      <div className="h-8 w-8 animate-spin rounded-full border-2 border-line border-t-brand-600" />
    </div>
  );
}

export function Empty({ icon = 'inbox', title, hint, action }: { icon?: string; title: string; hint?: string; action?: ReactNode }) {
  return (
    <div className="flex flex-col items-center justify-center gap-2 py-14 text-center">
      <span className="flex h-14 w-14 items-center justify-center rounded-2xl bg-panel2 text-muted">
        <Icon name={icon} className="text-[26px]" />
      </span>
      <div className="mt-1 font-semibold">{title}</div>
      {hint && <div className="max-w-sm text-sm text-muted">{hint}</div>}
      {action && <div className="mt-3">{action}</div>}
    </div>
  );
}

export function ErrorState({ error, onRetry }: { error: any; onRetry?: () => void }) {
  return (
    <div className="card flex flex-col items-center gap-2 p-8 text-center">
      <span className="flex h-12 w-12 items-center justify-center rounded-2xl bg-red-50 text-red-600 dark:bg-red-500/10">
        <Icon name="error" />
      </span>
      <div className="font-semibold">{error?.message ?? 'Error'}</div>
      {onRetry && (
        <button className="btn-ghost mt-2" onClick={onRetry}>
          <Icon name="refresh" /> Retry
        </button>
      )}
    </div>
  );
}

export function Modal({ open, onClose, title, children, footer }: { open: boolean; onClose: () => void; title: ReactNode; children: ReactNode; footer?: ReactNode }) {
  if (!open) return null;
  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-slate-900/40 p-4 backdrop-blur-sm" onClick={onClose}>
      <div className="card w-full max-w-lg animate-pop p-6" onClick={(e) => e.stopPropagation()}>
        <div className="mb-4 flex items-center justify-between">
          <h3 className="text-lg font-bold">{title}</h3>
          <button className="rounded-lg p-1 text-muted hover:bg-panel2" onClick={onClose} aria-label="Close">
            <Icon name="close" />
          </button>
        </div>
        {children}
        {footer && <div className="mt-5 flex justify-end gap-2">{footer}</div>}
      </div>
    </div>
  );
}

export function Tabs({ tabs, value, onChange }: { tabs: { id: string; label: string; icon?: string }[]; value: string; onChange: (id: string) => void }) {
  return (
    <div className="flex flex-wrap gap-1 rounded-xl bg-panel2 p-1">
      {tabs.map((t) => (
        <button
          key={t.id}
          onClick={() => onChange(t.id)}
          className={clsx(
            'flex items-center gap-1.5 rounded-lg px-3 py-1.5 text-sm font-medium transition-all',
            value === t.id ? 'bg-panel text-ink shadow-sm' : 'text-muted hover:text-ink',
          )}
        >
          {t.icon && <Icon name={t.icon} className="text-[16px]" />}
          {t.label}
        </button>
      ))}
    </div>
  );
}

export function Coin({ value }: { value: number }) {
  return (
    <span className="inline-flex items-center gap-1 font-mono text-xs font-semibold text-amber-600 dark:text-amber-400">
      <Icon name="monetization_on" className="text-[16px]" fill /> {value}
    </span>
  );
}

export function LinkButton({ to, children, className }: { to: string; children: ReactNode; className?: string }) {
  return (
    <Link to={to} className={clsx('btn-ghost', className)}>
      {children}
    </Link>
  );
}

export class AppErrorBoundary extends Component<{ children: ReactNode }, { failed: boolean }> {
  state = { failed: false };

  static getDerivedStateFromError() {
    return { failed: true };
  }

  componentDidCatch(error: Error, info: ErrorInfo) {
    console.error('Lumina UI error', error, info);
  }

  render() {
    if (this.state.failed) {
      return (
        <main className="flex min-h-screen items-center justify-center bg-canvas p-5">
          <section className="card w-full max-w-md overflow-hidden p-7 text-center shadow-glow">
            <span className="mx-auto flex h-14 w-14 items-center justify-center rounded-2xl bg-brand-50 text-brand-600 dark:bg-brand-500/10 dark:text-brand-300">
              <Icon name="auto_awesome" fill />
            </span>
            <p className="label mt-5">Lumina</p>
            <h1 className="mt-1 text-xl font-bold">Не удалось открыть страницу</h1>
            <p className="mt-2 text-sm leading-6 text-muted">Данные сохранены. Обновите страницу — мы уже записали техническую информацию для диагностики.</p>
            <button className="btn-primary mt-6 w-full" onClick={() => window.location.reload()}>
              <Icon name="refresh" /> Обновить страницу
            </button>
          </section>
        </main>
      );
    }
    return this.props.children;
  }
}

// ---------------- toasts ----------------
type Toast = { id: number; title: string; body?: string; icon?: string; tone?: 'brand' | 'success' | 'amber' | 'danger' };
interface ToastState {
  toasts: Toast[];
  push: (t: Omit<Toast, 'id'>) => void;
  remove: (id: number) => void;
}
let nextId = 1;
export const useToasts = create<ToastState>((set) => ({
  toasts: [],
  push: (t) => {
    const id = nextId++;
    set((s) => ({ toasts: [...s.toasts, { ...t, id }] }));
    setTimeout(() => set((s) => ({ toasts: s.toasts.filter((x) => x.id !== id) })), 4500);
  },
  remove: (id) => set((s) => ({ toasts: s.toasts.filter((x) => x.id !== id) })),
}));

export function Toaster() {
  const { toasts, remove } = useToasts();
  return (
    <div className="pointer-events-none fixed bottom-4 right-4 z-[60] flex w-[340px] flex-col gap-2">
      {toasts.map((t) => (
        <div key={t.id} className="pointer-events-auto card flex items-start gap-3 p-3.5 shadow-glow animate-pop">
          <span
            className={clsx(
              'flex h-9 w-9 items-center justify-center rounded-xl',
              t.tone === 'success' ? 'bg-emerald-50 text-emerald-600' : t.tone === 'amber' ? 'bg-amber-50 text-amber-600' : 'bg-brand-50 text-brand-600',
            )}
          >
            <Icon name={t.icon ?? 'notifications'} fill />
          </span>
          <div className="min-w-0 flex-1">
            <div className="text-sm font-semibold">{t.title}</div>
            {t.body && <div className="mt-0.5 text-xs text-muted">{t.body}</div>}
          </div>
          <button className="text-muted hover:text-ink" onClick={() => remove(t.id)} aria-label="Dismiss">
            <Icon name="close" className="text-[16px]" />
          </button>
        </div>
      ))}
    </div>
  );
}
