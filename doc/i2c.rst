.. _i2c:

***************
I2C via Greybus
***************

The following has been tested on BeaglePlay (host) + BeagleConnect Freedom (node).

Just to make sure that we're interacting with the Greybus I2C device, run
the following (reading and writing to random I2C devices can sometimes have
negative consequences). 

.. code-block:: bash

    ls -la /sys/bus/i2c/devices/* | grep "greybus"
    lrwxrwxrwx 1 root root 0 Oct 18 18:32 /sys/bus/i2c/devices/i2c-6 -> ../../../devices/platform/bus@f0000/2860000.serial/2860000.serial:0/2860000.serial:0.0/serial0/serial0-0/greybus1/1-3/1-3.3/1-3.3.4/gbphy6/i2c-6

In this case, we will read values a number of sensors on the ``beagleconnect_freedom``.

First, read the ID registers (at the i2c register address ``0x7e``) of the
``opt3001`` sensor (at i2c bus address ``0x44``) as shown below:

.. code-block:: bash

    i2cget -y 6 0x44 0x7e w
    0x4954
    

Next, read the ID registers (at the i2c register address ``0xfc``) of the
``hdc2080`` sensor (at i2c bus address ``0x41``) as shown below:

.. code-block:: bash

    i2cget -y 6 0x41 0xfc w
    0x5449

Lastly, we are going to probe the Linux kernel driver for the ``opt3001``
and use the Linux IIO subsystem to read ambient light values.

.. code-block:: bash

    echo opt3001 0x44 | sudo tee /sys/bus/i2c/devices/i2c-6/new_device
    cd /sys/bus/iio/devices/iio:device0
    cat in_illuminance_input
    3.33000

What is very interesting about the last example is that we are using the
local Linux driver to interact with the remote I2C PHY. In other words,
our microcontroller does not even require a specific driver for the sensor
and we can maintain the driver code in the Linux kernel!
 
