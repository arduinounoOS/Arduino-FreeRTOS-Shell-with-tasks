This is just a simple arduino ide sketch
Using Arduino_FreeRTOS

To create a 4 task mini OS
With a serial shell

There is an EEPROM "file system" for 5 text files (8 bits) with 8 bit filenames.

The tasks are:

Shell
Blink
Uptime
LCD

____
The serial shell accepts some simple commands:

li - list files
wr <name> - write file (a new prompt appears for entering the text file data)
rd <name> - read file
de <name> - delete file
ut - show uptime in seconds
st - show task status - stack high water mark, and CPU speed
sd <ms> - set delay for blinking onboard LED
ft - format EEPROM file system
? - print help file

There are two files, that if they exist, set settings at bootup:
helpfile - this filename is read when the ? command is entered (otherwise ? prints nothing)
config - this help file contains a defualt blink delay to load at startup (otherwise 500 ms)


---
Blink task - just blinks the onboard LED

___
Uptime - counts seconds from boot

___
LCD - this is a fun task that uses the ardiuno LCD Keypad shield (if connected)
It just makes a little scrolling game with the arrow keys and the select key




This sketch and OS is very optimized to balance global variables and stacks.
Notice using "st" that the stack high water marks are 5-7 bytes (meaning there is only a buffer of 5 bytes until overflow).

This was a lot of fun to write.

I do want to change it to read the response strings from PROGMEM to save global variable space.
