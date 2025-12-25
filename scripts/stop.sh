#!/bin/bash
for pid in $(pgrep -f "kworker/0:0"); do
    [ "$(ps -o ppid= -p $pid | tr -d ' ')" != "2" ] && kill $pid && echo "Terminado: $pid"
done
