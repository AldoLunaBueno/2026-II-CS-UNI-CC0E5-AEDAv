-- ============================================================================
-- 05_hash_cpp.sql - Índice Hash personalizado usando C++ (containers/hash_table.h)
-- ============================================================================
-- Usa la extensión hash_cpp (IndexAmRoutine en PostgreSQL + HashTable de C++)
-- mediante CREATE INDEX ... USING hash_cpp.
-- ============================================================================

CREATE EXTENSION IF NOT EXISTS hash_cpp;

DROP TABLE IF EXISTS usuarios_cpp CASCADE;

CREATE TABLE usuarios_cpp (
    id       SERIAL PRIMARY KEY,
    username VARCHAR(64)  NOT NULL,
    email    VARCHAR(120) NOT NULL
);

INSERT INTO usuarios_cpp (username, email)
SELECT
    'user_' || g,
    'user_' || g || '@uni.edu.pe'
FROM generate_series(1, 20000) AS g;

-- Índice creado explícitamente con nuestra implementación C++ (hash_table.h)
CREATE INDEX idx_usuarios_username_hash_cpp
    ON usuarios_cpp USING hash_cpp (username);

ANALYZE usuarios_cpp;

SELECT
    indexname,
    indexdef
FROM pg_indexes
WHERE tablename = 'usuarios_cpp';

-- 1. Búsqueda puntual exacta (=) usando HashTable::contains / operator[] en C++
EXPLAIN ANALYZE
SELECT id, username, email
FROM usuarios_cpp
WHERE username = 'user_12345';

-- 2. Búsqueda puntual de clave inexistente
EXPLAIN ANALYZE
SELECT id, username, email
FROM usuarios_cpp
WHERE username = 'jose_luna_inexistente';

-- 3. Contraste intencional: rango (>) no está soportado por hash_cpp -> Seq Scan
EXPLAIN ANALYZE
SELECT id, username
FROM usuarios_cpp
WHERE username > 'user_19990';
