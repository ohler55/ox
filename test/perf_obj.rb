#!/usr/bin/env ruby

$: << '.'
$: << '../lib'
$: << '../ext'

if __FILE__ == $0 && (i = ARGV.index('-I'))
  x = ARGV.slice!(i, 2)
  $: << x[1]
end

require 'optparse'
require 'ox'
require 'perf'
require 'sample'
require 'files'
begin
  require 'oj'
rescue Exception
end

$circular = false
$indent = 0
ox_only = false

do_sample = false
do_files = false

do_load = false
do_dump = false
do_read = false
do_write = false
$iter = 1000

opts = OptionParser.new
opts.on('-c', 'circular options')                           { $circular = true }

opts.on('-s', 'load and dump as sample Ruby object')        { do_sample = true }
opts.on('-f', 'load and dump as files Ruby object')         { do_files = true }

opts.on('-l', 'load')                                       { do_load = true }
opts.on('-d', 'dump')                                       { do_dump = true }
opts.on('-r', 'read')                                       { do_read = true }
opts.on('-w', 'write')                                      { do_write = true }
opts.on('-a', 'load, dump, read and write')                 { do_load = true; do_dump = true; do_read = true; do_write = true }

opts.on('-i', '--iterations [Int]', Integer, 'iterations')  { |it| $iter = it }
opts.on('-o', 'ox_only')                                    { ox_only = true }

opts.on('-h', '--help', 'Show this display')                { puts opts; Process.exit!(0) }
files = opts.parse(ARGV)

$obj = nil
$xml = nil
$mars = nil
$json = nil

unless do_load || do_dump || do_read || do_write
  do_load = true
  do_dump = true
  do_read = true
  do_write = true
end

# prepare all the formats for input
if files.empty?
  $obj = do_sample ? sample_doc(2) : files('..')
  $mars = Marshal.dump($obj)
  $xml = Ox.dump($obj, :indent => $indent, :circular => $circular)
  File.open('sample.xml', 'w') { |f| f.write($xml) }
  File.open('sample.marshal', 'w') { |f| f.write($mars) }
  unless defined?(Oj).nil?
    $json = Oj.dump($obj, :indent => $indent, :circular => $circular)
    File.open('sample.json', 'w') { |f| f.write($json) }
  end
else
  puts "loading and parsing #{files}\n\n"
  files.map do |f|
    $xml = File.read(f)
    $obj = Ox.load($xml);
    $mars = Marshal.dump($obj)
    $json = Oj.dump($obj, :indent => $indent, :circular => $circular) unless defined?(Oj).nil?
  end
end

Oj.default_options = { :mode => :object, :indent => $indent } unless defined?(Oj).nil?

if do_load
  puts '-' * 80
  puts 'Load Performance'
  perf = Perf.new
  perf.add('Ox', 'load') { Ox.load($xml, :mode => :object) }
  perf.add('Oj', 'load') { Oj.load($json) } unless (defined?(Oj).nil? || ox_only)
  perf.add('Marshal', 'load') { Marshal.load($mars) } unless ox_only
  perf.run($iter)
end

if do_dump
  puts '-' * 80
  puts 'Dump Performance'
  perf = Perf.new
  perf.add('Ox', 'dump') { Ox.dump($obj, :indent => $indent, :circular => $circular) }
  perf.add('Oj', 'dump') { Oj.dump($obj) } unless (defined?(Oj).nil? || ox_only)
  perf.add('Marshal', 'dump') { Marshal.dump($obj) } unless ox_only
  perf.run($iter)
end

if do_read
  puts '-' * 80
  puts 'Read from file Performance'
  perf = Perf.new
  perf.add('Ox', 'load_file') { Ox.load_file('sample.xml', :mode => :object) }
  perf.add('Oj', 'load') { Oj.load_file('sample.json') } unless (defined?(Oj).nil? || ox_only)
  perf.add('Marshal', 'load') { Marshal.load(File.new('sample.marshal')) } unless ox_only
  perf.run($iter)
end

if do_write
  puts '-' * 80
  puts 'Write to file Performance'
  perf = Perf.new
  perf.add('Ox', 'to_file') { Ox.to_file('sample.xml', $obj, :indent => $indent, :circular => $circular) }
  perf.add('Oj', 'to_file') { Oj.to_file('sample.json', $obj) } unless (defined?(Oj).nil? || ox_only)
  perf.add('Marshal', 'dump') { Marshal.dump($obj, File.new('sample.marshal', 'w')) } unless ox_only
  perf.run($iter)
end
