FILES=$(ls testInput/)
for f in $FILES
do
	echo "$f"
	Release/bC -c "testInput/$f" out.lz77
	Release/bC -d out.lz77 "testOutput/$f"
	diff -s "testInput/$f" "testOutput/$f"
done
