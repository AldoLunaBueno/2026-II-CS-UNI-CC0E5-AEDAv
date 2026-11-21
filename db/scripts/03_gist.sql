-- ============================================================================
-- 03_gist.sql - Índice tipo GiST (Generalized Search Tree) en PostgreSQL
-- ============================================================================
-- Qué es GiST:
--   Árbol balanceado generalizado donde cada nodo interno almacena un predicado
--   contenedor (bounding box / rango mínimo que engloba a sus hijos).
--   Permite implementar estructuras tipo R-Tree (espacial) y árboles de
--   intervalos sin necesidad de extensiones externas (tipos nativos).
--
-- Incluye ambas opciones solicitadas:
--   - Parte A: Datos espaciales 2D (point / box, familia R-Tree)
--   - Parte B: Intervalos de tiempo (tsrange, estilo calendario / reservas)
-- ============================================================================

-- ============================================================================
-- PARTE A: DATOS ESPACIALES NATIVOS (point / box - estilo R-Tree)
-- ============================================================================
DROP TABLE IF EXISTS sensores_campus CASCADE;

CREATE TABLE sensores_campus (
    id        SERIAL PRIMARY KEY,
    codigo    VARCHAR(32) NOT NULL,
    ubicacion POINT       NOT NULL
);

-- Poblar 100,000 sensores distribuidos en un plano [0, 1000) x [0, 1000)
INSERT INTO sensores_campus (codigo, ubicacion)
SELECT
    'SENS-' || g,
    point(
        ((g * 7919) % 100000) / 100.0,
        ((g * 6271) % 100000) / 100.0
    )
FROM generate_series(1, 100000) AS g;

-- Índice GiST explícito sobre coordenadas 2D
CREATE INDEX idx_sensores_ubicacion_gist
    ON sensores_campus USING GIST (ubicacion);

ANALYZE sensores_campus;

SELECT
    indexname,
    indexdef,
    pg_size_pretty(pg_relation_size(indexname::regclass)) AS index_size
FROM pg_indexes
WHERE tablename = 'sensores_campus';

-- A1. Búsqueda espacial por ventana rectangular (<@ "contenido dentro de box")
--     Poda subárboles cuyos bounding boxes no intersectan el área consultada.
EXPLAIN ANALYZE
SELECT id, codigo, ubicacion
FROM sensores_campus
WHERE ubicacion <@ box '((100, 100), (130, 130))';

-- A2. Búsqueda de K vecinos más cercanos (KNN con operador de distancia <->)
EXPLAIN ANALYZE
SELECT id, codigo, ubicacion, ubicacion <-> point '(500, 500)' AS distancia
FROM sensores_campus
ORDER BY ubicacion <-> point '(500, 500)'
LIMIT 5;


-- ============================================================================
-- PARTE B: INTERVALOS DE TIEMPO (tsrange - estilo Google Calendar / reservas)
-- ============================================================================
DROP TABLE IF EXISTS reservas_aulas CASCADE;

CREATE TABLE reservas_aulas (
    id      SERIAL PRIMARY KEY,
    aula    VARCHAR(20) NOT NULL,
    curso   VARCHAR(64) NOT NULL,
    horario TSRANGE     NOT NULL
);

-- Poblar 100,000 bloques de reserva de 2 horas cada uno
INSERT INTO reservas_aulas (aula, curso, horario)
SELECT
    'AULA-' || (100 + (g % 40)),
    'CURSO-' || (1 + (g % 25)),
    tsrange(
        TIMESTAMP '2026-01-01 08:00:00' + (g * INTERVAL '15 minutes'),
        TIMESTAMP '2026-01-01 10:00:00' + (g * INTERVAL '15 minutes'),
        '[)'
    )
FROM generate_series(1, 100000) AS g;

-- Índice GiST explícito sobre rangos de tiempo
CREATE INDEX idx_reservas_horario_gist
    ON reservas_aulas USING GIST (horario);

ANALYZE reservas_aulas;

SELECT
    indexname,
    indexdef,
    pg_size_pretty(pg_relation_size(indexname::regclass)) AS index_size
FROM pg_indexes
WHERE tablename = 'reservas_aulas';

-- B1. Detectar reservas que se solapan con una franja horaria (operador &&)
EXPLAIN ANALYZE
SELECT id, aula, curso, horario
FROM reservas_aulas
WHERE horario && '[2026-11-21 10:00, 2026-11-21 12:00)'::tsrange;

-- B2. Detectar qué reservas están activas en un instante exacto (operador @>)
EXPLAIN ANALYZE
SELECT id, aula, curso, horario
FROM reservas_aulas
WHERE horario @> '2026-11-21 10:30:00'::timestamp;
