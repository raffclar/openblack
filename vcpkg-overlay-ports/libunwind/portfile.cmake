# The upstream project moved the v1.8.3 tag to a different commit. The source files did not change, but the
# archive that GitHub makes for the tag changed, so its hash changed. A commit gives an archive that does not
# change. This commit is the v1.8.3 release (source tree 50ad40e10047fd26aa1c94c2c38e02e453c8dde4).
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO "libunwind/libunwind"
    REF 7bda80a564cddc5b215a84509026030cf16dfd94
    HEAD_REF master
    SHA512 e41b3b4bf559d4ed97863d21838b1d729afb1edd4d4e566a102bbc35555d5e4db3233aa593d4d6c393404851bdf631224aa72996586e0cbe581fcc7edcc5c5bd
    PATCHES
        liblzma.diff
)

vcpkg_find_acquire_program(PKGCONFIG)

vcpkg_configure_make(
    SOURCE_PATH "${SOURCE_PATH}"
    AUTOCONFIG
    OPTIONS
        --disable-documentation
        --disable-tests
        --disable-zlibdebuginfo
        --enable-minidebuginfo
)
vcpkg_install_make()
vcpkg_fixup_pkgconfig()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
