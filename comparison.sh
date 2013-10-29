#!/bin/bash

for i in {0..799};
do
    j=$(($i+1))
    echo ./compare_pgm $1`printf %07d ${i}`.pgm $2`printf %07d ${j}`.pgm
    ./compare_pgm $1`printf %07d ${i}`.pgm $2`printf %07d ${j}`.pgm
done
