# nes-emu

A Nintendo Entertainment System (NES) emulator written in C++

As this is just a pet project, don't expect a full-featured emulator. There is no GUI, many typical features are missing, and only a few mappers have been implemented. However, the code is pretty clean and straightforward, and I think can be useful to learn from. Of course, if someone wanted to fork this code to write a full-featured emulator, that would be cool as well :)


## Features

- Mappers 0,1,2,3,4,7 (~90% of games)
- Accurate audio emulation (no DMC)
- Automatic saving of SRAM
- Save state support
- Rewind any time up to 1 minute at normal speed
- Single and multi frame stepping when paused


## Controls

Input                 | Keyboard Key(s)
----------------------|----------------
D-Pad                 | arrows
A                     | A
B                     | S
Select                | Tab
Start                 | Enter
                      |
Open Rom              | Ctrl + O
Reset                 | Ctrl + R
Quit                  | Alt + F4
                      |
Rewind	              | Backspace
Pause	              | P
Step frame            |	[
Step many frames      |	]
			          |
Toggle audio channels |	F1-F4


## Challenge

As with most pet projects, the purpose of writing this emulator was mainly to learn. My background is not in hardware, but I have always had a keen interest in computer architecture, so part of my goals was to learn more about how a console works at the hardware level. The NES is simple enough in that respect, although it has enough quirks to make it interesting to emulate.

To further my learning, I challenged myself to write this emulator without looking at any other emulator code. So the implementation is completely my own, and I suspect parts of my code may seem strange or inefficient as compared to other implementations.


## Building

- Install CMake

- Download 32-bit SDL2 Development Libraries from https://www.libsdl.org/download-2.0.php
  - On Windows, set an environment variable named "SDL2" to the path of the extracted archive (e.g. ```set SDL2=C:\code\SDL2-2.0.4```)

- Run CMake and build. For example, on Windows:
```
cd nes-emu
md build
cd build
cmake ..
```


## Thanks

Although I did not look at other emulator source code, I did get a lot of information and help from other sources:

- The awesome people on the [#nesdev IRC channel](http://wiki.nesdev.com/w/index.php/NESdev_IRC_channel) (huge thanks!)
- Excellent documentation on the [nesdev wiki](http://wiki.nesdev.com/w/index.php/Nesdev_Wiki)
- Other excellent emulators I used to compare output, trace, and debug:
  - [fceux](http://www.fceux.com/)
  - [Nintendulator](http://www.qmtpro.com/~nes/nintendulator/)
  - [NO$NES](http://problemkaputt.de/nes.htm)
