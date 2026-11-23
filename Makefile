SHELL := /bin/bash
BUILD ?= build
CMAKE ?= cmake
NINJA ?= -G Ninja
JOBS ?= 8

.PHONY: build test migrate seed seed-reset server worker dev frontend demo docker clean

build:
	$(CMAKE) -S backend -B $(BUILD) $(NINJA) -DCMAKE_BUILD_TYPE=Release
	$(CMAKE) --build $(BUILD) -j $(JOBS)

test: build
	ctest --test-dir $(BUILD) --output-on-failure

migrate: build
	./$(BUILD)/lumina_migrator up

seed: build
	./$(BUILD)/lumina_seeder

seed-reset: build
	./$(BUILD)/lumina_seeder --reset

server: build
	./$(BUILD)/lumina_server

worker: build
	./$(BUILD)/lumina_worker

frontend:
	cd frontend && npm install && npm run build

dev: build
	@echo "Run 'make server' and 'cd frontend && npm run dev' in two terminals"

demo:
	node scripts/demo.mjs

docker:
	docker compose -f docker/docker-compose.yml up --build

clean:
	rm -rf $(BUILD)
