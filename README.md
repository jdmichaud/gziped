``` bash
cc gziped.c -o gziped
./gziped toto.gz
valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all ./gziped toto.gz
```

