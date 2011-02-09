#!/usr/bin/env ruby -wW1

$: << '.'
$: << '..'
$: << '../lib'
$: << '../ext'

if __FILE__ == $0
  if (i = ARGV.index('-I'))
    x,path = ARGV.slice!(i, 2)
    $: << path
  end
end

require 'optparse'
require 'ox'
require 'pod/cfg'
require 'pod/xml'

$verbose = 0
$use_opt_format = false

do_load = false
do_dump = false

opts = OptionParser.new
opts.on("-v", "increase verbosity")                         { $verbose += 1 }
opts.on("-o", "optimized format")                           { $use_opt_format = true }
opts.on("-h", "--help", "Show this display")                { puts opts; Process.exit!(0) }
files = opts.parse(ARGV)


$ox_load_time = 0
$opt_load_time = 0
$no_load_time = 0
$ox_dump_time = 0
$no_dump_time = 0

files = Dir.glob('data/**/*.xml')
xml_data = files.map { |f| File.read(f) }

objs = []
start = Time.now
xml_data.each do |xml|
  objs << Ox.load(xml, :mode => :object, :trace => 0)
end
$ox_load_time = Time.now - start
puts "Loading #{files.size} files with Ox took #{$ox_load_time} seconds."

start = Time.now
objs.each do |o|
  xml = Ox.dump(o, :opt_format => $use_opt_format, :indent => 0)
end
$ox_dump_time = Time.now - start
puts "Dumping #{files.size} files with Ox took #{$ox_dump_time} seconds."


opt_data = objs.map { |o| Ox.dump(o, :opt_format => $use_opt_format, :indent => 0) }
mode = $use_opt_format ? :optimized : :object
objs = []
start = Time.now
opt_data.each do |xml|
  objs << Ox.load(xml, :mode => mode, :trace => 0)
end
$opt_load_time = Time.now - start
puts "Loading #{files.size} files with Optimized Ox took #{$opt_load_time} seconds.\n\n"


objs = []
start = Time.now
xml_data.each do |xml|
  objs << ::Pod::Xml.to_obj(xml)
end
$no_load_time = Time.now - start
puts "Loading Pod::Xml #{files.size} files took #{$no_load_time} seconds."

start = Time.now
objs.each do |o|
  xml = ::Pod::Xml.to_xml(o)
end
$no_dump_time = Time.now - start
puts "Dumping #{files.size} files with Pod::Xml took #{$no_dump_time} seconds.\n\n"

puts ">>> Ox is %0.1f faster than Pod::Xml loading." % [$no_load_time/$ox_load_time]
puts ">>> Optimized Ox is %0.1f faster than Pod::Xml loading." % [$no_load_time/$opt_load_time]
puts ">>> Ox is %0.1f faster than Pod::Xml dumping.\n\n" % [$no_dump_time/$ox_dump_time]

puts ">>> Ox is %0.1f faster than Pod::Xml dumping and loading." % [($no_dump_time + $no_load_time)/($ox_dump_time + $opt_load_time)]
