# Ditto

![](sheep.ico)

ditto takes a Windows exe as input and copies the resources

of that binary into the target binary. Resources are usually

icons, copywright info and descriptions of the binary.

Info & Compiled version: http://www.room362.com/blog/2012/10/8/compiling-and-release-of-ditto.html

## Example usage:

Usage output:

```
C:\>ditto.exe

ditto - binary resource mirrorer
--------------------------------------------------------------------

C:\>ditto.exe sourcebin.exe targetbin.exe


C:\>
```

Copy (no output):
```
C:\>ditto.exe C:\windows\system32\notepad.exe evilbin.exe

C:\>
```

** Disclaimer: does not work with Metasploit In-memory: **
```
meterpreter > upload evilbin.exe
[*] uploading  : evilbin.exe -> evilbin.exe
[*] uploaded   : evilbin.exe -> evilbin.exe
meterpreter > execute -H -i -c -d calc.exe -m -f ditto.exe -a 'C:\windows\system32\notepad.exe evilbin.exe'
Process 1840 created.
Channel 2 created.
meterpreter >
```
But does not result in a file change of any kind and sometimes results in meterpreter dieing.

## CHANGELOG
2025-04-13 - huge facelift and focus on only relevant resources
2012-09-13 - code commit for version 1.0