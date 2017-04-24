# Disable crash report
OPTION_CRASH_REPORT = n
ifeq ($(BOARD),orb)
# Amulet doesn't have a JTAG, so we default to USB
OPTION_FLASH_CONFIGS = "usb_full"
endif
