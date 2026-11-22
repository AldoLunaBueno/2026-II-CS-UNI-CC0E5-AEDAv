CXX        := g++
CXXFLAGS   := -std=c++23
TARGET     := main
SRCS       := main.cpp demos.cpp
HEADERS    := demos.h types.h $(wildcard texcel/*.h) $(wildcard texcel/helpers/*.h)
PR_OUT     := tmp/pr_comment.md
PR_SCRIPT  := scripts/gen_pr_comment.sh
DOCS_PORT  ?= 8000
DB_COMPOSE := db/compose.yml

.PHONY: all run texcel pr docs clean db-up db-run db-down

all: clean $(TARGET) run

$(TARGET): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

texcel: $(TARGET)
	./$(TARGET) --texcel

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
