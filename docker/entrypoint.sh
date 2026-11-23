#!/bin/sh
set -e

echo "[lumina] waiting for postgres..."
attempt=0
if [ -n "${DATABASE_URL:-}" ]; then
  until pg_isready -d "$DATABASE_URL" >/dev/null 2>&1; do
    attempt=$((attempt + 1))
    if [ "$attempt" -gt 60 ]; then
      echo "[lumina] postgres is not reachable" >&2
      exit 1
    fi
    sleep 1
  done
else
  until pg_isready -h "${PGHOST:-db}" -p "${PGPORT:-5432}" -U "${PGUSER:-lumina}" >/dev/null 2>&1; do
    attempt=$((attempt + 1))
    if [ "$attempt" -gt 60 ]; then
      echo "[lumina] postgres is not reachable" >&2
      exit 1
    fi
    sleep 1
  done
fi

if [ "${RUN_MIGRATIONS:-1}" = "1" ]; then
  lumina_migrator up
fi

if [ "${RUN_SEED:-0}" = "1" ]; then
  lumina_seeder
fi

exec "$@"
