////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2022 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

#ifndef SFML_UTF_HPP
#define SFML_UTF_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Config.hpp>
#include <string>

namespace sf
{





} // namespace sf


#endif // SFML_UTF_HPP


////////////////////////////////////////////////////////////
/// \class sf::Utf
/// \ingroup system
///
/// Utility class providing generic functions for UTF conversions.
///
/// sf::Utf is a low-level, generic interface for counting, iterating,
/// encoding and decoding Unicode characters and strings. It is able
/// to handle ANSI, wide, latin-1, UTF-8, UTF-16 and UTF-32 encodings.
///
/// sf::Utf<X> functions are all static, these classes are not meant to
/// be instantiated. All the functions are template, so that you
/// can use any character / string type for a given encoding.
///
/// It has 3 specializations:
/// \li sf::Utf<8> (with sf::Utf8 type alias)
/// \li sf::Utf<16> (with sf::Utf16 type alias)
/// \li sf::Utf<32> (with sf::Utf32 type alias)
///
////////////////////////////////////////////////////////////
