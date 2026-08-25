# Ozayn Desktop App — Makefile

.PHONY: all build-core clean run install-deps

all: build-core

build-core:
	cd core && make all

clean:
	cd core && make clean

install-deps:
	pip install PyQt6 requests

run: build-core
	./run.sh

test-core:
	cd core && make test
