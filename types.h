#ifndef __TYPES_H__
#define __TYPES_H__

using TX = int;
using TXX = long long;
using Ref = long long;

// Crear el tipo CW como char si estamos en architectura de 16 bits
// pero como wchar_t si estamos en 32 o 64 bits. Esto permite que el mismo
// codigo compile en ambas arquitecturas y que el tipo CW sea lo suficientemente grande para contener cualquier caracter de cualquier idioma (unicode).
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
using CW = wchar_t;
#else
using  CW = char;
#endif

#endif // __TYPES_H__