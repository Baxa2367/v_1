const BASE = '/api/v1';

export type ApiError = { code: string; message: string; details?: Record<string, unknown>; status: number };

let accessToken: string | null = localStorage.getItem('lumina.access');
let refreshToken: string | null = localStorage.getItem('lumina.refresh');

export function getAccessToken() {
  return accessToken;
}

export function setTokens(access: string | null, refresh: string | null) {
  accessToken = access;
  refreshToken = refresh;
  if (access) localStorage.setItem('lumina.access', access);
  else localStorage.removeItem('lumina.access');
  if (refresh) localStorage.setItem('lumina.refresh', refresh);
  else localStorage.removeItem('lumina.refresh');
}

export function clearTokens() {
  setTokens(null, null);
}

async function parse(res: Response) {
  const text = await res.text();
  let body: any = null;
  try {
    body = text ? JSON.parse(text) : null;
  } catch {
    body = { raw: text };
  }
  if (!res.ok) {
    const err = body?.error ?? {};
    const apiError: ApiError = {
      status: res.status,
      code: err.code ?? 'http_error',
      message: err.message ?? `HTTP ${res.status}`,
      details: err.details,
    };
    throw apiError;
  }
  return body?.data;
}

let refreshPromise: Promise<boolean> | null = null;

async function refreshTokens(): Promise<boolean> {
  if (!refreshToken) return false;
  if (!refreshPromise) {
    refreshPromise = (async () => {
      try {
        const res = await fetch(`${BASE}/auth/refresh`, {
          method: 'POST',
          headers: { 'content-type': 'application/json' },
          body: JSON.stringify({ refresh_token: refreshToken }),
        });
        if (!res.ok) return false;
        const json = await res.json();
        const data = json.data;
        setTokens(data.access_token, data.refresh_token);
        return true;
      } catch {
        return false;
      } finally {
        refreshPromise = null;
      }
    })();
  }
  return refreshPromise;
}

type Options = {
  method?: string;
  body?: unknown;
  params?: Record<string, string | number | boolean | undefined | null>;
  raw?: boolean;
};

export async function api<T = any>(path: string, opts: Options = {}): Promise<T> {
  const url = new URL(BASE + path, window.location.origin);
  if (opts.params) {
    for (const [k, v] of Object.entries(opts.params)) {
      if (v !== undefined && v !== null && v !== '') url.searchParams.set(k, String(v));
    }
  }
  const headers: Record<string, string> = {};
  if (opts.body !== undefined) headers['content-type'] = 'application/json';
  if (accessToken) headers.authorization = `Bearer ${accessToken}`;

  let res = await fetch(url.toString(), {
    method: opts.method ?? 'GET',
    headers,
    body: opts.body !== undefined ? JSON.stringify(opts.body) : undefined,
  });

  if (res.status === 401 && refreshToken) {
    const ok = await refreshTokens();
    if (ok) {
      headers.authorization = `Bearer ${accessToken}`;
      res = await fetch(url.toString(), {
        method: opts.method ?? 'GET',
        headers,
        body: opts.body !== undefined ? JSON.stringify(opts.body) : undefined,
      });
    }
  }
  return parse(res);
}

export async function download(path: string, params: Record<string, string>) {
  const url = new URL(BASE + path, window.location.origin);
  for (const [k, v] of Object.entries(params)) url.searchParams.set(k, v);
  const res = await fetch(url.toString(), {
    headers: accessToken ? { authorization: `Bearer ${accessToken}` } : {},
  });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  const blob = await res.blob();
  const link = document.createElement('a');
  link.href = URL.createObjectURL(blob);
  link.download = `lumina-${params.type ?? 'report'}.${params.format ?? 'csv'}`;
  link.click();
  URL.revokeObjectURL(link.href);
}
