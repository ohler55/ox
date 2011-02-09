#!/usr/bin/env ruby -wW2

if $0 == __FILE__
  $: << '.'
  $: << '..'
  $: << '../lib'
  $: << '../ext'
end

require 'pp'
require 'ox'
require 'test/ox/file'
require 'test/ox/dir'

def files(dir)
  d = ::Test::Ox::Dir.new(dir)
  Dir.new(dir).each do |fn|
    next if fn.start_with?('.')
    filename = File.join(dir, fn)
    #filename = '.' == dir ? fn : File.join(dir, fn)
    if File.directory?(filename)
      d << files(filename)
    else
      d << Test::Ox::File.new(filename)
    end
  end
  #pp d
  d
end

if $0 == __FILE__
  File.open('files.xml', "w") { |f| f.write(Ox.dump(files('.'), :indent => 2, :opt_format => true)) }
  #Ox.to_file(files('.'), 'files2.xml', :indent => 2, :opt_format => true)
end
