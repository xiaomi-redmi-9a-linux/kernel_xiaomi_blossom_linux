# Xiaomi Redmi 9A Linux (`xiaomi-redmi-9a-linux`)

<p align="center">
  <strong>Native GNU/Linux for the Xiaomi Redmi 9A (<code>blossom</code> / <code>dandelion</code> / <code>angelica</code> / <code>cattail</code>)</strong><br>
  Powered by the MediaTek Helio G25 / MT6765 / MT6762G SoC
</p>

<p align="center">
  <a href="https://alpinelinux.org/"><img src="https://img.shields.io/badge/Distribution-Alpine_Linux-0D597F?logo=alpinelinux&logoColor=white" alt="Alpine Linux"></a>
  <a href="https://sxmo.org/"><img src="https://img.shields.io/badge/Mobile_UI-Sxmo_(dwm)-1793d1" alt="Sxmo"></a>
  <a href="https://pipewire.org/"><img src="https://img.shields.io/badge/Audio-PipeWire-blue" alt="PipeWire"></a>
  <a href="https://img.shields.io/badge/GPU-PowerVR_GE8320_3D-green"><img src="https://img.shields.io/badge/GPU-PowerVR_GE8320_3D-green" alt="3D Accelerated"></a>
  <a href="https://img.shields.io/badge/Boot-Safe_MicroSD_(Zero_eMMC_Risk)-success"><img src="https://img.shields.io/badge/Boot-Safe_MicroSD_(Zero_eMMC_Risk)-success" alt="Safe MicroSD Boot"></a>
  <a href="https://img.shields.io/badge/AI_Assisted-Antigravity_/_Google_DeepMind-purple"><img src="https://img.shields.io/badge/AI_Assisted-Antigravity_/_Google_DeepMind-purple" alt="AI Assisted"></a>
</p>

---

## 🌟 Overview

The **`xiaomi-redmi-9a-linux`** organization hosts the complete kernel source code, patched userland libraries, and system overlays required to run full native GNU/Linux on the **Xiaomi Redmi 9A** family of devices.

Instead of running an Android container, chroot, or PRoot, this project brings **true native Linux** (Alpine Linux / postmarketOS with the lightweight **Sxmo** mobile environment) directly to the bare metal, featuring:
- **Non-destructive MicroSD Boot**: Boots completely from an external MicroSD card loop image. The phone's internal eMMC flash (`/dev/mmcblk0*`) is strictly protected and left untouched.
- **Hardware 3D Acceleration**: Custom 64-bit musl compatibility layer in `libhybris` allowing native GLES applications to harness the PowerVR Rogue GE8320 GPU at ~390+ FPS.
- **In-Kernel Power Management**: Dedicated autonomous MT6357 PMIC fuelgauge with internal-resistance compensation and real-time battery status in `/sys/class/power_supply/battery`.
- **Integrated Mobile Audio**: PipeWire & WirePlumber routing directly to the MediaTek MT6357 DL2 audio path with working hardware volume buttons and statusbar indicators.

---

## 📱 Hardware Support Status

| Subsystem | Driver / Implementation | Status | Details |
| :--- | :--- | :---: | :--- |
| **CPU / SoC** | MediaTek MT6765 (Helio G25) | 🟢 **Working** | Octa-core Cortex-A53 with DVFS governor |
| **Display** | SimpleFB / Framebuffer (`/dev/fb0`) | 🟢 **Working** | 720 × 1600 resolution @ 60 Hz |
| **3D GPU Acceleration** | PowerVR Rogue GE8320 (Series 8XE) | 🟢 **Working** | Patched `libhybris` + DDK 1.13, ~395 FPS compute |
| **Touchscreen** | Novatek / FocalTech Multi-touch | 🟢 **Working** | Firmware loaded via kernel initramfs |
| **Audio (Speaker / I2S)** | MediaTek MT6357 ALSA SoC | 🟢 **Working** | PipeWire, hardware volume keys (`mtk-kpd`), Sxmo statusbar |
| **Internal Wi-Fi** | MediaTek WMT (`wlan0`) | 🟢 **Working** | Firmware loaded via `mtk-wmt-loader` |
| **Internal Bluetooth** | MediaTek MT6631 Bluetooth 5.0 | 🟢 **Working** | Native BlueZ 5.86 stack via `mtk-bt-bridge` |
| **Battery / Fuelgauge** | MT6357 In-Kernel Fuelgauge | 🟢 **Working** | Autonomous IR-compensated OCV tracking & smooth stepping |
| **Backlight Control** | MediaTek LCD Backlight (`lcd-backlight`)| 🟢 **Working** | Controlled via `brightnessctl` |
| **Buttons & Input** | MediaTek Keypad (`mtk-kpd`) | 🟢 **Working** | Volume Up/Down, Power button |
| **USB Serial & Network** | Linux ConfigFS (RNDIS & ACM) | 🟢 **Working** | Direct root shell over USB (`ttyGS0`) and USB tethering |
| **Storage / Boot** | MicroSD (`/dev/mmcblk1p1`) | 🟢 **Working** | Non-destructive loopback rootfs; eMMC completely safe |

---

## 🗂️ Core Repositories

| Repository | Purpose |
| :--- | :--- |
| [**`kernel_xiaomi_blossom_linux`**](https://github.com/xiaomi-redmi-9a-linux/kernel_xiaomi_blossom_linux) | Custom Linux kernel tailored for Redmi 9A. Includes in-kernel MT6357 fuelgauge, VKMS PRIME buffer sharing, CMDQ 32-bit compat ioctls, and MicroSD rootfs boot architecture. |
| [**`libhybris`**](https://github.com/xiaomi-redmi-9a-linux/libhybris) | Fork of `libhybris` patched with full **64-bit musl libc** compatibility, Android 10 (Q) dynamic linker relocations, and direct `/dev/fb0` double-buffered gralloc fallback for Treble. |
| [**`xiaomi-redmi-9a-linux`**](https://github.com/xiaomi-redmi-9a-linux/xiaomi-redmi-9a-linux) | Rootfs overlay, automated MicroSD installer scripts, Sxmo device profiles, and system services for Alpine Linux / postmarketOS. |

---

## 🤖 Acknowledgment of AI Assistance

This project has been developed in intensive, continuous collaboration with artificial intelligence:

- **AI Collaboration**: Co-engineered, debugged, and architected with **Antigravity** (powered by **Google DeepMind** models).
- **Scope of AI Involvement**:
  - **Kernel Subsystem Engineering**: Diagnosing and implementing 32-bit `CMDQ_IOCTL_ASYNC_EXEC_32` compatibility ioctls, memory mapping with ION virtual address fallback, and VKMS PRIME buffer export/import.
  - **Power & PMIC Architecture**: Designing the autonomous in-kernel MT6357 battery fuelgauge driver with internal-resistance compensation and state-of-charge calculation, removing the need for Android-specific userspace daemons (`healthd`).
  - **Reverse Engineering & Driver Interop**: Analyzing proprietary Android vendor blobs, diagnosing 64-bit musl libc TLS slot offsets, and writing the direct framebuffer fallback for Android 10 Treble HALs.
  - **System Integration**: Configuring PipeWire ALSA hardware mixers, debugging udev rules, and optimizing Sxmo mobile shell performance.

We believe in radical transparency: high-level systems programming and hardware bring-up on modern SoCs are vastly accelerated when human passion and intuition are paired with state-of-the-art AI reasoning.

---

## 📜 Safety & Disclaimer

This project is experimental and provided for research, hacking, and hobbyist purposes. 

> [!IMPORTANT]
> The kernel and boot scripts are deliberately built with safety guards to **never touch or mount the internal eMMC (`/dev/mmcblk0*`)**. Your stock Android/MIUI operating system, partition table, and userdata remain untouched on the phone's internal storage. Removing the MicroSD card allows the device to boot normally into Android.

---

## 🤝 Contributing

Issues, pull requests, and discussions are welcome! If you own a **Xiaomi Redmi 9A (`blossom` / `dandelion`)**, **Redmi 9C / NFC (`angelica` / `cattail`)**, or any other MT6765/MT6762 device, feel free to join in testing, reporting issues, or porting new features.
