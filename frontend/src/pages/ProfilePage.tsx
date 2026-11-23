import { useEffect, useState } from 'react';
import { useMutation, useQuery } from '@tanstack/react-query';
import { useTranslation } from 'react-i18next';
import clsx from 'clsx';
import { api } from '../api/client';
import { useAuth } from '../store/auth';
import { Avatar, ErrorState, Icon, Progress, SectionHeader, Spinner, useToasts } from '../components/ui';

export default function ProfilePage() {
  const { t } = useTranslation();
  const { profile, refreshMe, setLocale, locale, theme, toggleTheme, logout } = useAuth();
  const push = useToasts((s) => s.push);
  const me = useQuery({ queryKey: ['me'], queryFn: () => api('/me') });
  const [nickname, setNickname] = useState('');
  const [bio, setBio] = useState('');
  const [goal, setGoal] = useState(50);
  const [publicBoard, setPublicBoard] = useState(true);
  const [currentPassword, setCurrentPassword] = useState('');
  const [newPassword, setNewPassword] = useState('');

  useEffect(() => {
    if (me.data?.profile) {
      setNickname(me.data.profile.nickname ?? '');
      setBio(me.data.profile.bio ?? '');
      setGoal(me.data.profile.daily_goal_xp ?? 50);
      setPublicBoard(me.data.profile.privacy_flags?.public_leaderboard ?? true);
    }
  }, [me.data]);

  const save = useMutation({
    mutationFn: () =>
      api('/me', {
        method: 'PATCH',
        body: { nickname, bio, daily_goal_xp: goal, privacy_flags: { public_leaderboard: publicBoard } },
      }),
    onSuccess: () => {
      push({ title: t('common.saved'), icon: 'check_circle', tone: 'success' });
      refreshMe();
    },
    onError: (e: any) => push({ title: e.message, icon: 'error', tone: 'danger' }),
  });

  const password = useMutation({
    mutationFn: () => api('/me/password', { method: 'POST', body: { current_password: currentPassword, new_password: newPassword } }),
    onSuccess: () => {
      push({ title: t('common.saved'), icon: 'lock', tone: 'success' });
      setCurrentPassword('');
      setNewPassword('');
    },
    onError: (e: any) => push({ title: e.message, icon: 'error', tone: 'danger' }),
  });

  if (me.isLoading) return <Spinner />;
  if (me.error) return <ErrorState error={me.error} onRetry={() => me.refetch()} />;

  const p = me.data?.profile ?? profile;

  return (
    <div className="space-y-5">
      <section className="relative overflow-hidden rounded-2xl hero-gradient p-6 text-white">
        <div className="absolute -right-10 -top-16 h-64 w-64 rounded-full bg-white/10 blur-3xl" />
        <div className="relative flex flex-wrap items-center gap-5">
          <div className="rounded-full border-4 border-white/30">
            <Avatar name={p?.nickname} size={84} />
          </div>
          <div className="min-w-0">
            <div className="flex flex-wrap items-center gap-2">
              <h1 className="text-2xl font-extrabold">{p?.nickname || p?.display_name}</h1>
              <span className="chip bg-white/15 uppercase text-white">{p?.league}</span>
            </div>
            <div className="mt-0.5 text-sm text-white/70">
              {p?.level}-{t('common.level')} · {p?.title} · {p?.email}
            </div>
            <div className="mt-3 w-64 max-w-full">
              <div className="flex justify-between font-mono text-[10px] text-white/70">
                <span>{p?.xp_in_level} / {p?.xp_to_next} XP</span>
                <span>{Math.round((p?.level_progress ?? 0) * 100)}%</span>
              </div>
              <div className="mt-1 h-2 overflow-hidden rounded-full bg-white/20">
                <div className="h-full rounded-full bg-emerald-300" style={{ width: `${(p?.level_progress ?? 0) * 100}%` }} />
              </div>
            </div>
          </div>
          <div className="ml-auto grid grid-cols-3 gap-4 text-center">
            {[
              [p?.total_xp ?? 0, 'XP'],
              [p?.current_streak ?? 0, t('common.streak')],
              [p?.coins ?? 0, t('common.coins')],
            ].map(([v, l]) => (
              <div key={String(l)}>
                <div className="font-mono text-xl font-bold">{v}</div>
                <div className="text-[10px] uppercase text-white/60">{l}</div>
              </div>
            ))}
          </div>
        </div>
      </section>

      <div className="grid grid-cols-1 gap-5 lg:grid-cols-2">
        <section className="card p-5">
          <SectionHeader title={t('profile.title')} />
          <div className="space-y-3">
            <div>
              <label className="label mb-1 block">{t('auth.nickname')}</label>
              <input className="input" value={nickname} onChange={(e) => setNickname(e.target.value)} />
            </div>
            <div>
              <label className="label mb-1 block">{t('profile.bio')}</label>
              <textarea className="input min-h-[80px]" value={bio} onChange={(e) => setBio(e.target.value)} maxLength={300} />
            </div>
            <div>
              <label className="label mb-1 block">{t('profile.goal')}</label>
              <div className="grid grid-cols-4 gap-1.5">
                {[20, 50, 100, 200].map((g) => (
                  <button
                    key={g}
                    onClick={() => setGoal(g)}
                    className={clsx('rounded-xl border px-2 py-2 text-sm font-semibold', goal === g ? 'border-brand-500 bg-brand-50 text-brand-700 dark:bg-brand-500/10 dark:text-brand-300' : 'border-line text-muted')}
                  >
                    {g} XP
                  </button>
                ))}
              </div>
            </div>
            <label className="flex cursor-pointer items-center justify-between rounded-xl border border-line p-3">
              <span className="text-sm">{t('profile.privacy')}</span>
              <input type="checkbox" checked={publicBoard} onChange={(e) => setPublicBoard(e.target.checked)} className="h-5 w-9 accent-brand-600" />
            </label>
            <button className="btn-primary w-full" onClick={() => save.mutate()} disabled={save.isPending}>
              {save.isPending ? t('common.saving') : t('common.save')}
            </button>
          </div>
        </section>

        <div className="space-y-5">
          <section className="card p-5">
            <SectionHeader title={t('profile.language')} />
            <div className="grid grid-cols-3 gap-2">
              {(['kk', 'ru', 'en'] as const).map((l) => (
                <button
                  key={l}
                  onClick={() => {
                    setLocale(l);
                    api('/me', { method: 'PATCH', body: { locale: l } }).catch(() => {});
                  }}
                  className={clsx('rounded-xl border px-2 py-2.5 text-sm font-semibold uppercase', locale === l ? 'border-brand-500 bg-brand-50 text-brand-700 dark:bg-brand-500/10 dark:text-brand-300' : 'border-line text-muted')}
                >
                  {l === 'kk' ? 'KZ' : l}
                </button>
              ))}
            </div>
            <button className="btn-ghost mt-3 w-full" onClick={toggleTheme}>
              <Icon name={theme === 'dark' ? 'light_mode' : 'dark_mode'} /> {theme === 'dark' ? t('profile.light') : t('profile.dark')}
            </button>
          </section>

          <section className="card p-5">
            <SectionHeader title={t('profile.password')} />
            <div className="space-y-2">
              <input className="input" type="password" placeholder={t('profile.current_password')} value={currentPassword} onChange={(e) => setCurrentPassword(e.target.value)} />
              <input className="input" type="password" placeholder={t('profile.new_password')} value={newPassword} onChange={(e) => setNewPassword(e.target.value)} />
              <button className="btn-ghost w-full" disabled={!currentPassword || newPassword.length < 8 || password.isPending} onClick={() => password.mutate()}>
                <Icon name="lock_reset" /> {t('profile.password')}
              </button>
            </div>
          </section>

          <section className="card p-5">
            <div className="flex items-center justify-between">
              <div>
                <div className="label">{t('shop.balance')}</div>
                <div className="mt-1 flex items-center gap-4 font-mono text-lg font-bold">
                  <span className="text-amber-600">{p?.coins ?? 0} · {t('common.coins')}</span>
                </div>
              </div>
              <button
                className="btn-ghost text-red-600"
                onClick={async () => {
                  await logout();
                  window.location.href = '/login';
                }}
              >
                <Icon name="logout" /> {t('nav.logout')}
              </button>
            </div>
          </section>
        </div>
      </div>
    </div>
  );
}
