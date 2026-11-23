import { lazy, Suspense, useEffect } from 'react';
import { Navigate, Route, Routes, useLocation } from 'react-router-dom';
import { useAuth } from './store/auth';
import Layout from './components/Layout';
import { Toaster } from './components/ui';
const AuthPage = lazy(() => import('./pages/AuthPage'));
const DashboardPage = lazy(() => import('./pages/DashboardPage'));
const CoursesPage = lazy(() => import('./pages/CoursesPage'));
const CourseMapPage = lazy(() => import('./pages/CourseMapPage'));
const LevelPage = lazy(() => import('./pages/LevelPage'));
const TaskPage = lazy(() => import('./pages/TaskPage'));
const LeaderboardPage = lazy(() => import('./pages/LeaderboardPage'));
const AchievementsPage = lazy(() => import('./pages/AchievementsPage'));
const StatsPage = lazy(() => import('./pages/StatsPage'));
const ShopPage = lazy(() => import('./pages/ShopPage'));
const TeamsPage = lazy(() => import('./pages/TeamsPage'));
const SeasonPage = lazy(() => import('./pages/SeasonPage'));
const ProfilePage = lazy(() => import('./pages/ProfilePage'));
const NotificationsPage = lazy(() => import('./pages/NotificationsPage'));
const AiPage = lazy(() => import('./pages/AiPage'));
const TeacherPage = lazy(() => import('./pages/TeacherPage'));
const AdminPage = lazy(() => import('./pages/AdminPage'));

function Protected({ children }: { children: JSX.Element }) {
  const token = useAuth((s) => s.accessToken) || localStorage.getItem('lumina.access');
  const location = useLocation();
  if (!token) return <Navigate to="/login" state={{ from: location }} replace />;
  return children;
}

export default function App() {
  const { accessToken, refreshMe, profile } = useAuth();

  useEffect(() => {
    if (accessToken) refreshMe();
  }, [accessToken, refreshMe]);

  return (
    <>
      <Suspense
        fallback={
          <div className="grid min-h-screen place-items-center" role="status" aria-label="Loading page">
            <span className="h-8 w-8 animate-spin rounded-full border-2 border-brand-600 border-t-transparent" />
          </div>
        }
      >
        <Routes>
          <Route path="/login" element={<AuthPage mode="login" />} />
          <Route path="/register" element={<AuthPage mode="register" />} />
          <Route
            element={
              <Protected>
                <Layout />
              </Protected>
            }
          >
            <Route path="/" element={<Navigate to="/dashboard" replace />} />
            <Route path="/dashboard" element={<DashboardPage />} />
            <Route path="/courses" element={<CoursesPage />} />
            <Route path="/courses/:id" element={<CourseMapPage />} />
            <Route path="/levels/:id" element={<LevelPage />} />
            <Route path="/tasks/:id" element={<TaskPage />} />
            <Route path="/leaderboard" element={<LeaderboardPage />} />
            <Route path="/achievements" element={<AchievementsPage />} />
            <Route path="/stats" element={<StatsPage />} />
            <Route path="/shop" element={<ShopPage />} />
            <Route path="/teams" element={<TeamsPage />} />
            <Route path="/season" element={<SeasonPage />} />
            <Route path="/profile" element={<ProfilePage />} />
            <Route path="/notifications" element={<NotificationsPage />} />
            <Route path="/ai" element={<AiPage />} />
            <Route
              path="/teacher"
              element={profile && (profile.role === 'teacher' || profile.role === 'admin') ? <TeacherPage /> : <Navigate to="/dashboard" replace />}
            />
            <Route path="/admin" element={profile?.role === 'admin' ? <AdminPage /> : <Navigate to="/dashboard" replace />} />
            <Route path="*" element={<Navigate to="/dashboard" replace />} />
          </Route>
        </Routes>
      </Suspense>
      <Toaster />
    </>
  );
}
