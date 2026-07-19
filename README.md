# 音频格式转换器

基于 C++ 和 FFmpeg 的 Windows 桌面音频格式转换工具，支持主流音频格式互转。

## 功能特性

- 支持 7 种主流格式互转：MP3 / WAV / FLAC / AAC (M4A) / OGG Vorbis / WMA / Opus
- 有损格式支持 4 档音质：低 (128k) / 标准 (192k) / 高 (256k) / 最佳 (320k)
- 无损格式自动切换为无损模式
- 拖拽文件到窗口即可添加
- 转换中可随时取消
- 窗口支持最大化/缩放，控件自适应布局
- 完整的中文用户界面

## 系统要求

- Windows 10 / 11 (64 位)
- 无需提前安装环境，下载安装包后自动安装环境

## 下载安装

前往 [Releases](https://github.com/DWK66614/AudioConverter/releases) 页面下载 `AudioConverter_Setup.exe` 安装包。

安装包自动创建桌面快捷方式和开始菜单项，附带卸载程序。

## 本地编译

### 依赖

- Visual Studio 2022+ Build Tools 或 MinGW-w64
- NSIS (可选，用于生成安装包)

### 编译

```batch
build.bat
```

编译产出 `AudioConverter.exe` 和 `AudioConverter_Setup.exe`（如已安装 NSIS）。

## 项目结构

```
AudioConverter/
├── main.cpp          # Win32 GUI 主程序 + FFmpeg 转换逻辑
├── resource.h        # 资源 ID 定义
├── resource.rc       # 图标 + 版本信息
├── setup.nsi         # NSIS 安装脚本
├── build.bat         # 一键构建脚本
└── app.ico           # 应用图标
```

## 技术栈

- C++17 / Win32 API
- FFmpeg 命令行后端
- NSIS 安装包
- Visual Styles (现代 Windows 控件风格)

## 许可证

MIT
