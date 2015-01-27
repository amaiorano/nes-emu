# nes-emu

A Nintendo Entertainment System (NES) emulator written in C++

As this is just a pet project, don't expect a full-featured emulator. There is no GUI, many typical features are missing, and only a few mappers have been implemented. However, the code is pretty clean and straightforward, and I think can be useful to learn from. Of course, if someone wanted to fork this code to write a full-featured emulator, that would be cool as well :)


## Controls

Input     | Keyboard Key(s)
----------|----------------
D-Pad     | arrows
A         | A
B         | S
Select    | Tab
Start     | Enter
          |
Open Rom  | Ctrl + O
Reset     | Ctrl + R
Quit      | Alt + F4


## Challenge

As with most pet projects, the purpose of writing this emulator was mainly to learn. My background is not in hardware, but I have always had a keen interest in computer architecture, so part of my goals was to learn more about how a console works at the hardware level. The NES is simple enough in that respect, although it has enough quirks to make it interesting to emulate.

To further my learning, I challenged myself to write this emulator without looking at any other emulator code. So the implementation is completely my own, and I suspect parts of my code may seem strange or inefficient as compared to other implementations.


## Thanks

Although I did not look at other emulator source code, I did get a lot of information and help from other sources:

- The awesome people on the [#nesdev IRC channel](http://wiki.nesdev.com/w/index.php/NESdev_IRC_channel) (huge thanks!)
- Excellent documentation on the [nesdev wiki](http://wiki.nesdev.com/w/index.php/Nesdev_Wiki)
- Other excellent emulators I used to compare output, trace, and debug:
  - [fceux](http://www.fceux.com/)
  - [Nintendulator](http://www.qmtpro.com/~nes/nintendulator/)
  - [NO$NES](http://problemkaputt.de/nes.htm)
