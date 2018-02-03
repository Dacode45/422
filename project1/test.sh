#!/bin/bash
num_test=${1:-10}
num_records=${2:-10000}
for i in `seq 1 $num_test`;
do
echo "test case: $i of $num_test"
./generate -s $RANDOM -n $num_records -o /tmp/records.txt
./dump -i /tmp/records.txt | sort -k2,2 -k4,4 -k5,5 -k6,6 -k7,7 -k8,8 -k9,9 -k10,10 -k11,11 -k12,12 -n > /tmp/all_records.txt
threshold="$(shuf -n 1 /tmp/all_records.txt | awk '{print $2}')"
awk -v threshold="$threshold" '$2 >= threshold' /tmp/all_records.txt > /tmp/expected_records.txt
./sort_thresh -i /tmp/records.txt -o /tmp/tmp_records.txt -t $threshold -g
./dump -i /tmp/tmp_records.txt > /tmp/actual_records.txt
if cmp /tmp/expected_records.txt /tmp/actual_records.txt; then
  echo "test part a passed"
else
  echo "TEST PART A FAILED"
  exit 1
fi

awk -v threshold="$threshold" '$2 <= threshold' /tmp/all_records.txt > /tmp/expected_records.txt
./sort_thresh -i /tmp/records.txt -o /tmp/tmp_records.txt -t $threshold
./dump -i /tmp/tmp_records.txt > /tmp/actual_records.txt
if cmp /tmp/expected_records.txt /tmp/actual_records.txt; then
  echo "test part b passed"
else
  echo "TEST PART B FAILED"
  exit 1
fi

done
