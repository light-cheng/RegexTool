#pragma once

// Core engine for the Regex Tool (工单 01 范围).
// Pure logic, no UI dependency: positives in -> one candidate regex + per-example
// validation verdicts out. Tests live at this seam only.

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

// Validation verdict for one example against a candidate.
struct ExampleVerdict {
  std::string example;
  bool passed = false;
  // Index into Candidate::segments where the example first fails;
  // SIZE_MAX when it passed. Diagnostic only (see field_captures for the real check).
  std::size_t failing_segment = SIZE_MAX;
  // Captured value per field, in field order, for this example.
  std::vector<std::pair<std::string, std::string>> field_captures;
};

struct GenerationResult {
  Candidate candidate;
  std::vector<ExampleVerdict> positive_verdicts;  // one per positive, same order
};

// Generate one candidate from the given positives and validate it against them.
// Leading/trailing whitespace of each positive is trimmed (explicit warnings are
// a later ticket). Returns an empty candidate for empty input.
GenerationResult generate(const std::vector<std::string>& positives);

}  // namespace regex_tool
