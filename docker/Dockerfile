# syntax=docker/dockerfile:1.6
# ---------------------------------------------------------------------------
# Lumina — single image with the C++ API, worker and the built React client.
# Debian 13 (trixie) ships Drogon, libpqxx and all other dependencies.
# ---------------------------------------------------------------------------
FROM node:20-alpine AS web
WORKDIR /web
COPY frontend/package*.json ./
RUN npm ci --no-audit --no-fund || npm install --no-audit --no-fund
COPY frontend/ ./
RUN npm run build

FROM debian:trixie-slim AS runtime
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build pkg-config ca-certificates curl \
      libdrogon-dev libpqxx-dev libjsoncpp-dev libsodium-dev nlohmann-json3-dev \
      libspdlog-dev libxlsxwriter-dev libhpdf-dev libssl-dev libcurl4-openssl-dev \
      postgresql-client \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY backend/ ./backend/
RUN cmake -S backend -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLUMINA_BUILD_TESTS=OFF \
    && cmake --build build -j2 \
    && cp build/lumina_server build/lumina_migrator build/lumina_seeder build/lumina_worker /usr/local/bin/ \
    && rm -rf build

COPY --from=web /web/dist ./public
COPY db/ ./db/
COPY seeds/ ./seeds/
COPY docs/ ./docs/
COPY docker/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

ENV PORT=8080 \
    PUBLIC_DIR=/app/public \
    MIGRATIONS_DIR=/app/db/migrations \
    SEEDS_DIR=/app/seeds \
    APP_TIMEZONE=Asia/Qyzylorda

EXPOSE 8080
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["lumina_server"]
