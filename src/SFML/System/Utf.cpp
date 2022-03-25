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

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/System/Utf.hpp>

#include <codecvt>
#include <locale>

namespace sf
{

namespace
{

template<class internT, class externT, class stateT>
struct codecvt : std::codecvt<internT, externT, stateT>
{
    ~codecvt()
    {
    }
};

} // namespace

namespace encode
{

////////////////////////////////////////////////////////////
std::string Utf8(std::u16string source)
{
    std::wstring_convert<codecvt<char16_t, char, std::mbstate_t>, char16_t> convert;
    return convert.to_bytes(source);
}

////////////////////////////////////////////////////////////
std::string Utf8(std::u32string source)
{
    std::wstring_convert<codecvt<char32_t, char, std::mbstate_t>, char32_t> convert;
    return convert.to_bytes(source);
}

////////////////////////////////////////////////////////////
std::string Utf8(std::wstring source)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
    return convert.to_bytes(source);
}

////////////////////////////////////////////////////////////
std::u16string Utf16(std::string source)
{
    std::wstring_convert<codecvt<char16_t, char, std::mbstate_t>, char16_t> convert;
    return convert.from_bytes(source);
}

////////////////////////////////////////////////////////////
std::u16string Utf16(std::u32string source)
{
    return Utf16(Utf8(source));
}

////////////////////////////////////////////////////////////
std::u16string Utf16(std::wstring source)
{
    return Utf16(Utf8(source));
}

////////////////////////////////////////////////////////////
std::u32string Utf32(std::u16string source)
{
    return Utf32(Utf8(source));
}

////////////////////////////////////////////////////////////
std::u32string Utf32(std::string source)
{
    std::wstring_convert<codecvt<char32_t, char, std::mbstate_t>, char32_t> convert;
    return convert.from_bytes(source);
}

////////////////////////////////////////////////////////////
std::u32string Utf32(std::wstring source)
{
    return Utf32(Utf8(source));
}

////////////////////////////////////////////////////////////
std::wstring Wide(std::string source)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
    return convert.from_bytes(source);
}

////////////////////////////////////////////////////////////
std::wstring Wide(std::u16string source)
{
    return Wide(Utf8(source));
}

////////////////////////////////////////////////////////////
std::wstring Wide(std::u32string source)
{
    return Wide(Utf8(source));
}

} // namespace encode

} // namespace sf
