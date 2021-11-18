# TODO list
[ ] Finish GPIO implementation
[ ] Finish SPI implementation
[ ] Translate eval code to proper code
[ ] Look a bit more at the Display configuration
 - How does the dev sample match the data sheet?
 - Investigate splitting up screen
   - Perhaps not possible
[ ] Make display show image
[ ] Make display show text
[ ] EFL Evas library should be a good fit to create a drawing canvas
 - Add debug functionality to store rendered image to file
[ ] Fetch data from netatmo
[ ] Fetch weather forecast
 - [ ] curl should be a good fit
 - [ ] SMHI open data: https://opendata.smhi.se/apidocs/metobs/common.html
 - [ ] Fetch hourly

# Summary of used libraries
libjson / jansson
libcurl
libefl

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
