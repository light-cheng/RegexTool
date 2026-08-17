// Win32 UI for the Regex Tool.
// Panes: left (scrollable) = project bar, positive/negative example columns,
// segment configuration (mode, lengths, rule table with promote); right (pinned)
// = candidate results with per-example verdicts + self-test box.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "engine/engine.h"

namespace {

constexpr int IDC_PROJECT_EDIT = 1000;
constexpr int IDC_NEW = 1001;
constexpr int IDC_SAVE = 1002;
constexpr int IDC_LOAD = 1003;
constexpr int IDC_ADD_POS = 1004;
constexpr int IDC_ADD_NEG = 1005;
constexpr int IDC_BATCH_POS = 1006;
constexpr int IDC_BATCH_NEG = 1007;
constexpr int IDC_MODE_AUTO = 1008;
constexpr int IDC_MODE_LEN = 1009;
constexpr int IDC_LENGTHS_EDIT = 1010;
constexpr int IDC_GENERATE = 1011;
constexpr int IDC_SELFTEST_EDIT = 1012;
constexpr int IDC_SELFTEST_BTN = 1013;
constexpr int IDC_RESULT_EDIT = 1014;
constexpr int IDC_SEG_COUNT_COMBO = 1015;
constexpr int IDC_SEP_EDIT = 1016;
constexpr int IDC_MODE_SEP = 1017;
constexpr int IDC_POS_EDIT_BASE = 2000;
constexpr int IDC_POS_DEL_BASE = 3000;
constexpr int IDC_NEG_EDIT_BASE = 4000;
constexpr int IDC_NEG_DEL_BASE = 5000;
constexpr int IDC_SEG_EDIT_BASE = 6000;
constexpr int IDC_SEG_PROMOTE_BASE = 7000;
constexpr int IDC_LEN_BASE = 8000;

constexpr int LEFT = 8;
constexpr int EDIT_W = 330;
constexpr int DEL_W = 28;
constexpr int ROW_H = 28;
constexpr int CTRL_H = 22;
constexpr int RIGHT_X = 486;
constexpr std::size_t LEN_PER_LINE = 5;  // per-segment length boxes per row

HWND g_main_hwnd = nullptr;

struct AppState {
  HWND project_combo = nullptr;
  HWND new_btn = nullptr, save_btn = nullptr, load_btn = nullptr;
  HWND pos_label = nullptr, neg_label = nullptr, cfg_label = nullptr, sep_label = nullptr, count_label = nullptr;
  std::vector<HWND> pos_edits, pos_dels;
  std::vector<HWND> neg_edits, neg_dels;
  HWND add_pos = nullptr, add_neg = nullptr, batch_pos = nullptr, batch_neg = nullptr;
  HWND mode_auto = nullptr, mode_sep = nullptr, mode_len = nullptr;
  HWND sep_edit = nullptr;
  HWND seg_count_combo = nullptr;
  std::vector<HWND> length_boxes;
  HWND generate_btn = nullptr;
  HWND result_edit = nullptr;
  HWND selftest_edit = nullptr, selftest_btn = nullptr, selftest_label = nullptr;
  std::vector<HWND> seg_edits, seg_promote_btns;
  HFONT font = nullptr;
  int scroll_pos = 0;

  // Engine-side state that persists across regenerations.
  std::vector<regex_tool::SegmentRule> last_segments;
  std::vector<std::string> promote_tags;
  std::vector<regex_tool::FieldRule> field_overrides;
  regex_tool::Candidate selftest_candidate;
  bool has_selftest_candidate = false;
};

AppState g;

std::wstring utf8_to_wide(const std::string& s) {
  if (s.empty()) return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
  return w;
}

std::string wide_to_utf8(const std::wstring& w) {
  if (w.empty()) return "";
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
  std::string s(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
  return s;
}

std::wstring get_edit_text(HWND edit) {
  int len = GetWindowTextLengthW(edit);
  if (len <= 0) return L"";
  std::wstring w(static_cast<size_t>(len), L'\0');
  GetWindowTextW(edit, w.data(), len + 1);
  return w;
}

std::string trim(const std::string& s) {
  std::size_t b = 0, e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
  return s.substr(b, e - b);
}

HWND make_button(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h) {
  HWND btn = CreateWindowExW(0, L"BUTTON", text,
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                             x, y, w, h, parent,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
  SendMessageW(btn, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
  return btn;
}

HWND make_static(HWND parent, const wchar_t* text, int x, int y, int w, int h) {
  HWND st = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
                            x, y, w, h, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
  SendMessageW(st, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
  return st;
}

HWND make_edit(HWND parent, int id, int x, int y, int w, int h, DWORD extra_style) {
  HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | extra_style,
                              x, y, w, h, parent,
                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                              GetModuleHandleW(nullptr), nullptr);
  SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
  return edit;
}

HWND make_combo(HWND parent, int id, int x, int y, int w, int h, bool editable) {
  HWND cb = CreateWindowExW(0, L"COMBOBOX", L"",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                (editable ? CBS_DROPDOWN : CBS_DROPDOWNLIST) | WS_VSCROLL,
                            x, y, w, h, parent,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                            GetModuleHandleW(nullptr), nullptr);
  SendMessageW(cb, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
  return cb;
}

HWND make_radio(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h, bool first) {
  HWND btn = CreateWindowExW(0, L"BUTTON", text,
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON |
                                 (first ? WS_GROUP : 0),
                             x, y, w, h, parent,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
  SendMessageW(btn, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
  return btn;
}

// Layout positions every control; also refreshes the vertical scroll range.
void layout(HWND hwnd);

// Next free control id in the given base range: ids are never reused, so a
// deleted row's ids are not handed out again (deletion locates rows by handle).
int next_free_id(int base) {
  int best = base;
  auto scan = [&](const std::vector<HWND>& hs) {
    for (HWND h : hs) {
      int id = static_cast<int>(GetDlgCtrlID(h));
      if (id >= base && id < base + 1000 && id >= best) best = id + 1;
    }
  };
  scan(g.pos_edits);
  scan(g.neg_edits);
  return best;
}

void add_example_row(HWND hwnd, bool negative) {
  auto& edits = negative ? g.neg_edits : g.pos_edits;
  auto& dels = negative ? g.neg_dels : g.pos_dels;
  int edit_id = next_free_id(negative ? IDC_NEG_EDIT_BASE : IDC_POS_EDIT_BASE);
  int del_id = next_free_id(negative ? IDC_NEG_DEL_BASE : IDC_POS_DEL_BASE);
  HWND edit = make_edit(hwnd, edit_id, LEFT, 0, EDIT_W, CTRL_H, 0);
  HWND del = make_button(hwnd, L"\u00d7", del_id, 0, 0, DEL_W, CTRL_H);  // ×
  edits.push_back(edit);
  dels.push_back(del);
  layout(hwnd);
  SetFocus(edit);
}

void remove_example_row(HWND hwnd, bool negative, HWND del_button) {
  auto& edits = negative ? g.neg_edits : g.pos_edits;
  auto& dels = negative ? g.neg_dels : g.pos_dels;
  auto it = std::find(dels.begin(), dels.end(), del_button);
  if (it == dels.end()) return;
  size_t index = static_cast<size_t>(it - dels.begin());
  DestroyWindow(edits[index]);
  DestroyWindow(dels[index]);
  edits.erase(edits.begin() + static_cast<ptrdiff_t>(index));
  dels.erase(dels.begin() + static_cast<ptrdiff_t>(index));
  layout(hwnd);
}

void clear_example_rows(bool negative) {
  auto& edits = negative ? g.neg_edits : g.pos_edits;
  auto& dels = negative ? g.neg_dels : g.pos_dels;
  for (HWND h : edits) DestroyWindow(h);
  for (HWND h : dels) DestroyWindow(h);
  edits.clear();
  dels.clear();
}

std::vector<std::string> collect_edits(const std::vector<HWND>& edits) {
  std::vector<std::string> out;
  for (HWND edit : edits) {
    std::string s = wide_to_utf8(get_edit_text(edit));
    if (!s.empty()) out.push_back(s);
  }
  return out;
}

// ---- batch paste (工单 04) ----

struct BatchCtx {
  std::wstring text;
};

LRESULT CALLBACK batch_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  constexpr int BATCH_EDIT = 1, BATCH_OK = 2, BATCH_CANCEL = 3;
  switch (msg) {
    case WM_CREATE: {
      SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(
                             reinterpret_cast<LPCREATESTRUCTW>(lParam)->lpCreateParams));
      RECT rc;
      GetClientRect(hwnd, &rc);
      HWND edit = make_edit(hwnd, BATCH_EDIT, 10, 10, rc.right - 20, rc.bottom - 64,
                            ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL);
      make_button(hwnd, L"确定", BATCH_OK, rc.right - 200, rc.bottom - 46, 88, 32);
      make_button(hwnd, L"取消", BATCH_CANCEL, rc.right - 102, rc.bottom - 46, 88, 32);
      SetFocus(edit);
      return 0;
    }
    case WM_COMMAND: {
      int id = LOWORD(wParam);
      if (id == BATCH_OK || id == BATCH_CANCEL) {
        auto ctx = reinterpret_cast<BatchCtx*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (id == BATCH_OK && ctx) {
          HWND edit = GetDlgItem(hwnd, BATCH_EDIT);
          int len = GetWindowTextLengthW(edit);
          ctx->text.assign(static_cast<size_t>(len), L'\0');
          if (len > 0) GetWindowTextW(edit, ctx->text.data(), len + 1);
        }
        DestroyWindow(hwnd);
        return 0;
      }
      return 0;
    }
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

std::vector<std::string> split_lines(const std::wstring& text) {
  std::vector<std::string> lines;
  std::wstring cur;
  for (wchar_t c : text) {
    if (c == L'\n') {
      if (!cur.empty()) lines.push_back(wide_to_utf8(cur));
      cur.clear();
    } else if (c != L'\r') {
      cur += c;
    }
  }
  if (!cur.empty()) lines.push_back(wide_to_utf8(cur));
  return lines;
}

void run_batch_dialog(HWND owner, bool negative) {
  BatchCtx ctx;
  HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RegexToolBatch",
                             negative ? L"批量粘贴反例（每行一条）" : L"批量粘贴正例（每行一条）",
                             WS_POPUP | WS_CAPTION | WS_SYSMENU,
                             CW_USEDEFAULT, CW_USEDEFAULT, 520, 320,
                             owner, nullptr, GetModuleHandleW(nullptr), &ctx);
  if (!dlg) return;
  HWND prev_focus = GetFocus();
  EnableWindow(owner, FALSE);
  MSG msg;
  bool quit = false;
  while (IsWindow(dlg)) {
    BOOL r = GetMessageW(&msg, nullptr, 0, 0);
    if (r <= 0) { if (r == 0) quit = true; break; }
    if (!IsDialogMessageW(dlg, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  EnableWindow(owner, TRUE);
  SetFocus(prev_focus);
  if (quit) PostQuitMessage(0);

  for (const std::string& line : split_lines(ctx.text)) {
    if (line.empty()) continue;
    if (negative) {
      add_example_row(owner, true);
      SetWindowTextW(g.neg_edits.back(), utf8_to_wide(line).c_str());
    } else {
      add_example_row(owner, false);
      SetWindowTextW(g.pos_edits.back(), utf8_to_wide(line).c_str());
    }
  }
  layout(owner);
}

// ---- segment rule text (工单 05) ----

std::string field_rule_text(const regex_tool::FieldRule& f) {
  std::string t = f.charset;
  if (f.min_len == 1 && f.max_len == 1) {
    // bare class
  } else if (f.max_len == SIZE_MAX) {
    t += "{" + std::to_string(f.min_len) + ",}";
  } else if (f.min_len == f.max_len) {
    t += "{" + std::to_string(f.min_len) + "}";
  } else {
    t += "{" + std::to_string(f.min_len) + "," + std::to_string(f.max_len) + "}";
  }
  return t;
}

// Parse "charset{n}" | "charset{n,m}" | "charset{n,}" | "charset".
bool parse_rule_text(const std::string& raw, std::string& charset, std::size_t& min_len,
                     std::size_t& max_len) {
  std::string t = trim(raw);
  if (t.empty()) return false;
  std::size_t brace = t.find('{');
  if (brace == std::string::npos) {
    charset = t;
    min_len = max_len = 1;
    return true;
  }
  charset = t.substr(0, brace);
  std::string inner = t.substr(brace + 1);
  if (!inner.empty() && inner.back() == '}') inner.pop_back();
  if (charset.empty()) return false;
  std::size_t comma = inner.find(',');
  if (comma == std::string::npos) {
    min_len = max_len = static_cast<std::size_t>(std::atoll(inner.c_str()));
  } else {
    min_len = static_cast<std::size_t>(std::atoll(inner.substr(0, comma).c_str()));
    std::string rest = inner.substr(comma + 1);
    max_len = rest.empty() ? SIZE_MAX : static_cast<std::size_t>(std::atoll(rest.c_str()));
  }
  return min_len <= max_len;
}

// Read the rule edits of the segment table into g.field_overrides. When the
// table does not exist yet (fresh start or a just-loaded project), keep the
// current overrides untouched.
void collect_overrides_from_seg_edits() {
  if (g.seg_edits.empty()) return;
  g.field_overrides.clear();
  for (size_t i = 0; i < g.seg_edits.size() && i < g.last_segments.size(); ++i) {
    const auto& seg = g.last_segments[i];
    if (seg.is_literal) continue;
    std::string text = wide_to_utf8(get_edit_text(g.seg_edits[i]));
    std::size_t p = text.find(": ");
    if (p == std::string::npos) continue;
    regex_tool::FieldRule ov;
    ov.name = seg.field.name;
    if (parse_rule_text(text.substr(p + 2), ov.charset, ov.min_len, ov.max_len)) {
      g.field_overrides.push_back(ov);
    }
  }
}

void destroy_segment_table() {
  for (HWND h : g.seg_edits) DestroyWindow(h);
  for (HWND h : g.seg_promote_btns) DestroyWindow(h);
  g.seg_edits.clear();
  g.seg_promote_btns.clear();
  g.last_segments.clear();
}

bool is_mode_lengths() {
  return SendMessageW(g.mode_len, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

bool is_mode_separators() {
  return SendMessageW(g.mode_sep, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

// 0 = auto, 1 = separators, 2 = lengths.
void set_mode(int which) {
  SendMessageW(g.mode_auto, BM_SETCHECK, which == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.mode_sep, BM_SETCHECK, which == 1 ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g.mode_len, BM_SETCHECK, which == 2 ? BST_CHECKED : BST_UNCHECKED, 0);
}

// Collect lengths from the per-segment boxes (工单 05: one box per segment).
std::vector<std::size_t> parse_lengths() {
  std::vector<std::size_t> out;
  for (HWND h : g.length_boxes) {
    std::string t = trim(wide_to_utf8(get_edit_text(h)));
    if (t.empty()) continue;
    std::size_t v = static_cast<std::size_t>(std::atoll(t.c_str()));
    if (v > 0) out.push_back(v);
  }
  return out;
}

// Separators are comma-separated, e.g. "@" or "@,|".
std::vector<std::string> parse_separators() {
  std::vector<std::string> out;
  std::string s = wide_to_utf8(get_edit_text(g.sep_edit));
  std::size_t pos = 0;
  while (pos <= s.size()) {
    std::size_t comma = s.find(',', pos);
    std::string part = trim(s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos));
    if (!part.empty()) out.push_back(part);
    if (comma == std::string::npos) break;
    pos = comma + 1;
  }
  return out;
}

// Rebuild the per-segment length boxes to match the chosen segment count,
// preserving the values already typed.
void rebuild_length_boxes(HWND hwnd) {
  std::vector<std::string> old;
  for (HWND h : g.length_boxes) old.push_back(wide_to_utf8(get_edit_text(h)));
  for (HWND h : g.length_boxes) DestroyWindow(h);
  g.length_boxes.clear();
  int cur = static_cast<int>(SendMessageW(g.seg_count_combo, CB_GETCURSEL, 0, 0));
  if (cur < 0) cur = 0;
  for (int i = 0; i <= cur; ++i) {
    HWND h = make_edit(hwnd, IDC_LEN_BASE + i, LEFT, 0, 56, CTRL_H, 0);
    if (i < static_cast<int>(old.size())) SetWindowTextW(h, utf8_to_wide(old[i]).c_str());
    g.length_boxes.push_back(h);
  }
  layout(hwnd);
}

// ---- engine interaction ----

void rebuild_segment_table(HWND hwnd) {
  for (HWND h : g.seg_edits) DestroyWindow(h);
  for (HWND h : g.seg_promote_btns) DestroyWindow(h);
  g.seg_edits.clear();
  g.seg_promote_btns.clear();

  // seg_promote_btns is aligned to segment index; nullptr means no button.
  for (size_t i = 0; i < g.last_segments.size(); ++i) {
    g.seg_promote_btns.push_back(nullptr);
  }
  for (size_t i = 0; i < g.last_segments.size(); ++i) {
    const auto& seg = g.last_segments[i];
    std::string text = std::to_string(i + 1) + ". ";
    DWORD extra = 0;
    if (seg.is_literal) {
      text += "固定: " + seg.literal;
      extra = ES_READONLY;
    } else {
      text += "字段 " + seg.field.name + ": ";
      // Show the override when one exists for this field, else the inferred rule.
      bool overridden = false;
      for (const auto& ov : g.field_overrides) {
        if (ov.name == seg.field.name) {
          text += field_rule_text(ov);
          overridden = true;
          break;
        }
      }
      if (!overridden) text += field_rule_text(seg.field);
    }
    HWND edit = make_edit(hwnd, IDC_SEG_EDIT_BASE + static_cast<int>(i), LEFT, 0, EDIT_W, CTRL_H, extra);
    SetWindowTextW(edit, utf8_to_wide(text).c_str());

    if (seg.is_literal) {
      auto tags = regex_tool::promotable_tags(seg.literal);
      if (!tags.empty()) {
        bool any_promoted = false;
        for (const auto& t : tags) {
          if (std::find(g.promote_tags.begin(), g.promote_tags.end(), t) != g.promote_tags.end()) {
            any_promoted = true;
            break;
          }
        }
        HWND btn = make_button(hwnd, any_promoted ? L"还原" : L"提升",
                               IDC_SEG_PROMOTE_BASE + static_cast<int>(i),
                               LEFT + EDIT_W + 6, 0, 44, CTRL_H);
        g.seg_promote_btns[i] = btn;
      }
    }
    g.seg_edits.push_back(edit);
  }
}

std::string render_candidate(const regex_tool::CandidateVerdict& vd, bool recommended) {
  const char* tag = vd.candidate.variant == "exact"    ? "精确"
                    : vd.candidate.variant == "relaxed" ? "长度放宽"
                                                        : "字符放宽";
  std::string out;
  out += recommended ? "候选（推荐）[" : "候选 [";
  out += tag;
  out += "]:\n";
  out += vd.candidate.regex + "\n";
  if (!vd.passes_all) out += "  失败 " + std::to_string(vd.failures) + " 条\n";
  out += "  正例:\n";
  for (const auto& v : vd.positive_verdicts) {
    out += v.passed ? "    ✓ " : "    ✗ ";
    out += v.example;
    if (!v.passed) {
      out += v.failing_segment != SIZE_MAX
                 ? "（卡在分段 " + std::to_string(v.failing_segment + 1) + "）"
                 : "（整串不匹配）";
    }
    out += "\n";
  }
  out += "  反例:\n";
  for (const auto& v : vd.negative_verdicts) {
    out += v.passed ? "    ✓ [排除] " : "    ✗ [漏过] ";
    out += v.example;
    if (!v.passed && v.failing_segment != SIZE_MAX) {
      out += "（卡在分段 " + std::to_string(v.failing_segment + 1) + "）";
    }
    out += "\n";
  }
  out += "\n";
  return out;
}

void run_generate(HWND hwnd) {
  collect_overrides_from_seg_edits();

  regex_tool::Project p;
  p.positives = collect_edits(g.pos_edits);
  p.negatives = collect_edits(g.neg_edits);
  auto& cfg = p.segment_config;
  cfg.use_lengths = is_mode_lengths();
  cfg.use_separators = is_mode_separators();
  cfg.lengths = parse_lengths();
  cfg.separators = parse_separators();
  cfg.promote_tags = g.promote_tags;
  cfg.field_overrides = g.field_overrides;

  auto result = regex_tool::generate(p);

  std::string text;
  if (!result.warnings.empty()) {
    text += "警告:\n";
    for (const auto& w : result.warnings) text += "- " + w + "\n";
    text += "\n";
  }
  if (result.candidates.empty()) {
    text += "（请至少输入一条正例）";
  } else {
    if (!result.has_passing) {
      text += "⚠ 没有任何候选通过全部正例/反例。以下候选未通过验证，仅按接近程度排序供定位问题：\n\n";
    }
    for (size_t i = 0; i < result.candidates.size(); ++i) {
      text += render_candidate(result.candidates[i], i == 0 && result.has_passing);
    }
  }
  SetWindowTextW(g.result_edit, utf8_to_wide(text).c_str());

  g.has_selftest_candidate = !result.candidates.empty();
  if (g.has_selftest_candidate) g.selftest_candidate = result.candidates.front().candidate;

  g.last_segments = result.candidates.empty()
                        ? std::vector<regex_tool::SegmentRule>{}
                        : result.candidates.front().candidate.segments;
  rebuild_segment_table(hwnd);
  layout(hwnd);
}

void run_selftest() {
  if (!g.has_selftest_candidate) {
    SetWindowTextW(g.selftest_label, L"自测: 请先生成候选");
    return;
  }
  std::string input = wide_to_utf8(get_edit_text(g.selftest_edit));
  auto v = regex_tool::verify_example(g.selftest_candidate, input);
  std::wstring msg = v.passed ? L"自测: 通过 ✓" : L"自测: 不通过 ✗";
  if (!v.passed) {
    if (v.failing_segment != SIZE_MAX) {
      msg += L"（卡在分段 " + std::to_wstring(v.failing_segment + 1) + L"）";
    } else {
      msg += L"（整串不匹配）";
    }
  }
  SetWindowTextW(g.selftest_label, msg.c_str());
}

// ---- project save/load (工单 08) ----

std::wstring exe_dir() {
  wchar_t buf[MAX_PATH];
  GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring p = buf;
  std::size_t pos = p.find_last_of(L"\\/");
  return pos == std::wstring::npos ? L"." : p.substr(0, pos);
}

std::wstring project_name() {
  std::wstring n = get_edit_text(g.project_combo);
  if (n.empty()) n = L"project-1";
  return n;
}

// List saved projects (exe-adjacent *.rgx files) into the combo box.
void populate_projects() {
  if (!g.project_combo) return;
  SendMessageW(g.project_combo, CB_RESETCONTENT, 0, 0);
  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW((exe_dir() + L"\\*.rgx").c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return;
  do {
    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      std::wstring name = fd.cFileName;
      std::size_t dot = name.find_last_of(L'.');
      if (dot != std::wstring::npos) name = name.substr(0, dot);
      SendMessageW(g.project_combo, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(name.c_str()));
    }
  } while (FindNextFileW(h, &fd));
  FindClose(h);
}

std::string serialize_project() {
  std::string out;
  out += "name=" + wide_to_utf8(project_name()) + "\n";
  out += std::string("mode=") + (is_mode_separators() ? "separators"
                                : is_mode_lengths() ? "lengths" : "auto") + "\n";
  std::string lengths_text;
  for (size_t i = 0; i < g.length_boxes.size(); ++i) {
    if (i) lengths_text += ",";
    lengths_text += wide_to_utf8(get_edit_text(g.length_boxes[i]));
  }
  out += "lengths=" + lengths_text + "\n";
  out += "separators=" + wide_to_utf8(get_edit_text(g.sep_edit)) + "\n";
  if (!g.promote_tags.empty()) {
    out += "promote=";
    for (size_t i = 0; i < g.promote_tags.size(); ++i) {
      if (i) out += ",";
      out += g.promote_tags[i];
    }
    out += "\n";
  }
  for (const auto& ov : g.field_overrides) {
    out += "override=" + ov.name + ":" + ov.charset + "," + std::to_string(ov.min_len) + "," +
           std::to_string(ov.max_len == SIZE_MAX ? 0 : ov.max_len) + "\n";
  }
  for (const auto& s : collect_edits(g.pos_edits)) out += "positive=" + s + "\n";
  for (const auto& s : collect_edits(g.neg_edits)) out += "negative=" + s + "\n";
  if (g.has_selftest_candidate) out += "selected=" + g.selftest_candidate.regex + "\n";
  return out;
}

bool save_project() {
  std::wstring path = exe_dir() + L"\\" + project_name() + L".rgx";
  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  f << serialize_project();
  return f.good();
}

// Load a project file into the UI. Returns false with a readable message on
// corrupt/unrecognized content — never crashes (工单 08).
bool load_project(const std::wstring& name, std::wstring& error) {
  std::wstring path = exe_dir() + L"\\" + name + L".rgx";
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    error = L"无法打开项目文件: " + path;
    return false;
  }
  std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

  std::vector<std::string> positives, negatives;
  bool use_lengths = false, use_separators = false;
  std::wstring lengths_text, separators_text;
  std::vector<std::string> promote;
  std::vector<regex_tool::FieldRule> overrides;
  std::string selected;
  bool saw_name = false;

  std::size_t pos = 0;
  while (pos < content.size()) {
    std::size_t nl = content.find('\n', pos);
    std::string line = trim(content.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos));
    pos = (nl == std::string::npos) ? content.size() : nl + 1;
    if (line.empty() || line[0] == '#') continue;
    std::size_t eq = line.find('=');
    if (eq == std::string::npos) {
      error = L"项目文件格式无法识别（第 " + std::to_wstring(static_cast<long long>(pos)) + L" 行附近）";
      return false;
    }
    std::string key = line.substr(0, eq);
    std::string value = line.substr(eq + 1);
    if (key == "positive") positives.push_back(value);
    else if (key == "negative") negatives.push_back(value);
    else if (key == "mode") { use_separators = (value == "separators"); use_lengths = (value == "lengths"); }
    else if (key == "lengths") lengths_text = utf8_to_wide(value);
    else if (key == "separators") separators_text = utf8_to_wide(value);
    else if (key == "name") saw_name = true;
    else if (key == "promote") {
      std::size_t b = 0;
      while (b <= value.size()) {
        std::size_t comma = value.find(',', b);
        std::string t = value.substr(b, comma == std::string::npos ? std::string::npos : comma - b);
        if (!t.empty()) promote.push_back(t);
        if (comma == std::string::npos) break;
        b = comma + 1;
      }
    } else if (key == "override") {
      std::size_t colon = value.find(':');
      if (colon == std::string::npos) {
        error = L"项目文件规则覆盖格式无法识别";
        return false;
      }
      regex_tool::FieldRule ov;
      ov.name = value.substr(0, colon);
      std::string rest = value.substr(colon + 1);
      std::size_t c1 = rest.find(',');
      std::size_t c2 = rest.find(',', c1 + 1);
      if (c1 == std::string::npos || c2 == std::string::npos) {
        error = L"项目文件规则覆盖格式无法识别";
        return false;
      }
      ov.charset = rest.substr(0, c1);
      ov.min_len = static_cast<std::size_t>(std::atoll(rest.substr(c1 + 1, c2 - c1 - 1).c_str()));
      ov.max_len = static_cast<std::size_t>(std::atoll(rest.substr(c2 + 1).c_str()));
      if (ov.max_len == 0) ov.max_len = SIZE_MAX;
      overrides.push_back(ov);
    } else if (key == "selected") selected = value;
    else {
      error = L"项目文件包含无法识别的条目: " + utf8_to_wide(key);
      return false;
    }
  }
  if (!saw_name) {
    error = L"项目文件格式无法识别（缺少 name）";
    return false;
  }

  // Apply to the UI.
  SetWindowTextW(g.project_combo, name.c_str());
  set_mode(use_separators ? 1 : use_lengths ? 2 : 0);
  SetWindowTextW(g.sep_edit, separators_text.c_str());
  {
    std::vector<std::string> parts;
    std::wstring cur;
    for (wchar_t c : lengths_text) {
      if (c == L',') { if (!cur.empty()) { parts.push_back(wide_to_utf8(cur)); cur.clear(); } }
      else cur += c;
    }
    if (!cur.empty()) parts.push_back(wide_to_utf8(cur));
    int count = static_cast<int>(parts.size());
    if (count > 10) count = 10;
    if (count < 1) count = 1;
    SendMessageW(g.seg_count_combo, CB_SETCURSEL, count - 1, 0);
    rebuild_length_boxes(g_main_hwnd);
    for (int i = 0; i < count && i < static_cast<int>(g.length_boxes.size()); ++i) {
      SetWindowTextW(g.length_boxes[i], utf8_to_wide(parts[i]).c_str());
    }
  }
  g.promote_tags = promote;
  g.field_overrides = overrides;
  g.has_selftest_candidate = !selected.empty();
  if (!selected.empty()) {
    g.selftest_candidate.regex = selected;
  }
  clear_example_rows(false);
  clear_example_rows(true);
  add_example_row(g_main_hwnd, false);
  for (size_t i = 0; i < positives.size(); ++i) {
    if (i > 0) add_example_row(g_main_hwnd, false);
    SetWindowTextW(g.pos_edits[i], utf8_to_wide(positives[i]).c_str());
  }
  for (size_t i = 0; i < negatives.size(); ++i) {
    add_example_row(g_main_hwnd, true);
    SetWindowTextW(g.neg_edits.back(), utf8_to_wide(negatives[i]).c_str());
  }
  destroy_segment_table();  // force the loaded overrides to survive the next generate
  run_generate(g_main_hwnd);
  if (!selected.empty()) {
    SetWindowTextW(g.selftest_label,
                   (L"已加载项目，选定正则: " + utf8_to_wide(selected)).c_str());
  }
  return true;
}

// ---- layout ----

// Total height of the scrollable left column, in client pixels.
int content_height() {
  int y = 8;
  y += ROW_H + 4;                                            // project row
  y += 20;                                                    // positives label
  y += static_cast<int>(g.pos_edits.size()) * ROW_H;
  y += ROW_H + 6;                                             // +正例 / 批量粘贴
  y += 20;                                                    // negatives label
  y += static_cast<int>(g.neg_edits.size()) * ROW_H;
  y += ROW_H + 6;                                             // +反例 / 批量粘贴
  y += 20;                                                    // 分段配置 label
  y += ROW_H;                                                 // mode radios
  y += ROW_H;                                                 // separators + segment count
  y += static_cast<int>((g.length_boxes.size() + LEN_PER_LINE - 1) / LEN_PER_LINE) * ROW_H;
  y += static_cast<int>(g.seg_edits.size()) * ROW_H;          // segment rule table
  y += ROW_H;                                                 // generate button
  return y + 30;
}

void layout(HWND hwnd) {
  RECT rc;
  GetClientRect(hwnd, &rc);
  int view = std::max(static_cast<int>(rc.bottom), 1);

  // Clamp the scroll position to what the scrollbar can actually show, so a
  // shrinking content (rows deleted, window enlarged) cannot leave the layout
  // positioned far off-screen (the "elements clumped together" symptom).
  int content = std::max(content_height(), view);
  int max_pos = std::max(content - view, 0);
  if (g.scroll_pos > max_pos) g.scroll_pos = max_pos;

  int y = 8 - g.scroll_pos;

  MoveWindow(g.project_combo, LEFT, y, 150, CTRL_H, TRUE);
  MoveWindow(g.new_btn, LEFT + 156, y, 56, CTRL_H, TRUE);
  MoveWindow(g.save_btn, LEFT + 218, y, 56, CTRL_H, TRUE);
  MoveWindow(g.load_btn, LEFT + 280, y, 56, CTRL_H, TRUE);
  y += ROW_H + 4;

  MoveWindow(g.pos_label, LEFT, y, 160, 18, TRUE);
  y += 20;
  for (size_t i = 0; i < g.pos_edits.size(); ++i) {
    MoveWindow(g.pos_edits[i], LEFT, y, EDIT_W, CTRL_H, TRUE);
    MoveWindow(g.pos_dels[i], LEFT + EDIT_W + 6, y, DEL_W, CTRL_H, TRUE);
    y += ROW_H;
  }
  MoveWindow(g.add_pos, LEFT, y, 80, CTRL_H, TRUE);
  MoveWindow(g.batch_pos, LEFT + 88, y, 88, CTRL_H, TRUE);
  y += ROW_H + 6;

  MoveWindow(g.neg_label, LEFT, y, 160, 18, TRUE);
  y += 20;
  for (size_t i = 0; i < g.neg_edits.size(); ++i) {
    MoveWindow(g.neg_edits[i], LEFT, y, EDIT_W, CTRL_H, TRUE);
    MoveWindow(g.neg_dels[i], LEFT + EDIT_W + 6, y, DEL_W, CTRL_H, TRUE);
    y += ROW_H;
  }
  MoveWindow(g.add_neg, LEFT, y, 80, CTRL_H, TRUE);
  MoveWindow(g.batch_neg, LEFT + 88, y, 88, CTRL_H, TRUE);
  y += ROW_H + 6;

  MoveWindow(g.cfg_label, LEFT, y, 120, 18, TRUE);
  y += 20;
  MoveWindow(g.mode_auto, LEFT, y, 76, CTRL_H, TRUE);
  MoveWindow(g.mode_sep, LEFT + 82, y, 88, CTRL_H, TRUE);
  MoveWindow(g.mode_len, LEFT + 176, y, 76, CTRL_H, TRUE);
  y += ROW_H;

  MoveWindow(g.sep_label, LEFT, y, 62, CTRL_H, TRUE);
  MoveWindow(g.sep_edit, LEFT + 66, y, 92, CTRL_H, TRUE);
  MoveWindow(g.count_label, LEFT + 168, y, 46, CTRL_H, TRUE);
  MoveWindow(g.seg_count_combo, LEFT + 218, y, 60, CTRL_H, TRUE);
  y += ROW_H;

  for (size_t i = 0; i < g.length_boxes.size(); ++i) {
    MoveWindow(g.length_boxes[i],
               LEFT + static_cast<int>(i % LEN_PER_LINE) * 62,
               y + static_cast<int>(i / LEN_PER_LINE) * ROW_H, 56, CTRL_H, TRUE);
  }
  y += static_cast<int>((g.length_boxes.size() + LEN_PER_LINE - 1) / LEN_PER_LINE) * ROW_H;

  for (size_t i = 0; i < g.seg_edits.size(); ++i) {
    MoveWindow(g.seg_edits[i], LEFT, y, EDIT_W, CTRL_H, TRUE);
    if (i < g.seg_promote_btns.size() && g.seg_promote_btns[i]) {
      MoveWindow(g.seg_promote_btns[i], LEFT + EDIT_W + 6, y, 44, CTRL_H, TRUE);
    }
    y += ROW_H;
  }
  MoveWindow(g.generate_btn, LEFT, y, 88, CTRL_H, TRUE);

  SCROLLINFO si{};
  si.cbSize = sizeof(si);
  si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
  si.nMin = 0;
  si.nMax = content;
  si.nPage = static_cast<UINT>(view);
  si.nPos = g.scroll_pos;
  SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

  int result_w = std::max(static_cast<int>(rc.right) - RIGHT_X - 8, 120);
  int result_h = std::max(view - 40 - 88, 60);
  MoveWindow(g.result_edit, RIGHT_X, 40, result_w, result_h, TRUE);

  MoveWindow(g.selftest_label, RIGHT_X, view - 78, result_w, 18, TRUE);
  MoveWindow(g.selftest_edit, RIGHT_X, view - 52, result_w - 84, CTRL_H, TRUE);
  MoveWindow(g.selftest_btn, RIGHT_X + result_w - 76, view - 52, 68, CTRL_H, TRUE);
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE: {
      g.font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

      g.project_combo = make_combo(hwnd, IDC_PROJECT_EDIT, LEFT, 0, 150, 200, true);
      g.new_btn = make_button(hwnd, L"新建", IDC_NEW, 0, 0, 0, 0);
      g.save_btn = make_button(hwnd, L"保存", IDC_SAVE, 0, 0, 0, 0);
      g.load_btn = make_button(hwnd, L"加载", IDC_LOAD, 0, 0, 0, 0);

      g.pos_label = make_static(hwnd, L"正例（必须匹配）", LEFT, 0, 160, 18);
      g.add_pos = make_button(hwnd, L"+ 正例", IDC_ADD_POS, 0, 0, 0, 0);
      g.batch_pos = make_button(hwnd, L"批量粘贴", IDC_BATCH_POS, 0, 0, 0, 0);

      g.neg_label = make_static(hwnd, L"反例（必须排除）", LEFT, 0, 160, 18);
      g.add_neg = make_button(hwnd, L"+ 反例", IDC_ADD_NEG, 0, 0, 0, 0);
      g.batch_neg = make_button(hwnd, L"批量粘贴", IDC_BATCH_NEG, 0, 0, 0, 0);

      g.cfg_label = make_static(hwnd, L"分段配置", LEFT, 0, 120, 18);
      g.mode_auto = make_radio(hwnd, L"自动分段", IDC_MODE_AUTO, 0, 0, 0, 0, true);
      g.mode_sep = make_radio(hwnd, L"按分隔符", IDC_MODE_SEP, 0, 0, 0, 0, false);
      g.mode_len = make_radio(hwnd, L"按长度", IDC_MODE_LEN, 0, 0, 0, 0, false);
      set_mode(0);
      g.sep_label = make_static(hwnd, L"分隔符:", LEFT, 0, 62, CTRL_H);
      g.sep_edit = make_edit(hwnd, IDC_SEP_EDIT, LEFT + 66, 0, 92, CTRL_H, 0);
      g.count_label = make_static(hwnd, L"段数:", LEFT + 168, 0, 46, CTRL_H);
      g.seg_count_combo = make_combo(hwnd, IDC_SEG_COUNT_COMBO, LEFT + 218, 0, 60, 200, false);
      for (int i = 1; i <= 10; ++i) {
        std::wstring t = std::to_wstring(i);
        SendMessageW(g.seg_count_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t.c_str()));
      }
      SendMessageW(g.seg_count_combo, CB_SETCURSEL, 1, 0);  // 2 segments by default

      g.generate_btn = make_button(hwnd, L"生成", IDC_GENERATE, 0, 0, 0, 0);

      g.result_edit = make_edit(hwnd, IDC_RESULT_EDIT, 0, 0, 0, 0,
                                ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL);
      g.selftest_label = make_static(hwnd, L"自测:", RIGHT_X, 0, 200, 18);
      g.selftest_edit = make_edit(hwnd, IDC_SELFTEST_EDIT, 0, 0, 0, 0, 0);
      g.selftest_btn = make_button(hwnd, L"判定", IDC_SELFTEST_BTN, 0, 0, 0, 0);

      populate_projects();
      rebuild_length_boxes(hwnd);
      add_example_row(hwnd, false);  // start with one empty positive row
      return 0;
    }

    case WM_SIZE:
      layout(hwnd);
      return 0;

    case WM_GETMINMAXINFO: {
      // The right pane (results + self-test) is laid out against the window
      // height; forbid a window too small for it to fit without overlapping.
      auto mmi = reinterpret_cast<MINMAXINFO*>(lParam);
      mmi->ptMinTrackSize.x = 620;
      mmi->ptMinTrackSize.y = 420;
      return 0;
    }

    case WM_VSCROLL: {
      SCROLLINFO si{};
      si.cbSize = sizeof(si);
      si.fMask = SIF_ALL;
      GetScrollInfo(hwnd, SB_VERT, &si);
      int pos = si.nPos;
      switch (LOWORD(wParam)) {
        case SB_LINEUP: pos -= 1; break;
        case SB_LINEDOWN: pos += 1; break;
        case SB_PAGEUP: pos -= static_cast<int>(si.nPage); break;
        case SB_PAGEDOWN: pos += static_cast<int>(si.nPage); break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: pos = si.nTrackPos; break;
        default: return 0;
      }
      int max_pos = std::max(si.nMax - static_cast<int>(si.nPage), 0);
      pos = std::clamp(pos, 0, max_pos);
      if (pos != g.scroll_pos) {
        g.scroll_pos = pos;
        si.fMask = SIF_POS;
        si.nPos = pos;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        layout(hwnd);
      }
      return 0;
    }

    case WM_MOUSEWHEEL: {
      short delta = GET_WHEEL_DELTA_WPARAM(wParam);
      SendMessageW(hwnd, WM_VSCROLL, delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
      return 0;
    }

    case WM_COMMAND: {
      int id = LOWORD(wParam);
      int code = HIWORD(wParam);
      // 工单 07: the self-test box judges as you type, not only on the button.
      if (id == IDC_SELFTEST_EDIT && code == EN_CHANGE) {
        run_selftest();
        return 0;
      }
      // Picking a saved project from the combo loads it (工单 08).
      if (id == IDC_PROJECT_EDIT && code == CBN_SELCHANGE) {
        std::wstring sel = get_edit_text(g.project_combo);
        if (!sel.empty()) {
          std::wstring err;
          if (!load_project(sel, err)) MessageBoxW(hwnd, err.c_str(), L"加载项目", MB_ICONERROR);
        }
        return 0;
      }
      // Choosing the segment count rebuilds the per-segment length boxes.
      if (id == IDC_SEG_COUNT_COMBO && code == CBN_SELCHANGE) {
        rebuild_length_boxes(hwnd);
        return 0;
      }
      if (code != BN_CLICKED) return 0;
      if (id == IDC_NEW) {
        g.promote_tags.clear();
        g.field_overrides.clear();
        g.has_selftest_candidate = false;
        SetWindowTextW(g.project_combo, L"");
        SetWindowTextW(g.sep_edit, L"");
        SetWindowTextW(g.result_edit, L"");
        SetWindowTextW(g.selftest_label, L"自测:");
        set_mode(0);
        SendMessageW(g.seg_count_combo, CB_SETCURSEL, 1, 0);
        rebuild_length_boxes(hwnd);
        clear_example_rows(false);
        clear_example_rows(true);
        destroy_segment_table();
        add_example_row(hwnd, false);
        layout(hwnd);
        return 0;
      }
      if (id == IDC_SAVE) {
        if (save_project()) {
          SetWindowTextW(g.selftest_label, (L"已保存: " + project_name() + L".rgx").c_str());
        } else {
          MessageBoxW(hwnd, L"保存失败（无法写入文件）", L"保存项目", MB_ICONERROR);
        }
        return 0;
      }
      if (id == IDC_LOAD) {
        std::wstring error;
        if (!load_project(project_name(), error)) {
          MessageBoxW(hwnd, error.c_str(), L"加载项目", MB_ICONERROR);
        }
        return 0;
      }
      if (id == IDC_ADD_POS) { add_example_row(hwnd, false); return 0; }
      if (id == IDC_ADD_NEG) { add_example_row(hwnd, true); return 0; }
      if (id == IDC_BATCH_POS) { run_batch_dialog(hwnd, false); return 0; }
      if (id == IDC_BATCH_NEG) { run_batch_dialog(hwnd, true); return 0; }
      if (id == IDC_MODE_AUTO) { set_mode(0); return 0; }
      if (id == IDC_MODE_SEP) { set_mode(1); return 0; }
      if (id == IDC_MODE_LEN) { set_mode(2); return 0; }
      if (id == IDC_GENERATE) { run_generate(hwnd); return 0; }
      if (id == IDC_SELFTEST_BTN) { run_selftest(); return 0; }
      if (id >= IDC_POS_DEL_BASE && id < IDC_POS_DEL_BASE + 10000) {
        remove_example_row(hwnd, false, reinterpret_cast<HWND>(lParam));
        return 0;
      }
      if (id >= IDC_NEG_DEL_BASE && id < IDC_NEG_DEL_BASE + 10000) {
        remove_example_row(hwnd, true, reinterpret_cast<HWND>(lParam));
        return 0;
      }
      if (id >= IDC_SEG_PROMOTE_BASE && id < IDC_SEG_PROMOTE_BASE + 10000) {
        size_t index = static_cast<size_t>(id - IDC_SEG_PROMOTE_BASE);
        if (index < g.last_segments.size() && g.last_segments[index].is_literal) {
          auto tags = regex_tool::promotable_tags(g.last_segments[index].literal);
          bool any_promoted = false;
          for (const auto& t : tags) {
            if (std::find(g.promote_tags.begin(), g.promote_tags.end(), t) != g.promote_tags.end()) {
              any_promoted = true;
              break;
            }
          }
          if (any_promoted) {  // 还原: remove all tags of this literal
            for (const auto& t : tags) {
              auto it = std::find(g.promote_tags.begin(), g.promote_tags.end(), t);
              if (it != g.promote_tags.end()) g.promote_tags.erase(it);
            }
          } else {  // 提升: add all promotable tags of this literal
            for (const auto& t : tags) {
              if (std::find(g.promote_tags.begin(), g.promote_tags.end(), t) == g.promote_tags.end()) {
                g.promote_tags.push_back(t);
              }
            }
          }
          run_generate(hwnd);
        }
        return 0;
      }
      return 0;
    }

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  WNDCLASSW wc{};
  wc.lpfnWndProc = wnd_proc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
  wc.lpszClassName = L"RegexToolMain";
  RegisterClassW(&wc);

  WNDCLASSW bc{};
  bc.lpfnWndProc = batch_proc;
  bc.hInstance = hInstance;
  bc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
  bc.lpszClassName = L"RegexToolBatch";
  RegisterClassW(&bc);

  g_main_hwnd = CreateWindowExW(0, L"RegexToolMain", L"条码正则生成器",
                                WS_OVERLAPPEDWINDOW | WS_VSCROLL,
                                CW_USEDEFAULT, CW_USEDEFAULT, 940, 640,
                                nullptr, nullptr, hInstance, nullptr);
  ShowWindow(g_main_hwnd, nCmdShow);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
}
