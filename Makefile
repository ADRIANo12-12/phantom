PROJECT := phantom

.PHONY: all build clean run

all: build

build:
	@echo "Building $(PROJECT)..."

run:
	@echo "Starting $(PROJECT)..."

clean:
	@echo "Cleaning..."
