FROM postgres:17-alpine

RUN apk add --no-cache libstdc++ \
    && apk add --no-cache --virtual .build-deps \
        build-base \
        postgresql17-dev

WORKDIR /tmp/pg_cpp_build
COPY types.h foreach.h ./
COPY containers/ ./containers/
COPY utilities/ ./utilities/
COPY db/extension/ ./extension/

RUN PG_INC="$(pg_config --includedir-server)" \
    && PG_LIBDIR="$(pg_config --pkglibdir)" \
    && PG_SHAREDIR="$(pg_config --sharedir)" \
    && g++ -std=c++23 -O2 -fPIC -Icontainers -c extension/btree_cpp_bridge.cpp -o btree_cpp_bridge.o \
    && gcc -O2 -fPIC -I"$PG_INC" -Iextension -c extension/btree_cpp_am.c -o btree_cpp_am.o \
    && g++ -shared -o "$PG_LIBDIR/btree_cpp.so" btree_cpp_am.o btree_cpp_bridge.o \
    && cp extension/btree_cpp.control extension/btree_cpp--1.0.sql "$PG_SHAREDIR/extension/" \
    && g++ -std=c++23 -O2 -fPIC -I. -c extension/hash_cpp_bridge.cpp -o hash_cpp_bridge.o \
    && gcc -O2 -fPIC -I"$PG_INC" -Iextension -c extension/hash_cpp_am.c -o hash_cpp_am.o \
    && g++ -shared -o "$PG_LIBDIR/hash_cpp.so" hash_cpp_am.o hash_cpp_bridge.o \
    && cp extension/hash_cpp.control extension/hash_cpp--1.0.sql "$PG_SHAREDIR/extension/" \
    && rm -rf /tmp/pg_cpp_build \
    && apk del .build-deps
