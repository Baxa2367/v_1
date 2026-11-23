import { useQuery } from '@tanstack/react-query';
import { Link, useNavigate, useParams } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import clsx from 'clsx';
import { api } from '../api/client';
import { ErrorState, Icon, Progress, Spinner, useToasts } from '../components/ui';

const TYPE_ICON: Record<string, string> = {
  single_choice: 'radio_button_checked',
  multiple_choice: 'checklist',
  true_false: 'toggle_on',
  short_answer: 'edit_note',
  matching: 'compare_arrows',
  ordering: 'format_list_numbered',
  cloze: 'format_color_text',
  numeric: 'pin',
  timed_quiz: 'timer',
  code: 'code',
};

export default function LevelPage() {
  const { id } = useParams();
  const { t } = useTranslation();
  const navigate = useNavigate();
  const push = useToasts((s) => s.push);

  const level = useQuery({ queryKey: ['level', id], queryFn: () => api(`/levels/${id}`) });

  if (level.isLoading) return <Spinner />;
  if (level.error) return <ErrorState error={level.error} onRetry={() => level.refetch()} />;

  const data = level.data;
  const tasks = data.tasks ?? [];
  const solved = tasks.filter((x: any) => x.solved).length;
  const isLocked = data.state === 'locked';

  const start = async () => {
    if (isLocked) return;
    try {
      const next = await api(`/levels/${id}/next-task`);
      navigate(`/tasks/${next.id}?level=${id}`);
    } catch (e: any) {
      push({ title: e.message, icon: 'info', tone: 'amber' });
    }
  };

  return (
    <div className="space-y-5">
      <section className="flex items-center gap-2 text-xs text-muted">
        <Link to="/courses" className="hover:text-ink">{t('nav.courses')}</Link>
        <Icon name="chevron_right" className="text-[14px]" />
        <Link to={`/courses/${data.course.id}`} className="hover:text-ink">{data.course.title}</Link>
        <Icon name="chevron_right" className="text-[14px]" />
        <span className="font-medium text-ink">{data.level.title}</span>
      </section>

      <section className="card overflow-hidden">
        <div className="relative hero-gradient p-6 text-white">
          <div className="flex flex-wrap items-start justify-between gap-4">
            <div className="max-w-xl">
              <div className="flex flex-wrap items-center gap-2">
                <span className="chip bg-white/15 uppercase tracking-wider text-white">
                  {data.level.order_no}-{t('common.level')}
                </span>
                {data.level.is_boss && <span className="chip bg-red-400/30 text-white">BOSS</span>}
                <span className="chip bg-white/15 text-white">{data.level.xp_reward} XP</span>
              </div>
              <h1 className="mt-2 text-2xl font-extrabold tracking-tight">{data.level.title}</h1>
              <p className="mt-1 text-sm text-white/75">{data.level.description}</p>
            </div>
            <div className="flex flex-col items-end gap-2">
              <div className="flex items-center gap-0.5">
                {[1, 2, 3].map((s) => (
                  <Icon key={s} name="star" className={clsx('text-[26px]', s <= (data.level.stars ?? 0) ? 'text-amber' : 'text-white/30')} fill={s <= (data.level.stars ?? 0)} />
                ))}
              </div>
              <button className="btn bg-white text-brand-700 hover:bg-white/90" onClick={start} disabled={isLocked}>
                {data.state === 'completed' ? t('level.replay') : t('course.start')} <Icon name="play_arrow" />
              </button>
            </div>
          </div>
          <div className="mt-5 flex flex-wrap items-center gap-4 font-mono text-[11px] text-white/80">
            <span>{solved}/{tasks.length} solved</span>
            <span>{t('common.accuracy')}: {Math.round((data.level.best_accuracy ?? 0) * 100)}%</span>
            <span>+{data.progress?.xp_earned ?? 0} XP</span>
          </div>
          <Progress className="mt-3 bg-white/20" value={(solved / Math.max(1, tasks.length)) * 100} />
        </div>
      </section>

      {isLocked && (
        <div className="card flex items-center gap-3 border-amber-200 bg-amber-50/60 p-4 text-amber-800 dark:border-amber-500/30 dark:bg-amber-500/10 dark:text-amber-200">
          <Icon name="lock" /> {t('course.locked_hint')}
        </div>
      )}

      <section className="space-y-2.5">
        {tasks.map((task: any, i: number) => (
          <button
            key={task.id}
            onClick={() => !isLocked && navigate(`/tasks/${task.id}?level=${id}`)}
            disabled={isLocked}
            className={clsx(
              'card flex w-full items-center gap-3 p-3.5 text-left transition-all',
              !isLocked && 'hover:-translate-y-0.5 hover:border-brand-500/40',
              task.solved && 'border-emerald-200 dark:border-emerald-500/30',
            )}
          >
            <span className="flex h-9 w-9 items-center justify-center rounded-xl bg-panel2 font-mono text-xs font-bold text-muted">
              {i + 1}
            </span>
            <span className="flex h-9 w-9 items-center justify-center rounded-xl bg-brand-50 text-brand-600 dark:bg-brand-500/10 dark:text-brand-300">
              <Icon name={TYPE_ICON[task.type] ?? 'help'} />
            </span>
            <div className="min-w-0 flex-1">
              <div className="flex flex-wrap items-center gap-2">
                <span className="text-sm font-semibold">{t(`task.type.${task.type}`, { defaultValue: task.type.replace('_', ' ') })}</span>
                <span className="chip bg-panel2 text-[9px] uppercase text-muted">{task.type}</span>
                {task.solved && <span className="chip bg-emerald-100 text-[9px] text-emerald-700 dark:bg-emerald-500/20 dark:text-emerald-300">✓</span>}
              </div>
              <div className="mt-1 flex flex-wrap items-center gap-3 font-mono text-[10px] text-muted">
                <span>{t('common.difficulty')} {task.difficulty}/10</span>
                <span>+{task.base_xp} XP</span>
                <span>{task.time_limit_sec}s</span>
                <span>{t('common.accuracy')} {Math.round((task.best_score ?? 0) * 100)}%</span>
              </div>
            </div>
            <Icon name={task.solved ? 'check_circle' : 'chevron_right'} className={task.solved ? 'text-success' : 'text-muted'} />
          </button>
        ))}
      </section>
    </div>
  );
}
