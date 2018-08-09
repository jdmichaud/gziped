#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

if [[ $# -ne 1 ]];
then
  echo "usage: ./test.sh ../src/c/gziped"
  exit 1
fi

if [[ ! -x $1 ]];
then
  echo "$1: file is not executable"
  exit 2
fi

TMPDIR=$(mktemp -d)
CURDIR=$(pwd)
cd $TMPDIR

for filename in $CURDIR/resources/*.gz; do
  $CURDIR/$1 $filename
done

failures=0
for filename in $(ls $CURDIR/resources/ --ignore='*.gz'); do
  echo -n "testing for $filename"
  if [[ ! -f $filename ]];
  then
    echo -e "${RED}\t\t\tKO - missing${NC}"
    failures=$((failures+1))
    continue
  fi
  res=$(cmp $filename $CURDIR/resources/$filename 2>&1)
  if [[ $? -ne 0 ]];
  then
    echo -e "${RED}\t\t\tKO - different${NC}"
    echo $res
    failures=$((failures+1))
    continue
  fi
  echo -e "${GREEN}\t\t\tOK${NC}"
done

cd $CURDIR
rm -fr $TMPDIR

exit $failures
