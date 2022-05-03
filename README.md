# eac-mapper

## How it works?

When the EasyAntiCheat driver is initialized, it walks through each loaded driver's read-only sections with MmCopyMemory to ensure that malicious patches have not taken place.  But, EasyAntiCheat has a slight oversight resulting in certain drivers, known as session drivers, to not be accounted for during these initial scans.  From my debugging, EasyAntiCheat entirely skips session drivers and does not make any attemps to ensure their integrity.

## What are session drivers?
In short, session drivers are drivers that are not globally mapped to every address space, such as the address space EasyAntiCheat's driver is executing under.  This allows us to freely patch such drivers without any consequences.

## How can it be fixed?
EasyAntiCheat can easily patch this method by attaching to a process that has such drivers mapped into its address space.

## Note
- This project uses vdm by xerox to control the vulnerable driver
- This project uses the rweverything driver as the vulnerable driver
- The mapped shellcode will evade all EasyAntiCheat scans
