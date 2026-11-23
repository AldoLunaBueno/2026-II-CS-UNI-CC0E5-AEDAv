/**
 * @file types.h
 * @ingroup core
 * @brief Alias de tipos base utilizados en todo el proyecto.
 */

#ifndef __TYPES_H__
#define __TYPES_H__

using TX = int;          /**< Tipo entero estándar para ejemplos y pruebas. */
using TXX = long long;   /**< Tipo entero extendido de 64 bits. */
using Ref = long long;   /**< Tipo utilizado para referencias o identificadores de nodo. */

/**
 * @typedef CW
 * @brief Tipo de carácter ancho dependiente de la arquitectura.
 *
 * Se define como `wchar_t` en arquitecturas de 32/64 bits (x86/x64) para soportar
 * caracteres Unicode, y como `char` en otras arquitecturas.
 */
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
using CW = wchar_t;
#else
using  CW = char;
#endif

#endif // __TYPES_H__