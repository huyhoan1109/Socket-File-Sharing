cd client
make clean
make
clear
if ([ -z "$1"] & [ -z "$2"]);
then
    ./client 127.0.0.1 8888
else
    ./client $1 $2
fi