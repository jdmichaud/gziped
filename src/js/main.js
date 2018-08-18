function inflate(buffer, mark) {
  const content = new Uint8Array(buffer);
  const metadata = Gziped.getMetadata(content);
  const output = new Uint8Array(metadata.filesize);
  performance.mark(`${mark}-start`);
  Gziped.inflate(content.slice(metadata.offset), output);
  performance.mark(`${mark}-end`);
  performance.measure(mark, `${mark}-start`, `${mark}-end`);
}

function loadUrl(url) {
  return fetch('resources/lesmiserables.gz')
    .then(res => res.arrayBuffer())
    .then(b => {
      inflate(b, 'inflate');
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
  console.log(`mean: ${mean}ms, standard deviation: ${stddev}ms`);
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
    inflate(content, 'inflate');
  }
  report('inflate');
}

function main() {
}

window.onload = main
