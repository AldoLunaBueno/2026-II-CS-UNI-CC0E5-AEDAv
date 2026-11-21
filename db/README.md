# Índices en PostgreSQL: Hash, B-Tree y GiST (PS4)

Scripts SQL intencionales para demostrar cuándo usar cada estructura de índice en **PostgreSQL**, ejecutados sobre `postgres:17-alpine` en Docker.

## Estructura

- `db/compose.yml`: servicio ligero `postgres:17-alpine` (`aeda_postgres`).
- `db/scripts/01_hash.sql`: índice `USING HASH` para búsquedas puntuales de igualdad exacta (`=`) y demostración de que no soporta rangos.
- `db/scripts/02_btree.sql`: índice `USING BTREE` para consultas por rango (`>`, `<`, `BETWEEN`) y recorridos secuenciales ordenados (`ORDER BY`).
- `db/scripts/03_gist.sql`: índice `USING GIST` con dos casos nativos (sin extensiones externas):
  - **Parte A**: datos espaciales (`point` / `box`, estilo R-Tree) con contención `<@` y vecinos más cercanos `<->`.
  - **Parte B**: intervalos de tiempo (`tsrange`, estilo calendario / reservas) con solapamiento `&&` y contención `@>`.
- `db/scripts/04_btree_cpp.sql`: extensión personalizada `btree_cpp` (`db/extension/`) compilada en la imagen Docker que registra `CREATE INDEX ... USING btree_cpp` respaldado por `containers/BTree.h`.
- `db/scripts/05_hash_cpp.sql`: extensión personalizada `hash_cpp` (`db/extension/`) que registra `CREATE INDEX ... USING hash_cpp` respaldado por `containers/hash_table.h`.

## Ejecución rápida con Make / Docker

```bash
# Levantar PostgreSQL en Alpine y esperar a que esté listo
make db-up

# Ejecutar los 3 scripts (01_hash, 02_btree, 03_gist)
make db-run

# Ejecutar uno individualmente
docker compose -f db/compose.yml exec -T postgres psql -U postgres -d aeda_indexes -f /scripts/01_hash.sql
docker compose -f db/compose.yml exec -T postgres psql -U postgres -d aeda_indexes -f /scripts/02_btree.sql
docker compose -f db/compose.yml exec -T postgres psql -U postgres -d aeda_indexes -f /scripts/03_gist.sql

# Detener y limpiar el contenedor
make db-down
```
