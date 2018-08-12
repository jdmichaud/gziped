import typescript from 'rollup-plugin-typescript2';

export default {
  input: 'gziped.ts',
  output: [{
    name: 'Gziped',
    file: 'dist/gziped.js',
    format: 'umd',
  }],
  plugins: [
    typescript(),
  ],
}
