// Win32 UI for the Regex Tool.
// Modern Windows UI with DPI awareness, Visual Styles, isolated scroll viewport,
// dedicated recommended regex highlight box, interactive tutorial wizard,
// streamlined profile persistence, and full bilingual (Chinese / English) live switching.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>

#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")

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

// Languages
enum class Language {
  ZH_CN = 0,
  EN_US = 1
};

enum class StrId {
  APP_TITLE,
  STATUS_READY,
  PROJECT_NAME_HINT,
  RESET_BTN,
  SAVE_BTN,
  SAVE_DLG_TITLE,
  SAVE_DLG_PROMPT,
  SAVE_DLG_OK,
  SAVE_DLG_CANCEL,
  GUIDE_BTN,
  LANG_BTN,
  POS_HEADER,
  ADD_POS,
  BATCH_POS,
  NEG_HEADER,
  ADD_NEG,
  BATCH_NEG,
  CFG_HEADER,
  MODE_AUTO,
  MODE_SEP,
  MODE_LEN,
  SEP_LABEL,
  SEP_HINT,
  DETECT_SEP_BTN,
  COUNT_LABEL,
  GENERATE_BTN,
  REC_HEADER,
  COPY_REGEX_BTN,
  COPY_CAP_BTN,
  REC_REGEX_HINT,
  RESULT_HEADER,
  RESULT_DEFAULT_HINT,
  SELFTEST_TITLE,
  SELFTEST_HINT,
  SELFTEST_BTN,
  SELFTEST_IDLE,
  SELFTEST_PASS,
  SELFTEST_FAIL,
  SELFTEST_FAIL_SEG,
  SELFTEST_FAIL_MATCH,
  SELFTEST_NEED_GEN,
  SELFTEST_NEED_INPUT,
  BATCH_DLG_POS,
  BATCH_DLG_NEG,
  BATCH_DLG_HINT,
  BATCH_DLG_OK,
  BATCH_DLG_CANCEL,
  STATUS_SAVED,
  STATUS_LOADED,
  STATUS_SAVE_FAIL,
  STATUS_LOAD_FAIL,
  STATUS_COPIED_REC,
  STATUS_COPIED_CAP,
  STATUS_NO_REGEX,
  STATUS_RESET_DONE,
  STATUS_DETECTED_SEP,
  STATUS_NO_SEP,
  STATUS_CAND_SWITCH,
  STATUS_GEN_DONE,
  STATUS_GEN_FAIL,
  PROMPT_NEED_POS,
  WIZ_STEP1_TITLE,
  WIZ_STEP1_BODY,
  WIZ_STEP2_TITLE,
  WIZ_STEP2_BODY,
  WIZ_STEP3_TITLE,
  WIZ_STEP3_BODY,
  WIZ_STEP4_TITLE,
  WIZ_STEP4_BODY,
  WIZ_STEP5_TITLE,
  WIZ_STEP5_BODY,
  WIZ_SKIP,
  WIZ_NEXT,
  WIZ_FINISH,
  WIZ_PREV
};

const wchar_t* tr(Language lang, StrId id) {
  if (lang == Language::EN_US) {
    switch (id) {
      case StrId::APP_TITLE: return L"Barcode Regex Tool - Offline Edition";
      case StrId::STATUS_READY: return L"Ready";
      case StrId::PROJECT_NAME_HINT: return L"Profile Name";
      case StrId::RESET_BTN: return L"Reset";
      case StrId::SAVE_BTN: return L"Save";
      case StrId::SAVE_DLG_TITLE: return L"Save Profile";
      case StrId::SAVE_DLG_PROMPT: return L"Enter profile name:";
      case StrId::SAVE_DLG_OK: return L"Save";
      case StrId::SAVE_DLG_CANCEL: return L"Cancel";
      case StrId::GUIDE_BTN: return L"Guide";
      case StrId::LANG_BTN: return L"中文";
      case StrId::POS_HEADER: return L"Positives (Must Match)";
      case StrId::ADD_POS: return L"+ Positive";
      case StrId::BATCH_POS: return L"Batch Paste";
      case StrId::NEG_HEADER: return L"Negatives (Must Reject)";
      case StrId::ADD_NEG: return L"+ Negative";
      case StrId::BATCH_NEG: return L"Batch Paste";
      case StrId::CFG_HEADER: return L"Segment Config";
      case StrId::MODE_AUTO: return L"Auto";
      case StrId::MODE_SEP: return L"By Separator";
      case StrId::MODE_LEN: return L"By Length";
      case StrId::SEP_LABEL: return L"Separator:";
      case StrId::SEP_HINT: return L"e.g. @, |, GS or 1P,Q,1T,S";
      case StrId::DETECT_SEP_BTN: return L"Auto Detect";
      case StrId::COUNT_LABEL: return L"Count:";
      case StrId::GENERATE_BTN: return L"Generate Regex";
      case StrId::REC_HEADER: return L"Candidate:";
      case StrId::COPY_REGEX_BTN: return L"Copy Regex";
      case StrId::COPY_CAP_BTN: return L"Copy Capture";
      case StrId::REC_REGEX_HINT: return L"(Generated regex will be displayed here)";
      case StrId::RESULT_HEADER: return L"Verification Details & Alternatives:";
      case StrId::RESULT_DEFAULT_HINT: return L"Enter positive barcodes on the left (one per row) and click \"Generate Regex\".\r\nTip: Use \"Batch Paste\" to import multiple barcodes at once.";
      case StrId::SELFTEST_TITLE: return L"Barcode Self-Test:";
      case StrId::SELFTEST_HINT: return L"Enter barcode to test...";
      case StrId::SELFTEST_BTN: return L"Verify";
      case StrId::SELFTEST_IDLE: return L"Result: Waiting for input";
      case StrId::SELFTEST_PASS: return L"Result: Pass ✓";
      case StrId::SELFTEST_FAIL: return L"Result: Fail ✗";
      case StrId::SELFTEST_FAIL_SEG: return L"(Failed at segment %d)";
      case StrId::SELFTEST_FAIL_MATCH: return L"(Does not match whole regex)";
      case StrId::SELFTEST_NEED_GEN: return L"Self-test: Please generate candidate regex first";
      case StrId::SELFTEST_NEED_INPUT: return L"Self-test: Please enter barcode to test";
      case StrId::BATCH_DLG_POS: return L"Batch Paste Positive Barcodes";
      case StrId::BATCH_DLG_NEG: return L"Batch Paste Negative Barcodes";
      case StrId::BATCH_DLG_HINT: return L"Paste multiple barcodes here (one per row or separated by common delimiters):";
      case StrId::BATCH_DLG_OK: return L"Import & Parse";
      case StrId::BATCH_DLG_CANCEL: return L"Cancel";
      case StrId::STATUS_SAVED: return L"Saved profile: ";
      case StrId::STATUS_LOADED: return L"Loaded profile: ";
      case StrId::STATUS_SAVE_FAIL: return L"Save failed (unable to write file)";
      case StrId::STATUS_LOAD_FAIL: return L"Load failed: ";
      case StrId::STATUS_COPIED_REC: return L"Candidate regex copied to clipboard";
      case StrId::STATUS_COPIED_CAP: return L"Capturing regex copied to clipboard";
      case StrId::STATUS_NO_REGEX: return L"No candidate regex available to copy, please generate first";
      case StrId::STATUS_RESET_DONE: return L"Configuration reset to default";
      case StrId::STATUS_DETECTED_SEP: return L"Detected separator markers: ";
      case StrId::STATUS_NO_SEP: return L"No consistent delimiters detected in positives, please enter manually";
      case StrId::STATUS_CAND_SWITCH: return L"Switched active candidate: ";
      case StrId::STATUS_GEN_DONE: return L"Generation complete: produced ";
      case StrId::STATUS_GEN_FAIL: return L"Generation failed: invalid rule pattern";
      case StrId::PROMPT_NEED_POS: return L"Tip: Please enter at least one positive barcode";
      case StrId::WIZ_STEP1_TITLE: return L"Step 1/5: Add Positive Barcodes";
      case StrId::WIZ_STEP1_BODY: return L"Enter valid barcodes matching the target format. Click '+ Positive' to add rows, or 'Batch Paste' to paste multiple lines at once.";
      case StrId::WIZ_STEP2_TITLE: return L"Step 2/5: Add Negative Barcodes";
      case StrId::WIZ_STEP2_BODY: return L"Enter invalid or erroneous barcodes (e.g. wrong DI prefix, extra colon) to ensure candidate regexes reject anomalies.";
      case StrId::WIZ_STEP3_TITLE: return L"Step 3/5: Segmentation & DI Setup";
      case StrId::WIZ_STEP3_BODY: return L"The tool auto-detects industrial DI prefixes (1P, Q, 1T, S) and delimiters. You can also choose explicit separators or fixed lengths.";
      case StrId::WIZ_STEP4_TITLE: return L"Step 4/5: Generate Candidate Regexes";
      case StrId::WIZ_STEP4_BODY: return L"Click 'Generate Regex'. The core engine infers multiple variants (smart recommended, relaxed length, broadened charset) and validates all samples.";
      case StrId::WIZ_STEP5_TITLE: return L"Step 5/5: Results & Instant Self-Test";
      case StrId::WIZ_STEP5_BODY: return L"Review the recommended regex on the right. Click 'Copy Regex' for your decoder software, and test any new barcode instantly in the Self-Test box!";
      case StrId::WIZ_SKIP: return L"Skip Tour";
      case StrId::WIZ_NEXT: return L"Next →";
      case StrId::WIZ_FINISH: return L"Finish ✓";
      case StrId::WIZ_PREV: return L"← Back";
    }
  }

  // Default ZH_CN
  switch (id) {
    case StrId::APP_TITLE: return L"条码正则生成器";
    case StrId::STATUS_READY: return L"就绪";
    case StrId::PROJECT_NAME_HINT: return L"选择或输入配置名称";
    case StrId::RESET_BTN: return L"刷新配置";
    case StrId::SAVE_BTN: return L"保存";
    case StrId::SAVE_DLG_TITLE: return L"保存配置";
    case StrId::SAVE_DLG_PROMPT: return L"请输入配置名称：";
    case StrId::SAVE_DLG_OK: return L"保存";
    case StrId::SAVE_DLG_CANCEL: return L"取消";
    case StrId::GUIDE_BTN: return L"向导";
    case StrId::LANG_BTN: return L"EN";
    case StrId::POS_HEADER: return L"正例（必须匹配）";
    case StrId::ADD_POS: return L"+ 正例";
    case StrId::BATCH_POS: return L"批量粘贴";
    case StrId::NEG_HEADER: return L"反例（必须排除）";
    case StrId::ADD_NEG: return L"+ 反例";
    case StrId::BATCH_NEG: return L"批量粘贴";
    case StrId::CFG_HEADER: return L"分段配置";
    case StrId::MODE_AUTO: return L"自动分段";
    case StrId::MODE_SEP: return L"按分隔符";
    case StrId::MODE_LEN: return L"按长度";
    case StrId::SEP_LABEL: return L"分隔符:";
    case StrId::SEP_HINT: return L"如 @, |, GS 或 1P,Q,1T,S";
    case StrId::DETECT_SEP_BTN: return L"自动识别";
    case StrId::COUNT_LABEL: return L"段数:";
    case StrId::GENERATE_BTN: return L"生成候选正则";
    case StrId::REC_HEADER: return L"当前候选:";
    case StrId::COPY_REGEX_BTN: return L"复制正则";
    case StrId::COPY_CAP_BTN: return L"复制捕获";
    case StrId::REC_REGEX_HINT: return L"（生成后此处将显示高亮候选正则）";
    case StrId::RESULT_HEADER: return L"验证详情与备选候选:";
    case StrId::RESULT_DEFAULT_HINT: return L"请在左侧输入正例条码（每行一条），然后点击\"生成候选正则\"。\r\n提示：可使用\"批量粘贴\"一次输入多条。";
    case StrId::SELFTEST_TITLE: return L"自测条码验证:";
    case StrId::SELFTEST_HINT: return L"输入待判定条码进行自测...";
    case StrId::SELFTEST_BTN: return L"判定";
    case StrId::SELFTEST_IDLE: return L"自测结果: 待输入";
    case StrId::SELFTEST_PASS: return L"自测: 通过 ✓";
    case StrId::SELFTEST_FAIL: return L"自测: 不通过 ✗";
    case StrId::SELFTEST_FAIL_SEG: return L"（卡在分段 %d）";
    case StrId::SELFTEST_FAIL_MATCH: return L"（整串不匹配）";
    case StrId::SELFTEST_NEED_GEN: return L"自测: 请先生成候选正则";
    case StrId::SELFTEST_NEED_INPUT: return L"自测: 请输入待判定条码";
    case StrId::BATCH_DLG_POS: return L"批量粘贴正例条码";
    case StrId::BATCH_DLG_NEG: return L"批量粘贴反例条码";
    case StrId::BATCH_DLG_HINT: return L"请在此粘贴多行条码（支持每行一条或以常见标点分隔）：";
    case StrId::BATCH_DLG_OK: return L"导入并解析";
    case StrId::BATCH_DLG_CANCEL: return L"取消";
    case StrId::STATUS_SAVED: return L"已成功保存配置: ";
    case StrId::STATUS_LOADED: return L"已成功加载配置: ";
    case StrId::STATUS_SAVE_FAIL: return L"保存失败（无法写入文件）";
    case StrId::STATUS_LOAD_FAIL: return L"加载失败: ";
    case StrId::STATUS_COPIED_REC: return L"已复制推荐正则: ";
    case StrId::STATUS_COPIED_CAP: return L"已复制提取捕获正则: ";
    case StrId::STATUS_NO_REGEX: return L"暂无推荐正则可供复制，请先生成";
    case StrId::STATUS_RESET_DONE: return L"已重置清空全部配置";
    case StrId::STATUS_DETECTED_SEP: return L"已自动识别分隔标记: ";
    case StrId::STATUS_NO_SEP: return L"未在正例中检测到一致的分隔标记，请手动输入";
    case StrId::STATUS_CAND_SWITCH: return L"当前已切换测试候选: ";
    case StrId::STATUS_GEN_DONE: return L"生成完成: 产出 ";
    case StrId::STATUS_GEN_FAIL: return L"生成失败: 规则表达式异常";
    case StrId::PROMPT_NEED_POS: return L"提示: 请至少输入一条正例条码";
    case StrId::WIZ_STEP1_TITLE: return L"步骤 1/5: 输入正例条码";
    case StrId::WIZ_STEP1_BODY: return L"在左侧输入符合目标格式的标准条码。可点「+ 正例」逐条添加，或点「批量粘贴」快速贴入多条样本。";
    case StrId::WIZ_STEP2_TITLE: return L"步骤 2/5: 输入反例条码";
    case StrId::WIZ_STEP2_BODY: return L"在反例区输入格式不合规或错误的条码（如前缀错误、多余字符等），用于验证并排除误识别。";
    case StrId::WIZ_STEP3_TITLE: return L"步骤 3/5: 分段与前缀配置";
    case StrId::WIZ_STEP3_BODY: return L"系统默认全自动推断工业 DI 前缀（如 1P, Q, 1T）与分隔符。如需定制，也可切换为按指定分隔符或固定长度分段。";
    case StrId::WIZ_STEP4_TITLE: return L"步骤 4/5: 一键生成最优正则";
    case StrId::WIZ_STEP4_BODY: return L"点击「生成候选正则」，核心引擎将自动归纳多个正则变体（智能推荐、弹性变长、字符集放宽等），并逐例严格验证。";
    case StrId::WIZ_STEP5_TITLE: return L"步骤 5/5: 候选结果与即时自测";
    case StrId::WIZ_STEP5_BODY: return L"在右侧查看高亮推荐正则，点击「复制正则」粘贴到解码软件。下方自测框随手输入条码即可立即判定！";
    case StrId::WIZ_SKIP: return L"跳过向导";
    case StrId::WIZ_NEXT: return L"下一步 →";
    case StrId::WIZ_FINISH: return L"完成向导 ✓";
    case StrId::WIZ_PREV: return L"← 上一步";
  }
  return L"";
}

// Control IDs — non-overlapping intervals (P3-3)
constexpr int IDC_PROJECT_EDIT = 1000;
constexpr int IDC_RESET = 1001;
constexpr int IDC_SAVE = 1002;
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
constexpr int IDC_DETECT_SEP = 1021;
constexpr int IDC_STATUSBAR = 1022;
constexpr int IDC_CANDIDATE_COMBO = 1024;
constexpr int IDC_GUIDE_BTN = 1025;
constexpr int IDC_LANG_BTN = 1026;

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

std::wstring exe_dir() {
  wchar_t buf[MAX_PATH];
  GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring p = buf;
  std::size_t pos = p.find_last_of(L"\\/");
  if (pos != std::wstring::npos) return p.substr(0, pos);
  return L".";
}

struct AppState {
  HWND main_hwnd = nullptr;
  HWND scroll_container = nullptr;
  HWND wizard_overlay = nullptr;
  UINT dpi = 96;

  // Language state & Preferences
  Language language = Language::ZH_CN;
  bool wizard_shown = false;
  bool wizard_active = false;
  int wizard_step = 0; // 0..4
  std::wstring last_project_name;

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
  HWND save_btn = nullptr, reset_btn = nullptr;
  HWND guide_btn = nullptr, lang_btn = nullptr;

  // Controls - Scrollable Viewport (Children of scroll_container)
  HWND pos_label = nullptr, neg_label = nullptr, cfg_label = nullptr;
  HWND mode_auto = nullptr, mode_sep = nullptr, mode_len = nullptr;
  HWND sep_label = nullptr, sep_edit = nullptr, detect_sep_btn = nullptr;
  HWND count_label = nullptr, seg_count_combo = nullptr;
  std::vector<HWND> length_boxes;
  std::vector<HWND> pos_edits, pos_dels;
  std::vector<HWND> neg_edits, neg_dels;
  HWND add_pos = nullptr, add_neg = nullptr, batch_pos = nullptr, batch_neg = nullptr;

  // Controls - Fixed Left Pane Footer
  HWND generate_btn = nullptr;

  // Controls - Right Pane
  HWND rec_header_label = nullptr;
  HWND candidate_combo = nullptr;
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

  // Wizard Overlay Background Snapshot
  HDC wizard_bg_dc = nullptr;
  HBITMAP wizard_bg_bitmap = nullptr;

  // Wizard Card Hit-Testing Rectangles
  RECT wizard_card_rc{};
  RECT wizard_skip_btn_rc{};
  RECT wizard_prev_btn_rc{};
  RECT wizard_next_btn_rc{};
  RECT wizard_target_rc{};

  // Scroll state
  int scroll_pos = 0;

  // Engine state
  std::vector<regex_tool::SegmentRule> last_segments;
  std::vector<std::string> promote_tags;
  std::vector<regex_tool::FieldRule> field_overrides;
  std::vector<regex_tool::CandidateVerdict> last_candidates;
  std::vector<std::string> last_warnings;
  bool last_has_passing = false;
  int selected_candidate_index = 0;
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
    if (wizard_bg_dc) { DeleteDC(wizard_bg_dc); wizard_bg_dc = nullptr; }
    if (wizard_bg_bitmap) { DeleteObject(wizard_bg_bitmap); wizard_bg_bitmap = nullptr; }
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

// Configuration Persistence
std::wstring get_config_path() {
  return exe_dir() + L"\\config.ini";
}

void load_app_config(AppState* g) {
  if (!g) return;
  std::wstring path = get_config_path();
  wchar_t buf[64];
  GetPrivateProfileStringW(L"General", L"language", L"zh_CN", buf, 64, path.c_str());
  if (std::wstring(buf) == L"en_US") {
    g->language = Language::EN_US;
  } else {
    g->language = Language::ZH_CN;
  }
  g->wizard_shown = (GetPrivateProfileIntW(L"General", L"wizard_shown", 0, path.c_str()) != 0);

  wchar_t proj_buf[128];
  GetPrivateProfileStringW(L"General", L"last_project", L"", proj_buf, 128, path.c_str());
  g->last_project_name = proj_buf;
}

void save_app_config(const AppState* g) {
  if (!g) return;
  std::wstring path = get_config_path();
  WritePrivateProfileStringW(L"General", L"language",
                             g->language == Language::EN_US ? L"en_US" : L"zh_CN",
                             path.c_str());
  WritePrivateProfileStringW(L"General", L"wizard_shown",
                             g->wizard_shown ? L"1" : L"0",
                             path.c_str());
  WritePrivateProfileStringW(L"General", L"last_project",
                             g->last_project_name.c_str(),
                             path.c_str());
}

constexpr DWORD CHILD_STYLE = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS;

HWND make_button(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h, DWORD extra = 0) {
  AppState* g = get_app_state(parent);
  HWND hBtn = CreateWindowExW(0, L"BUTTON", text, CHILD_STYLE | BS_PUSHBUTTON | extra,
                              x, y, w, h, parent,
                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                              GetModuleHandleW(nullptr), nullptr);
  if (g && g->font) SendMessageW(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
  return hBtn;
}

HWND make_radio(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h, bool checked) {
  AppState* g = get_app_state(parent);
  HWND hRadio = CreateWindowExW(0, L"BUTTON", text, CHILD_STYLE | BS_AUTORADIOBUTTON,
                                x, y, w, h, parent,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                GetModuleHandleW(nullptr), nullptr);
  if (checked) SendMessageW(hRadio, BM_SETCHECK, BST_CHECKED, 0);
  if (g && g->font) SendMessageW(hRadio, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
  return hRadio;
}

HWND make_edit(HWND parent, int id, int x, int y, int w, int h, DWORD extra = 0, const wchar_t* cue = nullptr) {
  AppState* g = get_app_state(parent);
  HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                               CHILD_STYLE | ES_AUTOHSCROLL | extra,
                               x, y, w, h, parent,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                               GetModuleHandleW(nullptr), nullptr);
  if (cue && *cue) {
    SendMessageW(hEdit, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(cue));
  }
  if (g && g->font) SendMessageW(hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
  return hEdit;
}

HWND make_static(HWND parent, const wchar_t* text, int x, int y, int w, int h, bool is_header = false) {
  AppState* g = get_app_state(parent);
  HWND hStatic = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | WS_CLIPSIBLINGS,
                                 x, y, w, h, parent, nullptr,
                                 GetModuleHandleW(nullptr), nullptr);
  HFONT f = g ? (is_header && g->header_font ? g->header_font : g->font) : nullptr;
  if (f) SendMessageW(hStatic, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE);
  return hStatic;
}

HWND make_combo(HWND parent, int id, int x, int y, int w, int h, bool editable = false, const wchar_t* cue = nullptr) {
  AppState* g = get_app_state(parent);
  DWORD style = CHILD_STYLE | WS_VSCROLL | (editable ? CBS_DROPDOWN : CBS_DROPDOWNLIST);
  HWND hCombo = CreateWindowExW(0, L"COMBOBOX", L"", style,
                               x, y, w, h, parent,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                               GetModuleHandleW(nullptr), nullptr);
  if (cue && *cue) {
    SendMessageW(hCombo, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(cue));
  }
  if (g && g->font) SendMessageW(hCombo, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
  return hCombo;
}

// Forward declarations
void layout(HWND hwnd);
void layout_scroll_content(HWND scroll_hwnd);
void run_generate(HWND hwnd);
void run_selftest(HWND hwnd);
void set_wizard_step(AppState* g, int step);
void finish_wizard(AppState* g);
void start_wizard(AppState* g);
bool load_project(HWND hwnd, const std::wstring& name, std::wstring& error);
bool save_project(HWND hwnd, const std::wstring& name);
std::string get_candidate_display_name(const regex_tool::Candidate& cand, Language lang);
std::string translate_warning(const std::string& w, Language lang);
std::string format_candidate_block(const regex_tool::CandidateVerdict& vd, size_t index, bool recommended, Language lang);

HWND get_step_target(AppState* g, int step) {
  if (!g) return nullptr;
  switch (step) {
    case 0: return g->batch_pos ? g->batch_pos : g->add_pos;
    case 1: return g->batch_neg ? g->batch_neg : g->add_neg;
    case 2: return g->mode_auto;
    case 3: return g->generate_btn;
    case 4: return g->rec_regex_edit ? g->rec_regex_edit : g->copy_btn;
    default: return nullptr;
  }
}

RECT get_step_target_rect(AppState* g, HWND overlay_hwnd, int step) {
  RECT target_rc{ 0, 0, 0, 0 };
  if (!g) return target_rc;

  switch (step) {
    case 0: {
      if (g->add_pos && g->batch_pos) {
        RECT r1{}, r2{};
        GetWindowRect(g->add_pos, &r1);
        GetWindowRect(g->batch_pos, &r2);
        UnionRect(&target_rc, &r1, &r2);
        if (!g->pos_edits.empty()) {
          RECT r3{};
          GetWindowRect(g->pos_edits[0], &r3);
          UnionRect(&target_rc, &target_rc, &r3);
        }
      } else if (g->batch_pos) {
        GetWindowRect(g->batch_pos, &target_rc);
      }
      break;
    }
    case 1: {
      if (g->add_neg && g->batch_neg) {
        RECT r1{}, r2{};
        GetWindowRect(g->add_neg, &r1);
        GetWindowRect(g->batch_neg, &r2);
        UnionRect(&target_rc, &r1, &r2);
        if (!g->neg_edits.empty()) {
          RECT r3{};
          GetWindowRect(g->neg_edits[0], &r3);
          UnionRect(&target_rc, &target_rc, &r3);
        }
      } else if (g->batch_neg) {
        GetWindowRect(g->batch_neg, &target_rc);
      }
      break;
    }
    case 2: {
      if (g->mode_auto && g->mode_len) {
        RECT r1{}, r2{};
        GetWindowRect(g->mode_auto, &r1);
        GetWindowRect(g->mode_len, &r2);
        UnionRect(&target_rc, &r1, &r2);
      } else if (g->mode_auto) {
        GetWindowRect(g->mode_auto, &target_rc);
      }
      break;
    }
    case 3: {
      if (g->generate_btn) {
        GetWindowRect(g->generate_btn, &target_rc);
      }
      break;
    }
    case 4: {
      if (g->rec_regex_edit && g->copy_cap_btn) {
        RECT r1{}, r2{};
        GetWindowRect(g->rec_header_label ? g->rec_header_label : g->rec_regex_edit, &r1);
        GetWindowRect(g->copy_cap_btn, &r2);
        UnionRect(&target_rc, &r1, &r2);
        if (g->rec_regex_edit) {
          RECT r3{};
          GetWindowRect(g->rec_regex_edit, &r3);
          UnionRect(&target_rc, &target_rc, &r3);
        }
      } else if (g->rec_regex_edit) {
        GetWindowRect(g->rec_regex_edit, &target_rc);
      }
      break;
    }
  }

  MapWindowPoints(HWND_DESKTOP, overlay_hwnd, reinterpret_cast<LPPOINT>(&target_rc), 2);
  InflateRect(&target_rc, g->scale_dpi(6), g->scale_dpi(6));
  return target_rc;
}

void capture_wizard_background(AppState* g) {
  if (!g || !g->main_hwnd) return;
  if (g->wizard_overlay) {
    ShowWindow(g->wizard_overlay, SW_HIDE);
  }
  RedrawWindow(g->main_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);

  RECT rc;
  GetClientRect(g->main_hwnd, &rc);
  int win_w = rc.right;
  int win_h = rc.bottom;
  if (win_w <= 0 || win_h <= 0) return;

  POINT pt{ 0, 0 };
  ClientToScreen(g->main_hwnd, &pt);

  HDC screen_dc = GetDC(nullptr);
  if (!g->wizard_bg_dc) {
    g->wizard_bg_dc = CreateCompatibleDC(screen_dc);
  }
  if (g->wizard_bg_bitmap) {
    DeleteObject(g->wizard_bg_bitmap);
    g->wizard_bg_bitmap = nullptr;
  }
  g->wizard_bg_bitmap = CreateCompatibleBitmap(screen_dc, win_w, win_h);
  SelectObject(g->wizard_bg_dc, g->wizard_bg_bitmap);
  BitBlt(g->wizard_bg_dc, 0, 0, win_w, win_h, screen_dc, pt.x, pt.y, SRCCOPY);
  ReleaseDC(nullptr, screen_dc);
}

// Wizard Overlay Window Procedure
LRESULT CALLBACK wizard_overlay_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  auto g = reinterpret_cast<AppState*>(GetWindowLongPtrW(g_main_hwnd, GWLP_USERDATA));
  if (!g || !g->wizard_active) return DefWindowProcW(hwnd, msg, wParam, lParam);

  switch (msg) {
    case WM_ERASEBKGND:
      return 1;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      RECT rc;
      GetClientRect(hwnd, &rc);
      int win_w = rc.right;
      int win_h = rc.bottom;

      // Double buffering
      HDC mem_dc = CreateCompatibleDC(hdc);
      HBITMAP mem_bm = CreateCompatibleBitmap(hdc, win_w, win_h);
      HGDIOBJ old_bm = SelectObject(mem_dc, mem_bm);

      // 1. Draw base snapshot of the real underlying interface
      if (g->wizard_bg_dc) {
        BitBlt(mem_dc, 0, 0, win_w, win_h, g->wizard_bg_dc, 0, 0, SRCCOPY);
      } else {
        HBRUSH bg = CreateSolidBrush(RGB(243, 243, 243));
        FillRect(mem_dc, &rc, bg);
        DeleteObject(bg);
      }

      // 2. Determine Target Control bounding box
      RECT target_rc = get_step_target_rect(g, hwnd, g->wizard_step);
      g->wizard_target_rc = target_rc;

      // 3. AlphaBlend semi-transparent dark mask on the 4 surrounding bands
      BITMAPINFO bmi{};
      bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bmi.bmiHeader.biWidth = 1;
      bmi.bmiHeader.biHeight = 1;
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 32;
      bmi.bmiHeader.biCompression = BI_RGB;
      void* bits = nullptr;
      HBITMAP dim_bm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
      if (bits) {
        *reinterpret_cast<DWORD*>(bits) = 0x000F172A; // Deep Slate tint RGB(15, 23, 42)
      }
      HDC dim_dc = CreateCompatibleDC(hdc);
      HGDIOBJ old_dim_bm = SelectObject(dim_dc, dim_bm);

      BLENDFUNCTION bf{};
      bf.BlendOp = AC_SRC_OVER;
      bf.BlendFlags = 0;
      bf.SourceConstantAlpha = 145; // ~57% alpha dimming
      bf.AlphaFormat = 0;

      // Top band
      if (target_rc.top > 0) {
        AlphaBlend(mem_dc, 0, 0, win_w, static_cast<int>(target_rc.top), dim_dc, 0, 0, 1, 1, bf);
      }
      // Bottom band
      if (target_rc.bottom < win_h) {
        AlphaBlend(mem_dc, 0, static_cast<int>(target_rc.bottom), win_w, win_h - static_cast<int>(target_rc.bottom), dim_dc, 0, 0, 1, 1, bf);
      }
      // Left band
      if (target_rc.left > 0 && target_rc.bottom > target_rc.top) {
        AlphaBlend(mem_dc, 0, static_cast<int>(target_rc.top), static_cast<int>(target_rc.left),
                   static_cast<int>(target_rc.bottom - target_rc.top), dim_dc, 0, 0, 1, 1, bf);
      }
      // Right band
      if (target_rc.right < win_w && target_rc.bottom > target_rc.top) {
        AlphaBlend(mem_dc, static_cast<int>(target_rc.right), static_cast<int>(target_rc.top),
                   win_w - static_cast<int>(target_rc.right),
                   static_cast<int>(target_rc.bottom - target_rc.top), dim_dc, 0, 0, 1, 1, bf);
      }

      SelectObject(dim_dc, old_dim_bm);
      DeleteDC(dim_dc);
      DeleteObject(dim_bm);

      // 4. Draw Focus Border around target
      HPEN glow_pen = CreatePen(PS_SOLID, g->scale_dpi(2), RGB(37, 99, 235)); // Vibrant blue
      HGDIOBJ old_pen = SelectObject(mem_dc, glow_pen);
      HGDIOBJ old_brush = SelectObject(mem_dc, GetStockObject(NULL_BRUSH));
      int focus_radius = g->scale_dpi(6);
      RoundRect(mem_dc, target_rc.left, target_rc.top, target_rc.right, target_rc.bottom, focus_radius, focus_radius);
      DeleteObject(glow_pen);

      // 5. Position Tutorial Explanation Card
      int card_w = g->scale_dpi(360);
      int card_h = g->scale_dpi(175);
      int card_x = 0, card_y = 0;
      int margin = g->scale_dpi(16);

      if (static_cast<int>(target_rc.right) + card_w + margin <= win_w) {
        card_x = static_cast<int>(target_rc.right) + g->scale_dpi(14);
        int target_mid_y = static_cast<int>((target_rc.top + target_rc.bottom - card_h) / 2);
        card_y = std::clamp(target_mid_y, margin, win_h - card_h - margin);
      } else if (static_cast<int>(target_rc.left) - card_w - margin >= 0) {
        card_x = static_cast<int>(target_rc.left) - card_w - g->scale_dpi(14);
        int target_mid_y = static_cast<int>((target_rc.top + target_rc.bottom - card_h) / 2);
        card_y = std::clamp(target_mid_y, margin, win_h - card_h - margin);
      } else {
        card_x = std::clamp((win_w - card_w) / 2, margin, win_w - card_w - margin);
        if (static_cast<int>(target_rc.bottom) + card_h + margin <= win_h) {
          card_y = static_cast<int>(target_rc.bottom) + g->scale_dpi(14);
        } else {
          card_y = static_cast<int>(target_rc.top) - card_h - g->scale_dpi(14);
        }
      }
      g->wizard_card_rc = { card_x, card_y, card_x + card_w, card_y + card_h };

      // Card Background with Drop Border
      HBRUSH card_brush = CreateSolidBrush(RGB(255, 255, 255));
      HPEN card_border = CreatePen(PS_SOLID, 1, RGB(203, 213, 225));
      SelectObject(mem_dc, card_brush);
      SelectObject(mem_dc, card_border);
      int card_radius = g->scale_dpi(8);
      RoundRect(mem_dc, card_x, card_y, card_x + card_w, card_y + card_h, card_radius, card_radius);
      DeleteObject(card_brush);
      DeleteObject(card_border);

      // Card Pill Badge
      RECT pill_rc{ card_x + g->scale_dpi(14), card_y + g->scale_dpi(14),
                    card_x + g->scale_dpi(58), card_y + g->scale_dpi(34) };
      HBRUSH pill_brush = CreateSolidBrush(RGB(239, 246, 255));
      HPEN pill_pen = CreatePen(PS_SOLID, 1, RGB(191, 219, 254));
      SelectObject(mem_dc, pill_brush);
      SelectObject(mem_dc, pill_pen);
      RoundRect(mem_dc, pill_rc.left, pill_rc.top, pill_rc.right, pill_rc.bottom, g->scale_dpi(4), g->scale_dpi(4));
      DeleteObject(pill_brush);
      DeleteObject(pill_pen);

      SetBkMode(mem_dc, TRANSPARENT);
      SetTextColor(mem_dc, RGB(29, 78, 216));
      SelectObject(mem_dc, g->bold_font ? g->bold_font : g->font);
      std::wstring step_badge = std::to_wstring(g->wizard_step + 1) + L" / 5";
      DrawTextW(mem_dc, step_badge.c_str(), -1, &pill_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

      // Card Title
      StrId title_id = StrId::WIZ_STEP1_TITLE;
      StrId body_id = StrId::WIZ_STEP1_BODY;
      switch (g->wizard_step) {
        case 0: title_id = StrId::WIZ_STEP1_TITLE; body_id = StrId::WIZ_STEP1_BODY; break;
        case 1: title_id = StrId::WIZ_STEP2_TITLE; body_id = StrId::WIZ_STEP2_BODY; break;
        case 2: title_id = StrId::WIZ_STEP3_TITLE; body_id = StrId::WIZ_STEP3_BODY; break;
        case 3: title_id = StrId::WIZ_STEP4_TITLE; body_id = StrId::WIZ_STEP4_BODY; break;
        case 4: title_id = StrId::WIZ_STEP5_TITLE; body_id = StrId::WIZ_STEP5_BODY; break;
      }

      RECT title_rc{ card_x + g->scale_dpi(66), card_y + g->scale_dpi(14),
                     card_x + card_w - g->scale_dpi(14), card_y + g->scale_dpi(34) };
      SetTextColor(mem_dc, RGB(15, 23, 42));
      SelectObject(mem_dc, g->header_font ? g->header_font : g->font);
      DrawTextW(mem_dc, tr(g->language, title_id), -1, &title_rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

      // Card Body
      RECT body_rc{ card_x + g->scale_dpi(14), card_y + g->scale_dpi(44),
                    card_x + card_w - g->scale_dpi(14), card_y + card_h - g->scale_dpi(48) };
      SetTextColor(mem_dc, RGB(71, 85, 105));
      SelectObject(mem_dc, g->font);
      DrawTextW(mem_dc, tr(g->language, body_id), -1, &body_rc, DT_LEFT | DT_WORDBREAK);

      // Skip Button
      int btn_h = g->scale_dpi(28);
      int btn_y = card_y + card_h - btn_h - g->scale_dpi(12);
      g->wizard_skip_btn_rc = { card_x + g->scale_dpi(14), btn_y,
                                card_x + g->scale_dpi(92), btn_y + btn_h };
      SetTextColor(mem_dc, RGB(148, 163, 184));
      SelectObject(mem_dc, g->font);
      DrawTextW(mem_dc, tr(g->language, StrId::WIZ_SKIP), -1, &g->wizard_skip_btn_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

      // Prev Button (if step > 0)
      int next_btn_w = g->scale_dpi(92);
      int next_btn_x = card_x + card_w - next_btn_w - g->scale_dpi(14);
      g->wizard_next_btn_rc = { next_btn_x, btn_y, next_btn_x + next_btn_w, btn_y + btn_h };

      if (g->wizard_step > 0) {
        int prev_btn_w = g->scale_dpi(80);
        int prev_btn_x = next_btn_x - prev_btn_w - g->scale_dpi(8);
        g->wizard_prev_btn_rc = { prev_btn_x, btn_y, prev_btn_x + prev_btn_w, btn_y + btn_h };

        HBRUSH prev_brush = CreateSolidBrush(RGB(248, 250, 252));
        HPEN prev_pen = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
        SelectObject(mem_dc, prev_brush);
        SelectObject(mem_dc, prev_pen);
        RoundRect(mem_dc, prev_btn_x, btn_y, prev_btn_x + prev_btn_w, btn_y + btn_h, g->scale_dpi(5), g->scale_dpi(5));
        DeleteObject(prev_brush);
        DeleteObject(prev_pen);

        SetTextColor(mem_dc, RGB(71, 85, 105));
        SelectObject(mem_dc, g->font);
        DrawTextW(mem_dc, tr(g->language, StrId::WIZ_PREV), -1, &g->wizard_prev_btn_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      } else {
        g->wizard_prev_btn_rc = { 0, 0, 0, 0 };
      }

      // Next / Finish Button (Primary Blue)
      HBRUSH next_brush = CreateSolidBrush(RGB(0, 103, 192));
      HPEN next_pen = CreatePen(PS_SOLID, 1, RGB(0, 90, 158));
      SelectObject(mem_dc, next_brush);
      SelectObject(mem_dc, next_pen);
      RoundRect(mem_dc, next_btn_x, btn_y, next_btn_x + next_btn_w, btn_y + btn_h, g->scale_dpi(5), g->scale_dpi(5));
      DeleteObject(next_brush);
      DeleteObject(next_pen);

      SetTextColor(mem_dc, RGB(255, 255, 255));
      SelectObject(mem_dc, g->bold_font ? g->bold_font : g->font);
      const wchar_t* next_label = (g->wizard_step == 4) ? tr(g->language, StrId::WIZ_FINISH)
                                                         : tr(g->language, StrId::WIZ_NEXT);
      DrawTextW(mem_dc, next_label, -1, &g->wizard_next_btn_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

      // Cleanup GDI
      SelectObject(mem_dc, old_brush);
      SelectObject(mem_dc, old_pen);

      // Blit to window
      BitBlt(hdc, 0, 0, win_w, win_h, mem_dc, 0, 0, SRCCOPY);
      SelectObject(mem_dc, old_bm);
      DeleteObject(mem_bm);
      DeleteDC(mem_dc);
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_LBUTTONDOWN: {
      POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
      if (PtInRect(&g->wizard_skip_btn_rc, pt)) {
        finish_wizard(g);
        return 0;
      }
      if (PtInRect(&g->wizard_next_btn_rc, pt)) {
        set_wizard_step(g, g->wizard_step + 1);
        return 0;
      }
      if (g->wizard_step > 0 && PtInRect(&g->wizard_prev_btn_rc, pt)) {
        set_wizard_step(g, g->wizard_step - 1);
        return 0;
      }
      if (PtInRect(&g->wizard_target_rc, pt)) {
        // Forward click to target control
        HWND target = get_step_target(g, g->wizard_step);
        if (target && IsWindow(target)) {
          POINT pt_target = pt;
          MapWindowPoints(hwnd, target, &pt_target, 1);
          SendMessageW(target, WM_LBUTTONDOWN, wParam, MAKELPARAM(pt_target.x, pt_target.y));
          SendMessageW(target, WM_LBUTTONUP, wParam, MAKELPARAM(pt_target.x, pt_target.y));
          set_wizard_step(g, g->wizard_step + 1);
        }
        return 0;
      }
      // Eat clicks outside to prevent interaction with background
      return 0;
    }

    case WM_SETCURSOR: {
      POINT pt;
      GetCursorPos(&pt);
      MapWindowPoints(HWND_DESKTOP, hwnd, &pt, 1);
      if (PtInRect(&g->wizard_skip_btn_rc, pt) ||
          PtInRect(&g->wizard_next_btn_rc, pt) ||
          (g->wizard_step > 0 && PtInRect(&g->wizard_prev_btn_rc, pt)) ||
          PtInRect(&g->wizard_target_rc, pt)) {
        SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_HAND)));
        return TRUE;
      }
      SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW)));
      return TRUE;
    }

    case WM_KEYDOWN: {
      if (wParam == VK_ESCAPE) {
        finish_wizard(g);
        return 0;
      }
      if (wParam == VK_RETURN || wParam == VK_SPACE || wParam == VK_RIGHT) {
        set_wizard_step(g, g->wizard_step + 1);
        return 0;
      }
      if (wParam == VK_LEFT && g->wizard_step > 0) {
        set_wizard_step(g, g->wizard_step - 1);
        return 0;
      }
      return 0;
    }
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

std::string get_candidate_display_name(const regex_tool::Candidate& cand, Language lang) {
  if (lang == Language::EN_US) {
    if (cand.variant == "aim_stripped" || cand.display_name.find("剥离 AIM") != std::string::npos) return "[Payload] Smart Recommended (AIM Stripped)";
    if (cand.variant == "aim_full" || cand.display_name.find("保留 AIM") != std::string::npos) return "[Full-link] Route With AIM Prefix";
    if (cand.variant == "smart" || cand.display_name == "智能业务推荐") {
      if (cand.regex.find("\\[\\)>") != std::string::npos || cand.regex.find("[)>") != std::string::npos) return "[ISO 15434] Structured Envelope";
      return "[Smart] Recommended";
    }
    if (cand.variant == "relaxed" || cand.display_name == "弹性变长扩展") return "[Elastic] Flexible Length Match";
    if (cand.variant == "broadened" || cand.display_name == "字符集放宽") return "[Broadened] Extended Character Set";
    if (cand.variant == "exact" || cand.display_name == "样本严格拟合") return "[Strict] Positional Exact Fit";
    if (cand.variant == "custom" || cand.display_name == "自定义规则") return "[Custom] User Defined Rules";
    if (cand.display_name == "纯常量字面量") return "[Literal] Exact Constant Match";
    if (cand.display_name == "固定长度通配") return "[Fixed] Length Wildcard";
    if (cand.display_name == "变长宽松匹配") return "[Elastic] Flexible Length";
    if (!cand.variant.empty()) return cand.variant;
    return cand.display_name;
  } else {
    if (cand.variant == "aim_stripped") return "【标准载荷】业务载荷推荐（剥离 AIM 码制前缀）";
    if (cand.variant == "aim_full") return "【全链路路由】保留 AIM 码制前缀";
    if (cand.variant == "smart") {
      if (cand.regex.find("\\[\\)>") != std::string::npos || cand.regex.find("[)>") != std::string::npos) return "【ISO 15434 外壳】结构化协议封装";
      return "【智能业务推荐】";
    }
    if (cand.variant == "relaxed") return "【弹性变长】适应未来位数扩展";
    if (cand.variant == "broadened") return "【字符集放宽】兼容字母与扩展符号";
    if (cand.variant == "exact") return "【精准定长】严格样本拟合";
    if (cand.variant == "custom") return "【自定义规则】";
    if (!cand.display_name.empty()) return cand.display_name;
    return cand.variant;
  }
}

std::string translate_warning(const std::string& w, Language lang) {
  if (lang != Language::EN_US) return w;
  if (w.find("首尾有空白，已自动去除") != std::string::npos) {
    return "Leading/trailing whitespace automatically trimmed from positive samples.";
  }
  if (w.find("正例末尾多余的分隔符已自动去除并校正") != std::string::npos) {
    return "Trailing redundant delimiters in positives automatically cleaned.";
  }
  if (w.find("各正例的分隔符/标记数量不一致，已改用自动分段") != std::string::npos) {
    return "Inconsistent delimiter counts across positive samples; fallback to auto segmentation.";
  }
  if (w.find("正例中有条码短于给定长度之和") != std::string::npos) {
    return "Positive barcode shorter than sum of specified segment lengths; fallback to auto segmentation.";
  }
  if (w.find("EAN-13") != std::string::npos) {
    return "Detected standard EAN-13 format (13 digits). The final digit is a Modulo 10 checksum, which should be verified at the business logic layer.";
  }
  if (w.find("UPC-A") != std::string::npos) {
    return "Detected standard UPC-A format (12 digits). The final digit is a Modulo 10 checksum, which should be verified at the business logic layer.";
  }
  if (w.find("EAN-8") != std::string::npos) {
    return "Detected standard EAN-8 format (8 digits). Check digit verification belongs to the business logic layer.";
  }
  if (w.find("ITF-14") != std::string::npos) {
    return "Detected standard ITF-14 format (14 digits). Check digit verification belongs to the business logic layer.";
  }
  if (w.find("正则无法仅凭字符格式识别校验位数值错误") != std::string::npos) {
    return "Negative example matches valid numeric format. Regular expressions cannot calculate weighted check digits; this belongs to application business logic.";
  }
  return w;
}

void update_ui_language(AppState* g, Language lang) {
  if (!g) return;
  g->language = lang;

  SetWindowTextW(g->main_hwnd, tr(lang, StrId::APP_TITLE));
  SetWindowTextW(g->reset_btn, tr(lang, StrId::RESET_BTN));
  SetWindowTextW(g->save_btn, tr(lang, StrId::SAVE_BTN));
  SetWindowTextW(g->guide_btn, tr(lang, StrId::GUIDE_BTN));
  SetWindowTextW(g->lang_btn, tr(lang, StrId::LANG_BTN));

  SendMessageW(g->project_combo, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(tr(lang, StrId::PROJECT_NAME_HINT)));

  SetWindowTextW(g->pos_label, tr(lang, StrId::POS_HEADER));
  SetWindowTextW(g->add_pos, tr(lang, StrId::ADD_POS));
  SetWindowTextW(g->batch_pos, tr(lang, StrId::BATCH_POS));
  const wchar_t* pos_cue = (lang == Language::EN_US ? L"e.g. PN8821@BN9012 or 1PMX-90021-B@Q1500" : L"如 PN8821@BN9012 或 1PMX-90021-B@Q1500");
  for (HWND h : g->pos_edits) {
    SendMessageW(h, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(pos_cue));
  }

  SetWindowTextW(g->neg_label, tr(lang, StrId::NEG_HEADER));
  SetWindowTextW(g->add_neg, tr(lang, StrId::ADD_NEG));
  SetWindowTextW(g->batch_neg, tr(lang, StrId::BATCH_NEG));
  const wchar_t* neg_cue = (lang == Language::EN_US ? L"e.g. invalid prefix / format error" : L"如 异常前缀 / 格式错误");
  for (HWND h : g->neg_edits) {
    SendMessageW(h, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(neg_cue));
  }

  SetWindowTextW(g->cfg_label, tr(lang, StrId::CFG_HEADER));
  SetWindowTextW(g->mode_auto, tr(lang, StrId::MODE_AUTO));
  SetWindowTextW(g->mode_sep, tr(lang, StrId::MODE_SEP));
  SetWindowTextW(g->mode_len, tr(lang, StrId::MODE_LEN));

  SetWindowTextW(g->sep_label, tr(lang, StrId::SEP_LABEL));
  SendMessageW(g->sep_edit, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(tr(lang, StrId::SEP_HINT)));
  SetWindowTextW(g->detect_sep_btn, tr(lang, StrId::DETECT_SEP_BTN));
  SetWindowTextW(g->count_label, tr(lang, StrId::COUNT_LABEL));

  SetWindowTextW(g->generate_btn, tr(lang, StrId::GENERATE_BTN));

  SetWindowTextW(g->rec_header_label, tr(lang, StrId::REC_HEADER));
  SetWindowTextW(g->copy_btn, tr(lang, StrId::COPY_REGEX_BTN));
  SetWindowTextW(g->copy_cap_btn, tr(lang, StrId::COPY_CAP_BTN));
  SendMessageW(g->rec_regex_edit, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(tr(lang, StrId::REC_REGEX_HINT)));

  SetWindowTextW(g->result_header_label, tr(lang, StrId::RESULT_HEADER));

  // Refresh candidate combo and result text if candidates exist
  if (g->candidate_combo && !g->last_candidates.empty()) {
    int cur = static_cast<int>(SendMessageW(g->candidate_combo, CB_GETCURSEL, 0, 0));
    SendMessageW(g->candidate_combo, CB_RESETCONTENT, 0, 0);
    bool has_passing = g->last_has_passing;
    for (size_t i = 0; i < g->last_candidates.size(); ++i) {
      const auto& cv = g->last_candidates[i];
      std::string name = get_candidate_display_name(cv.candidate, lang);
      std::wstring item = std::to_wstring(i + 1) + L". " + utf8_to_wide(name);
      if (lang == Language::EN_US) {
        if (cv.passes_all) item += L" [PASS ✓]";
        else item += L" [FAILED ✗]";
        if (i == 0 && has_passing) item += L" (Recommended)";
      } else {
        if (cv.passes_all) item += L" [通过 ✓]";
        else item += L" [未全通过 ✗]";
        if (i == 0 && has_passing) item += L" (推荐)";
      }
      SendMessageW(g->candidate_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
    }
    if (cur >= 0 && cur < static_cast<int>(g->last_candidates.size())) {
      SendMessageW(g->candidate_combo, CB_SETCURSEL, cur, 0);
    } else {
      SendMessageW(g->candidate_combo, CB_SETCURSEL, 0, 0);
    }

    std::string text;
    if (!g->last_warnings.empty()) {
      text += (lang == Language::EN_US ? "[WARNINGS]\r\n" : "【警告提示】\r\n");
      for (const auto& w : g->last_warnings) text += "- " + translate_warning(w, lang) + "\r\n";
      text += "\r\n";
    }

    if (!has_passing) {
      text += (lang == Language::EN_US
                   ? "⚠ NOTICE: No candidate matched all positives & rejected all negatives. The closest candidates are listed below for diagnostics:\r\n\r\n"
                   : "⚠ 注意：没有任何候选通过全部正例/反例。以下候选未完全通过，仅按接近程度排序供定位问题：\r\n\r\n");
    }

    for (size_t i = 0; i < g->last_candidates.size(); ++i) {
      text += format_candidate_block(g->last_candidates[i], i, i == 0 && has_passing, lang);
    }
    SetWindowTextW(g->result_edit, utf8_to_wide(text).c_str());
  } else {
    std::wstring res_text = get_edit_text(g->result_edit);
    if (res_text.empty() || res_text == tr(Language::ZH_CN, StrId::RESULT_DEFAULT_HINT) ||
        res_text == tr(Language::EN_US, StrId::RESULT_DEFAULT_HINT)) {
      SetWindowTextW(g->result_edit, tr(lang, StrId::RESULT_DEFAULT_HINT));
    }
  }

  SetWindowTextW(g->selftest_title, tr(lang, StrId::SELFTEST_TITLE));
  SendMessageW(g->selftest_edit, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(tr(lang, StrId::SELFTEST_HINT)));
  SetWindowTextW(g->selftest_btn, tr(lang, StrId::SELFTEST_BTN));

  // Self-test label refresh
  if (g->has_selftest_candidate && !get_edit_text(g->selftest_edit).empty()) {
    run_selftest(g->main_hwnd);
  } else {
    SetWindowTextW(g->selftest_label, g->has_selftest_candidate ? tr(lang, StrId::SELFTEST_IDLE)
                                                               : tr(lang, StrId::SELFTEST_NEED_GEN));
  }

  set_status(g->main_hwnd, tr(lang, StrId::STATUS_READY));
  save_app_config(g);
  layout(g->main_hwnd);
  InvalidateRect(g->main_hwnd, nullptr, TRUE);
  if (g->scroll_container) InvalidateRect(g->scroll_container, nullptr, TRUE);
  if (g->wizard_active && g->wizard_overlay) InvalidateRect(g->wizard_overlay, nullptr, TRUE);
}

void set_wizard_step(AppState* g, int step) {
  if (!g) return;
  if (step < 0) step = 0;
  if (step >= 5) {
    finish_wizard(g);
    return;
  }
  g->wizard_step = step;
  g->wizard_active = true;

  // Auto scroll child container to make sure the target is in view
  if (step == 0) {
    g->scroll_pos = 0;
    layout_scroll_content(g->scroll_container);
  } else if (step == 1) {
    g->scroll_pos = g->scale_dpi(60);
    layout_scroll_content(g->scroll_container);
  } else if (step == 2) {
    g->scroll_pos = g->scale_dpi(120);
    layout_scroll_content(g->scroll_container);
  }

  capture_wizard_background(g);

  if (g->wizard_overlay) {
    ShowWindow(g->wizard_overlay, SW_SHOW);
    BringWindowToTop(g->wizard_overlay);
    InvalidateRect(g->wizard_overlay, nullptr, TRUE);
  }
}

void finish_wizard(AppState* g) {
  if (!g) return;
  g->wizard_active = false;
  g->wizard_shown = true;
  save_app_config(g);
  if (g->wizard_overlay) {
    ShowWindow(g->wizard_overlay, SW_HIDE);
  }
  InvalidateRect(g->main_hwnd, nullptr, TRUE);
}

void start_wizard(AppState* g) {
  if (!g) return;
  set_wizard_step(g, 0);
}

// Helpers for mode
bool is_mode_auto(const AppState* g) {
  return SendMessageW(g->mode_auto, BM_GETCHECK, 0, 0) == BST_CHECKED;
}
bool is_mode_separators(const AppState* g) {
  return SendMessageW(g->mode_sep, BM_GETCHECK, 0, 0) == BST_CHECKED;
}
bool is_mode_lengths(const AppState* g) {
  return SendMessageW(g->mode_len, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void set_mode(AppState* g, int mode) {
  SendMessageW(g->mode_auto, BM_SETCHECK, mode == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g->mode_sep, BM_SETCHECK, mode == 1 ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g->mode_len, BM_SETCHECK, mode == 2 ? BST_CHECKED : BST_UNCHECKED, 0);

  ShowWindow(g->sep_label, mode == 1 ? SW_SHOW : SW_HIDE);
  ShowWindow(g->sep_edit, mode == 1 ? SW_SHOW : SW_HIDE);
  ShowWindow(g->detect_sep_btn, mode == 1 ? SW_SHOW : SW_HIDE);

  ShowWindow(g->count_label, mode == 2 ? SW_SHOW : SW_HIDE);
  ShowWindow(g->seg_count_combo, mode == 2 ? SW_SHOW : SW_HIDE);
  for (HWND h : g->length_boxes) {
    ShowWindow(h, mode == 2 ? SW_SHOW : SW_HIDE);
  }
}

void rebuild_length_boxes(HWND hwnd) {
  auto g = get_app_state(hwnd);
  if (!g) return;
  for (HWND h : g->length_boxes) {
    DestroyWindow(h);
  }
  g->length_boxes.clear();

  int sel = static_cast<int>(SendMessageW(g->seg_count_combo, CB_GETCURSEL, 0, 0));
  int count = (sel >= 0) ? (sel + 1) : 2;
  if (count < 1) count = 1;
  if (count > 10) count = 10;

  for (int i = 0; i < count; ++i) {
    HWND h = make_edit(g->scroll_container, IDC_LEN_BASE + i, 0, 0, 0, 0, 0, L"4");
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(g->mono_font), TRUE);
    g->length_boxes.push_back(h);
  }
  set_mode(g, is_mode_lengths(g) ? 2 : (is_mode_separators(g) ? 1 : 0));
  layout_scroll_content(g->scroll_container);
}

std::vector<std::size_t> parse_lengths(const AppState* g) {
  std::vector<std::size_t> lens;
  for (HWND h : g->length_boxes) {
    std::string s = wide_to_utf8(get_edit_text(h));
    std::size_t val = 0;
    if (parse_size_t(s, val) && val > 0) {
      lens.push_back(val);
    }
  }
  return lens;
}

std::vector<std::string> parse_separators(const AppState* g) {
  std::string raw = wide_to_utf8(get_edit_text(g->sep_edit));
  std::vector<std::string> seps;
  std::string cur;
  for (char c : raw) {
    if (c == ',' || c == ';') {
      std::string t = regex_tool::trim(cur);
      if (!t.empty()) seps.push_back(t);
      cur.clear();
    } else {
      cur += c;
    }
  }
  std::string t = regex_tool::trim(cur);
  if (!t.empty()) seps.push_back(t);
  return seps;
}

std::vector<std::string> collect_edits(const std::vector<HWND>& edits) {
  std::vector<std::string> list;
  for (HWND h : edits) {
    std::wstring w = get_edit_text(h);
    std::string s = regex_tool::trim(wide_to_utf8(w));
    if (!s.empty()) list.push_back(s);
  }
  return list;
}

void clear_example_rows(AppState* g, bool negative) {
  auto& edits = negative ? g->neg_edits : g->pos_edits;
  auto& dels = negative ? g->neg_dels : g->pos_dels;
  for (HWND h : edits) DestroyWindow(h);
  for (HWND h : dels) DestroyWindow(h);
  edits.clear();
  dels.clear();
}

void add_example_row(HWND hwnd, bool negative) {
  auto g = get_app_state(hwnd);
  if (!g) return;

  auto& edits = negative ? g->neg_edits : g->pos_edits;
  auto& dels = negative ? g->neg_dels : g->pos_dels;
  int base_id = negative ? IDC_NEG_EDIT_BASE : IDC_POS_EDIT_BASE;
  int del_base_id = negative ? IDC_NEG_DEL_BASE : IDC_POS_DEL_BASE;

  if (edits.size() >= 500) return;
  int idx = static_cast<int>(edits.size());

  HWND edit = make_edit(g->scroll_container, base_id + idx, 0, 0, 0, 0, 0,
                        negative ? (g->language == Language::EN_US ? L"e.g. invalid prefix / format error" : L"如 异常前缀 / 格式错误")
                                 : (g->language == Language::EN_US ? L"e.g. PN8821@BN9012 or 1PMX-90021-B@Q1500" : L"如 PN8821@BN9012 或 1PMX-90021-B@Q1500"));
  SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(g->mono_font), TRUE);

  HWND del = make_button(g->scroll_container, L"✕", del_base_id + idx, 0, 0, 0, 0);
  SendMessageW(del, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);

  edits.push_back(edit);
  dels.push_back(del);

  layout_scroll_content(g->scroll_container);
}

void remove_example_row(HWND hwnd, bool negative, HWND del_btn) {
  auto g = get_app_state(hwnd);
  if (!g) return;

  auto& edits = negative ? g->neg_edits : g->pos_edits;
  auto& dels = negative ? g->neg_dels : g->pos_dels;

  auto it = std::find(dels.begin(), dels.end(), del_btn);
  if (it != dels.end()) {
    size_t idx = std::distance(dels.begin(), it);
    DestroyWindow(edits[idx]);
    DestroyWindow(dels[idx]);
    edits.erase(edits.begin() + idx);
    dels.erase(dels.begin() + idx);
  }

  if (edits.empty()) {
    add_example_row(hwnd, negative);
  } else {
    layout_scroll_content(g->scroll_container);
  }
}

// Batch Paste Dialog Context
struct BatchCtx {
  std::wstring text;
  bool ok = false;
};

LRESULT CALLBACK batch_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  constexpr int BATCH_EDIT = 1, BATCH_OK = 2, BATCH_CANCEL = 3;
  auto g = reinterpret_cast<AppState*>(GetWindowLongPtrW(g_main_hwnd, GWLP_USERDATA));
  Language lang = g ? g->language : Language::ZH_CN;

  switch (msg) {
    case WM_CREATE: {
      SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(
                             reinterpret_cast<LPCREATESTRUCTW>(lParam)->lpCreateParams));
      RECT rc;
      GetClientRect(hwnd, &rc);
      auto sc = [g](int val) { return g ? g->scale_dpi(val) : val; };

      int margin_x = sc(16);
      int label_y = sc(14);
      int label_h = sc(20);
      int edit_y = label_y + label_h + sc(6);
      int btn_h = sc(28);
      int btn_w = sc(96);
      int cancel_w = sc(84);
      int btn_gap = sc(10);
      int btn_y = rc.bottom - sc(14) - btn_h;
      int edit_h = btn_y - edit_y - sc(12);

      HWND label = CreateWindowExW(0, L"STATIC", tr(lang, StrId::BATCH_DLG_HINT),
                                  WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                                  margin_x, label_y, rc.right - margin_x * 2, label_h, hwnd, nullptr,
                                  GetModuleHandleW(nullptr), nullptr);
      HFONT font = g && g->font ? g->font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
      SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

      HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                  WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN,
                                  margin_x, edit_y, rc.right - margin_x * 2, edit_h, hwnd,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(BATCH_EDIT)),
                                  GetModuleHandleW(nullptr), nullptr);
      HFONT mono_f = g && g->mono_font ? g->mono_font : font;
      SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(mono_f), TRUE);

      int ok_x = rc.right - margin_x - cancel_w - btn_gap - btn_w;
      int cancel_x = rc.right - margin_x - cancel_w;

      HWND ok = CreateWindowExW(0, L"BUTTON", tr(lang, StrId::BATCH_DLG_OK), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                ok_x, btn_y, btn_w, btn_h, hwnd,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(BATCH_OK)),
                                GetModuleHandleW(nullptr), nullptr);
      HWND cancel = CreateWindowExW(0, L"BUTTON", tr(lang, StrId::BATCH_DLG_CANCEL), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                    cancel_x, btn_y, cancel_w, btn_h, hwnd,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(BATCH_CANCEL)),
                                    GetModuleHandleW(nullptr), nullptr);
      SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
      SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
      SetFocus(edit);
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
      int id = LOWORD(wParam);
      if (id == BATCH_OK || id == BATCH_CANCEL) {
        auto ctx = reinterpret_cast<BatchCtx*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (id == BATCH_OK && ctx) {
          HWND edit = GetDlgItem(hwnd, BATCH_EDIT);
          int len = GetWindowTextLengthW(edit);
          if (len > 0) {
            std::vector<wchar_t> buf(static_cast<size_t>(len) + 1, L'\0');
            GetWindowTextW(edit, buf.data(), len + 1);
            ctx->text = buf.data();
          }
          ctx->ok = true;
        }
        DestroyWindow(hwnd);
        return 0;
      }
      return 0;
    }
    case WM_CLOSE: {
      DestroyWindow(hwnd);
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
      std::string t = regex_tool::trim(wide_to_utf8(cur));
      if (!t.empty()) lines.push_back(t);
      cur.clear();
    } else if (c != L'\r') {
      cur += c;
    }
  }
  std::string t = regex_tool::trim(wide_to_utf8(cur));
  if (!t.empty()) lines.push_back(t);
  return lines;
}

void run_batch_dialog(HWND owner, bool negative) {
  auto g = get_app_state(owner);
  Language lang = g ? g->language : Language::ZH_CN;

  BatchCtx ctx;
  int client_w = g ? g->scale_dpi(540) : 540;
  int client_h = g ? g->scale_dpi(380) : 380;

  DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
  DWORD ex_style = WS_EX_DLGMODALFRAME;

  RECT rc_client{ 0, 0, client_w, client_h };
  AdjustWindowRectEx(&rc_client, style, FALSE, ex_style);
  int dlg_w = rc_client.right - rc_client.left;
  int dlg_h = rc_client.bottom - rc_client.top;

  RECT rc_owner;
  GetWindowRect(owner, &rc_owner);
  int dlg_x = rc_owner.left + (rc_owner.right - rc_owner.left - dlg_w) / 2;
  int dlg_y = rc_owner.top + (rc_owner.bottom - rc_owner.top - dlg_h) / 2;

  HWND dlg = CreateWindowExW(ex_style, L"RegexToolBatch",
                             negative ? tr(lang, StrId::BATCH_DLG_NEG) : tr(lang, StrId::BATCH_DLG_POS),
                             style,
                             dlg_x, dlg_y, dlg_w, dlg_h, owner, nullptr,
                             GetModuleHandleW(nullptr), &ctx);
  EnableWindow(owner, FALSE);

  MSG msg;
  while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0)) {
    if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
      DestroyWindow(dlg);
      break;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  EnableWindow(owner, TRUE);
  SetForegroundWindow(owner);

  if (ctx.ok && !ctx.text.empty() && g) {
    auto lines = split_lines(ctx.text);
    if (!lines.empty()) {
      clear_example_rows(g, negative);
      auto& edits = negative ? g->neg_edits : g->pos_edits;
      for (const auto& line : lines) {
        add_example_row(owner, negative);
        SetWindowTextW(edits.back(), utf8_to_wide(line).c_str());
      }
      layout_scroll_content(g->scroll_container);
      std::wstring st = (negative ? L"反例" : L"正例");
      if (lang == Language::EN_US) st = (negative ? L"negatives" : L"positives");
      set_status(owner, (lang == Language::EN_US ? L"Batch imported " : L"已批量导入 ") +
                          std::to_wstring(lines.size()) + L" " + st);
    }
  }
}

// Modal Save Profile Dialog
struct SaveDlgCtx {
  std::wstring name;
  bool ok = false;
};

LRESULT CALLBACK save_dlg_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  constexpr int DLG_EDIT = 1, DLG_OK = 2, DLG_CANCEL = 3;
  auto g = reinterpret_cast<AppState*>(GetWindowLongPtrW(g_main_hwnd, GWLP_USERDATA));
  Language lang = g ? g->language : Language::ZH_CN;

  switch (msg) {
    case WM_CREATE: {
      SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(
                             reinterpret_cast<LPCREATESTRUCTW>(lParam)->lpCreateParams));
      auto ctx = reinterpret_cast<SaveDlgCtx*>(
          reinterpret_cast<LPCREATESTRUCTW>(lParam)->lpCreateParams);

      RECT rc;
      GetClientRect(hwnd, &rc);
      auto sc = [g](int val) { return g ? g->scale_dpi(val) : val; };

      int margin_x = sc(18);
      int label_y = sc(16);
      int label_h = sc(20);
      int edit_y = label_y + label_h + sc(8);
      int edit_h = sc(26);
      int edit_w = rc.right - margin_x * 2;

      HWND label = CreateWindowExW(0, L"STATIC", tr(lang, StrId::SAVE_DLG_PROMPT),
                                  WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                                  margin_x, label_y, edit_w, label_h, hwnd, nullptr,
                                  GetModuleHandleW(nullptr), nullptr);
      HFONT font = g && g->font ? g->font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
      SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

      HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", ctx ? ctx->name.c_str() : L"",
                                  WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                                  margin_x, edit_y, edit_w, edit_h, hwnd,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(DLG_EDIT)),
                                  GetModuleHandleW(nullptr), nullptr);
      SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
      SendMessageW(edit, EM_SETSEL, 0, -1);

      int btn_w = sc(78);
      int btn_h = sc(28);
      int btn_gap = sc(10);
      int btn_y = edit_y + edit_h + sc(18);
      int ok_x = rc.right - margin_x - btn_w * 2 - btn_gap;
      int cancel_x = rc.right - margin_x - btn_w;

      HWND ok = CreateWindowExW(0, L"BUTTON", tr(lang, StrId::SAVE_DLG_OK),
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                ok_x, btn_y, btn_w, btn_h, hwnd,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(DLG_OK)),
                                GetModuleHandleW(nullptr), nullptr);
      HWND cancel = CreateWindowExW(0, L"BUTTON", tr(lang, StrId::SAVE_DLG_CANCEL),
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                    cancel_x, btn_y, btn_w, btn_h, hwnd,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(DLG_CANCEL)),
                                    GetModuleHandleW(nullptr), nullptr);
      SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
      SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
      SetFocus(edit);
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
      int id = LOWORD(wParam);
      if (id == DLG_OK || id == DLG_CANCEL) {
        auto ctx = reinterpret_cast<SaveDlgCtx*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (id == DLG_OK && ctx) {
          HWND edit = GetDlgItem(hwnd, DLG_EDIT);
          ctx->name = get_edit_text(edit);
          ctx->ok = true;
        }
        DestroyWindow(hwnd);
        return 0;
      }
      return 0;
    }
    case WM_CLOSE: {
      DestroyWindow(hwnd);
      return 0;
    }
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool run_save_dialog(HWND owner, std::wstring& out_name) {
  auto g = get_app_state(owner);
  Language lang = g ? g->language : Language::ZH_CN;

  SaveDlgCtx ctx;
  ctx.name = out_name.empty() ? L"project-1" : out_name;

  int client_w = g ? g->scale_dpi(380) : 380;
  int client_h = g ? g->scale_dpi(145) : 145;

  DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
  DWORD ex_style = WS_EX_DLGMODALFRAME;

  RECT rc_client{ 0, 0, client_w, client_h };
  AdjustWindowRectEx(&rc_client, style, FALSE, ex_style);
  int dlg_w = rc_client.right - rc_client.left;
  int dlg_h = rc_client.bottom - rc_client.top;

  RECT rc_owner;
  GetWindowRect(owner, &rc_owner);
  int dlg_x = rc_owner.left + (rc_owner.right - rc_owner.left - dlg_w) / 2;
  int dlg_y = rc_owner.top + (rc_owner.bottom - rc_owner.top - dlg_h) / 2;

  HWND dlg = CreateWindowExW(ex_style, L"RegexToolSaveDlg",
                             tr(lang, StrId::SAVE_DLG_TITLE),
                             style,
                             dlg_x, dlg_y, dlg_w, dlg_h, owner, nullptr,
                             GetModuleHandleW(nullptr), &ctx);
  EnableWindow(owner, FALSE);

  MSG msg;
  while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0)) {
    if (msg.message == WM_KEYDOWN) {
      if (msg.wParam == VK_ESCAPE) {
        DestroyWindow(dlg);
        break;
      } else if (msg.wParam == VK_RETURN) {
        HWND edit = GetDlgItem(dlg, 1);
        ctx.name = get_edit_text(edit);
        ctx.ok = true;
        DestroyWindow(dlg);
        break;
      }
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  EnableWindow(owner, TRUE);
  SetForegroundWindow(owner);

  if (ctx.ok && !ctx.name.empty()) {
    std::wstring clean;
    for (wchar_t c : ctx.name) {
      if (c != L'\\' && c != L'/' && c != L':' && c != L'*' && c != L'?' && c != L'"' && c != L'<' && c != L'>' && c != L'|') {
        clean += c;
      }
    }
    std::string s = regex_tool::trim(wide_to_utf8(clean));
    if (!s.empty()) {
      out_name = utf8_to_wide(s);
      return true;
    }
  }
  return false;
}

std::string format_candidate_block(const regex_tool::CandidateVerdict& vd, size_t index, bool recommended, Language lang) {
  std::string tag = get_candidate_display_name(vd.candidate, lang);
  std::string out;
  if (lang == Language::EN_US) {
    if (recommended) {
      out += "======================================================================\r\n";
      out += "[RECOMMENDED CANDIDATE] " + tag + "\r\n";
    } else {
      out += "----------------------------------------------------------------------\r\n";
      out += "[CANDIDATE " + std::to_string(index + 1) + "] " + tag + "\r\n";
    }
    out += "Regex Pattern:\r\n  " + vd.candidate.regex + "\r\n";
    if (!vd.candidate.capturing_regex.empty()) {
      out += "Capturing Regex:\r\n  " + vd.candidate.capturing_regex + "\r\n";
    }
    out += "\r\n";

    if (vd.passes_all) {
      out += "Verdict: All Passed ✓ (" + std::to_string(vd.positive_verdicts.size()) + " positives matched, " +
             std::to_string(vd.negative_verdicts.size()) + " negatives rejected)\r\n\r\n";
    } else {
      out += "Verdict: ⚠ Not fully satisfied (" + std::to_string(vd.failures) + " mismatch(es))\r\n\r\n";
    }

    if (!vd.positive_verdicts.empty()) {
      out += "[Positive Match Details]\r\n";
      for (const auto& v : vd.positive_verdicts) {
        out += v.passed ? "  ✓ " : "  ✗ [FAIL] ";
        out += regex_tool::visualize_control_chars(v.example);
        if (!v.passed) {
          out += v.failing_segment != SIZE_MAX
                     ? " (Failed at segment " + std::to_string(v.failing_segment + 1) + ")"
                     : " (Whole pattern mismatch)";
        }
        out += "\r\n";
      }
      out += "\r\n";
    }

    if (!vd.negative_verdicts.empty()) {
      out += "[Negative Exclusion Details]\r\n";
      for (const auto& v : vd.negative_verdicts) {
        out += v.passed ? "  ✓ [REJECTED] " : "  ✗ [⚠ LEAK] ";
        out += regex_tool::visualize_control_chars(v.example);
        if (!v.passed && v.failing_segment != SIZE_MAX) {
          out += " (Failed at segment " + std::to_string(v.failing_segment + 1) + ")";
        }
        out += "\r\n";
      }
      out += "\r\n";
    }
  } else {
    // Chinese format
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
        out += regex_tool::visualize_control_chars(v.example);
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
        out += regex_tool::visualize_control_chars(v.example);
        if (!v.passed && v.failing_segment != SIZE_MAX) {
          out += " (卡在分段 " + std::to_string(v.failing_segment + 1) + ")";
        }
        out += "\r\n";
      }
      out += "\r\n";
    }
  }

  return out;
}

void run_generate(HWND hwnd) {
  auto g = get_app_state(hwnd);
  if (!g) return;
  Language lang = g->language;

  try {
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

    g->last_candidates = result.candidates;
    g->last_warnings = result.warnings;
    g->last_has_passing = result.has_passing;
    g->selected_candidate_index = 0;
    if (g->candidate_combo) SendMessageW(g->candidate_combo, CB_RESETCONTENT, 0, 0);

    if (result.candidates.empty()) {
      g->recommended_regex.clear();
      g->recommended_capturing_regex.clear();
      g->has_selftest_candidate = false;
      SetWindowTextW(g->rec_regex_edit, L"");
      SetWindowTextW(g->result_edit, tr(lang, StrId::RESULT_DEFAULT_HINT));
      SetWindowTextW(g->selftest_label, tr(lang, StrId::SELFTEST_IDLE));
      set_status(hwnd, tr(lang, StrId::PROMPT_NEED_POS));
    } else {
      for (size_t i = 0; i < result.candidates.size(); ++i) {
        const auto& cv = result.candidates[i];
        std::string name = get_candidate_display_name(cv.candidate, lang);
        std::wstring item = std::to_wstring(i + 1) + L". " + utf8_to_wide(name);
        if (lang == Language::EN_US) {
          if (cv.passes_all) item += L" [PASS ✓]";
          else item += L" [FAILED ✗]";
          if (i == 0 && result.has_passing) item += L" (Recommended)";
        } else {
          if (cv.passes_all) item += L" [通过 ✓]";
          else item += L" [未全通过 ✗]";
          if (i == 0 && result.has_passing) item += L" (推荐)";
        }
        if (g->candidate_combo) {
          SendMessageW(g->candidate_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
        }
      }
      if (g->candidate_combo) {
        SendMessageW(g->candidate_combo, CB_SETCURSEL, 0, 0);
      }

      const auto& best = result.candidates.front().candidate;
      g->recommended_regex = best.regex;
      g->recommended_capturing_regex = best.capturing_regex;
      g->selftest_candidate = best;
      g->has_selftest_candidate = true;
      SetWindowTextW(g->rec_regex_edit, utf8_to_wide(best.regex).c_str());

      std::string text;
      if (!result.warnings.empty()) {
        text += (lang == Language::EN_US ? "[WARNINGS]\r\n" : "【警告提示】\r\n");
        for (const auto& w : result.warnings) text += "- " + translate_warning(w, lang) + "\r\n";
        text += "\r\n";
      }

      if (!result.has_passing) {
        text += (lang == Language::EN_US
                     ? "⚠ NOTICE: No candidate matched all positives & rejected all negatives. The closest candidates are listed below for diagnostics:\r\n\r\n"
                     : "⚠ 注意：没有任何候选通过全部正例/反例。以下候选未完全通过，仅按接近程度排序供定位问题：\r\n\r\n");
      }

      for (size_t i = 0; i < result.candidates.size(); ++i) {
        text += format_candidate_block(result.candidates[i], i, i == 0 && result.has_passing, lang);
      }
      SetWindowTextW(g->result_edit, utf8_to_wide(text).c_str());

      std::wstring st = tr(lang, StrId::STATUS_GEN_DONE) + std::to_wstring(result.candidates.size()) +
                        (lang == Language::EN_US ? L" candidate(s)" : L" 条候选");
      if (result.has_passing) {
        st += (lang == Language::EN_US ? L" (Recommended regex ready)" : L"（默认推荐候选已就绪）");
      } else {
        st += (lang == Language::EN_US ? L" (⚠ Not all passed)" : L"（⚠ 未完全通过）");
      }
      set_status(hwnd, st);
    }

    g->last_segments = result.candidates.empty()
                          ? std::vector<regex_tool::SegmentRule>{}
                          : result.candidates.front().candidate.segments;
    layout_scroll_content(g->scroll_container);

    if (g->has_selftest_candidate && !get_edit_text(g->selftest_edit).empty()) {
      run_selftest(hwnd);
    } else {
      SetWindowTextW(g->selftest_label, g->has_selftest_candidate ? tr(lang, StrId::SELFTEST_IDLE)
                                                                 : tr(lang, StrId::SELFTEST_NEED_GEN));
    }
  } catch (const std::exception& e) {
    std::string err_msg = (lang == Language::EN_US ? "Generation Error: " : "生成异常: ");
    err_msg += e.what();
    SetWindowTextW(g->rec_regex_edit, L"");
    SetWindowTextW(g->result_edit, utf8_to_wide(err_msg).c_str());
    set_status(hwnd, tr(lang, StrId::STATUS_GEN_FAIL));
  }
}

void run_selftest(HWND hwnd) {
  auto g = get_app_state(hwnd);
  if (!g) return;
  Language lang = g->language;

  if (!g->has_selftest_candidate) {
    SetWindowTextW(g->selftest_label, tr(lang, StrId::SELFTEST_NEED_GEN));
    return;
  }
  std::string input = regex_tool::trim(wide_to_utf8(get_edit_text(g->selftest_edit)));
  if (input.empty()) {
    SetWindowTextW(g->selftest_label, tr(lang, StrId::SELFTEST_NEED_INPUT));
    return;
  }
  auto v = regex_tool::verify_example(g->selftest_candidate, input);
  std::wstring cand_name = utf8_to_wide(get_candidate_display_name(g->selftest_candidate, lang));
  std::wstring msg = v.passed ? tr(lang, StrId::SELFTEST_PASS) : tr(lang, StrId::SELFTEST_FAIL);
  msg += (lang == Language::EN_US ? L" [Candidate: " : L" [测试候选: ") + cand_name + L"]";

  if (!v.passed) {
    if (v.failing_segment != SIZE_MAX) {
      msg += (lang == Language::EN_US
                  ? (L" (Failed at segment " + std::to_wstring(v.failing_segment + 1) + L")")
                  : (L"（卡在分段 " + std::to_wstring(v.failing_segment + 1) + L"）"));
    } else {
      msg += (lang == Language::EN_US ? L" (Pattern mismatch)" : L"（整串不匹配）");
    }
  } else if (!v.field_captures.empty()) {
    msg += (lang == Language::EN_US ? L" [Captured: " : L" [捕获: ");
    for (size_t i = 0; i < v.field_captures.size(); ++i) {
      if (i > 0) msg += L", ";
      std::string desc = regex_tool::get_business_field_description(v.field_captures[i].first);
      if (!desc.empty() && desc != v.field_captures[i].first) {
        msg += utf8_to_wide(v.field_captures[i].first) + L" (" + utf8_to_wide(desc) + L")=" + utf8_to_wide(regex_tool::visualize_control_chars(v.field_captures[i].second));
      } else {
        msg += utf8_to_wide(v.field_captures[i].first) + L"=" + utf8_to_wide(regex_tool::visualize_control_chars(v.field_captures[i].second));
      }
    }
    msg += L"]";
  }
  SetWindowTextW(g->selftest_label, msg.c_str());
}

std::wstring project_name(const AppState* g) {
  std::wstring n = get_edit_text(g->project_combo);
  if (n.empty()) n = L"project-1";
  return n;
}

std::wstring projects_dir() {
  // Check cwd/projects
  wchar_t cwd_buf[MAX_PATH];
  if (GetCurrentDirectoryW(MAX_PATH, cwd_buf) > 0) {
    std::wstring cwd_proj = std::wstring(cwd_buf) + L"\\projects";
    DWORD attr = GetFileAttributesW(cwd_proj.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
      return cwd_proj;
    }
  }
  // Check exe_dir/projects
  std::wstring exe_proj = exe_dir() + L"\\projects";
  DWORD attr2 = GetFileAttributesW(exe_proj.c_str());
  if (attr2 != INVALID_FILE_ATTRIBUTES && (attr2 & FILE_ATTRIBUTE_DIRECTORY)) {
    return exe_proj;
  }
  // Check exe_dir/../../projects (if inside build/Release)
  std::wstring parent_proj = exe_dir() + L"\\..\\..\\projects";
  DWORD attr3 = GetFileAttributesW(parent_proj.c_str());
  if (attr3 != INVALID_FILE_ATTRIBUTES && (attr3 & FILE_ATTRIBUTE_DIRECTORY)) {
    return parent_proj;
  }

  CreateDirectoryW(exe_proj.c_str(), nullptr);
  return exe_proj;
}

std::vector<std::wstring> get_project_search_dirs() {
  std::vector<std::wstring> dirs;
  std::wstring p1 = projects_dir();
  dirs.push_back(p1);

  std::wstring exe_proj = exe_dir() + L"\\projects";
  if (std::find(dirs.begin(), dirs.end(), exe_proj) == dirs.end()) {
    dirs.push_back(exe_proj);
  }

  wchar_t cwd_buf[MAX_PATH];
  if (GetCurrentDirectoryW(MAX_PATH, cwd_buf) > 0) {
    std::wstring cwd_proj = std::wstring(cwd_buf) + L"\\projects";
    if (std::find(dirs.begin(), dirs.end(), cwd_proj) == dirs.end()) {
      dirs.push_back(cwd_proj);
    }
  }

  std::wstring parent_proj = exe_dir() + L"\\..\\..\\projects";
  if (std::find(dirs.begin(), dirs.end(), parent_proj) == dirs.end()) {
    dirs.push_back(parent_proj);
  }

  return dirs;
}

void populate_projects(AppState* g) {
  if (!g || !g->project_combo) return;
  SendMessageW(g->project_combo, CB_RESETCONTENT, 0, 0);

  auto dirs = get_project_search_dirs();
  std::vector<std::wstring> found_files;
  for (const auto& dir : dirs) {
    std::wstring pattern = dir + L"\\*.rgx";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
      do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
          std::wstring fn = fd.cFileName;
          if (fn.size() > 4 && fn.substr(fn.size() - 4) == L".rgx") {
            std::wstring base = fn.substr(0, fn.size() - 4);
            if (std::find(found_files.begin(), found_files.end(), base) == found_files.end()) {
              found_files.push_back(base);
              SendMessageW(g->project_combo, CB_ADDSTRING, 0,
                           reinterpret_cast<LPARAM>(base.c_str()));
            }
          }
        }
      } while (FindNextFileW(h, &fd));
      FindClose(h);
    }
  }

  if (!g->last_project_name.empty()) {
    int idx = static_cast<int>(SendMessageW(g->project_combo, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(g->last_project_name.c_str())));
    if (idx != CB_ERR) {
      SendMessageW(g->project_combo, CB_SETCURSEL, idx, 0);
    } else {
      SetWindowTextW(g->project_combo, g->last_project_name.c_str());
    }
  }
}

std::string serialize_project(const AppState* g, const std::wstring& name) {
  std::string out;
  out += "name=" + wide_to_utf8(name) + "\n";
  out += "mode=" + std::string(is_mode_auto(g) ? "auto" : (is_mode_separators(g) ? "separators" : "lengths")) + "\n";
  out += "separators=" + wide_to_utf8(get_edit_text(g->sep_edit)) + "\n";

  std::string len_str;
  for (size_t i = 0; i < g->length_boxes.size(); ++i) {
    if (i > 0) len_str += ",";
    len_str += wide_to_utf8(get_edit_text(g->length_boxes[i]));
  }
  out += "lengths=" + len_str + "\n";

  std::string prom_str;
  for (size_t i = 0; i < g->promote_tags.size(); ++i) {
    if (i > 0) prom_str += ",";
    prom_str += g->promote_tags[i];
  }
  out += "promote=" + prom_str + "\n";

  out += "selected_regex=" + g->recommended_regex + "\n";

  for (const auto& s : collect_edits(g->pos_edits)) {
    out += "pos=" + s + "\n";
  }
  for (const auto& s : collect_edits(g->neg_edits)) {
    out += "neg=" + s + "\n";
  }
  return out;
}

bool save_project(HWND hwnd, const std::wstring& name) {
  auto g = get_app_state(hwnd);
  if (!g) return false;
  std::wstring dir = projects_dir();
  std::wstring path = dir + L"\\" + name + L".rgx";

  std::ofstream f(path);
  if (!f.is_open()) {
    CreateDirectoryW(dir.c_str(), nullptr);
    f.open(path);
  }
  if (f.is_open()) {
    f << serialize_project(g, name);
    f.close();
    g->last_project_name = name;
    save_app_config(g);
    populate_projects(g);
    SetWindowTextW(g->project_combo, name.c_str());
    set_status(hwnd, tr(g->language, StrId::STATUS_SAVED) + name + L".rgx");
    return true;
  }
  return false;
}

bool load_project(HWND hwnd, const std::wstring& name, std::wstring& error) {
  auto g = get_app_state(hwnd);
  if (!g) return false;

  auto dirs = get_project_search_dirs();
  std::vector<std::wstring> candidate_paths;
  for (const auto& d : dirs) {
    candidate_paths.push_back(d + L"\\" + name + L".rgx");
  }
  candidate_paths.push_back(name + L".rgx");

  std::ifstream f;
  for (const auto& cp : candidate_paths) {
    f.open(cp);
    if (f.is_open()) break;
  }

  if (!f.is_open()) {
    error = tr(g->language, StrId::STATUS_LOAD_FAIL) + name + L".rgx";
    return false;
  }

  std::string line;
  std::string mode_str = "auto";
  std::wstring separators;
  std::wstring lengths_text;
  std::vector<std::string> promote;
  std::vector<regex_tool::FieldRule> overrides;
  std::vector<std::string> positives;
  std::vector<std::string> negatives;
  std::string selected;

  while (std::getline(f, line)) {
    line = regex_tool::trim(line);
    if (line.empty()) continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq + 1);

    if (key == "mode") mode_str = val;
    else if (key == "separators") separators = utf8_to_wide(val);
    else if (key == "lengths") lengths_text = utf8_to_wide(val);
    else if (key == "promote") {
      std::string cur;
      for (char c : val) {
        if (c == ',') {
          std::string t = regex_tool::trim(cur);
          if (!t.empty()) promote.push_back(t);
          cur.clear();
        } else cur += c;
      }
      std::string t = regex_tool::trim(cur);
      if (!t.empty()) promote.push_back(t);
    } else if (key == "selected_regex") selected = val;
    else if (key == "pos") positives.push_back(val);
    else if (key == "neg") negatives.push_back(val);
  }
  f.close();

  if (mode_str == "separators") set_mode(g, 1);
  else if (mode_str == "lengths") set_mode(g, 2);
  else set_mode(g, 0);

  SetWindowTextW(g->sep_edit, separators.c_str());
  if (!lengths_text.empty()) {
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

  g->last_project_name = name;
  save_app_config(g);
  SetWindowTextW(g->project_combo, name.c_str());
  run_generate(hwnd);
  set_status(hwnd, tr(g->language, StrId::STATUS_LOADED) + name);
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

  MoveWindow(g->pos_label, 0, y, g->scale_dpi(220), g->scale_dpi(18), TRUE);
  y += g->scale_dpi(22);
  for (size_t i = 0; i < g->pos_edits.size(); ++i) {
    MoveWindow(g->pos_edits[i], 0, y, edit_w, ctrl_h, TRUE);
    MoveWindow(g->pos_dels[i], edit_w + g->scale_dpi(4), y, del_w, ctrl_h, TRUE);
    y += row_h;
  }
  int btn_w = g->scale_dpi(92);
  MoveWindow(g->add_pos, 0, y, btn_w, ctrl_h, TRUE);
  MoveWindow(g->batch_pos, btn_w + g->scale_dpi(6), y, btn_w + g->scale_dpi(8), ctrl_h, TRUE);
  y += row_h + g->scale_dpi(8);

  MoveWindow(g->neg_label, 0, y, g->scale_dpi(220), g->scale_dpi(18), TRUE);
  y += g->scale_dpi(22);
  for (size_t i = 0; i < g->neg_edits.size(); ++i) {
    MoveWindow(g->neg_edits[i], 0, y, edit_w, ctrl_h, TRUE);
    MoveWindow(g->neg_dels[i], edit_w + g->scale_dpi(4), y, del_w, ctrl_h, TRUE);
    y += row_h;
  }
  MoveWindow(g->add_neg, 0, y, btn_w, ctrl_h, TRUE);
  MoveWindow(g->batch_neg, btn_w + g->scale_dpi(6), y, btn_w + g->scale_dpi(8), ctrl_h, TRUE);
  y += row_h + g->scale_dpi(8);

  MoveWindow(g->cfg_label, 0, y, g->scale_dpi(180), g->scale_dpi(18), TRUE);
  y += g->scale_dpi(22);
  int radio_w = g->scale_dpi(96);
  MoveWindow(g->mode_auto, 0, y, radio_w, ctrl_h, TRUE);
  MoveWindow(g->mode_sep, radio_w + g->scale_dpi(4), y, radio_w + g->scale_dpi(12), ctrl_h, TRUE);
  MoveWindow(g->mode_len, (radio_w * 2) + g->scale_dpi(20), y, radio_w, ctrl_h, TRUE);
  y += row_h;

  if (is_mode_separators(g)) {
    MoveWindow(g->sep_label, 0, y, g->scale_dpi(64), ctrl_h, TRUE);
    int detect_w = g->scale_dpi(80);
    int edit_sep_w = edit_w - g->scale_dpi(68) - detect_w - g->scale_dpi(4);
    MoveWindow(g->sep_edit, g->scale_dpi(66), y, edit_sep_w, ctrl_h, TRUE);
    MoveWindow(g->detect_sep_btn, g->scale_dpi(66) + edit_sep_w + g->scale_dpi(4), y, detect_w, ctrl_h, TRUE);
    y += row_h;
  } else if (is_mode_lengths(g)) {
    MoveWindow(g->count_label, 0, y, g->scale_dpi(50), ctrl_h, TRUE);
    MoveWindow(g->seg_count_combo, g->scale_dpi(54), y, g->scale_dpi(66), ctrl_h, TRUE);
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
}

void layout(HWND hwnd) {
  auto g = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (!g) return;

  RECT rc;
  GetClientRect(hwnd, &rc);
  int win_w = rc.right;
  int win_h = rc.bottom;
  if (win_w <= 0 || win_h <= 0) return;

  // Status bar layout
  SendMessageW(g->status_bar, WM_SIZE, 0, 0);
  RECT status_rc{};
  GetWindowRect(g->status_bar, &status_rc);
  int status_h = status_rc.bottom - status_rc.top;

  int left_margin = g->scale_dpi(BASE_LEFT_MARGIN);
  int ctrl_h = g->scale_dpi(BASE_CTRL_H);

  // Responsive column division (~44% left, ~56% right) (P2-7)
  int left_pane_w = std::clamp(static_cast<int>(win_w * 0.44), g->scale_dpi(380), win_w - g->scale_dpi(380));
  int right_x = left_pane_w + g->scale_dpi(24);
  int right_pane_w = std::max(win_w - right_x - left_margin, g->scale_dpi(300));

  // 1. Left Pane: Project header bar (Fixed at top)
  int y_top = g->scale_dpi(12);
  int save_w = g->scale_dpi(52);
  int reset_w = (g->language == Language::ZH_CN) ? g->scale_dpi(68) : g->scale_dpi(54);
  int guide_w = g->scale_dpi(50);
  int lang_w = g->scale_dpi(50);
  int total_btns_w = save_w + reset_w + guide_w + lang_w + g->scale_dpi(14);
  int combo_w = std::max(left_pane_w - total_btns_w - g->scale_dpi(6), g->scale_dpi(110));

  MoveWindow(g->project_combo, left_margin, y_top, combo_w, ctrl_h, TRUE);
  int btn_x = left_margin + combo_w + g->scale_dpi(6);
  MoveWindow(g->save_btn, btn_x, y_top, save_w, ctrl_h, TRUE);
  btn_x += save_w + g->scale_dpi(3);
  MoveWindow(g->reset_btn, btn_x, y_top, reset_w, ctrl_h, TRUE);
  btn_x += reset_w + g->scale_dpi(3);
  MoveWindow(g->guide_btn, btn_x, y_top, guide_w, ctrl_h, TRUE);
  btn_x += guide_w + g->scale_dpi(3);
  MoveWindow(g->lang_btn, btn_x, y_top, lang_w, ctrl_h, TRUE);

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

  // 4.1 Candidate Selection Header & Copy Buttons
  int copy_btn_w1 = g->scale_dpi(80);
  int copy_btn_w2 = g->scale_dpi(80);
  int right_btn_w = copy_btn_w1 + copy_btn_w2 + g->scale_dpi(6);
  int cand_lbl_w = g->scale_dpi(72);
  int cand_combo_w = std::max(right_pane_w - cand_lbl_w - right_btn_w - g->scale_dpi(12), g->scale_dpi(140));

  MoveWindow(g->rec_header_label, right_x, r_top + g->scale_dpi(3), cand_lbl_w, g->scale_dpi(20), TRUE);
  MoveWindow(g->candidate_combo, right_x + cand_lbl_w, r_top, cand_combo_w, ctrl_h, TRUE);
  MoveWindow(g->copy_btn, right_x + right_pane_w - right_btn_w, r_top, copy_btn_w1, ctrl_h, TRUE);
  MoveWindow(g->copy_cap_btn, right_x + right_pane_w - copy_btn_w2, r_top, copy_btn_w2, ctrl_h, TRUE);

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

  // 5. Wizard Overlay Size
  if (g->wizard_overlay) {
    MoveWindow(g->wizard_overlay, 0, 0, win_w, win_h, TRUE);
    if (g->wizard_active) {
      InvalidateRect(g->wizard_overlay, nullptr, TRUE);
    }
  }
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
        if (g->wizard_active && g->wizard_overlay) {
          InvalidateRect(g->wizard_overlay, nullptr, TRUE);
        }
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
        if (g->wizard_active && g->wizard_overlay) {
          InvalidateRect(g->wizard_overlay, nullptr, TRUE);
        }
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
      HWND parent = GetParent(hwnd);
      return SendMessageW(parent ? parent : g_main_hwnd, WM_COMMAND, wParam, lParam);
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
      load_app_config(g);

      update_theme_titlebar(hwnd);

      // Status Bar
      g->status_bar = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
                                       WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                       0, 0, 0, 0, hwnd,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUSBAR)),
                                       GetModuleHandleW(nullptr), nullptr);
      SendMessageW(g->status_bar, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);

      // 1. Top Fixed Project Header
      g->project_combo = make_combo(hwnd, IDC_PROJECT_EDIT, 0, 0, 0, 0, true, tr(g->language, StrId::PROJECT_NAME_HINT));
      g->save_btn = make_button(hwnd, tr(g->language, StrId::SAVE_BTN), IDC_SAVE, 0, 0, 0, 0);
      g->reset_btn = make_button(hwnd, tr(g->language, StrId::RESET_BTN), IDC_RESET, 0, 0, 0, 0);
      g->guide_btn = make_button(hwnd, tr(g->language, StrId::GUIDE_BTN), IDC_GUIDE_BTN, 0, 0, 0, 0);
      g->lang_btn = make_button(hwnd, tr(g->language, StrId::LANG_BTN), IDC_LANG_BTN, 0, 0, 0, 0);

      // 2. Bottom Fixed Generate Action Button
      g->generate_btn = make_button(hwnd, tr(g->language, StrId::GENERATE_BTN), IDC_GENERATE, 0, 0, 0, 0, BS_OWNERDRAW);

      // 3. Isolated Scroll Viewport (Child Container with Hardware Clipping)
      g->scroll_container = CreateWindowExW(0, L"RegexToolScrollArea", nullptr,
                                            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VSCROLL,
                                            0, 0, 0, 0, hwnd,
                                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SCROLL_CONTAINER)),
                                            GetModuleHandleW(nullptr), nullptr);

      // 4. Children inside the Scroll Viewport
      HWND sc = g->scroll_container;
      g->pos_label = make_static(sc, tr(g->language, StrId::POS_HEADER), 0, 0, 0, 0, true);
      g->add_pos = make_button(sc, tr(g->language, StrId::ADD_POS), IDC_ADD_POS, 0, 0, 0, 0);
      g->batch_pos = make_button(sc, tr(g->language, StrId::BATCH_POS), IDC_BATCH_POS, 0, 0, 0, 0);

      g->neg_label = make_static(sc, tr(g->language, StrId::NEG_HEADER), 0, 0, 0, 0, true);
      g->add_neg = make_button(sc, tr(g->language, StrId::ADD_NEG), IDC_ADD_NEG, 0, 0, 0, 0);
      g->batch_neg = make_button(sc, tr(g->language, StrId::BATCH_NEG), IDC_BATCH_NEG, 0, 0, 0, 0);

      g->cfg_label = make_static(sc, tr(g->language, StrId::CFG_HEADER), 0, 0, 0, 0, true);
      g->mode_auto = make_radio(sc, tr(g->language, StrId::MODE_AUTO), IDC_MODE_AUTO, 0, 0, 0, 0, true);
      g->mode_sep = make_radio(sc, tr(g->language, StrId::MODE_SEP), IDC_MODE_SEP, 0, 0, 0, 0, false);
      g->mode_len = make_radio(sc, tr(g->language, StrId::MODE_LEN), IDC_MODE_LEN, 0, 0, 0, 0, false);

      g->sep_label = make_static(sc, tr(g->language, StrId::SEP_LABEL), 0, 0, 0, 0);
      g->sep_edit = make_edit(sc, IDC_SEP_EDIT, 0, 0, 0, 0, 0, tr(g->language, StrId::SEP_HINT));
      g->detect_sep_btn = make_button(sc, tr(g->language, StrId::DETECT_SEP_BTN), IDC_DETECT_SEP, 0, 0, 0, 0);
      g->count_label = make_static(sc, tr(g->language, StrId::COUNT_LABEL), 0, 0, 0, 0);
      g->seg_count_combo = make_combo(sc, IDC_SEG_COUNT_COMBO, 0, 0, 0, 0, false);
      for (int i = 1; i <= 10; ++i) {
        std::wstring t = std::to_wstring(i);
        SendMessageW(g->seg_count_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t.c_str()));
      }
      SendMessageW(g->seg_count_combo, CB_SETCURSEL, 1, 0);

      set_mode(g, 0);

      // 5. Right Pane Controls (Dedicated Candidate Selector + Highlight Regex Box + Details Box + Self-test)
      g->rec_header_label = make_static(hwnd, tr(g->language, StrId::REC_HEADER), 0, 0, 0, 0, true);
      g->candidate_combo = make_combo(hwnd, IDC_CANDIDATE_COMBO, 0, 0, 0, 0, false);
      g->copy_btn = make_button(hwnd, tr(g->language, StrId::COPY_REGEX_BTN), IDC_COPY_REGEX, 0, 0, 0, 0);
      g->copy_cap_btn = make_button(hwnd, tr(g->language, StrId::COPY_CAP_BTN), IDC_COPY_CAP_REGEX, 0, 0, 0, 0);
      g->rec_regex_edit = make_edit(hwnd, IDC_REC_REGEX_EDIT, 0, 0, 0, 0, ES_READONLY, tr(g->language, StrId::REC_REGEX_HINT));
      SendMessageW(g->rec_regex_edit, WM_SETFONT, reinterpret_cast<WPARAM>(g->mono_bold_font), TRUE);

      g->result_header_label = make_static(hwnd, tr(g->language, StrId::RESULT_HEADER), 0, 0, 0, 0, true);
      g->result_edit = make_edit(hwnd, IDC_RESULT_EDIT, 0, 0, 0, 0,
                                 ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL);
      SendMessageW(g->result_edit, WM_SETFONT, reinterpret_cast<WPARAM>(g->mono_font), TRUE);
      SetWindowTextW(g->result_edit, tr(g->language, StrId::RESULT_DEFAULT_HINT));

      g->selftest_title = make_static(hwnd, tr(g->language, StrId::SELFTEST_TITLE), 0, 0, 0, 0, true);
      g->selftest_edit = make_edit(hwnd, IDC_SELFTEST_EDIT, 0, 0, 0, 0, 0, tr(g->language, StrId::SELFTEST_HINT));
      SendMessageW(g->selftest_edit, WM_SETFONT, reinterpret_cast<WPARAM>(g->mono_font), TRUE);
      g->selftest_btn = make_button(hwnd, tr(g->language, StrId::SELFTEST_BTN), IDC_SELFTEST_BTN, 0, 0, 0, 0);
      g->selftest_label = make_static(hwnd, tr(g->language, StrId::SELFTEST_IDLE), 0, 0, 0, 0);

      // 6. Interactive Tutorial Wizard Overlay
      g->wizard_overlay = CreateWindowExW(0, L"RegexToolWizardOverlay", nullptr,
                                          WS_CHILD | WS_CLIPSIBLINGS,
                                          0, 0, 0, 0, hwnd, nullptr,
                                          GetModuleHandleW(nullptr), nullptr);

      populate_projects(g);

      // Restore last profile if available, or first available profile
      bool loaded = false;
      std::wstring err;
      if (!g->last_project_name.empty()) {
        if (load_project(hwnd, g->last_project_name, err)) {
          loaded = true;
        }
      }
      if (!loaded) {
        int count = static_cast<int>(SendMessageW(g->project_combo, CB_GETCOUNT, 0, 0));
        if (count > 0) {
          int len = static_cast<int>(SendMessageW(g->project_combo, CB_GETLBTEXTLEN, 0, 0));
          if (len > 0) {
            std::wstring first_proj(static_cast<size_t>(len + 1), L'\0');
            if (SendMessageW(g->project_combo, CB_GETLBTEXT, 0, reinterpret_cast<LPARAM>(first_proj.data())) != CB_ERR) {
              first_proj.resize(static_cast<size_t>(len));
              if (load_project(hwnd, first_proj, err)) {
                loaded = true;
              }
            }
          }
        }
      }

      if (!loaded) {
        rebuild_length_boxes(hwnd);
        add_example_row(hwnd, false);
      }

      update_ui_language(g, g->language);
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
      if (g && g->wizard_overlay && g->wizard_active) InvalidateRect(g->wizard_overlay, nullptr, TRUE);
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
              if (load_project(hwnd, text, err)) {
                g->last_project_name = text;
                save_app_config(g);
              } else {
                MessageBoxW(hwnd, err.c_str(), tr(g->language, StrId::SAVE_DLG_TITLE), MB_ICONERROR);
                set_status(hwnd, tr(g->language, StrId::STATUS_LOAD_FAIL) + err);
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

      if (id == IDC_CANDIDATE_COMBO && code == CBN_SELCHANGE) {
        int cur = static_cast<int>(SendMessageW(g->candidate_combo, CB_GETCURSEL, 0, 0));
        if (cur >= 0 && cur < static_cast<int>(g->last_candidates.size())) {
          g->selected_candidate_index = cur;
          const auto& cand = g->last_candidates[cur].candidate;
          g->recommended_regex = cand.regex;
          g->recommended_capturing_regex = cand.capturing_regex;
          g->selftest_candidate = cand;
          g->has_selftest_candidate = true;
          SetWindowTextW(g->rec_regex_edit, utf8_to_wide(cand.regex).c_str());
          std::string name = get_candidate_display_name(cand, g->language);
          set_status(hwnd, tr(g->language, StrId::STATUS_CAND_SWITCH) + utf8_to_wide(name));
          if (!get_edit_text(g->selftest_edit).empty()) {
            run_selftest(hwnd);
          }
        }
        return 0;
      }

      if (code != BN_CLICKED) return 0;

      if (id == IDC_GUIDE_BTN) {
        start_wizard(g);
        return 0;
      }

      if (id == IDC_LANG_BTN) {
        Language next_lang = (g->language == Language::ZH_CN) ? Language::EN_US : Language::ZH_CN;
        update_ui_language(g, next_lang);
        return 0;
      }

      if (id == IDC_RESET) {
        g->promote_tags.clear();
        g->field_overrides.clear();
        g->has_selftest_candidate = false;
        g->recommended_regex.clear();
        g->last_project_name.clear();
        save_app_config(g);
        SetWindowTextW(g->project_combo, L"");
        SetWindowTextW(g->sep_edit, L"");
        SetWindowTextW(g->rec_regex_edit, L"");
        SetWindowTextW(g->result_edit, tr(g->language, StrId::RESULT_DEFAULT_HINT));
        SetWindowTextW(g->selftest_label, tr(g->language, StrId::SELFTEST_IDLE));
        SetWindowTextW(g->selftest_edit, L"");
        set_mode(g, 0);
        SendMessageW(g->seg_count_combo, CB_SETCURSEL, 1, 0);
        rebuild_length_boxes(hwnd);
        clear_example_rows(g, false);
        clear_example_rows(g, true);
        add_example_row(hwnd, false);
        layout_scroll_content(g->scroll_container);
        set_status(hwnd, tr(g->language, StrId::STATUS_RESET_DONE));
        return 0;
      }
      if (id == IDC_SAVE) {
        std::wstring name = g->last_project_name.empty() ? project_name(g) : g->last_project_name;
        if (run_save_dialog(hwnd, name)) {
          if (save_project(hwnd, name)) {
            g->last_project_name = name;
            save_app_config(g);
          } else {
            MessageBoxW(hwnd, tr(g->language, StrId::STATUS_SAVE_FAIL), tr(g->language, StrId::SAVE_BTN), MB_ICONERROR);
          }
        }
        return 0;
      }
      if (id == IDC_ADD_POS) { add_example_row(hwnd, false); return 0; }
      if (id == IDC_ADD_NEG) { add_example_row(hwnd, true); return 0; }
      if (id == IDC_BATCH_POS) { run_batch_dialog(hwnd, false); return 0; }
      if (id == IDC_BATCH_NEG) { run_batch_dialog(hwnd, true); return 0; }
      if (id == IDC_MODE_AUTO) { set_mode(g, 0); layout_scroll_content(g->scroll_container); return 0; }
      if (id == IDC_MODE_SEP) {
        set_mode(g, 1);
        std::wstring cur_sep = get_edit_text(g->sep_edit);
        if (cur_sep.empty()) {
          auto seps = regex_tool::detect_separators(collect_edits(g->pos_edits));
          if (!seps.empty()) {
            std::string sep_str;
            for (size_t i = 0; i < seps.size(); ++i) {
              if (i > 0) sep_str += ",";
              sep_str += seps[i];
            }
            SetWindowTextW(g->sep_edit, utf8_to_wide(sep_str).c_str());
          }
        }
        layout_scroll_content(g->scroll_container);
        return 0;
      }
      if (id == IDC_DETECT_SEP) {
        auto seps = regex_tool::detect_separators(collect_edits(g->pos_edits));
        if (!seps.empty()) {
          std::string sep_str;
          for (size_t i = 0; i < seps.size(); ++i) {
            if (i > 0) sep_str += ",";
            sep_str += seps[i];
          }
          SetWindowTextW(g->sep_edit, utf8_to_wide(sep_str).c_str());
          set_status(hwnd, tr(g->language, StrId::STATUS_DETECTED_SEP) + utf8_to_wide(sep_str));
        } else {
          set_status(hwnd, tr(g->language, StrId::STATUS_NO_SEP));
        }
        return 0;
      }
      if (id == IDC_MODE_LEN) { set_mode(g, 2); layout_scroll_content(g->scroll_container); return 0; }
      if (id == IDC_GENERATE) { run_generate(hwnd); return 0; }
      if (id == IDC_COPY_REGEX) {
        if (!g->recommended_regex.empty()) {
          copy_to_clipboard(hwnd, utf8_to_wide(g->recommended_regex));
          set_status(hwnd, tr(g->language, StrId::STATUS_COPIED_REC) + utf8_to_wide(g->recommended_regex));
        } else {
          set_status(hwnd, tr(g->language, StrId::STATUS_NO_REGEX));
        }
        return 0;
      }
      if (id == IDC_COPY_CAP_REGEX) {
        if (!g->recommended_capturing_regex.empty()) {
          copy_to_clipboard(hwnd, utf8_to_wide(g->recommended_capturing_regex));
          set_status(hwnd, tr(g->language, StrId::STATUS_COPIED_CAP) + utf8_to_wide(g->recommended_capturing_regex));
        } else {
          set_status(hwnd, tr(g->language, StrId::STATUS_NO_REGEX));
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
  wsprintfW(buf, L"Program encountered exception (Exception Code: 0x%08X).",
            ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0);
  MessageBoxW(nullptr, buf, L"RegexTool Exception", MB_ICONERROR | MB_OK);
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

  // Register Batch Import Dialog Window Class
  WNDCLASSW bc{};
  bc.hbrBackground = CreateSolidBrush(RGB(243, 243, 243));
  bc.lpfnWndProc = batch_proc;
  bc.hInstance = hInstance;
  bc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
  bc.lpszClassName = L"RegexToolBatch";
  RegisterClassW(&bc);

  // Register Save Profile Dialog Window Class
  WNDCLASSW sdc{};
  sdc.hbrBackground = CreateSolidBrush(RGB(243, 243, 243));
  sdc.lpfnWndProc = save_dlg_proc;
  sdc.hInstance = hInstance;
  sdc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
  sdc.lpszClassName = L"RegexToolSaveDlg";
  RegisterClassW(&sdc);

  // Register Interactive Wizard Overlay Window Class
  WNDCLASSW wz{};
  wz.hbrBackground = nullptr;
  wz.lpfnWndProc = wizard_overlay_proc;
  wz.hInstance = hInstance;
  wz.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
  wz.lpszClassName = L"RegexToolWizardOverlay";
  RegisterClassW(&wz);

  // Keyboard Accelerators (P2-3)
  ACCEL accels[] = {
    { FCONTROL | FVIRTKEY, 'S', IDC_SAVE },
    { FCONTROL | FVIRTKEY, 'N', IDC_RESET },
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

  auto g = reinterpret_cast<AppState*>(GetWindowLongPtrW(g_main_hwnd, GWLP_USERDATA));
  if (g && !g->wizard_shown) {
    start_wizard(g);
  }

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
