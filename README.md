Wilbrand
2011 giantpune


[1]	About
	This is a program used to build the mailbox bomb exploit for the Wii system menu.  

[2] Usage
This is a cli program.  If you don't know how to use it, google.  Here is the usage:
```sh
Wilbrand <MAC address> <date> <sysmenu version> <SD Root>
```

MAC address:	the wireless mac address of the wii. you can get this from the internet settings, or if those are inaccessible
				for whatever reason, many routers will display it.  It may contain dashes, colons, or spaces, but should be 12
				hex characters when these extra characters are removed.

Date:			this is the date you want the message to show up under in the message board. It should be within the range of
				2000 - 2035.  Accepted formats are mm/dd/yyyy or in a 32bit hex representation of the number of seconds since
				00:00 Jan, 01  2000. *NOTE* The System menu is triggered to load messages from the SD card when you scroll
				across days.  So if you build the message for the current day, you will have to scroll to a different day and
				then back again to trigger the loading of it.

SysmenuVersion	This is the version of the System menu to build the exploit for.  It can be read in the first page under Wii
				settings.  Accepted formats are 3.2u, 32u, 3.2U, and 32U.  Accepted versions are any official, publicly
				released version from 3.0 - 4.3.

SD Root			This is the base folder where you would like the message created.  The design of the cdb archive uses about 13
				subfolders to store files.  This program will use the given folder and create the necessary folder structure
				and then place the exploit message in its place.

Examples:
./Wilbrand 001122334455 10/25/2011 3.2u /media/sdcard
./Wilbrand 00-11-22-33-44-55 163a09c4 43U /media/sdcard
./Wilbrand '00 11 22 33 44 55' 163a09c4 4.3K F:

You need to use an SD card that is supported by your system menu version.  To test this, go to the data management or SD card
menu and try to use the SD card.  It will tell you if it can't be used.  Once you have run this program, and have the exploit
message on your SD card, you need some homebrew to load.  This one works pretty well [ http://bootmii.org/download/ ].  Save
the homebrew as SD:/boot.elf.  Then stick the SD card in your wii, start it up, go to the message board, and then scroll to the
day that the exploit was built for.  It should be easily identifiable as it will be the best looking envelope there.  Click it
and watch the magic happen.

Doesn't work?
Q -	I can't run the program.
Q -	Wilbrand just gives me errors and doesn't build the exploit.
A -	Read and re-read the usage instructions.
	google how to use a cli program.

Q -	I don't see the incredibly cool image.
A -	Double check that you are looking at the date you gave to wilbrand when building the exploit.
	Make sure the system menu can access your SD card.
	Make sure you gave the correct WIRELESS MAC address to the program

Q -	When I click the message, it freezes.
A -	You gave the wrong system menu version when building the exploit message.

Q -	I click the message and I get a black screen with white text flying by over and over.
A -	Make sure you have SD:/boot.elf.
	Try with a different SD card.  Apparently this SD code doesn't like some cards.

[3] Compiling
The elf loader code should be built with a toolchain targeting the wii ppc.  Change the PREFIX in ./loader/Makefile to point to
yours.  Once you have built ./loader/loader.bin, copy that into the ./data folder.  Along with the usual gcc tools, Wilbrand
needs the bin2s program from devkitpro to build.  Wilbrand has 3 makefiles. I'm using a debian Linux x86_64 system with multilib
and mingw packages and the *x86 and *.win makefiles to build 3 different binaries.  On any linux machine, the plain Makefile
should build a native binary.  Windows users should use the *.win makefile and change the PREFIX, as I've added a little makefile
voodoo in there.  It SHOULD work with mingw and cygwin.  I don't have access to a Mac with a decent build system, so I can't say
if it will work there or not.

[4] Authors
Hackmii team		- Savezelda's loader was used as a starting point for the payload.
Paul E. Jones	-	sha1 code
Mike Scott		-	AES code
others			-	there is some misc code involved (such as wbe32()) that was takes from various open-source projects which
					was very nice to have, but I'm sure I couldn't name all occurances and credit all authors appropriately
giantpune		-	finding the initial bug [WOOHOO!], dumping and debugging 32 version of the system menu to find the offsets
					pretty much everything else involved


[5] Changelog

3.0
	Initial public release.


[6]	License
Wilbrand is licensed under the GPL v2 license.  The AES and SHA1 code each have their own license and copyright.  See those files
for their respective licenses.  Savezelda is licensed under GPL v2 as well.

