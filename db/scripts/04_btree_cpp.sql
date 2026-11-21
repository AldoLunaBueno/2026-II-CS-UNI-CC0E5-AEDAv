-- ============================================================================
-- 04_btree_cpp.sql - Índice B-Tree personalizado usando C++ (containers/BTree.h)
-- ============================================================================
-- Usa la extensión btree_cpp (IndexAmRoutine en PostgreSQL + BTree<double, int>
-- de C++) mediante CREATE INDEX ... USING btree_cpp.
-- ============================================================================

CREATE EXTENSION IF NOT EXISTS btree_cpp;

DROP TABLE IF EXISTS notas_cpp CASCADE;

CREATE TABLE notas_cpp (
    id        SERIAL PRIMARY KEY,
    curso_id  INT              NOT NULL,
    alumno_id INT              NOT NULL,
    nota      DOUBLE PRECISION NOT NULL
);

INSERT INTO notas_cpp (curso_id, alumno_id, nota)
SELECT
    1 + (g % 50),
    1000 + g,
    ROUND((((g * 7919) % 2001) / 100.0)::numeric, 2)::double precision
FROM generate_series(1, 20000) AS g;

-- Índice creado explícitamente con nuestra implementación C++ (BTree.h)
CREATE INDEX idx_notas_btree_cpp
    ON notas_cpp USING btree_cpp (nota);

ANALYZE notas_cpp;

SELECT
    indexname,
    indexdef
FROM pg_indexes
WHERE tablename = 'notas_cpp';

-- 1. Búsqueda puntual exacta (=) usando BTree::Search en C++
EXPLAIN ANALYZE
SELECT id, curso_id, alumno_id, nota
FROM notas_cpp
WHERE nota = 16.50;

-- 2. Consulta por rango acotado (> y <) usando btree_cpp
EXPLAIN ANALYZE
SELECT id, curso_id, alumno_id, nota
FROM notas_cpp
WHERE nota > 19.50 AND nota < 19.90;

-- 3. Consulta por rango abierto (>=)
EXPLAIN ANALYZE
SELECT id, curso_id, alumno_id, nota
FROM notas_cpp
WHERE nota >= 19.80;

-- 4. Recorrido ordenado (amcanorder = true, sin Sort adicional)
EXPLAIN ANALYZE
SELECT id, curso_id, alumno_id, nota
FROM notas_cpp
WHERE nota BETWEEN 15.00 AND 18.00
ORDER BY nota ASC
LIMIT 15;
