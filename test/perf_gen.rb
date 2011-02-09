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
require 'sample'
require 'test/ox/doc'
require 'files'
require 'nokogiri'

$verbose = 0
$ox_only = false

do_sample = false
do_files = false

do_load = false
do_dump = false
do_read = false
do_write = false
$iter = 1000

opts = OptionParser.new
opts.on("-v", "increase verbosity")                         { $verbose += 1 }

opts.on("-x", "ox only")                                    { $ox_only = true }

opts.on("-s", "load and dump as sample Ruby object")        { do_sample = true }
opts.on("-f", "load and dump as files Ruby object")         { do_files = true }

opts.on("-l", "load")                                       { do_load = true }
opts.on("-d", "dump")                                       { do_dump = true }
opts.on("-r", "read")                                       { do_read = true }
opts.on("-w", "write")                                      { do_write = true }
opts.on("-a", "load, dump, read and write")                 { do_load = true; do_dump = true; do_read = true; do_write = true }

opts.on("-i", "--iterations [Int]", Integer, "iterations")  { |i| $iter = i }

opts.on("-h", "--help", "Show this display")                { puts opts; Process.exit!(0) }
files = opts.parse(ARGV)

if files.empty?
  data = []
  obj = do_sample ? sample_doc(2) : files('..')
  xml = Ox.dump(obj, :indent => 2, :opt_format => true)
  gen = Ox.parse(xml)
  no = Nokogiri::XML::Document.parse(xml)
  File.open('sample.xml', 'w') { |f| f.write(xml) }
  data << { :file => 'sample.xml', :xml => xml, :ox => gen, :nokogiri => no }
else
  puts "loading and parsing #{files}\n\n"
  data = files.map do |f|
    xml = File.read(f)
    obj = Ox.parse(xml);
    gen = Ox.parse(xml)
    no = Nokogiri::XML::Document.parse(xml)
    { :file => f, :xml => xml, :ox => gen, :nokogiri => no }
  end
end

$ox_load_time = 0
$no_load_time = 0
$ox_dump_time = 0
$no_dump_time = 0

def perf_load(d)
  xml = d[:xml]

  if 0 < $verbose
    obj = Ox.load(xml, :mode => :generic, :trace => $verbose)
    return
  end
  start = Time.now
  (1..$iter).each do
    #obj = Ox.load(xml, :mode => :generic)
    obj = Ox.parse(xml)
  end
  $ox_load_time = Time.now - start
  puts "Parsing #{$iter} times with Ox took #{$ox_load_time} seconds."

  return if $ox_only
  
  start = Time.now
  (1..$iter).each do
    obj = Nokogiri::XML::Document.parse(xml)
    #obj = Nokogiri::XML::Document.parse(xml, nil, nil, Nokogiri::XML::ParseOptions::DEFAULT_XML | Nokogiri::XML::ParseOptions::NOBLANKS)
  end
  $no_load_time = Time.now - start
  puts "Nokogiri parse #{$iter} times took #{$no_load_time} seconds."
  puts ">>> Ox is %0.1f faster than Nokogiri parsing.\n\n" % [$no_load_time/$ox_load_time]
end

def perf_dump(d)
  obj = d[:ox]

  start = Time.now
  (1..$iter).each do
    xml = Ox.dump(obj, :indent => 2)
    # puts "*** ox:\n#{xml}"
  end
  $ox_dump_time = Time.now - start
  puts "Ox dumping #{$iter} times with ox took #{$ox_dump_time} seconds."

  return if $ox_only

  obj = d[:nokogiri]
  start = Time.now
  (1..$iter).each do
    xml = obj.to_xml(:indent => 0)
  end
  $no_dump_time = Time.now - start
  puts "Nokogiri to_xml #{$iter} times took #{$no_dump_time} seconds."
  puts ">>> Ox is %0.1f faster than Nokkgiri to_xml.\n\n" % [$no_dump_time/$ox_dump_time]
end

def perf_read(d)
  ox_read_time = 0

  filename = d[:file]

  # now load from the file
  start = Time.now
  (1..$iter).each do
    obj = Ox.load_file(filename, :mode => :generic)
  end
  ox_read_time = Time.now - start
  puts "Loading and parsing #{$iter} times with ox took #{ox_read_time} seconds."

  if !$ox_only
    start = Time.now
    (1..$iter).each do
      xml = File.read(filename)
      obj = Nokogiri::XML::Document.parse(xml)
    end
    no_read_time = Time.now - start
    puts "Reading and parsing #{$iter} times took #{no_read_time} seconds."
    puts ">>> Ox is %0.1f faster than Nokogiri loading and parsing.\n\n" % [no_read_time/ox_read_time]

  end
end

def perf_write(d)
  ox_write_time = 0
  
  filename = 'out.xml'
  no = 'out.xml'
  obj = d[:ox]

  start = Time.now
  (1..$iter).each do
    xml = Ox.to_file(filename, obj, :indent => 0)
  end
  ox_write_time = Time.now - start
  puts "Ox dumping #{$iter} times with ox took #{ox_write_time} seconds."

  if !$ox_only
    obj = d[:nokogiri]
    start = Time.now
    (1..$iter).each do
      xml = obj.to_xml(:indent => 0)
      File.open(filename, "w") { |f| f.write(xml) }
    end
    no_write_time = Time.now - start
    puts "Nokogiri dumping and writing #{$iter} times took #{no_write_time} seconds."
    puts ">>> Ox is %0.1f faster than Nokogiri writing.\n\n" % [no_write_time/ox_write_time]

  end
end

data.each do |d|
  puts "Using file #{d[:file]}."
  
  perf_load(d) if do_load
  perf_dump(d) if do_dump
  if do_load and do_dump
    puts ">>> Ox is %0.1f faster than Nokogiri dumping and loading.\n\n" % [($no_load_time + $no_dump_time)/($ox_load_time + $ox_dump_time)] unless 0 == $no_load_time
  end

  perf_read(d) if do_read
  perf_write(d) if do_write

end
