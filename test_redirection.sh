echo "Hello Input Redirection test :) " > input.txt
cat < input.txt > output.txt
cat output.txt
rm input.txt output.txt
