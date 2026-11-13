# Thin wrapper over the Python tools, which do the work on every platform.
PYTHON ?= python3

.PHONY: all ext setup test clean

all:
	$(PYTHON) tools/build.py -q

# the original's code with only the extensions
ext:
	$(PYTHON) tools/build.py -q --ports none --out build/ext

setup:
	$(PYTHON) tools/setup.py

test: all ext
	$(PYTHON) tools/exttest.py
	$(PYTHON) tools/exttest.py --cand build/ext/mwccps2.exe
	$(PYTHON) tools/difftest.py --cand build/mwccps2.exe --flags O0,O2,O4p,O4pg,imoq --manifest tests/cli/manifest.json --run check

clean:
	rm -rf build
