FILES=$(ls testInput/)
for f in $FILES
do
	Release/bC -c "testInput/$f" out.lz77
done
