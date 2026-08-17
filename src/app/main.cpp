// Win32 UI for the Regex Tool (工单 01).
// Minimal vertical slice: dynamic positive-example inputs ("+ 正例" adds,
// "×" removes), a Generate button, and a results pane showing the candidate
// regex, per-example verdicts, and the segment model.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

#include "engine/engine.h"

namespace {

constexpr int IDC_ADD_POSITIVE = 1000;
constexpr int IDC_GENERATE = 1001;
constexpr int IDC_RESULT_EDIT = 1002;
constexpr int IDC_EDIT_BASE = 2000;
constexpr int IDC_DEL_BASE = 3000;

constexpr int ROW_HEIGHT = 28;
constexpr int EDIT_W = 380;
constexpr int DEL_W = 30;
constexpr int RESULT_X = 480;

struct AppState {
  std::vector<HWND> edits;  // one edit per positive row
  std::vector<HWND> dels;   // matching "×" buttons
  HWND add_btn = nullptr;
  HWND generate_btn = nullptr;
  HWND result_edit = nullptr;
  HFONT font = nullptr;
  int scroll_pos = 0;
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

void layout(HWND hwnd) {
  RECT rc;
  GetClientRect(hwnd, &rc);
  int view = std::max(static_cast<int>(rc.bottom), 1);

  int y = 8 - g.scroll_pos;
  for (size_t i = 0; i < g.edits.size(); ++i) {
    MoveWindow(g.edits[i], 8, y, EDIT_W, 22, TRUE);
    MoveWindow(g.dels[i], 8 + EDIT_W + 6, y, DEL_W, 22, TRUE);
    y += ROW_HEIGHT;
  }
  MoveWindow(g.add_btn, 8, y, 90, 24, TRUE);

  int content = std::max(y + 30, view);
  SCROLLINFO si{};
  si.cbSize = sizeof(si);
  si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
  si.nMin = 0;
  si.nMax = content;
  si.nPage = static_cast<UINT>(view);
  si.nPos = g.scroll_pos;
  SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

  int result_w = std::max(static_cast<int>(rc.right) - RESULT_X - 8, 120);
  int result_h = std::max(view - 48, 40);
  MoveWindow(g.generate_btn, RESULT_X, 8, 70, 24, TRUE);
  MoveWindow(g.result_edit, RESULT_X, 40, result_w, result_h, TRUE);
}

HWND make_button(HWND parent, const wchar_t* text, int id) {
  HWND btn = CreateWindowExW(0, L"BUTTON", text,
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                             0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
  SendMessageW(btn, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
  return btn;
}

void add_row(HWND hwnd) {
  size_t i = g.edits.size();
  HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                              0, 0, 0, 0, hwnd,
                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EDIT_BASE + i)),
                              GetModuleHandleW(nullptr), nullptr);
  HWND del = CreateWindowExW(0, L"BUTTON", L"\u00d7",  // ×
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                             0, 0, 0, 0, hwnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DEL_BASE + i)),
                             GetModuleHandleW(nullptr), nullptr);
  SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
  SendMessageW(del, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
  g.edits.push_back(edit);
  g.dels.push_back(del);
  layout(hwnd);
  SetFocus(edit);
}

void remove_row(HWND hwnd, HWND del_button) {
  auto it = std::find(g.dels.begin(), g.dels.end(), del_button);
  if (it == g.dels.end()) return;
  size_t index = static_cast<size_t>(it - g.dels.begin());
  DestroyWindow(g.edits[index]);
  DestroyWindow(g.dels[index]);
  g.edits.erase(g.edits.begin() + static_cast<ptrdiff_t>(index));
  g.dels.erase(g.dels.begin() + static_cast<ptrdiff_t>(index));
  layout(hwnd);
}

std::vector<std::string> collect_positives() {
  std::vector<std::string> positives;
  for (HWND edit : g.edits) {
    int len = GetWindowTextLengthW(edit);
    if (len <= 0) continue;
    std::wstring w(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(edit, w.data(), len + 1);
    w.resize(static_cast<size_t>(len));
    std::string s = wide_to_utf8(w);
    if (!s.empty()) positives.push_back(s);
  }
  return positives;
}

void run_generate() {
  auto positives = collect_positives();
  auto result = regex_tool::generate(positives);

  if (result.candidates.empty()) {
    SetWindowTextW(g.result_edit, L"（请至少输入一条正例）");
    return;
  }
  const auto& vd = result.candidates.front();

  std::string text;
  if (!result.warnings.empty()) {
    text += "警告:\n";
    for (const auto& w : result.warnings) text += "- " + w + "\n";
    text += "\n";
  }
  text += "候选正则:\n" + vd.candidate.regex + "\n\n正例验证:\n";
  for (const auto& v : vd.positive_verdicts) {
    text += v.passed ? "[通过] " : "[失败] ";
    text += v.example;
    if (!v.passed && v.failing_segment != SIZE_MAX) {
      text += "  (卡在分段 " + std::to_string(v.failing_segment + 1) + ")";
    }
    text += "\n";
  }
  text += "\n分段:\n";
  int n = 0;
  for (const auto& seg : vd.candidate.segments) {
    ++n;
    if (seg.is_literal) {
      text += std::to_string(n) + ". 固定: " + seg.literal + "\n";
    } else {
      text += std::to_string(n) + ". 字段 " + seg.field.name + ": " + seg.field.charset;
      if (seg.field.min_len == seg.field.max_len) {
        text += "{" + std::to_string(seg.field.min_len) + "}\n";
      } else {
        text += "{" + std::to_string(seg.field.min_len) + "," + std::to_string(seg.field.max_len) + "}\n";
      }
    }
  }
  SetWindowTextW(g.result_edit, utf8_to_wide(text).c_str());
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE: {
      g.font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
      g.add_btn = make_button(hwnd, L"+ 正例", IDC_ADD_POSITIVE);
      g.generate_btn = make_button(hwnd, L"生成", IDC_GENERATE);
      g.result_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                      WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY |
                                          WS_VSCROLL | ES_AUTOVSCROLL,
                                      0, 0, 0, 0, hwnd,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RESULT_EDIT)),
                                      GetModuleHandleW(nullptr), nullptr);
      SendMessageW(g.result_edit, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
      add_row(hwnd);  // start with one empty row, not a wall of boxes
      return 0;
    }

    case WM_SIZE:
      layout(hwnd);
      return 0;

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
      if (HIWORD(wParam) != BN_CLICKED) return 0;
      if (id == IDC_ADD_POSITIVE) {
        add_row(hwnd);
        return 0;
      }
      if (id == IDC_GENERATE) {
        run_generate();
        return 0;
      }
      if (id >= IDC_DEL_BASE && id < IDC_DEL_BASE + 10000) {
        remove_row(hwnd, reinterpret_cast<HWND>(lParam));
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

  HWND hwnd = CreateWindowExW(0, L"RegexToolMain", L"条码正则生成器",
                              WS_OVERLAPPEDWINDOW | WS_VSCROLL,
                              CW_USEDEFAULT, CW_USEDEFAULT, 900, 620,
                              nullptr, nullptr, hInstance, nullptr);
  ShowWindow(hwnd, nCmdShow);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
}
