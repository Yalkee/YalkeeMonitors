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
            <h1><b><a href="https://yalkee.github.io/YalkeeMonitors/">Yalkee Monitors</a></b></h1>
            <p><strong>Аппаратное управление яркостью, контрастом и RGB-каналами гаммы мониторов</strong></p>
        </td>
     </tr>
</table>

<div align="center">

[![Info](https://img.shields.io/badge/Info_/_Download-HTML-pink.svg)](https://yalkee.github.io/YalkeeMonitors/)
[![Version](https://img.shields.io/badge/last_version-1.1.0-pink.svg)](https://github.com/yalkee/YalkeeMonitors/releases)
![Windows](https://img.shields.io/badge/Windows-10/11-pink.svg)
[![License](https://img.shields.io/badge/license-MIT-pink.svg)](LICENSE)

</div>

---

### 💭 Как я пришёл к созданию этой программы (история)

Было тяжело сидеть ночью за ярким монитором. Да, я использую профили на мониторе переключая их. Но более удобно сделать это через софт. Не смог найти миниатюрный простой софт, везде перебор иисчерпывающие настройки. Решил углубиться в DDC/CI самостоятельно, и получилось! 

---

## 🖥️ О программе

**Yalkee Monitors** - лёгкая Windows-утилита для управления параметрами мониторов через DDC/CI и гамма-коррекцию видеокарты. Позволяет настраивать яркость, контраст и **(начиная с версии 1.1)** раздельные RGB-каналы для каждого подключённого монитора. Настройки сохраняются в реестре и могут автоматически применяться при старте системы. Переключение аппаратных профилей монитора через DDC/CI практически невозможно, поэтому программа работает со стандартным профилем и изменяет его параметры напрямую.

`Программу держать постоянно запущенной - не нужно`

## 💥 Возможности

- 🔆 Аппаратное управление яркостью/контрастом (DDC/CI)
- 🎨 Программная регулировка RGB-каналов (гамма-коррекция)
- 💾 Индивидуальные настройки для каждого монитора (сохраняются в реестре)
- 🚀 Автозагрузка с тихим режимом - фильтр применяется при входе в Windows и сразу закрывается
- 🗑️ Кнопка сброса всех RGB-настроек (+ удаления из реестра)
- 🖱️ Минималистичный интерфейс на Dear ImGui с DirectX 11

## 🔧 Системные требования

- Windows 10 / 11 (64-bit)
- DirectX 11
- Монитор с поддержкой DDC/CI (для аппаратной яркости/контраста)

## 🧩 Использованные библиотеки
- **[Dear ImGui](https://github.com/ocornut/imgui)** - интерфейс
- **[stb_image.h](https://github.com/nothings/stb)** - загрузка изображений
- **Dxva2.dll** - API мониторов (в составе Windows)

## ❗Проверено на:

<div align="center">

### ` DELL U221Ht; Cooler Master GM238-FFS; ...`

### ` Howens CX133TP-C; ...`

</div>

**Вашей модели нет в списке?**  
[Создайте issue](https://github.com/yalkee/YalkeeMonitors/issues/new?title=Модель%20монитора:%20[ваша%20модель]&body=Пожалуйста,%20напишите%20модель%20вашего%20монитора%20и%20кратко%20опишите,%20работает%20ли%20программа%20с%20ней), и я добавлю в список!

---

<div align="center">

**© 2026 Yalkee** - [Лицензия MIT](LICENSE)

</div>