# fast_recon — pure FASM x86-64 Linux build
#
# Tests live in tests/test_*.asm. Each assembles to a standalone binary
# in build/ and is run by the test harness; non-zero exit means fail.

FASM    := fasm
SRCDIR  := src
TESTDIR := tests
BUILD   := build

TEST_SRCS := $(wildcard $(TESTDIR)/test_*.asm)
TEST_BINS := $(patsubst $(TESTDIR)/%.asm,$(BUILD)/%,$(TEST_SRCS))

.PHONY: all test clean

all: $(BUILD)/fast_recon

$(BUILD)/fast_recon: $(SRCDIR)/fast_recon.asm $(wildcard $(SRCDIR)/*.inc) | $(BUILD)
	$(FASM) $< $@

$(BUILD)/test_%: $(TESTDIR)/test_%.asm $(wildcard $(SRCDIR)/*.inc) | $(BUILD)
	$(FASM) $< $@

test: $(TEST_BINS)
	@bash $(TESTDIR)/run_tests.sh $(BUILD)

$(BUILD):
	@mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
