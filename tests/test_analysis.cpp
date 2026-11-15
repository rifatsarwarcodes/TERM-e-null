#include "TestFramework.hpp"

#include "analysis/BurstDetector.hpp"
#include "analysis/Fingerprint.hpp"
#include "analysis/PasteClassifier.hpp"
#include "editor/TextBuffer.hpp"

#include <variant>

using namespace ovc;
using namespace ovc::analysis;
using platform::InputEvent;
using platform::KeyCode;
using platform::KeyEvent;
using platform::PasteEvent;

namespace {

std::vector<InputEvent> keysFor(const std::string& text)
{
    std::vector<InputEvent> batch;
    for (char c : text) {
        if (c == '\n')
            batch.emplace_back(KeyEvent::special(KeyCode::Enter));
        else
            batch.emplace_back(KeyEvent::character(std::string(1, c)));
    }
    return batch;
}

const std::string StackCode = R"(bool isBalanced(const string& s) {
    stack<char> st;
    for (char c : s) {
        if (c == '(' || c == '[' || c == '{') st.push(c);
        else {
            if (st.empty()) return false;
            st.pop();
        }
    }
    return st.empty();
})";

} // namespace

TEST_CASE("burst: a long run of keys in one batch becomes a probable paste")
{
    BurstDetector detector(8);
    const auto events = detector.process(keysFor("int a;\nint b;"));
    CHECK_EQ(events.size(), std::size_t(1));
    const auto* paste = std::get_if<PasteEvent>(&events[0]);
    CHECK(paste && paste->text == "int a;\nint b;" && !paste->bracketed);
}

TEST_CASE("burst: normal typing and held-down keys stay keys")
{
    BurstDetector detector(8);
    CHECK_EQ(detector.process(keysFor("ab")).size(), std::size_t(2));
    CHECK_EQ(detector.process(keysFor("xxxxxxxxxxxx")).size(), std::size_t(12));
}

TEST_CASE("burst: other events split runs")
{
    BurstDetector detector(4);
    auto batch = keysFor("abcdef");
    batch.insert(batch.begin() + 2, KeyEvent::special(KeyCode::Escape));
    const auto events = detector.process(batch);
    CHECK_EQ(events.size(), std::size_t(2 + 1 + 1)); // a, b, Esc, paste "cdef"
    CHECK(std::holds_alternative<PasteEvent>(events.back()));
}

TEST_CASE("fingerprint: normalising drops comments and whitespace")
{
    CHECK_EQ(Fingerprinter::normalise("int  a ; // note\n /* x */ b"), std::string("inta;b"));
}

TEST_CASE("fingerprint: reformatted code still matches")
{
    FingerprintIndex index;
    index.add(StackCode);
    std::string reformatted = StackCode;
    for (auto& c : reformatted)
        if (c == '\n')
            c = ' ';
    CHECK(index.coverage(reformatted) > 0.99);
    CHECK(index.coverage("int main() { cout << 42 << endl; return 0; }") < 0.2);
}

TEST_CASE("fingerprint: short text falls back to substring check")
{
    FingerprintIndex index;
    index.add("st.push(c);");
    CHECK(index.coverage("st . push") > 0.99);
    CHECK(index.coverage("q.pop()") < 0.01);
}

TEST_CASE("classifier: self, allowed and outside")
{
    PasteClassifier classifier({"#include <iostream>\nusing namespace std;\nint main() {\n}\n"});
    CHECK(classifier.classify(StackCode, std::string("// mine\n") + StackCode) == PasteSource::Self);
    CHECK(classifier.classify("#include <iostream>\nusing namespace std;", "") == PasteSource::Allowed);
    CHECK(classifier.classify(StackCode, "int main() {}") == PasteSource::Outside);
}

TEST_CASE("classifier: deleted text still counts as the student's own")
{
    RemovedTextHistory history;
    history.onErase(0, StackCode);
    PasteClassifier classifier({});
    CHECK(classifier.classify(StackCode, history.text()) == PasteSource::Self);
}

TEST_CASE("retention: share of a paste still unchanged")
{
    editor::TextBuffer buffer;
    const auto origin = editor::Origin::paste(editor::OriginKind::Outside, 4);
    buffer.insert(0, "0123456789", origin);
    CHECK(retention(buffer.origins(), origin, 10) > 0.99);
    buffer.erase(0, 3);
    buffer.insert(0, "abc", editor::Origin::typed());
    CHECK(retention(buffer.origins(), origin, 10) > 0.69);
    CHECK(retention(buffer.origins(), origin, 10) < 0.71);
}
