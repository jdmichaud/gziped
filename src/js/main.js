function inflateWA(buffer, mark) {
  const content = new Uint8Array(buffer);
  const metadata = Gziped.getMetadata(content);
  const output = Module._malloc(metadata.filesize);
  // We could allocate once and always copy the data into the same place
  const input = Module._malloc((content.length + metadata.offset) * content.BYTES_PER_ELEMENT);
  Module.HEAP8.set(content.slice(metadata.offset), input);
  performance.mark(`${mark}-start`);
  Module._em_inflate(input, output);
  performance.mark(`${mark}-end`);
  performance.measure(mark, `${mark}-start`, `${mark}-end`);
  Module._free(input);
  Module._free(output);
}

function inflateJS(buffer, mark) {
  const content = new Uint8Array(buffer);
  const metadata = Gziped.getMetadata(content);
  const output = new Uint8Array(metadata.filesize);
  const input = content.slice(metadata.offset);
  performance.mark(`${mark}-start`);
  Gziped.inflate(input, output);
  performance.mark(`${mark}-end`);
  performance.measure(mark, `${mark}-start`, `${mark}-end`);
}

function loadUrl(url) {
  return fetch('resources/lesmiserables.gz')
    .then(res => res.arrayBuffer())
    .then(b => {
      inflateJS(b, 'inflate');
      performance.measure('inflate', 'inflate-start', 'inflate-end');
      return performance.getEntriesByName("inflate")[0].duration;
    });
}

function readFileAsText(file) {
  return new Promise(resolve => {
    const reader = new FileReader();
    reader.onload = (event) => {
      if (event.target.readyState === FileReader.DONE) {
        resolve(event.target.result);
      }
    }
    reader.readAsText(file);
  });
}

function readFile(file) {
  return new Promise(resolve => {
    const reader = new FileReader();
    reader.onload = (event) => {
      if (event.target.readyState === FileReader.DONE) {
        resolve(event.target.result);
      }
    }
    reader.readAsArrayBuffer(file);
  });
}

function report(mark) {
  const measures = performance.getEntriesByName(mark);
  const mean = measures.reduce((acc, m) => acc + m.duration, 0) / measures.length;
  const stddev = Math.sqrt(measures.reduce((acc, m) => acc + ((m.duration - mean) ** 2), 0) / (measures.length - 1));
  console.log(`mean: ${mean.toFixed(2)} ms, standard deviation: ${stddev.toFixed(2)} ms`);
}

async function handleFiles(fileList) {
  const files = []
  // First, convert that ugly object to a proper array
  for (let i = 0; i < fileList.length; ++i) {
    files.push(fileList[i]);
  }
  // Load index files
  const indexFile = files.find(f => f.name === 'index.txt');
  if (indexFile === undefined) {
    throw new Error('no index.txt in selected file');
    return;
  }
  const indexContent = await readFileAsText(indexFile);
  const index = indexContent
    .split('\n')
    .map(line => line.trim())
    .map(line => {
      [filename, md5] = line.split(' ');
      return { filename, md5 };
    });

  const zipfiles = files.filter(f => f.name.endsWith('.gz'));
  // Check all zip files are in the index
  if (zipfiles.some(f => index.find(e => e.filename === f.name) === undefined)) {
    throw new Error('missing file in index');
    return;
  }
  for (let zipfile of zipfiles) {
    console.log(`inflating ${zipfile.name}`);
    const content = await readFile(zipfile);
    inflateJS(content, 'inflateJS');
    inflateWA(content, 'inflateWA');
  }
  report('inflateJS');
  report('inflateWA');
}

window.test = () => {
  return fetch('resources/lesmiserables.gz')
    .then(res => res.arrayBuffer())
    .then(b => {
      const content = new Uint8Array(b);
      const metadata = Gziped.getMetadata(content);
      const output = Module._malloc(metadata.filesize);
      // We could allocate once and always copy the data into the same place
      const input = Module._malloc((content.length - metadata.offset) * content.BYTES_PER_ELEMENT);
      Module.HEAPU8.set(content.slice(metadata.offset), input);
      const then = performance.now();
      window._inflateWA(input, output);
      console.log(performance.now() - then);
      window.output = output;
      window.metadata = metadata;
      window.deflated = new TextDecoder('utf-8').decode(Module.HEAPU8.slice(output, output + metadata.filesize));
    });
}

function main() {
  Module.onRuntimeInitialized = _ => {
    console.log(Module);
  };
}

window.onload = main
