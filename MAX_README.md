# Chameleon Ultra — Max 固件 / Max Firmware

基于 **RfidResearchGroup/ChameleonUltra 官方最新正式版 v2.3.0** 的增强固件，
参考 Neo-403 改版固件的功能思路，用完全开源的方式重新实现，并新增离线整卡克隆。

Based on the official **RfidResearchGroup/ChameleonUltra v2.3.0** release, with the
Neo-403 mod features re-implemented in open source plus a new offline full-card
clone feature.

> ⚠️ 本固件为独立修改版，与官方及 Neo-403 均无隶属关系。刷机前请阅读下方的「能力边界」。
> This is an independent fork. Not affiliated with RfidResearchGroup or Neo-403.

---

## 新增功能 / New features

### 1. 长按 A 键离线整卡克隆 / Offline full-card clone (assignable to any button)

* 不连接电脑/手机：把真实 Mifare Classic 卡（Mini / 1K / 2K / 4K）**整张**读入当前卡槽
* 密钥恢复：固件内置 78 个常用密钥字典（含传输默认密钥、NXP/MAD 密钥、常见系统密钥、UID 派生密钥），
  用固件自带的 `mf1_toolbox_check_keys_of_sectors` 扫全部扇区；成功后还会自动从 trailer 收获可读的 key B
* 卡槽类型自动匹配：读 4K 卡时卡槽自动切成 4K（失败自动还原）
* 成功后立即仿真 + 写入 flash，拔掉原卡后设备就是原卡的完整副本
* 全程纯读卡，不向真实卡写任何数据

**LED 指示 / LED patterns**（当前卡槽 LED，闪烁 3 轮）：

| 模式 | 含义 |
|---|---|
| 蓝色常亮 | 正在克隆 |
| 绿色 2 闪 | 成功 |
| 红色 1 闪 | 没找到卡 |
| 红色 2 闪 | 不是 Mifare Classic |
| 红色 3 闪 | 读块通信失败（卡槽已还原） |
| 红色 4~8 闪 | 有 N 个扇区密钥不在字典里（卡槽已还原） |

在官方上位机的按键设置里选择 `Full clone card`（整卡克隆）即可分配给
短按/长按 A 或 B。默认不占用长按 A（官方默认长按 A 是克隆 UID，可自行改）。

### 2. 卡槽自动轮询 / Slot auto-polling

仿真模式下按设定间隔自动切换卡槽，依次仿真多张卡；一旦有读卡器激活了当前卡
（`g_is_field_detected`）立即暂停轮询，读卡器离开后继续。设置项：

* 开关（默认关）
* 间隔：100ms ~ 10s（默认 500ms）
* 轮询卡槽掩码：8 个卡槽任意组合
* 可分配按键功能 `Toggle slot polling`（一键开关轮询）

### 3. 卡组 / Card groups

* 卡组大小可设为 2 或 4（0 = 关闭，默认）
* 普通切换卡槽（A/B 键、GUI）只在当前卡组内循环
* 新按键功能 `Switch card group`（切换卡组）跳到下一组

### 4. 软重启 / Soft reboot

保存卡槽数据 + 设置后立即重启。新按键功能 `Soft reboot`，
以及新命令 `DATA_CMD_RESET_DEVICE (1046)`。

---

## 上位机 / Companion app

配套修改版 GUI（Flutter，Android APK + Windows）：见 GUI 仓库同名文档。
* 设置页四个按键配置改为下拉框，含全部 10 种功能
* 新增「卡槽自动轮询」设置区（开关/间隔/卡槽/卡组）
* 新增「离线整卡克隆」按钮：触发后轮询进度，结果以通知显示（也支持不看手机直接看 LED）
* 新增「软重启」按钮
* 全部新功能通过 `GET_DEVICE_CAPABILITIES` 能力协商，连旧固件不会显示新选项，官方固件兼容

---

## 协议新增（供开发者）/ New protocol commands

| 命令 | 值 | 说明 |
|---|---|---|
| `GET_SLOT_POLLING_CONFIG` | 1044 | 读 4 字节 `{enable, interval(100ms), slot_mask, group_size}` |
| `SET_SLOT_POLLING_CONFIG` | 1045 | 写同上 4 字节（记得再发 1013 保存） |
| `RESET_DEVICE` | 1046 | 保存并软重启 |
| `MF1_OFFLINE_CLONE` | 2023 | 触发离线整卡克隆（立即返回，作业在主循环同步执行，期间设备不响应其他命令） |
| `MF1_OFFLINE_CLONE_STATUS` | 2024 | 轮询状态，返回 `{state, missing_sectors}`；state: 0=idle 1=running 2=ok 3=no card 4=not MF1 5=keys missing 6=read fail 7=apply fail。作业执行期间响应会被延迟到作业结束，上位机需容忍超时并重试 |

`GET_DEVICE_SETTINGS (1034)` 现返回 18 字节：官方 13 字节布局不变，`[13]` = sleep timeout（与 GUI 既有约定一致），
`[14..17]` = 轮询配置。设置版本号 v7 → v8，旧数据自动迁移。

---

## 能力边界（务必阅读）/ Capability boundaries (read this!)

**离线（无电脑/手机）能做的：**
* 任意卡的 UID 克隆（官方原有功能）
* **字典密钥可恢复的卡**的整卡克隆 —— 现实中绝大多数门禁/电梯/水卡使用默认或常见密钥，覆盖面很大
* key A 已知时从 trailer 顺带获得 key B，并自动把已恢复密钥传播到同卡其他扇区

**离线做不到的（这是物理/内存限制，不是没写）：**
* darkside / nested / hardnested 等 crypto1 密钥**破解**需要 8MB 级内存做 lfsr_recovery32
  运算，nRF52840 只有 256KB RAM，设备端无法完成（官方固件同样只做采集、破解在电脑端完成）。
  完全自定义密钥 + 硬 PRNG 的卡请用配套 GUI 连机攻击（darkside/nested/hardnested 均已支持），
  拿到完整 dump 后再导入卡槽。

固件不会装作破解成功：字典未命中的扇区会导致克隆中止并还原卡槽，LED 报数。

---

## 编译 / Build

1. 把本仓库推到你的 GitHub（如 `yourname/ChameleonUltra`）
2. Actions 自动构建：push 到 `main` → `dev` 预发布（`ultra-dfu-app.zip` / `ultra-dfu-full.zip` / `.hex`）；
   打 `v*` tag → 正式 release
3. 建议先打个 tag 让版本号正确（不打 tag 也能编译，版本号回落为 2.3.0）：
   ```bash
   git tag v2.3.0-max.1 && git push origin v2.3.0-max.1
   ```

## 烧录 / Flash (DFU)

* **只更新应用（推荐，安全可回滚）**：解压 release 里的 `ultra-dfu-app.zip`
  * 安卓：官方 ChameleonUltra APP → Firmware management → Flash .zip FW via DFU
  * 电脑：https://hackady.github.io/chameleon-ultra-dfu-web/ 或 nRF Connect → DFU
* **全镜像（含 bootloader，慎用）**：`ultra-dfu-full.zip`，仅在 bootloader 也需要更新时使用
* DFU 步骤：设备关机 → 按住 B 键插 USB（或 APP 里 Enter DFU）→ 出现 DFU 盘符/设备 → 刷入 zip

## 与官方仓库的差异 / Diff summary vs official v2.3.0

| 文件 | 改动 |
|---|---|
| `firmware/application/src/settings.h/.c` | 设置 v8：轮询/卡组字段 + 4 个新按键功能枚举 + 迁移 |
| `firmware/application/src/data_cmd.h` | 5 个新命令号 |
| `firmware/application/src/app_cmd.c` | 新命令处理器 + settings blob 扩展 |
| `firmware/application/src/app_main.c` | 新按键功能、轮询状态机、卡组切换、克隆入口与 LED |
| `firmware/application/src/rfid/reader/hf/mf1_offline_clone.c/.h` | 全新：离线整卡克隆模块 + 内置字典 |
| `firmware/application/Makefile` | 新增源文件 |
| `firmware/Makefile.defs` | 无 tag 仓库的版本号兜底 |
