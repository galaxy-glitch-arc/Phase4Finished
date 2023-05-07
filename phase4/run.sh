/bin/rm out.txt
touch out.txt

make clean

for i in $(seq -f "%02g" 0 12)
do
  make test$i
  echo starting test $i ....  >> out.txt
  echo >> out.txt
  echo  running test $i
  ./test$i >> out.txt 2>&1 3>&-
  echo >> out.txt
  rm test$i.o test$i
done