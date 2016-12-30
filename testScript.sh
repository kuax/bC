FILES=$(ls testInput/)
for f in $FILES
do
	Debug/byteComp -c "testInput/$f" out.lz77
done
