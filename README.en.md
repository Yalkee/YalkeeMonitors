<div align="center">
  <h6>Выберите язык / Select language</h6>
  <p>
    <a href="README.md"><img src="https://img.shields.io/badge/Русский-RU-skyblue" alt="Русский"></a>
    <a href="README.en.md"><img src="https://img.shields.io/badge/English-GB-skyblue" alt="English"></a>
  </p>
</div>

---

<table border="20" align="center">
     <tr>
         <td>
            <img src="https://github.com/somenmi/images/raw/main/Yalkee_Corporation/YalkeeMonitors/logo.png" width="120" height="120">
         </td>
        <td align="center">
            <h1><b>Yalkee Monitors</b></h1>
            <p><strong>Hardware control of brightness, contrast and RGB gamma channels for monitors</strong></p>
        </td>
     </tr>
</table>

<div align="center">

[![Info](https://img.shields.io/badge/Info_/_Download-HTML-pink.svg)](https://yalkee.github.io/YalkeeMonitors/)
[![Version](https://img.shields.io/badge/version-1.1.0-pink.svg)](https://github.com/yalkee/YalkeeMonitors/releases)
![Windows](https://img.shields.io/badge/Windows-10/11-pink.svg)
[![License](https://img.shields.io/badge/license-MIT-pink.svg)](LICENSE)

</div>

---

### 💭 How I came to create this program (story)

It was tough sitting at a bright monitor at night. Yes, I use monitor profiles, switching between them, but it's much more convenient to do it through software. I couldn't find a tiny, simple tool – everywhere there was an overabundance of exhaustive settings. I decided to dive into DDC/CI on my own – and it worked!

---

## 🖥️ About the program

**Yalkee Monitors** is a lightweight Windows utility for controlling monitor parameters via DDC/CI and graphics card gamma correction. It allows you to adjust brightness, contrast and **(starting from version 1.1)** individual RGB channels for each connected monitor. Settings are saved in the registry and can be automatically applied at system startup. Switching hardware monitor profiles via DDC/CI is practically impossible, so the program works with the standard profile and changes its parameters directly.

`You don't need to keep the program running all the time`

## 💥 Features

- 🔆 Hardware brightness/contrast control (DDC/CI)
- 🎨 Software RGB channel adjustment (gamma correction)
- 💾 Individual settings for each monitor (saved in the registry)
- 🚀 Auto-start with silent mode – the filter is applied at Windows login and then the program closes immediately
- 🗑️ Reset button for all RGB settings (+ removal from the registry)
- 🖱️ Minimalist interface based on Dear ImGui with DirectX 11

## 🔧 System requirements

- Windows 10 / 11 (64-bit)
- DirectX 11
- Monitor with DDC/CI support (for hardware brightness/contrast)

## 🧩 Libraries used

- **[Dear ImGui](https://github.com/ocornut/imgui)** – interface
- **[stb_image.h](https://github.com/nothings/stb)** – image loading
- **Dxva2.dll** – monitor API (included with Windows)

## ❗ Tested on:

<div align="center">

### ` DELL U221Ht; Cooler Master GM238-FFS; ...`

### ` Howens CX133TP-C; ...`

</div>

**Is your model not listed?**  
[Create an issue](https://github.com/yalkee/YalkeeMonitors/issues/new?title=Monitor%20model:%20[your%20model]&body=Please%20write%20your%20monitor%20model%20and%20briefly%20describe%20whether%20the%20program%20works%20with%20it), and I will add it after verification!

---

<div align="center">

**© 2026 Yalkee** – [MIT License](LICENSE)

</div>