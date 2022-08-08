adopath ++ "../src"
cscript strgroup adofile strgroup
set more off

version 17
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

* Example using byable
sysuse auto, clear
strgroup make, gen(group1) threshold(.75)
by foreign: strgroup make, gen(group2) threshold(.75)
cf _all using "compare/strgroup2.dta"

* Examples using first and byable
drop _all
set seed 2312
set obs 1000
gen rstr = char(runiformint(65,90))
qui forval x = 1/10 {
	replace rstr = rstr + char(runiformint(65,90))
}
strgroup rstr, gen(group) threshold(.7)
strgroup rstr, gen(group2) threshold(.7) first
gen g = floor(_n/5)
isid rstr
gen sortid1 = _n
sort g sortid1
by g: strgroup rstr, gen(group3) threshold(.7) first
expand 2 in 1/5
gen sortid2 = _n
sort g sortid2
by g: strgroup rstr, gen(group4) threshold(.7) first
cf _all using "compare/strgroup3.dta"

* The following two snippets code should each only report a one-line warning message
sysuse auto, clear
expand 2
strgroup make, gen(group1) threshold(.75)
sort foreign
by foreign: strgroup make, gen(group2) threshold(.75)

sysuse auto, clear
expand 2 if foreign==0
strgroup make, gen(group1) threshold(.75)
sort foreign
by foreign: strgroup make, gen(group2) threshold(.75)

** EOF
