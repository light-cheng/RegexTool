// Engine tests — the single pre-agreed seam: the engine's public interface.
// Expected values come from known-good sources (the user's real examples and
// their hand-written target regex), never recomputed from the implementation.

#include "engine/engine.h"

#include <cstdio>
#include <string>
#include <vector>

#include <sstream>

static int g_failures = 0;

template <typename T>
std::string format_val(const T& val) {
  std::ostringstream oss;
  oss << val;
  return oss.str();
}

inline std::string format_val(const std::string& val) {
  return "\"" + val + "\"";
}

inline std::string format_val(const char* val) {
  return val ? ("\"" + std::string(val) + "\"") : "nullptr";
}

inline std::string format_val(bool val) {
  return val ? "true" : "false";
}

template <typename A, typename B>
void check_eq_impl(const char* file, int line, const char* a_str, const char* b_str, const A& a, const B& b) {
  if (!(a == b)) {
    std::printf("FAIL %s:%d: %s == %s [actual: %s, expected: %s]\n",
                file, line, a_str, b_str, format_val(a).c_str(), format_val(b).c_str());
    ++g_failures;
  }
}

template <typename A, typename B>
void check_ne_impl(const char* file, int line, const char* a_str, const char* b_str, const A& a, const B& b) {
  if (!(a != b)) {
    std::printf("FAIL %s:%d: %s != %s [both are: %s]\n",
                file, line, a_str, b_str, format_val(a).c_str());
    ++g_failures;
  }
}

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)

#define CHECK_EQ(a, b) check_eq_impl(__FILE__, __LINE__, #a, #b, (a), (b))
#define CHECK_NE(a, b) check_ne_impl(__FILE__, __LINE__, #a, #b, (a), (b))

namespace {

// The user's real positive examples (field tags PN/BN/DT/LT/SQ/WK, "@" separators).
const std::vector<std::string> kRealExamples = {
    "PN8821@BN9012@DT2026@LT3344@SQ0001@WK12",
    "PN4503@BN7741@DT2026@LT5566@SQ0002@WK15",
    "PN1192@BN3325@DT2026@LT1122@SQ0003@WK08",
    "PN6678@BN5589@DT2026@LT9900@SQ0004@WK23",
    "PN3345@BN1102@DT2026@LT4488@SQ0005@WK04",
};

// The user's hand-written target regex is "^PN\d{4}@BN\d{4}@DT2026@LT\d{4}@SQ\d{4}@WK\d{2}$".
// Honest data-driven inference cannot reproduce it exactly: the samples are
// SQ0001..SQ0005, so the leading "000" is constant in the data and the tool
// keeps it as fixed text. Promoting "SQ000" back to a field is ticket 05.
// This test pins the honest output; the two regexes differ only there.
const char* kExpectedRegex =
    "^PN\\d{4}@BN\\d{4}@DT2026@LT\\d{4}@SQ000\\d@WK\\d{2}$";

// The candidate regex of a result is its first (recommended) candidate.
const std::string& first_regex(const regex_tool::GenerationResult& r) {
  return r.candidates.front().candidate.regex;
}

void test_real_examples_produce_expected_regex() {
  auto result = regex_tool::generate(kRealExamples);
  CHECK_EQ(first_regex(result), std::string(kExpectedRegex));
  const auto& vd = result.candidates.front();
  CHECK_EQ(vd.positive_verdicts.size(), kRealExamples.size());
  for (const auto& v : vd.positive_verdicts) {
    CHECK(v.passed);
    CHECK(v.matched);
    CHECK_EQ(v.failing_segment, SIZE_MAX);
  }
  CHECK(vd.passes_all);
}

void test_field_names_and_rules_follow_tags() {
  auto result = regex_tool::generate(kRealExamples);
  const auto& segments = result.candidates.front().candidate.segments;
  std::vector<std::string> names;
  std::vector<std::string> charsets;
  std::vector<std::pair<std::size_t, std::size_t>> lens;
  for (const auto& seg : segments) {
    if (!seg.is_literal) {
      names.push_back(seg.field.name);
      charsets.push_back(seg.field.charset);
      lens.emplace_back(seg.field.min_len, seg.field.max_len);
    }
  }
  CHECK_EQ(names.size(), 5u);
  if (names.size() == 5u) {
    CHECK_EQ(names[0], "PN");
    CHECK_EQ(names[1], "BN");
    CHECK_EQ(names[2], "LT");
    CHECK_EQ(names[3], "SQ");   // tag extracted from the literal "@SQ000"
    CHECK_EQ(names[4], "WK");
    for (const auto& cs : charsets) CHECK_EQ(cs, "\\d");
    CHECK_EQ(lens[0].first, 4u);
    CHECK_EQ(lens[0].second, 4u);
    CHECK_EQ(lens[3].first, 1u);  // only the last digit varies in the data
    CHECK_EQ(lens[3].second, 1u);
    CHECK_EQ(lens[4].first, 2u);
    CHECK_EQ(lens[4].second, 2u);
  }
}

void test_field_captures_match_actual_values() {
  auto result = regex_tool::generate(kRealExamples);
  const auto& vd = result.candidates.front();
  // Fourth field captures only the varying digit; "000" stays in the literal.
  const std::vector<std::vector<std::string>> expected = {
      {"8821", "9012", "3344", "1", "12"},
      {"4503", "7741", "5566", "2", "15"},
      {"1192", "3325", "1122", "3", "08"},
      {"6678", "5589", "9900", "4", "23"},
      {"3345", "1102", "4488", "5", "04"},
  };
  for (std::size_t i = 0; i < vd.positive_verdicts.size(); ++i) {
    const auto& v = vd.positive_verdicts[i];
    CHECK_EQ(v.field_captures.size(), expected[i].size());
    for (std::size_t j = 0; j < v.field_captures.size() && j < expected[i].size(); ++j) {
      CHECK_EQ(v.field_captures[j].second, expected[i][j]);
    }
  }
}

void test_varying_length_field_emits_range() {
  auto result = regex_tool::generate({"AB1", "AB22", "AB333"});
  CHECK_EQ(first_regex(result), std::string("^AB\\d{1,3}$"));
  const auto& vd = result.candidates.front();
  for (const auto& v : vd.positive_verdicts) CHECK(v.passed);
  CHECK(vd.passes_all);
}

void test_letter_charset_inferred() {
  auto result = regex_tool::generate({"XXabc", "XXxyz"});
  // Observed chars are all lowercase, so the class is tight: [a-z].
  CHECK_EQ(first_regex(result), std::string("^XX[a-z]{3}$"));
}

void test_literal_escaping() {
  auto result = regex_tool::generate({"A1.B2", "A3.B4"});
  // The separator "." is a literal; regex metacharacters must be escaped.
  CHECK_EQ(first_regex(result), std::string("^A\\d\\.B\\d$"));
}

void test_all_identical_positives_emit_literal_regex() {
  auto result = regex_tool::generate({"FOO", "FOO"});
  CHECK_EQ(first_regex(result), std::string("^FOO$"));
  const auto& vd = result.candidates.front();
  for (const auto& v : vd.positive_verdicts) CHECK(v.passed);
  CHECK(vd.passes_all);
}

void test_single_field_when_no_constant_positions() {
  auto result = regex_tool::generate({"abc", "xyz"});
  CHECK_EQ(first_regex(result), std::string("^[a-z]{3}$"));
}

void test_leading_trailing_whitespace_trimmed_with_warning() {
  // Real data came in with a trailing space on one example; it must not leak
  // into the rules (工单 04: the trim surfaces as an explicit warning).
  auto result = regex_tool::generate({"AB12 ", " AB34"});
  CHECK_EQ(first_regex(result), std::string("^AB\\d{2}$"));
  const auto& vd = result.candidates.front();
  for (const auto& v : vd.positive_verdicts) CHECK(v.passed);
  CHECK(!result.warnings.empty());
  bool found = false;
  for (const auto& w : result.warnings) {
    if (w.find("空格") != std::string::npos || w.find("空白") != std::string::npos) found = true;
  }
  CHECK(found);
}

void test_empty_input_returns_empty_candidate() {
  auto result = regex_tool::generate(regex_tool::Project{});
  CHECK_EQ(result.candidates.size(), 0u);
}

// ---- 工单 02: negatives ----

void test_negative_with_different_structure_rejected() {
  regex_tool::Project p;
  p.positives = kRealExamples;
  p.negatives = {"THIS-IS-NOT-A-BARCODE", "PN8821@BN9012"};  // wrong shape / truncated
  auto result = regex_tool::generate(p);
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
  CHECK_EQ(vd.failures, 0u);
  CHECK_EQ(vd.negative_verdicts.size(), 2u);
  for (const auto& v : vd.negative_verdicts) {
    CHECK(v.passed);       // correctly rejected
    CHECK(!v.matched);
    CHECK_NE(v.failing_segment, SIZE_MAX);  // the walk points at where it was rejected
  }
}

void test_negative_with_invalid_field_content_points_at_field() {
  regex_tool::Project p;
  p.positives = kRealExamples;
  // Same structure, but the PN field (4 digits) contains a letter -> rejected
  // by the field rule, and the verdict must point at the PN segment.
  p.negatives = {"PN88A1@BN9012@DT2026@LT3344@SQ0001@WK12"};
  auto result = regex_tool::generate(p);
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
  const auto& v = vd.negative_verdicts.front();
  CHECK(v.passed);  // rejected, which is what we want for a negative
  CHECK(!v.matched);
  CHECK_NE(v.failing_segment, SIZE_MAX);
  // The segment it points at must be the PN field.
  if (v.failing_segment != SIZE_MAX) {
    const auto& seg = vd.candidate.segments[v.failing_segment];
    CHECK(!seg.is_literal);
    CHECK_EQ(seg.field.name, "PN");
  }
}

void test_negative_that_matches_is_a_leak() {
  regex_tool::Project p;
  p.positives = kRealExamples;
  // This negative is actually a valid barcode of the same format: it must leak.
  p.negatives = {"PN9999@BN8888@DT2026@LT7777@SQ0006@WK99"};
  auto result = regex_tool::generate(p);
  const auto& vd = result.candidates.front();
  CHECK(!vd.passes_all);
  CHECK_EQ(vd.failures, 1u);
  const auto& v = vd.negative_verdicts.front();
  CHECK(!v.passed);
  CHECK(v.matched);
  CHECK_EQ(v.failing_segment, SIZE_MAX);  // whole string matched
}

void test_promoted_sq_field_accepts_any_digit_four() {
  // The honest inference keeps "000" fixed; with the SQ tag promoted to a field
  // (工单 05 machinery exercised via overrides), SQ must be \d{4} and a barcode
  // with a different prefix still matches. This pins the verify side used by
  // the self-test box: a candidate regex is judged as-is.
  regex_tool::Candidate c;
  c.regex = "^PN\\d{4}@BN\\d{4}@DT2026@LT\\d{4}@SQ\\d{4}@WK\\d{2}$";
  c.segments = {
      {true, "PN", {}},             {false, "", {"PN", "\\d", 4, 4}},
      {true, "@BN", {}},            {false, "", {"BN", "\\d", 4, 4}},
      {true, "@DT2026", {}},        {true, "@LT", {}},
      {false, "", {"LT", "\\d", 4, 4}}, {true, "@SQ", {}},
      {false, "", {"SQ", "\\d", 4, 4}}, {true, "@WK", {}},
      {false, "", {"WK", "\\d", 2, 2}},
  };
  auto ok = regex_tool::verify_example(c, "PN1234@BN5678@DT2026@LT9012@SQ0007@WK42");
  CHECK(ok.passed);
  CHECK(ok.matched);
  auto bad = regex_tool::verify_example(c, "PN1234@BN5678@DT2025@LT9012@SQ0007@WK42");
  CHECK(!bad.passed);
  CHECK(!bad.matched);
}

// ---- 工单 07: self-test via the same verification logic ----

void test_verify_example_pass_and_fail_with_segment() {
  auto result = regex_tool::generate(kRealExamples);
  const auto& c = result.candidates.front().candidate;

  auto pass = regex_tool::verify_example(c, "PN9999@BN8888@DT2026@LT7777@SQ0006@WK99");
  CHECK(pass.passed);
  CHECK_EQ(pass.failing_segment, SIZE_MAX);
  CHECK_EQ(pass.field_captures.size(), 5u);
  if (pass.field_captures.size() == 5u) {
    CHECK_EQ(pass.field_captures[0].second, "9999");
    CHECK_EQ(pass.field_captures[4].second, "99");
  }

  auto fail_structure = regex_tool::verify_example(c, "NO-SEPARATOR-HERE");
  CHECK(!fail_structure.passed);
  CHECK_NE(fail_structure.failing_segment, SIZE_MAX);

  auto fail_field = regex_tool::verify_example(c, "PN88A1@BN9012@DT2026@LT3344@SQ0001@WK12");
  CHECK(!fail_field.passed);
  CHECK_NE(fail_field.failing_segment, SIZE_MAX);
  if (fail_field.failing_segment != SIZE_MAX) {
    const auto& seg = c.segments[fail_field.failing_segment];
    CHECK(!seg.is_literal);
    CHECK_EQ(seg.field.name, "PN");
  }
}

// ---- 工单 04: non-ASCII preserved ----

void test_non_ascii_preserved_in_literal() {
  auto result = regex_tool::generate({"ABC\xC3\xA9" "12", "ABC\xC3\xA9" "34"});  // "ABCé12"
  CHECK_EQ(first_regex(result), std::string("^ABC\xC3\xA9" "\\d{2}$"));
  const auto& vd = result.candidates.front();
  for (const auto& v : vd.positive_verdicts) CHECK(v.passed);
}

void test_empty_negatives_ok() {
  regex_tool::Project p;
  p.positives = kRealExamples;
  auto result = regex_tool::generate(p);
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
  CHECK_EQ(vd.negative_verdicts.size(), 0u);
}

// ---- 工单 03: alphanumeric fixed markers + single-segment format ----

void test_alnum_marker_segmentation() {
  // European format: 1P / NQ / 3N are alphanumeric fixed markers acting as
  // separators; they must stay as literals while the variable runs become fields.
  auto result = regex_tool::generate({
      "1P123456NQ7890123N3456",
      "1P654321NQ2109873N7890",
      "1P111222NQ3334443N5555",
  });
  CHECK_EQ(first_regex(result), std::string("^1P\\d{6}NQ\\d{6}3N\\d{4}$"));
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
  // Markers appear as literal segments, in order.
  std::string literals;
  for (const auto& seg : vd.candidate.segments) {
    if (seg.is_literal) literals += seg.literal;
  }
  CHECK_EQ(literals, std::string("1PNQ3N"));
}

void test_single_segment_without_separator_or_marker() {
  // No separators, no markers: the tool must still produce a regex without any
  // segmentation configuration.
  auto result = regex_tool::generate({"ABC123", "ABD456"});
  CHECK_EQ(first_regex(result), std::string("^AB[A-Z0-9]{4}$"));
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
}

void test_marker_segmentation_consistent_across_examples() {
  // The same marker segmentation must hold for every positive: an example that
  // does not fit the shared structure is a different format, not a marker break.
  auto result = regex_tool::generate({
      "1P123456NQ7890123N3456",
      "1P654321NQ2109873N7890",
  });
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
  CHECK_EQ(vd.positive_verdicts.size(), 2u);
  for (const auto& v : vd.positive_verdicts) CHECK(v.passed);
}

// ---- 工单 05: length-based segmentation, promote, overrides ----

void test_length_based_segmentation() {
  regex_tool::Project p;
  p.positives = {"ABCD1234EF", "ABCD5678GH"};
  p.segment_config.use_lengths = true;
  p.segment_config.lengths = {4, 4, 2};
  auto result = regex_tool::generate(p);
  CHECK_EQ(first_regex(result), std::string("^ABCD\\d{4}[A-Z]{2}$"));
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
  // Field names follow the preceding fixed chunk (the tag of "ABCD"); a field
  // with no preceding literal keeps its positional name.
  std::vector<std::string> names;
  for (const auto& seg : vd.candidate.segments) {
    if (!seg.is_literal) names.push_back(seg.field.name);
  }
  CHECK_EQ(names.size(), 2u);
  if (names.size() == 2u) {
    CHECK_EQ(names[0], "ABCD");
    CHECK_EQ(names[1], "Field1");  // no preceding literal chunk; positional name
  }
}

void test_length_based_short_example_warns() {
  regex_tool::Project p;
  p.positives = {"ABCD1234EF", "ABCD12"};  // second is shorter than 4+4+2
  p.segment_config.use_lengths = true;
  p.segment_config.lengths = {4, 4, 2};
  auto result = regex_tool::generate(p);
  CHECK(!result.warnings.empty());
  CHECK_EQ(result.candidates.front().positive_verdicts.size(), 2u);
}

void test_promote_constant_segment_to_field_sq() {
  // SQ000 decision payoff: promoting the "SQ" tag turns the constant "000" into
  // a field and merges with the trailing digit field — the user's hand-written
  // target regex comes out exactly.
  regex_tool::Project p;
  p.positives = kRealExamples;
  p.segment_config.promote_tags = {"SQ"};
  auto result = regex_tool::generate(p);
  CHECK_EQ(first_regex(result),
           std::string("^PN\\d{4}@BN\\d{4}@DT2026@LT\\d{4}@SQ\\d{4}@WK\\d{2}$"));
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
  // Captures now include the full four digits of SQ.
  const auto& v = vd.positive_verdicts.front();
  CHECK_EQ(v.field_captures.size(), 5u);
  if (v.field_captures.size() == 5u) {
    CHECK_EQ(v.field_captures[3].first, "SQ");
    CHECK_EQ(v.field_captures[3].second, "0001");
  }
}

void test_promote_constant_segment_to_field_dt() {
  regex_tool::Project p;
  p.positives = kRealExamples;
  p.segment_config.promote_tags = {"DT"};
  auto result = regex_tool::generate(p);
  CHECK_EQ(first_regex(result),
           std::string("^PN\\d{4}@BN\\d{4}@DT\\d{4}@LT\\d{4}@SQ000\\d@WK\\d{2}$"));
  CHECK(result.candidates.front().passes_all);
}

void test_field_override_by_name() {
  regex_tool::Project p;
  p.positives = kRealExamples;
  regex_tool::FieldRule ov;
  ov.name = "PN";
  ov.charset = "\\d";  // keep digits, widen the width the data satisfies
  ov.min_len = 2;
  ov.max_len = 6;
  p.segment_config.field_overrides = {ov};
  auto result = regex_tool::generate(p);
  // The override is applied by field name: the recommended exact candidate
  // carries it and still passes all examples.
  CHECK(result.has_passing);
  bool found_exact = false;
  for (const auto& vd : result.candidates) {
    if (vd.candidate.variant == "exact") {
      found_exact = true;
      CHECK_EQ(vd.candidate.regex,
               std::string("^PN\\d{2,6}@BN\\d{4}@DT2026@LT\\d{4}@SQ000\\d@WK\\d{2}$"));
      CHECK(vd.passes_all);
    }
  }
  CHECK(found_exact);
}

void test_length_based_segmentation_with_promote() {
  // Promote works in length mode too: the value of a constant chunk (tag AB +
  // value 12) becomes a field while the tag stays fixed.
  regex_tool::Project p;
  p.positives = {"AB1234", "AB1234"};
  p.segment_config.use_lengths = true;
  p.segment_config.lengths = {4, 2};
  p.segment_config.promote_tags = {"AB"};
  auto result = regex_tool::generate(p);
  CHECK_EQ(first_regex(result), std::string("^AB\\d{2}34$"));
  CHECK(result.candidates.front().passes_all);
}

// ---- 工单 06: multi-candidate generation, filtering, ranking ----

void test_multiple_candidates_generated_and_ranked() {
  auto result = regex_tool::generate(kRealExamples);
  CHECK(result.has_passing);
  CHECK_EQ(result.candidates.size(), 3u);
  CHECK_EQ(result.candidates[0].candidate.variant, std::string("exact"));
  // Sorted by simplicity: non-decreasing regex length, all pass.
  for (std::size_t i = 1; i < result.candidates.size(); ++i) {
    CHECK(result.candidates[i - 1].candidate.regex.size() <=
          result.candidates[i].candidate.regex.size());
    CHECK(result.candidates[i].passes_all);
  }
}

void test_variant_regex_texts() {
  // Exact lengths: base keeps {1} as a bare class, relaxed widens to {1,}.
  auto result = regex_tool::generate({"AB1", "AB2"});
  bool found_base = false, found_relaxed = false, found_broadened = false;
  for (const auto& vd : result.candidates) {
    if (vd.candidate.regex == "^AB\\d$") found_base = true;
    if (vd.candidate.regex == "^AB\\d{1,}$") found_relaxed = true;
    if (vd.candidate.regex == "^AB[A-Z0-9]$") found_broadened = true;
  }
  CHECK(found_base);
  CHECK(found_relaxed);
  CHECK(found_broadened);
}

void test_candidates_filtered_when_negative_leaks_under_relaxation() {
  regex_tool::Project p;
  p.positives = kRealExamples;
  // 5-digit PN leaks the relaxed-length variant; letter-in-PN leaks the
  // broadened-charset variant. The exact candidate rejects both.
  p.negatives = {"PN88211@BN9012@DT2026@LT3344@SQ0001@WK12",
                 "PN8A21@BN9012@DT2026@LT3344@SQ0001@WK12"};
  auto result = regex_tool::generate(p);
  CHECK(result.has_passing);
  CHECK_EQ(result.candidates.size(), 1u);
  CHECK_EQ(result.candidates.front().candidate.variant, std::string("exact"));
  CHECK_EQ(result.candidates.front().candidate.regex, std::string(kExpectedRegex));
}

// ---- 工单 09: fallback when nothing passes ----

// ---- 显式分隔符分段（单/多） ----

void test_explicit_separator_matches_automatic() {
  // Explicit "@" segmentation reproduces the automatic result on the real data.
  regex_tool::Project p;
  p.positives = kRealExamples;
  p.segment_config.use_separators = true;
  p.segment_config.separators = {"@"};
  auto result = regex_tool::generate(p);
  CHECK_EQ(first_regex(result), std::string(kExpectedRegex));
  CHECK(result.candidates.front().passes_all);
  CHECK(result.warnings.empty());
}

void test_multiple_separators() {
  regex_tool::Project p;
  p.positives = {"AB12|CD34@EF56", "AB78|CD90@EF12"};
  p.segment_config.use_separators = true;
  p.segment_config.separators = {"|", "@"};
  auto result = regex_tool::generate(p);
  CHECK_EQ(first_regex(result), std::string("^AB\\d{2}\\|CD\\d{2}@EF\\d{2}$"));
  CHECK(result.candidates.front().passes_all);
}

void test_separator_position_mismatch_falls_back_with_warning() {
  regex_tool::Project p;
  p.positives = {"AB12|CD34", "AB|CD34@EF"};  // "|" at different positions
  p.segment_config.use_separators = true;
  p.segment_config.separators = {"|"};
  auto result = regex_tool::generate(p);
  CHECK(!result.warnings.empty());
  // Falls back to positional segmentation: the result still validates.
  CHECK(result.candidates.front().passes_all);
}

void test_separator_mode_with_promote_sq() {
  // The SQ000 payoff works in explicit-separator mode too.
  regex_tool::Project p;
  p.positives = kRealExamples;
  p.segment_config.use_separators = true;
  p.segment_config.separators = {"@"};
  p.segment_config.promote_tags = {"SQ"};
  auto result = regex_tool::generate(p);
  CHECK_EQ(first_regex(result),
           std::string("^PN\\d{4}@BN\\d{4}@DT2026@LT\\d{4}@SQ\\d{4}@WK\\d{2}$"));
  CHECK(result.candidates.front().passes_all);
}

void test_none_pass_fallback_keeps_closest() {
  regex_tool::Project p;
  p.positives = {"AB1", "AB2"};
  p.negatives = {"AB1"};  // identical to a positive: no candidate can satisfy both
  auto result = regex_tool::generate(p);
  CHECK(!result.has_passing);
  CHECK_EQ(result.candidates.size(), 3u);  // nothing is silently dropped
  for (const auto& vd : result.candidates) {
    CHECK(!vd.passes_all);
    CHECK_EQ(vd.failures, 1u);
  }
  // Closest first: fewest failures, then simplest.
  for (std::size_t i = 1; i < result.candidates.size(); ++i) {
    CHECK(result.candidates[i - 1].failures <= result.candidates[i].failures);
    CHECK(result.candidates[i - 1].candidate.regex.size() <=
          result.candidates[i].candidate.regex.size());
  }
  // The failing example is identified in the verdicts (leak: whole string matched).
  bool leak_found = false;
  for (const auto& v : result.candidates.front().negative_verdicts) {
    if (!v.passed) {
      leak_found = true;
      CHECK(v.matched);
      CHECK_EQ(v.failing_segment, SIZE_MAX);
    }
  }
  CHECK(leak_found);
}

void test_invalid_regex_no_crash() {
  regex_tool::Candidate c;
  c.regex = "^PN[0-9";  // unclosed bracket
  c.segments = {{true, "PN", {}}, {false, "", {"F1", "[0-9", 1, 1}}};
  auto v = regex_tool::verify_example(c, "PN1");
  CHECK(!v.passed);
  CHECK(!v.matched);
}

void test_charset_matches_ranges_and_custom_classes() {
  CHECK(regex_tool::charset_matches("[0-9A-F]", '0'));
  CHECK(regex_tool::charset_matches("[0-9A-F]", '9'));
  CHECK(regex_tool::charset_matches("[0-9A-F]", 'A'));
  CHECK(regex_tool::charset_matches("[0-9A-F]", 'F'));
  CHECK(!regex_tool::charset_matches("[0-9A-F]", 'G'));
  CHECK(!regex_tool::charset_matches("[0-9A-F]", 'a'));

  CHECK(regex_tool::charset_matches("[a-z0-9]", 'a'));
  CHECK(regex_tool::charset_matches("[a-z0-9]", 'z'));
  CHECK(regex_tool::charset_matches("[a-z0-9]", '5'));
  CHECK(!regex_tool::charset_matches("[a-z0-9]", 'A'));

  CHECK(regex_tool::charset_matches("[^0-9]", 'A'));
  CHECK(!regex_tool::charset_matches("[^0-9]", '3'));
}

void test_field_captures_adjacent_fields() {
  regex_tool::Candidate c;
  c.regex = "^\\d{2}\\d{3}$";
  c.segments = {
      {false, "", {"F1", "\\d", 2, 2}},
      {false, "", {"F2", "\\d", 3, 3}},
  };
  auto v = regex_tool::verify_example(c, "12345");
  CHECK(v.passed);
  CHECK_EQ(v.field_captures.size(), 2u);
  if (v.field_captures.size() == 2u) {
    CHECK_EQ(v.field_captures[0].first, "F1");
    CHECK_EQ(v.field_captures[0].second, "12");
    CHECK_EQ(v.field_captures[1].first, "F2");
    CHECK_EQ(v.field_captures[1].second, "345");
  }
}

void test_literal_hyphen_not_escaped() {
  auto result = regex_tool::generate({"A-1", "A-2"});
  CHECK_EQ(first_regex(result), std::string("^A-\\d$"));
  CHECK(result.candidates.front().passes_all);
}

void test_trim_utility() {
  CHECK_EQ(regex_tool::trim("  hello \r\n\t"), std::string("hello"));
  CHECK_EQ(regex_tool::trim(""), std::string(""));
  CHECK_EQ(regex_tool::trim("abc"), std::string("abc"));
}

}  // namespace

int main() {
  test_real_examples_produce_expected_regex();
  test_field_names_and_rules_follow_tags();
  test_field_captures_match_actual_values();
  test_varying_length_field_emits_range();
  test_letter_charset_inferred();
  test_literal_escaping();
  test_all_identical_positives_emit_literal_regex();
  test_single_field_when_no_constant_positions();
  test_leading_trailing_whitespace_trimmed_with_warning();
  test_empty_input_returns_empty_candidate();
  test_negative_with_different_structure_rejected();
  test_negative_with_invalid_field_content_points_at_field();
  test_negative_that_matches_is_a_leak();
  test_promoted_sq_field_accepts_any_digit_four();
  test_verify_example_pass_and_fail_with_segment();
  test_non_ascii_preserved_in_literal();
  test_empty_negatives_ok();
  test_alnum_marker_segmentation();
  test_single_segment_without_separator_or_marker();
  test_marker_segmentation_consistent_across_examples();
  test_length_based_segmentation();
  test_length_based_short_example_warns();
  test_promote_constant_segment_to_field_sq();
  test_promote_constant_segment_to_field_dt();
  test_field_override_by_name();
  test_length_based_segmentation_with_promote();
  test_multiple_candidates_generated_and_ranked();
  test_variant_regex_texts();
  test_candidates_filtered_when_negative_leaks_under_relaxation();
  test_explicit_separator_matches_automatic();
  test_multiple_separators();
  test_separator_position_mismatch_falls_back_with_warning();
  test_separator_mode_with_promote_sq();
  test_none_pass_fallback_keeps_closest();
  test_invalid_regex_no_crash();
  test_charset_matches_ranges_and_custom_classes();
  test_field_captures_adjacent_fields();
  test_literal_hyphen_not_escaped();
  test_trim_utility();

  if (g_failures == 0) {
    std::printf("ALL TESTS PASSED\n");
    return 0;
  }
  std::printf("%d TEST(S) FAILED\n", g_failures);
  return 1;
}
