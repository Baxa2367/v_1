import { useState } from 'react';
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { useTranslation } from 'react-i18next';
import clsx from 'clsx';
import { api } from '../api/client';
import { Avatar, ErrorState, Icon, Modal, Progress, SectionHeader, Spinner, useToasts } from '../components/ui';

export default function TeacherPage() {
  const { t } = useTranslation();
  const queryClient = useQueryClient();
  const push = useToasts((s) => s.push);
  const [selected, setSelected] = useState<string>('');
  const [open, setOpen] = useState(false);
  const [titleKk, setTitleKk] = useState('');
  const [titleRu, setTitleRu] = useState('');
  const [desc, setDesc] = useState('');
  const [category, setCategory] = useState('programming');

  const courses = useQuery({ queryKey: ['teacher', 'courses'], queryFn: () => api('/teacher/courses') });
  const courseId = selected || courses.data?.courses?.[0]?.id;
  const analytics = useQuery({
    queryKey: ['teacher', 'analytics', courseId],
    queryFn: () => api(`/teacher/courses/${courseId}/analytics`),
    enabled: !!courseId,
  });

  const create = useMutation({
    mutationFn: () =>
      api('/teacher/courses', {
        method: 'POST',
        body: {
          title: { kk: titleKk, ru: titleRu || titleKk, en: titleRu || titleKk },
          description: { kk: desc, ru: desc, en: desc },
          category,
          difficulty: 3,
          slug: titleRu || titleKk,
        },
      }),
    onSuccess: () => {
      setOpen(false);
      setTitleKk('');
      setTitleRu('');
      setDesc('');
      queryClient.invalidateQueries({ queryKey: ['teacher', 'courses'] });
      push({ title: t('common.saved'), icon: 'school', tone: 'success' });
    },
    onError: (e: any) => push({ title: e.message, icon: 'error', tone: 'danger' }),
  });

  if (courses.isLoading) return <Spinner />;
  if (courses.error) return <ErrorState error={courses.error} onRetry={() => courses.refetch()} />;

  const a = analytics.data;

  return (
    <div className="space-y-5">
      <section className="flex flex-col gap-3 sm:flex-row sm:items-end sm:justify-between">
        <div>
          <div className="label">TEACHER · ANALYTICS</div>
          <h1 className="text-3xl font-extrabold tracking-tight">{t('teacher.title')}</h1>
        </div>
        <button className="btn-primary" onClick={() => setOpen(true)}>
          <Icon name="add" /> {t('teacher.new_course')}
        </button>
      </section>

      <section className="flex flex-wrap gap-2">
        {(courses.data?.courses ?? []).map((c: any) => (
          <button
            key={c.id}
            onClick={() => setSelected(c.id)}
            className={clsx('card px-4 py-3 text-left transition-all', courseId === c.id ? 'border-brand-500 ring-2 ring-brand-500/10' : 'hover:-translate-y-0.5')}
          >
            <div className="text-sm font-semibold">{c.title}</div>
            <div className="mt-1 flex gap-3 font-mono text-[10px] text-muted">
              <span>{c.levels} lv</span>
              <span>{c.students} st</span>
              <span className={c.status === 'published' ? 'text-success' : 'text-amber'}>{c.status}</span>
            </div>
          </button>
        ))}
      </section>

      {analytics.isLoading && <Spinner />}
      {analytics.error && <ErrorState error={analytics.error} onRetry={() => analytics.refetch()} />}

      {a && (
        <>
          <section className="grid grid-cols-2 gap-3 lg:grid-cols-4">
            {[
              ['groups', t('teacher.students'), a.summary.students, ''],
              ['warning', t('teacher.at_risk'), a.summary.at_risk, 'amber'],
              ['target', t('teacher.avg_accuracy'), `${a.summary.avg_accuracy}%`, 'success'],
              ['menu_book', t('teacher.weak_topics'), a.weak_topics.length, 'sky'],
            ].map(([icon, label, value, tone]: any) => (
              <div key={label} className="card p-4">
                <div className="flex items-start justify-between">
                  <div>
                    <div className="label">{label}</div>
                    <div className="stat-value mt-1">{value}</div>
                  </div>
                  <span className={clsx('flex h-9 w-9 items-center justify-center rounded-xl', tone === 'amber' ? 'bg-amber-50 text-amber-600' : tone === 'success' ? 'bg-emerald-50 text-emerald-600' : tone === 'sky' ? 'bg-sky-50 text-sky-600' : 'bg-brand-50 text-brand-600')}>
                    <Icon name={icon} />
                  </span>
                </div>
              </div>
            ))}
          </section>

          <div className="grid grid-cols-1 gap-5 xl:grid-cols-12">
            <section className="card overflow-hidden xl:col-span-8">
              <div className="p-5 pb-2">
                <SectionHeader title={t('teacher.students')} subtitle={a.course?.title} />
              </div>
              <div className="max-h-[460px] overflow-auto">
                <table className="w-full text-left text-sm">
                  <thead className="sticky top-0 bg-panel text-[10px] uppercase tracking-wider text-muted">
                    <tr>
                      <th className="px-5 py-2 font-medium">Student</th>
                      <th className="px-3 py-2 font-medium">Lv</th>
                      <th className="px-3 py-2 font-medium">Progress</th>
                      <th className="px-3 py-2 font-medium">Acc</th>
                      <th className="px-3 py-2 font-medium">XP</th>
                      <th className="px-3 py-2 font-medium"> </th>
                    </tr>
                  </thead>
                  <tbody>
                    {(a.students ?? []).map((s: any) => (
                      <tr key={s.user_id} className="border-t border-line/60">
                        <td className="px-5 py-2.5">
                          <div className="flex items-center gap-2">
                            <Avatar name={s.nickname} size={26} />
                            <span className="font-medium">{s.nickname}</span>
                          </div>
                        </td>
                        <td className="px-3 py-2.5 font-mono text-xs">{s.level}</td>
                        <td className="px-3 py-2.5">
                          <div className="flex items-center gap-2">
                            <Progress className="h-1.5 w-20" value={s.progress_pct ?? 0} />
                            <span className="font-mono text-[10px] text-muted">{Math.round(s.progress_pct ?? 0)}%</span>
                          </div>
                        </td>
                        <td className="px-3 py-2.5 font-mono text-xs">{s.accuracy ?? 0}%</td>
                        <td className="px-3 py-2.5 font-mono text-xs">{s.total_xp}</td>
                        <td className="px-3 py-2.5">
                          {s.at_risk ? <span className="chip bg-red-50 text-[9px] text-red-600 dark:bg-red-500/10 dark:text-red-300">risk</span> : <span className="chip bg-emerald-50 text-[9px] text-emerald-600 dark:bg-emerald-500/10">ok</span>}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </section>

            <section className="space-y-4 xl:col-span-4">
              <div className="card p-5">
                <SectionHeader title={t('teacher.weak_topics')} />
                <div className="space-y-2">
                  {(a.weak_topics ?? []).slice(0, 6).map((w: any) => (
                    <div key={w.name} className="flex items-center justify-between rounded-xl border border-line px-3 py-2">
                      <span className="truncate text-xs font-medium">{w.name}</span>
                      <span className="font-mono text-xs text-red-500">{w.accuracy}%</span>
                    </div>
                  ))}
                  {(a.weak_topics ?? []).length === 0 && <div className="text-xs text-muted">{t('common.empty')}</div>}
                </div>
              </div>
              <div className="card p-5">
                <SectionHeader title={t('teacher.at_risk')} />
                <div className="space-y-2">
                  {(a.risk ?? []).slice(0, 5).map((s: any) => (
                    <div key={s.user_id} className="flex items-center gap-2 rounded-xl bg-red-50/60 p-2 dark:bg-red-500/10">
                      <Avatar name={s.nickname} size={26} />
                      <div className="min-w-0 flex-1">
                        <div className="truncate text-xs font-semibold">{s.nickname}</div>
                        <div className="text-[10px] text-muted">{s.days_inactive} d inactive · {Math.round(s.progress_pct ?? 0)}%</div>
                      </div>
                    </div>
                  ))}
                  {(a.risk ?? []).length === 0 && <div className="text-xs text-muted">{t('common.empty')}</div>}
                </div>
              </div>
            </section>
          </div>
        </>
      )}

      <Modal open={open} onClose={() => setOpen(false)} title={t('teacher.new_course')}>
        <div className="space-y-3">
          <div>
            <label className="label mb-1 block">Атауы (қазақша)</label>
            <input className="input" value={titleKk} onChange={(e) => setTitleKk(e.target.value)} />
          </div>
          <div>
            <label className="label mb-1 block">Название (русский)</label>
            <input className="input" value={titleRu} onChange={(e) => setTitleRu(e.target.value)} />
          </div>
          <div>
            <label className="label mb-1 block">Описание</label>
            <textarea className="input min-h-[80px]" value={desc} onChange={(e) => setDesc(e.target.value)} />
          </div>
          <div>
            <label className="label mb-1 block">Категория</label>
            <select className="input" value={category} onChange={(e) => setCategory(e.target.value)}>
              <option value="programming">Программирование</option>
              <option value="math">Математика</option>
              <option value="database">Базы данных</option>
              <option value="language">Языки</option>
            </select>
          </div>
          <button className="btn-primary w-full" disabled={!titleKk || create.isPending} onClick={() => create.mutate()}>
            {t('common.save')}
          </button>
        </div>
      </Modal>
    </div>
  );
}
