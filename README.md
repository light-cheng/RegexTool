# RegexTool (工业条码正则生成与验证工具)

[![Language](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/Platform-Windows-0078D7.svg)](https://microsoft.com/windows)
[![Architecture](https://img.shields.io/badge/Architecture-Native%20Win32-success.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

**RegexTool** 是一款专为工业条码解码工程师、视觉检测工程师与自动化产线工程师量身打造的**离线单文件 Windows 正则推断与验证工具**。

只需输入多条条码**正例 (Positive Examples)** 与**反例 (Negative Examples)**，工具即可自动对齐样本、识别工业标识与不可见控制字符、推断字段分段规则，并生成多种严格度梯度的候选正则表达式，同时提供三维交叉验证与实时自测演练。

---

## 🌟 核心特性 (Key Features)

- ⚡ **零外部依赖，单文件便携 (Zero Dependencies, Single Portable EXE)**
  - 基于纯 **C++17 STL + 原生 Windows Win32 GDI API** 打造。
  - 无需安装任何运行时环境（无 Qt、MFC、Boost 或 .NET 依赖），输出仅单个绿色可执行文件（约 500KB）。
- 🏭 **全工业级条码标准支持 (Industrial Standards Built-in)**
  - **ISO/IEC 15434 报文格式**：深度支持 Format 05 / Format 06 封套，完美解析 `[)>` 引导头、Record Separator (RS `\x1E`)、Group Separator (GS `\x1D`) 与 End-of-Transmission (EOT `\x04`)。
  - **ASC MH10 数据标识符 (Data Identifiers / DI)**：内置 `1P`、`Q`、`1T`、`S`、`1J`、`3S`、`4S`、`V`、`D`、`K` 等 20+ 工业常见字段前缀自动识别与智能分段。
  - **GS1 应用标识符 (Application Identifiers / AI)**：支持 `(01)` GTIN、`(10)` 批号、`(17)` 有效期、`(21)` 序列号等定长与变长字段解析。
  - **AIM Symbology Identifiers (条码码制前缀识别)**：自动侦测扫描枪输出的 `]C1` (Code 128 / GS1-128)、`]d2` (Data Matrix)、`]Q3` (QR Code) 等码制标识符，并自动生成剥离版 (AIM-Stripped) 与完整版 (AIM-Full) 双重候选。
- 🔍 **多候选正则变体合成与排序 (Multi-Candidate Synthesis & Ranking)**
  - 自动输出 4~6 种不同严格度梯度的候选正则供生产场景灵活选用：
    - `smart` (智能推荐)：结构精准且具备合理的泛化容错。
    - `relaxed` (宽松兼容)：放宽长度与字符集限制，适用于可变长字段。
    - `broadened` (字符集拓展)：统一转换为 `[A-Z0-9]` 等工业标准字符集。
    - `exact` (极度严格)：完全锁定样例字符与固定格式。
    - `aim_stripped` / `aim_full`：面向扫码枪前缀配置的专用变体。
- 🎯 **三维全流程验证 (3D Comprehensive Validation)**
  - **正例全匹配 (Positive Pass)**：保证所有生产良品条码 100% 吻合。
  - **反例全拒绝 (Negative Rejection)**：精确拦截不良混料条码，并高亮失败的具体分段位置。
  - **捕获值精准比对 (Field Capture Accuracy)**：验证各字段变量提取的一致性。
- 🧪 **实时交互式自测试验区 (Live Self-Test Box)**
  - 支持即时输入或扫描测试条码，毫秒级反馈匹配结果与逐段解析状态。
- 🌐 **双语支持与 DPI 自适应 (Bilingual & High-DPI Ready)**
  - 原生支持 **简体中文** / **English** 一键切换。
  - 适配 100% / 125% / 150% / 200% 等 Windows 系统高分屏 DPI 缩放。
- 💾 **项目持久化 (Project File Persistence)**
  - 支持保存与加载 `.rgx` 结构化项目配置，方便多产线标准规程归档与协同。

---

## 🖥️ 界面预览 (UI Overview)

| 中文界面 (Chinese Interface) | 英文界面 (English Interface) |
|:---------------------------:|:---------------------------:|
| 🇨🇳 原生 Win32 高性能 GDI 渲染 | 🇺🇸 Native High-DPI UI |

---

## 🛠️ 编译与构建 (Build from Source)

### 环境要求
- Windows 10 / 11 / Windows Server
- Visual Studio 2019 / 2022 (安装 "C++ 桌面开发" 工作负载)
- CMake 3.16+

### 快速构建 (Release)
```powershell
# 1. 配置 CMake 生成 VS 解决方案
cmake -B build -S .

# 2. 编译 Release 单文件可执行程序
cmake --build build --config Release

# 3. 运行自动化单元测试
ctest --test-dir build -C Release --output-on-failure
# 或直接运行测试程序：
.\build\Release\engine_tests.exe
```

编译生成的独立可执行文件位于 `build\Release\regex_tool.exe`。

---

## 📂 模块分层与代码结构 (Architecture)

```text
RegexTool/
├── src/
│   ├── engine/           # 核心推断与验证计算引擎（纯 C++17，零依赖，可脱机独立测试）
│   │   ├── engine.h      # 引擎公共接口、AST 定义与工业标准定义
│   │   └── engine.cpp    # 样本对齐、分段切分、控制字符归一化与正则生成算法
│   └── app/              # Win32 GUI 宿主交互层
│       ├── main.cpp      # 原生 Win32 窗口管理、GDI 渲染、双语资源与 DPI 适配
│       ├── app.rc        # Windows 资源定义（嵌入图标与应用清单）
│       └── app.manifest  # Common Controls v6 & DPI 启用清单
├── tests/
│   └── engine_tests.cpp  # 核心引擎单元测试套件（覆盖工业标准场景与边界测试）
├── projects/             # 预置与保存的 .rgx 格式工程配置
├── docs/                 # 架构设计决策 (ADR) 与工业标准文档
├── CMakeLists.txt        # CMake 构建脚本
└── README.md             # 项目说明文档
```

---

## 📄 开源许可证 (License)

本项目采用 [MIT License](LICENSE) 开源许可证。
