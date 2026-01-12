/**
 * @file debouncer.cpp
 * @brief MOC source file for Debouncer classes
 *
 * This file provides the translation unit required for Qt's MOC (Meta-Object
 * Compiler) to generate the meta-object code for the Debouncer and
 * PropertyDebouncer classes. These classes are defined in the header-only
 * debouncer.hpp but require MOC processing due to their use of Q_OBJECT.
 *
 * Without this .cpp file, the linker would fail with "undefined reference to
 * vtable" errors when the Debouncer classes are used.
 */

#include "NovelMind/editor/qt/debouncer.hpp"

// Force MOC to generate meta-object code for these classes
// by including them in a compiled translation unit
