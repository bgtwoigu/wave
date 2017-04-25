inherit autotools update-rc.d
DESCRIPTION = "Device specific config"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/${LICENSE};md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r3"

FILESPATH =+ "${WORKSPACE}:"
# Provide a baseline
SRC_URI = "file://mdm-init/"
SRC_URI += "file://wlan_daemon.service"

# Update for each machine
S = "${WORKDIR}/mdm-init/"

do_install_append_msm(){
  install -m 0755 ${S}/wlan_daemon -D ${D}${sysconfdir}/init.d/wlan_daemon
  if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
      install -d ${D}/etc/systemd/system/
      install -m 0644 ${WORKDIR}/wlan_daemon.service -D ${D}/etc/systemd/system/wlan_daemon.service
      install -d ${D}/etc/systemd/system/multi-user.target.wants/
      # enable the service for multi-user.target
      ln -sf /etc/systemd/wlan_daemon.service \
         ${D}/etc/systemd/system/multi-user.target.wants/wlan_daemon.service
  fi
}

FILES_${PN} += "${userfsdatadir}/misc/wifi/*"
FILES_${PN} += "${base_libdir}/firmware/wlan/qca_cld/*"
FILES_${PN} += "${nonarch_base_libdir}/firmware/wlan/qca_cld/* ${sysconfdir}/init.d/* "

BASEPRODUCT = "${@d.getVar('PRODUCT', False)}"

EXTRA_OECONF += "${@base_conditional('BASEMACHINE', 'mdm9607', '--enable-target-mdm9607=yes', '', d)}"
EXTRA_OECONF += "${@base_conditional('BASEMACHINE', 'mdm9650', '--enable-target-mdm9650=yes', '', d)}"
EXTRA_OECONF += "${@base_conditional('BASEMACHINE', 'apq8096', '--enable-target-apq8096=yes', '', d)}"
EXTRA_OECONF += "${@base_conditional('BASEMACHINE', 'apq8009', '--enable-target-apq8009=yes', '', d)}"
EXTRA_OECONF += "${@base_conditional('BASEMACHINE', 'apq8017', '--enable-target-apq8017=yes', '', d)}"

EXTRA_OECONF += "${@base_conditional('BASEMACHINE', 'apq8009', '--enable-pronto-wlan=yes', '', d)}"
EXTRA_OECONF += "${@base_conditional('BASEMACHINE', 'apq8053', '--enable-pronto-wlan=yes', '', d)}"
EXTRA_OECONF += "${@base_conditional('BASEMACHINE', 'apq8017', '--enable-pronto-wlan=yes', '', d)}"

# Enable drone-wlan in place of pronto-wlan for Drones
EXTRA_OECONF_remove = "${@base_conditional('BASEPRODUCT', 'drone', '--enable-pronto-wlan=yes', '', d)}"
EXTRA_OECONF += "${@base_conditional('BASEPRODUCT', 'drone', '--enable-drone-wlan=yes', '', d)}"
EXTRA_OECONF_remove = "${@base_conditional('BASEPRODUCT', 'qsap', '--enable-pronto-wlan=yes', '', d)}"
EXTRA_OECONF += "${@base_conditional('BASEPRODUCT', 'qsap', '--enable-snap-wlan=yes', '', d)}"
EXTRA_OECONF += "${@base_conditional('BASEPRODUCT', 'qsap', '--enable-qsap-wlan=yes', '', d)}"

INITSCRIPT_NAME   = "wlan_daemon"
INITSCRIPT_PARAMS = "remove"
INITSCRIPT_PARAMS_apq8009 = "start 98 5 . stop 2 0 1 6 ."
INITSCRIPT_PARAMS_apq8053 = "start 98 5 . stop 2 0 1 6 ."
INITSCRIPT_PARAMS_apq8017 = "start 98 5 . stop 2 0 1 6 ."
INITSCRIPT_PARAMS_apq8096 = "${@base_conditional('BASEPRODUCT', 'drone', 'start 01 5 . stop 2 0 1 6 .', 'start 98 5 . stop 2 0 1 6 .', d)}"
