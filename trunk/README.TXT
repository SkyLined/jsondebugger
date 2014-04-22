This project is intended to be build using Visual Studio. However, since I
cannot for the life of me figure out how to set the thing up correctly, I've
left that to you. You will need to install the Windows Software Development
Kit as well as Visual Studio (Express).

To build the project, create a new console application with precompiled headers
in Visual Studio and name it JSONDebugger. Then manually add all the files to
this project. You may want to add an 64-bit configuration to the project and
build that if you want to debug 64-bit applications.

If any of this fails, send me an email at berendjanwever@gmail.com and I'll see
if I can help you fix it.

If you have Python installed, you can run build.py to build Debug and Release
versions of 32- and 64-bit applications all at once.
