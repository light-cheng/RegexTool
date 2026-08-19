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
    std::fflush(stdout);
    ++g_failures;
  }
}

template <typename A, typename B>
void check_ne_impl(const char* file, int line, const char* a_str, const char* b_str, const A& a, const B& b) {
  if (!(a != b)) {
    std::printf("FAIL %s:%d: %s != %s [both are: %s]\n",
                file, line, a_str, b_str, format_val(a).c_str());
    std::fflush(stdout);
    ++g_failures;
  }
}

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
      std::fflush(stdout);                                                \
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
// Smart semantic tokenization automatically recognizes tag SQ + 4-digit payload.
const char* kExpectedRegex =
    "^PN\\d{4}@BN\\d{4}@DT2026@LT\\d{4}@SQ\\d{4}@WK\\d{2}$";
const char* kExpectedExactRegex =
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
    CHECK_EQ(names[3], "SQ");
    CHECK_EQ(names[4], "WK");
    for (const auto& cs : charsets) CHECK_EQ(cs, "\\d");
    CHECK_EQ(lens[0].first, 4u);
    CHECK_EQ(lens[0].second, 4u);
    CHECK_EQ(lens[3].first, 4u);
    CHECK_EQ(lens[3].second, 4u);
    CHECK_EQ(lens[4].first, 2u);
    CHECK_EQ(lens[4].second, 2u);
  }
}

void test_field_captures_match_actual_values() {
  auto result = regex_tool::generate(kRealExamples);
  const auto& vd = result.candidates.front();
  const std::vector<std::vector<std::string>> expected = {
      {"8821", "9012", "3344", "0001", "12"},
      {"4503", "7741", "5566", "0002", "15"},
      {"1192", "3325", "1122", "0003", "08"},
      {"6678", "5589", "9900", "0004", "23"},
      {"3345", "1102", "4488", "0005", "04"},
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
  // No separators, no markers: the tool produces a clean regex with alpha & digit fields.
  auto result = regex_tool::generate({"ABC123", "ABD456"});
  CHECK_EQ(first_regex(result), std::string("^AB[A-Z]\\d{3}$"));
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
  std::vector<std::string> names;
  for (const auto& seg : vd.candidate.segments) {
    if (!seg.is_literal) names.push_back(seg.field.name);
  }
  CHECK_EQ(names.size(), 2u);
  if (names.size() == 2u) {
    CHECK_EQ(names[0], "ABCD");
    CHECK_EQ(names[1], "Field1");
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
  regex_tool::Project p;
  p.positives = kRealExamples;
  p.segment_config.promote_tags = {"SQ"};
  auto result = regex_tool::generate(p);
  CHECK_EQ(first_regex(result),
           std::string("^PN\\d{4}@BN\\d{4}@DT2026@LT\\d{4}@SQ\\d{4}@WK\\d{2}$"));
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
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
           std::string("^PN\\d{4}@BN\\d{4}@DT\\d{4}@LT\\d{4}@SQ\\d{4}@WK\\d{2}$"));
  CHECK(result.candidates.front().passes_all);
}

void test_field_override_by_name() {
  regex_tool::Project p;
  p.positives = kRealExamples;
  regex_tool::FieldRule ov;
  ov.name = "PN";
  ov.charset = "\\d";
  ov.min_len = 2;
  ov.max_len = 6;
  p.segment_config.field_overrides = {ov};
  auto result = regex_tool::generate(p);
  CHECK(result.has_passing);
  bool found = false;
  for (const auto& vd : result.candidates) {
    if (vd.candidate.regex.find("^PN\\d{2,6}@") == 0) {
      found = true;
      CHECK(vd.passes_all);
    }
  }
  CHECK(found);
}

void test_length_based_segmentation_with_promote() {
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
  CHECK(result.candidates.size() >= 3u);
  CHECK_EQ(result.candidates[0].candidate.variant, std::string("smart"));
  for (std::size_t i = 0; i < result.candidates.size(); ++i) {
    CHECK(result.candidates[i].passes_all);
  }
}

void test_variant_regex_texts() {
  auto result = regex_tool::generate({"AB1", "AB2"});
  bool found_smart = false, found_relaxed = false, found_broadened = false;
  for (const auto& vd : result.candidates) {
    if (vd.candidate.regex == "^AB\\d$") found_smart = true;
    if (vd.candidate.regex == "^AB\\d{1,}$") found_relaxed = true;
    if (vd.candidate.regex == "^AB[A-Z0-9]$") found_broadened = true;
  }
  CHECK(found_smart);
  CHECK(found_relaxed);
  CHECK(found_broadened);
}

void test_candidates_filtered_when_negative_leaks_under_relaxation() {
  regex_tool::Project p;
  p.positives = kRealExamples;
  p.negatives = {"PN88211@BN9012@DT2026@LT3344@SQ0001@WK12",
                 "PN8A21@BN9012@DT2026@LT3344@SQ0001@WK12"};
  auto result = regex_tool::generate(p);
  CHECK(result.has_passing);
  // Relaxed & broadened are filtered out due to negative leak; smart/exact pass
  for (const auto& vd : result.candidates) {
    if (vd.candidate.variant == "relaxed" || vd.candidate.variant == "broadened") {
      CHECK(!vd.passes_all);
    }
  }
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

void test_empty_positives_generate_returns_empty() {
  regex_tool::Project p;
  p.segment_config.use_lengths = false;
  p.segment_config.use_separators = false;
  auto result = regex_tool::generate(p);
  CHECK(result.candidates.empty());
  CHECK(!result.has_passing);
}

void test_user_reported_barcode_scenario() {
  regex_tool::Project p;
  p.positives = {
    "PN-A10402@L260818A@2634@Q1000@LN01@00891",
    "PN-A10402@L260818A@2634@Q1000@LN01@00892",
    "PN-A10402@L260818B@2634@Q1000@LN02@00104",
    "PN-A10402@L260818B@2634@Q0500@LN02@00105",
    "PN-A10402@L260819A@2634@Q1000@LN03@00012"
  };
  p.negatives = {
    "PN-A10402@L260818A@2634@Q1000@00891",
    "PN-A10402#L260818A@2634@Q1000@LN01@00891",
    "PN-A10402@L260818A@2634@Q1000@LN01@00891@",
    "PN-A10402@L260818A@2634@ONE-THOUSAND@LN01@00891",
    "PN-A10402@2634@L260818A@Q1000@LN01@00891"
  };

  auto res = regex_tool::generate(p);
  std::printf("Generated candidates count: %zu\n", res.candidates.size());
  for (size_t i = 0; i < res.candidates.size(); ++i) {
    std::printf("  [%zu] %s: %s\n", i, res.candidates[i].candidate.variant.c_str(),
                res.candidates[i].candidate.regex.c_str());
  }

  std::string test_barcode = "PN-A10402@L260819C@2634@Q2000@LN01@00001";
  CHECK(!res.candidates.empty());
  if (!res.candidates.empty()) {
    auto v = regex_tool::verify_example(res.candidates[0].candidate, test_barcode);
    CHECK(v.passed);
  }
}

void test_detect_separators_alnum_and_punct() {
  // Story 8 & 9: Auto-detect punctuation and alphanumeric markers
  auto seps1 = regex_tool::detect_separators({"PN8821@BN9012@DT2026", "PN8822@BN9012@DT2026"});
  CHECK(!seps1.empty());
  CHECK(std::find(seps1.begin(), seps1.end(), "@") != seps1.end());

  auto seps2 = regex_tool::detect_separators({"1P123456NQ7890123N3456", "1P998877NQ1122333N0012"});
  CHECK(seps2.size() >= 2u);
  bool has_1p = false, has_nq = false, has_3n = false;
  for (const auto& s : seps2) {
    if (s == "1P") has_1p = true;
    if (s == "NQ") has_nq = true;
    if (s == "3N") has_3n = true;
  }
  CHECK(has_1p || has_nq || has_3n);
}

void test_promote_pure_digit_constant_segment() {
  // Story 15: Promote constant pure digit segment (e.g. 2634 or 2026) to a field
  regex_tool::Project p;
  p.positives = {"PN-A10402@2634@Q1000", "PN-A10402@2634@Q2000"};
  p.segment_config.promote_tags = {"2634"};
  auto result = regex_tool::generate(p);
  CHECK(result.has_passing);
  CHECK_EQ(first_regex(result), std::string("^PN-A10402@\\d{4}@Q\\d{4}$"));
  CHECK(result.candidates.front().passes_all);
}

void test_field_override_literal_to_field() {
  // Story 15: In-place manual override of constant segment to field
  regex_tool::Project p;
  p.positives = {"PN-A10402@2634@Q1000", "PN-A10402@2634@Q2000"};
  regex_tool::FieldRule ov;
  ov.name = "2634";
  ov.charset = "\\d";
  ov.min_len = 4;
  ov.max_len = 4;
  p.segment_config.field_overrides = {ov};
  auto result = regex_tool::generate(p);
  CHECK(result.candidates.front().passes_all);
}

void test_user_separator_trailing_delimiter_scenario() {
  regex_tool::Project p;
  p.positives = {
      "PN-A10402@L260818A@2634@Q1000@LN01@00891",
      "PN-A10402@L260818A@2634@Q1000@LN01@00892",
      "PN-A10402@L260818B@2634@Q1000@LN02@00104",
      "PN-A10402@L260819C@2634@Q2000@LN01@00001@",
  };
  p.segment_config.use_separators = true;
  p.segment_config.separators = {"@"};
  auto result = regex_tool::generate(p);
  CHECK(result.has_passing);
  CHECK_EQ(first_regex(result), std::string("^PN-A10402@L\\d{6}[A-Z]@2634@Q\\d{4}@LN\\d{2}@\\d{5}$"));

  // Verify self-test against the generated candidate
  std::string selftest_input = "PN-A10402@L260819C@2634@Q2000@LN01@00001";
  auto v = regex_tool::verify_example(result.candidates.front().candidate, selftest_input);
  CHECK(v.passed);
  CHECK(v.matched);
  CHECK_EQ(v.failing_segment, SIZE_MAX);
}

void test_custom_sequential_markers_european() {
  regex_tool::Project p;
  p.positives = {
      "1P123456NQ7890123N3456",
      "1P887766NQ5544333N1122",
  };
  p.segment_config.use_separators = true;
  p.segment_config.separators = {"1P", "NQ", "3N"};
  auto result = regex_tool::generate(p);
  CHECK(result.has_passing);
  CHECK_EQ(first_regex(result), std::string("^1P\\d{6}NQ\\d{6}3N\\d{4}$"));
}

void test_asc_mh10_data_identifier_concatenation() {
  // ASC MH10 DI prefixes: 1P (PN), Q (Quantity), 1T (Lot), S (Serial)
  auto result = regex_tool::generate({
      "1P12345678Q1001TA1B2C3D4S98765",
      "1P87654321Q5001T12345678S12345",
      "1P11223344Q0501TABCDEFGHS67890",
  });
  CHECK(result.has_passing);
  CHECK_EQ(first_regex(result), std::string("^1P\\d{8}Q\\d{3}1T[A-Z0-9]{8}S\\d{5}$"));
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
  CHECK_EQ(vd.positive_verdicts.size(), 3u);
  // Check that field names match the DI tags
  std::vector<std::string> field_names;
  for (const auto& seg : vd.candidate.segments) {
    if (!seg.is_literal) field_names.push_back(seg.field.name);
  }
  CHECK_EQ(field_names.size(), 4u);
  CHECK_EQ(field_names[0], std::string("1P"));
  CHECK_EQ(field_names[1], std::string("Q"));
  CHECK_EQ(field_names[2], std::string("1T"));
  CHECK_EQ(field_names[3], std::string("S"));
}

void test_iso_15434_format_06_with_gs_separator() {
  // ISO/IEC 15434 Format 06 envelope with Group Separator (GS = \x1D)
  std::string p1 = std::string("[)>06\x1d") + "1P12345678" + "\x1d" + "Q100" + "\x1d" + "1TLOT2024A" + "\x1d" + "S98765";
  std::string p2 = std::string("[)>06\x1d") + "1P87654321" + "\x1d" + "Q500" + "\x1d" + "1TLOT2024B" + "\x1d" + "S12345";
  auto result = regex_tool::generate({p1, p2});
  CHECK(result.has_passing);
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);

  // Also verify explicit separator resolution with "GS" or "\x1d"
  regex_tool::Project proj;
  proj.positives = {p1, p2};
  proj.segment_config.use_separators = true;
  proj.segment_config.separators = {"GS"};
  auto res_gs = regex_tool::generate(proj);
  CHECK(res_gs.has_passing);
  CHECK(res_gs.candidates.front().passes_all);
}

void test_user_exact_barcode_scenario() {
  std::vector<std::string> positives = {
      "[)>[RS]06[GS]1PMX-90021-B[GS]3SBX26081802[GS]1TLOT2026A[GS]Q1500[GS]D260818[GS]V12883[RS][EOT]",
      "[)>[RS]06[GS]1PMX-88103-A[GS]3SBX26081803[GS]1TLOT2026B[GS]Q2000[GS]D260819[GS]V99412[RS][EOT]",
  };

  // 1. Fully automatic generation
  auto auto_res = regex_tool::generate(positives);
  CHECK(auto_res.has_passing);
  const auto& auto_vd = auto_res.candidates.front();
  CHECK(auto_vd.passes_all);

  // 2. Explicitly setting DI markers: 1P, 3S, 1T, Q, D, V
  regex_tool::Project proj_di;
  proj_di.positives = positives;
  proj_di.segment_config.use_separators = true;
  proj_di.segment_config.separators = {"1P", "3S", "1T", "Q", "D", "V"};
  auto res_di = regex_tool::generate(proj_di);
  CHECK(res_di.has_passing);
  CHECK(res_di.candidates.front().passes_all);

  // 3. Explicitly setting [GS] separator
  regex_tool::Project proj_gs;
  proj_gs.positives = positives;
  proj_gs.segment_config.use_separators = true;
  proj_gs.segment_config.separators = {"[GS]"};
  auto res_gs = regex_tool::generate(proj_gs);
  CHECK(res_gs.has_passing);
  CHECK(res_gs.candidates.front().passes_all);
}

void test_user_di_negative_examples_scenario() {
  regex_tool::Project proj;
  proj.positives = {
      "1PMX-90021-B@3SBX26081801@1TLOT2026A@Q3000@D260818@V12883",
      "1PMX-90021-B@3SBX26081802@1TLOT2026A@Q1500@D260818@V12883",
      "1PMX-90021-B@3SBX26081901@1TLOT2026B@Q0500@D260819@V12883",
  };
  proj.negatives = {
      // 1. Extra colon/space
      "1P:MX-90021-B@3S:BX26081801@1T:LOT2026A@Q:3000@D:260818@V:12883",
      // 2. Missing DI prefix
      "MX-90021-B@BX26081801@LOT2026A@3000@260818@12883",
      // 3. Prefix typo/overlap
      "PMX-90021-B@SBX26081801@TLOT2026A@Q3000@D260818@V12883",
      // 4. ISO 15434 header residual
      "[)>06@1PMX-90021-B@3SBX26081801@1TLOT2026A@Q3000@D260818@V12883",
  };

  auto result = regex_tool::generate(proj);
  CHECK(result.has_passing);
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
  CHECK_EQ(vd.positive_verdicts.size(), 3u);
  CHECK_EQ(vd.negative_verdicts.size(), 4u);
  for (const auto& pv : vd.positive_verdicts) {
    CHECK(pv.passed);
  }
  for (const auto& nv : vd.negative_verdicts) {
    CHECK(nv.passed);    // Passed negative verdict means regex correctly REJECTED it
    CHECK(!nv.matched);  // Did not match
  }
}

void test_pure_digits_variable_length_anti_overfitting() {
  auto result = regex_tool::generate({"1001", "10002", "100003", "1000004"});
  CHECK(result.has_passing);
  CHECK_EQ(first_regex(result), std::string("^\\d{4,7}$"));
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
  for (const auto& v : vd.positive_verdicts) {
    CHECK(v.passed);
  }

  // Check relaxed candidate exists and has {4,} or +
  bool found_relaxed = false;
  for (const auto& cv : result.candidates) {
    if (cv.candidate.variant == "relaxed" || cv.candidate.regex == "^\\d{4,}$" || cv.candidate.regex == "^\\d+$") {
      found_relaxed = true;
      break;
    }
  }
  CHECK(found_relaxed);
}

void test_tag_with_variable_length_digits() {
  auto result = regex_tool::generate({"SN1001", "SN10002", "SN100003"});
  CHECK(result.has_passing);
  CHECK_EQ(first_regex(result), std::string("^SN\\d{4,6}$"));
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
  CHECK_EQ(vd.candidate.segments.size(), 2u);
  if (vd.candidate.segments.size() >= 2u) {
    CHECK(vd.candidate.segments[0].is_literal);
    CHECK_EQ(vd.candidate.segments[0].literal, "SN");
    CHECK(!vd.candidate.segments[1].is_literal);
    CHECK_EQ(vd.candidate.segments[1].field.name, "SN");
    CHECK_EQ(vd.candidate.segments[1].field.min_len, 4u);
    CHECK_EQ(vd.candidate.segments[1].field.max_len, 6u);
  }
  CHECK_EQ(vd.positive_verdicts.size(), 3u);
  if (vd.positive_verdicts.size() == 3u) {
    CHECK_EQ(vd.positive_verdicts[0].field_captures[0].second, "1001");
    CHECK_EQ(vd.positive_verdicts[1].field_captures[0].second, "10002");
    CHECK_EQ(vd.positive_verdicts[2].field_captures[0].second, "100003");
  }
}

void test_middle_coincidental_char_anti_overfitting() {
  auto result = regex_tool::generate({"201", "3002", "40003"});
  CHECK(result.has_passing);
  CHECK_EQ(first_regex(result), std::string("^\\d{3,5}$"));
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
}

void test_variable_length_with_delimiters() {
  auto result = regex_tool::generate({
      "PN8821@SQ1001@WK12",
      "PN4503@SQ10002@WK15",
      "PN1192@SQ100003@WK08",
  });
  CHECK(result.has_passing);
  CHECK_EQ(first_regex(result), std::string("^PN\\d{4}@SQ\\d{4,6}@WK\\d{2}$"));
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
  CHECK_EQ(vd.positive_verdicts.size(), 3u);
  if (vd.positive_verdicts.size() == 3u) {
    CHECK_EQ(vd.positive_verdicts[0].field_captures[1].second, "1001");
    CHECK_EQ(vd.positive_verdicts[1].field_captures[1].second, "10002");
    CHECK_EQ(vd.positive_verdicts[2].field_captures[1].second, "100003");
  }
}

void test_real_iso_15434_raw_control_bytes_and_negatives() {
  regex_tool::Project proj;
  proj.positives = {
      std::string("[)>\x1E" "06\x1D") + "1PA10402" + "\x1D" + "Q1000" + "\x1D" + "S00891\x1E\x04",
      std::string("[)>\x1E" "06\x1D") + "1PB20500" + "\x1D" + "Q2500" + "\x1D" + "S00992\x1E\x04",
  };
  // Negative missing the \x1D between 1P and Q
  proj.negatives = {
      std::string("[)>\x1E" "06\x1D") + "1PA10402" + "Q1000" + "\x1D" + "S00891\x1E\x04",
  };

  auto result = regex_tool::generate(proj);
  CHECK(result.has_passing);
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);
  CHECK(first_regex(result).find("\x1D") == std::string::npos); // Regex must escape control characters
  CHECK(first_regex(result).find("\\x1D") != std::string::npos || first_regex(result).find("\\x1d") != std::string::npos);
  CHECK(first_regex(result).find("\\x1E") != std::string::npos || first_regex(result).find("\\x1e") != std::string::npos);
  CHECK(first_regex(result).find("\\x04") != std::string::npos);

  CHECK_EQ(vd.negative_verdicts.size(), 1u);
  if (!vd.negative_verdicts.empty()) {
    CHECK(vd.negative_verdicts[0].passed);
    CHECK(!vd.negative_verdicts[0].matched);
    CHECK_NE(vd.negative_verdicts[0].failing_segment, SIZE_MAX);
  }
}

void test_visualized_escape_text_normalization() {
  regex_tool::Project proj;
  proj.positives = {
      "[)>06<GS>1PA10402<GS>Q1000<GS>S00891<RS><EOT>",
      "[)>06<GS>1PB20500<GS>Q2500<GS>S00992<RS><EOT>",
  };

  auto result = regex_tool::generate(proj);
  CHECK(result.has_passing);
  const auto& vd = result.candidates.front();
  CHECK(vd.passes_all);

  // Self-test with raw binary string against candidate generated from text escapes
  std::string raw_test = std::string("[)>06\x1d") + "1PA10402" + "\x1d" + "Q1000" + "\x1d" + "S00891" + "\x1e\x04";
  auto v1 = regex_tool::verify_example(vd.candidate, raw_test);
  CHECK(v1.passed);
  CHECK(v1.matched);

  // Self-test with visualized string
  auto v2 = regex_tool::verify_example(vd.candidate, "[)>06<GS>1PA10402<GS>Q1000<GS>S00891<RS><EOT>");
  CHECK(v2.passed);
  CHECK(v2.matched);

  // Check utility functions
  CHECK_EQ(regex_tool::normalize_control_chars("<GS><RS><EOT><US>"), std::string("\x1D\x1E\x04\x1F"));
  CHECK_EQ(regex_tool::visualize_control_chars("\x1D\x1E\x04\x1F"), std::string("[GS][RS][EOT][US]"));
}

void test_aim_id_detection_and_dual_variants() {
  // Data Matrix ECC 200 with AIM ID ]d1
  regex_tool::Project proj_dm;
  proj_dm.positives = {
      "]d11PA10402@Q1000@S00891",
      "]d11PB20500@Q2500@S00992",
  };
  proj_dm.negatives = {
      "]d11PA10402@Q-BAD@S00891",
  };

  auto res_dm = regex_tool::generate(proj_dm);
  CHECK(res_dm.has_passing);
  CHECK(res_dm.candidates.size() >= 2u);

  bool found_stripped = false, found_full = false;
  for (const auto& cv : res_dm.candidates) {
    if (cv.candidate.variant == "aim_stripped") {
      found_stripped = true;
      CHECK(cv.passes_all);
      CHECK(cv.candidate.regex.rfind("1P", 0) != std::string::npos || cv.candidate.regex.rfind("^1P", 0) != std::string::npos);
    }
    if (cv.candidate.variant == "aim_full") {
      found_full = true;
      CHECK(cv.passes_all);
      CHECK(cv.candidate.regex.rfind("^\\]d1", 0) != std::string::npos);
    }
  }
  CHECK(found_stripped);
  CHECK(found_full);

  // GS1-128 with AIM ID ]C1
  auto res_c1 = regex_tool::generate({"]C10101234567890128", "]C10109876543210987"});
  CHECK(res_c1.has_passing);
  CHECK(first_regex(res_c1) == "^\\d{16}$" || first_regex(res_c1) == "^\\]C1\\d{16}$" || first_regex(res_c1) == "^01\\d{14}$" || first_regex(res_c1) == "^\\]C101\\d{14}$");

  // QR Code with AIM ID ]Q1
  auto res_q1 = regex_tool::generate({"]Q1URL123", "]Q1URL456"});
  CHECK(res_q1.has_passing);
}

void test_industrial_di_vda_and_aerospace() {
  // Automotive VDA 4994: 1J (PackageID), 3J (InnerPackageID), 5J (MixedPackageID)
  auto res_vda = regex_tool::generate({
      "1JMAT10402@3JIN2001@5JMX901",
      "1JMAT20500@3JIN5002@5JMX902",
  });
  CHECK(res_vda.has_passing);
  CHECK_EQ(first_regex(res_vda), std::string("^1JMAT\\d{5}@3JIN\\d{4}@5JMX\\d{3}$"));

  // Aerospace ATA 2000 / MIL-STD-130: 17V (Manufacturer), 1S (Serial), 30P (PartRev), 30Q (Qty)
  auto res_ata = regex_tool::generate({
      "17VCAGE01@1SSN9001@30PREVA@30QQTY10",
      "17VCAGE02@1SSN9002@30PREVB@30QQTY20",
  });
  CHECK(res_ata.has_passing);
  CHECK_EQ(first_regex(res_ata), std::string("^17VCAGE\\d{2}@1SSN\\d{4}@30PREV[A-Z]@30QQTY\\d{2}$"));
}

void test_gs1_application_identifiers_parenthesized() {
  // GS1 AI standard with human-readable parentheses: (01) GTIN, (10) Batch, (17) ExpDate, (21) Serial
  auto res_gs1 = regex_tool::generate({
      "(01)01234567890128(10)LOT2026A(17)261231(21)SN1001",
      "(01)01234567890128(10)LOT2026B(17)261231(21)SN1002",
  });
  CHECK(res_gs1.has_passing);
  CHECK_EQ(first_regex(res_gs1), std::string("^\\(01\\)01234567890128\\(10\\)LOT2026[A-Z]\\(17\\)261231\\(21\\)SN\\d{4}$"));
}

void test_business_field_description_dictionary() {
  CHECK_EQ(regex_tool::get_business_field_description("1P"), std::string("PartNumber"));
  CHECK_EQ(regex_tool::get_business_field_description("Q"), std::string("Quantity"));
  CHECK_EQ(regex_tool::get_business_field_description("S"), std::string("SerialNumber"));
  CHECK_EQ(regex_tool::get_business_field_description("1T"), std::string("TraceabilityLot"));
  CHECK_EQ(regex_tool::get_business_field_description("17V"), std::string("ManufacturerCode"));
  CHECK_EQ(regex_tool::get_business_field_description("1J"), std::string("PackageID"));
  CHECK_EQ(regex_tool::get_business_field_description("01"), std::string("GTIN"));
  CHECK_EQ(regex_tool::get_business_field_description("(01)"), std::string("GTIN"));
}

void test_ean13_symbology_recognition_and_checksum_advisory() {
  regex_tool::Project proj;
  proj.positives = {"6901234567895", "6909876543212"};
  proj.negatives = {"6901234567890"}; // Same length and digit format, wrong check digit

  auto result = regex_tool::generate(proj);
  CHECK_EQ(first_regex(result), std::string("^\\d{13}$"));

  bool found_ean_warn = false, found_checksum_diag = false;
  for (const auto& w : result.warnings) {
    if (w.find("EAN-13") != std::string::npos && w.find("Modulo 10") != std::string::npos) {
      found_ean_warn = true;
    }
    if (w.find("无法仅凭字符格式识别校验位数值错误") != std::string::npos) {
      found_checksum_diag = true;
    }
  }
  CHECK(found_ean_warn);
  CHECK(found_checksum_diag);
}

void test_upca_symbology_recognition_and_advisory() {
  auto result = regex_tool::generate({"012345678905", "098765432108"});
  CHECK_EQ(first_regex(result), std::string("^\\d{12}$"));
  bool found_upc = false;
  for (const auto& w : result.warnings) {
    if (w.find("UPC-A") != std::string::npos) found_upc = true;
  }
  CHECK(found_upc);
}

void test_itf14_symbology_recognition() {
  auto result = regex_tool::generate({"10012345678902", "20012345678909"});
  CHECK_EQ(first_regex(result), std::string("^\\d{14}$"));
  bool found_itf = false;
  for (const auto& w : result.warnings) {
    if (w.find("ITF-14") != std::string::npos) found_itf = true;
  }
  CHECK(found_itf);
}

}  // namespace

int main() {
  test_ean13_symbology_recognition_and_checksum_advisory();
  test_upca_symbology_recognition_and_advisory();
  test_itf14_symbology_recognition();
  test_business_field_description_dictionary();
  test_industrial_di_vda_and_aerospace();
  test_gs1_application_identifiers_parenthesized();
  test_aim_id_detection_and_dual_variants();
  test_real_iso_15434_raw_control_bytes_and_negatives();
  test_visualized_escape_text_normalization();
  test_pure_digits_variable_length_anti_overfitting();
  test_tag_with_variable_length_digits();
  test_middle_coincidental_char_anti_overfitting();
  test_variable_length_with_delimiters();
  test_user_di_negative_examples_scenario();
  test_user_exact_barcode_scenario();
  test_asc_mh10_data_identifier_concatenation();
  test_iso_15434_format_06_with_gs_separator();
  test_user_separator_trailing_delimiter_scenario();
  test_custom_sequential_markers_european();
  test_user_reported_barcode_scenario();
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
  test_separator_mode_with_promote_sq();
  test_none_pass_fallback_keeps_closest();
  test_invalid_regex_no_crash();
  test_charset_matches_ranges_and_custom_classes();
  test_field_captures_adjacent_fields();
  test_detect_separators_alnum_and_punct();
  test_promote_pure_digit_constant_segment();
  test_field_override_literal_to_field();
  test_literal_hyphen_not_escaped();
  test_trim_utility();
  test_empty_positives_generate_returns_empty();

  if (g_failures == 0) {
    std::printf("ALL TESTS PASSED\n");
    return 0;
  }
  std::printf("%d TEST(S) FAILED\n", g_failures);
  return 1;
}
