# Native OBS plugin source

[中文](README.zh.md) · [English](README.en.md)

Installation and runtime behavior: [OBS English guide](../../obs-input-overlay/README.en.md) / [OBS 中文安装](../../obs-input-overlay/README.zh.md).

Build with Visual Studio C++ Build Tools and Python. From `source/`, run:

```powershell
.\tools\BuildObsNative.ps1 -ObsRoot "C:\Program Files\obs-studio"
```

The script fetches the OBS 32.2.2 headers, generates an import library from the specified OBS installation's `obs.dll`, and writes the DLL into the project overlay directory. Close OBS before replacing an installed plugin. Source licensing is in [LICENSE.txt](LICENSE.txt).
