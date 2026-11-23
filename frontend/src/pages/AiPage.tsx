import { useState } from 'react';
import { useMutation, useQuery } from '@tanstack/react-query';
import { useNavigate } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import clsx from 'clsx';
import { api } from '../api/client';
import { ErrorState, Icon, Progress, SectionHeader, Spinner, useToasts } from '../components/ui';

export default function AiPage() {
  const { t } = useTranslation();
  const navigate = useNavigate();
  const push = useToasts((s) => s.push);
  const [courseId, setCourseId] = useState<string>('');
  const [preview, setPreview] = useState<any>(null);

  const courses = useQuery({ queryKey: ['courses', 'ai'], queryFn: () => api('/courses') });
  const activeCourse = courseId || courses.data?.[0]?.id;

  const forecast = useQuery({ queryKey: ['ai', 'forecast'], queryFn: () => api('/ai/forecast') });
  const tips = useQuery({ queryKey: ['ai', 'tips'], queryFn: () => api('/ai/tips') });
  const mastery = useQuery({
    queryKey: ['ai', 'mastery', activeCourse],
    queryFn: () => api('/ai/mastery', { params: { course_id: activeCourse } }),
    enabled: !!activeCourse,
  });

  const next = useMutation({
    mutationFn: () => api('/ai/next-task', { params: { course_id: activeCourse } }),
    onSuccess: (res: any) => setPreview(res),
    onError: (e: any) => push({ title: e.message, icon: 'error', tone: 'danger' }),
  });

  if (forecast.isLoading || tips.isLoading) return <Spinner />;
  if (forecast.error) return <ErrorState error={forecast.error} onRetry={() => forecast.refetch()} />;

  const f = forecast.data;
  const tip = tips.data?.tips?.[0];

  return (
    <div className="space-y-5">
      <section className="relative overflow-hidden rounded-2xl border border-sky-200 bg-gradient-to-br from-sky-50 via-white to-brand-50 p-6 dark:border-sky-500/20 dark:from-sky-500/10 dark:via-panel dark:to-brand-500/10">
        <div className="flex flex-wrap items-start justify-between gap-4">
          <div>
            <div className="flex items-center gap-2">
              <span className="flex h-10 w-10 items-center justify-center rounded-xl bg-sky-500 text-white shadow-glow">
                <Icon name="auto_awesome" fill />
              </span>
              <div>
                <div className="label text-sky-600 dark:text-sky-300">LUMINA AI</div>
                <h1 className="text-2xl font-extrabold tracking-tight">{t('ai.title')}</h1>
              </div>
            </div>
            <p className="mt-2 max-w-xl text-sm text-muted">{tip?.text ?? t('auth.feature2')}</p>
          </div>
          <button className="btn-primary" onClick={() => next.mutate()} disabled={next.isPending || !activeCourse}>
            <Icon name="auto_awesome" /> {t('ai.start')}
          </button>
        </div>
      </section>

      {preview && (
        <section className="card border-sky-300 p-5 dark:border-sky-500/40">
          <div className="flex items-start justify-between gap-4">
            <div className="min-w-0">
              <div className="flex flex-wrap items-center gap-2">
                <span className="chip bg-sky-100 text-sky-700 dark:bg-sky-500/20 dark:text-sky-300">
                  {t('ai.why')}
                </span>
                <span className="chip bg-panel2 text-[10px] uppercase text-muted">{preview.decision?.reason_code}</span>
                <span className="chip bg-panel2 text-[10px] text-muted">{Math.round((preview.decision?.expected_success ?? 0) * 100)}% success</span>
              </div>
              <p className="mt-2 text-sm">{preview.decision?.explanation}</p>
              <div className="mt-3 rounded-xl bg-panel2 p-3">
                <div className="text-xs font-semibold text-muted">{preview.task?.topic?.name}</div>
                <div className="mt-1 text-sm font-medium">{preview.task?.prompt}</div>
                <div className="mt-2 flex gap-3 font-mono text-[10px] text-muted">
                  <span>{t('common.difficulty')} {preview.task?.difficulty}/10</span>
                  <span>+{preview.task?.base_xp} XP</span>
                  <span>{preview.task?.time_limit_sec}s</span>
                </div>
              </div>
            </div>
            <div className="flex shrink-0 flex-col gap-2">
              <button className="btn-success" onClick={() => navigate(`/tasks/${preview.task.id}`)}>
                {t('common.start')} <Icon name="arrow_forward" />
              </button>
              <a className="btn-ghost text-xs" href={`/ai/explain/${preview.decision.id}`} onClick={(e) => e.preventDefault()}>
                {t('ai.explain')}
              </a>
            </div>
          </div>
        </section>
      )}

      <div className="grid grid-cols-1 gap-4 sm:grid-cols-3">
        {[
          ['trending_up', t('ai.days_to_level'), f.days_to_next_level > 0 ? `${f.days_to_next_level} d` : '—', `${f.xp_remaining} XP left`, 'brand'],
          ['warning', t('ai.risk'), `${Math.round(f.dropout_risk * 100)}%`, `${f.days_inactive} days inactive`, f.dropout_risk > 0.5 ? 'danger' : 'success'],
          ['flag', t('ai.recommended_goal'), `${f.recommended_daily_goal} XP`, `${Math.round(f.daily_avg_xp)} avg/day`, 'sky'],
        ].map(([icon, label, value, sub, tone]: any) => (
          <div key={label} className="card p-4">
            <div className="flex items-start justify-between">
              <div>
                <div className="label">{label}</div>
                <div className="mt-1 font-mono text-2xl font-bold">{value}</div>
                <div className="text-[11px] text-muted">{sub}</div>
              </div>
              <span className={clsx('flex h-10 w-10 items-center justify-center rounded-xl', tone === 'danger' ? 'bg-red-50 text-red-600' : tone === 'success' ? 'bg-emerald-50 text-emerald-600' : tone === 'sky' ? 'bg-sky-50 text-sky-600' : 'bg-brand-50 text-brand-600')}>
                <Icon name={icon} />
              </span>
            </div>
          </div>
        ))}
      </div>

      <div className="grid grid-cols-1 gap-5 xl:grid-cols-12">
        <section className="card p-5 xl:col-span-7">
          <div className="flex flex-wrap items-center justify-between gap-2">
            <SectionHeader title={t('ai.mastery')} subtitle={courses.data?.find((c: any) => c.id === activeCourse)?.title} />
            <select className="input max-w-[220px] py-1.5 text-xs" value={activeCourse ?? ''} onChange={(e) => setCourseId(e.target.value)}>
              {(courses.data ?? []).map((c: any) => (
                <option key={c.id} value={c.id}>{c.title}</option>
              ))}
            </select>
          </div>
          <div className="space-y-3">
            {(mastery.data?.topics ?? []).map((topic: any) => (
              <div key={topic.topic_id}>
                <div className="flex items-center justify-between text-xs">
                  <span className="truncate pr-2 font-medium">{topic.name}</span>
                  <span className="font-mono text-muted">{Math.round(topic.mastery * 100)}% · {topic.attempts} att</span>
                </div>
                <Progress className="mt-1 h-1.5" value={topic.mastery * 100} tone={topic.mastery > 0.7 ? 'success' : topic.mastery > 0.4 ? 'brand' : 'amber'} />
              </div>
            ))}
            {(mastery.data?.topics ?? []).length === 0 && <div className="py-6 text-center text-xs text-muted">{t('common.empty')}</div>}
          </div>
        </section>

        <section className="card p-5 xl:col-span-5">
          <SectionHeader title={t('dash.ai_tip')} subtitle={tips.data?.kind} />
          <div className="space-y-2">
            {(tips.data?.tips ?? []).slice(0, 4).map((tip: any) => (
              <div key={tip.id} className="flex gap-2 rounded-xl border border-line p-3">
                <Icon name="lightbulb" className="text-amber" fill />
                <div>
                  <div className="text-[10px] uppercase text-muted">{tip.kind}</div>
                  <p className="mt-0.5 text-xs leading-relaxed">{tip.text}</p>
                </div>
              </div>
            ))}
          </div>
        </section>
      </div>
    </div>
  );
}
