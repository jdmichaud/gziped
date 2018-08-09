# From a huffman code (as a string), generates the index in the array
# representing the huffman tree.
def c(a):
  res = 0
  for y in a:
    res <<= 1
    res += 1 if int(y) == 0 else 2
  return str(res)

print(c('10101'))
