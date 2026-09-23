# MaxRegnerOS Cloud Avatar — top level orchestration.
# Everything here is a thin wrapper; the real work lives in the subprojects.

SHELL := /bin/sh
PY ?= python3
PIP ?= $(PY) -m pip

.PHONY: help
help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) \
		| awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-18s\033[0m %s\n", $$1, $$2}'

# ── client ────────────────────────────────────────────────────
.PHONY: client
client:  ## Build the client for this host (dev only)
	sh client/src/build.sh --host

.PHONY: client-test
client-test:  ## Run the client unit suite
	make -C client/src test

.PHONY: client-termux
client-termux:  ## Build the client on-device with Termux clang
	sh client/src/build.sh --termux

.PHONY: client-clean
client-clean:
	make -C client/src clean

# ── server ────────────────────────────────────────────────────
.PHONY: server-install
server-install:  ## Install the server into the active environment
	$(PIP) install -e 'server[jpeg,dev]'

.PHONY: server-test
server-test:  ## Run the server test suite
	cd server && $(PY) -m pytest -q

.PHONY: server-cov
server-cov:
	cd server && $(PY) -m pytest --cov=mrca_server --cov-report=term-missing

.PHONY: server-check
server-check:  ## Verify chromium, pillow and the differ are usable
	cd server && PYTHONPATH=src $(PY) -m mrca_server --check

.PHONY: server
server:  ## Run the server with default ports
	cd server && PYTHONPATH=src $(PY) -m mrca_server

# ── gateway ───────────────────────────────────────────────────
.PHONY: gateway
gateway:  ## Build the gateway binary
	cd gateway && go build -o ../dist/mrca-gateway ./cmd/gateway

.PHONY: gateway-test
gateway-test:
	cd gateway && go test ./...

# ── console ───────────────────────────────────────────────────
.PHONY: console
console:  ## Install console dev dependencies
	cd console && npm install

.PHONY: console-dev
console-dev:  ## Run the console dev server on :5180
	cd console && npm run dev

# ── vectors and benchmarks ────────────────────────────────────
.PHONY: vectors
vectors:  ## Regenerate the codec conformance corpus
	$(PY) tools/gen_vectors.py --out tests/vectors

.PHONY: bench
bench:  ## Benchmark the differ against the corpus
	$(PY) tools/bench.py --vectors tests/vectors

.PHONY: probe
probe:  ## End-to-end probe against a running server
	$(PY) tools/probe.py --host $${HOST:-127.0.0.1} --token "$${MRCA_SHARED_SECRET:-}"

# ── packaging ─────────────────────────────────────────────────
.PHONY: dist
dist:  ## Assemble the release archives
	sh tools/package.sh

.PHONY: clean
clean: client-clean
	rm -rf dist
	find . -name __pycache__ -type d -prune -exec rm -rf {} + 2>/dev/null || true
