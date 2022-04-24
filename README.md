# A simple wrapper to run any executable as a Windows service

## Usage
Copy the wrapper (winsvcwrapper.exe) to your Windows directory and create a service using the following command:  
`sc create MyService start= auto binPath= "C:\Windows\winsvcwrapper.exe cmd /c win32calc"`  
**Replace `MyService` with the desired service name and `cmd /c win32calc` with the path and command line arguments of the executable you are trying to run.**

## License
[//]: # ([![CC BY-SA 4.0][cc-by-sa-shield]][cc-by-sa])  

This work is licensed under a [Creative Commons Attribution-ShareAlike 4.0 International License][cc-by-sa].

[![CC BY-SA 4.0][cc-by-sa-image]][cc-by-sa]

[cc-by-sa]: http://creativecommons.org/licenses/by-sa/4.0/
[cc-by-sa-image]: https://licensebuttons.net/l/by-sa/4.0/88x31.png
[cc-by-sa-shield]: https://img.shields.io/badge/License-CC%20BY--SA%204.0-lightgrey.svg
