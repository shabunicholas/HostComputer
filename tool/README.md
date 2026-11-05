# 虚拟从站工具（ModbusSlaveTool）

一台"假设备"，跑在串口上，用来和本工程的上位机联调——不需要真机、不需要下位机。

## 一、需要哪些文件

| 文件 | 位置 | 作用 |
|---|---|---|
| `main_slavetool.cpp` | `tool/slavetool/` | 工具主程序：串口、模拟数据、按键注入、帧日志 |
| `slavetool.pro` | `tool/slavetool/` | 独立编译配置 |
| `modbusslave.h` / `.cpp` | **`tool/`** | 从站核心：应答、异常码、寄存器、故障注入 |
| `modbusprotocol.h` / `.cpp` | **仓库根目录** | CRC16、功能码、异常码、帧工具 |

后两组是**复用**的，不要复制进 `slavetool/`。`slavetool.pro` 用相对路径引用：
`../` 指向 `tool/`，`../..` 指向仓库根目录。

## 二、编译

Qt Creator 打开 `tool/slavetool/slavetool.pro` → 直接构建。

命令行（MinGW + Qt 5）：

```powershell
$qt = 'C:\Qt\Qt5.14.1\5.14.1\mingw73_64\bin'
$mk = 'C:\Qt\Qt5.14.1\Tools\mingw730_64\bin\mingw32-make.exe'
$env:PATH = "$qt;$(Split-Path $mk);$env:PATH"

cd tool\slavetool
qmake slavetool.pro
mingw32-make -j4
```

产物：`tool/slavetool/build/ModbusSlaveTool.exe`

**想让 exe 能双击运行**（把 Qt 运行库拷到旁边）：

```powershell
cd tool\slavetool\build
& 'C:\Qt\Qt5.14.1\5.14.1\mingw73_64\bin\windeployqt.exe' --no-translations --compiler-runtime ModbusSlaveTool.exe
```

## 三、和上位机联调

用一对虚拟串口（如 com0com / ELTIMA 的 `COM1 ↔ COM2`）：从站占一端，上位机占另一端。

**从站**（默认参数，直接双击或运行）：

```powershell
ModbusSlaveTool.exe --port COM2 --baud 19200 --parity even --slave 1
```

**上位机**：把 `config.ini` 放到**上位机 exe 旁边**（程序读的是 `applicationDirPath()`，不是源码目录），内容：

```ini
[Serial]
Port=COM1
BaudRate=19200

[Protocol]
Name=modbus
ModbusSlaveAddress=1
ModbusResponseTimeoutMs=300
ModbusRetries=2
ModbusTempRegister=0
ModbusPressureRegister=16
```

**启动顺序**：先开从站，再开上位机（反了也不会崩，上位机会重连）。

联调正常时，从站窗口每 3 秒打印两帧：

```
[20:06:33] <- 01 04 00 00 00 01   读输入寄存器 起始=0x0000 数量=1
[20:06:33] -> 01 04 02 01 08 B9 66
[20:06:33] <- 01 04 00 10 00 03   读输入寄存器 起始=0x0010 数量=3
[20:06:33] -> 01 04 06 04 CD 06 11 07 1C 1F B3
```

## 四、命令行参数

| 参数 | 默认 | 说明 |
|---|---|---|
| `--port` | `COM2` | 串口号 |
| `--baud` | `19200` | 波特率 |
| `--parity` | `even` | `even` / `odd` / `none`，**必须和上位机一致** |
| `--databits` | `8` | 数据位 |
| `--stopbits` | `1` | 停止位 |
| `--slave` | `1` | 从站地址 |
| `--temp` | `0` | 温度所在的输入寄存器地址（读 1 个） |
| `--press` | `16` | 压力起始地址（连续读 3 个） |
| `--presscount` | `3` | 压力路数 |

打开失败不会退出：会列出本机可用串口并每 2 秒重试，按 `q` 退出。

## 五、运行中按键（即时生效，不用回车）

| 键 | 作用 | 用来看上位机 |
|---|---|---|
| `d` | 丢应答（开/关） | 超时 → 重发 → 报"超时应答" |
| `c` | 应答 CRC 写错（开/关） | 判 CRC 错 → 重发 |
| `s` | 慢应答 +500ms（开/关） | 超时阈值够不够 |
| `x` | 强制异常码 04（开/关） | 异常应答**不该重发** |
| **`m`** | **手动设值**（输入 `通道 数值` 后回车） | 精确验证阈值边界、报警触发/解除 |
| **`a`** | **暂停/恢复**自动模拟 | 让数值定住不动，方便对数和截图 |
| `r` | 寄存器值跳变（只改正弦相位） | 数值突变，但**跨不过阈值** |
| `v` | 打印当前寄存器值 | 和界面对数 |
| `q` | 退出 | —— |

故障注入键（`d`/`c`/`s`/`x`）都是**再按一次关闭**。

### 手动设值怎么用

按 `m` 后输入「通道 数值」，回车生效：

```
m                      → 提示输入
2 1500  <回车>          → 压力2 设为 1500
0 276   <回车>          → 温度原始值设为 276（= 27.6℃）
Esc                     → 取消
```

设值会**自动暂停自动模拟**（否则 1 秒后就被覆盖），按 `a` 恢复。

**测报警解除的正确姿势**：模拟数据的正弦幅度只有 ±60，跨不过默认阈值 1300，所以
`r` 跳变并不能制造"报警解除"。用 `m` 直接设值即可：

```
1 1299  → 压力1 应显示「正常」（绿色）
1 1300  → 压力1 应显示「超限」（红色）+ WARN 日志
1 1299  → 应恢复绿色 + NORMAL「警告解除」
```

## 六、注意

- 构建产物 `build/`、`*.o`、`moc_*` 不要提交（加进 `.gitignore`）
- 从站和上位机**串口参数必须完全一致**（本工程 Modbus 模式固定用 8E1）
- 同一对虚拟串口不能两端都填同一个口号（一端 COM1、另一端 COM2）
