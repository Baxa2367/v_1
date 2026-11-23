import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { Link, useNavigate, useParams } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import clsx from 'clsx';
import { api } from '../api/client';
import { ErrorState, Icon, Progress, SectionHeader, Spinner, useToasts } from '../components/ui';

export default function CourseMapPage() {
  const { id } = useParams();
  const { t } = useTranslation();
  const navigate = useNavigate();
  const push = useToasts((s) => s.push);
  const queryClient = useQueryClient();

  const map = useQuery({ queryKey: ['map', id], queryFn: () => api(`/courses/${id}/map`) });
  const course = useQuery({ queryKey: ['course', id], queryFn: () => api(`/courses/${id}`) });
  const mastery = useQuery({
    queryKey: ['mastery', id],
    queryFn: () => api('/ai/mastery', { params: { course_id: id } }),
    enabled: !!id,
  });

  const enroll = useMutation({
    mutationFn: () => api(`/courses/${id}/enroll`, { method: 'POST', body: {} }),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['course', id] });
      queryClient.invalidateQueries({ queryKey: ['map', id] });
      push({ title: t('course.enrolled'), icon: 'check_circle', tone: 'success' });
    },
  });

  if (map.isLoading) return <Spinner />;
  if (map.error) return <ErrorState error={map.error} onRetry={() => map.refetch()} />;

  const data = map.data;
  const levels = data.levels ?? [];
  const recommended = data.recommended_level;
  const completed = levels.filter((l: any) => l.state === 'completed').length;
  const stars = levels.reduce((a: number, l: any) => a + (l.stars ?? 0), 0);

  const openLevel = (level: any) => {
    if (level.state === 'locked') {
      push({ title: t('course.locked_hint'), icon: 'lock', tone: 'amber' });
      return;
    }
    navigate(`/levels/${level.id}`);
  };

  return (
    <div className="space-y-5">
      <section className="flex items-center gap-2 text-xs text-muted">
        <Link to="/courses" className="hover:text-ink">{t('nav.courses')}</Link>
        <Icon name="chevron_right" className="text-[14px]" />
        <span className="font-medium text-ink">{data.course.title}</span>
      </section>

      <section className="relative overflow-hidden rounded-2xl hero-gradient p-6 text-white lg:p-8">
        <div className="absolute -right-10 -top-16 h-64 w-64 rounded-full bg-white/10 blur-3xl" />
        <div className="relative flex flex-col justify-between gap-5 lg:flex-row lg:items-end">
          <div className="max-w-2xl">
            <div className="flex flex-wrap items-center gap-2">
              <span className="chip bg-white/15 uppercase tracking-wider text-white backdrop-blur">
                {data.course.category}
              </span>
              <span className="chip bg-white/15 text-white">{'★'.repeat(Math.min(5, Math.round(data.course.difficulty / 2)))}</span>
            </div>
            <h1 className="mt-2 text-3xl font-extrabold tracking-tight">{data.course.title}</h1>
            <p className="mt-1.5 text-sm text-white/75">{data.course.description}</p>
            <div className="mt-4 flex flex-wrap gap-5 font-mono text-[11px] text-white/80">
              <span>{levels.length} {t('course.levels').toLowerCase()}</span>
              <span>{data.course.tasks_count} {t('common.tasks')}</span>
              <span>{completed} {t('common.completed').toLowerCase()}</span>
              <span>★ {stars}</span>
            </div>
          </div>
          <div className="flex flex-col gap-2 sm:flex-row lg:flex-col">
            {recommended ? (
              <button className="btn bg-success text-white hover:brightness-95" onClick={() => openLevel(recommended)}>
                {t('course.continue')} <Icon name="arrow_forward" />
              </button>
            ) : (
              <button className="btn bg-success text-white hover:brightness-95" onClick={() => enroll.mutate()} disabled={enroll.isPending}>
                {t('course.enroll')}
              </button>
            )}
            <Link to="/ai" className="btn bg-white/10 text-white backdrop-blur hover:bg-white/20">
              <Icon name="auto_awesome" /> {t('ai.title')}
            </Link>
          </div>
        </div>
        <div className="relative mt-5">
          <div className="flex items-center justify-between font-mono text-[10px] text-white/70">
            <span>{t('course.progress')}</span>
            <span>{completed}/{levels.length}</span>
          </div>
          <div className="mt-1.5 h-2 overflow-hidden rounded-full bg-white/20">
            <div className="h-full rounded-full bg-emerald-300" style={{ width: `${(completed / Math.max(1, levels.length)) * 100}%` }} />
          </div>
        </div>
      </section>

      <div className="grid grid-cols-1 gap-5 xl:grid-cols-12">
        <section className="xl:col-span-8">
          <div className="mb-3 flex items-center justify-between">
            <SectionHeader title={t('course.map')} subtitle={`${completed} / ${levels.length}`} />
          </div>

          <div className="relative space-y-3">
            {levels.map((level: any, idx: number) => {
              const isCompleted = level.state === 'completed';
              const isLocked = level.state === 'locked';
              const isCurrent = recommended?.id === level.id;
              return (
                <div key={level.id} className="relative flex gap-3">
                  {idx < levels.length - 1 && (
                    <div className={clsx('absolute left-[23px] top-12 h-[calc(100%-20px)] w-0.5', isCompleted ? 'bg-success/50' : 'bg-line')} />
                  )}
                  <div className="relative z-10 flex flex-col items-center">
                    <button
                      onClick={() => openLevel(level)}
                      className={clsx(
                        'flex h-12 w-12 items-center justify-center rounded-2xl border-2 transition-all',
                        isCompleted
                          ? 'border-success bg-success text-white'
                          : isCurrent
                            ? 'border-brand-600 bg-brand-600 text-white shadow-glow'
                            : isLocked
                              ? 'border-line bg-panel2 text-muted'
                              : 'border-brand-200 bg-panel text-brand-600 dark:border-brand-500/30',
                      )}
                      aria-label={level.title}
                    >
                      <Icon name={isLocked ? 'lock' : isCompleted ? 'check' : level.icon || 'flag'} />
                    </button>
                  </div>

                  <button
                    onClick={() => openLevel(level)}
                    className={clsx(
                      'card flex-1 p-4 text-left transition-all',
                      isCurrent && 'border-brand-500/60 ring-2 ring-brand-500/10',
                      isLocked && 'opacity-70',
                      !isLocked && 'hover:-translate-y-0.5 hover:shadow-glow',
                    )}
                  >
                    <div className="flex flex-wrap items-center gap-2">
                      <span className="label">{level.order_no}-{t('common.level')}</span>
                      {level.is_boss && (
                        <span className="chip bg-red-50 text-[9px] uppercase text-red-600 dark:bg-red-500/10 dark:text-red-300">
                          <Icon name="swords" className="text-[12px]" /> {t('course.boss')}
                        </span>
                      )}
                      {isCurrent && <span className="chip bg-brand-50 text-[9px] text-brand-700 dark:bg-brand-500/10 dark:text-brand-300">NEXT</span>}
                      <span className="ml-auto flex items-center gap-0.5">
                        {[1, 2, 3].map((s) => (
                          <Icon key={s} name="star" className={clsx('text-[16px]', s <= (level.stars ?? 0) ? 'text-amber' : 'text-line')} fill={s <= (level.stars ?? 0)} />
                        ))}
                      </span>
                    </div>
                    <div className="mt-1 font-semibold">{level.title}</div>
                    <div className="mt-0.5 line-clamp-1 text-xs text-muted">{level.description}</div>
                    <div className="mt-2 flex flex-wrap items-center gap-3 font-mono text-[10px] text-muted">
                      <span>{level.tasks_count} {t('common.tasks')}</span>
                      <span>{level.solved_tasks ?? 0} solved</span>
                      <span className="text-brand-600 dark:text-brand-300">+{level.xp_reward} XP</span>
                      {level.best_accuracy > 0 && <span>{t('common.accuracy')}: {Math.round(level.best_accuracy * 100)}%</span>}
                      {isLocked && <span className="inline-flex items-center gap-1"><Icon name="lock" className="text-[12px]" /> {t('course.locked_hint')}</span>}
                    </div>
                  </button>
                </div>
              );
            })}
          </div>
        </section>

        <aside className="space-y-4 xl:col-span-4">
          <div className="card p-4">
            <div className="label mb-2">AI · {t('ai.mastery')}</div>
            <div className="space-y-2">
              {(mastery.data?.topics ?? []).slice(0, 8).map((topic: any) => (
                <div key={topic.topic_id}>
                  <div className="flex items-center justify-between text-[11px]">
                    <span className="truncate pr-2">{topic.name}</span>
                    <span className="font-mono text-muted">{Math.round(topic.mastery * 100)}%</span>
                  </div>
                  <Progress className="mt-1 h-1.5" value={topic.mastery * 100} tone={topic.mastery > 0.7 ? 'success' : topic.mastery > 0.4 ? 'brand' : 'amber'} />
                </div>
              ))}
              {(mastery.data?.topics ?? []).length === 0 && <div className="text-xs text-muted">{t('common.empty')}</div>}
            </div>
          </div>

          <div className="card p-4">
            <div className="label mb-2">{t('course.enrolled')}</div>
            <div className="flex items-center justify-between text-sm">
              <span>{t('course.students')}</span>
              <span className="font-mono font-semibold">{data.course.students_count ?? '—'}</span>
            </div>
            <div className="mt-2 flex items-center justify-between text-sm">
              <span>{t('common.stars')}</span>
              <span className="font-mono font-semibold text-amber">{stars}</span>
            </div>
            <div className="mt-2 flex items-center justify-between text-sm">
              <span>{t('common.difficulty')}</span>
              <span className="font-mono font-semibold">{data.course.difficulty}/10</span>
            </div>
          </div>
        </aside>
      </div>
    </div>
  );
}
