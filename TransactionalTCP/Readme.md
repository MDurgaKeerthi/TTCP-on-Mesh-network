# Readme

# Compile

Some of the codes write to database, so proper tables have to be created in mysql.
They will work even on commenting those parts of the code
* g++ -std=c++11 $(mysql_config --cflags) filename -lpthread -o client $(mysql_config --libs) 

# Testing

* ./client
* ./server
