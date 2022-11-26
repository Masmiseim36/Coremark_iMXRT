# Coremark port for iMXRT family

[CoreMark](https://www.eembc.org/coremark/scores.php) is a benchmark that measures the performance of central processing units (CPU) used in embedded systems. It was developed in 2009 by Shay Gal-On at [EEMBC](https://www.eembc.org/) and is intended to become an industry standard, replacing the Dhrystone benchmark. The code is written in C and contains implementations of the following algorithms: list processing (find and sort), matrix manipulation (common matrix operations), state machine (determine if an input stream contains valid numbers), and CRC. The code is under the [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0) and is free of cost to use, but ownership is retained by the Consortium and publication of modified versions under the CoreMark name prohibited. ([Wikipedia](https://en.wikipedia.org/wiki/Coremark))

This projects implements the Coremark benchmark for the [iMXRT Family](https://www.nxp.com/products/processors-and-microcontrollers/arm-microcontrollers/i-mx-rt-crossover-mcus:IMX-RT-SERIES) controller using [Rowley Crossworks](https://www.rowley.co.uk/). For this project the CPU-Support Package from [here](https://github.com/Masmiseim36/iMXRT) is used.

## Results with GCC 10.3

The results are in iterations per second (higher is better), code and data is stored in TCM.

Controller                    | O0       | OG       | O1       | OSize    |  O2      | O3       | O3 + LTO       
--------                      | --------:| --------:| --------:| --------:| --------:| --------:| --------:
**iMX RT1011     ( 396 MHz)** |   289.59 |   906.88 |  1159.52 |  1147.26 |  1590.48 |  1584.53 | 1622.42
**iMX RT1011     ( 500 MHz)** |   365.65 |  1140.87 |  1463.22 |  1448.56 |  2008.67 |  1999.68 | 2048.59
**iMX RT1060     ( 528 MHz)** |   386.13 |  1204.90 |  1548.17 |  1527.88 |  2121.52 |  2106.59 | 2153.87
**iMX RT1060     ( 600 MHz)** |   438.78 |  1369.22 |  1759.32 |  1736.29 |  2410.91 |  2393.83 | 2447.62
**iMX RT1160_CM7 ( 500 MHz)** |          |          |          |          |          |          | 
**iMX RT1160_CM7 ( 600 MHz)** |          |          |          |          |          |          | 
**iMX RT1160_CM4 ( 240 MHz)** |          |          |          |          |          |          | 
**iMX RT1170_CM7 ( 800 MHz)** |   547.71 |  1821.55 |  2338.19 |  2303.51 |  3199.38 |  3193.25 | 3266.69
**iMX RT1170_CM7 (1000 MHz)** |   683.62 |  2273.55 |  2918.34 |  2875.05 |  3993.29 |  3985.65 | 4076.97
**iMX RT1170_CM4 ( 400 MHz)** |   219.33 |   631.95 |   745.00 |   676.28 |   952.38 |  1009.35 | 1018.60

## Results with GCC 11.3

The results are in iterations per second (higher is better), code and data is stored in TCM.

Controller                    | O0       | OG       | O1       | OSize    |  O2      | O3       | O3 + LTO       
--------                      | --------:| --------:| --------:| --------:| --------:| --------:| --------:
**iMX RT1011     ( 396 MHz)** |   289.59 |   909.12 |  1170.16 |  1207.23 |  1598.10 |  1588.56 | 1630.31
**iMX RT1011     ( 500 MHz)** |   365.65 |  1153.61 |  1480.42 |  1524.29 |  2008.67 |  2001.60 | 2058.46
**iMX RT1060     ( 528 MHz)** |   386.13 |  1212.32 |  1561.86 |  1610.72 |  2116.93 |  2110.77 | 2167.22
**iMX RT1060     ( 600 MHz)** |   438.78 |  1377.63 |  1774.87 |  1830.36 |  2405.69 |  2398.65 | 2462.81
**iMX RT1160_CM7 ( 500 MHz)** |   341.81 |  1143.32 |  1478.28 |  1503.80 |  2055.24 |  1994.65 | 2055.24
**iMX RT1160_CM7 ( 600 MHz)** |   411.82 |  1377.48 |  1781.07 |  1811.85 |  2410.45 |  2403.15 | 2476.22
**iMX RT1160_CM4 ( 240 MHz)** |   134.03 |   389.72 |   464.88 |   436.04 |   594.31 |   626.06 |  629.69
**iMX RT1170_CM7 ( 800 MHz)** |   547.71 |  1831.16 |  2359.15 |  2435.46 |  3206.56 |  3199.79 | 3293.59
**iMX RT1170_CM7 (1000 MHz)** |   683.63 |  2285.60 |  2944.64 |  3038.59 |  4002.24 |  3993.92 | 4099.36
**iMX RT1170_CM4 ( 400 MHz)** |   219.33 |   637.75 |   760.76 |   713.57 |   972.57 |  1024,52 | 1030.43

## Results with GCC 12.2

The results are in iterations per second (higher is better), code and data is stored in TCM.

Controller                    | O0       | OG       | O1       | OSize    |  O2      | O3       | O3 + LTO       
--------                      | --------:| --------:| --------:| --------:| --------:| --------:| --------:
**iMX RT1011     ( 396 MHz)** |   289.59 |   887.04 |  1201.69 |  1498.62 |  1601.38 |  1604.87 | 1635.96
**iMX RT1011     ( 500 MHz)** |   365.65 |  1127.11 |  1521.23 |  1892.21 |  2024.12 |  2032.93 | 2065.51
**iMX RT1060     ( 528 MHz)** |   386.13 |  1192.66 |  1605.03 |  1996.16 |  2132.19 |  2148.04 | 2195.48
**iMX RT1060     ( 600 MHz)** |   438.78 |  1355.38 |  1823.95 |  2268.39 |  2423.06 |  2440.92 | 2494.88
**iMX RT1160_CM7 ( 500 MHz)** |          |          |          |          |          |          | 
**iMX RT1160_CM7 ( 600 MHz)** |          |          |          |          |          |          | 
**iMX RT1160_CM4 ( 240 MHz)** |          |          |          |          |          |          | 
**iMX RT1170_CM7 ( 800 MHz)** |   547.71 |  1797.20 |  2422.59 |  3017.68 |  3221.64 |  3244.64 | 3314.77
**iMX RT1170_CM7 (1000 MHz)** |   683.62 |  2243.15 |  3023.88 |  3766.47 |  4020.90 |  4049.56 | 4137.36
**iMX RT1170_CM4 ( 400 MHz)** |   219.33 |   632.21 |   803.21 |   836.30 |   991.60 |  1038.49 | 1058.72