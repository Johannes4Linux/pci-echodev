# Emulating a PCI Device with QEMU

This repository contains a PCI Device for QEMU and a driver for it.

## pci-echodev

The PCI devices comes with two memory space BARs. BAR1 is 4kB in size and offers
memory storage. Values written to it, will be stored and can be read back.

BAR0 is also memory space and is diveded in four 4 Byte registers according to
the following table:

| Offset | Name       | Default Value | Description                                |
|--------|------------|---------------|--------------------------------------------|
| 0x0    | ID         | 0xcafeaffe    | ID for identification                      |
| 0x4    | INVERT     | 0             | The inverted writen value can be read back |
| 0x8    | IRQ CTRL   | 0             | Can be used to fire and acknowledge IRQs   |
| 0xC    | RANDOM VAL | 0             | Returns a random value on read             |

## Driver

Tbd.

## Resources

[implementation of a custom QEMU PCI
device](https://www.linkedin.com/pulse/implementing-custom-qemu-pci-device-nikos-mouzakitis/)
by [Nikos Mouzakitis](https://www.linkedin.com/in/nikos-mouzakitis-526b99aa/)
