import { getInteger } from './gziped';

describe('gziped', () => {
  describe('getInteger', () => {
    it('shall retrieve an integer of size length from the buffer', () => {
      const buf = new Uint8Array([0x12, 0x34, 0x56, 0x78]);
      const bufpos = { index: 1, mask: 0 };
      expect(getInteger(buf, bufpos, 2)).to.equals(0x5634);
      expect(bufpos.index).to.equals(3);
    });
  });
});
