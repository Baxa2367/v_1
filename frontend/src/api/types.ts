export interface Localized {
  kk: string;
  ru: string;
  en?: string;
}

export interface Course {
  id: string;
  slug: string;
  title: string;
  description: string;
  category: string;
  cover: string;
  difficulty: number;
  levels_count: number;
  tasks_count: number;
  enrolled: boolean;
  progress_pct: number;
  completed_levels: number;
  students_count?: number;
  tags?: string[];
}

export interface LevelProgress {
  status: string;
  stars: number;
  best_accuracy: number;
  xp_earned: number;
}

export interface Level {
  id: string;
  course_id: string;
  order_no: number;
  title: string;
  description: string;
  icon: string;
  is_boss: boolean;
  xp_reward: number;
  state: 'locked' | 'available' | 'completed' | 'in_progress';
  stars: number;
  best_accuracy: number;
  xp_earned: number;
  tasks_count?: number;
  solved_tasks?: number;
  unlock_rules?: Record<string, number>;
}

export interface TaskPayload {
  options?: { id: string; text: string }[];
  statement?: string;
  left?: { id: string; text: string }[];
  right?: { id: string; text: string }[];
  items?: { id: string; text: string }[];
  text?: string;
  blanks?: { id: string; options: { id: string; text: string }[] }[];
  unit?: string;
  language?: string;
  starter_code?: string;
  visible_tests?: { input: string; expected_output: string }[];
  placeholder?: string;
}

export interface Task {
  id: string;
  level_id: string;
  type: string;
  prompt: string;
  payload: TaskPayload;
  difficulty: number;
  base_xp: number;
  time_limit_sec: number;
  hints: { index: number; text: string }[];
  topic?: { code: string; name: string };
  my_attempts?: number;
  best_score?: number;
  extra_hints?: number;
}

export interface AttemptResult {
  attempt_id: string;
  result: 'correct' | 'partial' | 'wrong';
  score: number;
  suspicious: boolean;
  time_spent_ms: number;
  hints_used: number;
  grading: Record<string, any>;
  xp: {
    amount: number;
    total_xp: number;
    level: number;
    level_up: boolean;
    title: string;
    coins_earned: number;
    breakdown: Record<string, number>;
  };
  combo: number;
  streak: { current: number; longest: number; increased: boolean };
  daily_goal: { goal_xp: number; xp_earned: number; completed: boolean; bonus_xp: number };
  level: {
    completed: boolean;
    stars: number;
    accuracy: number;
    bonus_xp: number;
    first_completion: boolean;
    level_id: string;
    course_id: string;
    level_title: string;
    is_boss: boolean;
    tasks_solved: number;
    tasks_total: number;
  };
  achievements: any[];
  next_task_id: string | null;
  explanation: string;
  answer_key: Record<string, any>;
  level_finished?: boolean;
}

export interface LeaderboardEntry {
  rank: number;
  student_id: string;
  nickname: string;
  avatar: string;
  level: number;
  title: string;
  league: string;
  score: number;
  delta: number;
  is_me: boolean;
}

export interface Leaderboard {
  scope: string;
  entries: LeaderboardEntry[];
  total: number;
  me: { rank: number; score: number };
}

export interface Achievement {
  id: string;
  code: string;
  category: string;
  rarity: 'common' | 'rare' | 'epic' | 'legendary';
  title: string;
  description: string;
  icon: string;
  points: number;
  reward_coins: number;
  awarded: boolean;
  awarded_at?: string;
  progress: number;
  progress_value: number;
  target_value: number;
  is_secret?: boolean;
}

export interface Quest {
  id: string;
  code: string;
  title: string;
  description: string;
  kind: 'daily' | 'weekly';
  icon: string;
  progress: number;
  target: number;
  status: 'active' | 'completed' | 'claimed';
  reward_xp: number;
  reward_coins: number;
}

export interface Dashboard {
  profile: any;
  today: { goal_xp: number; xp_earned: number; completed: boolean };
  rank: { global: number; weekly: number; weekly_xp: number };
  continue: any;
  quests: { quests: Quest[]; daily_completed: number; daily_total: number; bonus_available: boolean };
  streak: { current: number; longest: number; xp_today: number };
  recent_achievements: any[];
  tip: { kind: string; text: string } | null;
  active_season: any;
}

export interface TopicStat {
  code: string;
  name: string;
  mastery: number;
  attempts: number;
  accuracy: number;
  avg_time_ms: number;
}
