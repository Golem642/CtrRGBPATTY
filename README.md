# CtrRGBPAT2
This 3DS application allows the LED color to be changed. In order for this to work, Luma CFW (custom firmware) needs to be installed.
Thanks to [CPunch](https://github.com/CPunch/CtrRGBPATTY/) for the original project !

## Features
Customize the LED color and pattern for when you receive SpotPass notifications ! Why keep the default blue when you can have a cool-looking purple 😎
Everything is simple and made so you cannot possibly screw something up (or else you really wanted to)

## Upcoming
- [ ] StreetPass, Friends, and "Low Battery" notifications support
- [ ] Full editor to allow you to create your own patterns
- [ ] Save and restore feature
- [x] Toggle on/off
- [ ] Proper UI ?


# Building
After some researches i determined that this is made for devkitpro for Windows. I don't know how CPunch managed to make this work but i had to make a lot of changes. 
The compilation process is very weird but i didn't want to change anything to avoid breaking the currently "working" setup. 
- The main program is in `build/main.cpp`, to compile it you can use the devkitPro Windows MSYS installation with `make` at the root of the project folder. 
- You will get errors, that's normal. You will need to copy/paste the updated `build/CtrRGBPAT2.elf` to `resources/CtrRGBPAT2.elf`, replace if asked. 
- Open a command prompt in the `resources` folder and execute the `Build.bat` file, it should compile without errors and you'll have a new `CtrRGBPAT
.cia` in the folder

## eclipse-3ds-template
This template is basically a fork of [TricksterGuy's 3ds-template](https://github.com/TricksterGuy/3ds-template) which is essentially a fork of two other templates, which I'm not going to ramble on about.

## Modifications:
1) Supports eclipse now.
2) Eclipse also has build targets.

## To set up in eclipse:
Open eclipse and import a project into the workspace. Select the root of the folder (or select the ZIP of this repository). You must have devkitPro installed to /opt/devkitPro and thus devkitARM installed to /opt/devkitPro/devkitARM.

Atop this, you also need buildtools.

As a result, all build targets and includes should be added. If you'd like auto-corrections on libraries other than ctrulib.

## To set up for elsewhere (or windows.)
Modify the eclipse environment variables, C includes, and C++ includes to be where your install of devkitPro and CTRUlib is.
