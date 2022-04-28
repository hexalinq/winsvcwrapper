# A simple wrapper to run any executable as a Windows service

## Usage
Copy the wrapper (winsvcwrapper.exe) to your Windows directory and create a service using the following command:  
`sc create MyService start= auto binPath= "C:\Windows\winsvcwrapper.exe cmd /c win32calc"`  
**Replace `MyService` with the desired service name and `cmd /c win32calc` with the path and command line arguments of the executable you are trying to run.**

## License
MIT
