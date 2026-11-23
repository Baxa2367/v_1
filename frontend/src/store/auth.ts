import { create } from 'zustand';
import { persist } from 'zustand/middleware';
import { api, clearTokens, setTokens } from '../api/client';

export type Locale = 'kk' | 'ru' | 'en';

export interface Profile {
  id: string;
  email: string;
  role: string;
  locale: Locale;
  display_name: string;
  nickname: string;
  avatar: string;
  bio?: string;
  total_xp: number;
  level: number;
  title: string;
  xp_in_level: number;
  xp_to_next: number;
  level_progress: number;
  coins: number;
  gems: number;
  current_streak: number;
  longest_streak: number;
  daily_goal_xp: number;
  league: string;
  privacy_flags?: Record<string, boolean>;
  showcase?: string[];
}

interface AuthState {
  accessToken: string | null;
  profile: Profile | null;
  locale: Locale;
  theme: 'light' | 'dark';
  setSession: (data: any) => void;
  setProfile: (p: Profile) => void;
  setLocale: (l: Locale) => void;
  toggleTheme: () => void;
  logout: () => Promise<void>;
  refreshMe: () => Promise<void>;
}

export const useAuth = create<AuthState>()(
  persist(
    (set, get) => ({
      accessToken: localStorage.getItem('lumina.access'),
      profile: null,
      locale: (localStorage.getItem('lumina.locale') as Locale) || 'kk',
      theme: (localStorage.getItem('lumina.theme') as 'light' | 'dark') || 'light',
      setSession: (data) => {
        setTokens(data.access_token, data.refresh_token);
        set({ accessToken: data.access_token, profile: data.profile ?? null });
        if (data.profile?.locale) {
          localStorage.setItem('lumina.locale', data.profile.locale);
          set({ locale: data.profile.locale });
        }
      },
      setProfile: (profile) => set({ profile }),
      setLocale: (locale) => {
        localStorage.setItem('lumina.locale', locale);
        set({ locale });
      },
      toggleTheme: () => {
        const theme = get().theme === 'light' ? 'dark' : 'light';
        localStorage.setItem('lumina.theme', theme);
        set({ theme });
      },
      logout: async () => {
        try {
          await api('/auth/logout', { method: 'POST', body: {} });
        } catch {
          /* ignore */
        }
        clearTokens();
        set({ accessToken: null, profile: null });
      },
      refreshMe: async () => {
        try {
          const me = await api('/me');
          const profile = me.profile ?? {
            ...me.user,
            nickname: me.user.display_name,
            avatar: 'admin_panel_settings',
            total_xp: 0,
            level: 1,
            title: 'Admin',
            coins: 0,
            current_streak: 0,
            longest_streak: 0,
            daily_goal_xp: 50,
            league: 'bronze',
          };
          set({ profile });
        } catch {
          /* handled by caller */
        }
      },
    }),
    {
      name: 'lumina-auth',
      partialize: (s) => ({ locale: s.locale, theme: s.theme, profile: s.profile }),
    },
  ),
);
