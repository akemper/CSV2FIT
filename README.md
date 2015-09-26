# CSV2FIT
The code converts WGS-84 coordinates from a CSV file to a Garmin FIT course file.

Based on version 16 of http://www.thisisant.com/resources/fit, this code converts a CSV file, as can be downloaded from www.GPSies.com, to a Garmin FIT course file for upload to Forerunner 910XT and similar devices.

Note that the FIT file structure slightly differs from the official course file specification as included in the FIT SDK download. Doing a bit of reverse engineering from a Garmin Express upload, it turned out that at least my device needs additional event messages, instaed of course lab messages to be included in the FIT binary.

Feel free to reuse and enhance the code for your own projects and of course at your own risk.
