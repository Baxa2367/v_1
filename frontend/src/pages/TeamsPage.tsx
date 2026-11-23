import { useState } from 'react';
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { useTranslation } from 'react-i18next';
import clsx from 'clsx';
import { api } from '../api/client';
import { Avatar, ErrorState, Icon, SectionHeader, Spinner, useToasts } from '../components/ui';

export default function TeamsPage() {
  const { t } = useTranslation();
  const queryClient = useQueryClient();
  const push = useToasts((s) => s.push);
  const [name, setName] = useState('');
  const [code, setCode] = useState('');
  const [tab, setTab] = useState<'teams' | 'tournaments'>('teams');

  const mine = useQuery({ queryKey: ['team', 'mine'], queryFn: () => api('/teams/mine') });
  const teams = useQuery({ queryKey: ['teams'], queryFn: () => api('/teams') });
  const tournaments = useQuery({ queryKey: ['tournaments'], queryFn: () => api('/tournaments') });
  const firstTournament = tournaments.data?.tournaments?.[0];
  const standings = useQuery({
    queryKey: ['standings', firstTournament?.id],
    queryFn: () => api(`/tournaments/${firstTournament.id}/standings`),
    enabled: !!firstTournament?.id,
  });

  const create = useMutation({
    mutationFn: () => api('/teams', { method: 'POST', body: { name, emblem: 'groups' } }),
    onSuccess: () => {
      push({ title: t('teams.create'), icon: 'groups', tone: 'success' });
      setName('');
      queryClient.invalidateQueries({ queryKey: ['team'] });
      queryClient.invalidateQueries({ queryKey: ['teams'] });
    },
    onError: (e: any) => push({ title: e.message, icon: 'error', tone: 'danger' }),
  });

  const join = useMutation({
    mutationFn: () => api('/teams/join', { method: 'POST', body: { invite_code: code } }),
    onSuccess: () => {
      push({ title: t('teams.join'), icon: 'login', tone: 'success' });
      setCode('');
      queryClient.invalidateQueries({ queryKey: ['team'] });
      queryClient.invalidateQueries({ queryKey: ['teams'] });
    },
    onError: (e: any) => push({ title: e.message, icon: 'error', tone: 'danger' }),
  });

  const leave = useMutation({
    mutationFn: () => api('/teams/leave', { method: 'POST', body: {} }),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['team'] });
      queryClient.invalidateQueries({ queryKey: ['teams'] });
    },
  });

  if (mine.isLoading) return <Spinner />;
  if (mine.error) return <ErrorState error={mine.error} onRetry={() => mine.refetch()} />;

  const myTeam = mine.data?.team;

  return (
    <div className="space-y-5">
      <section className="flex flex-col gap-3 lg:flex-row lg:items-end lg:justify-between">
        <div>
          <div className="label">TEAMS · TOURNAMENTS</div>
          <h1 className="text-3xl font-extrabold tracking-tight">{t('teams.title')}</h1>
        </div>
        <div className="flex gap-1 rounded-xl bg-panel2 p-1">
          {[
            { id: 'teams', label: t('nav.teams'), icon: 'groups' },
            { id: 'tournaments', label: t('teams.tournaments'), icon: 'emoji_events' },
          ].map((x) => (
            <button key={x.id} onClick={() => setTab(x.id as any)} className={clsx('flex items-center gap-1.5 rounded-lg px-3 py-1.5 text-sm font-medium', tab === x.id ? 'bg-panel shadow-sm' : 'text-muted')}>
              <Icon name={x.icon} className="text-[16px]" /> {x.label}
            </button>
          ))}
        </div>
      </section>

      {tab === 'teams' ? (
        <div className="grid grid-cols-1 gap-5 xl:grid-cols-12">
          <section className="space-y-4 xl:col-span-5">
            {myTeam ? (
              <div className="card p-5">
                <div className="flex items-start gap-3">
                  <span className="flex h-14 w-14 items-center justify-center rounded-2xl bg-brand-50 text-brand-600 dark:bg-brand-500/10 dark:text-brand-300">
                    <Icon name={myTeam.emblem || 'groups'} className="text-[28px]" />
                  </span>
                  <div className="min-w-0 flex-1">
                    <div className="label">{t('teams.my')}</div>
                    <h2 className="text-xl font-bold">{myTeam.name}</h2>
                    <div className="mt-1 flex flex-wrap gap-2 font-mono text-[10px] text-muted">
                      <span>{myTeam.member_count} {t('teams.members')}</span>
                      <span>{myTeam.total_xp} XP</span>
                    </div>
                  </div>
                </div>
                <div className="mt-3 rounded-xl bg-panel2 p-2.5 text-center">
                  <div className="label">{t('teams.code')}</div>
                  <div className="font-mono text-lg font-bold tracking-[0.2em]">{myTeam.invite_code}</div>
                </div>
                <div className="mt-3 space-y-1.5">
                  {(myTeam.members ?? []).map((m: any) => (
                    <div key={m.student_id} className="flex items-center gap-2.5 rounded-xl px-1 py-1">
                      <Avatar name={m.nickname} size={28} />
                      <span className="flex-1 truncate text-xs font-medium">{m.nickname}</span>
                      {m.role === 'captain' && <Icon name="star" className="text-amber text-[16px]" fill />}
                      <span className="font-mono text-[10px] text-muted">{m.total_xp} XP</span>
                    </div>
                  ))}
                </div>
                <button className="btn-ghost mt-3 w-full" onClick={() => leave.mutate()} disabled={leave.isPending}>
                  {t('teams.leave')}
                </button>
              </div>
            ) : (
              <div className="card p-5">
                <SectionHeader title={t('teams.no_team')} subtitle={t('teams.create')} />
                <label className="label mb-1 block">{t('teams.name')}</label>
                <input className="input" value={name} onChange={(e) => setName(e.target.value)} placeholder="Samuryq" />
                <button className="btn-primary mt-2 w-full" disabled={name.trim().length < 2 || create.isPending} onClick={() => create.mutate()}>
                  <Icon name="add" /> {t('teams.create')}
                </button>
                <div className="my-4 flex items-center gap-2 text-[10px] uppercase text-muted">
                  <div className="h-px flex-1 bg-line" /> OR <div className="h-px flex-1 bg-line" />
                </div>
                <label className="label mb-1 block">{t('teams.code')}</label>
                <div className="flex gap-2">
                  <input className="input font-mono uppercase" value={code} onChange={(e) => setCode(e.target.value)} placeholder="LUMINA1" />
                  <button className="btn-ghost" disabled={!code || join.isPending} onClick={() => join.mutate()}>
                    <Icon name="login" />
                  </button>
                </div>
              </div>
            )}
          </section>

          <section className="card p-5 xl:col-span-7">
            <SectionHeader title={t('nav.teams')} subtitle={`${teams.data?.teams?.length ?? 0} teams`} />
            <div className="space-y-2">
              {(teams.data?.teams ?? []).map((team: any, i: number) => (
                <div key={team.id} className={clsx('flex items-center gap-3 rounded-xl border border-line p-3', myTeam?.id === team.id && 'border-brand-300 bg-brand-50/40 dark:bg-brand-500/5')}>
                  <span className="w-5 font-mono text-xs font-bold text-muted">{i + 1}</span>
                  <span className="flex h-10 w-10 items-center justify-center rounded-xl bg-panel2 text-brand-600">
                    <Icon name={team.emblem} />
                  </span>
                  <div className="min-w-0 flex-1">
                    <div className="truncate text-sm font-semibold">{team.name}</div>
                    <div className="text-[10px] text-muted">{team.members} {t('teams.members')}</div>
                  </div>
                  <span className="font-mono text-sm font-semibold">{team.total_xp}</span>
                </div>
              ))}
            </div>
          </section>
        </div>
      ) : (
        <div className="grid grid-cols-1 gap-5 xl:grid-cols-12">
          <section className="card p-5 xl:col-span-7">
            <SectionHeader title={t('teams.tournaments')} subtitle={`${tournaments.data?.tournaments?.length ?? 0}`} />
            <div className="space-y-2">
              {(tournaments.data?.tournaments ?? []).map((tr: any) => (
                <div key={tr.id} className="flex items-center gap-3 rounded-xl border border-line p-3">
                  <span className="flex h-10 w-10 items-center justify-center rounded-xl bg-amber-50 text-amber-600 dark:bg-amber-500/10">
                    <Icon name="emoji_events" />
                  </span>
                  <div className="min-w-0 flex-1">
                    <div className="truncate text-sm font-semibold">{tr.title}</div>
                    <div className="text-[10px] uppercase text-muted">{tr.status} · {tr.metric} · {tr.teams} teams</div>
                  </div>
                  {tr.my_rank && <span className="chip bg-brand-50 text-brand-700 dark:bg-brand-500/10 dark:text-brand-300">#{tr.my_rank.rank}</span>}
                </div>
              ))}
            </div>
          </section>
          <section className="card p-5 xl:col-span-5">
            <SectionHeader title={t('teams.standings')} subtitle={firstTournament?.title} />
            <div className="space-y-1.5">
              {(standings.data?.standings ?? []).map((s: any) => (
                <div key={s.team_id} className="flex items-center gap-2.5 rounded-xl px-2 py-1.5">
                  <span className="w-4 font-mono text-xs text-muted">{s.rank}</span>
                  <span className="flex h-8 w-8 items-center justify-center rounded-lg bg-panel2 text-brand-600">
                    <Icon name={s.emblem} className="text-[16px]" />
                  </span>
                  <span className="flex-1 truncate text-xs font-semibold">{s.name}</span>
                  <span className="font-mono text-xs">{Math.round(s.score)}</span>
                </div>
              ))}
              {(standings.data?.standings ?? []).length === 0 && <div className="py-6 text-center text-xs text-muted">{t('common.empty')}</div>}
            </div>
          </section>
        </div>
      )}
    </div>
  );
}
