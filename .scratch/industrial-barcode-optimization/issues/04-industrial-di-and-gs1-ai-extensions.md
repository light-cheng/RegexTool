# 04 — 工业 DI (ASC MH10.8.2) 与 GS1 AI 字典智能推断与紧凑拼接切分

**What to build:** 扩展内置汽车（VDA 4994）、航天军工（ATA 2000 / MIL-STD-130）与医疗（GS1 AI）标准数据标识符字典。在无分隔符的紧凑拼接条码（如 `1PA10402Q10001TLOT2026S00891`）中通过字典树最长匹配实现自动切分，并为每个捕获字段赋予标准业务语义名称（如 `PartNumber`, `Quantity`, `SerialNumber`, `TraceabilityLot`, `VendorCode`）。

**Blocked by:** 01 — 智能变长归纳与抗过拟合对齐, 02 — ISO/IEC 15434 协议外壳与不可见控制字符全生命周期支持

**Status:** done

- [x] 完整支持汽车 VDA 4994 (`1J`, `3J`, `4J`, `5J`, `6J`)、航天军工 (`17V`, `1S`, `30P`, `30Q`) 及 GS1 AI (`(01)`, `(10)`, `(17)`, `(21)`) 等标识符
- [x] 输入无分隔符紧凑拼接条码 `1PA10402Q10001TLOT2026S00891`，工具无需手动配置即可自动依据 DI 字典切分为 `1P`, `Q`, `1T`, `S` 对应字段
- [x] 生成的命名捕获正则及界面字段列表中自动显示规范业务名称（如 `PartNumber`, `Quantity`, `TraceabilityLot`, `SerialNumber`）
- [x] 核心引擎单元测试覆盖各种工业 DI / GS1 AI 组合样本及捕获值验证
