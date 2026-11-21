-- ============================================================================
-- 01_hash.sql - Índice tipo HASH en PostgreSQL
-- ============================================================================
-- Cuándo usarlo:
--   Búsquedas puntuales por igualdad exacta (=), como validar un login por
--   username/email o buscar un registro específico por ID sin necesitar orden
--   ni iteradores hacia elementos adyacentes.
--
-- Por qué NO usar UNIQUE/PRIMARY KEY en la columna de prueba:
--   PostgreSQL implementa UNIQUE y PRIMARY KEY internamente con B-Tree.
--   Aquí creamos explícitamente USING HASH para demostrar la estructura Hash.
-- ============================================================================

DROP TABLE IF EXISTS usuarios CASCADE;

CREATE TABLE usuarios (
    id            SERIAL PRIMARY KEY,
    username      VARCHAR(64)  NOT NULL,
    email         VARCHAR(120) NOT NULL,
    password_hash CHAR(64)     NOT NULL,
    ultimo_login  TIMESTAMP    NOT NULL
);

-- Poblar 100,000 usuarios para que el optimizador prefiera el índice frente a Seq Scan
INSERT INTO usuarios (username, email, password_hash, ultimo_login)
SELECT
    'user_' || g,
    'user_' || g || '@uni.edu.pe',
    md5(g::text) || md5((g * 2)::text),
    TIMESTAMP '2026-01-01 00:00:00' + (g || ' minutes')::interval
FROM generate_series(1, 100000) AS g;

-- Índice HASH explícito e intencional sobre username
CREATE INDEX idx_usuarios_username_hash
    ON usuarios USING HASH (username);

ANALYZE usuarios;

-- Información del índice creado
SELECT
    indexname,
    indexdef,
    pg_size_pretty(pg_relation_size(indexname::regclass)) AS index_size
FROM pg_indexes
WHERE tablename = 'usuarios';

-- 1. CASO IDEAL (igualdad exacta: login / búsqueda dirigida)
--    Debe usar: Index Scan using idx_usuarios_username_hash
EXPLAIN ANALYZE
SELECT id, username, email, ultimo_login
FROM usuarios
WHERE username = 'user_54321';

-- 2. CASO IDEAL (igualdad exacta para clave inexistente: O(1) en bucket vacío)
--    Debe usar: Index Scan using idx_usuarios_username_hash
EXPLAIN ANALYZE
SELECT id, username, email
FROM usuarios
WHERE username = 'jose_luna_inexistente';

-- 3. CONTRASTE INTENCIONAL (consulta por rango con '>')
--    Un índice Hash NO mantiene orden; no puede resolver rangos ni iterar
--    secuencialmente, por lo que PostgreSQL cae a Seq Scan.
EXPLAIN ANALYZE
SELECT id, username
FROM usuarios
WHERE username > 'user_99990';
