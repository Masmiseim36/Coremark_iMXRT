# Coremark port for iMXRT 1170

[CoreMark](https://www.eembc.org/coremark/scores.php) is a benchmark that measures the performance of central processing units (CPU) used in embedded systems. It was developed in 2009 by Shay Gal-On at [EEMBC](https://www.eembc.org/) and is intended to become an industry standard, replacing the Dhrystone benchmark. The code is written in C and contains implementations of the following algorithms: list processing (find and sort), matrix manipulation (common matrix operations), state machine (determine if an input stream contains valid numbers), and CRC. The code is under the [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0) and is free of cost to use, but ownership is retained by the Consortium and publication of modified versions under the CoreMark name prohibited. ([Wikipedia](https://en.wikipedia.org/wiki/Coremark))

This projects implements the Coremark benchmark for the [iMXRT1170]( https://www.nxp.com/products/processors-and-microcontrollers/arm-microcontrollers/i-mx-rt-crossover-mcus/i-mx-rt1170-crossover-mcu-family-first-ghz-mcu-with-arm-cortex-m7-and-cortex-m4-cores:i.MX-RT1170) controller using [Rowley Crossworks](https://www.rowley.co.uk/). For this project the CPU-Support Package from [here](https://github.com/Masmiseim36/iMXRT) is used, as the official Port does not support the iMXRT1170 controller.
