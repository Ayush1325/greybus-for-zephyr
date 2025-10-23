.. _gpio:

****************
GPIO via Greybus
****************

The following has been tested on BeaglePlay (host) + BeagleConnect Freedom (node).

Just to make sure that we're interacting with the Greybus GPIO device, run
the following (toggling random GPIO can sometimes have negative consequences). 

.. code-block:: bash

    gpiodetect | grep greybus_gpio
    gpiochip0 [greybus_gpio] (32 lines)

Most GPIO controllers expose a nubmer of GPIO, so it's important to know which
physical pin number should be toggled and what wiill happen when we do so.

We can use normal gpiod CLI tools.

For example, to toggle GPIO 22, we can use the following script:

.. code-block:: bash

    #!/bin/bash
    # GPIO toggle demo for GPIO 22 pin, i.e. MB2 UART1 TX
    
    # /dev/gpiochipN that Greybus created
    CHIP="$(gpiodetect | grep greybus_gpio | head -n 1 | awk '{print $1}')"

    gpioset -c $CHIP -t 0 22=0
    sleep 1s
    gpioset -c $CHIP -t 0 22=1


We can also read the GPIO pin state as follows:

.. code-block:: bash

    #!/bin/bash
    # GPIO toggle demo for GPIO 22 pin, i.e. MB2 UART1 TX
    
    # /dev/gpiochipN that Greybus created
    CHIP="$(gpiodetect | grep greybus_gpio | head -n 1 | awk '{print $1}')"

    gpioget -c $CHIP 22
