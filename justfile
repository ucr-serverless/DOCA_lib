dev:
    git checkout dev
reset_main:
    git fetch
    git reset --hard origin/main
build:
    meson setup build
    ninja -C build/ -v
format:
    ninja -C build/ clang-format
debug:
    meson setup build --buildtype=debug
    ninja -C build/
