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
  std::vector<SegmentRule> segments; // in order, concatenation equals the regex
};

// Input project: positives (must match the format) and negatives (must not).
struct Project {
  std::vector<std::string> positives;
  std::vector<std::string> negatives;
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
  std::vector<CandidateVerdict> candidates;
  std::vector<std::string> warnings;  // input-hygiene warnings (工单 04)
};

// Generate candidates from the project and validate each against every example.
// Leading/trailing whitespace of each example is trimmed; hygiene problems are
// reported as warnings, never silently folded into the rules (工单 04).
GenerationResult generate(const Project& project);

// Convenience overload: positives only.
GenerationResult generate(const std::vector<std::string>& positives);

// Self-test (工单 07): judge one arbitrary barcode against a candidate using the
// same verification logic as the project examples.
ExampleVerdict verify_example(const Candidate& candidate, const std::string& example);

}  // namespace regex_tool
