# TODO list
[ ] Convert settings file to some form of KV-store like solution, which on linux would be Sqlite
[ ] Create RPM that can be installed
[ ] Have cmake generate settings for program

# Thoughts on settings
There are three classes of settings: 
* Program parameters - flags given while starting program
* persistent configurations - settings likely to never change
* volatile keys - tokens that change frequently, and doesn't really make sense to store 
  - Can each module be in charge of it's own keys? This is mostly related to OAuth2

# Misc notes
## Explanations of EPD functions

### SendCommand
Pull `EPD_DC_PIN` and `EPD_CS_PIN` low, write byte `reg`, and then pull `EPD_CS_PIN` high

### SendData
Pull `EPD_DC_PIN` high, and `EPD_CS_PIN` low. Then write byte `Data`, and then
pull `EPD_CS_PIN` high

### Writing stuff
```
SPI   0xxx      0xxx     0xxx    0xxx
DC  \________ /------- --------
CS  \______/- -\_____/ \______/
```
This is how it looks currently when doing `command + n * data`

Documentation states that CS is chip select, active low. So no reason to toggle

DC is Data/Command toggle

Sometimes wait on busy signal

## Overview
All steps involve sending different commands, followed by data. 

<!-- vim set: et sw=2 ss=2 tw=80 : -->
