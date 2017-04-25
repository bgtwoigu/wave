Sensory Confidential

(c)2002-2014 Sensory Inc. All rights reserved.
See "License.txt" for licensing information.

Platform: ix86-linux-2.6
~~~~~~~~~~~~~~~~~~~~~~~~~
Requirements:
 * Development platform:
   - Intel x86 processor
   - Linux (Kernel v2.6.32-25) 
 * Target platform:
   - Same as development platform
 * Development Tools:
   - GNU compiler (v4.3.3) 
   - GNU make (v3.81)

Compilation:
 * Interface to the SDK by including "trulyhandsfree.h" in your application
   code (located in the SDKs "include" directory).

 * The SDK provides static libraries in <platform>/lib/lib*.a

Sample Code: 
 * A set of samples is provided in "TrulyHandsfree SDK/<platform>/Samples". 
 * Samples can be compiled using the general makefile provided, as follows:
   1. Change directory: "TrulyHandsfree SDK/<platform>/Samples"
   2. make -f GNUmakefile

 * Samples must be executed from WITHIN their specific directory, as follows:
   1. Change Directory: "TrulyHandsfree SDK/Samples/<sample>"
   2. Execute: <platform>/<sample>

Technical Support:
~~~~~~~~~~~~~~~~~~
Email technical support questions or bug reports to: 
      techsupport@sensoryinc.com



