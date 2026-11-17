# YT8531C

Driver support for the Motorcomm YT8531C Gigabit Ethernet PHY.

The repository contains the portable PHY driver, its Linux kernel
integration, a userspace utility for controlling the PHY, and the test
suite.

## Tested hardware

The driver has been tested on the
[Orange Pi 3 LTS](https://armbian.com/boards/orangepi3-lts) running
Armbian (sunxi64 kernel): the out-of-tree module builds, loads and
binds against the in-kernel PHY driver, and brings the on-board
YT8531C link up at 1000BASE-T full duplex.

## Building and using the kernel module

Kernel headers, a compiler and CMake are required, for example:

```sh
apt install linux-headers-$(uname -r) build-essential cmake
```

Build the out-of-tree module against the running kernel:

```sh
cmake -S . -B build-kmod \
    -DCMAKE_BUILD_TYPE=Release \
    -DYT8531C_BUILD_TESTS=OFF \
    -DYT8531C_BUILD_FREESTANDING=OFF \
    -DYT8531C_BUILD_LINUX_USER=OFF \
    -DYT8531C_BUILD_LINUX_KMOD=ON \
    -DYT8531C_KERNEL_DIR=/lib/modules/$(uname -r)/build
cmake --build build-kmod --target yt8531c_kmod
```

Install and register it:

```sh
cp build-kmod/kmod/yt8531c.ko /lib/modules/$(uname -r)/extra/
depmod -a
```

The tested Armbian kernel ships a built-in driver for this PHY
(`YT8531 Gigabit Ethernet`) which claims the device at boot, so the
new module must take over after loading.  On the Orange Pi 3 LTS
(interface `end0`, PHY device `stmmac-0:01`):

```sh
ip link set end0 down
echo stmmac-0:01 > '/sys/bus/mdio_bus/drivers/YT8531 Gigabit Ethernet/unbind'
modprobe yt8531c
ip link set end0 up
```

To switch back to the built-in driver:

```sh
ip link set end0 down
echo stmmac-0:01 > '/sys/bus/mdio_bus/drivers/Motorcomm YT8531C/unbind'
modprobe -r yt8531c
echo stmmac-0:01 > '/sys/bus/mdio_bus/drivers/YT8531 Gigabit Ethernet/bind'
ip link set end0 up
```

## License

The project is licensed under GPL-3.0. Files that are built into the Linux
kernel are additionally available under GPL-2.0; see `LICENSE`,
`LICENSE.GPL-2.0`, and the SPDX headers in the source files.
