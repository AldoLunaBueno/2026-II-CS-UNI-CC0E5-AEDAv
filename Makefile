CXX        := g++
CXXFLAGS   := -std=c++23 -O2
TARGET     := main
SRCS       := src/main.cpp src/demos/demos.cpp
HEADERS    := src/demos/demos.h src/types.h $(wildcard src/texcel/*.h) $(wildcard src/texcel/helpers/*.h) src/utilities/clipped_graph_renderer.h
TEST_SRCS  := $(wildcard tests/*/unit/*.cpp)
E2E_SRCS   := $(wildcard tests/*/e2e/*.py)
PR_OUT     := tmp/pr_comment.md
PR_SCRIPT  := scripts/gen_pr_comment.sh
DOCS_PORT  ?= 8000
DB_COMPOSE := db/compose.yml

.PHONY: all run texcel test test-unit test-e2e test-perf pr docs clean db-up db-run db-down

all: clean $(TARGET) run

$(TARGET): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

texcel: $(TARGET)
	./$(TARGET) --texcel

test: test-unit test-e2e

test-unit: $(HEADERS) $(TEST_SRCS)
	@for src in $(TEST_SRCS); do \
		bin="/tmp/$$(basename $$src .cpp)"; \
		echo "Running $$src..."; \
		$(CXX) $(CXXFLAGS) "$$src" -o "$$bin" && "$$bin" && rm -f "$$bin" || exit 1; \
	done

test-e2e: $(TARGET) $(E2E_SRCS)
	@for src in $(E2E_SRCS); do \
		echo "Running $$src..."; \
		python3 "$$src" "./$(TARGET)" || exit 1; \
	done

test-perf: $(HEADERS) tests/texcel/performance/gen_perf_sheets.py tests/texcel/performance/bench_texcel.cpp
	@mkdir -p tmp
	@python3 tests/texcel/performance/gen_perf_sheets.py
	@$(CXX) $(CXXFLAGS) -O2 -I. tests/texcel/performance/bench_texcel.cpp -o /tmp/bench_texcel && /tmp/bench_texcel && rm -f /tmp/bench_texcel

pr:
	@bash $(PR_SCRIPT) README.md $(PR_OUT)

docs:
	@mkdir -p build/docs
	doxygen Doxyfile
	@echo ""
	@echo "Documentacion generada. Abre: http://localhost:$(DOCS_PORT)"
	@echo "Presiona Ctrl+C para detener el servidor."
	@python3 -m http.server $(DOCS_PORT) -d build/docs/html

clean:
	rm -rf $(TARGET) tmp build/docs
	@clear 2>/dev/null || printf "\033c"

db-up:
	docker compose -f $(DB_COMPOSE) up -d --build --wait

db-run:
	docker compose -f $(DB_COMPOSE) exec -T postgres psql -U postgres -d aeda_indexes -v ON_ERROR_STOP=1 -f /scripts/01_hash.sql
	docker compose -f $(DB_COMPOSE) exec -T postgres psql -U postgres -d aeda_indexes -v ON_ERROR_STOP=1 -f /scripts/02_btree.sql
	docker compose -f $(DB_COMPOSE) exec -T postgres psql -U postgres -d aeda_indexes -v ON_ERROR_STOP=1 -f /scripts/03_gist.sql
	docker compose -f $(DB_COMPOSE) exec -T postgres psql -U postgres -d aeda_indexes -v ON_ERROR_STOP=1 -f /scripts/04_btree_cpp.sql
	docker compose -f $(DB_COMPOSE) exec -T postgres psql -U postgres -d aeda_indexes -v ON_ERROR_STOP=1 -f /scripts/05_hash_cpp.sql

db-down:
	docker compose -f $(DB_COMPOSE) down -v
