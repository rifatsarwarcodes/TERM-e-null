#pragma once

namespace ovc::penalty {

// The numbers behind the paste rules. They come from the [policy] section of
// the exam package so teachers can agree on them per exam.
struct PenaltyPolicy {
    int smallPasteChars = 80;     // outside pastes up to this size are only recorded
    int smallPasteLines = 2;
    int largePasteChars = 300;    // more than this many characters or lines is "large"
    int largePasteLines = 10;
    int largePastePoints = 3;

    int repeatCount = 3;          // more than this many outside pastes...
    int repeatWindowMinutes = 10; // ...within this window
    int repeatPoints = 2;

    int focusReturnSeconds = 60;  // outside paste this soon after coming back
    int focusReturnPoints = 2;

    double unchangedRatio = 0.80; // share of a paste still unchanged when tested
    int unchangedPoints = 3;

    int passcodeFailLimit = 3;    // wrong examiner passcodes before flagging

    int warningAt = 3;
    int flaggedAt = 8;
};

} // namespace ovc::penalty
