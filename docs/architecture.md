# Architecture

OV_C is one program with three modes (student, examiner, setup) built from
small modules. Modules talk through interfaces and the event bus, never
through global data.

```
                 exam package (exam.ovx)
                          |
   +----------------------v-----------------------+
   | Student mode                                 |
   |                                              |
   |  platform --keys/pastes/focus--> StudentApp  |
   |  (terminal, processes)            |    |     |
   |                                   |    v     |
   |                        editor <---+  build   |
   |                (buffer, origins,       (g++, |
   |                 modes, undo)           tests)|
   |                      |                       |
   |              edits   |   events              |
   |                      v                       |
   |   JournalWriter   ExamSession --> EventBus   |
   |        |                        /   |    \   |
   |        |          EvidenceRecorder  |  StudentNotifier
   |        |                 |    PenaltyEngine  |
   +--------|-----------------|-------------------+
            v                 v
       journal.log        events.log      pastes/  runs/  submission/  seal.txt
            \                 |                   /
             +----------------+------------------+
                              |  reads (never writes)
   +--------------------------v-------------------+
   | Examiner mode: SessionLoader -> SessionRecord |
   |   verify chains, seal, receipt, files         |
   |   replay journal -> final code with origins   |
   |   recalculate penalties, build viva focus     |
   |   screens: overview, session, origin, replay  |
   +-----------------------------------------------+
```

## One student action, step by step

A student pastes 40 lines copied from a website:

1. The terminal wraps the text in bracketed-paste markers. `InputDecoder`
   turns it into a `PasteEvent` (without markers, `BurstDetector` recognises
   the burst of queued keys instead and marks it as probable).
2. `StudentApp` asks `PasteClassifier` where it came from. Fingerprints are
   compared with the session's own text and the allowed snippets: outside.
3. The editor inserts it as one `InsertTextCommand` tagged with origin
   `O#<n>`. `JournalWriter` (an `IEditObserver`) appends the insert to the
   chained `journal.log`.
4. `ExamSession` stamps a `PASTE` event with sequence number `n` and publishes
   it on the `EventBus`.
5. `EvidenceRecorder` appends it to the chained `events.log`. `PenaltyEngine`
   runs its rules; `LargePasteRule` fires and the engine publishes `RULE_HIT`
   and `LEVEL_CHANGE`, which are queued behind the paste. `StudentNotifier`
   shows a neutral notice.
6. At the viva the dashboard replays the journal, finds the 40 lines still
   tagged `O#<n>` and puts them at the top of the viva focus list.

## Object-oriented design

| Idea | Where |
|---|---|
| Inheritance / polymorphism | `Mode` (Insert, Normal, Prompt), `Command` (Insert, Delete, Composite), `PenaltyRule` (5 rules), `Screen` (5 screens), `ITerminal` (POSIX, Windows, scripted), `IProcessRunner` |
| Observer | `EventBus` + `IEventListener`; `TextBuffer` + `IEditObserver` |
| Command | every edit is a `Command` with `execute`/`undo`; undo, redo and the journal share it |
| State | editor modes; penalty level `OK -> Warning -> Flagged` |
| Strategy | paste detection (bracketed / burst), penalty rules |
| Composition | `Editor` owns buffer, undo manager and modes; `StudentSession` owns logs, recorder and engine |

## Algorithms written for the project

| Algorithm | File |
|---|---|
| Gap buffer with a line-start index | `editor/GapBuffer`, `editor/TextBuffer` |
| Run-length origin map | `editor/OriginMap` |
| State machine for `[count][op][count][motion]` | `editor/NormalCommandParser` |
| Knuth-Morris-Pratt search | `editor/KmpSearch` |
| Screen diffing (changed rows only) | `ui/AnsiRenderer` |
| Terminal input decoder (UTF-8, CSI/SS3, bracketed paste, focus) | `platform/InputDecoder` |
| SHA-256 (FIPS 180-4), salted iterated passcode hash | `crypto/` |
| Hash chain with crash-tolerant reading | `evidence/HashChain` |
| Journal replay with per-character insertion times | `evidence/Journal` |
| Karp-Rabin rolling hash + winnowing | `analysis/Fingerprint` |
| Burst detection over input batches | `analysis/BurstDetector` |
| Sliding-window penalty rules | `penalty/` |
| Interval merging for the viva focus list | `dashboard/VivaFocus` |
| Stable bottom-up merge sort | `dashboard/MergeSort` |

## Files in a session folder

```
sessions/<exam>/<student id>/
  session.info        key=value: student, exam, file, start time, limit, package hash
  events.log          one event per line, hash-chained
  journal.log         one edit per line, hash-chained
  pastes/NNN.txt      text of paste event NNN
  runs/NNN.txt        compiler output / program output / test report of event NNN
  submission/<file>   final file, hash stored in SESSION_END
  seal.txt            last hash of both logs and the receipt code
  build/              compiled program and test inputs
```

### events.log

```
<seq>|<epoch ms>|<TYPE>|key=value;key=value|h=<sha256>
```

`h` is `SHA-256(previous h + "\n" + everything before "|h=")`; the first line
chains from 64 zeros. `| ; = \` and line breaks inside values are written as
`\p \s \e \\ \n`.

Event types: `SESSION_START`, `SESSION_RESUMED`, `SESSION_END`, `SAVE`,
`PASTE`, `PASTE_NOTE`, `FOCUS_LOST`, `FOCUS_GAINED`, `COMPILE`, `RUN`, `TEST`,
`FILE_CHANGED_OUTSIDE`, `TIME_WARNING`, `RULE_HIT`, `LEVEL_CHANGE`,
`DASHBOARD_OPENED`, `DASHBOARD_CLOSED`, `PASSCODE_FAILED`.

### journal.log

```
<seq>|<epoch ms>|INS|pos=<offset>;origin=<T|L|I#n|S#n|A#n|O#n>;text=<text>|h=<sha256>
<seq>|<epoch ms>|DEL|pos=<offset>;len=<bytes>;text=<removed text>|h=<sha256>
```

Origins: typed, loaded (in the file at start), and pastes from inside the
editor, self, allowed snippet or outside, with the paste's event number.

### Receipt

`receipt = first 10 hex digits of SHA-256(events head + journal head)`,
written as `XXXX-XXXX-XX`.

## Testing

`make test` runs about 120 test cases: every module on its own, the editor
driven by key sequences, the student app driven end to end through a scripted
terminal and a fake compiler, tampering with logs and files, and every
dashboard screen.
