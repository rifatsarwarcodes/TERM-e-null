# Exam package format

An exam package is a plain text file (usually `exam.ovx`) that the teacher
prepares before the exam, either with `ovc --setup` or by hand. It is copied
next to the students' files. OV_C records the package's SHA-256 hash in
`SESSION_START`, and the examiner keeps the original, so a changed package is
noticed.

```
# Lines starting with # are comments.

[exam]
code = SWE4304-LT2
title = Lab Test 2: Balanced Brackets
time_limit = 90               # minutes, 0 for no limit
sessions = sessions/LT2       # where session folders are created
compiler = g++
flags = -std=c++17 -O2 -Wall
run_time_limit = 2            # seconds per program run or test
passcode = sha256-iter:10000:<salt>:<hash>   # written by ovc --setup

[problem]
Free text shown in the student's problem panel.
Everything until the next [section] line belongs to the problem.

[test 1]
input <<<
3
()
([)]
{[]}
>>>
output <<<
YES
NO
YES
>>>

[test 2]
input = 1\n{}                 # short form, \n for line breaks
output = YES

[allowed]
file = template.cpp           # read relative to the package file
snippet <<<
#include <bits/stdc++.h>
using namespace std;
>>>

[policy]
small_paste_chars = 80        # outside pastes up to this size cost nothing
small_paste_lines = 2
large_paste_chars = 300
large_paste_lines = 10
large_paste_points = 3
repeat_count = 3              # more than 3 outside pastes...
repeat_window_min = 10        # ...within 10 minutes
repeat_points = 2
focus_return_sec = 60         # outside paste within 60 s of coming back
focus_return_points = 2
unchanged_ratio = 0.80        # 80% of a paste still unchanged when tested
unchanged_points = 3
passcode_fail_limit = 3
warning_at = 3
flagged_at = 8
```

Text blocks start with `key <<<` and end with a line containing only `>>>`.
Expected output is compared line by line, ignoring trailing spaces and
trailing empty lines.

The passcode is never stored in plain text. Use `ovc --setup` to create the
hash; a package without a passcode lets anyone open the dashboard and is only
meant for practice.
