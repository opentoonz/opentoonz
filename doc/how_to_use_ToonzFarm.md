# Using the OpenToonz Farm

## Notes about versions

- Mac OSX Version 1.1.2 and earlier do not include OpenToonz Farm binaries.
- Currently OpenToonz Farm Controller, Server and OpenToonz application all need to be on the same platform (e.g. Linux OpenToonz requires Linux OpenToonz Farm Controller and Server), Issue #1131 has been opened to track the resolution of this.

## Planning your OpenToonz Farm

To setup your OpenToonz Farm you will need a file share location that can be accessed by all computers in your OpenToonz Farm to hold the FarmRoot folder. This can be file share on one machine, a NAS or a dedicated file server, the important thing to remember is that the path to the FarmRoot must be exactly the same for all the computers in your OpenToonz Farm.

Once you have identified your file share location, copy the OpenToonz Stuff folder to this file share location. The toonzfarm folder in the OpenToonz Stuff folder you copied to the file share location becomes your FARMROOT.

## Local Controller / Server configuration

On the OpenToonz Farm Controller and each OpenToonz Farm Server, you need to configure the FARMROOT, do do this you enter the location of your FARMROOT as follows:
- Windows: Set the registry key SOFTWARE\\OpenToonz\\OpenToonz\\<version>\\FARMROOT where the FARMROOT key is a string type with the value being the path to your FARMROOT.
- Mac OSX: Find the OpenToonz_<version>.app file (usually in /Applications), and using "show package contents" create / edit OpenToonz_<version>.app/Contents/Resources/configfarmroot.txt, the path to the FARMROOT should be paced on the first line, leave the second line blank and no other lines.
- Linux  : create / edit /etc/OpenToonz/opentoonz.conf, the path to the FARMROOT should be paced on the first line, leave the second line blank and no other lines.

## OpenToonz Farm configuration

In your FARMROOT you need to create the configuration to allow the farm computers to communicate, first ensure that toonzfarm (your FARMROOT folder) contains a "config" folder. Inside the config folder you will need the following files:
- controller.txt : This file should contain 1 line with each value delimited by a space ( ), the required values as as follows in this order:
  - Controller Host Name
  - Controller IP Address
  - Controller Port Number

  e.g. "controller.my.net 192.168.0.10 1"

- servers.txt    : This file should contain 1 line per OpenToonz Farm Server. Each line should contain 3 values each value delimited by a space ( ) as follows in this order
  - Server Host Name
  - Server IP Address
  - Server Port Number

  e.g. "server1.my.net 192.168.0.11 88"
       "server2.my.net 192.168.0.12 88"
       "server3.my.net 192.168.0.13 88"

Once you have configured these files, start the Controller and Servers.
- If you started with the -console option check the console of each server and controller to confirm they started without error and report starting on the port you chose in the config files above.
- If you started without the -console option, check the logs in the FARMROOT folder, look for controller.log and server.log files.

## Local OpenToonz application configuration

The final piece to the puzzle is to configure the FARMROOT in you OpenToonz application:
- First select the Farm room (formerly known as Batches)
- In the Batch Server window you should see two fields (Process with, and Farm Global Root):
  - set the value of Farm Global Root to the path of FARMROOT
  - select the Process with option list and select "Render Farm", once you do this there might be a delay, don't worry this is normal. Next either:
    - Connection to the Controller was successful, you can verify this by checking under the Farm Global Root field, here you should see a list of OpenToonz Farm Servers.
    - Connection to the Controller was not successful, in this case you should get a warning dialogue asking you to start the Controller, take note of the Controller host name and port number on this dialogue, as this might help you diagnose why you can't connect to the Controller.
