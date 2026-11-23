import { useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { useMutation } from '@tanstack/react-query';
import { useTranslation } from 'react-i18next';
import clsx from 'clsx';
import { api } from '../api/client';
import { useAuth } from '../store/auth';
import { Icon } from '../components/ui';

export default function AuthPage({ mode }: { mode: 'login' | 'register' }) {
  const { t, i18n } = useTranslation();
  const { setSession, locale, setLocale } = useAuth();
  const navigate = useNavigate();
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [name, setName] = useState('');
  const [nickname, setNickname] = useState('');
  const [error, setError] = useState<string | null>(null);

  const mutation = useMutation({
    mutationFn: () =>
      api(`/auth/${mode === 'login' ? 'login' : 'register'}`, {
        method: 'POST',
        body:
          mode === 'login'
            ? { email, password }
            : { email, password, display_name: name, nickname: nickname || name, locale },
      }),
    onSuccess: (data) => {
      setSession(data);
      const role = data?.user?.role;
      navigate(role === 'student' ? '/dashboard' : '/teacher', { replace: true });
    },
    onError: (e: any) => setError(e.message ?? t('auth.wrong')),
  });

  const demo = () => {
    setEmail('alisher@lumina.kz');
    setPassword('Lumina2026!');
    setError(null);
  };

  return (
    <div className="grid min-h-screen lg:grid-cols-[1.05fr,1fr]">
      <section className="relative hidden overflow-hidden hero-gradient p-12 text-white lg:flex lg:flex-col lg:justify-between">
        <div className="absolute -right-24 -top-24 h-96 w-96 rounded-full bg-white/10 blur-3xl" />
        <div className="absolute -bottom-32 -left-20 h-96 w-96 rounded-full bg-emerald-400/20 blur-3xl" />
        <div className="relative">
          <div className="flex items-center gap-3">
            <span className="flex h-11 w-11 items-center justify-center rounded-2xl bg-white/15 backdrop-blur">
              <Icon name="flare" fill />
            </span>
            <div>
              <div className="text-2xl font-extrabold tracking-tight">Lumina</div>
              <div className="font-mono text-[10px] uppercase tracking-[0.2em] text-white/60">Gamified Learning</div>
            </div>
          </div>

          <h1 className="mt-14 max-w-lg text-5xl font-extrabold leading-[1.08] tracking-tight">
            {t('auth.tagline')}
          </h1>

          <div className="mt-10 space-y-3">
            {[
              ['map', t('auth.feature1')],
              ['auto_awesome', t('auth.feature2')],
              ['leaderboard', t('auth.feature3')],
            ].map(([icon, label]) => (
              <div key={label} className="flex items-center gap-3 rounded-xl bg-white/10 px-4 py-3 backdrop-blur-sm">
                <span className="flex h-9 w-9 items-center justify-center rounded-lg bg-white/15">
                  <Icon name={icon} />
                </span>
                <span className="text-sm font-medium text-white/90">{label}</span>
              </div>
            ))}
          </div>
        </div>

        <div className="relative flex items-center gap-6 text-white/60">
          <div className="font-mono text-xs">320 tasks · 45 achievements · 4 courses</div>
        </div>
      </section>

      <section className="flex flex-col px-5 py-8 sm:px-10">
        <div className="flex items-center justify-between">
          <Link to="/" className="flex items-center gap-2 lg:invisible">
            <span className="flex h-8 w-8 items-center justify-center rounded-lg bg-brand-600 text-white">
              <Icon name="flare" fill />
            </span>
            <span className="font-extrabold">Lumina</span>
          </Link>
          <div className="flex items-center rounded-full bg-panel2 p-0.5">
            {(['kk', 'ru', 'en'] as const).map((l) => (
              <button
                key={l}
                onClick={() => {
                  setLocale(l);
                  i18n.changeLanguage(l);
                }}
                className={clsx(
                  'rounded-full px-2.5 py-1 font-mono text-[10px] font-bold uppercase',
                  locale === l ? 'bg-panel shadow-sm' : 'text-muted',
                )}
              >
                {l === 'kk' ? 'KZ' : l.toUpperCase()}
              </button>
            ))}
          </div>
        </div>

        <div className="mx-auto flex w-full max-w-md flex-1 flex-col justify-center">
          <h2 className="text-3xl font-extrabold tracking-tight">
            {mode === 'login' ? t('auth.login') : t('auth.register')}
          </h2>
          <p className="mt-1.5 text-sm text-muted">{t('auth.tagline')}</p>

          <form
            className="mt-7 space-y-4"
            onSubmit={(e) => {
              e.preventDefault();
              setError(null);
              mutation.mutate();
            }}
          >
            {mode === 'register' && (
              <>
                <div>
                  <label className="label mb-1.5 block">{t('auth.display_name')}</label>
                  <input className="input" value={name} onChange={(e) => setName(e.target.value)} required minLength={2} />
                </div>
                <div>
                  <label className="label mb-1.5 block">{t('auth.nickname')}</label>
                  <input className="input" value={nickname} onChange={(e) => setNickname(e.target.value)} placeholder="alisher" />
                </div>
              </>
            )}
            <div>
              <label className="label mb-1.5 block">{t('auth.email')}</label>
              <input className="input" type="email" value={email} onChange={(e) => setEmail(e.target.value)} required autoComplete="email" />
            </div>
            <div>
              <label className="label mb-1.5 block">{t('auth.password')}</label>
              <input className="input" type="password" value={password} onChange={(e) => setPassword(e.target.value)} required minLength={8} autoComplete="current-password" />
            </div>

            {error && (
              <div className="flex items-center gap-2 rounded-xl bg-red-50 px-3 py-2 text-sm text-red-700 dark:bg-red-500/10 dark:text-red-300">
                <Icon name="error" className="text-[18px]" /> {error}
              </div>
            )}

            <button className="btn-primary w-full py-3" disabled={mutation.isPending}>
              {mutation.isPending ? t('common.loading') : mode === 'login' ? t('auth.login') : t('auth.register')}
              <Icon name="arrow_forward" />
            </button>
          </form>

          {import.meta.env.DEV && mode === 'login' && (
            <button className="btn-ghost mt-3 w-full" type="button" onClick={demo}>
              <Icon name="person" /> {t('auth.demo')}
            </button>
          )}

          <div className="mt-6 text-center text-sm text-muted">
            {mode === 'login' ? t('auth.no_account') : t('auth.have_account')}{' '}
            <Link to={mode === 'login' ? '/register' : '/login'} className="font-semibold text-brand-600 hover:underline dark:text-brand-300">
              {mode === 'login' ? t('auth.register') : t('auth.login')}
            </Link>
          </div>
        </div>
      </section>
    </div>
  );
}
