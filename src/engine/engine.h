#pragma once

// Core engine for the Regex Tool.
// Pure logic, no UI dependency: a project (positives + negatives) in -> candidate
// regexes + per-example validation verdicts + warnings out. Tests live at this
// seam only.

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace regex_tool {

// Rule for one variable segment (字段).
struct FieldRule {
  std::string name;      // 字段名, e.g. "PN"; falls back to "Field1" when no tag
  std::string charset;   // emitted regex fragment, e.g. "\\d" or "[A-Z0-9]"
  std::size_t min_len = 0;
  std::size_t max_len = 0;
};

// Rule for one segment (分段): either fixed text or a field.
struct SegmentRule {
  bool is_literal = true;
  std::string literal;   // when is_literal: fixed text, kept verbatim (escaped in regex)
  FieldRule field;       // when !is_literal
};

// One candidate regex plus the segment model it was composed from.
struct Candidate {
  std::string regex;                 // anchored, e.g. "^PN\\d{4}@BN\\d{4}@...$"
  std::string capturing_regex;       // anchored capturing regex, e.g. "^(PN\\d{4})@...$"
  std::vector<SegmentRule> segments; // in order, concatenation equals the regex
  std::string variant;               // "smart" | "relaxed" | "broadened" | "exact"
  std::string display_name;          // Human-readable title
};

// Segmentation configuration. Defaults = fully automatic.
struct SegmentConfig {
  bool use_lengths = false;                 // manual length-based segmentation
  std::vector<std::size_t> lengths;         // chunk sizes when use_lengths
  bool use_separators = false;              // explicit separator(s) segmentation
  std::vector<std::string> separators;      // one or more separators, e.g. {"@"} or {"@", "|"}
  std::vector<std::string> promote_tags;    // literals whose tag is listed become fields
  std::vector<FieldRule> field_overrides;   // override inferred field rules, keyed by name
};

// Input project: positives (must match the format) and negatives (must not).
struct Project {
  std::vector<std::string> positives;
  std::vector<std::string> negatives;
  SegmentConfig segment_config;
};

// Validation verdict for one example against one candidate.
struct ExampleVerdict {
  std::string example;
  bool passed = false;   // positive: regex matched; negative: regex did NOT match
  bool matched = false;  // raw regex-match outcome (diagnostic)
  // Index into Candidate::segments where the walk first failed (positive: where it
  // stopped; negative: where it was rejected); SIZE_MAX when the example matched
  // the whole regex (positive pass / negative leak).
  std::size_t failing_segment = SIZE_MAX;
  // Captured value per field, in field order, for this example.
  std::vector<std::pair<std::string, std::string>> field_captures;
};

// Verdicts of one candidate against the whole project.
struct CandidateVerdict {
  Candidate candidate;
  std::vector<ExampleVerdict> positive_verdicts;  // one per positive, same order
  std::vector<ExampleVerdict> negative_verdicts;  // one per negative, same order
  bool passes_all = false;  // every positive passes and every negative is rejected
  std::size_t failures = 0; // positives failing + negatives leaking
};

struct GenerationResult {
  std::vector<CandidateVerdict> candidates;  // passing first (recommended first); when
                                             // none pass, the closest failing ones
  std::vector<std::string> warnings;         // input-hygiene warnings
  bool has_passing = false;                  // whether any candidate passes all examples
};

// Generate candidates from the project and validate each against every example.
GenerationResult generate(const Project& project);

// Convenience overload: positives only.
GenerationResult generate(const std::vector<std::string>& positives);

// Self-test: judge one arbitrary barcode against a candidate using the
// same verification logic as the project examples.
ExampleVerdict verify_example(const Candidate& candidate, const std::string& example);

// Tags inside a literal that can be promoted to fields: letter runs
// immediately followed by digits, e.g. "@DT2026@LT" -> {"DT"}.
std::vector<std::string> promotable_tags(const std::string& literal);

// Compose anchored non-capturing regex.
std::string compose_regex(const std::vector<SegmentRule>& segments);

// Compose anchored capturing regex.
std::string compose_capturing_regex(const std::vector<SegmentRule>& segments);

// Utility: trim leading and trailing whitespace (\r, \n, \t, space).
std::string trim(const std::string& s);

// Utility: test whether a character matches a charset fragment (e.g. \d, [A-Z], [0-9A-F]).
bool charset_matches(const std::string& frag, char c);

}  // namespace regex_tool
