#!/usr/bin/env bash
# ==============================================================================
# Redmi 9A (blossom / dandelion) Native Linux Unified Build Script
# ==============================================================================
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="${ROOT_DIR}/out-linux"
BUILD_TOOLS="${ROOT_DIR}/build-tools"
ROOTFS_DIR="${ROOTFS_DIR:-${ROOT_DIR}/../alpine-rootfs}"

# Terminal styling
BOLD="\033[1m"
GREEN="\033[0;32m"
BLUE="\033[0;34m"
YELLOW="\033[0;33m"
RED="\033[0;31m"
RESET="\033[0m"

log_info() { echo -e "${BLUE}${BOLD}[+]${RESET} $*"; }
log_ok()   { echo -e "${GREEN}${BOLD}[✔]${RESET} $*"; }
log_warn() { echo -e "${YELLOW}${BOLD}[!]${RESET} $*"; }
log_err()  { echo -e "${RED}${BOLD}[✘]${RESET} $*" >&2; }

show_help() {
    echo -e "${BOLD}Usage:${RESET} ./build.sh [COMMAND] [OPTIONS]

${BOLD}Commands:${RESET}
  boot              Build complete boot image (default)
  kernel            Compile Linux kernel (Image.gz & DTBs)
  ramdisk           Pack native initramfs (out-linux/initramfs.cpio.gz)
  rootfs            Build 5 GiB Alpine Linux rootfs (linux-rootfs.ext4)
  all               Build kernel, ramdisk, boot image, and rootfs
  flash             Flash boot-linux.img to connected device via fastboot
  console           Monitor USB ACM serial console (/dev/ttyACM*)
  clean [--all]     Clean intermediate build objects (or entire out-linux)
  help, -h, --help  Show this help screen

${BOLD}Outputs:${RESET}
  Boot Image:   ${OUT_DIR}/boot-linux.img
  Rootfs Image: ${ROOTFS_DIR}/linux-rootfs.ext4"
}

cmd_kernel() {
    log_info "Building Linux kernel..."
    bash "${BUILD_TOOLS}/compile_linux.sh"
    log_ok "Kernel build completed: ${OUT_DIR}/arch/arm64/boot/Image.gz"
}

cmd_ramdisk() {
    log_info "Packaging initramfs..."
    bash "${BUILD_TOOLS}/build_linux_ramdisk.sh"
    log_ok "Initramfs created: ${OUT_DIR}/initramfs.cpio.gz"
}

cmd_boot() {
    local stock_img="${1:-}"

    if [ ! -f "${OUT_DIR}/arch/arm64/boot/Image.gz" ]; then
        log_warn "Kernel Image.gz not found, compiling kernel first..."
        cmd_kernel
    fi

    if [ ! -f "${OUT_DIR}/initramfs.cpio.gz" ]; then
        log_warn "Initramfs not found, packaging ramdisk first..."
        cmd_ramdisk
    fi

    log_info "Repacking boot-linux.img..."
    if [ -n "${stock_img}" ]; then
        bash "${BUILD_TOOLS}/package_linux_boot.sh" "${stock_img}"
    else
        bash "${BUILD_TOOLS}/package_linux_boot.sh"
    fi
    log_ok "Boot image ready: ${OUT_DIR}/boot-linux.img"
}

cmd_rootfs() {
    local script="${ROOT_DIR}/../xiaomi-redmi-9a-linux/scripts/build-alpine-rootfs.sh"
    if [ ! -f "${script}" ]; then
        script="${ROOTFS_DIR}/build-alpine-rootfs.sh"
    fi

    if [ -f "${script}" ]; then
        log_info "Building Alpine Linux rootfs (${script})..."
        bash "${script}" "${ROOTFS_DIR}/linux-rootfs.ext4"
        log_ok "Alpine rootfs ready: ${ROOTFS_DIR}/linux-rootfs.ext4"
    else
        log_err "Alpine rootfs build script not found at ${script}"
        exit 1
    fi
}

cmd_flash() {
    local img="${OUT_DIR}/boot-linux.img"
    if [ ! -f "${img}" ]; then
        log_warn "boot-linux.img not found. Building it now..."
        cmd_boot
    fi

    log_info "Checking for Fastboot device..."
    if ! fastboot devices | grep -q 'fastboot'; then
        log_err "No fastboot device detected. Connect your phone in fastboot mode (Vol- + Power)."
        exit 1
    fi

    log_info "Flashing ${img} to boot partition..."
    fastboot flash boot "${img}"
    log_ok "Boot image flashed successfully!"
    read -r -p "Reboot device now? [Y/n] " choice
    if [[ "${choice:-Y}" =~ ^[Yy]$ ]]; then
        fastboot reboot
    fi
}

cmd_clean() {
    bash "${BUILD_TOOLS}/clean.sh" "$@"
}

cmd_console() {
    bash "${BUILD_TOOLS}/capture_acm.sh"
}

# Entrypoint
ACTION="${1:-boot}"
shift 2>/dev/null || true

case "${ACTION}" in
    boot)
        cmd_boot "$@"
        ;;
    kernel)
        cmd_kernel "$@"
        ;;
    ramdisk)
        cmd_ramdisk "$@"
        ;;
    rootfs)
        cmd_rootfs "$@"
        ;;
    all)
        cmd_kernel
        cmd_ramdisk
        cmd_boot
        cmd_rootfs
        log_ok "All components built successfully!"
        ;;
    flash)
        cmd_flash "$@"
        ;;
    console)
        cmd_console "$@"
        ;;
    clean)
        cmd_clean "$@"
        ;;
    help|-h|--help)
        show_help
        ;;
    *)
        log_err "Unknown command: ${ACTION}"
        show_help
        exit 1
        ;;
esac
