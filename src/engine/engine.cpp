#include "engine/engine.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>

namespace regex_tool {

namespace {

std::string escape_literal(const std::string& s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '\\': case '.': case '^': case '$': case '[': case ']':
      case '(': case ')': case '{': case '}': case '*': case '+':
      case '?': case '|':
        out += '\\';
        out += c;
        break;
      default:
        out += c;
    }
  }
  return out;
}

std::string charset_for(const std::set<char>& chars) {
  if (chars.empty()) return "\\d";
  bool has_digit = false, has_upper = false, has_lower = false, has_other = false;
  for (char c : chars) {
    unsigned char u = static_cast<unsigned char>(c);
    if (std::isdigit(u)) has_digit = true;
    else if (std::isupper(u)) has_upper = true;
    else if (std::islower(u)) has_lower = true;
    else has_other = true;
  }
  if (!has_other) {
    if (has_digit && !has_upper && !has_lower) return "\\d";
    if (has_upper && !has_digit && !has_lower) return "[A-Z]";
    if (has_lower && !has_digit && !has_upper) return "[a-z]";
    if (has_upper && has_lower && !has_digit) return "[A-Za-z]";
    if (has_digit && !has_lower) return "[A-Z0-9]";
    if (has_digit && !has_upper) return "[a-z0-9]";
    if (has_digit) return "[A-Za-z0-9]";
  }
  std::string cls = "[";
  for (char c : chars) {
    switch (c) {
      case '\\': case ']': case '^': case '-':
        cls += '\\';
        cls += c;
        break;
      default:
        cls += c;
    }
  }
  cls += "]";
  return cls;
}

std::string tag_from_literal(const std::string& literal) {
  if (literal.empty()) return "";
  std::size_t end = literal.size();
  while (end > 0 && (literal[end - 1] == '@' || literal[end - 1] == '|' || literal[end - 1] == '_' || literal[end - 1] == '-')) {
    --end;
  }
  while (end > 0 && std::isdigit(static_cast<unsigned char>(literal[end - 1]))) --end;
  std::size_t begin = end;
  while (begin > 0 && (std::isalnum(static_cast<unsigned char>(literal[begin - 1])) || literal[begin - 1] == '-')) {
    if (literal[begin - 1] == '@' || literal[begin - 1] == '|' || literal[begin - 1] == '_') break;
    --begin;
  }
  std::string tag = literal.substr(begin, end - begin);
  while (!tag.empty() && (tag.front() == '@' || tag.front() == '|' || tag.front() == '-' || tag.front() == '_')) {
    tag.erase(tag.begin());
  }
  while (!tag.empty() && (tag.back() == '@' || tag.back() == '|' || tag.back() == '-' || tag.back() == '_')) {
    tag.pop_back();
  }
  return tag;
}

// Classical positional segmentation (strictly non-constant positions become fields).
std::vector<SegmentRule> segment_positives_exact(const std::vector<std::string>& positives) {
  std::size_t min_len = SIZE_MAX, max_len = 0;
  for (const auto& s : positives) {
    min_len = std::min(min_len, s.size());
    max_len = std::max(max_len, s.size());
  }
  if (min_len == SIZE_MAX) min_len = 0;

  std::vector<bool> is_const(max_len);
  for (std::size_t p = 0; p < max_len; ++p) {
    bool same = p < min_len;
    if (same) {
      char c = positives[0][p];
      for (const auto& s : positives) {
        if (s[p] != c) { same = false; break; }
      }
    }
    is_const[p] = same;
  }

  struct Stretch {
    bool constant;
    std::size_t begin;
    std::size_t len;
  };
  std::vector<Stretch> stretches;

  auto push = [&](bool constant, std::size_t begin, std::size_t len) {
    if (len == 0) return;
    if (!stretches.empty() && stretches.back().constant == constant) {
      stretches.back().len += len;
    } else {
      stretches.push_back({constant, begin, len});
    }
  };

  std::size_t p = 0;
  while (p < max_len) {
    bool constant = is_const[p];
    std::size_t q = p;
    while (q < max_len && is_const[q] == constant) ++q;
    push(constant, p, q - p);
    p = q;
  }

  std::vector<SegmentRule> segments;
  int field_no = 0;
  for (std::size_t i = 0; i < stretches.size(); ++i) {
    const Stretch& st = stretches[i];
    if (st.constant) {
      SegmentRule seg;
      seg.is_literal = true;
      seg.literal = positives[0].substr(st.begin, st.len);
      segments.push_back(std::move(seg));
      continue;
    }
    std::set<char> chars;
    std::size_t min_f = SIZE_MAX, max_f = 0;
    for (const auto& s : positives) {
      std::size_t b = std::min(st.begin, s.size());
      std::size_t e = std::min(st.begin + st.len, s.size());
      for (std::size_t k = b; k < e; ++k) chars.insert(s[k]);
      std::size_t n = (e > b) ? e - b : 0;
      min_f = std::min(min_f, n);
      max_f = std::max(max_f, n);
    }
    SegmentRule seg;
    seg.is_literal = false;
    seg.field.name = (i > 0) ? tag_from_literal(segments.back().literal) : "";
    if (seg.field.name.empty()) seg.field.name = "Field" + std::to_string(++field_no);
    seg.field.charset = charset_for(chars);
    seg.field.min_len = min_f;
    seg.field.max_len = max_f;
    segments.push_back(std::move(seg));
  }
  return segments;
}

enum CharClassCategory {
  CAT_CONST,
  CAT_DIGIT,
  CAT_UPPER,
  CAT_LOWER,
  CAT_ALNUM,
  CAT_OTHER
};

// Smart Semantic Tokenization (Anti-overfitting for industrial barcodes).
std::vector<SegmentRule> segment_positives_semantic(const std::vector<std::string>& positives) {
  if (positives.empty()) return {};
  if (positives.size() == 1) {
    return segment_positives_exact(positives);
  }

  std::size_t min_len = SIZE_MAX, max_len = 0;
  for (const auto& s : positives) {
    min_len = std::min(min_len, s.size());
    max_len = std::max(max_len, s.size());
  }
  if (min_len == SIZE_MAX) min_len = 0;

  // Handle variable-length inputs with shared constant prefix
  if (min_len != max_len) {
    // Check common constant prefix
    std::size_t prefix_len = 0;
    while (prefix_len < min_len) {
      char c = positives[0][prefix_len];
      bool match = true;
      for (const auto& s : positives) {
        if (s[prefix_len] != c) { match = false; break; }
      }
      if (!match) break;
      ++prefix_len;
    }
    if (prefix_len > 0) {
      std::vector<SegmentRule> segs;
      SegmentRule pseg;
      pseg.is_literal = true;
      pseg.literal = positives[0].substr(0, prefix_len);
      segs.push_back(std::move(pseg));

      std::set<char> var_chars;
      std::size_t min_rem = SIZE_MAX, max_rem = 0;
      for (const auto& s : positives) {
        std::size_t rem = (s.size() > prefix_len) ? s.size() - prefix_len : 0;
        min_rem = std::min(min_rem, rem);
        max_rem = std::max(max_rem, rem);
        for (std::size_t k = prefix_len; k < s.size(); ++k) {
          var_chars.insert(s[k]);
        }
      }
      SegmentRule fseg;
      fseg.is_literal = false;
      std::string tag = tag_from_literal(segs.back().literal);
      fseg.field.name = tag.empty() ? "Field1" : tag;
      fseg.field.charset = charset_for(var_chars);
      fseg.field.min_len = min_rem;
      fseg.field.max_len = max_rem;
      segs.push_back(std::move(fseg));
      return segs;
    }
    return segment_positives_exact(positives);
  }

  // Same-length inputs
  std::vector<CharClassCategory> cats(max_len, CAT_OTHER);
  std::vector<bool> is_const(max_len, false);

  for (std::size_t p = 0; p < max_len; ++p) {
    char first_c = positives[0][p];
    bool const_char = true;
    bool all_digit = true, all_upper = true, all_lower = true, all_alnum = true;

    for (const auto& s : positives) {
      char c = s[p];
      if (c != first_c) const_char = false;
      unsigned char u = static_cast<unsigned char>(c);
      if (!std::isdigit(u)) all_digit = false;
      if (!std::isupper(u)) all_upper = false;
      if (!std::islower(u)) all_lower = false;
      if (!std::isalnum(u)) all_alnum = false;
    }
    is_const[p] = const_char;
    if (const_char) cats[p] = CAT_CONST;
    else if (all_digit) cats[p] = CAT_DIGIT;
    else if (all_upper) cats[p] = CAT_UPPER;
    else if (all_lower) cats[p] = CAT_LOWER;
    else if (all_alnum) cats[p] = CAT_ALNUM;
    else cats[p] = CAT_OTHER;
  }

  bool completely_const = true;
  for (std::size_t p = 0; p < max_len; ++p) {
    if (!is_const[p]) { completely_const = false; break; }
  }
  if (completely_const) {
    SegmentRule seg;
    seg.is_literal = true;
    seg.literal = positives[0];
    return {seg};
  }

  std::vector<SegmentRule> segments;
  int field_no = 0;
  std::size_t p = 0;

  while (p < max_len) {
    if (is_const[p]) {
      std::size_t q = p;
      while (q < max_len && is_const[q]) ++q;

      std::string lit = positives[0].substr(p, q - p);

      // Check if this constant run ends with digits that are part of a variable numeric run
      // BUT if the constant run has a letter after digits (like 3N or 1P), it's a fixed tag marker
      if (q < max_len && cats[q] == CAT_DIGIT) {
        std::size_t digit_start = lit.size();
        while (digit_start > 0 && std::isdigit(static_cast<unsigned char>(lit[digit_start - 1]))) {
          --digit_start;
        }
        if (digit_start < lit.size()) {
          // Keep the non-digit prefix as literal (if any)
          if (digit_start > 0) {
            SegmentRule seg;
            seg.is_literal = true;
            seg.literal = lit.substr(0, digit_start);
            segments.push_back(std::move(seg));
          }

          // The trailing or full constant digits merge with the variable digits
          std::size_t num_start = p + digit_start;
          std::size_t num_end = q;
          while (num_end < max_len && (cats[num_end] == CAT_DIGIT || (is_const[num_end] && std::isdigit(static_cast<unsigned char>(positives[0][num_end]))))) {
            // Stop if followed by a constant letter (e.g. 3N)
            if (is_const[num_end] && num_end + 1 < max_len && is_const[num_end + 1] && std::isalpha(static_cast<unsigned char>(positives[0][num_end + 1]))) {
              break;
            }
            ++num_end;
          }
          std::size_t field_len = num_end - num_start;
          SegmentRule num_seg;
          num_seg.is_literal = false;
          std::string tag = (!segments.empty() && segments.back().is_literal)
                                ? tag_from_literal(segments.back().literal) : "";
          num_seg.field.name = tag.empty() ? "Field" + std::to_string(++field_no) : tag;
          num_seg.field.charset = "\\d";
          num_seg.field.min_len = field_len;
          num_seg.field.max_len = field_len;
          segments.push_back(std::move(num_seg));
          p = num_end;
          continue;
        }
      }

      SegmentRule seg;
      seg.is_literal = true;
      seg.literal = lit;
      segments.push_back(std::move(seg));
      p = q;
    } else if (cats[p] == CAT_DIGIT) {
      std::size_t q = p;
      while (q < max_len && (cats[q] == CAT_DIGIT || (is_const[q] && std::isdigit(static_cast<unsigned char>(positives[0][q]))))) {
        // If constant digit is part of a constant tag (e.g. 3N), stop before it
        if (is_const[q] && q + 1 < max_len && is_const[q + 1] && std::isalpha(static_cast<unsigned char>(positives[0][q + 1]))) {
          break;
        }
        ++q;
      }
      std::size_t field_len = q - p;
      SegmentRule seg;
      seg.is_literal = false;
      std::string tag = (!segments.empty() && segments.back().is_literal)
                            ? tag_from_literal(segments.back().literal) : "";
      seg.field.name = tag.empty() ? "Field" + std::to_string(++field_no) : tag;
      seg.field.charset = "\\d";
      seg.field.min_len = field_len;
      seg.field.max_len = field_len;
      segments.push_back(std::move(seg));
      p = q;
    } else if (cats[p] == CAT_UPPER) {
      std::size_t q = p;
      while (q < max_len && cats[q] == CAT_UPPER) ++q;
      std::size_t field_len = q - p;
      SegmentRule seg;
      seg.is_literal = false;
      std::string tag = (!segments.empty() && segments.back().is_literal)
                            ? tag_from_literal(segments.back().literal) : "";
      seg.field.name = tag.empty() ? "Field" + std::to_string(++field_no) : tag;
      seg.field.charset = "[A-Z]";
      seg.field.min_len = field_len;
      seg.field.max_len = field_len;
      segments.push_back(std::move(seg));
      p = q;
    } else {
      std::size_t q = p;
      while (q < max_len && !is_const[q] && cats[q] != CAT_DIGIT && cats[q] != CAT_UPPER) ++q;
      std::set<char> chars;
      std::size_t min_f = SIZE_MAX, max_f = 0;
      for (const auto& s : positives) {
        std::size_t b = std::min(p, s.size());
        std::size_t e = std::min(q, s.size());
        for (std::size_t k = b; k < e; ++k) chars.insert(s[k]);
        std::size_t n = (e > b) ? e - b : 0;
        min_f = std::min(min_f, n);
        max_f = std::max(max_f, n);
      }
      SegmentRule seg;
      seg.is_literal = false;
      std::string tag = (!segments.empty() && segments.back().is_literal)
                            ? tag_from_literal(segments.back().literal) : "";
      seg.field.name = tag.empty() ? "Field" + std::to_string(++field_no) : tag;
      seg.field.charset = charset_for(chars);
      seg.field.min_len = min_f;
      seg.field.max_len = max_f;
      segments.push_back(std::move(seg));
      p = q;
    }
  }

  return segments;
}

std::vector<SegmentRule> segment_by_lengths(const std::vector<std::string>& positives,
                                            const std::vector<std::size_t>& lengths,
                                            bool semantic,
                                            std::vector<std::string>& warnings) {
  std::vector<SegmentRule> segments;
  std::size_t offset = 0;
  for (std::size_t c = 0; c < lengths.size(); ++c) {
    std::size_t b = offset;
    std::size_t e = offset + lengths[c];
    std::vector<std::string> chunks;
    bool any_short = false;
    for (const auto& s : positives) {
      if (s.size() < e) any_short = true;
      std::size_t bb = std::min(b, s.size());
      std::size_t ee = std::min(e, s.size());
      chunks.push_back(s.substr(bb, ee > bb ? ee - bb : 0));
    }
    if (any_short) {
      warnings.push_back("正例中有条码短于给定长度之和（第 " + std::to_string(c + 1) +
                         " 段），已按现有长度切分");
    }
    auto sub = semantic ? segment_positives_semantic(chunks) : segment_positives_exact(chunks);
    for (auto& seg : sub) segments.push_back(std::move(seg));
    offset = e;
  }
  return segments;
}

std::vector<SegmentRule> segment_by_separators(const std::vector<std::string>& positives,
                                               const std::vector<std::string>& separators,
                                               bool semantic,
                                               std::vector<std::string>& warnings) {
  if (positives.empty() || separators.empty()) return {};

  std::vector<std::string> seps;
  for (const auto& s : separators) {
    std::string t = trim(s);
    if (!t.empty()) seps.push_back(t);
  }
  if (seps.empty()) return {};

  std::vector<std::vector<std::string>> all_chunks;
  std::vector<std::vector<std::string>> all_seps;

  for (const auto& raw_s : positives) {
    std::string s = trim(raw_s);
    std::vector<std::string> chunks;
    std::vector<std::string> matched_seps;

    if (seps.size() == 1) {
      const std::string& sep = seps[0];
      std::size_t start = 0;
      while (start <= s.size()) {
        std::size_t pos = s.find(sep, start);
        if (pos == std::string::npos) {
          chunks.push_back(s.substr(start));
          break;
        } else {
          chunks.push_back(s.substr(start, pos - start));
          matched_seps.push_back(sep);
          start = pos + sep.size();
        }
      }
    } else {
      // Sequential custom markers (e.g. 1P, NQ, 3N or EP, 3N, NG)
      std::size_t start = 0;
      for (std::size_t k = 0; k < seps.size(); ++k) {
        const std::string& sep = seps[k];
        std::size_t pos = s.find(sep, start);
        if (pos != std::string::npos) {
          chunks.push_back(s.substr(start, pos - start));
          matched_seps.push_back(sep);
          start = pos + sep.size();
        } else {
          break;
        }
      }
      chunks.push_back(s.substr(start));
    }

    // Clean any accidental empty trailing chunk if barcode has extra trailing delimiter
    while (chunks.size() > 1 && chunks.back().empty()) {
      chunks.pop_back();
      if (!matched_seps.empty()) matched_seps.pop_back();
    }

    all_chunks.push_back(std::move(chunks));
    all_seps.push_back(std::move(matched_seps));
  }

  std::size_t nchunks = all_chunks[0].size();
  for (std::size_t i = 1; i < all_chunks.size(); ++i) {
    if (all_chunks[i].size() != nchunks) {
      warnings.push_back("各正例的分隔符/标记数量不一致，已改用自动分段");
      return {};
    }
  }

  std::vector<SegmentRule> segments;
  for (std::size_t c = 0; c < nchunks; ++c) {
    std::vector<std::string> col(positives.size());
    for (std::size_t i = 0; i < positives.size(); ++i) {
      col[i] = all_chunks[i][c];
    }
    auto sub = semantic ? segment_positives_semantic(col) : segment_positives_exact(col);
    for (auto& seg : sub) segments.push_back(std::move(seg));

    if (c < all_seps[0].size()) {
      SegmentRule lit;
      lit.is_literal = true;
      lit.literal = all_seps[0][c];
      segments.push_back(std::move(lit));
    }
  }

  return segments;
}

std::vector<SegmentRule> normalize_segments(std::vector<SegmentRule> segments) {
  std::vector<SegmentRule> out;
  for (auto& seg : segments) {
    if (out.empty()) {
      out.push_back(std::move(seg));
      continue;
    }
    auto& prev = out.back();
    if (seg.is_literal && prev.is_literal) {
      prev.literal += seg.literal;
    } else if (!seg.is_literal && !prev.is_literal &&
               prev.field.charset == seg.field.charset &&
               prev.field.name == seg.field.name) {
      prev.field.min_len += seg.field.min_len;
      prev.field.max_len += seg.field.max_len;
    } else {
      out.push_back(std::move(seg));
    }
  }
  return out;
}

struct TagUnit {
  std::size_t letter_begin;
  std::size_t letter_end;
  std::size_t digit_end;
};

std::vector<TagUnit> scan_tag_units(const std::string& s) {
  std::vector<TagUnit> units;
  std::size_t i = 0;
  while (i < s.size()) {
    std::size_t l = i;
    while (l < s.size() && std::isalpha(static_cast<unsigned char>(s[l]))) ++l;
    if (l == i) { ++i; continue; }
    std::size_t d = l;
    while (d < s.size() && std::isdigit(static_cast<unsigned char>(s[d]))) ++d;
    if (d > l) units.push_back({i, l, d});
    i = (d > l) ? d : l;
  }
  return units;
}

void apply_promotes(std::vector<SegmentRule>& segments, const std::vector<std::string>& tags) {
  if (tags.empty()) return;
  std::vector<SegmentRule> out;
  for (auto& seg : segments) {
    if (!seg.is_literal) {
      out.push_back(std::move(seg));
      continue;
    }

    std::string rest = seg.literal;
    std::vector<SegmentRule> pieces;
    bool promoted_any = false;
    while (!rest.empty()) {
      auto units = scan_tag_units(rest);
      std::size_t pick = SIZE_MAX;
      for (std::size_t u = 0; u < units.size(); ++u) {
        std::string tag = rest.substr(units[u].letter_begin,
                                      units[u].letter_end - units[u].letter_begin);
        if (std::find(tags.begin(), tags.end(), tag) != tags.end()) { pick = u; break; }
      }

      if (pick != SIZE_MAX) {
        const TagUnit& unit = units[pick];
        std::string tag = rest.substr(unit.letter_begin, unit.letter_end - unit.letter_begin);
        if (unit.letter_begin > 0) {
          SegmentRule f;
          f.is_literal = true;
          f.literal = rest.substr(0, unit.letter_begin);
          pieces.push_back(std::move(f));
        }
        SegmentRule tag_seg;
        tag_seg.is_literal = true;
        tag_seg.literal = tag;
        pieces.push_back(std::move(tag_seg));

        SegmentRule field_seg;
        field_seg.is_literal = false;
        field_seg.field.name = tag;
        std::set<char> value_chars;
        for (std::size_t k = unit.letter_end; k < unit.digit_end; ++k) value_chars.insert(rest[k]);
        field_seg.field.charset = charset_for(value_chars);
        field_seg.field.min_len = unit.digit_end - unit.letter_end;
        field_seg.field.max_len = unit.digit_end - unit.letter_end;
        pieces.push_back(std::move(field_seg));

        rest = rest.substr(unit.digit_end);
        promoted_any = true;
        continue;
      }

      // Check if any tag in tags is a pure digit chunk (e.g. "2634" or "2026") that is a substring of rest
      bool found_sub = false;
      for (const auto& tag : tags) {
        bool tag_digits = !tag.empty();
        for (char c : tag) {
          if (!std::isdigit(static_cast<unsigned char>(c))) { tag_digits = false; break; }
        }
        if (!tag_digits) continue;

        std::size_t sub_pos = rest.find(tag);
        if (sub_pos != std::string::npos) {
          if (sub_pos > 0) {
            SegmentRule f;
            f.is_literal = true;
            f.literal = rest.substr(0, sub_pos);
            pieces.push_back(std::move(f));
          }
          SegmentRule field_seg;
          field_seg.is_literal = false;
          field_seg.field.name = tag;
          field_seg.field.charset = "\\d";
          field_seg.field.min_len = tag.size();
          field_seg.field.max_len = tag.size();
          pieces.push_back(std::move(field_seg));

          rest = rest.substr(sub_pos + tag.size());
          promoted_any = true;
          found_sub = true;
          break;
        }
      }

      if (!found_sub) {
        break;
      }
    }

    if (!promoted_any) {
      out.push_back(std::move(seg));
      continue;
    }
    if (!rest.empty()) {
      SegmentRule f;
      f.is_literal = true;
      f.literal = rest;
      pieces.push_back(std::move(f));
    }
    for (auto& p : pieces) out.push_back(std::move(p));
  }
  segments = std::move(out);
}

void apply_overrides(std::vector<SegmentRule>& segments, const std::vector<FieldRule>& overrides) {
  if (overrides.empty()) return;
  for (const auto& ov : overrides) {
    bool matched_field = false;
    for (auto& seg : segments) {
      if (!seg.is_literal && ov.name == seg.field.name) {
        seg.field.charset = ov.charset;
        seg.field.min_len = ov.min_len;
        seg.field.max_len = ov.max_len;
        matched_field = true;
      }
    }
    if (!matched_field) {
      std::vector<SegmentRule> next_segs;
      bool replaced_any = false;
      for (auto& seg : segments) {
        if (seg.is_literal && !replaced_any) {
          std::size_t pos = seg.literal.find(ov.name);
          if (pos != std::string::npos) {
            if (pos > 0) {
              SegmentRule pre;
              pre.is_literal = true;
              pre.literal = seg.literal.substr(0, pos);
              next_segs.push_back(std::move(pre));
            }
            SegmentRule f;
            f.is_literal = false;
            f.field.name = ov.name;
            f.field.charset = ov.charset;
            f.field.min_len = ov.min_len;
            f.field.max_len = ov.max_len;
            next_segs.push_back(std::move(f));
            if (pos + ov.name.size() < seg.literal.size()) {
              SegmentRule post;
              post.is_literal = true;
              post.literal = seg.literal.substr(pos + ov.name.size());
              next_segs.push_back(std::move(post));
            }
            replaced_any = true;
            continue;
          }
        }
        next_segs.push_back(std::move(seg));
      }
      if (replaced_any) segments = std::move(next_segs);
    }
  }
}

void name_fields_from_preceding_literals(std::vector<SegmentRule>& segments) {
  int field_no = 0;
  for (std::size_t i = 0; i < segments.size(); ++i) {
    auto& seg = segments[i];
    if (seg.is_literal) continue;
    if (seg.field.name.rfind("Field", 0) == 0) {
      std::string tag;
      if (i > 0 && segments[i - 1].is_literal) tag = tag_from_literal(segments[i - 1].literal);
      seg.field.name = tag.empty() ? "Field" + std::to_string(++field_no) : tag;
    }
  }
}

std::string widen_charset(const std::string& cs) {
  if (cs == "\\d" || cs == "[0-9]") return "[A-Z0-9]";
  if (cs == "[A-Z]") return "[A-Za-z0-9]";
  if (cs == "[a-z]") return "[A-Za-z0-9]";
  if (cs == "[A-Za-z]") return "[A-Za-z0-9]";
  if (cs == "[A-Z0-9]") return "[A-Za-z0-9]";
  if (cs == "[a-z0-9]") return "[A-Za-z0-9]";
  return cs;
}

ExampleVerdict walk_verify(const Candidate& candidate, const std::string& example) {
  ExampleVerdict verdict;
  verdict.example = example;

  std::string cap_re_str = candidate.capturing_regex.empty()
                               ? compose_capturing_regex(candidate.segments)
                               : candidate.capturing_regex;

  bool cap_valid = false;
  std::regex cap_re;
  try {
    cap_re.assign(cap_re_str);
    cap_valid = true;
  } catch (...) {}

  if (cap_valid) {
    std::smatch match;
    if (std::regex_match(example, match, cap_re)) {
      verdict.matched = true;
      verdict.passed = true;
      verdict.failing_segment = SIZE_MAX;
      std::size_t field_idx = 1;
      for (const auto& seg : candidate.segments) {
        if (!seg.is_literal) {
          if (field_idx < match.size()) {
            verdict.field_captures.push_back({seg.field.name, match[field_idx].str()});
            ++field_idx;
          }
        }
      }
      return verdict;
    }
  }

  std::regex re;
  try {
    re.assign(candidate.regex);
    if (std::regex_match(example, re)) {
      verdict.matched = true;
      verdict.passed = true;
      verdict.failing_segment = SIZE_MAX;
      return verdict;
    }
  } catch (...) {}

  verdict.matched = false;
  verdict.passed = false;

  std::size_t pos = 0;
  std::size_t first_fail = SIZE_MAX;
  for (std::size_t i = 0; i < candidate.segments.size(); ++i) {
    const auto& seg = candidate.segments[i];
    if (seg.is_literal) {
      if (example.compare(pos, seg.literal.size(), seg.literal) != 0) {
        first_fail = i;
        break;
      }
      pos += seg.literal.size();
    } else {
      std::size_t run = 0;
      while (run < seg.field.max_len && pos + run < example.size() &&
             charset_matches(seg.field.charset, example[pos + run])) {
        ++run;
      }
      if (run < seg.field.min_len) {
        first_fail = i;
        break;
      }
      pos += run;
    }
  }
  verdict.failing_segment = first_fail;
  return verdict;
}

}  // namespace

std::string compose_regex(const std::vector<SegmentRule>& segments) {
  std::string out = "^";
  for (const auto& seg : segments) {
    if (seg.is_literal) {
      out += escape_literal(seg.literal);
    } else {
      out += seg.field.charset;
      if (seg.field.min_len == 1 && seg.field.max_len == 1) {
        // bare class
      } else if (seg.field.max_len == SIZE_MAX) {
        out += "{" + std::to_string(seg.field.min_len) + ",}";
      } else if (seg.field.min_len == seg.field.max_len) {
        out += "{" + std::to_string(seg.field.min_len) + "}";
      } else {
        out += "{" + std::to_string(seg.field.min_len) + "," + std::to_string(seg.field.max_len) + "}";
      }
    }
  }
  out += "$";
  return out;
}

std::string compose_capturing_regex(const std::vector<SegmentRule>& segments) {
  std::string out = "^";
  for (const auto& seg : segments) {
    if (seg.is_literal) {
      out += escape_literal(seg.literal);
    } else {
      out += "(";
      out += seg.field.charset;
      if (seg.field.min_len == 1 && seg.field.max_len == 1) {
        // bare class
      } else if (seg.field.max_len == SIZE_MAX) {
        out += "{" + std::to_string(seg.field.min_len) + ",}";
      } else if (seg.field.min_len == seg.field.max_len) {
        out += "{" + std::to_string(seg.field.min_len) + "}";
      } else {
        out += "{" + std::to_string(seg.field.min_len) + "," + std::to_string(seg.field.max_len) + "}";
      }
      out += ")";
    }
  }
  out += "$";
  return out;
}

std::string trim(const std::string& s) {
  std::size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  std::size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

bool charset_matches(const std::string& frag, char c) {
  unsigned char u = static_cast<unsigned char>(c);
  if (frag == "\\d" || frag == "[0-9]") return std::isdigit(u) != 0;
  if (frag == "[A-Z]") return std::isupper(u) != 0;
  if (frag == "[a-z]") return std::islower(u) != 0;
  if (frag == "[A-Za-z]") return std::isalpha(u) != 0;
  if (frag == "[A-Z0-9]" || frag == "[0-9A-Z]") return (std::isdigit(u) || std::isupper(u));
  if (frag == "[a-z0-9]" || frag == "[0-9a-z]") return (std::isdigit(u) || std::islower(u));
  if (frag == "[A-Za-z0-9]" || frag == "[0-9A-Za-z]") return std::isalnum(u) != 0;

  if (frag.size() >= 2 && frag.front() == '[' && frag.back() == ']') {
    std::string inner = frag.substr(1, frag.size() - 2);
    bool negate = false;
    std::size_t idx = 0;
    if (!inner.empty() && inner[0] == '^') {
      negate = true;
      idx = 1;
    }
    bool in_set = false;
    while (idx < inner.size()) {
      if (idx + 2 < inner.size() && inner[idx + 1] == '-' && inner[idx] != '\\') {
        char start = inner[idx];
        char end = inner[idx + 2];
        if (c >= start && c <= end) { in_set = true; break; }
        idx += 3;
        continue;
      }
      char target = inner[idx];
      if (target == '\\' && idx + 1 < inner.size()) {
        ++idx;
        target = inner[idx];
      }
      if (c == target) { in_set = true; break; }
      ++idx;
    }
    return negate ? !in_set : in_set;
  }
  return frag.find(c) != std::string::npos;
}

ExampleVerdict verify_example(const Candidate& candidate, const std::string& example) {
  return walk_verify(candidate, example);
}

std::vector<std::string> promotable_tags(const std::string& literal) {
  std::vector<std::string> tags;
  for (const auto& unit : scan_tag_units(literal)) {
    tags.push_back(literal.substr(unit.letter_begin, unit.letter_end - unit.letter_begin));
  }
  if (tags.empty() && !literal.empty()) {
    bool all_punct = true;
    for (char c : literal) {
      if (std::isalnum(static_cast<unsigned char>(c))) { all_punct = false; break; }
    }
    if (!all_punct) {
      tags.push_back(literal);
    }
  }
  return tags;
}

std::vector<std::string> detect_separators(const std::vector<std::string>& positives) {
  if (positives.empty()) return {};

  std::vector<std::string> cleaned;
  for (const auto& s : positives) {
    std::string t = trim(s);
    if (!t.empty()) cleaned.push_back(t);
  }
  if (cleaned.empty()) return {};

  const std::string kDelimChars = "@|_#/:,;";
  std::set<char> found_delims;
  for (char d : kDelimChars) {
    bool consistent = true;
    std::size_t count0 = 0;
    for (char c : cleaned[0]) if (c == d) ++count0;
    if (count0 == 0) continue;
    for (const auto& s : cleaned) {
      std::size_t c_cnt = 0;
      for (char c : s) if (c == d) ++c_cnt;
      if (c_cnt != count0) { consistent = false; break; }
    }
    if (consistent) found_delims.insert(d);
  }

  std::vector<std::string> detected;
  for (char d : found_delims) {
    detected.push_back(std::string(1, d));
  }

  if (cleaned.size() >= 2) {
    std::size_t min_len = SIZE_MAX;
    for (const auto& s : cleaned) min_len = std::min(min_len, s.size());
    std::vector<bool> is_const(min_len, true);
    for (std::size_t p = 0; p < min_len; ++p) {
      char c = cleaned[0][p];
      for (const auto& s : cleaned) {
        if (s[p] != c) { is_const[p] = false; break; }
      }
    }

    std::size_t p = 0;
    while (p < min_len) {
      if (is_const[p]) {
        std::size_t q = p;
        while (q < min_len && is_const[q]) ++q;
        std::string token = cleaned[0].substr(p, q - p);
        if (token.size() >= 2 && token.size() <= 6) {
          bool has_letter = false, has_punct = false;
          for (char c : token) {
            if (std::isalpha(static_cast<unsigned char>(c))) has_letter = true;
            if (found_delims.find(c) != found_delims.end()) has_punct = true;
          }
          if (!has_punct && has_letter && (p > 0 || q < min_len)) {
            if (std::find(detected.begin(), detected.end(), token) == detected.end()) {
              detected.push_back(token);
            }
          }
        }
        p = q;
      } else {
        ++p;
      }
    }
  }

  return detected;
}

GenerationResult generate(const Project& project) {
  GenerationResult result;

  std::vector<std::string> cleaned;
  cleaned.reserve(project.positives.size());
  std::size_t index = 0;
  for (const auto& s : project.positives) {
    ++index;
    std::string t = trim(s);
    if (t != s) {
      result.warnings.push_back("正例 " + std::to_string(index) + " 首尾有空白，已自动去除");
    }
    if (!t.empty()) cleaned.push_back(t);
  }
  if (cleaned.empty()) return result;

  const auto& cfg = project.segment_config;

  if (cfg.use_separators && cfg.separators.size() == 1) {
    const std::string& sep = cfg.separators[0];
    if (!sep.empty()) {
      bool any_has_trailing = false, any_no_trailing = false;
      for (const auto& s : cleaned) {
        if (s.size() > sep.size() && s.rfind(sep) == s.size() - sep.size()) any_has_trailing = true;
        else any_no_trailing = true;
      }
      if (any_has_trailing && any_no_trailing) {
        for (auto& s : cleaned) {
          if (s.size() > sep.size() && s.rfind(sep) == s.size() - sep.size()) {
            s = s.substr(0, s.size() - sep.size());
          }
        }
        result.warnings.push_back("正例末尾多余的分隔符已自动去除并校正");
      }
    }
  }

  // 1. Build the Smart Semantic Segment Model
  std::vector<SegmentRule> smart_segments;
  if (cfg.use_separators && !cfg.separators.empty()) {
    smart_segments = segment_by_separators(cleaned, cfg.separators, true, result.warnings);
    if (smart_segments.empty()) smart_segments = segment_positives_semantic(cleaned);
  } else if (cfg.use_lengths && !cfg.lengths.empty()) {
    smart_segments = segment_by_lengths(cleaned, cfg.lengths, true, result.warnings);
    name_fields_from_preceding_literals(smart_segments);
  } else {
    smart_segments = segment_positives_semantic(cleaned);
  }
  apply_promotes(smart_segments, cfg.promote_tags);
  smart_segments = normalize_segments(std::move(smart_segments));
  apply_overrides(smart_segments, cfg.field_overrides);

  // 2. Build the Strict Exact Positional Segment Model
  std::vector<SegmentRule> exact_segments;
  if (cfg.use_separators && !cfg.separators.empty()) {
    exact_segments = segment_by_separators(cleaned, cfg.separators, false, result.warnings);
    if (exact_segments.empty()) exact_segments = segment_positives_exact(cleaned);
  } else if (cfg.use_lengths && !cfg.lengths.empty()) {
    exact_segments = segment_by_lengths(cleaned, cfg.lengths, false, result.warnings);
    name_fields_from_preceding_literals(exact_segments);
  } else {
    exact_segments = segment_positives_exact(cleaned);
  }
  apply_promotes(exact_segments, cfg.promote_tags);
  exact_segments = normalize_segments(std::move(exact_segments));
  apply_overrides(exact_segments, cfg.field_overrides);

  struct TierDef {
    std::string variant;
    std::string display_name;
    std::vector<SegmentRule> segments;
    bool relax_lengths;
    bool broaden_charsets;
  };

  std::vector<TierDef> tiers;
  tiers.push_back({"smart", "智能业务推荐", smart_segments, false, false});
  tiers.push_back({"relaxed", "弹性变长扩展", smart_segments, true, false});
  tiers.push_back({"broadened", "字符集放宽", smart_segments, false, true});
  tiers.push_back({"exact", "样本严格拟合", exact_segments, false, false});

  auto fill_verdicts = [&](CandidateVerdict& vd) {
    for (const auto& s : cleaned) {
      vd.positive_verdicts.push_back(walk_verify(vd.candidate, s));
    }
    for (const auto& s : project.negatives) {
      auto v = walk_verify(vd.candidate, s);
      v.passed = !v.matched;
      vd.negative_verdicts.push_back(std::move(v));
    }
    std::size_t failures = 0;
    for (const auto& v : vd.positive_verdicts) if (!v.passed) ++failures;
    for (const auto& v : vd.negative_verdicts) if (!v.passed) ++failures;
    vd.failures = failures;
    vd.passes_all = (failures == 0);
  };

  std::vector<CandidateVerdict> passing;
  std::vector<CandidateVerdict> failing;
  std::set<std::string> seen_regexes;

  for (const auto& td : tiers) {
    Candidate cand;
    cand.variant = td.variant;
    cand.display_name = td.display_name;
    cand.segments = td.segments;

    if (td.relax_lengths || td.broaden_charsets) {
      for (auto& seg : cand.segments) {
        if (seg.is_literal) continue;
        if (td.relax_lengths && seg.field.min_len == seg.field.max_len) {
          seg.field.max_len = SIZE_MAX;
        }
        if (td.broaden_charsets) {
          seg.field.charset = widen_charset(seg.field.charset);
        }
      }
    }

    cand.regex = compose_regex(cand.segments);
    cand.capturing_regex = compose_capturing_regex(cand.segments);

    if (seen_regexes.find(cand.regex) != seen_regexes.end()) {
      continue;
    }
    seen_regexes.insert(cand.regex);

    CandidateVerdict vd;
    vd.candidate = std::move(cand);
    fill_verdicts(vd);

    if (vd.passes_all) {
      passing.push_back(std::move(vd));
    } else {
      failing.push_back(std::move(vd));
    }
  }

  result.has_passing = !passing.empty();

  if (result.has_passing) {
    result.candidates = std::move(passing);
    for (auto& f : failing) {
      result.candidates.push_back(std::move(f));
    }
  } else {
    std::sort(failing.begin(), failing.end(), [](const CandidateVerdict& a, const CandidateVerdict& b) {
      if (a.failures != b.failures) return a.failures < b.failures;
      return a.candidate.regex.size() < b.candidate.regex.size();
    });
    result.candidates = std::move(failing);
  }

  return result;
}

GenerationResult generate(const std::vector<std::string>& positives) {
  Project p;
  p.positives = positives;
  return generate(p);
}

}  // namespace regex_tool
