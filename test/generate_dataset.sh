#!/bin/bash

if [[ $# -ne 1 ]]
then
  echo "usage: generate_dataset.sh <folder>"
  exit 1
fi

input_folder=$1

rm -fr $input_folder/index.txt
for file in $(ls $input_folder)
do
  echo $file
  md5=$(md5sum $input_folder/$file | cut -c1-32)
  gzip -c $input_folder/$file > $input_folder/$file.gz
  cat >> $input_folder/index.txt << EOF
    $file.gz $md5
EOF
done
