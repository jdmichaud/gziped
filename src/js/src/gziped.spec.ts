import { generateDictionary, generateNextCodes, getInteger } from './gziped';

describe('gziped', () => {
  describe('getInteger', () => {
    it('shall retrieve an integer of size length from the buffer', () => {
      const buf = new Uint8Array([0x12, 0x34, 0x56, 0x78]);
      expect(getInteger(buf, 1, 2)).toEqual(0x5634);
    });
  });

  describe('getNextCodes', () => {
    it('shall generate the next codes base on the code lenghts list', () => {
      const expectedOutcome = Array.apply({}, new Array(16)).map(_ => 0);
      expectedOutcome[3] = 2;
      expectedOutcome[4] = 14;
      for (let i = 5; i < 16; i++) {
        expectedOutcome[i] = 2 ** i;
      }
      expect(generateNextCodes([3, 3, 3, 3, 3, 2, 4, 4])).toEqual(expectedOutcome);
    });
  });

  describe('generateDict', () => {
    it('shall generate the dictionary based on the code lengths list', () => {
      const dictionary = generateDictionary([3, 3, 3, 3, 3, 2, 4, 4]);
      expect(dictionary[3]).toEqual(5); // F: "00"
      expect(dictionary[9]).toEqual(0); // A: "010"
      expect(dictionary[10]).toEqual(1); // B: "011"
      expect(dictionary[11]).toEqual(2); // C: "100"
      expect(dictionary[12]).toEqual(3); // D: "101"
      expect(dictionary[13]).toEqual(4); // E: "110"
      expect(dictionary[29]).toEqual(6); // G: "1110"
      expect(dictionary[30]).toEqual(7); // H: "1111"
    });
  });
});
