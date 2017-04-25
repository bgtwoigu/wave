SUMMARY = "Sensory Truly Handsfree SDK"
SECTION = "multimedia"
DEPENDS = "pulseaudio"
RDEPENDS_${PN} = "pulseaudio-server"
LICENSE = "Proprietary"
LIC_FILES_CHKSUM = "file://License.txt;beginline=1;endline=6;md5=05f67b05a74702d699b73ce35c1c37ef"

SRC_URI = "file://${PN}_${PV}"

S = "${WORKDIR}/${PN}_${PV}"

do_compile() {
    cd arm-yocto_linux/Samples/
    oe_runmake
}

do_install() {
    cd arm-yocto_linux/Samples/PhraseSpot
    # TODO: Install file somewhere
}
