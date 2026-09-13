#!/bin/bash

if [ $# -eq 0 ]; then
    echo "Использование: $0 число1 число2 ..."
    exit 1
fi

echo "$@" | awk '{
    sum = 0
    for (i = 1; i <= NF; i++) {
        sum += $i
    }
    print "Количество аргументов:", NF
    print "Среднее арифметическое:", sum / NF
}'