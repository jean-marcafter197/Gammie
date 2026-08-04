<div align="center">
    <br/>
    <p>
        <img src="gammie icon.png" title="Gammie" alt="Gammie logo" width="100" />
    </p>
    <p>
        I'll change ur gamma
        <br/>
</div>

# Gammie

Gammie is a lightweight Windows utility that lets you adjust your display’s gamma, contrast, and brightness. You can save your preferred settings as presets and switch between them using hotkeys.

---
<div align="left">
    <br/>
    <p>
        <img src="Gammie app.png" title="Gammie" alt="Gammie app image" width="500" />
    </p>
</div>

## 📦 Getting Gammie

You can run Gammie in several ways:

1. **Releases**
   Download the pre-compiled executable from the  **[Releases](https://github.com/daiich/gammie/releases/latest)**   page.

2. **Build with script**
   Download the source code and run `build.bat` (see build instructions below).

3. **Manual build**
   Download the source code and follow the step-by-step instructions below.

---

## 🏗️ Build Instructions

### Prerequisites

* Visual Studio 2019 or 2022 (with *Desktop Development with C++* workload)
  **OR** MinGW-w64
* CMake 3.16 or newer (included with Visual Studio or installed separately)

---

### Option 1: Using `build.bat`

1. Open the project folder in Windows Explorer.
2. Double-click `build.bat`.

The script will automatically:

* Download dependencies (GLFW, Dear ImGui)
* Configure the project
* Compile the program

The final executable will be located at:
`build/Release/Gammie.exe`

---

### Option 2: Command Line

1. Download and extract the source files.
2. Copy the folder path.
3. Open Command Prompt and run:

```bash
cd folder path
```

4. Create and enter the build directory:

```bash
mkdir build
cd build
```

5. Generate build files:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

6. Compile the project:

```bash
cmake --build . --config Release
```

The compiled executable will be located at:
`build/Release/Gammie.exe`

## ⚠️ Windows Defender Warning⚠️

Some antivirus programs may flag Gammie as a false positive.  
This is common for new or unsigned applications and does not necessarily indicate malicious behavior.

### Transparency

- The release executable is automatically built with GitHub Actions, so it always matches the source code in this repository.
- You can inspect the full build logs of the latest release [here](https://github.com/Mokkito/Gammie/actions/runs/30944794348) or all of the build logs [here](https://github.com/mokkito/Gammie/actions)
  

### Build it yourself

- Build from source using the instructions above  

Gammie does not collect data or connect to the internet.

## 🔍 VirusTotal

![VirusTotal](https://img.shields.io/badge/VirusTotal-2%2F71%20detections-brightgreen) ![](https://img.shields.io/badge/Updated%20%2004/08/2026-22:00-8A2BE2)

[View full scan on VirusTotal](https://www.virustotal.com/gui/file/7725f760e03e738057046e81d3fddfc21d3310bdf8125471e593ca1cac29fcc8?nocache=1)
