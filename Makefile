# use `make VESC_TOOL=path/to/your/vesc_tool` to specify custom vesc_tool path
VESC_TOOL ?= vesc_tool
# use `make MINIFY_QML=0` to skip qml minification and pack the qml verbatim
MINIFY_QML ?= 1
# use `make OLDVT=1` to build with old vesc_tool which doesn't support pkgdesc.qml
OLDVT ?= 0
# use `make LCM_FW=1` to embed the SLCM firmware update image (needs
# arm-none-eabi-gcc with newlib-nano). Off by default since rf70: it made the
# package heavy; the package only carries the SLCM version (for CHECK LCM)
# and the rider picks SLCM.hex (lcm-latest release) to flash.
LCM_FW ?= 0

all: refloatrm.vescpkg

refloatrm.vescpkg: src lisp/package.lisp package_README-gen.md ui.qml
ifeq ($(OLDVT), 1)
	"$(VESC_TOOL)" --buildPkg "refloatrm.vescpkg:lisp/package.lisp:ui.qml:0:package_README-gen.md:Re-Float Rink-Mixed"
else
	"$(VESC_TOOL)" --buildPkgFromDesc pkgdesc.qml
endif

src:
	$(MAKE) -C $@

VERSION=`cat version`
PACKAGE_NAME=`cat package_name | cut -c-20`

ifeq ($(strip $(MINIFY_QML)),1)
    MINIFY_CMD="./rjsmin.py"
else
    MINIFY_CMD="cat"
endif

package_README-gen.md: package_README.md version
	cp $< $@
	echo "" >> $@
	echo "### Build Info" >> $@
	echo "- Version: ${VERSION}" >> $@
	echo "- Build Date: `date --rfc-3339=seconds`" >> $@
	echo "- Git Commit: #`git rev-parse --short HEAD`" >> $@

LCM_FW_JSON := lcm/alcm/gcc/build/ALCM_VP_app.json

$(LCM_FW_JSON): $(wildcard lcm/bootloader/*.c lcm/bootloader/*.h lcm/alcm/Library/HK32F030Mxx_Library_V1.1.6/HK32F030M_Project/src/*.c lcm/alcm/Library/HK32F030Mxx_Library_V1.1.6/HK32F030M_Project/inc/*.h)
	$(MAKE) -C lcm/bootloader
	$(MAKE) -C lcm/alcm/gcc VP=1 BL=1
	python3 lcm/tools/lcm_images.py lcm/bootloader/build/lcm_bootloader.hex \
		lcm/alcm/gcc/build/ALCM_VP_BL_app.hex lcm/alcm/gcc/build `sh lcm/tools/lcm_version.sh`

ifeq ($(strip $(LCM_FW)),1)
    LCM_FW_DEP = $(LCM_FW_JSON)
else
    LCM_FW_DEP =
endif

ui.qml: ui.qml.in package_name version $(LCM_FW_DEP)
	python3 lcm/tools/embed_lcm_fw.py $< "$(LCM_FW_DEP)" `sh lcm/tools/lcm_version.sh` | \
	sed "s/{{PACKAGE_NAME}}/${PACKAGE_NAME}/g" | \
	sed "s/{{VERSION}}/${VERSION}/g" | \
	${MINIFY_CMD} > $@

clean:
	rm -f refloatrm.vescpkg package_README-gen.md ui.qml
	rm -rf lcm/bootloader/build lcm/alcm/gcc/build
	$(MAKE) -C src clean

.PHONY: all clean src
