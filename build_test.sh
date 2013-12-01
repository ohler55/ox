#!/bin/sh

for ruby in \
 1.8.7-p374\
 rubinius\
 ree-1.8.7-2012.02\
 1.9.3-p484\
 2.0.0-p353
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

echo "resetting to 2.0.0\n"

cd ext/ox
rbenv local 2.0.0-p247
cd ../../test
rbenv local 2.0.0-p247
cd sax
rbenv local 2.0.0-p247
cd ../..
echo "\n"
