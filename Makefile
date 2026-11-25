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
DEMO_SPEED ?= 1.0
DB_COMPOSE := db/compose.yml

.PHONY: all run texcel demo test test-unit test-e2e test-perf pr docs clean db-up db-run db-down help

##@ Compilación y ejecución

## Limpia artefactos previos, compila $(TARGET) y lo ejecuta.
all: clean $(TARGET) run

$(TARGET): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

## Compila y ejecuta el binario principal.
run: $(TARGET)
	./$(TARGET)

## Ejecuta la aplicación en modo Texcel (--texcel).
texcel: $(TARGET)
	./$(TARGET) --texcel

## Ejecuta la demo interactiva en vivo de Texcel en tmux (S=1.0 velocidad por defecto).
texcel-demo: $(TARGET)
	python3 scripts/demo_live.py --speed $(S)

## Elimina el binario compilado, tmp/ y build/docs/, y limpia la terminal.
clean:
	rm -rf $(TARGET) tmp build/docs
	@clear 2>/dev/null || printf "\033c"

##@ Pruebas

## Ejecuta todas las pruebas unitarias y end-to-end.
test: test-unit test-e2e

## Compila y ejecuta cada prueba unitaria en tests/*/unit/*.cpp.
test-unit: $(HEADERS) $(TEST_SRCS)
	@for src in $(TEST_SRCS); do \
		bin="/tmp/$$(basename $$src .cpp)"; \
		echo "Running $$src..."; \
		$(CXX) $(CXXFLAGS) "$$src" -o "$$bin" && "$$bin" && rm -f "$$bin" || exit 1; \
	done

## Ejecuta las pruebas end-to-end en Python sobre $(TARGET).
test-e2e: $(TARGET) $(E2E_SRCS)
	@for src in $(E2E_SRCS); do \
		echo "Running $$src..."; \
		python3 "$$src" "./$(TARGET)" || exit 1; \
	done

## Genera hojas de prueba y ejecuta el benchmark de rendimiento de Texcel.
test-perf: $(HEADERS) tests/texcel/performance/gen_perf_sheets.py tests/texcel/performance/bench_texcel.cpp
	@mkdir -p tmp
	@python3 tests/texcel/performance/gen_perf_sheets.py
	@$(CXX) $(CXXFLAGS) -O2 -I. tests/texcel/performance/bench_texcel.cpp -o /tmp/bench_texcel && /tmp/bench_texcel && rm -f /tmp/bench_texcel

##@ Base de datos

## Levanta el contenedor de PostgreSQL con Docker Compose y espera a que esté listo.
db-up:
	docker compose -f $(DB_COMPOSE) up -d --build --wait

## Ejecuta los scripts SQL de índices (01 al 05) en PostgreSQL.
db-run:
	docker compose -f $(DB_COMPOSE) exec -T postgres psql -U postgres -d aeda_indexes -v ON_ERROR_STOP=1 -f /scripts/01_hash.sql
	docker compose -f $(DB_COMPOSE) exec -T postgres psql -U postgres -d aeda_indexes -v ON_ERROR_STOP=1 -f /scripts/02_btree.sql
	docker compose -f $(DB_COMPOSE) exec -T postgres psql -U postgres -d aeda_indexes -v ON_ERROR_STOP=1 -f /scripts/03_gist.sql
	docker compose -f $(DB_COMPOSE) exec -T postgres psql -U postgres -d aeda_indexes -v ON_ERROR_STOP=1 -f /scripts/04_btree_cpp.sql
	docker compose -f $(DB_COMPOSE) exec -T postgres psql -U postgres -d aeda_indexes -v ON_ERROR_STOP=1 -f /scripts/05_hash_cpp.sql

## Detiene y elimina los contenedores y volúmenes de la base de datos.
db-down:
	docker compose -f $(DB_COMPOSE) down -v

##@ Documentación y utilidades

## Genera la documentación Doxygen y levanta un servidor HTTP local (DOCS_PORT=8000).
docs:
	@mkdir -p build/docs
	doxygen Doxyfile
	@echo ""
	@echo "Documentacion generada. Abre: http://localhost:$(DOCS_PORT)"
	@echo "Presiona Ctrl+C para detener el servidor."
	@python3 -m http.server $(DOCS_PORT) -d build/docs/html

## Genera el comentario de PR en $(PR_OUT) usando $(PR_SCRIPT).
pr:
	@bash $(PR_SCRIPT) README.md $(PR_OUT)

## Muestra esta ayuda generada desde los comentarios del Makefile.
help:
	@awk -v no_color="$${NO_COLOR:-}" -v def="$(.DEFAULT_GOAL)" ' \
		BEGIN { \
			bold = (no_color != "" ? "" : "\033[1;33m"); \
			cyan = (no_color != "" ? "" : "\033[36m"); \
			dim  = (no_color != "" ? "" : "\033[2m"); \
			rst  = (no_color != "" ? "" : "\033[0m"); \
			printf "%sUso:%s make <regla>  %s(sin argumentos ejecuta: %s)%s\n", bold, rst, dim, def, rst; \
		} \
		/^##@/ { \
			sub(/^##@[[:space:]]*/, ""); \
			printf "\n%s%s%s\n", bold, $$0, rst; \
			desc = ""; \
			next; \
		} \
		/^##([[:space:]]|$$)/ { \
			sub(/^##[[:space:]]?/, ""); \
			desc = (desc == "" ? $$0 : desc "\n                " $$0); \
			next; \
		} \
		/^[a-zA-Z0-9_-]+:([^=]|$$)/ { \
			if (desc != "") { \
				target = $$0; \
				sub(/:.*/, "", target); \
				tag = (target == def ? " " dim "(por defecto)" rst : ""); \
				printf "  %s%-12s%s  %s%s\n", cyan, target, rst, desc, tag; \
				desc = ""; \
			} \
			next; \
		} \
		{ desc = "" } \
	' $(MAKEFILE_LIST)
