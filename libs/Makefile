ifeq ($(OS),Windows_NT)
    RM := del /Q
    MKDIR := mkdir
    DIR_SEP := \\
else
    RM := rm -f
    MKDIR := mkdir -p
    DIR_SEP := /
endif

.PHONY: all

# Shell command to list all directories except those named 'include'
DIRS := $(shell find . -type d -not -name include -not -path './include/*' -not -path '.')

all:
	@for dir in $(DIRS); do \
		echo "Entering $$dir"; \
		$(MAKE) -C $$dir clean; \
		$(MAKE) -C $$dir dist; \
		done
