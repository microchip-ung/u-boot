# U-Boot SPL Image for booting Sparx5 in PCIe Endpoint Mode

This branch of Microchip's U-Boot software provides a SPL build image.
The size of the image is around 190KB.

When the image is flashed a boot NOR flash in a Sparx5 EVB  the image will execute and boot the system and configure the
PCIe EP controller.
The execution will then loop and monitor the EP link state, but no OS (e.g. Linux Image) will be booted.

The EP mode will configure Sparx5 as a network device class device with vendor id 0x101b and device id 0xb006.

If you want to change this, you can do so in the `board/mscc/sparx5/sparx5_pcie_ep.c` file.

This SW is provided as-is and you should not expect Microchip to provide any assistance in debugging or enhancing this
software.

## Building the U-Boot SPL image

If you have a Microchip toolchain installed in /opt/mchp on your Linux desktop, you can build the image like this:

    ARCH=arm64 CROSS_COMPILE=/opt/mchp/mscc-toolchain-bin-2024.02-105/arm64-armv8_a-linux-gnu/bin/aarch64-linux- make O=build_sparx5_pcie_ep_arm64 mscc_sparx5_pcb13x_pciep_defconfig
    ARCH=arm64 CROSS_COMPILE=/opt/mchp/mscc-toolchain-bin-2024.02-105/arm64-armv8_a-linux-gnu/bin/aarch64-linux- make O=build_sparx5_pcie_ep_arm64

