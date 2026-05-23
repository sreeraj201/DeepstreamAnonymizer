#!/bin/bash

set -e

if [ "$#" -ne 3 ]; then
    echo "Usage:"
    echo "./scripts/generate_embeddings.sh <faces_folder> <arcface.onnx> <output_folder>"
    exit 1
fi

FACES_FOLDER=$1
MODEL_PATH=$2
OUTPUT_FOLDER=$3

mkdir -p ${OUTPUT_FOLDER}

ONNXRUNTIME_DIR=third_party/onnxruntime-linux-aarch64-1.23.0

echo "Compiling generate_embeddings.cpp..."

g++ tools/generate_embeddings.cpp \
    -o generate_embeddings \
    -std=c++17 \
    `pkg-config --cflags --libs opencv4` \
    -I${ONNXRUNTIME_DIR}/include \
    -L${ONNXRUNTIME_DIR}/lib \
    -lonnxruntime

export LD_LIBRARY_PATH=${ONNXRUNTIME_DIR}/lib:$LD_LIBRARY_PATH

echo ""
echo "Generating embeddings..."

./generate_embeddings \
    "$FACES_FOLDER" \
    "$MODEL_PATH" \
    "$OUTPUT_FOLDER"

echo ""
echo "Done."