# STRGROUP: match strings based on their Levenshtein edit distance

- Current version: `1.0.2 07aug2010`
- Jump to: [`updates`](#recent-updates) [`install`](#install) [`description`](#description) [`author`](#author)

-----------

## Updates:

* **January 14, 2019**
  - Updated Stata help files

## Install:

Type `which strgroup` at the Stata prompt to determine which version you have installed. To install the most recent version of `strgroup`, copy/paste the following line of code:

```
net install strgroup, from("https://raw.githubusercontent.com/reifjulian/strgroup/master") replace
```

To install the version that was uploaded to SSC, copy/paste the following line of code:
```
ssc install strgroup, replace
```

These two versions are typically synced, but occasionally the SSC version may be slightly out of date.

`strgroup` is implemented as a [C plugin](https://www.stata.com/plugins/) in order to minimize memory requirements and to maximize speed.  Plugins are specific to the hardware architecture and software framework of your computer. Define a platform by two characteristics: machine type and operating system.  Stata stores these characteristics in `c(machine_type)` and `c(os)`, respectively. `strgroup` supports the following platforms at this time:

| Machine type    | Operating system           |
| ------------- |:-------------:| 
| PC      | Windows |
| PC (64-bit x86-64)      | Windows      | 
| PC (64-bit x86-64)      | Unix      | 
| Macintosh      | MacOSX      | 
| Macintosh (Intel 64-bit)       | MacOSX      | 

## Description: 

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

## Author:

[Julian Reif](http://www.julianreif.com)
<br>University of Illinois
<br>jreif@illinois.edu
