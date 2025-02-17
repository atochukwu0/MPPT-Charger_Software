# Libre Solar Charge Controller Firmware

![build badge](https://travis-ci.com/LibreSolar/charge-controller-firmware.svg?branch=master)

Firmware based on ARM mbed framework for the Libre Solar MPPT/PWM solar charge controllers

## Supported devices

The software is configurable to support different charge controller PCBs with either STM32F072 (including CAN support) or low-power STM32L072/3 MCUs.

- [Libre Solar MPPT 12/24V 20A with CAN (v0.10)](https://github.com/LibreSolar/MPPT-2420-LC)
- [Libre Solar MPPT 12V 10A with USB (v0.2 and v0.4)](https://github.com/LibreSolar/MPPT-1210-HUS)
- [Libre Solar PWM 12/24V 20A](https://github.com/LibreSolar/PWM-2420-LUS)

## Initial software setup (IMPORTANT!)

1. Select the correct board in `platformio.ini` by removing the comment before the board name under `[platformio]`
2. Copy `src/config.h_template` to `src/config.h`, and adjust basic settings

   (`config.h` is ignored by git, so your changes are kept after software updates using `git pull`)

       cp src/config.h_template src/config.h
       $EDITOR src/config.h

## Toolchain and flashing instructions

See the Libre Solar website for project-agnostic instructions on how to
[develop software](http://libre.solar/docs/toolchain) and
[flash new firmware](http://libre.solar/docs/flashing).

### Troubleshooting

#### If flashing the STM32L072 MCU using OpenOCD fails

... as seems to happen with the standard settings from PlatformIO, then ...

*NOTE* This MCU is used in the MPPT 1210 HUS and the PWM charge controller, for example.

Try one of these workarounds:

1. Change OpenOCD settings to `set WORKAREASIZE 0x1000` in the file `~/.platformio/packages/tool-openocd/scripts/board/st_nucleo_l073rz.cfg`.
2. Use ST-Link tools. For Windows there is a GUI tool. Under Linux, use the following command:

       st-flash write .pioenvs/mppt-2420-lc-v0.10/firmware.bin 0x08000000
3. Use other debuggers and tools, e.g. Segger J-Link.

#### If flashing fails like this ...

In PlatformIO:

```
Error: init mode failed (unable to connect to the target)
in procedure 'program'
** OpenOCD init failed **
shutdown command invoked
```

or with ST-Link:

```bash
$ st-flash write .pioenvs/mppt-1210-hus-v0.4/firmware.bin  0x08000000
st-flash 1.5.1
2019-06-21T18:13:03 INFO common.c: Loading device parameters....
2019-06-21T18:13:03 WARN common.c: unknown chip id! 0x5fa0004
```

check the connection between the board used for flashing
(for example the Nucleo), and the MPPT.

## API documentation

The documentation auto-generated by Doxygen can be found [here](https://libre.solar/charge-controller-firmware/).

## Unit tests

In order to run the unit tests, you need a PlatformIO Plus account. Run tests with the following command:

    platformio test -e unit-test-native

## Additional firmware documentation (docs folder)

- [MPPT charger firmware details](docs/firmware.md)
- [Charger state machine](docs/charger.md)
