import fs from 'fs';
// eslint-disable-next-line import/no-extraneous-dependencies
import binaryen from 'binaryen';

console.log('binaryen optimize start');

const mod = binaryen.readBinary(fs.readFileSync('./wasm/bcrypt.wasm'));
mod.optimize();

const wasmData = mod.emitBinary();
fs.writeFileSync('./wasm/bcrypt.wasm', wasmData);

console.log('binaryen optimize done');
