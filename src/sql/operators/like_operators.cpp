#include "sql/operators/like_operators.h"

#include "common/macros.h"

namespace tpl::sql {

namespace {
#define NextByte(p, plen) ((p)++, (plen)--)

// Inspired by Postgres.
bool LikeImpl(const char *str, size_t str_len, const char *pattern, const std::size_t pattern_len,
              char escape) {
  TPL_ASSERT(str != nullptr, "Input string cannot be NULL");
  TPL_ASSERT(pattern != nullptr, "Pattern cannot be NULL");

  const char *s = str, *p = pattern;
  std::size_t slen = str_len, plen = pattern_len;

  for (; plen > 0 && slen > 0; NextByte(p, plen)) {
    if (*p == escape) {
      // Next pattern character must match exactly, whatever it is
      NextByte(p, plen);

      if (plen == 0 || *p != *s) {
        return false;
      }

      NextByte(s, slen);
    } else if (*p == '%') {
      // Any sequence of '%' wildcards can essentially be replaced by one '%'. Similarly, any
      // sequence of N '_'s will blindly consume N characters from the input string. Process the
      // pattern until we reach a non-wildcard character.
      NextByte(p, plen);
      while (plen > 0) {
        if (*p == '%') {
          NextByte(p, plen);
        } else if (*p == '_') {
          if (slen == 0) {
            return false;
          }
          NextByte(s, slen);
          NextByte(p, plen);
        } else {
          break;
        }
      }

      // If we've reached the end of the pattern, the tail of the input string is accepted.
      if (plen == 0) {
        return true;
      }

      if (*p == escape) {
        NextByte(p, plen);
        TPL_ASSERT(*p != 0, "LIKE pattern must not end with an escape character");
        if (plen == 0) {
          return false;
        }
      }

      while (slen > 0) {
        if (LikeImpl(s, slen, p, plen, escape)) {
          return true;
        }
        NextByte(s, slen);
      }
      // No match
      return false;
    } else if (*p == '_') {
      // '_' wildcard matches a single character in the input
      NextByte(s, slen);
    } else if (*p == *s) {
      // Exact character match
      NextByte(s, slen);
    } else {
      // Unmatched!
      return false;
    }
  }

  return slen == 0 && plen == 0;
}

}  // namespace

bool Like::operator()(const VarlenEntry &str, const VarlenEntry &pattern, char escape) const {
  return LikeImpl(reinterpret_cast<const char *>(str.GetContent()), str.GetSize(),
                  reinterpret_cast<const char *>(pattern.GetContent()), pattern.GetSize(), escape);
}

}  // namespace tpl::sql
