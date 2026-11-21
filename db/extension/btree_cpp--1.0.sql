CREATE FUNCTION btree_cpp_handler(internal)
RETURNS index_am_handler
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE ACCESS METHOD btree_cpp TYPE INDEX HANDLER btree_cpp_handler;

CREATE OPERATOR CLASS btree_cpp_float8_ops
    DEFAULT FOR TYPE float8 USING btree_cpp AS
    OPERATOR 1 <,
    OPERATOR 2 <=,
    OPERATOR 3 =,
    OPERATOR 4 >=,
    OPERATOR 5 >,
    FUNCTION 1 btfloat8cmp(float8, float8);

CREATE OPERATOR CLASS btree_cpp_int4_ops
    DEFAULT FOR TYPE int4 USING btree_cpp AS
    OPERATOR 1 <,
    OPERATOR 2 <=,
    OPERATOR 3 =,
    OPERATOR 4 >=,
    OPERATOR 5 >,
    FUNCTION 1 btint4cmp(int4, int4);
