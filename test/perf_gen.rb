#!/usr/bin/env ruby

$: << '.'
$: << '..'
$: << '../lib'
$: << '../ext'

if __FILE__ == $0 && (i = ARGV.index('-I'))
  x = ARGV.slice!(i, 2)
  $: << x[1]
end

require 'optparse'
require 'ox'
require 'sample'
require 'test/ox/doc'
require 'files'
require 'perf'
begin
  require 'nokogiri'
rescue Exception => e
end
begin
  require 'libxml'
rescue Exception => e
end

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
opts.on('-v', 'increase verbosity')                         { $verbose += 1 }

opts.on('-x', 'ox only')                                    { $ox_only = true }

opts.on('-s', 'load and dump as sample Ruby object')        { do_sample = true }
opts.on('-f', 'load and dump as files Ruby object')         { do_files = true }

opts.on('-l', 'load')                                       { do_load = true }
opts.on('-d', 'dump')                                       { do_dump = true }
opts.on('-r', 'read')                                       { do_read = true }
opts.on('-w', 'write')                                      { do_write = true }
opts.on('-a', 'load, dump, read and write')                 { do_load = true; do_dump = true; do_read = true; do_write = true }

opts.on('-i', '--iterations [Int]', Integer, 'iterations')  { |it| $iter = it }

opts.on('-h', '--help', 'Show this display')                { puts opts; Process.exit!(0) }
files = opts.parse(ARGV)

Ox.default_options = {mode: :generic}

data = []

if files.empty?
  data = []
  obj = do_sample ? sample_doc(2) : files('..')
  xml = Ox.dump(obj, :indent => 2, :opt_format => true)
  File.write('sample.xml', xml)
  gen = Ox.parse(xml)
  h = { :file => 'sample.xml', :xml => xml, :ox => gen }
  h[:nokogiri] = Nokogiri::XML::Document.parse(xml) unless defined?(Nokogiri).nil?
  h[:libxml] = LibXML::XML::Document.string(xml) unless defined?(LibXML).nil?
  data << h
else
  puts "loading and parsing #{files}\n\n"
  data = files.map do |f|
    xml = File.read(f)
    obj = Ox.parse(xml)
    gen = Ox.parse(xml)
    h = { :file => f, :xml => xml, :ox => gen }
    h[:nokogiri] = Nokogiri::XML::Document.parse(xml) unless defined?(Nokogiri).nil?
    h[:libxml] = LibXML::XML::Document.string(xml) unless defined?(LibXML).nil?
    h
  end
end

data.each do |d|
  if do_load
    perf = Perf.new
    perf.add('Ox', 'parse') { Ox.parse(xml) }
    perf.add('Nokogiri', 'parse') { Nokogiri::XML::Document.parse(xml) } unless defined?(Nokogiri).nil?
    perf.add('LibXML', 'parse') { LibXML::XML::Document.string(xml) } unless defined?(LibXML).nil?
    perf.run($iter)
  end

  if do_dump
    perf = Perf.new
    perf.add('Ox', 'dump') { Ox.dump($obj, :indent => 2) }
    perf.before('Ox') { $obj = d[:ox] }
    unless defined?(Nokogiri).nil?
      perf.add('Nokogiri', 'dump') { $obj.to_xml(:indent => 2) }
      perf.before('Nokogiri') { $obj = d[:nokogiri] }
    end
    unless defined?(LibXML).nil?
      perf.add('LibXML', 'dump') { $obj.to_s }
      perf.before('LibXML') { $obj = d[:libxml] }
    end
    perf.run($iter)
  end

  if do_read
    $filename = d[:file]
    perf = Perf.new
    perf.add('Ox', 'load_file') { Ox.load_file($filename) }
    perf.add('Nokogiri', 'parse') { Nokogiri::XML::Document.parse(File.open($filename)) } unless defined?(Nokogiri).nil?
    perf.add('LibXML', 'parse') { LibXML::XML::Document.file($filename) } unless defined?(LibXML).nil?
    perf.run($iter)
  end

  next unless do_write

  $filename = 'out.xml'
  perf = Perf.new
  perf.add('Ox', 'to_file') { Ox.to_file($filename, $obj, :indent => 0) }
  perf.before('Ox') { $obj = d[:ox] }
  unless defined?(Nokogiri).nil?
    perf.add('Nokogiri', 'dump') do
      xml = $obj.to_xml(:indent => 0)
      File.write($filename, xml)
    end
  end
  perf.before('Nokogiri') { $obj = d[:nokogiri] }
  unless defined?(LibXML).nil?
    perf.add('LibXML', 'dump') do
      xml = $obj.to_s
      File.write($filename, xml)
    end
    perf.before('LibXML') { $obj = d[:libxml] }
  end
  perf.run($iter)
end
