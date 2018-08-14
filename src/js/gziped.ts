const FTEXT    =       1;
const FHCRC    = (1 << 1);
const FEXTRA   = (1 << 2);
const FNAME    = (1 << 3);
const FCOMMENT = (1 << 4);

const OS = [
  'FAT filesystem (MS-DOS, OS/2, NT/Win32)',
  'Amiga',
  'VMS (or OpenVMS)',
  'Unix',
  'VM/CMS',
  'Atari TOS',
  'HPFS filesystem (OS/2, NT)',
  'Macintosh',
  'Z-System',
  'CP/M',
  'TOPS-20',
  'NTFS filesystem (NT)',
  'QDOS',
  'Acorn RISCOS',
];

class Position {
  public index = 0;
  public mask = 0;
}

class Metadata {
  // header
  public compressionMethod: number;
  public flag: number;
  public modificationTime: number;
  public extraFlag: number;
  public os: string;
  public extraFlagLength: number;
  public filename: string;
  public comment: string;
  public crc16: number;
  // footer
  public crc32: number;
  public filesize: number;
  // app metadata
  public offset: number; // where does the first block starts
}

const textDecoder = new TextDecoder("utf-8");

function getInteger(buf: Uint8Array, offset: number, length: number): number {
  let value = 0;
  let pos = length;
  do {
    const shift = pos - 1;
    value |= buf[offset + shift] << 2 ** shift;
  } while (pos--);
  return value;
}

function inflate_block(buf: Uint8Array, bufpos: Position,
                       litdict: any, distdict: any,
                       output: Uint8Array, outputpos: Position): void {
}

export function inflate(buf: Uint8Array, output: Uint8Array): void {
}

export function getMetadata(buf: Uint8Array): Metadata {
  let offset = 0;
  // Check magic
  if (buf[offset++] !== 0x1F || buf[offset++] !== 0x8B) {
    throw new Error('invalid gzip format (bad magic number)');
  }
  // retrieve header fields
  const compressionMethod = buf[offset++];
  if (compressionMethod !== 8) {
    throw new Error(`unknown compression method ${compressionMethod}`);
  }
  const flag = buf[offset++];
  const modificationTime = getInteger(buf, offset, 4);
  offset += 4;
  const extraFlag = buf[offset++];
  const os = OS[buf[offset++]];
  // we are supposed to be at offset 10 here
  let extraFlagLength = 0;
  if (flag & FEXTRA) {
    extraFlagLength = getInteger(buf, offset, 2);
    offset += 2;
  }
  offset += extraFlagLength; // just skip the extra header
  // retrieve 0 terminated filename
  let endofstr = offset;
  let filename = '';
  if (flag & FNAME) {
    while (buf[endofstr++] !== 0) {}
    filename = textDecoder.decode(buf.slice(offset, endofstr - 1));
    offset = endofstr;
  }
  // retrieve 0 terminated comment
  let comment = '';
  if (flag & FCOMMENT) {
    while (buf[endofstr++] !== 0) {}
    comment = textDecoder.decode(buf.slice(offset, endofstr - 1));
    offset = endofstr;
  }
  let crc16 = 0;
  if (flag & FHCRC) {
    crc16 = getInteger(buf, offset, 2);
    offset += 2;
  }
  // retrieve footer fields
  const size = buf.length;
  const crc32 = buf[size - 8] | buf[size - 7] << 8 |
    buf[size - 6] << 16 | buf[size - 5] << 24;
  const filesize = buf[size - 4] | buf[size - 3] << 8 |
    buf[size - 2] << 16 | buf[size - 1] << 24;

  return {
    compressionMethod,
    flag,
    modificationTime,
    extraFlag,
    os,
    extraFlagLength,
    filename,
    comment,
    crc16,
    crc32,
    filesize,
    offset,
  };
}
