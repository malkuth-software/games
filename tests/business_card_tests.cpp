// The business card's remarks, which GUI.Forms' card (since cp33) says through the shell's
// remark source: whole sentences, varied, with no slot left unfilled. (PlaySuite's former held
// card, shared/cards/held_card.*, is removed: GUI.Forms' card turns in the hand.)
#include "card_remarks.hpp"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

using namespace games;

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAILED: %s\n", what);
        ++failures;
    }
}

void remarks_are_whole_and_varied() {
    CardRemarks remarks(12345U);
    std::set<std::string> said;
    std::string previous;
    for (int i = 0; i < 400; ++i) {
        const std::string remark = remarks.next();
        check(!remark.empty(), "a remark has words");
        check(remark.find('{') == std::string::npos && remark.find('}') == std::string::npos, "every slot is filled");
        check(remark.front() >= 'A' && remark.front() <= 'Z', "a remark begins with a capital");
        const char end = remark.back();
        check(end == '.' || end == '?' || end == '!', "a remark ends a sentence");
        check(remark != previous, "no remark twice running");
        previous = remark;
        said.insert(remark);
    }
    check(said.size() > 250, "the remarks vary");
    std::printf("%zu different remarks in 400; for instance:\n", said.size());
    CardRemarks sample(99U);
    for (int i = 0; i < 8; ++i)
        std::printf("  %s\n", sample.next().c_str());
}

}  // namespace

int main() {
    remarks_are_whole_and_varied();
    if (failures > 0)
        return EXIT_FAILURE;
    std::printf("Business card passed\n");
    return EXIT_SUCCESS;
}
