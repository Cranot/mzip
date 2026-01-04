# Multi-stage build for production Go application
# Stage 1: Build environment
FROM golang:1.21-alpine AS builder

# Build arguments
ARG VERSION=dev
ARG BUILD_TIME
ARG SERVICE_NAME=api-gateway

# Install build dependencies
RUN apk add --no-cache \
    git \
    ca-certificates \
    tzdata \
    make \
    gcc \
    musl-dev

# Set working directory
WORKDIR /app

# Copy go module files first for caching
COPY go.mod go.sum ./

# Download dependencies
RUN go mod download && go mod verify

# Copy source code
COPY . .

# Build the application
RUN CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build \
    -ldflags="-w -s -X main.Version=${VERSION} -X main.BuildTime=${BUILD_TIME}" \
    -o /app/build/${SERVICE_NAME} \
    ./cmd/${SERVICE_NAME}

# Stage 2: Runtime environment
FROM alpine:3.19 AS runtime

# Security: run as non-root user
RUN addgroup -g 1000 appgroup && \
    adduser -u 1000 -G appgroup -h /home/appuser -D appuser

# Install runtime dependencies
RUN apk add --no-cache \
    ca-certificates \
    tzdata \
    curl \
    dumb-init

# Set timezone
ENV TZ=UTC

# Create necessary directories
RUN mkdir -p /app/logs /app/config /app/data && \
    chown -R appuser:appgroup /app

# Copy binary from builder
COPY --from=builder /app/build/* /app/

# Copy configuration files
COPY --chown=appuser:appgroup configs/ /app/config/

# Set working directory
WORKDIR /app

# Switch to non-root user
USER appuser

# Expose application port
EXPOSE 8080
EXPOSE 9090

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# Use dumb-init as PID 1
ENTRYPOINT ["/usr/bin/dumb-init", "--"]

# Run the application
CMD ["./api-gateway", "--config", "/app/config/production.yaml"]

# Labels
LABEL maintainer="DevOps Team <devops@company.com>"
LABEL org.opencontainers.image.title="API Gateway Service"
LABEL org.opencontainers.image.description="Production API Gateway for microservices architecture"
LABEL org.opencontainers.image.version="${VERSION}"
LABEL org.opencontainers.image.vendor="Company Inc."
LABEL org.opencontainers.image.source="https://github.com/company/myapp"
LABEL org.opencontainers.image.licenses="MIT"

---

# Alternative: Node.js Application Dockerfile
# Uncomment below for Node.js services

# FROM node:20-alpine AS builder
#
# WORKDIR /app
#
# # Install dependencies
# COPY package*.json ./
# RUN npm ci --only=production
#
# # Copy source
# COPY . .
#
# # Build TypeScript
# RUN npm run build
#
# # Production stage
# FROM node:20-alpine AS runtime
#
# RUN addgroup -g 1000 nodegroup && \
#     adduser -u 1000 -G nodegroup -h /home/nodeuser -D nodeuser
#
# WORKDIR /app
#
# COPY --from=builder /app/dist ./dist
# COPY --from=builder /app/node_modules ./node_modules
# COPY --from=builder /app/package*.json ./
#
# USER nodeuser
#
# EXPOSE 3000
#
# HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
#     CMD node healthcheck.js || exit 1
#
# CMD ["node", "dist/index.js"]

---

# Alternative: Python Application Dockerfile
# Uncomment below for Python services

# FROM python:3.12-slim AS builder
#
# WORKDIR /app
#
# # Install build dependencies
# RUN apt-get update && apt-get install -y \
#     build-essential \
#     libpq-dev \
#     && rm -rf /var/lib/apt/lists/*
#
# # Install Python dependencies
# COPY requirements.txt .
# RUN pip install --no-cache-dir --prefix=/install -r requirements.txt
#
# # Production stage
# FROM python:3.12-slim AS runtime
#
# RUN useradd -m -u 1000 appuser
#
# WORKDIR /app
#
# # Copy installed packages
# COPY --from=builder /install /usr/local
#
# # Copy application
# COPY --chown=appuser:appuser . .
#
# USER appuser
#
# EXPOSE 8000
#
# HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
#     CMD python -c "import urllib.request; urllib.request.urlopen('http://localhost:8000/health')" || exit 1
#
# CMD ["gunicorn", "--bind", "0.0.0.0:8000", "--workers", "4", "app:create_app()"]
