adopath ++ "../src"
set more off
version 17

which strgroup

******************************
* Timing test for strgroup
* Tests multiple scenarios for optimization benchmarking
******************************

* Number of iterations per test (for averaging)
local iters = 3

* Test sizes - smaller for quick checks, larger for scaling analysis
local sizes 10000 15000 20000

* Thresholds to test
local thresholds 0.3

foreach N of local sizes {

    * Generate random string data (varying lengths 5-15 chars)
    drop _all
    set seed 98765
    set obs `N'
    gen len = runiformint(5, 15)
    gen rstr = ""
    qui forval x = 1/15 {
        replace rstr = rstr + char(runiformint(65,90)) if `x' <= len
    }
    drop len

    di _n "===== N = `N' ====="

    foreach thresh of local thresholds {

        * Test without 'first' option
        timer clear 1
        forval i = 1/`iters' {
            cap drop group
            timer on 1
            qui strgroup rstr, gen(group) threshold(`thresh') force
            timer off 1
        }
        qui timer list 1
        local time_basic = r(t1) / `iters'
        qui sum group
        local ngroups = r(max)
        di "  threshold=`thresh', first=no:  " %6.3f `time_basic' "s avg (" `ngroups' " groups)"

        * Test with 'first' option
        timer clear 2
        forval i = 1/`iters' {
            cap drop group
            timer on 2
            qui strgroup rstr, gen(group) threshold(`thresh') force first
            timer off 2
        }
        qui timer list 2
        local time_first = r(t2) / `iters'
        qui sum group
        local ngroups_first = r(max)
        di "  threshold=`thresh', first=yes: " %6.3f `time_first' "s avg (" `ngroups_first' " groups)"
    }
}

di _n "===== Done ====="

** EOF
