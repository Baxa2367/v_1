import { useMemo, useState } from 'react';
import { Link, useSearchParams } from 'react-router-dom';
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { useTranslation } from 'react-i18next';
import clsx from 'clsx';
import { api } from '../api/client';
import { ErrorState, Icon, Progress, SectionHeader, Spinner, useToasts } from '../components/ui';

const CATEGORIES = [
  { id: '', label: 'Барлығы', icon: 'apps' },
  { id: 'programming', label: 'Бағдарламалау', icon: 'terminal' },
  { id: 'math', label: 'Математика', icon: 'calculate' },
  { id: 'database', label: 'Деректер қоры', icon: 'database' },
  { id: 'language', label: 'Тілдер', icon: 'translate' },
];

export default function CoursesPage() {
  const { t } = useTranslation();
  const [params] = useSearchParams();
  const [category, setCategory] = useState('');
  const queryClient = useQueryClient();
  const push = useToasts((s) => s.push);
  const q = params.get('q') ?? '';

  const courses = useQuery({
    queryKey: ['courses', category, q],
    queryFn: () => api('/courses', { params: { category, q } }),
  });

  const enroll = useMutation({
    mutationFn: (id: string) => api(`/courses/${id}/enroll`, { method: 'POST', body: {} }),
    onSuccess: (_data, id) => {
      push({ title: t('course.enrolled'), icon: 'check_circle', tone: 'success' });
      queryClient.invalidateQueries({ queryKey: ['courses'] });
      queryClient.invalidateQueries({ queryKey: ['course', id] });
    },
    onError: (e: any) => push({ title: e.message, icon: 'error', tone: 'danger' }),
  });

  const list = useMemo(() => courses.data ?? [], [courses.data]);

  return (
    <div className="space-y-5">
      <section className="flex flex-col gap-3 lg:flex-row lg:items-end lg:justify-between">
        <div>
          <div className="label">{t('course.all_courses')}</div>
          <h1 className="text-3xl font-extrabold tracking-tight">{t('nav.courses')}</h1>
          <p className="mt-1 text-sm text-muted">
            {list.length} {t('common.tasks')} · {t('auth.feature1')}
          </p>
        </div>
        <div className="flex flex-wrap gap-1 rounded-xl bg-panel2 p-1">
          {CATEGORIES.map((c) => (
            <button
              key={c.id}
              onClick={() => setCategory(c.id)}
              className={clsx(
                'flex items-center gap-1.5 rounded-lg px-3 py-1.5 text-sm font-medium transition-all',
                category === c.id ? 'bg-panel shadow-sm' : 'text-muted hover:text-ink',
              )}
            >
              <Icon name={c.icon} className="text-[16px]" /> {c.label}
            </button>
          ))}
        </div>
      </section>

      {q && (
        <div className="text-sm text-muted">
          {t('course.search')}: <span className="font-semibold text-ink">“{q}”</span>
        </div>
      )}

      {courses.isLoading && <Spinner />}
      {courses.error && <ErrorState error={courses.error} onRetry={() => courses.refetch()} />}

      <div className="grid grid-cols-1 gap-4 md:grid-cols-2 xl:grid-cols-3">
        {list.map((c: any) => (
          <div key={c.id} className="card card-hover flex flex-col overflow-hidden">
            <div className="relative h-28 hero-gradient p-4 text-white">
              <div className="absolute -right-6 -top-8 h-32 w-32 rounded-full bg-white/10 blur-2xl" />
              <div className="flex items-start justify-between">
                <span className="flex h-10 w-10 items-center justify-center rounded-xl bg-white/15 backdrop-blur">
                  <Icon name={c.cover || 'school'} />
                </span>
                <span className="chip bg-white/15 text-[9px] uppercase tracking-wider text-white backdrop-blur">
                  {c.category}
                </span>
              </div>
              <div className="absolute bottom-3 left-4 right-4 flex items-center justify-between font-mono text-[10px] text-white/80">
                <span>{c.levels_count} {t('course.levels').toLowerCase()} · {c.tasks_count} {t('common.tasks')}</span>
                <span>★ {c.difficulty}/10</span>
              </div>
            </div>
            <div className="flex flex-1 flex-col p-4">
              <h3 className="text-base font-bold leading-snug">{c.title}</h3>
              <p className="mt-1 line-clamp-2 text-xs leading-relaxed text-muted">{c.description}</p>
              <div className="mt-3 flex items-center gap-2 text-[11px] text-muted">
                <span className="chip bg-panel2">{c.completed_levels}/{c.levels_count} {t('common.stars')}</span>
                {c.students_count > 0 && <span>{c.students_count} {t('course.students')}</span>}
              </div>
              {c.enrolled ? (
                <div className="mt-3">
                  <div className="mb-1 flex items-center justify-between text-[11px]">
                    <span className="text-muted">{t('course.progress')}</span>
                    <span className="font-mono font-semibold">{Math.round(c.progress_pct)}%</span>
                  </div>
                  <Progress value={c.progress_pct} />
                </div>
              ) : null}
              <div className="mt-4 flex gap-2">
                {c.enrolled ? (
                  <Link to={`/courses/${c.id}`} className="btn-primary flex-1">
                    {t('common.continue')} <Icon name="arrow_forward" />
                  </Link>
                ) : (
                  <>
                    <button className="btn-soft flex-1" onClick={() => enroll.mutate(c.id)} disabled={enroll.isPending}>
                      <Icon name="add" /> {t('course.enroll')}
                    </button>
                    <Link to={`/courses/${c.id}`} className="btn-ghost" aria-label={t('course.map')}>
                      <Icon name="map" />
                    </Link>
                  </>
                )}
              </div>
            </div>
          </div>
        ))}
      </div>

      {!courses.isLoading && list.length === 0 && (
        <div className="card p-10 text-center text-sm text-muted">{t('common.empty')}</div>
      )}
    </div>
  );
}
