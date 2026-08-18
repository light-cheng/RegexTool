// Win32 UI for the Regex Tool.
// Modern Windows UI with DPI awareness, Visual Styles, isolated scroll viewport,
// dedicated recommended regex highlight box, and structured validation details.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "engine/engine.h"
#include "resource.h"

#ifndef ECM_FIRST
#define ECM_FIRST 0x1500
#endif
#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER (ECM_FIRST + 1)
#endif

namespace {

// Control IDs — non-overlapping intervals (P3-3)
constexpr int IDC_PROJECT_EDIT = 1000;
constexpr int IDC_NEW = 1001;
constexpr int IDC_SAVE = 1002;
constexpr int IDC_LOAD = 1003;
constexpr int IDC_SCROLL_CONTAINER = 1004;
constexpr int IDC_ADD_POS = 1005;
constexpr int IDC_ADD_NEG = 1006;
constexpr int IDC_BATCH_POS = 1007;
constexpr int IDC_BATCH_NEG = 1008;
constexpr int IDC_MODE_AUTO = 1009;
constexpr int IDC_MODE_SEP = 1010;
constexpr int IDC_MODE_LEN = 1011;
constexpr int IDC_SEP_EDIT = 1012;
constexpr int IDC_SEG_COUNT_COMBO = 1013;
constexpr int IDC_GENERATE = 1014;
constexpr int IDC_REC_REGEX_EDIT = 1015;
constexpr int IDC_COPY_REGEX = 1016;
constexpr int IDC_COPY_CAP_REGEX = 1017;
constexpr int IDC_RESULT_EDIT = 1018;
constexpr int IDC_SELFTEST_EDIT = 1019;
constexpr int IDC_SELFTEST_BTN = 1020;
constexpr int IDC_STATUSBAR = 1021;

// Dynamic rows ID intervals
constexpr int IDC_POS_EDIT_BASE = 2000;      // 2000 .. 2499
constexpr int IDC_POS_DEL_BASE = 2500;       // 2500 .. 2999
constexpr int IDC_NEG_EDIT_BASE = 3000;      // 3000 .. 3499
constexpr int IDC_NEG_DEL_BASE = 3500;       // 3500 .. 3999
constexpr int IDC_SEG_EDIT_BASE = 4000;      // 4000 .. 4499
constexpr int IDC_SEG_PROMOTE_BASE = 4500;   // 4500 .. 4999
constexpr int IDC_LEN_BASE = 5000;           // 5000 .. 5499

// Base metrics at 96 DPI (scaled dynamically with window DPI)
constexpr int BASE_LEFT_MARGIN = 14;
constexpr int BASE_ROW_H = 32;
constexpr int BASE_CTRL_H = 25;
constexpr int BASE_DEL_W = 28;
constexpr std::size_t LEN_PER_LINE = 5;

HWND g_main_hwnd = nullptr;

UINT get_window_dpi(HWND hwnd) {
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  if (user32) {
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    auto fn = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"));
    if (fn) return fn(hwnd);
  }
  HDC hdc = GetDC(hwnd);
  UINT dpi = 96;
  if (hdc) {
    dpi = static_cast<UINT>(GetDeviceCaps(hdc, LOGPIXELSX));
    ReleaseDC(hwnd, hdc);
  }
  return dpi ? dpi : 96;
}

struct AppState {
  HWND main_hwnd = nullptr;
  HWND scroll_container = nullptr;
  UINT dpi = 96;

  // Modern UI Brushes & Fonts
  HBRUSH bg_brush = nullptr;
  HBRUSH edit_bg_brush = nullptr;
  HBRUSH readonly_bg_brush = nullptr;
  HBRUSH rec_bg_brush = nullptr;

  HFONT font = nullptr;
  HFONT bold_font = nullptr;
  HFONT header_font = nullptr;
  HFONT mono_font = nullptr;
  HFONT mono_bold_font = nullptr;

  // Controls - Fixed Left Pane Header
  HWND project_combo = nullptr;
  HWND new_btn = nullptr, save_btn = nullptr, load_btn = nullptr;

  // Controls - Scrollable Viewport (Children of scroll_container)
  HWND pos_label = nullptr, neg_label = nullptr, cfg_label = nullptr;
  HWND mode_auto = nullptr, mode_sep = nullptr, mode_len = nullptr;
  HWND sep_label = nullptr, sep_edit = nullptr;
  HWND count_label = nullptr, seg_count_combo = nullptr;
  std::vector<HWND> length_boxes;
  std::vector<HWND> pos_edits, pos_dels;
  std::vector<HWND> neg_edits, neg_dels;
  HWND add_pos = nullptr, add_neg = nullptr, batch_pos = nullptr, batch_neg = nullptr;
  std::vector<HWND> seg_edits, seg_promote_btns;

  // Controls - Fixed Left Pane Footer
  HWND generate_btn = nullptr;

  // Controls - Right Pane
  HWND rec_header_label = nullptr;
  HWND copy_btn = nullptr;
  HWND copy_cap_btn = nullptr;
  HWND rec_regex_edit = nullptr;
  HWND result_header_label = nullptr;
  HWND result_edit = nullptr;
  HWND selftest_title = nullptr;
  HWND selftest_edit = nullptr, selftest_btn = nullptr;
  HWND selftest_label = nullptr;

  // Status Bar
  HWND status_bar = nullptr;

  // Scroll state
  int scroll_pos = 0;

  // Engine state
  std::vector<regex_tool::SegmentRule> last_segments;
  std::vector<std::string> promote_tags;
  std::vector<regex_tool::FieldRule> field_overrides;
  regex_tool::Candidate selftest_candidate;
  bool has_selftest_candidate = false;
  std::string recommended_regex;
  std::string recommended_capturing_regex;

  int scale_dpi(int val) const {
    return MulDiv(val, static_cast<int>(dpi), 96);
  }

  void init_resources(HWND window_hwnd) {
    main_hwnd = window_hwnd;
    dpi = get_window_dpi(main_hwnd);
    if (!bg_brush) bg_brush = CreateSolidBrush(RGB(243, 243, 243));
    if (!edit_bg_brush) edit_bg_brush = CreateSolidBrush(RGB(255, 255, 255));
    if (!readonly_bg_brush) readonly_bg_brush = CreateSolidBrush(RGB(236, 236, 236));
    if (!rec_bg_brush) rec_bg_brush = CreateSolidBrush(RGB(248, 250, 252));
    update_fonts();
  }

  void update_fonts() {
    if (font) DeleteObject(font);
    if (bold_font) DeleteObject(bold_font);
    if (header_font) DeleteObject(header_font);
    if (mono_font) DeleteObject(mono_font);
    if (mono_bold_font) DeleteObject(mono_bold_font);

    int font_h = -MulDiv(9, static_cast<int>(dpi), 72);
    int hdr_h = -MulDiv(10, static_cast<int>(dpi), 72);
    int mono_h = -MulDiv(9, static_cast<int>(dpi), 72);
    int mono_bold_h = -MulDiv(11, static_cast<int>(dpi), 72);

    font = CreateFontW(font_h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    bold_font = CreateFontW(font_h, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    header_font = CreateFontW(hdr_h, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    mono_font = CreateFontW(mono_h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    mono_bold_font = CreateFontW(mono_bold_h, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
  }

  void cleanup_resources() {
    if (bg_brush) { DeleteObject(bg_brush); bg_brush = nullptr; }
    if (edit_bg_brush) { DeleteObject(edit_bg_brush); edit_bg_brush = nullptr; }
    if (readonly_bg_brush) { DeleteObject(readonly_bg_brush); readonly_bg_brush = nullptr; }
    if (rec_bg_brush) { DeleteObject(rec_bg_brush); rec_bg_brush = nullptr; }
    if (font) { DeleteObject(font); font = nullptr; }
    if (bold_font) { DeleteObject(bold_font); bold_font = nullptr; }
    if (header_font) { DeleteObject(header_font); header_font = nullptr; }
    if (mono_font) { DeleteObject(mono_font); mono_font = nullptr; }
    if (mono_bold_font) { DeleteObject(mono_bold_font); mono_bold_font = nullptr; }
  }
};

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
  if (!edit) return L"";
  int len = GetWindowTextLengthW(edit);
  if (len <= 0) return L"";
  std::wstring w(static_cast<size_t>(len), L'\0');
  GetWindowTextW(edit, w.data(), len + 1);
  return w;
}

bool parse_size_t(const std::string& raw, std::size_t& out) {
  std::string s = regex_tool::trim(raw);
  if (s.empty()) return false;
  for (char c : s) {
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  }
  char* end = nullptr;
  errno = 0;
  unsigned long long v = std::strtoull(s.c_str(), &end, 10);
  if (errno != 0 || end == s.c_str() || *end != '\0') return false;
  if (v > SIZE_MAX) return false;
  out = static_cast<std::size_t>(v);
  return true;
}

void set_status(HWND hwnd, const std::wstring& msg) {
  auto g = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (g && g->status_bar) {
    SendMessageW(g->status_bar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(msg.c_str()));
  }
}

void update_theme_titlebar(HWND hwnd) {
  HKEY hKey;
  DWORD apps_use_light = 1;
  DWORD size = sizeof(apps_use_light);
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                    0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                     reinterpret_cast<LPBYTE>(&apps_use_light), &size);
    RegCloseKey(hKey);
  }
  BOOL dark = (apps_use_light == 0);
  if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark)))) {
    DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
  }
}

AppState* get_app_state(HWND hwnd) {
  if (!hwnd) return nullptr;
  HWND root = GetAncestor(hwnd, GA_ROOT);
  if (!root) root = hwnd;
  return reinterpret_cast<AppState*>(GetWindowLongPtrW(root, GWLP_USERDATA));
}

void copy_to_clipboard(HWND hwnd, const std::wstring& text) {
  if (text.empty()) return;
  if (!OpenClipboard(hwnd)) return;
  EmptyClipboard();
  size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (hMem) {
    void* pMem = GlobalLock(hMem);
    if (pMem) {
      memcpy(pMem, text.c_str(), bytes);
      GlobalUnlock(hMem);
      SetClipboardData(CF_UNICODETEXT, hMem);
    }
  }
  CloseClipboard();
}

constexpr DWORD CHILD_STYLE = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS;

HWND make_button(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h, DWORD extra = 0) {
  auto g = get_app_state(parent);
  HWND btn = CreateWindowExW(0, L"BUTTON", text,
                             CHILD_STYLE | BS_PUSHBUTTON | extra,
                             x, y, w, h, parent,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
  if (g && g->font) SendMessageW(btn, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
  return btn;
}

HWND make_static(HWND parent, const wchar_t* text, int x, int y, int w, int h, bool header = false) {
  auto g = get_app_state(parent);
  HWND st = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                            x, y, w, h, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
  if (g) {
    HFONT f = header && g->header_font ? g->header_font : (g->font ? g->font : nullptr);
    if (f) SendMessageW(st, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE);
  }
  return st;
}

HWND make_edit(HWND parent, int id, int x, int y, int w, int h, DWORD extra_style, const wchar_t* cue = nullptr) {
  auto g = get_app_state(parent);
  DWORD base = CHILD_STYLE | WS_BORDER | extra_style;
  if (!(extra_style & ES_MULTILINE)) base |= ES_AUTOHSCROLL;
  HWND edit = CreateWindowExW(0, L"EDIT", L"",
                              base,
                              x, y, w, h, parent,
                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                              GetModuleHandleW(nullptr), nullptr);
  if (g && g->font) SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
  if (cue) {
    SendMessageW(edit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(cue));
  }
  return edit;
}

HWND make_combo(HWND parent, int id, int x, int y, int w, int h, bool editable, const wchar_t* cue = nullptr) {
  auto g = get_app_state(parent);
  HWND cb = CreateWindowExW(0, L"COMBOBOX", L"",
                            CHILD_STYLE | (editable ? CBS_DROPDOWN : CBS_DROPDOWNLIST) | WS_VSCROLL,
                            x, y, w, h, parent,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                            GetModuleHandleW(nullptr), nullptr);
  if (g && g->font) SendMessageW(cb, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
  if (cue && editable) {
    HWND edit = FindWindowExW(cb, nullptr, L"EDIT", nullptr);
    if (edit) SendMessageW(edit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(cue));
  }
  return cb;
}

HWND make_radio(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h, bool first) {
  auto g = get_app_state(parent);
  HWND btn = CreateWindowExW(0, L"BUTTON", text,
                             CHILD_STYLE | BS_AUTORADIOBUTTON | (first ? WS_GROUP : 0),
                             x, y, w, h, parent,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
  if (g && g->font) SendMessageW(btn, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
  return btn;
}

void layout(HWND hwnd);
void layout_scroll_content(HWND scroll_hwnd);

int next_free_id(AppState* g, int base) {
  int best = base;
  auto scan = [&](const std::vector<HWND>& hs) {
    for (HWND h : hs) {
      int id = static_cast<int>(GetDlgCtrlID(h));
      if (id >= base && id < base + 500 && id >= best) best = id + 1;
    }
  };
  scan(g->pos_edits);
  scan(g->pos_dels);
  scan(g->neg_edits);
  scan(g->neg_dels);
  return best;
}

void add_example_row(HWND hwnd, bool negative) {
  auto g = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (!g || !g->scroll_container) return;
  auto& edits = negative ? g->neg_edits : g->pos_edits;
  auto& dels = negative ? g->neg_dels : g->pos_dels;
  int edit_id = next_free_id(g, negative ? IDC_NEG_EDIT_BASE : IDC_POS_EDIT_BASE);
  int del_id = next_free_id(g, negative ? IDC_NEG_DEL_BASE : IDC_POS_DEL_BASE);

  const wchar_t* cue = negative ? L"输入反例条码（必须被排除）..."
                                : L"输入正例条码，如 PN8821@BN9012@DT2026...";
  HWND edit = make_edit(g->scroll_container, edit_id, 0, 0, g->scale_dpi(320), g->scale_dpi(BASE_CTRL_H), 0, cue);
  HWND del = make_button(g->scroll_container, L"\u00d7", del_id, 0, 0, g->scale_dpi(BASE_DEL_W), g->scale_dpi(BASE_CTRL_H)); // ×
  edits.push_back(edit);
  dels.push_back(del);
  layout_scroll_content(g->scroll_container);
  SetFocus(edit);
}

void remove_example_row(HWND hwnd, bool negative, HWND del_button) {
  auto g = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (!g || !g->scroll_container) return;
  auto& edits = negative ? g->neg_edits : g->pos_edits;
  auto& dels = negative ? g->neg_dels : g->pos_dels;
  auto it = std::find(dels.begin(), dels.end(), del_button);
  if (it == dels.end()) return;
  size_t index = static_cast<size_t>(it - dels.begin());
  DestroyWindow(edits[index]);
  DestroyWindow(dels[index]);
  edits.erase(edits.begin() + static_cast<ptrdiff_t>(index));
  dels.erase(dels.begin() + static_cast<ptrdiff_t>(index));
  layout_scroll_content(g->scroll_container);
}

void clear_example_rows(AppState* g, bool negative) {
  auto& edits = negative ? g->neg_edits : g->pos_edits;
  auto& dels = negative ? g->neg_dels : g->pos_dels;
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

// Batch Paste Dialog
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
      HWND edit = CreateWindowExW(0, L"EDIT", L"",
                                  CHILD_STYLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                                  12, 12, rc.right - 24, rc.bottom - 60, hwnd,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(BATCH_EDIT)),
                                  GetModuleHandleW(nullptr), nullptr);
      HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
      SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
      HWND ok = CreateWindowExW(0, L"BUTTON", L"确定", CHILD_STYLE | BS_PUSHBUTTON,
                                rc.right - 190, rc.bottom - 40, 80, 28, hwnd,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(BATCH_OK)),
                                GetModuleHandleW(nullptr), nullptr);
      HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消", CHILD_STYLE | BS_PUSHBUTTON,
                                    rc.right - 96, rc.bottom - 40, 80, 28, hwnd,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(BATCH_CANCEL)),
                                    GetModuleHandleW(nullptr), nullptr);
      SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
      SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
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
                             CW_USEDEFAULT, CW_USEDEFAULT, 520, 340,
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

  auto g = reinterpret_cast<AppState*>(GetWindowLongPtrW(owner, GWLP_USERDATA));
  if (!g) return;

  for (const std::string& line : split_lines(ctx.text)) {
    if (line.empty()) continue;
    if (negative) {
      add_example_row(owner, true);
      SetWindowTextW(g->neg_edits.back(), utf8_to_wide(line).c_str());
    } else {
      add_example_row(owner, false);
      SetWindowTextW(g->pos_edits.back(), utf8_to_wide(line).c_str());
    }
  }
  layout_scroll_content(g->scroll_container);
}

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

bool parse_rule_text(const std::string& raw, std::string& charset, std::size_t& min_len,
                     std::size_t& max_len) {
  std::string t = regex_tool::trim(raw);
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
    if (!parse_size_t(inner, min_len)) return false;
    max_len = min_len;
  } else {
    if (!parse_size_t(inner.substr(0, comma), min_len)) return false;
    std::string rest = regex_tool::trim(inner.substr(comma + 1));
    if (rest.empty()) {
      max_len = SIZE_MAX;
    } else {
      if (!parse_size_t(rest, max_len)) return false;
    }
  }
  return min_len <= max_len;
}

void collect_overrides_from_seg_edits(AppState* g) {
  if (g->seg_edits.empty()) return;
  g->field_overrides.clear();
  for (size_t i = 0; i < g->seg_edits.size() && i < g->last_segments.size(); ++i) {
    const auto& seg = g->last_segments[i];
    if (seg.is_literal) continue;
    std::string text = wide_to_utf8(get_edit_text(g->seg_edits[i]));
    std::size_t p = text.find(": ");
    if (p == std::string::npos) continue;
    regex_tool::FieldRule ov;
    ov.name = seg.field.name;
    if (parse_rule_text(text.substr(p + 2), ov.charset, ov.min_len, ov.max_len)) {
      g->field_overrides.push_back(ov);
    }
  }
}

void destroy_segment_table(AppState* g) {
  if (!g) return;
  for (HWND h : g->seg_edits) { if (h) DestroyWindow(h); }
  for (HWND h : g->seg_promote_btns) { if (h) DestroyWindow(h); }
  g->seg_edits.clear();
  g->seg_promote_btns.clear();
  g->last_segments.clear();
}

bool is_mode_lengths(const AppState* g) {
  return SendMessageW(g->mode_len, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

bool is_mode_separators(const AppState* g) {
  return SendMessageW(g->mode_sep, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void set_mode(AppState* g, int which) {
  SendMessageW(g->mode_auto, BM_SETCHECK, which == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g->mode_sep, BM_SETCHECK, which == 1 ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g->mode_len, BM_SETCHECK, which == 2 ? BST_CHECKED : BST_UNCHECKED, 0);

  ShowWindow(g->sep_label, which == 1 ? SW_SHOW : SW_HIDE);
  ShowWindow(g->sep_edit, which == 1 ? SW_SHOW : SW_HIDE);
  ShowWindow(g->count_label, which == 2 ? SW_SHOW : SW_HIDE);
  ShowWindow(g->seg_count_combo, which == 2 ? SW_SHOW : SW_HIDE);
  for (HWND h : g->length_boxes) {
    if (h) ShowWindow(h, which == 2 ? SW_SHOW : SW_HIDE);
  }
}

std::vector<std::size_t> parse_lengths(const AppState* g) {
  std::vector<std::size_t> out;
  for (HWND h : g->length_boxes) {
    if (!h) continue;
    std::string t = regex_tool::trim(wide_to_utf8(get_edit_text(h)));
    if (t.empty()) continue;
    std::size_t v = 0;
    if (parse_size_t(t, v) && v > 0) out.push_back(v);
  }
  return out;
}

std::vector<std::string> parse_separators(const AppState* g) {
  std::vector<std::string> out;
  std::string s = wide_to_utf8(get_edit_text(g->sep_edit));
  std::size_t pos = 0;
  while (pos <= s.size()) {
    std::size_t comma = s.find(',', pos);
    std::string part = regex_tool::trim(s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos));
    if (!part.empty()) out.push_back(part);
    if (comma == std::string::npos) break;
    pos = comma + 1;
  }
  return out;
}

void rebuild_length_boxes(HWND hwnd) {
  auto g = get_app_state(hwnd);
  if (!g || !g->scroll_container) return;
  std::vector<std::string> old;
  for (HWND h : g->length_boxes) {
    if (h) old.push_back(wide_to_utf8(get_edit_text(h)));
  }
  for (HWND h : g->length_boxes) {
    if (h) DestroyWindow(h);
  }
  g->length_boxes.clear();
  int cur = static_cast<int>(SendMessageW(g->seg_count_combo, CB_GETCURSEL, 0, 0));
  if (cur < 0) cur = 0;
  bool show_len = is_mode_lengths(g);
  for (int i = 0; i <= cur; ++i) {
    HWND h = make_edit(g->scroll_container, IDC_LEN_BASE + i, 0, 0, g->scale_dpi(56), g->scale_dpi(BASE_CTRL_H), 0, L"长度");
    if (i < static_cast<int>(old.size())) SetWindowTextW(h, utf8_to_wide(old[i]).c_str());
    ShowWindow(h, show_len ? SW_SHOW : SW_HIDE);
    g->length_boxes.push_back(h);
  }
  layout_scroll_content(g->scroll_container);
}

void rebuild_segment_table(HWND hwnd) {
  auto g = get_app_state(hwnd);
  if (!g || !g->scroll_container) return;

  for (HWND h : g->seg_edits) { if (h) DestroyWindow(h); }
  for (HWND h : g->seg_promote_btns) { if (h) DestroyWindow(h); }
  g->seg_edits.clear();
  g->seg_promote_btns.clear();

  for (size_t i = 0; i < g->last_segments.size(); ++i) {
    g->seg_promote_btns.push_back(nullptr);
  }
  for (size_t i = 0; i < g->last_segments.size(); ++i) {
    const auto& seg = g->last_segments[i];
    std::string text = std::to_string(i + 1) + ". ";
    DWORD extra = 0;
    if (seg.is_literal) {
      text += "固定: " + seg.literal;
      extra = ES_READONLY;
    } else {
      text += "字段 " + seg.field.name + ": ";
      bool overridden = false;
      for (const auto& ov : g->field_overrides) {
        if (ov.name == seg.field.name) {
          text += field_rule_text(ov);
          overridden = true;
          break;
        }
      }
      if (!overridden) text += field_rule_text(seg.field);
    }
    HWND edit = make_edit(g->scroll_container, IDC_SEG_EDIT_BASE + static_cast<int>(i), 0, 0, g->scale_dpi(320), g->scale_dpi(BASE_CTRL_H), extra);
    SetWindowTextW(edit, utf8_to_wide(text).c_str());

    if (seg.is_literal) {
      auto tags = regex_tool::promotable_tags(seg.literal);
      if (!tags.empty()) {
        bool any_promoted = false;
        for (const auto& t : tags) {
          if (std::find(g->promote_tags.begin(), g->promote_tags.end(), t) != g->promote_tags.end()) {
            any_promoted = true;
            break;
          }
        }
        HWND btn = make_button(g->scroll_container, any_promoted ? L"还原" : L"提升",
                               IDC_SEG_PROMOTE_BASE + static_cast<int>(i),
                               0, 0, g->scale_dpi(48), g->scale_dpi(BASE_CTRL_H));
        g->seg_promote_btns[i] = btn;
      }
    }
    g->seg_edits.push_back(edit);
  }
}

std::string format_candidate_block(const regex_tool::CandidateVerdict& vd, size_t index, bool recommended) {
  std::string tag = vd.candidate.display_name.empty() ? vd.candidate.variant : vd.candidate.display_name;
  std::string out;
  if (recommended) {
    out += "======================================================================\r\n";
    out += "【推荐候选】 " + tag + "\r\n";
  } else {
    out += "----------------------------------------------------------------------\r\n";
    out += "【候选 " + std::to_string(index + 1) + "】 " + tag + "\r\n";
  }
  out += "匹配正则:\r\n  " + vd.candidate.regex + "\r\n";
  if (!vd.candidate.capturing_regex.empty()) {
    out += "捕获正则:\r\n  " + vd.candidate.capturing_regex + "\r\n";
  }
  out += "\r\n";

  if (vd.passes_all) {
    out += "验证结论: 全部通过 ✓ (" + std::to_string(vd.positive_verdicts.size()) + " 条正例匹配，" +
           std::to_string(vd.negative_verdicts.size()) + " 条反例排除)\r\n\r\n";
  } else {
    out += "验证结论: ⚠ 未完全通过 (" + std::to_string(vd.failures) + " 处不符合)\r\n\r\n";
  }

  if (!vd.positive_verdicts.empty()) {
    out += "[正例匹配明细]\r\n";
    for (const auto& v : vd.positive_verdicts) {
      out += v.passed ? "  ✓ " : "  ✗ [失败] ";
      out += v.example;
      if (!v.passed) {
        out += v.failing_segment != SIZE_MAX
                   ? " (卡在分段 " + std::to_string(v.failing_segment + 1) + ")"
                   : " (整串不匹配)";
      }
      out += "\r\n";
    }
    out += "\r\n";
  }

  if (!vd.negative_verdicts.empty()) {
    out += "[反例排除明细]\r\n";
    for (const auto& v : vd.negative_verdicts) {
      out += v.passed ? "  ✓ [已排除] " : "  ✗ [⚠ 漏过] ";
      out += v.example;
      if (!v.passed && v.failing_segment != SIZE_MAX) {
        out += " (卡在分段 " + std::to_string(v.failing_segment + 1) + ")";
      }
      out += "\r\n";
    }
    out += "\r\n";
  }

  return out;
}

void run_generate(HWND hwnd) {
  auto g = get_app_state(hwnd);
  if (!g) return;

  try {
    collect_overrides_from_seg_edits(g);

    regex_tool::Project p;
    p.positives = collect_edits(g->pos_edits);
    p.negatives = collect_edits(g->neg_edits);
    auto& cfg = p.segment_config;
    cfg.use_lengths = is_mode_lengths(g);
    cfg.use_separators = is_mode_separators(g);
    cfg.lengths = parse_lengths(g);
    cfg.separators = parse_separators(g);
    cfg.promote_tags = g->promote_tags;
    cfg.field_overrides = g->field_overrides;

    auto result = regex_tool::generate(p);

    if (result.candidates.empty()) {
      g->recommended_regex.clear();
      g->recommended_capturing_regex.clear();
      SetWindowTextW(g->rec_regex_edit, L"");
      SetWindowTextW(g->result_edit, L"（请在左侧至少输入一条正例条码，然后点击\"生成\"）");
      set_status(hwnd, L"提示: 请至少输入一条正例条码");
    } else {
      g->recommended_regex = result.candidates.front().candidate.regex;
      g->recommended_capturing_regex = result.candidates.front().candidate.capturing_regex;
      SetWindowTextW(g->rec_regex_edit, utf8_to_wide(g->recommended_regex).c_str());

      std::string text;
      if (!result.warnings.empty()) {
        text += "【警告提示】\r\n";
        for (const auto& w : result.warnings) text += "- " + w + "\r\n";
        text += "\r\n";
      }

      if (!result.has_passing) {
        text += "⚠ 注意：没有任何候选通过全部正例/反例。以下候选未完全通过，仅按接近程度排序供定位问题：\r\n\r\n";
      }

      for (size_t i = 0; i < result.candidates.size(); ++i) {
        text += format_candidate_block(result.candidates[i], i, i == 0 && result.has_passing);
      }
      SetWindowTextW(g->result_edit, utf8_to_wide(text).c_str());

      std::wstring st = L"生成完成: 产出 " + std::to_wstring(result.candidates.size()) + L" 条候选";
      if (result.has_passing) st += L"（智能业务推荐候选已就绪，可一键复制）";
      else st += L"（⚠ 未完全通过）";
      set_status(hwnd, st);
    }

    g->has_selftest_candidate = !result.candidates.empty();
    if (g->has_selftest_candidate) g->selftest_candidate = result.candidates.front().candidate;

    g->last_segments = result.candidates.empty()
                          ? std::vector<regex_tool::SegmentRule>{}
                          : result.candidates.front().candidate.segments;
    rebuild_segment_table(hwnd);
    layout_scroll_content(g->scroll_container);
  } catch (const std::exception& e) {
    std::string err_msg = "生成异常: ";
    err_msg += e.what();
    SetWindowTextW(g->rec_regex_edit, L"");
    SetWindowTextW(g->result_edit, utf8_to_wide(err_msg).c_str());
    set_status(hwnd, L"生成失败: 规则表达式异常");
  }
}

void run_selftest(HWND hwnd) {
  auto g = get_app_state(hwnd);
  if (!g) return;
  if (!g->has_selftest_candidate) {
    SetWindowTextW(g->selftest_label, L"自测: 请先生成候选正则");
    return;
  }
  std::string input = wide_to_utf8(get_edit_text(g->selftest_edit));
  if (input.empty()) {
    SetWindowTextW(g->selftest_label, L"自测: 请输入待判定条码");
    return;
  }
  auto v = regex_tool::verify_example(g->selftest_candidate, input);
  std::wstring msg = v.passed ? L"自测: 通过 ✓" : L"自测: 不通过 ✗";
  if (!v.passed) {
    if (v.failing_segment != SIZE_MAX) {
      msg += L"（卡在分段 " + std::to_wstring(v.failing_segment + 1) + L"）";
    } else {
      msg += L"（整串不匹配）";
    }
  } else if (!v.field_captures.empty()) {
    msg += L" [捕获:";
    for (size_t i = 0; i < v.field_captures.size(); ++i) {
      if (i > 0) msg += L", ";
      msg += utf8_to_wide(v.field_captures[i].first) + L"=" + utf8_to_wide(v.field_captures[i].second);
    }
    msg += L"]";
  }
  SetWindowTextW(g->selftest_label, msg.c_str());
}

std::wstring exe_dir() {
  wchar_t buf[MAX_PATH];
  GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring p = buf;
  std::size_t pos = p.find_last_of(L"\\/");
  return pos == std::wstring::npos ? L"." : p.substr(0, pos);
}

std::wstring project_name(const AppState* g) {
  std::wstring n = get_edit_text(g->project_combo);
  if (n.empty()) n = L"project-1";
  return n;
}

void populate_projects(AppState* g) {
  if (!g || !g->project_combo) return;
  SendMessageW(g->project_combo, CB_RESETCONTENT, 0, 0);

  std::vector<std::wstring> dirs = { exe_dir() };
  wchar_t cwd_buf[MAX_PATH];
  if (GetCurrentDirectoryW(MAX_PATH, cwd_buf)) {
    if (_wcsicmp(dirs[0].c_str(), cwd_buf) != 0) {
      dirs.push_back(cwd_buf);
    }
  }

  std::vector<std::wstring> added;
  for (const auto& dir : dirs) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*.rgx").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) continue;
    do {
      if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        std::wstring name = fd.cFileName;
        std::size_t dot = name.find_last_of(L'.');
        if (dot != std::wstring::npos) name = name.substr(0, dot);
        if (std::find(added.begin(), added.end(), name) == added.end()) {
          added.push_back(name);
          SendMessageW(g->project_combo, CB_ADDSTRING, 0,
                       reinterpret_cast<LPARAM>(name.c_str()));
        }
      }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
  }
}

std::string serialize_project(const AppState* g) {
  std::string out;
  out += "name=" + wide_to_utf8(project_name(g)) + "\n";
  out += std::string("mode=") + (is_mode_separators(g) ? "separators"
                                : is_mode_lengths(g) ? "lengths" : "auto") + "\n";
  std::string lengths_text;
  for (size_t i = 0; i < g->length_boxes.size(); ++i) {
    if (!g->length_boxes[i]) continue;
    if (!lengths_text.empty()) lengths_text += ",";
    lengths_text += wide_to_utf8(get_edit_text(g->length_boxes[i]));
  }
  out += "lengths=" + lengths_text + "\n";
  out += "separators=" + wide_to_utf8(get_edit_text(g->sep_edit)) + "\n";
  if (!g->promote_tags.empty()) {
    out += "promote=";
    for (size_t i = 0; i < g->promote_tags.size(); ++i) {
      if (i) out += ",";
      out += g->promote_tags[i];
    }
    out += "\n";
  }
  for (const auto& ov : g->field_overrides) {
    out += "override=" + ov.name + ":" + ov.charset + "," + std::to_string(ov.min_len) + "," +
           std::to_string(ov.max_len == SIZE_MAX ? 0 : ov.max_len) + "\n";
  }
  for (const auto& s : collect_edits(g->pos_edits)) out += "positive=" + s + "\n";
  for (const auto& s : collect_edits(g->neg_edits)) out += "negative=" + s + "\n";
  if (g->has_selftest_candidate) out += "selected=" + g->selftest_candidate.regex + "\n";
  return out;
}

bool save_project(HWND hwnd) {
  auto g = get_app_state(hwnd);
  if (!g) return false;
  std::wstring name = project_name(g);
  std::wstring path = exe_dir() + L"\\" + name + L".rgx";
  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  f << serialize_project(g);
  if (f.good()) {
    populate_projects(g);
    SetWindowTextW(g->project_combo, name.c_str());
    set_status(hwnd, L"已保存项目: " + name + L".rgx");
    return true;
  }
  return false;
}

bool load_project(HWND hwnd, const std::wstring& name, std::wstring& error) {
  auto g = get_app_state(hwnd);
  if (!g) return false;
  std::wstring path = exe_dir() + L"\\" + name + L".rgx";
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    path = name + L".rgx";
    f.open(path, std::ios::binary);
  }
  if (!f) {
    error = L"无法打开项目文件: " + name + L".rgx";
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
  std::size_t line_no = 0;
  while (pos < content.size()) {
    ++line_no;
    std::size_t nl = content.find('\n', pos);
    std::string line = regex_tool::trim(content.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos));
    pos = (nl == std::string::npos) ? content.size() : nl + 1;
    if (line.empty() || line[0] == '#') continue;
    std::size_t eq = line.find('=');
    if (eq == std::string::npos) {
      error = L"第 " + std::to_wstring(line_no) + L" 行格式无法识别（缺少等号）";
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
        error = L"第 " + std::to_wstring(line_no) + L" 行规则覆盖格式错误";
        return false;
      }
      regex_tool::FieldRule ov;
      ov.name = value.substr(0, colon);
      std::string rest = value.substr(colon + 1);
      std::size_t c1 = rest.find(',');
      std::size_t c2 = rest.find(',', c1 + 1);
      if (c1 == std::string::npos || c2 == std::string::npos) {
        error = L"第 " + std::to_wstring(line_no) + L" 行规则覆盖格式错误";
        return false;
      }
      ov.charset = rest.substr(0, c1);
      if (!parse_size_t(rest.substr(c1 + 1, c2 - c1 - 1), ov.min_len) ||
          !parse_size_t(rest.substr(c2 + 1), ov.max_len)) {
        error = L"第 " + std::to_wstring(line_no) + L" 行规则长度解析失败";
        return false;
      }
      if (ov.max_len == 0) ov.max_len = SIZE_MAX;
      overrides.push_back(ov);
    } else if (key == "selected") selected = value;
    else {
      error = L"第 " + std::to_wstring(line_no) + L" 行包含未知字段: " + utf8_to_wide(key);
      return false;
    }
  }
  if (!saw_name) {
    error = L"项目文件格式错误（缺少 name 字段）";
    return false;
  }

  SetWindowTextW(g->project_combo, name.c_str());
  set_mode(g, use_separators ? 1 : use_lengths ? 2 : 0);
  SetWindowTextW(g->sep_edit, separators_text.c_str());
  {
    std::vector<std::string> parts;
    std::wstring cur;
    for (wchar_t c : lengths_text) {
      if (c == L',') {
        std::string s = regex_tool::trim(wide_to_utf8(cur));
        if (!s.empty()) parts.push_back(s);
        cur.clear();
      } else {
        cur += c;
      }
    }
    std::string s = regex_tool::trim(wide_to_utf8(cur));
    if (!s.empty()) parts.push_back(s);

    int count = static_cast<int>(parts.size());
    if (count > 10) count = 10;
    if (count < 1) count = 1;
    SendMessageW(g->seg_count_combo, CB_SETCURSEL, count - 1, 0);
    rebuild_length_boxes(hwnd);
    for (size_t i = 0; i < parts.size() && i < g->length_boxes.size(); ++i) {
      SetWindowTextW(g->length_boxes[i], utf8_to_wide(parts[i]).c_str());
    }
  }
  g->promote_tags = promote;
  g->field_overrides = overrides;
  g->has_selftest_candidate = !selected.empty();
  if (!selected.empty()) {
    g->selftest_candidate.regex = selected;
  }
  clear_example_rows(g, false);
  clear_example_rows(g, true);
  if (positives.empty()) {
    add_example_row(hwnd, false);
  } else {
    for (const auto& p_str : positives) {
      add_example_row(hwnd, false);
      SetWindowTextW(g->pos_edits.back(), utf8_to_wide(p_str).c_str());
    }
  }
  for (const auto& n_str : negatives) {
    add_example_row(hwnd, true);
    SetWindowTextW(g->neg_edits.back(), utf8_to_wide(n_str).c_str());
  }
  destroy_segment_table(g);
  run_generate(hwnd);
  set_status(hwnd, L"已成功加载项目: " + name);
  return true;
}

int content_height(const AppState* g) {
  int row_h = g->scale_dpi(BASE_ROW_H);
  int y = 0;
  y += g->scale_dpi(22);                                                // positives label
  y += static_cast<int>(g->pos_edits.size()) * row_h;
  y += row_h + g->scale_dpi(8);                                         // +正例 / 批量粘贴
  y += g->scale_dpi(22);                                                // negatives label
  y += static_cast<int>(g->neg_edits.size()) * row_h;
  y += row_h + g->scale_dpi(8);                                         // +反例 / 批量粘贴
  y += g->scale_dpi(22);                                                // 分段配置 label
  y += row_h;                                                           // mode radios

  if (is_mode_separators(g)) {
    y += row_h;                                                         // separators input
  } else if (is_mode_lengths(g)) {
    y += row_h;                                                         // count combo
    y += static_cast<int>((g->length_boxes.size() + LEN_PER_LINE - 1) / LEN_PER_LINE) * row_h;
  }

  y += static_cast<int>(g->seg_edits.size()) * row_h;                  // segment rule table
  return y + g->scale_dpi(20);
}

void layout_scroll_content(HWND scroll_hwnd) {
  auto g = reinterpret_cast<AppState*>(GetWindowLongPtrW(g_main_hwnd, GWLP_USERDATA));
  if (!g || !scroll_hwnd) return;

  RECT rc;
  GetClientRect(scroll_hwnd, &rc);
  int view_w = rc.right;
  int view_h = rc.bottom;

  int total_content = content_height(g);
  int max_scroll = std::max(total_content - view_h, 0);
  if (g->scroll_pos > max_scroll) g->scroll_pos = max_scroll;

  SCROLLINFO si{};
  si.cbSize = sizeof(si);
  si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
  si.nMin = 0;
  si.nMax = total_content;
  si.nPage = static_cast<UINT>(view_h);
  si.nPos = g->scroll_pos;
  SetScrollInfo(scroll_hwnd, SB_VERT, &si, TRUE);

  int ctrl_h = g->scale_dpi(BASE_CTRL_H);
  int row_h = g->scale_dpi(BASE_ROW_H);
  int del_w = g->scale_dpi(BASE_DEL_W);
  int edit_w = view_w - del_w - g->scale_dpi(10);

  int y = 0 - g->scroll_pos;

  MoveWindow(g->pos_label, 0, y, g->scale_dpi(180), g->scale_dpi(18), TRUE);
  y += g->scale_dpi(22);
  for (size_t i = 0; i < g->pos_edits.size(); ++i) {
    MoveWindow(g->pos_edits[i], 0, y, edit_w, ctrl_h, TRUE);
    MoveWindow(g->pos_dels[i], edit_w + g->scale_dpi(4), y, del_w, ctrl_h, TRUE);
    y += row_h;
  }
  MoveWindow(g->add_pos, 0, y, g->scale_dpi(88), ctrl_h, TRUE);
  MoveWindow(g->batch_pos, g->scale_dpi(94), y, g->scale_dpi(96), ctrl_h, TRUE);
  y += row_h + g->scale_dpi(8);

  MoveWindow(g->neg_label, 0, y, g->scale_dpi(180), g->scale_dpi(18), TRUE);
  y += g->scale_dpi(22);
  for (size_t i = 0; i < g->neg_edits.size(); ++i) {
    MoveWindow(g->neg_edits[i], 0, y, edit_w, ctrl_h, TRUE);
    MoveWindow(g->neg_dels[i], edit_w + g->scale_dpi(4), y, del_w, ctrl_h, TRUE);
    y += row_h;
  }
  MoveWindow(g->add_neg, 0, y, g->scale_dpi(88), ctrl_h, TRUE);
  MoveWindow(g->batch_neg, g->scale_dpi(94), y, g->scale_dpi(96), ctrl_h, TRUE);
  y += row_h + g->scale_dpi(8);

  MoveWindow(g->cfg_label, 0, y, g->scale_dpi(140), g->scale_dpi(18), TRUE);
  y += g->scale_dpi(22);
  int radio_w = g->scale_dpi(84);
  MoveWindow(g->mode_auto, 0, y, radio_w, ctrl_h, TRUE);
  MoveWindow(g->mode_sep, radio_w + g->scale_dpi(4), y, radio_w + g->scale_dpi(8), ctrl_h, TRUE);
  MoveWindow(g->mode_len, (radio_w * 2) + g->scale_dpi(16), y, radio_w, ctrl_h, TRUE);
  y += row_h;

  if (is_mode_separators(g)) {
    MoveWindow(g->sep_label, 0, y, g->scale_dpi(64), ctrl_h, TRUE);
    MoveWindow(g->sep_edit, g->scale_dpi(68), y, g->scale_dpi(130), ctrl_h, TRUE);
    y += row_h;
  } else if (is_mode_lengths(g)) {
    MoveWindow(g->count_label, 0, y, g->scale_dpi(46), ctrl_h, TRUE);
    MoveWindow(g->seg_count_combo, g->scale_dpi(50), y, g->scale_dpi(66), ctrl_h, TRUE);
    y += row_h;
    int len_box_w = g->scale_dpi(54);
    int len_gap = g->scale_dpi(6);
    for (size_t i = 0; i < g->length_boxes.size(); ++i) {
      MoveWindow(g->length_boxes[i],
                 static_cast<int>(i % LEN_PER_LINE) * (len_box_w + len_gap),
                 y + static_cast<int>(i / LEN_PER_LINE) * row_h, len_box_w, ctrl_h, TRUE);
    }
    y += static_cast<int>((g->length_boxes.size() + LEN_PER_LINE - 1) / LEN_PER_LINE) * row_h;
  }

  for (size_t i = 0; i < g->seg_edits.size(); ++i) {
    MoveWindow(g->seg_edits[i], 0, y, edit_w - g->scale_dpi(54), ctrl_h, TRUE);
    if (i < g->seg_promote_btns.size() && g->seg_promote_btns[i]) {
      MoveWindow(g->seg_promote_btns[i], edit_w - g->scale_dpi(50), y, g->scale_dpi(48), ctrl_h, TRUE);
    }
    y += row_h;
  }
}

void layout(HWND hwnd) {
  auto g = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (!g) return;

  RECT rc;
  GetClientRect(hwnd, &rc);
  int win_w = std::max(static_cast<int>(rc.right), 100);
  int win_h = std::max(static_cast<int>(rc.bottom), 100);

  int status_h = g->scale_dpi(24);
  SendMessageW(g->status_bar, WM_SIZE, 0, 0);

  int left_margin = g->scale_dpi(BASE_LEFT_MARGIN);
  int ctrl_h = g->scale_dpi(BASE_CTRL_H);

  // Responsive column division (~44% left, ~56% right) (P2-7)
  int left_pane_w = std::clamp(static_cast<int>(win_w * 0.44), g->scale_dpi(380), win_w - g->scale_dpi(380));
  int right_x = left_pane_w + g->scale_dpi(24);
  int right_pane_w = std::max(win_w - right_x - left_margin, g->scale_dpi(300));

  // 1. Left Pane: Project header bar (Fixed at top)
  int y_top = g->scale_dpi(12);
  int combo_w = std::max(left_pane_w - g->scale_dpi(196), g->scale_dpi(130));
  MoveWindow(g->project_combo, left_margin, y_top, combo_w, ctrl_h, TRUE);
  int btn_w = g->scale_dpi(54);
  int btn_x = left_margin + combo_w + g->scale_dpi(6);
  MoveWindow(g->new_btn, btn_x, y_top, btn_w, ctrl_h, TRUE);
  btn_x += btn_w + g->scale_dpi(4);
  MoveWindow(g->save_btn, btn_x, y_top, btn_w, ctrl_h, TRUE);
  btn_x += btn_w + g->scale_dpi(4);
  MoveWindow(g->load_btn, btn_x, y_top, btn_w, ctrl_h, TRUE);

  // 2. Left Pane: Action Button (Fixed at bottom)
  int gen_btn_h = g->scale_dpi(38);
  int left_bottom_y = win_h - status_h - gen_btn_h - g->scale_dpi(12);
  MoveWindow(g->generate_btn, left_margin, left_bottom_y, left_pane_w, gen_btn_h, TRUE);

  // 3. Left Pane: Isolated Scroll Container Viewport (WS_CLIPCHILDREN)
  int scroll_y = y_top + ctrl_h + g->scale_dpi(10);
  int scroll_h = std::max(left_bottom_y - scroll_y - g->scale_dpi(10), 60);
  MoveWindow(g->scroll_container, left_margin, scroll_y, left_pane_w, scroll_h, TRUE);
  layout_scroll_content(g->scroll_container);

  // 4. Right Pane: Structured Recommended Regex + Verification Details + Self-test
  int r_top = g->scale_dpi(12);

  // 4.1 Recommended Regex Section Header & Copy Buttons
  int btn_w1 = g->scale_dpi(104);
  int btn_w2 = g->scale_dpi(104);
  int total_btn_w = btn_w1 + btn_w2 + g->scale_dpi(6);
  MoveWindow(g->rec_header_label, right_x, r_top, std::max(right_pane_w - total_btn_w - g->scale_dpi(10), g->scale_dpi(100)), g->scale_dpi(22), TRUE);
  MoveWindow(g->copy_btn, right_x + right_pane_w - total_btn_w, r_top, btn_w1, ctrl_h, TRUE);
  MoveWindow(g->copy_cap_btn, right_x + right_pane_w - btn_w2, r_top, btn_w2, ctrl_h, TRUE);

  // 4.2 Recommended Regex Highlight Box
  int rec_box_y = r_top + ctrl_h + g->scale_dpi(6);
  int rec_box_h = g->scale_dpi(32);
  MoveWindow(g->rec_regex_edit, right_x, rec_box_y, right_pane_w, rec_box_h, TRUE);

  // 4.3 Verification Verdicts & Details Section Header
  int detail_hdr_y = rec_box_y + rec_box_h + g->scale_dpi(12);
  MoveWindow(g->result_header_label, right_x, detail_hdr_y, right_pane_w, g->scale_dpi(22), TRUE);

  // 4.4 Verification Details Edit Box
  int selftest_area_h = g->scale_dpi(72);
  int detail_box_y = detail_hdr_y + g->scale_dpi(22);
  int detail_box_h = std::max(win_h - status_h - detail_box_y - selftest_area_h - g->scale_dpi(14), g->scale_dpi(90));
  MoveWindow(g->result_edit, right_x, detail_box_y, right_pane_w, detail_box_h, TRUE);

  // 4.5 Self-test Section (Bottom of right pane)
  int selftest_y = detail_box_y + detail_box_h + g->scale_dpi(10);
  MoveWindow(g->selftest_title, right_x, selftest_y, right_pane_w, g->scale_dpi(18), TRUE);
  int test_btn_w = g->scale_dpi(76);
  MoveWindow(g->selftest_edit, right_x, selftest_y + g->scale_dpi(22), right_pane_w - test_btn_w - g->scale_dpi(8), ctrl_h, TRUE);
  MoveWindow(g->selftest_btn, right_x + right_pane_w - test_btn_w, selftest_y + g->scale_dpi(22), test_btn_w, ctrl_h, TRUE);
  MoveWindow(g->selftest_label, right_x, selftest_y + g->scale_dpi(48), right_pane_w, g->scale_dpi(18), TRUE);
}

// Window Procedure for the isolated scroll container viewport
LRESULT CALLBACK scroll_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  auto g = reinterpret_cast<AppState*>(GetWindowLongPtrW(g_main_hwnd, GWLP_USERDATA));

  switch (msg) {
    case WM_VSCROLL: {
      if (!g) return 0;
      SCROLLINFO si{};
      si.cbSize = sizeof(si);
      si.fMask = SIF_ALL;
      GetScrollInfo(hwnd, SB_VERT, &si);
      int pos = si.nPos;
      int step = g->scale_dpi(BASE_ROW_H);
      switch (LOWORD(wParam)) {
        case SB_LINEUP: pos -= step; break;
        case SB_LINEDOWN: pos += step; break;
        case SB_PAGEUP: pos -= static_cast<int>(si.nPage); break;
        case SB_PAGEDOWN: pos += static_cast<int>(si.nPage); break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: pos = si.nTrackPos; break;
        default: return 0;
      }
      int max_pos = std::max(si.nMax - static_cast<int>(si.nPage), 0);
      pos = std::clamp(pos, 0, max_pos);
      if (pos != g->scroll_pos) {
        g->scroll_pos = pos;
        si.fMask = SIF_POS;
        si.nPos = pos;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        layout_scroll_content(hwnd);
      }
      return 0;
    }

    case WM_MOUSEWHEEL: {
      if (!g) return 0;
      short delta = GET_WHEEL_DELTA_WPARAM(wParam);
      int steps = delta / WHEEL_DELTA;
      int scroll_amount = steps * g->scale_dpi(BASE_ROW_H) * 3;
      int pos = g->scroll_pos - scroll_amount;
      SCROLLINFO si{};
      si.cbSize = sizeof(si);
      si.fMask = SIF_ALL;
      GetScrollInfo(hwnd, SB_VERT, &si);
      int max_pos = std::max(si.nMax - static_cast<int>(si.nPage), 0);
      pos = std::clamp(pos, 0, max_pos);
      if (pos != g->scroll_pos) {
        g->scroll_pos = pos;
        si.fMask = SIF_POS;
        si.nPos = pos;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        layout_scroll_content(hwnd);
      }
      return 0;
    }

    case WM_CTLCOLORSTATIC: {
      HDC hdc = reinterpret_cast<HDC>(wParam);
      SetTextColor(hdc, RGB(32, 32, 32));
      SetBkMode(hdc, TRANSPARENT);
      return g && g->bg_brush ? reinterpret_cast<LRESULT>(g->bg_brush)
                              : reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    }

    case WM_CTLCOLOREDIT: {
      HDC hdc = reinterpret_cast<HDC>(wParam);
      HWND edit_h = reinterpret_cast<HWND>(lParam);
      LONG style = GetWindowLongW(edit_h, GWL_STYLE);
      if (style & ES_READONLY) {
        SetTextColor(hdc, RGB(80, 80, 80));
        SetBkColor(hdc, RGB(236, 236, 236));
        return g && g->readonly_bg_brush ? reinterpret_cast<LRESULT>(g->readonly_bg_brush)
                                         : reinterpret_cast<LRESULT>(GetStockObject(LTGRAY_BRUSH));
      }
      SetTextColor(hdc, RGB(32, 32, 32));
      SetBkColor(hdc, RGB(255, 255, 255));
      return g && g->edit_bg_brush ? reinterpret_cast<LRESULT>(g->edit_bg_brush)
                                   : reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }

    case WM_CTLCOLORBTN: {
      return g && g->bg_brush ? reinterpret_cast<LRESULT>(g->bg_brush)
                              : reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    }

    case WM_COMMAND: {
      // Forward child commands to parent main window
      return SendMessageW(g_main_hwnd, WM_COMMAND, wParam, lParam);
    }
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  auto g = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  switch (msg) {
    case WM_CREATE: {
      auto new_state = new AppState();
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(new_state));
      g = new_state;
      g->init_resources(hwnd);

      update_theme_titlebar(hwnd);

      // Status Bar
      g->status_bar = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
                                      WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                      0, 0, 0, 0, hwnd,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUSBAR)),
                                      GetModuleHandleW(nullptr), nullptr);
      SendMessageW(g->status_bar, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
      set_status(hwnd, L"就绪");

      // 1. Top Fixed Project Header
      g->project_combo = make_combo(hwnd, IDC_PROJECT_EDIT, 0, 0, 0, 0, true, L"项目名称");
      g->new_btn = make_button(hwnd, L"新建", IDC_NEW, 0, 0, 0, 0);
      g->save_btn = make_button(hwnd, L"保存", IDC_SAVE, 0, 0, 0, 0);
      g->load_btn = make_button(hwnd, L"加载", IDC_LOAD, 0, 0, 0, 0);

      // 2. Bottom Fixed Generate Action Button
      g->generate_btn = make_button(hwnd, L"生成候选正则", IDC_GENERATE, 0, 0, 0, 0, BS_OWNERDRAW);

      // 3. Isolated Scroll Viewport (Child Container with Hardware Clipping)
      g->scroll_container = CreateWindowExW(0, L"RegexToolScrollArea", nullptr,
                                            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VSCROLL,
                                            0, 0, 0, 0, hwnd,
                                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SCROLL_CONTAINER)),
                                            GetModuleHandleW(nullptr), nullptr);

      // 4. Children inside the Scroll Viewport
      HWND sc = g->scroll_container;
      g->pos_label = make_static(sc, L"正例（必须匹配）", 0, 0, 0, 0, true);
      g->add_pos = make_button(sc, L"+ 正例", IDC_ADD_POS, 0, 0, 0, 0);
      g->batch_pos = make_button(sc, L"批量粘贴", IDC_BATCH_POS, 0, 0, 0, 0);

      g->neg_label = make_static(sc, L"反例（必须排除）", 0, 0, 0, 0, true);
      g->add_neg = make_button(sc, L"+ 反例", IDC_ADD_NEG, 0, 0, 0, 0);
      g->batch_neg = make_button(sc, L"批量粘贴", IDC_BATCH_NEG, 0, 0, 0, 0);

      g->cfg_label = make_static(sc, L"分段配置", 0, 0, 0, 0, true);
      g->mode_auto = make_radio(sc, L"自动分段", IDC_MODE_AUTO, 0, 0, 0, 0, true);
      g->mode_sep = make_radio(sc, L"按分隔符", IDC_MODE_SEP, 0, 0, 0, 0, false);
      g->mode_len = make_radio(sc, L"按长度", IDC_MODE_LEN, 0, 0, 0, 0, false);

      g->sep_label = make_static(sc, L"分隔符:", 0, 0, 0, 0);
      g->sep_edit = make_edit(sc, IDC_SEP_EDIT, 0, 0, 0, 0, 0, L"如 @ 或 @,|");
      g->count_label = make_static(sc, L"段数:", 0, 0, 0, 0);
      g->seg_count_combo = make_combo(sc, IDC_SEG_COUNT_COMBO, 0, 0, 0, 0, false);
      for (int i = 1; i <= 10; ++i) {
        std::wstring t = std::to_wstring(i);
        SendMessageW(g->seg_count_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t.c_str()));
      }
      SendMessageW(g->seg_count_combo, CB_SETCURSEL, 1, 0);

      set_mode(g, 0);

      // 5. Right Pane Controls (Dedicated Recommended Regex Box + Details Box + Self-test)
      g->rec_header_label = make_static(hwnd, L"推荐候选正则:", 0, 0, 0, 0, true);
      g->copy_btn = make_button(hwnd, L"复制推荐正则", IDC_COPY_REGEX, 0, 0, 0, 0);
      g->copy_cap_btn = make_button(hwnd, L"复制捕获正则", IDC_COPY_CAP_REGEX, 0, 0, 0, 0);
      g->rec_regex_edit = make_edit(hwnd, IDC_REC_REGEX_EDIT, 0, 0, 0, 0, ES_READONLY, L"（生成后此处将显示高亮推荐正则）");
      SendMessageW(g->rec_regex_edit, WM_SETFONT, reinterpret_cast<WPARAM>(g->mono_bold_font), TRUE);

      g->result_header_label = make_static(hwnd, L"验证详情与备选候选:", 0, 0, 0, 0, true);
      g->result_edit = make_edit(hwnd, IDC_RESULT_EDIT, 0, 0, 0, 0,
                                 ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL);
      SendMessageW(g->result_edit, WM_SETFONT, reinterpret_cast<WPARAM>(g->mono_font), TRUE);
      SetWindowTextW(g->result_edit, L"请在左侧输入正例条码（每行一条），然后点击\"生成候选正则\"。\r\n提示：可使用\"批量粘贴\"一次输入多条。");

      g->selftest_title = make_static(hwnd, L"自测条码验证:", 0, 0, 0, 0, true);
      g->selftest_edit = make_edit(hwnd, IDC_SELFTEST_EDIT, 0, 0, 0, 0, 0, L"输入待判定条码进行自测...");
      SendMessageW(g->selftest_edit, WM_SETFONT, reinterpret_cast<WPARAM>(g->mono_font), TRUE);
      g->selftest_btn = make_button(hwnd, L"判定", IDC_SELFTEST_BTN, 0, 0, 0, 0);
      g->selftest_label = make_static(hwnd, L"自测结果: 待输入", 0, 0, 0, 0);

      populate_projects(g);
      rebuild_length_boxes(hwnd);
      add_example_row(hwnd, false);
      return 0;
    }

    case WM_SIZE:
      layout(hwnd);
      return 0;

    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
      update_theme_titlebar(hwnd);
      InvalidateRect(hwnd, nullptr, TRUE);
      if (g && g->scroll_container) InvalidateRect(g->scroll_container, nullptr, TRUE);
      return 0;

    case WM_DPICHANGED: {
      if (g) {
        g->dpi = HIWORD(wParam);
        g->update_fonts();

        // Broadcast font update to all children
        HWND child = FindWindowExW(hwnd, nullptr, nullptr, nullptr);
        while (child) {
          if (child == g->rec_regex_edit) {
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(g->mono_bold_font), TRUE);
          } else if (child == g->result_edit || child == g->selftest_edit) {
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(g->mono_font), TRUE);
          } else {
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
          }
          child = FindWindowExW(hwnd, child, nullptr, nullptr);
        }

        if (g->rec_header_label) SendMessageW(g->rec_header_label, WM_SETFONT, reinterpret_cast<WPARAM>(g->header_font), TRUE);
        if (g->result_header_label) SendMessageW(g->result_header_label, WM_SETFONT, reinterpret_cast<WPARAM>(g->header_font), TRUE);
        if (g->selftest_title) SendMessageW(g->selftest_title, WM_SETFONT, reinterpret_cast<WPARAM>(g->header_font), TRUE);

        if (g->scroll_container) {
          HWND sc_child = FindWindowExW(g->scroll_container, nullptr, nullptr, nullptr);
          while (sc_child) {
            SendMessageW(sc_child, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
            sc_child = FindWindowExW(g->scroll_container, sc_child, nullptr, nullptr);
          }
          if (g->pos_label) SendMessageW(g->pos_label, WM_SETFONT, reinterpret_cast<WPARAM>(g->header_font), TRUE);
          if (g->neg_label) SendMessageW(g->neg_label, WM_SETFONT, reinterpret_cast<WPARAM>(g->header_font), TRUE);
          if (g->cfg_label) SendMessageW(g->cfg_label, WM_SETFONT, reinterpret_cast<WPARAM>(g->header_font), TRUE);
        }

        auto prc = reinterpret_cast<RECT*>(lParam);
        if (prc) {
          SetWindowPos(hwnd, nullptr, prc->left, prc->top,
                       prc->right - prc->left, prc->bottom - prc->top,
                       SWP_NOZORDER | SWP_NOACTIVATE);
        }
        layout(hwnd);
      }
      return 0;
    }

    case WM_GETMINMAXINFO: {
      auto mmi = reinterpret_cast<MINMAXINFO*>(lParam);
      int dpi = g ? g->dpi : 96;
      mmi->ptMinTrackSize.x = MulDiv(880, static_cast<int>(dpi), 96);
      mmi->ptMinTrackSize.y = MulDiv(520, static_cast<int>(dpi), 96);
      return 0;
    }

    case WM_CTLCOLORSTATIC: {
      HDC hdc = reinterpret_cast<HDC>(wParam);
      SetTextColor(hdc, RGB(32, 32, 32));
      SetBkMode(hdc, TRANSPARENT);
      return g && g->bg_brush ? reinterpret_cast<LRESULT>(g->bg_brush)
                              : reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    }

    case WM_CTLCOLOREDIT: {
      HDC hdc = reinterpret_cast<HDC>(wParam);
      HWND edit_h = reinterpret_cast<HWND>(lParam);
      if (g && edit_h == g->rec_regex_edit) {
        SetTextColor(hdc, RGB(0, 79, 148)); // Deep Accent Blue
        SetBkColor(hdc, RGB(248, 250, 252));
        return reinterpret_cast<LRESULT>(g->rec_bg_brush);
      }
      LONG style = GetWindowLongW(edit_h, GWL_STYLE);
      if (style & ES_READONLY) {
        SetTextColor(hdc, RGB(40, 40, 40));
        SetBkColor(hdc, RGB(255, 255, 255));
        return g && g->edit_bg_brush ? reinterpret_cast<LRESULT>(g->edit_bg_brush)
                                     : reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
      }
      SetTextColor(hdc, RGB(32, 32, 32));
      SetBkColor(hdc, RGB(255, 255, 255));
      return g && g->edit_bg_brush ? reinterpret_cast<LRESULT>(g->edit_bg_brush)
                                   : reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }

    case WM_CTLCOLORBTN: {
      return g && g->bg_brush ? reinterpret_cast<LRESULT>(g->bg_brush)
                              : reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    }

    case WM_DRAWITEM: {
      auto dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
      if (dis && dis->CtlID == IDC_GENERATE) {
        HDC hdc = dis->hDC;
        RECT rc = dis->rcItem;
        bool is_pressed = (dis->itemState & ODS_SELECTED) != 0;
        bool is_disabled = (dis->itemState & ODS_DISABLED) != 0;

        COLORREF bg_color = is_disabled ? RGB(204, 204, 204)
                           : is_pressed ? RGB(0, 90, 158)
                                        : RGB(0, 103, 192);
        HBRUSH brush = CreateSolidBrush(bg_color);
        HPEN pen = CreatePen(PS_SOLID, 1, bg_color);
        HGDIOBJ old_brush = SelectObject(hdc, brush);
        HGDIOBJ old_pen = SelectObject(hdc, pen);

        int radius = g ? g->scale_dpi(6) : 6;
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);

        SelectObject(hdc, old_brush);
        SelectObject(hdc, old_pen);
        DeleteObject(brush);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        HGDIOBJ old_font = SelectObject(hdc, g && g->bold_font ? g->bold_font : g->font);

        wchar_t text[64];
        GetWindowTextW(dis->hwndItem, text, 64);
        DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, old_font);
        return TRUE;
      }
      break;
    }

    case WM_COMMAND: {
      int id = LOWORD(wParam);
      int code = HIWORD(wParam);
      if (!g) return 0;

      if (id == IDC_SELFTEST_EDIT && code == EN_CHANGE) {
        run_selftest(hwnd);
        return 0;
      }

      if (id == IDC_PROJECT_EDIT && code == CBN_SELCHANGE) {
        int sel = static_cast<int>(SendMessageW(g->project_combo, CB_GETCURSEL, 0, 0));
        if (sel != CB_ERR) {
          int len = static_cast<int>(SendMessageW(g->project_combo, CB_GETLBTEXTLEN, sel, 0));
          if (len > 0) {
            std::wstring text(static_cast<size_t>(len + 1), L'\0');
            LRESULT res = SendMessageW(g->project_combo, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(text.data()));
            if (res != CB_ERR) {
              text.resize(static_cast<size_t>(len));
              std::wstring err;
              if (!load_project(hwnd, text, err)) {
                MessageBoxW(hwnd, err.c_str(), L"加载项目", MB_ICONERROR);
                set_status(hwnd, L"加载失败: " + err);
              }
            }
          }
        }
        return 0;
      }

      if (id == IDC_SEG_COUNT_COMBO && code == CBN_SELCHANGE) {
        rebuild_length_boxes(hwnd);
        return 0;
      }

      if (code != BN_CLICKED) return 0;

      if (id == IDC_NEW) {
        g->promote_tags.clear();
        g->field_overrides.clear();
        g->has_selftest_candidate = false;
        g->recommended_regex.clear();
        SetWindowTextW(g->project_combo, L"");
        SetWindowTextW(g->sep_edit, L"");
        SetWindowTextW(g->rec_regex_edit, L"");
        SetWindowTextW(g->result_edit, L"请在左侧输入正例条码（每行一条），然后点击\"生成候选正则\"。\r\n提示：可使用\"批量粘贴\"一次输入多条。");
        SetWindowTextW(g->selftest_label, L"自测结果: 待输入");
        SetWindowTextW(g->selftest_edit, L"");
        set_mode(g, 0);
        SendMessageW(g->seg_count_combo, CB_SETCURSEL, 1, 0);
        rebuild_length_boxes(hwnd);
        clear_example_rows(g, false);
        clear_example_rows(g, true);
        destroy_segment_table(g);
        add_example_row(hwnd, false);
        layout_scroll_content(g->scroll_container);
        set_status(hwnd, L"已新建空白项目");
        return 0;
      }
      if (id == IDC_SAVE) {
        if (!save_project(hwnd)) {
          MessageBoxW(hwnd, L"保存失败（无法写入文件）", L"保存项目", MB_ICONERROR);
        }
        return 0;
      }
      if (id == IDC_LOAD) {
        std::wstring error;
        if (!load_project(hwnd, project_name(g), error)) {
          MessageBoxW(hwnd, error.c_str(), L"加载项目", MB_ICONERROR);
        }
        return 0;
      }
      if (id == IDC_ADD_POS) { add_example_row(hwnd, false); return 0; }
      if (id == IDC_ADD_NEG) { add_example_row(hwnd, true); return 0; }
      if (id == IDC_BATCH_POS) { run_batch_dialog(hwnd, false); return 0; }
      if (id == IDC_BATCH_NEG) { run_batch_dialog(hwnd, true); return 0; }
      if (id == IDC_MODE_AUTO) { set_mode(g, 0); layout_scroll_content(g->scroll_container); return 0; }
      if (id == IDC_MODE_SEP) { set_mode(g, 1); layout_scroll_content(g->scroll_container); return 0; }
      if (id == IDC_MODE_LEN) { set_mode(g, 2); layout_scroll_content(g->scroll_container); return 0; }
      if (id == IDC_GENERATE) { run_generate(hwnd); return 0; }
      if (id == IDC_COPY_REGEX) {
        if (!g->recommended_regex.empty()) {
          copy_to_clipboard(hwnd, utf8_to_wide(g->recommended_regex));
          set_status(hwnd, L"已复制推荐正则: " + utf8_to_wide(g->recommended_regex));
        } else {
          set_status(hwnd, L"暂无推荐正则可供复制，请先生成");
        }
        return 0;
      }
      if (id == IDC_COPY_CAP_REGEX) {
        if (!g->recommended_capturing_regex.empty()) {
          copy_to_clipboard(hwnd, utf8_to_wide(g->recommended_capturing_regex));
          set_status(hwnd, L"已复制提取捕获正则: " + utf8_to_wide(g->recommended_capturing_regex));
        } else {
          set_status(hwnd, L"暂无提取捕获正则可供复制，请先生成");
        }
        return 0;
      }
      if (id == IDC_SELFTEST_BTN) { run_selftest(hwnd); return 0; }

      if (id >= IDC_POS_DEL_BASE && id < IDC_POS_DEL_BASE + 500) {
        remove_example_row(hwnd, false, reinterpret_cast<HWND>(lParam));
        return 0;
      }
      if (id >= IDC_NEG_DEL_BASE && id < IDC_NEG_DEL_BASE + 500) {
        remove_example_row(hwnd, true, reinterpret_cast<HWND>(lParam));
        return 0;
      }
      if (id >= IDC_SEG_PROMOTE_BASE && id < IDC_SEG_PROMOTE_BASE + 500) {
        size_t index = static_cast<size_t>(id - IDC_SEG_PROMOTE_BASE);
        if (index < g->last_segments.size() && g->last_segments[index].is_literal) {
          auto tags = regex_tool::promotable_tags(g->last_segments[index].literal);
          bool any_promoted = false;
          for (const auto& t : tags) {
            if (std::find(g->promote_tags.begin(), g->promote_tags.end(), t) != g->promote_tags.end()) {
              any_promoted = true;
              break;
            }
          }
          if (any_promoted) {
            for (const auto& t : tags) {
              auto it = std::find(g->promote_tags.begin(), g->promote_tags.end(), t);
              if (it != g->promote_tags.end()) g->promote_tags.erase(it);
            }
          } else {
            for (const auto& t : tags) {
              if (std::find(g->promote_tags.begin(), g->promote_tags.end(), t) == g->promote_tags.end()) {
                g->promote_tags.push_back(t);
              }
            }
          }
          run_generate(hwnd);
        }
        return 0;
      }
      return 0;
    }

    case WM_NCDESTROY: {
      if (g) {
        g->cleanup_resources();
        delete g;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
      }
      PostQuitMessage(0);
      return 0;
    }
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LONG WINAPI GlobalCrashHandler(EXCEPTION_POINTERS* ep) {
  wchar_t buf[256];
  wsprintfW(buf, L"程序发生异常 (Exception Code: 0x%08X)。", ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0);
  MessageBoxW(nullptr, buf, L"RegexTool 异常提示", MB_ICONERROR | MB_OK);
  return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  SetUnhandledExceptionFilter(GlobalCrashHandler);

  // Initialize Common Controls v6 (P1-1)
  INITCOMMONCONTROLSEX icex{};
  icex.dwSize = sizeof(icex);
  icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
  InitCommonControlsEx(&icex);

  HICON hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
  HICON hIconSm = reinterpret_cast<HICON>(LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                                      GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));

  WNDCLASSW wc{};
  wc.hbrBackground = CreateSolidBrush(RGB(243, 243, 243));
  wc.lpfnWndProc = wnd_proc;
  wc.hInstance = hInstance;
  wc.hIcon = hIcon;
  wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
  wc.lpszClassName = L"RegexToolMain";
  RegisterClassW(&wc);

  // Register Scroll Area Viewport Child Window Class
  WNDCLASSW sc{};
  sc.hbrBackground = CreateSolidBrush(RGB(243, 243, 243));
  sc.lpfnWndProc = scroll_proc;
  sc.hInstance = hInstance;
  sc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
  sc.lpszClassName = L"RegexToolScrollArea";
  RegisterClassW(&sc);

  WNDCLASSW bc{};
  bc.hbrBackground = CreateSolidBrush(RGB(243, 243, 243));
  bc.lpfnWndProc = batch_proc;
  bc.hInstance = hInstance;
  bc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
  bc.lpszClassName = L"RegexToolBatch";
  RegisterClassW(&bc);

  // Keyboard Accelerators (P2-3)
  ACCEL accels[] = {
    { FCONTROL | FVIRTKEY, 'S', IDC_SAVE },
    { FCONTROL | FVIRTKEY, 'N', IDC_NEW },
    { FCONTROL | FVIRTKEY, 'O', IDC_LOAD },
    { FCONTROL | FVIRTKEY, VK_RETURN, IDC_GENERATE },
  };
  HACCEL hAccel = CreateAcceleratorTableW(accels, sizeof(accels) / sizeof(accels[0]));

  g_main_hwnd = CreateWindowExW(WS_EX_COMPOSITED, L"RegexToolMain", L"条码正则生成器",
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                CW_USEDEFAULT, CW_USEDEFAULT, 1020, 720,
                                nullptr, nullptr, hInstance, nullptr);
  if (hIcon) SendMessageW(g_main_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
  if (hIconSm) SendMessageW(g_main_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIconSm));

  ShowWindow(g_main_hwnd, nCmdShow);
  UpdateWindow(g_main_hwnd);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    if (!TranslateAcceleratorW(g_main_hwnd, hAccel, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }

  if (hAccel) DestroyAcceleratorTable(hAccel);
  return static_cast<int>(msg.wParam);
}
