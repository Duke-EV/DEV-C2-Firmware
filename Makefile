.PHONY: help lint format check-format build upload

PIO ?= platformio
CPP_CHECK ?= cppcheck
CLANG_FORMAT ?= clang-format
BOARD ?= throttle

help:
	@printf "Available targets:\n"
	@printf "  make lint            Run C++ lint checks on the shared library and board sources\n"
	@printf "  make format          Auto-format C++ files with clang-format\n"
	@printf "  make check-format    Check formatting without modifying files\n"
	@printf "  make build BOARD=throttle   Build a specific board project\n"
	@printf "  make upload BOARD=throttle  Build and upload a specific board project\n"
	@printf "\nExamples:\n"
	@printf "  make build BOARD=communications\n"
	@printf "  make upload BOARD=peripherals\n"

lint:
	@command -v $(CPP_CHECK) >/dev/null 2>&1 || { echo "cppcheck is required. Install it with: sudo apt install cppcheck"; exit 1; }
	$(CPP_CHECK) --language=c++ --std=c++17 --enable=warning,style,performance,portability --quiet \
		--inline-suppr --suppress=missingIncludeSystem:*.h --suppress=missingIncludeSystem:*.hpp \
		boards lib

format:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo "clang-format is required. Install it with: sudo apt install clang-format"; exit 1; }
	find boards lib -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0 | xargs -0 $(CLANG_FORMAT) -i

check-format:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo "clang-format is required. Install it with: sudo apt install clang-format"; exit 1; }
	find boards lib -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0 | xargs -0 $(CLANG_FORMAT) --dry-run --Werror

build:
	$(PIO) run -d boards/$(BOARD)

upload:
	$(PIO) run -d boards/$(BOARD) -t upload
