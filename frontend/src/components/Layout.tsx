import { useEffect, useState } from 'react';
import { Link, NavLink, Outlet, useLocation, useNavigate } from 'react-router-dom';
import { useQuery } from '@tanstack/react-query';
import clsx from 'clsx';
import { api } from '../api/client';
import { useAuth } from '../store/auth';
import { Avatar, Icon, Progress, useToasts } from './ui';
import { useTranslation } from 'react-i18next';

function useWs() {
  const { accessToken } = useAuth();
  const push = useToasts((s) => s.push);
  useEffect(() => {
    if (!accessToken) return;
    const proto = window.location.protocol === 'https:' ? 'wss' : 'ws';
    const ws = new WebSocket(`${proto}://${window.location.host}/ws?token=${accessToken}`);
    ws.onmessage = (e) => {
      try {
        const msg = JSON.parse(e.data);
        if (msg.type === 'xp_gained') {
          push({ title: `+${msg.amount} XP`, body: `Level ${msg.level}`, icon: 'bolt', tone: 'success' });
        } else if (msg.type === 'level_up') {
          push({ title: 'Level up!', body: `${msg.level} · ${msg.title}`, icon: 'military_tech', tone: 'amber' });
        } else if (msg.type === 'achievement_unlocked') {
          push({ title: msg.title, body: 'Achievement unlocked', icon: msg.icon || 'emoji_events', tone: 'amber' });
        }
      } catch {
        /* ignore */
      }
    };
    return () => ws.close();
  }, [accessToken, push]);
}

export default function Layout() {
  const { t, i18n } = useTranslation();
  const { profile, locale, setLocale, theme, toggleTheme, logout } = useAuth();
  const [q, setQ] = useState('');
  const [mobileOpen, setMobileOpen] = useState(false);
  const navigate = useNavigate();
  const location = useLocation();
  useWs();

  const { data: notifications } = useQuery({
    queryKey: ['notifications', 'badge'],
    queryFn: () => api('/notifications', { params: { limit: 5 } }),
    refetchInterval: 60_000,
  });

  const nav = [
    { to: '/dashboard', label: t('nav.dashboard'), icon: 'space_dashboard' },
    { to: '/courses', label: t('nav.courses'), icon: 'auto_stories' },
    { to: '/ai', label: t('nav.ai'), icon: 'auto_awesome', accent: true },
    { to: '/leaderboard', label: t('nav.leaderboard'), icon: 'leaderboard' },
    { to: '/achievements', label: t('nav.achievements'), icon: 'emoji_events' },
    { to: '/shop', label: t('nav.shop'), icon: 'storefront' },
    { to: '/stats', label: t('nav.stats'), icon: 'monitoring' },
    { to: '/teams', label: t('nav.teams'), icon: 'groups' },
    { to: '/season', label: t('nav.season'), icon: 'calendar_month' },
  ];

  const role = profile?.role;
  if (role === 'teacher' || role === 'admin') nav.push({ to: '/teacher', label: t('nav.teacher'), icon: 'school' });
  if (role === 'admin') nav.push({ to: '/admin', label: t('nav.admin'), icon: 'admin_panel_settings' });

  const mobileNav = nav.slice(0, 5);

  useEffect(() => {
    i18n.changeLanguage(locale);
    document.documentElement.lang = locale;
  }, [locale, i18n]);

  useEffect(() => {
    document.documentElement.classList.toggle('dark', theme === 'dark');
  }, [theme]);

  useEffect(() => setMobileOpen(false), [location.pathname]);

  return (
    <div className="min-h-screen">
      {/* Sidebar */}
      <aside className="fixed inset-y-0 left-0 z-40 hidden w-[248px] flex-col border-r border-line bg-panel/80 px-4 py-5 backdrop-blur-xl lg:flex">
        <Link to="/dashboard" className="mb-6 flex items-center gap-2.5 px-2">
          <span className="flex h-9 w-9 items-center justify-center rounded-xl bg-brand-600 text-white shadow-glow">
            <Icon name="flare" fill />
          </span>
          <span className="text-lg font-extrabold tracking-tight">Lumina</span>
          <span className="rounded-md bg-brand-50 px-1.5 py-0.5 font-mono text-[9px] font-bold uppercase text-brand-600 dark:bg-brand-500/10 dark:text-brand-300">
            Pro
          </span>
        </Link>

        <div className="label mb-2 px-2">{t('nav.workspace')}</div>
        <nav className="flex flex-col gap-0.5">
          {nav.slice(0, 3).map((n) => (
            <NavLink key={n.to} to={n.to} className={({ isActive }) => clsx('nav-link', isActive && 'active')}>
              <Icon name={n.icon} />
              <span className="flex-1">{n.label}</span>
              {n.accent && <span className="chip bg-emerald-100 px-1.5 py-0 text-[9px] text-emerald-700 dark:bg-emerald-500/20 dark:text-emerald-300">AI</span>}
            </NavLink>
          ))}
        </nav>

        <div className="label mb-2 mt-6 px-2">{t('nav.progress')}</div>
        <nav className="flex flex-1 flex-col gap-0.5 overflow-y-auto">
          {nav.slice(3).map((n) => (
            <NavLink key={n.to} to={n.to} className={({ isActive }) => clsx('nav-link', isActive && 'active')}>
              <Icon name={n.icon} />
              <span className="flex-1">{n.label}</span>
            </NavLink>
          ))}
        </nav>

        <div className="mt-4 rounded-2xl border border-line bg-panel2/60 p-3">
          <div className="flex items-center gap-2.5">
            <Avatar name={profile?.nickname || profile?.display_name} size={38} />
            <div className="min-w-0 flex-1">
              <div className="truncate text-sm font-semibold">{profile?.nickname || profile?.display_name || '—'}</div>
              <div className="truncate text-[11px] text-muted">
                {profile?.level ?? 1}-{t('common.level')} · {profile?.title ?? 'Novice'}
              </div>
            </div>
          </div>
          <div className="mt-2.5 flex items-center justify-between font-mono text-[10px] text-muted">
            <span>{profile?.xp_in_level ?? 0} / {profile?.xp_to_next ?? 100} XP</span>
            <span className="text-brand-600 dark:text-brand-300">{Math.round((profile?.level_progress ?? 0) * 100)}%</span>
          </div>
          <Progress className="mt-1.5" value={(profile?.level_progress ?? 0) * 100} />
        </div>

        <button onClick={toggleTheme} className="nav-link mt-2 justify-start">
          <Icon name={theme === 'dark' ? 'light_mode' : 'dark_mode'} />
          <span>{theme === 'dark' ? t('profile.light') : t('profile.dark')}</span>
        </button>
      </aside>

      {/* Main */}
      <div className="lg:pl-[248px]">
        <header className="sticky top-0 z-30 border-b border-line bg-canvas/80 backdrop-blur-xl">
          <div className="mx-auto flex h-16 max-w-[1400px] items-center gap-3 px-4 sm:px-6">
            <button className="rounded-lg p-2 text-muted hover:bg-panel2 lg:hidden" onClick={() => setMobileOpen((v) => !v)} aria-label="Menu">
              <Icon name="menu" />
            </button>

            <div className="relative hidden max-w-md flex-1 md:block">
              <Icon name="search" className="pointer-events-none absolute left-3 top-1/2 -translate-y-1/2 text-muted" />
              <input
                value={q}
                onChange={(e) => setQ(e.target.value)}
                onKeyDown={(e) => {
                  if (e.key === 'Enter' && q.trim()) navigate(`/courses?q=${encodeURIComponent(q.trim())}`);
                }}
                className="input pl-10"
                placeholder={t('nav.search')}
                aria-label={t('nav.search')}
              />
            </div>

            <div className="ml-auto flex items-center gap-2">
              <div className="flex items-center rounded-full bg-panel2 p-0.5" role="group" aria-label="Language">
                {(['kk', 'ru', 'en'] as const).map((l) => (
                  <button
                    key={l}
                    onClick={() => {
                      setLocale(l);
                      api('/me', { method: 'PATCH', body: { locale: l } }).catch(() => {});
                    }}
                    className={clsx(
                      'rounded-full px-2 py-1 font-mono text-[10px] font-bold uppercase transition-all',
                      locale === l ? 'bg-panel text-ink shadow-sm' : 'text-muted hover:text-ink',
                    )}
                  >
                    {l === 'kk' ? 'KZ' : l.toUpperCase()}
                  </button>
                ))}
              </div>

              <Link to="/stats" className="hidden items-center gap-1 rounded-full bg-amber-50 px-2.5 py-1 text-xs font-semibold text-amber-700 dark:bg-amber-500/10 dark:text-amber-300 sm:flex">
                <Icon name="local_fire_department" className="text-[16px]" fill />
                <span className="font-mono">{profile?.current_streak ?? 0}</span>
              </Link>
              <Link to="/stats" className="hidden items-center gap-1 rounded-full bg-brand-50 px-2.5 py-1 text-xs font-semibold text-brand-700 dark:bg-brand-500/10 dark:text-brand-300 sm:flex">
                <Icon name="bolt" className="text-[16px]" fill />
                <span className="font-mono">{profile?.total_xp ?? 0}</span>
              </Link>
              <Link to="/shop" className="hidden items-center gap-1 rounded-full bg-panel2 px-2.5 py-1 text-xs font-semibold sm:flex">
                <Icon name="monetization_on" className="text-[16px] text-amber-500" fill />
                <span className="font-mono">{profile?.coins ?? 0}</span>
              </Link>

              <Link to="/notifications" className="relative rounded-full p-2 text-muted hover:bg-panel2" aria-label={t('nav.notifications')}>
                <Icon name="notifications" />
                {(notifications?.unread ?? 0) > 0 && (
                  <span className="absolute right-1 top-1 flex h-4 min-w-4 items-center justify-center rounded-full bg-red-500 px-1 text-[9px] font-bold text-white">
                    {notifications.unread}
                  </span>
                )}
              </Link>

              <Link to="/profile" aria-label={t('nav.profile')}>
                <Avatar name={profile?.nickname || profile?.display_name} size={34} />
              </Link>
            </div>
          </div>
        </header>

        {mobileOpen && (
          <div className="fixed inset-0 z-40 bg-slate-900/40 backdrop-blur-sm lg:hidden" onClick={() => setMobileOpen(false)}>
            <div className="h-full w-[280px] bg-panel p-4" onClick={(e) => e.stopPropagation()}>
              <div className="mb-4 flex items-center justify-between">
                <span className="text-lg font-extrabold">Lumina</span>
                <button className="p-2 text-muted" onClick={() => setMobileOpen(false)}>
                  <Icon name="close" />
                </button>
              </div>
              {nav.map((n) => (
                <NavLink key={n.to} to={n.to} className={({ isActive }) => clsx('nav-link', isActive && 'active')}>
                  <Icon name={n.icon} /> {n.label}
                </NavLink>
              ))}
              <button
                className="nav-link mt-4 w-full"
                onClick={async () => {
                  await logout();
                  navigate('/login');
                }}
              >
                <Icon name="logout" /> {t('nav.logout')}
              </button>
            </div>
          </div>
        )}

        <main className="mx-auto max-w-[1400px] px-4 pb-24 pt-6 sm:px-6 lg:pb-12">
          <Outlet />
        </main>

        <nav className="fixed inset-x-0 bottom-0 z-30 flex border-t border-line bg-panel/95 backdrop-blur-xl lg:hidden">
          {mobileNav.map((n) => (
            <NavLink
              key={n.to}
              to={n.to}
              className={({ isActive }) =>
                clsx('flex flex-1 flex-col items-center gap-0.5 py-2 text-[10px] font-medium', isActive ? 'text-brand-600' : 'text-muted')
              }
            >
              <Icon name={n.icon} />
              {n.label.split(' ')[0]}
            </NavLink>
          ))}
        </nav>
      </div>
    </div>
  );
}
