# ADC_DMA — STM32F103 数据采集与上报

电位器电压 → ADC(100 kHz，TIM3 触发) → DMA 循环双缓冲 → 每 512 点算一次均值/极值 → 环形缓冲 → 10 Hz 二进制帧上报 → 上位机画曲线 + 0.96" OLED 本地显示，并可反向下发 `START/STOP/SET/GET`。

## 硬件

| 信号 | 引脚 | 接法 |
|---|---|---|
| 模拟输入 | PA0 / ADC1_IN0 | 10 kΩ 电位器中间脚，两端接 3.3 V 与 GND |
| 串口 TX | PA9 | → CH340 的 RXD |
| 串口 RX | PA10 | ← CH340 的 TXD |
| 供电 | 3.3 V / GND | 与 CH340 共地 |
| OLED SCL | PB6 | → OLED 模块 SCL |
| OLED SDA | PB7 | → OLED 模块 SDA |

> 模拟输入不得超过 3.3 V，否则会打坏 ADC 输入保护二极管。

## 数据链路

```
PA0 ──► ADC1 (12 bit, 55.5 cycles, 外部触发 T3_TRGO)
          │  100 kHz
          ▼
      adc_dma_buff[1024]  ◄── DMA1_Channel1 循环搬运
          │  半满 / 全满中断
          ▼
      adc_frame_a[512] / adc_frame_b[512]   （拷贝半区，避免被覆盖）
          │
          ▼
      process_frame()  均值 → 滑动平均(MA_WIN=8) → 极值 → g_voltage
          │
          ├──► app_poll() 10 Hz 组帧 0x20 → frame_send() → USART1 115200 8N1 → CH340 → PC
          │
          └──► 5 Hz → OLED(SSD1306，I2C 400 kHz) → 本地显示
```

接收方向：

```
USART1 RX ──► DMA1_Channel5 (循环 256 B) ──► 空闲/整满回调
                                              │ 增量搬运
                                              ▼
                                          g_rx_ring (1023 B)
                                              │
                                              ▼
                                    frame_try_parse() → app_handle()
```

## 协议 v1

### 帧格式

```
AA 55 | LEN | SEQ | PAYLOAD(LEN 字节) | CRC_L | CRC_H
```

| 字段 | 说明 |
|---|---|
| `AA 55` | 帧头 |
| `LEN` | **payload 长度**（0~64），总长 = `LEN + 6` |
| `SEQ` | 序号，0~255 循环；应答沿用请求的 SEQ |
| `PAYLOAD` | 命令字 + 数据，见下表 |
| `CRC` | CRC-16/CCITT-FALSE（init `0xFFFF`、poly `0x1021`、不反射、无终值异或），覆盖 `AA` 到 payload 末尾，**小端**存放（先低字节） |

自检：对 ASCII `"123456789"` 计算应得 `0x29B1`。

### 命令

| CMD | 方向 | payload | 应答 payload | 说明 |
|---|---|---|---|---|
| `0x10` SET | 上位机→设备 | `10 p_lo p_hi` | `10 err p_lo p_hi` | 设置上报周期，`20~5000` ms |
| `0x11` GET | 上位机→设备 | `11` | `11 state p_lo p_hi mv_lo mv_hi flags` | 查询状态 |
| `0x12` START | 上位机→设备 | `12` | `12 err` | 进入 RUN，立即发一帧 |
| `0x13` STOP | 上位机→设备 | `13` | `13 err` | 进入 IDLE，停止数据流 |
| `0x20` DATA | 设备→上位机 | `20 mv_lo mv_hi min_lo min_hi max_lo max_hi` | — | 周期上报，单位 mV |

`err`：`0` 成功 / `1` 参数越界 / `2` 状态不允许 / `3` 未知命令
`state`：`0` IDLE / `1` RUN / `2` FAULT
`flags`：bit0 `adc_err`，bit1 接收环溢出

### 联调用例（HEX 直接发送，XCOM 勾「16进制发送」、取消「发送新行」）

| 发送 | 期望收到 | 说明 |
|---|---|---|
| `AA 55 01 01 13 EB 92` | `AA 55 02 01 13 00 27 D3` | STOP：停数据流 |
| `AA 55 01 02 12 99 D7` | `AA 55 02 02 12 00 46 B9` | START：恢复上报 |
| `AA 55 03 04 10 32 00 AD F0` | `AA 55 04 04 10 00 32 00 6C 42` | SET 50 ms |
| `AA 55 03 05 10 0A 00 25 0A` | `AA 55 04 05 10 01 32 00 0D DF` | SET 10 ms 越界，`err=01`，周期保持 50 ms |
| `AA 55 01 06 11 3E 2B` | `AA 55 07 06 11 01 32 00 00 08 00 00 EB` | GET：RUN / 50 ms / 2048 mV |
| `AA 55 01 07 99 8F 08` | `AA 55 02 07 99 03 B7 A5` | 未知命令，`err=03` |

> GET 的应答里 `mv` 与 CRC 每帧都变，校验时只看前 9 字节
> （`AA 55 07 06 11 01 32 00`），第 10、11 字节是 mV，第 12 字节是 flags。

## 本地显示（OLED）

| 项目 | 值 |
|---|---|
| 型号 | SSD1306，0.96"，128×64 |
| 接口 | I2C1，PB6=SCL / PB7=SDA，400 kHz |
| 地址 | `0x3C`（8 位形式 `0x78`） |
| 显存 | `fb[8][128]`，每字节对应一列 8 个像素 |
| 字库 | 6×8 点阵，仅 `' '` ~ `'Z'`（59 个字符） |
| 刷新 | 5 Hz（200 ms），整屏 1024 字节 |

刷新流程：`OLED_str()` 写入 `fb` → `OLED_flush()` 发 `0x21/0x22` 设窗口 → 一次 `HAL_I2C_Mem_Write` 推 1024 字节。

**注意**：推送是阻塞的，400 kHz 下约 23 ms。放在主循环里会占掉约 1/4 的 100 ms 周期，测抖动时要把它算进去。

## 构建与烧录

```bat
cube-cmake --build build/Debug
```

VS Code 里选 `STM32Cube: Build` 也可。烧录用 ST-LINK，`Under Reset` 模式（本板 SWD 容易被占用）。

## 回归测试

```bat
pip install pyserial
python tools\regress.py COM6 10
```

读 10 秒，统计帧数、CRC 错、丢帧、平均间隔，并检查 `min ≤ mv ≤ max`。

### 基线（2026-10-27，COM6，周期 100 ms，电位器不动）

```
帧数        : 100
CRC 错      : 0
SEQ 跳变    : 0
数值越界    : 0   (要求 vmin <= mv <= vmax)
平均间隔    : 99.9 ms
间隔抖动    : 91.7 ~ 111.0 ms
mv 范围     : 2054 ~ 2054 mV
实测帧率    : 10.0 Hz
结果: PASS
```

> 抖动 ±10 ms 主要是 PC 侧测量误差：`ser.read()` 按 50 ms 超时批量返回，两帧落在同一次读取里会被记成极短/极长的间隔。固件侧的周期判断只看 `HAL_GetTick()`，实际抖动在毫秒级。

## 关键设计

1. **DMA 缓冲必须双份** —— `adc_dma_buff` 会被硬件持续覆盖，只有拷到 `adc_frame_a/b` 之后才能慢慢算。
2. **半满/全满各触发一次** —— 1024 点缓冲拆成两个 512 点半区，中断里只置标志，拷贝和计算放在主循环，避免中断里做 512 次乘除。
3. **均值/极值在循环外统计** —— `sum/len`、`ma_push`、单位换算都是每帧一次，不是每点一次。放在循环内会让滑动平均被同一帧塞满，读数系统性偏低。
4. **CRC 覆盖帧头** —— 只覆盖 payload 的话，帧头被干扰吞掉一个字节能骗过校验。
5. **解析器只前进不后退** —— 校验失败 `drop(1)` 后重找帧头，半帧时保留数据等下一批，所以必须是“先 peek 后 drop”，不能“先取出再解析”。
6. **应答沿用请求 SEQ** —— 上位机可以用 SEQ 做超时重发与配对。

## 已知限制

- 发送是阻塞的 `HAL_UART_Transmit`，13 字节约 1.13 ms @115200；上报频率高于 ~200 Hz 会挤占主循环。
- 接收环容量 1023 字节，溢出策略是**丢新**并累加 `overflow`（可通过 GET 的 flags 观察）。
- 没有硬件流控，长时间高负载需要应用层限速。
- 电压换算按 `3300 mV / 4095` 估算，未做 VDDA 实测校准，绝对误差约 ±1%。
- `STATE` 只有 IDLE/RUN/FAULT，暂无 CALIB。
- OLED 刷新阻塞 ≈23 ms（1024 B @400 kHz），当前 5 Hz；提高刷新率会挤占主循环并放大上报抖动。
- 字库只覆盖 `' '`~`'Z'`（59 个字符），小写字母会越界读，`OLED_char()` 的范围判断需同步改为 `ch > 'Z'`。
- I2C 总线未做超时重试，屏幕线松了会静默不刷新。

## 排障

| 现象 | 先查 |
|---|---|
| 完全没有帧 | USART1 的 RX DMA 是否配置、`usart_rx_arm()` 是否调用 |
| 有帧但 mv 恒为 0 | `main.c` 的 while 里是否还在调用 `process_frame` |
| min/max 与 mv 差 500 | 极值没做码值→毫伏换算 |
| 只有第一帧有应答 | `tx_busy` 是否被正确清零（比较符方向、回调是否触发） |
| HardFault | `huart1.hdmatx` 是否为空（未配置 TX DMA） |
| 数据帧与应答混在一起 | 按 CMD 字段区分，`0x20` 是上报，`0x10~0x13` 是应答 |
| OLED 黑屏 | `OLED_init()` 是否调用；`OLED_flush()` 是否超时（1024 B 在 100 kHz 下需 ~92 ms） |
| OLED 只显示一个字符 / 整串叠在一列 | `OLED_str()` 里 `col` 的步进是否写成 `col += 6` |

## 后续

- CALIB 状态：上电采 N 帧求偏移，写入 `g_offset`
- I2C 传感器（BMP280，I2C1 的 PB6/PB7），新增 `0x21` 环境帧
- 交互式上位机：实时曲线 + 命令面板（`pyserial` + `matplotlib`/PyQt）
- 用 MiniRTOS 重写调度，与当前裸机版本做结构对比
