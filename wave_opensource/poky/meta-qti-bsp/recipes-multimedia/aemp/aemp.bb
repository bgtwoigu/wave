DESCRIPTION = "Naver AEMP application"
SECTION = "multimedia"
LICENSE = "Proprietary"
LIC_FILES_CHKSUM = "file://../LICENSE;md5=d41d8cd98f00b204e9800998ecf8427e"
DEPENDS = "wpa-supplicant-qcacld system-media libhardware pulseaudio qsthw-api"
RDEPENDS_${PN} = "wpa-supplicant-qcacld system-media libhardware pulseaudio-server qsthw-api-dev libcurl ffmpeg"
PR = "r3"

SRC_URI = "file://from_customer/ \
           file://aemp.service \
           file://LICENSE"

S = "${WORKDIR}/from_customer/"

FILES_${PN} += "/naver"
FILES_${PN} += "/etc/systemd/system/aemp.service"
FILES_${PN} += "/etc/systemd/system/multi-user.target.wants/aemp.service"
FILES_${PN}-dbg += "naver/aemp/.debug"

do_install() {
    if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
        install -d ${D}/etc/systemd/system/
        install -m 0644 ${WORKDIR}/aemp.service -D ${D}/etc/systemd/system/aemp.service
        install -d ${D}/etc/systemd/system/multi-user.target.wants/
        # enable the service for multi-user.target
        ln -sf /etc/systemd/system/aemp.service \
            ${D}/etc/systemd/system/multi-user.target.wants/aemp.service
    fi
}

do_install_append() {
       cp -rf ${S}. ${D}
}

INSANE_SKIP_${PN} = "dev-deps ldflags"
