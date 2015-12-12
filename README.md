# marbelous.cpp
C++ interpreter for Marbelous

##### What is Marbelous?
Marbelous is an [esoteric programming language](https://en.wikipedia.org/wiki/Esoteric_programming_language) based on marbles (aka 8-bit unsigned integers) falling through a Rube Goldberg-like board, filled with various devices that manipulate the value and/or motion of the marble.

For example, in the following board, `30` is a marble with value `0x30`. The marble falls down one row, is pushed right, and then falls off the bottom. Marbles that fall off the bottom are outputted to STDOUT; here, `0`, which is `0x30` in ASCII, is outputted.

    30 ..   ==>   .. ..   ==>   .. ..   ==> .. ..
    \\ ..         30 ..         \\ 30       .. ..  (0 is outputted)


Marbelous was conceived in [Programming Puzzles & Code Golf's chat room (The Nineteenth Byte)](http://chat.stackexchange.com/transcript/message/16955224#16955224). Discussion about the spec was then continued in a [dedicated chatroom](http://chat.stackexchange.com/rooms/16230/marbelous-esolang-design). The original idea for Marbelous came from cjfaure; Martin Büttner, Nathan Merril, overactor, sparr, githubphagocyte, es1024, and VisualMelon provided additional input on language design.

For more examples of Marbelous code, see the examples directory in the [Python interpreter repo](https://github.com/marbelous-lang/marbelous.py) (written by sparr), or head over to [Programming Puzzles & Code Golf Stack Exchange](http://codegolf.stackexchange.com) and search `Marbelous`.

##### Compiling marbelous (interpreter)
If you have `make` on your system, just run `make bin/marbelous`. Alternatively, you can just compile and link all the `.cpp` files in source (no external libraries are used for the interpreter). Note that this interpreter is written to C++11,  so you may need to pass a flag to your compiler to specify this (for gcc: `--std=c++11`).

##### Compiling vmarbelous (debugger)
The debugger (which shows the marbles moving throughout the board: see below) requires GTK+ 3.0, FreeType 2, Pango, and Cairo.

Run `make bin/vmarbelous` to compile. Note that this interpreter is written to C++11,  so you may need to pass a flag to your compiler to specify this (for gcc: `--std=c++11`).

If you are on Windows, I recommend using [MSYS](http://sourceforge.net/p/mingw-w64/wiki2/MSYS/) to compile marbelous.cpp.

vmarbelous printing an arch of ascending/descending numbers ([written by Sparr](http://codegolf.stackexchange.com/a/52073/29611)):

![](http://i.stack.imgur.com/Jyibf.gif)

##### Interpreter Options
The C++ interpreter has the following options:

Option | Description
------ | ---------------
&#8209;&#8209;help | Display help information
&#8209;v[vv] | Change verbosity level (default 0); add more v's to increase verbosity. `marbelous` only.
&#8209;&#8209;enable&#8209;cylindrical, &#8209;&#8209;disable&#8209;cylindrical | Enable or disable cylindrical boards (default disabled). If disabled, marbles falling off the side of the board are destroyed. If enabled, marbles falling off the side of the board reappear on the other side.

##### More information/Other interpreters
[Python interpreter by sparr (first Marbelous interpreter)](https://github.com/marbelous-lang/marbelous.py)

[Javascript interpreter by es1024 (on codegolf.stackexchange.com)](http://codegolf.stackexchange.com/a/40808/29611)

[Spec draft for Marbelous](https://github.com/marbelous-lang/docs/blob/master/spec-draft.md)
