// Static variables
const FTEXT    =       1;
const FHCRC    = (1 << 1);
const FEXTRA   = (1 << 2);
const FNAME    = (1 << 3);
const FCOMMENT = (1 << 4);

// 16 bits per code max
const MAX_BITS = 16;

const LITERAL_BLOCK = 0;
const FIX_HUFF_BLOCK = 1;
const DYN_HUFF_BLOCK = 2;

const END_OF_BLOCK = 256;

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

const staticHuffmanLengthCodeLengths = [
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
const staticHuffmanDistanceCodeLengths = [
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5,
 ];
const length_lookup = [
  3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67,
  83, 99, 115, 131, 163, 195, 227, 258,
];
const length_extra_bits = [
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5,
  5, 5, 0,
];
const distance_lookup = [
  1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
  1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577,
];
const distance_extra_bits = [
  0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11,
  11, 12, 12, 13, 13,
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
  while (n--) {
    value |= buf[bufpos.index] & bufpos.mask ? pos : 0;
    pos <<= 1;
    // Move the mask bit to the left up to 128, then reinit to 1 and increment ptr
    bufpos.mask = (bufpos.mask = bufpos.mask << 1) <= 128 ? bufpos.mask : ++bufpos.index && 1;
  }

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
  while (n--) {
    value <<= 1;
    value |= buf[bufpos.index] & bufpos.mask ? 1 : 0;
    // Move the mask bit to the left up to 128, then reinit to 1 and increment ptr
    bufpos.mask = (bufpos.mask = bufpos.mask << 1) <= 128 ? bufpos.mask : ++bufpos.index && 1;
  }

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

function parseDynamicTree(buf: Uint8Array, bufpos: Position): { litdict: object; distdict: object} {
  const litdict = {};
  const distdict = {};
  return { litdict, distdict };
}

function inflate_block(buf: Uint8Array, bufpos: Position,
                       litdict: any, distdict: any,
                       output: Uint8Array, outputpos: Position): void {
  let value = 0;
  while (value !== END_OF_BLOCK) {
    let index = 0;
    do {
      index <<= 1;
      index += (buf[bufpos.index] & bufpos.mask) ? 2 : 1;
      bufpos.mask = (bufpos.mask = bufpos.mask << 1) <= 128 ? bufpos.mask : ++bufpos.index && 1;
      if (bufpos.index > buf.length) throw new Error('buffer overrun');
    } while ((value = litdict[index]) === undefined);
    if (value < END_OF_BLOCK) {
      output[outputpos.index++] = value;
    }
    if (value > END_OF_BLOCK) {
      // We have a length
      let length = length_lookup[value - END_OF_BLOCK - 1];
      length += read(buf, bufpos, length_extra_bits[length]);
      // Read the distance
      index = 0;
      value = 0;
      do {
        index <<= 1;
        index += (buf[bufpos.index] & bufpos.mask) ? 2 : 1;
        bufpos.mask = (bufpos.mask = bufpos.mask << 1) <= 128 ? bufpos.mask : ++bufpos.index && 1;
        if (bufpos.index > buf.length) throw new Error('buffer overrun');
      } while ((value = distdict[index]) === undefined);
      let distance = distance_lookup[value];
      distance += read(buf, bufpos, distance_extra_bits[distance]);
      // Copy bytes from the past. TODO: find a way to do that faster!
      while (length--) {
        output[outputpos.index] = output[outputpos.index - distance];
        outputpos.index++;
      }
    }
  }
}

export function inflate(buf: Uint8Array, output: Uint8Array): void {
  const staticLitdict = generateDictionary(staticHuffmanLengthCodeLengths);
  const staticDistdict = generateDictionary(staticHuffmanDistanceCodeLengths);
  let bfinal;
  let bufpos = { index: 0, mask: 1 };
  let outputpos = { index: 0, mask: undefined };
  do {
    bfinal = read(buf, bufpos, 1);
    console.log('bufpos', bufpos);
    const btype = read(buf, bufpos, 2);
    console.log('btype', btype);
    switch (btype) {
      case LITERAL_BLOCK:
        console.log('literal block');
        if (bufpos.mask === 1) bufpos.index++;
        const length = getInteger(buf, bufpos.index, 2);
        bufpos.index += 4;
        output.set(buf.slice(bufpos.index, bufpos.index + length), outputpos.index);
        outputpos.index += length;
        console.log(`copied ${length} from ${bufpos.index}`);
        break;
      case FIX_HUFF_BLOCK:
        console.log('fix huffman block');
        inflate_block(buf, bufpos, staticLitdict, staticDistdict, output, outputpos);
        break;
      case DYN_HUFF_BLOCK:
        console.log('dynamic huffman block');
        break;
      default:
        throw new Error('unknown block type');
    }
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
