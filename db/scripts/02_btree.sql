-- ============================================================================
-- 02_btree.sql - Índice tipo B-TREE en PostgreSQL
-- ============================================================================
-- Cuándo usarlo:
--   Consultas por rango (<, <=, >, >=, BETWEEN) y recorridos secuenciales
--   ordenados (ORDER BY), donde tras ubicar el primer elemento se avanza
--   por las hojas ordenadas hacia los elementos siguientes/anteriores.
--
-- Nota:
--   En PostgreSQL, CREATE INDEX sin especificar USING crea un B-Tree por
--   defecto. Aquí se especifica USING BTREE de forma explícita e intencional.
-- ============================================================================

DROP TABLE IF EXISTS matriculas CASCADE;

CREATE TABLE matriculas (
    id              SERIAL PRIMARY KEY,
    curso_id        INT           NOT NULL,
    alumno_id       INT           NOT NULL,
    nota            NUMERIC(4, 2) NOT NULL,
    fecha_matricula DATE          NOT NULL
);

-- Poblar 100,000 matrículas con notas pseudo-aleatorias entre 0.00 y 20.00
INSERT INTO matriculas (curso_id, alumno_id, nota, fecha_matricula)
SELECT
    1 + (g % 50) AS curso_id,
    1000 + g     AS alumno_id,
    ROUND((((g * 7919) % 2001) / 100.0)::numeric, 2) AS nota,
    DATE '2026-08-01' + (g % 30) AS fecha_matricula
FROM generate_series(1, 100000) AS g;

-- Índices B-Tree explícitos
CREATE INDEX idx_matriculas_nota_btree
    ON matriculas USING BTREE (nota);

CREATE INDEX idx_matriculas_curso_nota_btree
    ON matriculas USING BTREE (curso_id, nota);

ANALYZE matriculas;

-- Información de los índices creados
SELECT
    indexname,
    indexdef,
    pg_size_pretty(pg_relation_size(indexname::regclass)) AS index_size
FROM pg_indexes
WHERE tablename = 'matriculas';

-- 1. Rango acotado (ejemplo de clase: nota > 15 AND nota < 18 dentro de un curso)
--    Ubica el inicio del intervalo y recorre secuencialmente hasta salir del rango.
EXPLAIN ANALYZE
SELECT id, curso_id, alumno_id, nota
FROM matriculas
WHERE curso_id = 12
  AND nota > 15.00
  AND nota < 18.00;

-- 2. Rango acotado hacia el extremo superior (nota >= 19.80, equivalente a [19.80, +inf))
EXPLAIN ANALYZE
SELECT id, curso_id, alumno_id, nota
FROM matriculas
WHERE nota >= 19.80;

-- 3. Rango con BETWEEN + recorrido ordenado (sin operación extra de Sort)
EXPLAIN ANALYZE
SELECT id, curso_id, alumno_id, nota
FROM matriculas
WHERE nota BETWEEN 15.00 AND 18.00
ORDER BY nota ASC
LIMIT 20;

-- 4. Listado completo de alumnos matriculados en un curso, ordenados por nota
--    (encuentra el primero del curso y recorre secuencialmente los siguientes)
EXPLAIN ANALYZE
SELECT alumno_id, nota
FROM matriculas
WHERE curso_id = 12
ORDER BY nota ASC;
