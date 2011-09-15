#!/usr/bin/env ruby -wW1

$: << '.'
$: << '..'
$: << '../lib'
$: << '../ext'

if __FILE__ == $0
  while (i = ARGV.index('-I'))
    x,path = ARGV.slice!(i, 2)
    $: << path
  end
end

require 'optparse'
require 'ox'
require 'sample'
require 'files'
require 'nokogiri'

$verbose = 0
$ox_only = false
$filename = 'Sample.graffle'
$iter = 1000

opts = OptionParser.new
opts.on("-v", "increase verbosity")                         { $verbose += 1 }
opts.on("-x", "ox only")                                    { $ox_only = true }
opts.on("-f", "--file [String]", String, "filename")        { |f| $filename = f }
opts.on("-i", "--iterations [Int]", Integer, "iterations")  { |i| $iter = i }
opts.on("-h", "--help", "Show this display")                { puts opts; Process.exit!(0) }
rest = opts.parse(ARGV)

$xml_str = File.read($filename)

$ox_time = 0
$no_time = 0

class OxSax < ::Ox::Sax
  def start_element(name, attrs)
    #puts "#{name} #{attrs}" # temporary
  end

  def error(message, line, column)
    puts message
  end
end

class NoSax < Nokogiri::XML::SAX::Document
  def start_element(name, attrs = [])
  end
end

def perf_stringio()
  start = Time.now
  handler = OxSax.new()
  $iter.times do
    input = StringIO.new($xml_str)
    Ox.sax_parse(handler, input)
    input.close
  end
  $ox_time = Time.now - start
  puts "StringIO SAX parsing #{$iter} times with Ox took #{$ox_time} seconds."

  return if $ox_only

  handler = Nokogiri::XML::SAX::Parser.new(NoSax.new)
  $iter.times do
    input = StringIO.new($xml_str)
    handler.parse(input)
    input.close
  end
  $no_time = Time.now - start
  puts "StringIO SAX parsing #{$iter} times with Nokogiri took #{$no_time} seconds."

  puts ">>> Ox is %0.1f faster than Nokogiri SAX parsing using StringIO.\n\n" % [$no_time/$ox_time]
end

def perf_fileio()
  start = Time.now
  handler = OxSax.new()
  $iter.times do
    input = IO.open(IO.sysopen($filename))
    Ox.sax_parse(handler, input)
    input.close
  end
  $ox_time = Time.now - start
  puts "File IO SAX parsing #{$iter} times with Ox took #{$ox_time} seconds."

  return if $ox_only

  handler = Nokogiri::XML::SAX::Parser.new(NoSax.new)
  $iter.times do
    input = IO.open(IO.sysopen($filename))
    handler.parse(input)
    input.close
  end
  $no_time = Time.now - start
  puts "File IO SAX parsing #{$iter} times with Nokogiri took #{$no_time} seconds."

  puts ">>> Ox is %0.1f faster than Nokogiri SAX parsing using file IO.\n\n" % [$no_time/$ox_time]
end

perf_stringio()
perf_fileio()
