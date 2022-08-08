# STRGROUP: match strings based on their Levenshtein edit distance

- Current version: `1.0.4 17dec2021`
- Jump to: [`overview`](#overview) [`installation`](#Installation) [`compiling`](#compiling) [`update history`](#update-history) [`author`](#author)

-----------

## Overview: 

`strgroup` is a [Stata](http://www.stata.com) command that performs a fuzzy string match using the following algorithm:

1. Calculate the Levenshtein edit distance between all pairwise combinations of strings.

2. Normalize the edit distance. The default is to divide the edit distance by the length of the shorter string in the pair.

3. Match a string pair if their normalized edit distance is less than or equal to the user-specified threshold.

4. If string A is matched to string B and string B is matched to string C, then match A to C.

5. Assign each group of matches a unique number.

The Levenshtein edit distance is defined as the minimum number of insertions, deletions, or substitutions necessary to change one string into the other. For example, the Levenshtein edit distance between "mitten" and "fitting" is 3, since the following three edits change one into the other, and it is impossible to do it with fewer than three edits:

1. mitten -> fitten (substitution of 'f' for 'm')

2. fitten -> fittin (substitution of 'i' for 'e')

3. fittin -> fitting (insert 'g' at the end)

For more details, see the Stata help file included in this package.

## Installation:

Type `which strgroup` at the Stata prompt to determine which version you have installed. To install the most recent version, copy and paste the following line of code:

```stata
net install strgroup, from("https://raw.githubusercontent.com/reifjulian/strgroup/master") replace
```

To install the version that was uploaded to SSC, copy and paste the following line of code:
```stata
ssc install strgroup, replace
```

After installing, type `help strgroup` to learn the syntax.

`strgroup` is implemented as a [C plugin](https://www.stata.com/plugins/) in order to minimize memory requirements and to maximize speed.  Plugins are specific to the hardware architecture and software framework of your computer. Define a platform by two characteristics: machine type and operating system.  Stata stores these characteristics in `c(machine_type)` and `c(os)`, respectively. `strgroup` supports the following platforms at this time:

| Machine type    | Operating system           |
| :------------- |:-------------| 
| PC      | Windows |
| PC (64-bit x86-64)      | Windows      | 
| PC (64-bit x86-64)      | Unix      | 
| Macintosh      | MacOSX      | 
| Macintosh (Intel 64-bit)       | MacOSX      | 

`strgroup` might work with some other platforms (e.g., 32-bit unix), but this has not been tested.

## Compiling:

The C source code for `strgroup` is available in `/src/c`. It must be compiled separately for different machine types.

PC Windows 32-bit (Cygwin):
```
gcc -shared -mno-cygwin stplugin.c strgroup.c -O3 -funroll-loops -o strgroup.windows32.plugin
```

PC Windows 64-bit (needs Cygwin and mingw compiler):
```
x86_64-w64-mingw32-gcc -shared stplugin.c strgroup.c -O3 -funroll-loops -o "strgroup.windows64.plugin"
```

Unix 64-bit:
```
gcc -shared -fPIC -DSYSTEM=OPUNIX stplugin.c strgroup.c -O3 -funroll-loops -o "strgroup.unix.plugin"
```

Mac OS X (Intel and ARM):
```
clang -bundle -o strgroup.macosx.x86_64 stplugin.c strgroup.c -DSYSTEM=APPLEMAC -target x86_64-apple-macos10.11
clang -bundle -o strgroup.macosx.arm64 stplugin.c strgroup.c -DSYSTEM=APPLEMAC -target arm64-apple-macos11
lipo -create -output strgroup.macosx.plugin strgroup.macosx.x86_64 strgroup.macosx.arm64
```

Additional information that may be helpful:

http://stackoverflow.com/questions/873812/how-to-compile-existing-posix-code-for-64-bit-windows

InspectExe allows you to to figure out if a compiled file is a 32-bit or 64-bit DLL: install InspectExe, right-click, properties->InspectExe

http://www.silurian.com/win32/inspect.htm

64-bit Cygwin should be able to compile to either 32-bit or 64-bit Windows. There are issues with longs, but `strgroup` doesn't use them

http://cygwin.com/cygwin-ug-net/programming.html

## Update History:
* **December 17, 2021**
  - Edited text of warning message in case of duplicate observations

* **February 28, 2020**
  - Package installation now downloads plugins for all platforms
  - **strgroup.ado** then autodetects which plugin to use based on user's platform
  - Thus, user is no longer required to do multiple installs when using a shared library with multiple platforms

## Author:

[Julian Reif](http://www.julianreif.com)
<br>University of Illinois
<br>jreif@illinois.edu
