# Project build configuration
# Auto-generated Makefile for multi-service application

.PHONY: all build test clean install deps lint format docker-build docker-push deploy help

# Configuration
PROJECT_NAME := myapp
VERSION := $(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")
BUILD_TIME := $(shell date -u '+%Y-%m-%d_%H:%M:%S')
GO_VERSION := $(shell go version | cut -d' ' -f3)
LDFLAGS := -ldflags "-X main.Version=$(VERSION) -X main.BuildTime=$(BUILD_TIME)"

# Directories
SRC_DIR := ./cmd
PKG_DIR := ./pkg
INTERNAL_DIR := ./internal
BUILD_DIR := ./build
DIST_DIR := ./dist

# Go settings
GOCMD := go
GOBUILD := $(GOCMD) build
GOTEST := $(GOCMD) test
GOGET := $(GOCMD) get
GOMOD := $(GOCMD) mod
GOFMT := gofmt
GOLINT := golangci-lint

# Docker settings
DOCKER := docker
DOCKER_REGISTRY := ghcr.io/myorg
DOCKER_IMAGE := $(DOCKER_REGISTRY)/$(PROJECT_NAME)

# Kubernetes settings
KUBECTL := kubectl
KUSTOMIZE := kustomize
HELM := helm

# Services
SERVICES := api-gateway user-service order-service payment-service notification-service

all: deps lint test build

help:
	@echo "Available targets:"
	@echo "  all          - Run deps, lint, test, and build"
	@echo "  build        - Build all services"
	@echo "  test         - Run all tests"
	@echo "  clean        - Remove build artifacts"
	@echo "  deps         - Download dependencies"
	@echo "  lint         - Run linter"
	@echo "  format       - Format code"
	@echo "  docker-build - Build Docker images"
	@echo "  docker-push  - Push Docker images"
	@echo "  deploy       - Deploy to Kubernetes"

deps:
	@echo "==> Downloading dependencies..."
	$(GOMOD) download
	$(GOMOD) verify
	$(GOMOD) tidy

build: $(SERVICES)

$(SERVICES):
	@echo "==> Building $@..."
	@mkdir -p $(BUILD_DIR)
	CGO_ENABLED=0 GOOS=linux GOARCH=amd64 $(GOBUILD) $(LDFLAGS) \
		-o $(BUILD_DIR)/$@ $(SRC_DIR)/$@/main.go

build-darwin:
	@echo "==> Building for macOS..."
	@for service in $(SERVICES); do \
		CGO_ENABLED=0 GOOS=darwin GOARCH=amd64 $(GOBUILD) $(LDFLAGS) \
			-o $(BUILD_DIR)/$$service-darwin $(SRC_DIR)/$$service/main.go; \
	done

build-windows:
	@echo "==> Building for Windows..."
	@for service in $(SERVICES); do \
		CGO_ENABLED=0 GOOS=windows GOARCH=amd64 $(GOBUILD) $(LDFLAGS) \
			-o $(BUILD_DIR)/$$service.exe $(SRC_DIR)/$$service/main.go; \
	done

test:
	@echo "==> Running tests..."
	$(GOTEST) -v -race -coverprofile=coverage.out ./...
	$(GOCMD) tool cover -html=coverage.out -o coverage.html

test-short:
	@echo "==> Running short tests..."
	$(GOTEST) -v -short ./...

test-integration:
	@echo "==> Running integration tests..."
	$(GOTEST) -v -tags=integration ./...

benchmark:
	@echo "==> Running benchmarks..."
	$(GOTEST) -bench=. -benchmem ./...

lint:
	@echo "==> Running linter..."
	$(GOLINT) run --timeout=5m ./...

format:
	@echo "==> Formatting code..."
	$(GOFMT) -s -w .
	goimports -w .

clean:
	@echo "==> Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -rf $(DIST_DIR)
	rm -f coverage.out coverage.html
	$(GOCMD) clean -cache -testcache

install: build
	@echo "==> Installing binaries..."
	@for service in $(SERVICES); do \
		cp $(BUILD_DIR)/$$service /usr/local/bin/; \
	done

# Docker targets
docker-build:
	@echo "==> Building Docker images..."
	@for service in $(SERVICES); do \
		$(DOCKER) build \
			--build-arg VERSION=$(VERSION) \
			--build-arg BUILD_TIME=$(BUILD_TIME) \
			-t $(DOCKER_IMAGE)-$$service:$(VERSION) \
			-t $(DOCKER_IMAGE)-$$service:latest \
			-f deployments/docker/$$service/Dockerfile .; \
	done

docker-push:
	@echo "==> Pushing Docker images..."
	@for service in $(SERVICES); do \
		$(DOCKER) push $(DOCKER_IMAGE)-$$service:$(VERSION); \
		$(DOCKER) push $(DOCKER_IMAGE)-$$service:latest; \
	done

docker-compose-up:
	@echo "==> Starting services with docker-compose..."
	$(DOCKER) compose -f deployments/docker-compose.yml up -d

docker-compose-down:
	@echo "==> Stopping services..."
	$(DOCKER) compose -f deployments/docker-compose.yml down

# Kubernetes targets
deploy:
	@echo "==> Deploying to Kubernetes..."
	$(KUSTOMIZE) build deployments/k8s/overlays/production | $(KUBECTL) apply -f -

deploy-staging:
	@echo "==> Deploying to staging..."
	$(KUSTOMIZE) build deployments/k8s/overlays/staging | $(KUBECTL) apply -f -

deploy-dev:
	@echo "==> Deploying to development..."
	$(KUSTOMIZE) build deployments/k8s/overlays/development | $(KUBECTL) apply -f -

rollback:
	@echo "==> Rolling back deployment..."
	@for service in $(SERVICES); do \
		$(KUBECTL) rollout undo deployment/$$service; \
	done

# Helm targets
helm-install:
	@echo "==> Installing Helm chart..."
	$(HELM) install $(PROJECT_NAME) deployments/helm/$(PROJECT_NAME) \
		--namespace $(PROJECT_NAME) \
		--create-namespace \
		--values deployments/helm/$(PROJECT_NAME)/values-production.yaml

helm-upgrade:
	@echo "==> Upgrading Helm release..."
	$(HELM) upgrade $(PROJECT_NAME) deployments/helm/$(PROJECT_NAME) \
		--namespace $(PROJECT_NAME) \
		--values deployments/helm/$(PROJECT_NAME)/values-production.yaml

helm-uninstall:
	@echo "==> Uninstalling Helm release..."
	$(HELM) uninstall $(PROJECT_NAME) --namespace $(PROJECT_NAME)

# Database migrations
migrate-up:
	@echo "==> Running database migrations..."
	migrate -path ./migrations -database "$(DATABASE_URL)" up

migrate-down:
	@echo "==> Rolling back database migrations..."
	migrate -path ./migrations -database "$(DATABASE_URL)" down 1

migrate-create:
	@echo "==> Creating new migration..."
	migrate create -ext sql -dir ./migrations -seq $(name)

# Code generation
generate:
	@echo "==> Generating code..."
	$(GOCMD) generate ./...

proto:
	@echo "==> Generating protobuf files..."
	protoc --go_out=. --go-grpc_out=. proto/*.proto

swagger:
	@echo "==> Generating Swagger documentation..."
	swag init -g $(SRC_DIR)/api-gateway/main.go -o ./docs/swagger

# Security scanning
security-scan:
	@echo "==> Running security scan..."
	gosec -fmt=json -out=security-report.json ./...
	trivy fs --security-checks vuln,config .

# Release
release: clean test build docker-build docker-push
	@echo "==> Creating release $(VERSION)..."
	@mkdir -p $(DIST_DIR)
	@for service in $(SERVICES); do \
		tar -czvf $(DIST_DIR)/$$service-$(VERSION)-linux-amd64.tar.gz \
			-C $(BUILD_DIR) $$service; \
	done
	gh release create $(VERSION) $(DIST_DIR)/*.tar.gz \
		--title "Release $(VERSION)" \
		--notes-file CHANGELOG.md

# Development helpers
dev:
	@echo "==> Starting development environment..."
	$(DOCKER) compose -f deployments/docker-compose.dev.yml up -d
	air -c .air.toml

watch:
	@echo "==> Watching for changes..."
	watchexec -e go -r make build

# Print variables
print-%:
	@echo '$*=$($*)'
