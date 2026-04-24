# Stage 1: Build the Data Model (C++)
FROM ubuntu:22.04 AS builder

# Install dependencies (No Qt!)
RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    libocct-data-exchange-dev \
    libocct-draw-dev \
    libocct-foundation-dev \
    libocct-modeling-algorithms-dev \
    libocct-modeling-data-dev \
    libocct-ocaf-dev \
    libocct-visualization-dev \
    git \
    python3 \
    python3-pip

# Install Python deps for MDA
RUN pip3 install PyYAML Jinja2

# Copy source
WORKDIR /app
COPY . .

# Run MDA Generator
RUN python3 mda_generator.py

# Build C++ Library
WORKDIR /app/build
RUN cmake .. && make -j$(nproc)

# Stage 2: Microservice Runtime (Python)
FROM python:3.10-slim

# Install runtime OCCT libs
RUN apt-get update && apt-get install -y \
    libocct-ocaf-7.6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /service
COPY --from=builder /app/build/libcore_data_model.so /usr/local/lib/
# Copy the FastAPI service
COPY ./scripts-service /service/scripts-service

# Environment setup
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

CMD ["python", "scripts-service/main.py"]
