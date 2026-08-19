# AGENTS.md

本文件是针对 AI Agent（如 Antigravity / Claude / Codex 等）在本项目中进行代码研发、调试、测试与工单交付的操作指南与工程守则。

---

## 1. 项目核心概述与架构边界

- **项目定位**：供工业条码解码工程师使用的离线单文件 Windows 桌面工具（C++17 + 原生 Win32 API）。
- **零外部依赖原则**（参见 [ADR-0001](file:///docs/adr/0001-portable-offline-cpp-exe.md)）：
  - 仅使用标准 C++17 STL 和 Windows SDK 原生 API。
  - 严禁引入 Qt、Boost、MFC、wxWidgets 或任何第三方 DLL 运行时。编译输出的 `regex_tool.exe` 必须为单个绿色免安装文件。
- **模块分层**：
  - **核心计算引擎** (`src/engine/`)：纯 C++ 业务逻辑。负责样本对齐、分段切分、正则变体合成、控制字符归一化与逐段匹配验证（`walk_verify`）。不包含任何 GUI 窗口代码，可 100% 独立于控制台测试。
  - **GUI 交互层** (`src/app/main.cpp`)：原生 Win32 GDI 界面。处理 DPI 缩放适配、中英双语切换、不可见控制符徽标渲染、分段详情编辑、自测框以及向导遮罩层。
  - **单元测试套件** (`tests/engine_tests.cpp`)：轻量宏断言测试套件，覆盖全部算法与工业标准条码场景。
  - **持久化配置** (`projects/`)：`.rgx` 结构化项目文件。

---

## 2. 领域术语统一（Ubiquitous Language）

所有代码符号命名、注释、文档与测试均严格遵守 [CONTEXT.md](file:///CONTEXT.md) 中的统一领域语言：
- **正例 (Positive Example)**：目标格式必须匹配的条码。
- **反例 (Negative Example)**：目标格式必须拒绝的条码。
- **分隔符 (Separator)**：固定出现用于切分的可见标点（`@`, `|`）或 ASCII 控制符（GS `\x1D`, RS `\x1E`, EOT `\x04`）。
- **数据标识符 (Data Identifier / DI / AI)**：工业标准字段前缀（`1P`, `Q`, `1T`, `S`, `1J`, `(01)` 等）。
- **分段 (Segment)** / **字段 (Field)**：条码切分单元，字段包含字符集与长度范围。
- **候选正则 (Candidate Regex)**：由引擎合成排序的多条规则（`smart`, `relaxed`, `broadened`, `exact`, `aim_stripped`, `aim_full`）。
- **验证 (Validation)**：正例全通、反例全拒、捕获值 100% 吻合的三维检验。

---

## 3. 构建与测试指令

Agent 在执行代码变更后，必须执行以下指令验证并保证测试全绿：

### 编译全部目标（Release）
```powershell
cmake --build build --config Release
```

### 运行自动化测试
```powershell
ctest --test-dir build -C Release --output-on-failure
# 或直接运行测试可执行文件：
.\build\Release\engine_tests.exe
```

### 同步根目录发布程序
```powershell
powershell -Command "Copy-Item build\Release\regex_tool.exe -Destination . -Force"
```

---

## 4. 关键开发守则与踩坑提示

1. **TDD 驱动开发**：
   - 为引擎添加新特性或修复 bug 时，必须先在 `tests/engine_tests.cpp` 中编写测试用例。
   - 验证通过后方可提交工单状态为 `done`。
2. **字符编码与控制字符处理**：
   - 内部逻辑一律采用 UTF-8 `std::string`。
   - Win32 API 交互一律使用 `std::wstring` 与 `utf8_to_wide` / `wide_to_utf8` 转换。
   - **MSVC 16进制转义语法陷阱**：在 C++ 字符串字面量中，`"\x1E06"` 会被 MSVC 视作单字符 `0x1E06`。涉及后接十六进制数字的转义时必须拆分拼接：`"\x1E" "06"` 或 `std::string("\x1E") + "06"`。
3. **向导与界面渲染**：
   - 严禁调用 `GetDC(nullptr)` 抓取物理显示器屏幕。所有窗口内部绘制与快照必须使用 `PrintWindow` 与内存 DC，并预充实体背景色以防止 DWM 异步合成透明穿透。

---

## 5. 工单跟踪与协作流程

- **工单与规范位置**：位于 `.scratch/<feature-name>/` 下（如 `.scratch/industrial-barcode-optimization/`）。
- **工单状态流转**：
  - `needs-triage` -> `needs-info` -> `ready-for-agent` -> `done` (或 `ready-for-human` / `wontfix`)。
- **工单完成标准**：
  - 代码实现完整且满足所有 Acceptance Criteria；
  - 核心单元测试全部通过（0 failure）；
  - 工单文件中的 checkbox 勾选，`Status` 更新为 `done`。
