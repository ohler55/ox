#!/bin/sh

export RBXOPT=-X19

for ruby in \
 1.8.7-p358\
 1.9.2-p290\
 rbx-1.2.4\
 rbx-2.0.0-dev\
 ree-1.8.7-2012.02\
 1.9.3-p385 \
 2.0.0-p195
do
    echo "\n********************************************************************************"
    echo "Building $ruby\n"
    cd ext/ox
    rbenv local $ruby
    ruby extconf.rb
    make clean
    make

    echo "\nRunning tests for $ruby"
    cd ../../test
    rbenv local $ruby
    ./tests.rb
    ./sax/sax_test.rb
    cd ..

    echo "\n"
done

echo "\n********************************************************************************"
echo "Building jruby-1.6.7.2 --1.8\n"
export JRUBY_OPTS="-Xcext.enabled=true --1.8"
cd ext/ox
rbenv local jruby-1.6.7.2
ruby extconf.rb
make clean
make

echo "\nRunning tests for jruby-1.6.7.2 --1.8"
cd ../../test
rbenv local jruby-1.6.7.2
./tests.rb
./sax/sax_test.rb
cd ..

echo "\n"

echo "\n********************************************************************************"
echo "Building jruby-1.6.7.2 --1.9\n"
export JRUBY_OPTS="-Xcext.enabled=true --1.9"
cd ext/ox
rbenv local jruby-1.6.7.2
ruby extconf.rb
make clean
make

echo "\nRunning tests for jruby-1.6.7.2 --1.9"
cd ../../test
rbenv local jruby-1.6.7.2
./tests.rb
./sax/sax_test.rb
cd ..

echo "\n"

PATH=/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr/bin:$PATH
echo "\n********************************************************************************"
echo "Building OS X Ruby\n"
cd ext/ox
ruby extconf.rb
make

echo "\nRunning tests for OS X Ruby"
cd ../../test
./tests.rb
./sax/sax_test.rb
cd ..

echo "resetting to 1.9.3-p374\n"

cd ext/ox
rbenv local 2.0.0-p195
cd ../../test
rbenv local 2.0.0-p195
cd ..
echo "\n"
