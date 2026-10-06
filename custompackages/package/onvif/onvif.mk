################################################################################
#
# onvif
#
################################################################################

ONVIF_VERSION = v0.1.0
ONVIF_SITE = /src/custompackages/package/onvif
ONVIF_DEPENDENCIES =
ONVIF_CONF_OPTS =
ONVIF_SITE_METHOD = local
ONVIF_GO = /usr/local/bin/go
ONVIF_GO_ENV = GOARCH=mipsle GOOS=linux CGO_ENABLED=0 GOCACHE=$(@D)/.gocache

# No UPX: the binary is paged in from squashfs on demand instead of unpacked into RAM.
define ONVIF_BUILD_CMDS
	cd $(@D)/src; $(ONVIF_GO_ENV) $(ONVIF_GO) build -ldflags "-s -w" -trimpath -o onvif
endef

define ONVIF_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/onvif $(TARGET_DIR)/usr/bin/onvif
endef

$(eval $(generic-package))
