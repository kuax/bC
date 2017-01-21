FILES=$(ls testInput/)

if [ ! -d "testOutput" ]; then
	mkdir testOutput
fi

for f in $FILES
do
	echo "$f"
	src/byteComp -c "testInput/$f" out.hulz
	src/byteComp -d out.hulz "testOutput/$f"
	diff -s "testInput/$f" "testOutput/$f"
done
