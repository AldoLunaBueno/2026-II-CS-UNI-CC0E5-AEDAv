CREATE FUNCTION hash_cpp_handler(internal)
RETURNS index_am_handler
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE ACCESS METHOD hash_cpp TYPE INDEX HANDLER hash_cpp_handler;

CREATE OPERATOR CLASS hash_cpp_text_ops
    DEFAULT FOR TYPE text USING hash_cpp AS
    OPERATOR 1 =,
    FUNCTION 1 hashtext(text);

CREATE OPERATOR CLASS hash_cpp_int4_ops
    DEFAULT FOR TYPE int4 USING hash_cpp AS
    OPERATOR 1 =,
    FUNCTION 1 hashint4(int4);
