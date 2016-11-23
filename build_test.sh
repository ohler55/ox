#!/bin/sh

# 1.8.7-p374\

for ruby in \
 rbx-2.2.6\
 1.9.3-p547\
 2.1.5\
 2.2.2\
 2.3.3\
 2.4.0-preview3
do
    echo "\n********************************************************************************"
    echo "Building $ruby\n"
    cd ext/ox
    rbenv local $ruby
    ruby extconf.rb
    make clean
    find . -name "*.rbc" -exec rm {} \;
    make

    echo "\nRunning tests for $ruby"
    cd ../../test
    rbenv local $ruby
    ./tests.rb
    cd sax
    rbenv local $ruby
    ./sax_test.rb
    cd ../..

    echo "\n"
done

PATH=/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr/bin:$PATH
echo "\n********************************************************************************"
echo "Building OS X Ruby\n"
cd ext/ox
ruby extconf.rb
make clean
make

echo "\nRunning tests for OS X Ruby"
cd ../../test
./tests.rb
./sax/sax_test.rb
cd ..

echo "resetting to 2.3.0\n"

cd ext/ox
rbenv local 2.3.3
cd ../../test
rbenv local 2.3.3
cd sax
rbenv local 2.3.3
cd ../..
echo "\n"
