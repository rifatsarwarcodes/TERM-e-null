# TERM-e-null (OV_C)

OV_C is a terminal-based exam environment for C++. Students write, compile and
test their code in a terminal editor while OV_C keeps a tamper-evident record
of how the solution was written: what was typed, what was pasted and from
where, when the student left the window, and every compile, run and test.
At the viva, the examiner opens a terminal dashboard that shows where each
line of the final code came from, replays the session and lists the parts of
the code worth asking about.

Built for SWE 4304 (Software Project Lab I), Islamic University of Technology.
C++17, standard library only, no network.

## Features

**Student**
- Editor that starts in Insert mode (arrows, Ctrl+S, Ctrl+Z work everywhere);
  Vim-style Normal mode with counts, operators and motions (`d3w`, `cc`, `5j`, `gg`)
- Problem statement panel, C++ syntax highlighting, search with `/`
- Compile (F5), run with an input file or interactively (F6), sample tests (F7),
  error list with jump to error (F8)
- Paste notes (Ctrl+N), the exam rules (`:rules`), time left in the status bar
- A receipt code when the session ends

**Recording**
- Every event goes to `events.log`, every edit to `journal.log`; both are
  SHA-256 hash chains, flushed as they happen
- Pastes are detected through bracketed paste (certain) or input bursts
  (probable) and classified as inside, self, allowed or outside
- Every character carries its origin; crashes are resumed from the journal
- A configurable penalty policy produces OK / Warning / Flagged; the tool never
  deducts marks

**Examiner**
- Class overview, session screen with integrity checks, timeline, paste
  details, compile/run/test history and recalculated penalties
- Code origin view, viva focus list, session replay, plain-text reports
- Receipt check against the attendance sheet

## Building

```
make            # builds build/ovc
make test       # builds and runs the unit tests
```

Works with g++ or clang++ on Linux and macOS, and with MinGW-w64 on Windows
(MSYS2 `make`, or `build.bat` / `build.bat test` from a plain command prompt).
Windows 10 or later is needed for the console features.

## Using it

### 1. The teacher prepares an exam

```
ovc --setup exam.ovx
```

The wizard asks for the exam code, title, time limit, problem statement,
sample tests, allowed files, rule values and the examiner passcode, and writes
a plain text package (format: [docs/exam-package.md](docs/exam-package.md)).
An example is in [examples/balanced-brackets](examples/balanced-brackets)
(passcode `examiner`).

### 2. The student takes the exam

Copy `exam.ovx` (and any template files) into the student's folder, then:

```
ovc main.cpp
```

OV_C asks for the student ID and name, then opens the editor. Without an
`exam.ovx` it runs a practice session.

| Keys | |
|---|---|
| typing, arrows, Home/End, PgUp/PgDn | edit and move (Insert mode) |
| Esc | Normal mode: `h j k l w b e 0 ^ $ gg G`, `x dd yy p P u Ctrl+R`, `d/c/y` + motion, `o O i a A I`, `/` `n` `N` |
| Ctrl+S / `:w` | save |
| F5 / `:compile` | compile |
| F6 / `:run`, `:run in.txt` | run interactively, or with an input file (recorded) |
| F7 / `:test` | run the sample tests |
| F8 / `:cn` | next compiler error |
| F2, `:problem` | problem panel, full problem |
| Ctrl+N, `:note text` | explain the last paste |
| `:rules`, F1 | exam rules, help |
| Ctrl+Q / `:q` | end the exam (asks first) |

At the end OV_C shows a receipt code such as `7F3A-91C2-B0`; the invigilator
writes it on the attendance sheet.

### 3. The examiner reviews

```
ovc --examiner                         # class overview of the package's session folder
ovc --examiner sessions/LT2/240042125  # one session
```

or type `:dashboard` in the student's running editor at the viva. Both ask for
the examiner passcode.

| Keys | |
|---|---|
| Up/Down, Enter | select, open |
| G / V | code origin view / viva focus list |
| R | replay (Left/Right, n/N jump between pastes, compiles and tests) |
| O | all saved compile, run and test outputs |
| K | type the receipt from the attendance sheet to check it |
| X | export a plain-text report |
| S | (class overview) change the sort column |
| Esc / Q | back / quit |

## Project layout

```
src/
  common/     strings, files, time, escaping
  crypto/     SHA-256, passcode hashing
  platform/   terminal (POSIX, Windows, scripted), input decoding, processes
  ui/         screen buffer, ANSI renderer, editor view, highlighting
  editor/     gap buffer, origins, commands/undo, motions, modes, editor
  session/    events, event bus, session folder, exam session and clock
  evidence/   hash chains, edit journal and replay, seal and receipt
  exam/       exam package parser and setup wizard
  analysis/   burst detection, fingerprints, paste classification
  penalty/    policy, rules and penalty engine
  build/      compiler messages, output comparison, build runner
  dashboard/  session loading and verification, viva focus, screens, reports
  app/        student application, command line, launch
tests/        unit and end-to-end tests (assert-style runner)
docs/         architecture and file formats
```

See [docs/architecture.md](docs/architecture.md) for how the modules work
together.

## Limits

- Paste detection depends on the console: bracketed paste is certain, burst
  detection is a labelled guess. Test on the lab machines.
- Everything runs on the student's machine. The hash chains, the receipt on
  the attendance sheet and the examiner's own copy of the package make changes
  visible; the passcode is only a small obstacle.
- Output of interactive runs is not recorded; use `:run file` or `:test`.
