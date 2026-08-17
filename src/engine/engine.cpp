#include "engine/engine.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>

namespace regex_tool {

namespace {

std::string trim_whitespace(const std::string& s) {
  auto is_space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  std::size_t b = 0;
  std::size_t e = s.size();
  while (b < e && is_space(s[b])) ++b;
  while (e > b && is_space(s[e - 1])) --e;
  return s.substr(b, e - b);
}

std::string escape_literal(const std::string& s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '\\': case '.': case '^': case '$': case '[': case ']':
      case '(': case ')': case '{': case '}': case '*': case '+':
      case '?': case '|': case '-':
        out += '\\';
        out += c;
        break;
      default:
        out += c;
    }
  }
  return out;
}

// Classify the distinct characters observed in a field into a regex charset.
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
  // Punctuation, spaces or non-ASCII bytes: emit an explicit class.
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

bool charset_matches(const std::string& frag, char c) {
  unsigned char u = static_cast<unsigned char>(c);
  if (frag == "\\d") return std::isdigit(u) != 0;
  if (frag == "[A-Z]") return std::isupper(u) != 0;
  if (frag == "[a-z]") return std::islower(u) != 0;
  if (frag == "[A-Za-z]") return std::isalpha(u) != 0;
  if (frag == "[A-Z0-9]") return std::isupper(u) != 0 || std::isdigit(u) != 0;
  if (frag == "[a-z0-9]") return std::islower(u) != 0 || std::isdigit(u) != 0;
  if (frag == "[A-Za-z0-9]") return std::isalnum(u) != 0;
  if (frag.size() >= 2 && frag.front() == '[' && frag.back() == ']') {
    for (std::size_t i = 1; i + 1 < frag.size(); ++i) {
      char cc = frag[i];
      if (cc == '\\' && i + 2 < frag.size()) { cc = frag[++i]; }
      if (cc == c) return true;
    }
  }
  return false;
}

// Field name = the trailing run of letters of the literal that precedes it,
// tolerating trailing digits (e.g. "@BN" -> "BN", "@DT2026@LT" -> "LT",
// "@SQ000" -> "SQ"); empty when there is no tag.
std::string tag_from_literal(const std::string& literal) {
  std::size_t end = literal.size();
  while (end > 0 && std::isdigit(static_cast<unsigned char>(literal[end - 1]))) --end;
  std::size_t begin = end;
  while (begin > 0 && std::isalpha(static_cast<unsigned char>(literal[begin - 1]))) --begin;
  return literal.substr(begin, end - begin);
}

// Split the aligned positives into alternating literal/field segments.
// Constancy is judged only within the shared prefix (min_len); everything past
// it is variable, and variable stretches extend to the longest example.
std::vector<SegmentRule> segment_positives(const std::vector<std::string>& positives) {
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
      // The stretch text is identical across all positives; take it from the first.
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

// Length-based segmentation (工单 05): each positive is cut at the given chunk
// sizes, then each chunk is segmented independently by the same constancy logic.
std::vector<SegmentRule> segment_by_lengths(const std::vector<std::string>& positives,
                                            const std::vector<std::size_t>& lengths,
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
    auto sub = segment_positives(chunks);
    for (auto& seg : sub) segments.push_back(std::move(seg));
    offset = e;
  }
  return segments;
}

// Merge adjacent literal segments and adjacent fields with identical charset +
// name (e.g. promoted "SQ\d{3}" next to an existing "SQ\d{1}" -> "SQ\d{4}").
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

// Promote literal segments that contain a listed tag followed by digits (e.g.
// "@DT2026@LT" with tag "DT"): the tag stays as fixed text, the trailing digits
// become a field, and the rest of the literal stays fixed. Constant stretches of
// adjacent segments are merged by segmentation, so a literal may contain several
// tag+value units; each listed tag is promoted in place.
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
    std::size_t cur = 0;
    while (cur < rest.size()) {
      std::size_t l = cur;
      while (l < rest.size() && std::isalpha(static_cast<unsigned char>(rest[l]))) ++l;
      if (l == cur) { ++cur; continue; }  // not a letter run
      std::size_t d = l;
      while (d < rest.size() && std::isdigit(static_cast<unsigned char>(rest[d]))) ++d;
      std::string tag = rest.substr(cur, l - cur);
      bool want = std::find(tags.begin(), tags.end(), tag) != tags.end();
      if (want && d > l) {
        if (cur > 0) {  // fixed text before the tag
          SegmentRule f;
          f.is_literal = true;
          f.literal = rest.substr(0, cur);
          pieces.push_back(std::move(f));
        }
        SegmentRule tag_seg;  // the tag itself stays fixed
        tag_seg.is_literal = true;
        tag_seg.literal = tag;
        pieces.push_back(std::move(tag_seg));

        SegmentRule field_seg;  // the value becomes a field
        field_seg.is_literal = false;
        field_seg.field.name = tag;
        std::set<char> value_chars;
        for (std::size_t k = l; k < d; ++k) value_chars.insert(rest[k]);
        field_seg.field.charset = charset_for(value_chars);
        field_seg.field.min_len = d - l;
        field_seg.field.max_len = d - l;
        pieces.push_back(std::move(field_seg));

        rest = rest.substr(d);  // continue scanning the remainder
        cur = 0;
        promoted_any = true;
        continue;
      }
      cur = (d > l) ? d : l;  // skip this tag+value unit untouched
    }
    if (!promoted_any) {
      out.push_back(std::move(seg));
      continue;
    }
    if (!rest.empty()) {  // trailing fixed text after the last promoted unit
      SegmentRule f;
      f.is_literal = true;
      f.literal = rest;
      pieces.push_back(std::move(f));
    }
    for (auto& p : pieces) out.push_back(std::move(p));
  }
  segments = std::move(out);
}

// Replace inferred field rules by field name (工单 05 rule-table editing).
void apply_overrides(std::vector<SegmentRule>& segments, const std::vector<FieldRule>& overrides) {
  if (overrides.empty()) return;
  for (auto& seg : segments) {
    if (seg.is_literal) continue;
    for (const auto& ov : overrides) {
      if (ov.name == seg.field.name) {
        seg.field.charset = ov.charset;
        seg.field.min_len = ov.min_len;
        seg.field.max_len = ov.max_len;
      }
    }
  }
}

// Length-mode fields get positional names (Field1…); when a field directly
// follows a fixed chunk, name it after that chunk's tag instead.
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

std::string compose_regex(const std::vector<SegmentRule>& segments) {
  std::string out = "^";
  for (const auto& seg : segments) {
    if (seg.is_literal) {
      out += escape_literal(seg.literal);
    } else {
      out += seg.field.charset;
      if (seg.field.min_len == 1 && seg.field.max_len == 1) {
        // bare class, e.g. \d — {1} would be noise
      } else if (seg.field.max_len == SIZE_MAX) {
        out += "{" + std::to_string(seg.field.min_len) + ",}";  // relaxed length
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

// Broaden an observed-only charset for the "broadened" variant (工单 06).
std::string widen_charset(const std::string& cs) {
  if (cs == "\\d" || cs == "[0-9]") return "[A-Z0-9]";
  if (cs == "[A-Z]") return "[A-Za-z0-9]";
  if (cs == "[a-z]") return "[A-Za-z0-9]";
  if (cs == "[A-Za-z]") return "[A-Za-z0-9]";
  if (cs == "[A-Z0-9]") return "[A-Za-z0-9]";
  if (cs == "[a-z0-9]") return "[A-Za-z0-9]";
  return cs;  // custom class or already maximal
}

// Walk one example against the segment model: authoritative full-match plus a
// segment-level diagnosis (where the walk first failed) and field captures.
// `passed` is set to the positive polarity (passed == matched); callers flip it
// for negatives.
ExampleVerdict walk_verify(const Candidate& candidate, const std::string& example) {
  ExampleVerdict verdict;
  verdict.example = example;

  // Authoritative check: anchored full match with the same engine style we emit.
  verdict.matched = std::regex_match(example, std::regex(candidate.regex));
  verdict.passed = verdict.matched;

  // Diagnostic + captures: greedy walk over the segment model.
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
      if (verdict.matched) {
        verdict.field_captures.emplace_back(seg.field.name, example.substr(pos, run));
      }
      pos += run;
    }
  }
  verdict.failing_segment = verdict.matched ? SIZE_MAX : first_fail;
  return verdict;
}

}  // namespace

ExampleVerdict verify_example(const Candidate& candidate, const std::string& example) {
  // Self-test polarity: "does this barcode match?" — passed == matched.
  return walk_verify(candidate, example);
}

GenerationResult generate(const Project& project) {
  GenerationResult result;

  // Input hygiene (工单 04): trim whitespace, report it, never fold it into rules.
  std::vector<std::string> cleaned;
  cleaned.reserve(project.positives.size());
  std::size_t index = 0;
  for (const auto& s : project.positives) {
    ++index;
    std::string t = trim_whitespace(s);
    if (t != s) {
      result.warnings.push_back("正例 " + std::to_string(index) + " 首尾有空白，已自动去除");
    }
    cleaned.push_back(t);
  }
  if (cleaned.empty()) return result;  // nothing to infer from

  std::vector<SegmentRule> segments;
  const auto& cfg = project.segment_config;
  if (cfg.use_lengths && !cfg.lengths.empty()) {
    segments = segment_by_lengths(cleaned, cfg.lengths, result.warnings);
    name_fields_from_preceding_literals(segments);
  } else {
    segments = segment_positives(cleaned);
  }
  apply_promotes(segments, cfg.promote_tags);
  segments = normalize_segments(std::move(segments));
  apply_overrides(segments, cfg.field_overrides);

  Candidate base;
  base.variant = "exact";
  base.segments = std::move(segments);
  base.regex = compose_regex(base.segments);

  // Variant axes (工单 06): length exactness and charset tightness.
  struct VariantSpec {
    const char* name;
    bool relax_lengths;
    bool broaden_charsets;
  };
  const VariantSpec kVariants[] = {
      {"exact", false, false},
      {"relaxed", true, false},
      {"broadened", false, true},
  };

  auto fill_verdicts = [&](CandidateVerdict& vd) {
    for (const auto& s : cleaned) {
      vd.positive_verdicts.push_back(walk_verify(vd.candidate, s));
    }
    for (const auto& s : project.negatives) {
      auto v = walk_verify(vd.candidate, s);
      v.passed = !v.matched;  // negative polarity: correctly rejected = passed
      vd.negative_verdicts.push_back(std::move(v));
    }
    std::size_t failures = 0;
    for (const auto& v : vd.positive_verdicts) if (!v.passed) ++failures;
    for (const auto& v : vd.negative_verdicts) if (!v.passed) ++failures;
    vd.failures = failures;
    vd.passes_all = failures == 0;
  };

  std::vector<CandidateVerdict> passing;
  std::vector<CandidateVerdict> failing;
  for (const auto& spec : kVariants) {
    CandidateVerdict vd;
    vd.candidate = base;
    vd.candidate.variant = spec.name;
    if (spec.relax_lengths || spec.broaden_charsets) {
      for (auto& seg : vd.candidate.segments) {
        if (seg.is_literal) continue;
        if (spec.relax_lengths && seg.field.min_len == seg.field.max_len) {
          seg.field.max_len = SIZE_MAX;  // {n,}
        }
        if (spec.broaden_charsets) seg.field.charset = widen_charset(seg.field.charset);
      }
      vd.candidate.regex = compose_regex(vd.candidate.segments);
    }
    fill_verdicts(vd);
    (vd.passes_all ? passing : failing).push_back(std::move(vd));
  }

  // Simpler first: shorter regex wins, then the stricter variant.
  auto simpler = [](const CandidateVerdict& a, const CandidateVerdict& b) {
    if (a.candidate.regex.size() != b.candidate.regex.size()) {
      return a.candidate.regex.size() < b.candidate.regex.size();
    }
    return a.candidate.variant < b.candidate.variant;
  };
  auto closer = [&](const CandidateVerdict& a, const CandidateVerdict& b) {
    if (a.failures != b.failures) return a.failures < b.failures;
    return simpler(a, b);
  };
  std::sort(passing.begin(), passing.end(), simpler);
  std::sort(failing.begin(), failing.end(), closer);

  result.has_passing = !passing.empty();
  if (!passing.empty()) {
    result.candidates = std::move(passing);  // recommended candidate first
  } else {
    result.candidates = std::move(failing);  // 工单 09: closest candidates, no silent empty
  }
  return result;
}

GenerationResult generate(const std::vector<std::string>& positives) {
  Project p;
  p.positives = positives;
  return generate(p);
}

}  // namespace regex_tool
