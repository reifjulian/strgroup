cscript strgroup adofile strgroup

adopath ++ "../src"
set more off

version 13
program drop _all


******************************
* Run examples and verify output
******************************

* strgoup example
sysuse auto, clear
tempfile t
keep make price
replace make = make + "a" in 5
replace make = "gibberish" in 10
save "`t'"
sysuse auto, clear
keep make
merge make using "`t'", sort
list if _merge!=3
strgroup make if _merge!=3, gen(group) threshold(0.25)
cf _all using "compare/strgroup1.dta"

* levenshtein example 1
levenshtein mitten fitting
assert r(distance)==3

* levenshtein example 2
sysuse auto, clear
decode foreign, gen(foreign_string)
levenshtein make foreign_string, gen(edit_dist)
cf _all using "compare/levenshtein1.dta"


** EOF
