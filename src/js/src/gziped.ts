// Static variables
const FTEXT    =       1;
const FHCRC    = (1 << 1);
const FEXTRA   = (1 << 2);
const FNAME    = (1 << 3);
const FCOMMENT = (1 << 4);

// 16 bits per code max
const MAX_BITS = 16;

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

const staticHuffmanCodeLengths = [
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
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

/**
 * Reads n bits starting at bufpos. Generates results as read from left
 * to right.
 * 76543210 FEDCBA98
 * ——▶——▶—— —————▶——
 *  3 2  1    5   3
 * result:
 *  210 543 9876 FEDCB
 */
export function read(buf: Uint8Array, bufpos: Position, n: number): number {
  let value = 0;
  let pos = 1;
  do {
    value |= buf[bufpos.index] & bufpos.mask ? pos : 0;
    pos <<= 1;
    // Move the mask bit to the left up to 128, then reinit to 1 and increment ptr
    bufpos.mask = (bufpos.mask = bufpos.mask << 1) <= 128 ? bufpos.mask : ++bufpos.index && 1;
  } while (--n);

  return value;
}

/**
 * Reads n bits start at offset and mask. Generates results as read from right
 * to left.
 * 76543210 FEDCBA98
 * ◀——————— ◀———————
 *     1        2
 * result:
 *  01234567 89ABCDEF
 */
export function readInv(buf: Uint8Array, bufpos: Position, n: number): number {
  let value = 0;
  do {
    value <<= 1;
    value |= buf[bufpos.index] & bufpos.mask ? 1 : 0;
    // Move the mask bit to the left up to 128, then reinit to 1 and increment ptr
    bufpos.mask = (bufpos.mask = bufpos.mask << 1) <= 128 ? bufpos.mask : ++bufpos.index && 1;
  } while (--n);

  return value;
}

/**
 * Extracts a value of length bytes at offset in buf.
 */
export function getInteger(buf: Uint8Array, offset: number, length: number): number {
  let value = 0;
  let pos = 0;
  do {
    value |= buf[offset + pos] << (pos * 8);
  } while (++pos < length);
  return value;
}

/**
 * Generates the first codes for each code lengths.
 */
export function generateNextCodes(codeLength: number[]): number[] {
  const codeLengthCount = Array.apply({}, new Array(MAX_BITS)).map(_ => 0);
  codeLength.forEach(i => codeLengthCount[i]++);
  codeLengthCount[0] = 0;

  let code = 0;
  const nextCodes = Array.apply({}, new Array(MAX_BITS)).map(_ => 0);
  for (let i = 1; i < MAX_BITS; i++) {
    nextCodes[i] = code = (code + codeLengthCount[i - 1]) << 1;
  }
  return nextCodes;
}

/**
 * Generates a Huffman dictionary based on code lengths.
 * TODO: to improve performance replace the object dictionary by a faster
 *       data structure.
 */
export function generateDictionary(codeLengths: number[]): object {
  const dictionary = {};
  const nextCodes = generateNextCodes(codeLengths);
  for (let i = 0; i < codeLengths.length; ++i) {
    const length = codeLengths[i];
    let mask = 1 << (length - 1);
    let index = 0;
    while (mask > 0) {
      index <<= 1;
      index += (nextCodes[length] & mask) ? 2 : 1;
      mask >>= 1;
    }
    dictionary[index] = i;
    nextCodes[length]++;
  }
  return dictionary;
}

function inflate_block(buf: Uint8Array, bufpos: Position,
                       litdict: any, distdict: any,
                       output: Uint8Array, outputpos: Position): void {
}

export function inflate(buf: Uint8Array, output: Uint8Array): void {
  const staticDict = generateDictionary(staticHuffmanCodeLengths);
  let bfinal;
  do {
  } while (!bfinal);
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
    filename = new TextDecoder('utf-8').decode(buf.slice(offset, endofstr - 1));
    offset = endofstr;
  }
  // retrieve 0 terminated comment
  let comment = '';
  if (flag & FCOMMENT) {
    while (buf[endofstr++] !== 0) {}
    comment = new TextDecoder('utf-8').decode(buf.slice(offset, endofstr - 1));
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
