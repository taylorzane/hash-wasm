#!/bin/bash

mkdir -p dist
mkdir -p wasm

npm run eslint

docker run \
  --rm \
  -v $(pwd):/app \
  -u $(id -u):$(id -g) \
  trzeci/emscripten-upstream:1.39.15 \
  sh -c /app/scripts/build_all.sh

# node scripts/optimize
node scripts/make_json
npx rollup -c
npx tsc ./lib/index --outDir ./dist --downlevelIteration --emitDeclarationOnly --declaration --resolveJsonModule --allowSyntheticDefaultImports

#-s ASSERTIONS=1 \