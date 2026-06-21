# C++ Technology Stack

本文档逐项回应 `c++技术栈.md` 中的所有模块，给出选择方案和理由。

**选择原则**: Qt-first（已有则优先用 Qt）、header-only 优先、BSD/MIT 协议、GitHub 活跃维护。

---

## 1. C++ 标准库

| 候选项 | 选择 | 理由 |
|--------|------|------|
| Apache C++ Standard Library | 否 | 已停止维护 10+ 年 |
| **Boost** | **按需引入** | 全套太重（150+ 库），Qt 已覆盖大部分功能。只在确实需要时按模块引入（如 Boost.GIL 图形处理） |
| Folly | 否 | Facebook 内部库，依赖重，不适合通用模板 |
| JUCE | 否 | 与 Qt 定位重叠（UI 框架），已有 Qt |

**选择**: C++17 标准库 + Qt6 + 按需 Boost 模块

---

## 2. 用户界面

| 候选项 | 选择 | 理由 |
|--------|------|------|
| **Qt (QWidget/QMainWindow)** | **已集成** | 自定义 Docking + Room + Panel 系统，8 套 QSS 主题 |
| Ncurses | 否 | 终端 UI，桌面应用不需要 |
| imgui | 否 | 即时模式，适合调试/工具窗口，不适合生产级主界面 |
| CEGUI | 否 | 老旧，社区萎缩 |

**选择**: Qt6 Widgets（已集成）

---

## 3. 数据管理

| 候选项 | 选择 | 理由 |
|--------|------|------|
| **SQLite (sqlite3)** | **P1 推荐** | 零配置嵌入式，桌面应用首选。通过 Qt SQL 模块统一 API |
| MySQL Connector/C++ | 否 | 桌面应用通常不需要客户端-服务器数据库 |
| PostgreSQL (libpq) | 否 | 同上，需要额外安装和配置 |
| Redis (hiredis) | 否 | 缓存场景，桌面应用不常用 |
| google/protobuf | 否 | 跨语言 RPC 才需要，桌面应用 JSON 够用 |
| Boost.Serialization | 否 | Qt QDataStream 覆盖二进制序列化 |
| Qt ORM | 否 | Qt 无官方 ORM。QSqlTableModel 足够，不需要 ORM |
| Pistache ORM | 否 | Web 框架，不适用 |
| Hibernate for C++ | 否 | 不存在成熟实现 |

**选择**: Qt SQL + SQLite（QSqlDatabase/QSqlQuery）

```cmake
find_package(Qt6 REQUIRED COMPONENTS Sql)
target_link_libraries(myapp PRIVATE Qt6::Sql)
```

---

## 4. 网络通信

| 候选项 | 选择 | 理由 |
|--------|------|------|
| **Boost.Asio** | 否 | 功能强大但 API 复杂。桌面应用用 Qt Network 足够 |
| **cURL (libcurl)** | 否 | Qt Network 已封装 HTTP/HTTPS |
| WebSocket | **Qt WebSockets** | 如需 WebSocket，用 Qt 内置模块 |
| Oat++ | 否 | Web 框架，桌面应用不需要 |
| CppCMS | 否 | Web 框架 |
| Drogon | 否 | Web 框架 |
| Boost.Beast | 否 | HTTP 库，Qt Network 已覆盖 |
| Serial Communication | **QSerialPort** | 如需要串口通信，用 Qt 内置模块 |

**选择**: Qt Network（QNetworkAccessManager，已集成）

```cmake
find_package(Qt6 REQUIRED COMPONENTS Network)
target_link_libraries(myapp PRIVATE Qt6::Network)
```

---

## 5. 安全性

| 候选项 | 选择 | 理由 |
|--------|------|------|
| **OpenSSL** | **Qt Network 已封装** | Qt Network 内置 TLS/SSL 支持，不需要直接调用 OpenSSL API |
| GmSSL | 否 | 国密场景才需要，通用模板不带 |

**选择**: Qt Network 内置 TLS（不需要额外库）

---

## 6. 日志和监控

| 候选项 | 选择 | 理由 |
|--------|------|------|
| **spdlog** | **P0 必须** | Header-only，高性能异步，多 sink，C++20 风格 API |
| Boost.Log | 否 | 需要链接 Boost，编译慢，配置繁琐 |

**选择**: spdlog

```cmake
find_package(spdlog REQUIRED)
target_link_libraries(myapp PRIVATE spdlog::spdlog)
```

---

## 7. 配置管理

| 候选项 | 选择 | 理由 |
|--------|------|------|
| Boost.PropertyTree | 否 | 已有替代方案 |
| INIh | 否 | 模板已用 XML 替代 INI |

**选择**: QXmlStreamReader/Writer（已集成），配合 nlohmann/json 用于 JSON 配置

---

## 8. 错误处理

| 候选项 | 选择 | 理由 |
|--------|------|------|
| std::exception | **已使用** | C++ 标准异常类 |
| 自定义异常 | **推荐** | 按项目需要派生 `std::runtime_error` |

**选择**: C++ 标准异常体系，无需额外库

---

## 9. 任务调度

| 候选项 | 选择 | 理由 |
|--------|------|------|
| Boost.Asio (steady_timer) | 否 | Qt 已有定时器 |
| cpp-taskflow | 否 | 强大但桌面应用通常不需要复杂任务图 |
| C++ Task Scheduler | 否 | 同上 |
| **Qt Concurrent** | **已集成** | QtConcurrent::run() 简洁够用 |
| Croncpp | 否 | cron 表达式解析，特定场景才需要 |

**选择**: Qt Concurrent + QTimer + std::async

---

## 10. 数据导入导出

| 候选项 | 选择 | 理由 |
|--------|------|------|
| RapidJSON | 否 | nlohmann/json 更易用 |
| **nlohmann/json** | **P0 必须** | 43k+ stars，Header-only，最流行的 C++ JSON 库 |
| boost | 否 | 不做 JSON 解析 |

**选择**: nlohmann/json

```cmake
find_package(nlohmann_json REQUIRED)
target_link_libraries(myapp PRIVATE nlohmann_json::nlohmann_json)
```

---

## 11. 测试和质量保证

| 候选项 | 选择 | 理由 |
|--------|------|------|
| **Google Test** | **P1 推荐** | 业界标准，CMake FetchContent 一行拉取 |
| Catch2 | 备选 | 也不错，单头文件更轻量 |
| Boost.Test | 否 | 需要 Boost |

**选择**: Google Test

```cmake
include(FetchContent)
FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.15.0)
FetchContent_MakeAvailable(googletest)
```

---

## 12. 插件和扩展

| 候选项 | 选择 | 理由 |
|--------|------|------|
| **Qt Plugin Framework** | **推荐** | 已有 Qt，原生支持插件加载 |
| Boost.DLL | 备选 | 跨平台动态库加载 |
| Poco Libraries | 否 | 太大，功能与 Qt 重叠 |
| Oat++ Plugin System | 否 | Web 框架 |

**选择**: Qt Plugin Framework（QPluginLoader）

---

## 13. 帮助和文档

| 候选项 | 选择 | 理由 |
|--------|------|------|
| **Doxygen** | **P1 推荐** | 从注释自动生成 API 文档 |

---

## 14. 版本控制

| 候选项 | 选择 | 理由 |
|--------|------|------|
| libgit2 | 否 | 桌面应用通常不需要内嵌 Git |
| Boost.Version | 否 | 版本号管理用 CMake `project(VERSION)` |

**选择**: CMake 内置版本管理，不需要库

---

## 15. 性能监控和分析

| 候选项 | 选择 | 理由 |
|--------|------|------|
| gperftools | 否 | 开发时工具，不嵌入应用 |
| Valgrind | 否 | Linux 内存调试工具，不嵌入应用 |
| **Google Benchmark** | **P2 可选** | 微基准测试 |

**选择**: 开发时使用外部工具（Valgrind/gperftools），不嵌入模板

---

## 16. API 文档生成

| 候选项 | 选择 | 理由 |
|--------|------|------|
| **Doxygen** | **P1 推荐** | 同 #13 |

---

## 17. 多线程

| 候选项 | 选择 | 理由 |
|--------|------|------|
| Boost.Thread | 否 | C++11 已有 std::thread |
| Intel TBB | 否 | 高性能计算场景，桌面应用不需要 |
| OpenMP | 否 | 科学计算场景 |
| C++20 Coroutines | **推荐** | C++20 原生协程 |
| **Qt Concurrent** | **已集成** | QtConcurrent::run() / QThreadPool |
| **std::thread / std::async** | **已使用** | C++17 标准库 |

**选择**: std::thread + Qt Concurrent，C++20 项目可用协程

---

## 18. 国际化/本地化

| 候选项 | 选择 | 理由 |
|--------|------|------|
| **Qt Internationalization** | **已集成** | QTranslator + .ts/.qm，热切换 |
| Boost.Locale | 否 | Qt 已覆盖 |
| ICU4C | 否 | 太重（20MB+），Qt 已处理 Unicode |

**选择**: Qt QTranslator（已集成）

---

## 19. 主题和样式

| 候选项 | 选择 | 理由 |
|--------|------|------|
| **Qt Style Sheets** | **已集成** | 8 套 QSS 主题 + ThemeManager + SvgIconEngine |
| imgui Themes | 否 | 不是 Qt 生态 |
| JUCE Look & Feel | 否 | 不是 Qt 生态 |
| NanoGUI Themes | 否 | 不是 Qt 生态 |

**选择**: Qt QSS + ThemeManager（已集成）

---

## 20. 数据加密

| 候选项 | 选择 | 理由 |
|--------|------|------|
| OpenSSL | **按需** | 如需 AES/RSA 直接调用，可引入 OpenSSL。Qt Network 已封装 TLS |
| GmSSL | 否 | 国密场景才需要 |

**选择**: 需要时引入 OpenSSL，模板不带

---

## 21. 用户认证

**选择**: 业务逻辑，不嵌入框架。需要时用 OpenSSL + JWT (nlohmann/json)

---

## 22. 性能基准测试

| 候选项 | 选择 | 理由 |
|--------|------|------|
| **Google Benchmark** | **P2 可选** | 微基准测试，非运行时监控 |

---

## 23. 环境配置

| 候选项 | 选择 | 理由 |
|--------|------|------|
| Boost.Program_options | 否 | Qt QCommandLineParser 已覆盖 |
| **CMake** | **已使用** | CMake 管理构建环境 |

**选择**: CMake + QCommandLineParser + XML/JSON 配置文件

---

## 24. 用户友好的错误提示

**选择**: 业务逻辑，不嵌入框架。用 Qt QMessageBox + spdlog

---

## 25. 数据迁移

**选择**: 业务逻辑，不嵌入框架。需要时用 Qt SQL 迁移脚本

---

## 26. 内置帮助文档

**选择**: 嵌入 `docs/` Markdown 文档，或使用 Qt Assistant (QHelpEngine)

---

## 27. 用户反馈机制

**选择**: 业务逻辑，不嵌入框架。可通过 Qt Network + nlohmann/json 对接 API

---

## 28. 版本升级

| 候选项 | 选择 | 理由 |
|--------|------|------|
| Boost.Version | 否 | CMake project(VERSION) 管理版本号 |
| 软件升级模块 | **按需** | 用 Qt Network 下载更新包 |

**选择**: CMake 版本管理 + Qt Network 在线更新检测

---

## 29. 脚本支持

| 候选项 | 选择 | 理由 |
|--------|------|------|
| Python (boost.python) | 否 | 嵌入 Python 太重（~30MB） |
| JavaScript (V8/Duktape) | 否 | V8 太大，Duktape 小众 |
| **Lua (sol2 + LuaJIT)** | **P2 可选** | 极轻量（~200KB），启动快，适合嵌入式脚本/插件 |

**选择**: sol2 + LuaJIT（需要脚本能力时引入）

---

## 30. 事件通知

| 候选项 | 选择 | 理由 |
|--------|------|------|
| Boost.Signals2 | 否 | Qt 信号/槽已覆盖 |
| libev/libhv/libuv | 否 | 事件循环库，Qt 已有 QEventLoop |
| **Qt Signals/Slots** | **已使用** | 类型安全的观察者模式 |

**选择**: Qt 信号/槽（已集成）

---

## 31. 压缩

| 候选项 | 选择 | 理由 |
|--------|------|------|
| **zlib** | **P2 可选** | 最通用的压缩库，Qt 已有 qCompress (zlib 封装) |
| quicklz | 否 | 小众，zlib 已够 |

**选择**: Qt 内置 qCompress/qUncompress（zlib 封装），需要更强控制时直接引入 zlib

---

## 32. 图形处理

| 候选项 | 选择 | 理由 |
|--------|------|------|
| Boost.GIL | 否 | 功能有限 |
| Magick++ | 否 | ImageMagick 绑定，太重 |
| **OpenCV** | **按需** | 只有图像处理/计算机视觉场景才需要（400MB+ 依赖） |
| **Qt QImage/QPixmap** | **已集成** | 基础图像加载/缩放/格式转换 |

**选择**: Qt QImage/QPixmap 已覆盖基础图像处理，不嵌入 OpenCV

---

## 33. 文件保存/序列化

| 候选项 | 选择 | 理由 |
|--------|------|------|
| cereal | 否 | 二进制/JSON 序列化，nlohmann/json + QDataStream 已覆盖 |
| msgpack | 否 | 跨语言 RPC 场景 |
| protobuf | 否 | 需要 .proto 编译步骤 |
| Boost.Serialization | 否 | Boost 依赖 |
| FlatBuffers | 否 | 游戏/高性能场景 |

**选择**: nlohmann/json（文本）+ QDataStream（二进制），已覆盖

---

## 34. PDF 处理

| 候选项 | 选择 | 理由 |
|--------|------|------|
| QPDF | 否 | 命令行工具，不嵌入 |
| Poppler | 备选 | Qt 绑定可用，Linux 友好 |
| MuPDF | 备选 | 轻量，多平台 |
| PDFium | 否 | Chromium 的 PDF 引擎，太大 |
| libharu | **P2 可选** | 轻量 PDF 生成库 |
| PoDoFo | 否 | 功能有限 |

**选择**: libharu（PDF 生成）或 MuPDF（PDF 渲染），需要时引入

---

## 35. 文本编辑器

| 候选项 | 选择 | 理由 |
|--------|------|------|
| **Scintilla** | **P2 可选** | 最成熟的代码编辑器控件（Notepad++/SciTE 同款）。Qt 有 QScintilla 绑定 |
| **QPlainTextEdit** | **已集成** | 基础文本编辑，代码高亮需额外实现 |

**选择**: QPlainTextEdit（基础场景），Scintilla/QScintilla（代码编辑器场景）

---

## 汇总

```
已集成（P0）:
  Qt6 Widgets/Core/Gui/Svg/Network     UI + 网络
  QXmlStreamReader/Writer               XML 配置
  Qt QTranslator                        i18n 热切换
  ThemeManager + QSS                    主题系统
  CommandManager + DVAction             命令/菜单
  DockLayout + Panel + Room             停靠界面

推荐集成（P0-P1）:
  spdlog                                日志
  nlohmann/json                         JSON
  Qt SQL + SQLite                       数据库
  Google Test                           测试
  Doxygen                               API 文档

按需集成（P2）:
  sol2 + LuaJIT                         脚本
  zlib / qCompress                      压缩
  libharu / MuPDF                       PDF
  Scintilla / QScintilla                代码编辑器
  OpenCV                                图像处理
  OpenSSL                               加密
  QSerialPort                           串口
  Qt WebSockets                         WebSocket
```
