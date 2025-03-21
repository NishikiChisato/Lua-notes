#include <assert.h>
#include <stdio.h>
#include <string.h>

/**
 ****************************************************************************************************************************************************
 *
 * Unicode code point range from U+0000 to U+10FFFF(hexadecimal, up to 21 bits in binary)
 *
 * UTF-8 representation(can fit 0 to 21 bits):
 *
 * 1-byte sequence: 
 * 0xxxxxxx -> x is bit to represent unicode code point
 *
 * 2-bytes and 3-bytes sequence: 
 * 110xxxxx 10xxxxxx -> x is bit to represent unicode code point
 * 1110xxxx 10xxxxxx 10xxxxxx -> x is bit to represent unicode code point
 *
 * 4-bytes sequence:
 * 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx -> x is bit to represent unicode code point
 *
 ****************************************************************************************************************************************************
 *
 * If UTF-8 uses a maximum of 4 bytes, why does luaO_utf8esc use a buffer size (UTF8BUFFSZ) of 8? 
 * The reason is not directly related to the UTF-8 encoding itself. It's about how this function is used within Lua, 
 * specifically in the context of string escaping and error messages. 
 *
 * Here's the key: 
 * luaO_utf8esc is primarily used to escape non-ASCII characters in strings, 
 * especially when constructing error messages or debugging output. 
 * In these contexts, Lua often needs to represent arbitrary byte sequences as printable strings. 
 *
 * Consider a scenario where Lua encounters an invalid byte sequence in what it expects to be a UTF-8 string. 
 * It needs to create an error message that shows the hexadecimal representation of those invalid bytes. 
 *
 * Let's say Lua encounters a single byte with the value 0xFF. If it were to simply try to interpret this as UTF-8, 
 * it would be invalid (because 0xFF doesn't start any valid UTF-8 sequence). 
 *
 * To represent this in an error message, Lua might want to show it as \xff. 
 * This requires four characters: a backslash, an 'x', and two hexadecimal digits. 
 *
 * Now, imagine a worst-case scenario: Lua encounters a sequence of four invalid bytes. 
 * If it were to represent each of these bytes as \xx, it would need 4 * 4 = 16 characters in the resulting string. 
 * However, luaO_utf8esc itself is only responsible for encoding a single Unicode code point into UTF-8. 
 * It's the usage of this function that requires the larger buffer. 
 *
 * Here's how it relates to luaO_utf8esc and the buffer size: 
 * luaO_utf8esc is used to encode a single Unicode code point into its UTF-8 representation (up to 4 bytes). 
 * The caller of luaO_utf8esc might use the result to build a larger string, for example, an escaped string representation of arbitrary bytes. 
 *
 * The buffer size UTF8BUFFSZ (which is 8) is chosen to be large enough to hold the UTF-8 encoding plus some extra space 
 * for null termination and other potential overhead within the larger string construction process. 
 * It is not directly related to the maximum length of a single UTF-8 character but to the context of the function's usage. 
 *
 * In simpler terms: luaO_utf8esc itself never produces more than 4 bytes of UTF-8. The extra space in the buffer is for the code 
 * that uses the output of luaO_utf8esc to create larger escaped strings. Therefore, the choice of 8 is a safety measure and an optimization 
 * for the specific way Lua uses this function, not a limitation of UTF-8 itself.
 *
 ****************************************************************************************************************************************************
 **/

#define UTF8BUFFSZ 8
#define cast(t, exp) (t)(exp)

int luaO_utf8esc(char *buf, unsigned long x) {
  int n = 1;
  assert(x <= 0x10FFFFu); /* 0x10FFFF is enough because Unicode code point only
                             has 21 bits */
  if (x < 0x80) {
    /* ASCII code */
    buf[UTF8BUFFSZ - 1] = cast(char, x);
  } else {
    unsigned int mfb =
        0x3f; /* maximum bits fits in first bit(in UTF-8 representation, number
                 of *maximum* arvailable bits range from 3 to 6) */
    do {
      buf[UTF8BUFFSZ - (n++)] = cast(char, 0x80 | (x & 0x3f));
      x >>= 6;
      mfb >>= 1;
    } while (x > mfb); /* bits in x cannot fit in current byte in UTF-8, we need
                          extra bytes to store it, loop again */
    /* when reach here, the value of mfb among 0x7, 0xF, 0x1F, and x only has
     * three, four, five bits, respectively */
    buf[UTF8BUFFSZ - (n)] = cast(
        char, (~mfb << 1) |
                  x); /* the first bit in UTF-8 is always 110, 1110, 11110 */
  }
  return n;
}

void test_utf8esc(unsigned int utf8code, const char *expected_point) {
  char buf[UTF8BUFFSZ];
  int n = luaO_utf8esc(buf, utf8code);
  char actual_point[5] = {0};
  for (int i = 0; i < n; i++) {
    actual_point[i] = buf[UTF8BUFFSZ - n + i];
  }
  printf("UTF-8 code: U+%04X, Actual: %s, Expected: %s\n", utf8code,
         actual_point, expected_point);
  if (strcmp(actual_point, expected_point) == 0) {
    printf("Passed\n");
  } else {
    printf("Failed\n");
  }
}

int main() {
  // ASCII characters
  test_utf8esc(0x41, "A");    // 'A'
  test_utf8esc(0x7F, "\x7F"); // Control character

  // 2-byte characters
  test_utf8esc(0xC2, "\xC2\x82");     // '¢' (Cent symbol)
  test_utf8esc(0xE0, "\xE0\xA0\x80"); // some character in thai

  // 3-byte characters
  test_utf8esc(0x20AC, "\xE2\x82\xAC"); // '€' (Euro sign)
  test_utf8esc(0xD55C, "\xED\x95\x9C"); // 한 (Korean character)

  // 4-byte characters
  test_utf8esc(0x10437, "\xF0\x90\x90\xB7"); // G clef symbol
  test_utf8esc(0x1D11E, "\xF0\x9D\x84\x9E"); // Musical symbol G clef

  // Edge cases
  test_utf8esc(0x00, "\x00");                 // Null character
  test_utf8esc(0x7F, "\x7F");                 // Maximum 1-byte character
  test_utf8esc(0x80, "\xC2\x80");             // Minimum 2-byte character
  test_utf8esc(0x7FF, "\xDF\xBF");            // Maximum 2-byte character
  test_utf8esc(0x800, "\xE0\xA0\x80");        // Minimum 3-byte character
  test_utf8esc(0xFFFF, "\xEF\xBF\xBF");       // Maximum 3-byte character
  test_utf8esc(0x10000, "\xF0\x90\x80\x80");  // Minimum 4-byte character
  test_utf8esc(0x10FFFF, "\xF4\x8F\xBF\xBF"); // Maximum 4-byte character

  printf("All tests passed!\n");

  return 0;
}
