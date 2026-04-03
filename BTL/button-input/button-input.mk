BUTTON_INPUT_VERSION = 1.0
BUTTON_INPUT_SITE = $(BUTTON_INPUT_PKGDIR)/src
BUTTON_INPUT_SITE_METHOD = local

# Quan trọng: Thêm $(LINUX_MAKE_FLAGS) để truyền đúng CROSS_COMPILE và ARCH
define BUTTON_INPUT_BUILD_CMDS
	$(MAKE) -C $(LINUX_DIR) $(LINUX_MAKE_FLAGS) M=$(@D) modules
endef

define BUTTON_INPUT_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/button_input.ko $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/extra/button_input.ko
endef

$(eval $(kernel-module))
$(eval $(generic-package))
