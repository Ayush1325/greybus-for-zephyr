.. _greybus_for_zephyr:

******************
Greybus for Zephyr
******************

Overview
########
This repository contains a `Greybus <https://lwn.net/Articles/715955/>`_
`module <https://docs.zephyrproject.org/latest/guides/modules.html>`_ for the
`Zephyr Real-Time Operating System <https://zephyrproject.org/>`_.

Adding Greybus to Zephyr
#########################

First, ensure that all required tools are installed by following Zephyr's
`Getting Started Guide <https://docs.zephyrproject.org/latest/getting_started/index.html>`_.

Add following entry to west.yml file in manifest/projects subtree of Zephyr:

.. code-block::

    - name: greybus
      revision: main
      path: modules/lib/greybus
      url: https://github.com/Ayush1325/greybus-for-zephyr.git

Then, clone the repository by running

.. code-block:: bash

    west update

Building and Running
####################

The current testing has been performed using mainline Linux kernel (6.12) on `BeaglePlay <www.beagleboard.org/boards/beagleplay>`_ with cc1352p7 running `greybus-host firmware <https://github.com/Ayush1325/cc1352-firmware>`_ and `BeagleConnect Freedom <https://www.beagleboard.org/boards/beagleconnect-freedom>`_ running greybus-for-zephyr.

.. code-block:: bash

    west build -b beagleconnect_freedom --sysbuild -p modules/lib/greybus/samples/basic -- -DEXTRA_CONF_FILE="transport-tcpip.conf;802154-subg.conf"


The sample uses `MCUboot <https://docs.mcuboot.com/>`_ to provide OTA support. The following commands can be used to merge mcuboot and application firmware, and flash to beagleconnect freedom:

.. code-block:: bash

    cp build/mcuboot/zephyr/zephyr.bin zephyr.bin
    dd conv=notrunc bs=1024 seek=56 if=build/basic/zephyr/zephyr.signed.bin of=zephyr.bin
    cc1352_flasher --bcf zephyr.bin
    rm zephyr.bin

Using Greybus for I/O
#####################

At this point, we should be ready to perform some I/O on our remote devices
using Greybus. Currently, this module supports the protocols below. 

* `GPIO <doc/gpio.rst>`_
* `I2C <doc/i2c.rst>`_
* `SPI <doc/spi.rst>`_

Additional Information
**********************

Additional Information about Greybus including videos, slide presentations,
and deprecated demo instructions can be found `here <doc/old.md>`_.

A compiled version of the `Greybus Specification <https://github.com/projectara/greybus-spec>`_
is available `here <doc/GreybusSpecification.pdf>`_.
