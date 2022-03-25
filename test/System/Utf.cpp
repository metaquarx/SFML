#include <SFML/System/Utf.hpp>

#include <doctest.h>

TEST_CASE("sf::encode functions - [system]")
{
    // Test strings were sampled from a range of unicode character blocks, to ensure that
    // we're testing across byte boundaries.

    // String literal prefixes:
    //  u8"" utf 8  string
    //   u"" utf 16 string
    //   U"" utf 32 string
    //   L""   wide string

    std::string    utf8_str  = u8"A0@^µÔØĉƘǿɊΔϿῶБѤӽԵالجیم‡⊁⏳○🯵";
    std::u16string utf16_str = u"A0@^µÔØĉƘǿɊΔϿῶБѤӽԵالجیم‡⊁⏳○🯵";
    std::u32string utf32_str = U"A0@^µÔØĉƘǿɊΔϿῶБѤӽԵالجیم‡⊁⏳○🯵";
    std::wstring   wide_str  = L"A0@^µÔØĉƘǿɊΔϿῶБѤӽԵالجیم‡⊁⏳○🯵";

    SUBCASE("Encode to UTF-8")
    {
        CHECK(sf::encode::Utf8(utf16_str) == utf8_str);
        CHECK(sf::encode::Utf8(utf32_str) == utf8_str);
        CHECK(sf::encode::Utf8(wide_str) == utf8_str);
    }

    SUBCASE("Encode to UTF-32")
    {
        CHECK(sf::encode::Utf32(utf8_str) == utf32_str);
        CHECK(sf::encode::Utf32(utf16_str) == utf32_str);
        CHECK(sf::encode::Utf32(wide_str) == utf32_str);
    }

    SUBCASE("Encode to UTF-16")
    {
        CHECK(sf::encode::Utf16(utf8_str) == utf16_str);
        CHECK(sf::encode::Utf16(utf32_str) == utf16_str);
        CHECK(sf::encode::Utf16(wide_str) == utf16_str);
    }

    SUBCASE("Encode to Wide string")
    {
        CHECK(sf::encode::Wide(utf8_str) == wide_str);
        CHECK(sf::encode::Wide(utf16_str) == wide_str);
        CHECK(sf::encode::Wide(utf32_str) == wide_str);
    }
}
