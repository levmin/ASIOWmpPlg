Documents enclosed:
"ASIO Plugin.pdf" - installation and configuration instructions
gpl.txt - GNU Public License

Change log:

Version 6.1

Transition to Visual Studio 2017
Use Microsoft's IsWindowsVistaOrGreater instead of IsVistaOrLater

Version 6.0

New WMP transport synchronization method
Visual Studio 2012 port

Version 5.5

Bug fixes, minor performance improvements

Version 5.0

A major reimplementation. Visual Studio project files were converted to VS2010.
A background plugin was added to better synchronize ASIO playback with WMP UI.
Windows 7 SP1 compatibility issues resolved. Several potential stability issues addressed.

Version 4.4

WAV, WMA and MP3 files are played on Windows 7 without registry modifications.
64-bit support.

Version 4.3

Proper handling of containered streams.
FLAC files from HDTracks.com are now playable.
Tighter error handling

Version 4.2

More accurate error diagnostic

Version 4.1

An error message when ASIO cannot be used by WMP.

Version 4.0

WMP shutdown and track transitions made more reliable.

Version 3.1

A small tweak in the interaction with the WMP output device.

Version 3

A new, simpler multi-threading scheme.
Thread priority is no longer elevated, process priority is.

Version 2

An additional run-time DLL was added to the installer.
The flow control no longer depends on the WMP output device.
Thread and process priorities are elevated.


Version 1

First release