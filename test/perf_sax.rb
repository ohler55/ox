#!/usr/bin/env ruby

$: << '.'
$: << '..'
$: << '../lib'
$: << '../ext'

if __FILE__ == $0
  while (i = ARGV.index('-I'))
    x = ARGV.slice!(i, 2)
    $: << x[1]
  end
end

require 'optparse'
require 'ox'
require 'perf'
require 'files'
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
$all_cbs = false
$filename = nil # nil indicates new file names perf.xml will be created and used
$filesize = 1000 # KBytes
$iter = 1000
$strio = false
$pos = false
$smart = false

opts = OptionParser.new
opts.on('-v', 'increase verbosity')                            { $verbose += 1 }
opts.on('-x', 'ox only')                                       { $ox_only = true }
opts.on('-a', 'all callbacks')                                 { $all_cbs = true }
opts.on('-b', 'html smart')                                    { $smart = true }
opts.on('-p', 'update position')                               { $pos = true; $all_cbs = true }
opts.on('-z', 'use StringIO instead of file')                  { $strio = true }
opts.on('-f', '--file [String]', String, 'filename')           { |f| $filename = f }
opts.on('-i', '--iterations [Int]', Integer, 'iterations')     { |it| $iter = it }
opts.on('-s', '--size [Int]', Integer, 'file size in KBytes')  { |s| $filesize = s }
opts.on('-h', '--help', 'Show this display')                   { puts opts; Process.exit!(0) }
opts.parse(ARGV)

$xml_str = nil

# size is in Kbytes
def create_file(filename, size)
  head = %{<?xml version="1.0"?>
<?ox version="1.0" mode="object" circular="no" xsd_date="no"?>
<!DOCTYPE table PUBLIC "-//ox//DTD TABLE 1.0//EN" "http://www.ohler.com/DTDs/TestTable-1.0.dtd">
<table>
}
  tail = %{</table>
}
  row = %{  <!-- row %08d element -->
  <row id="%08d">
    <cell id="A" type="Fixnum">1234</cell>
    <cell id="B" type="String">A string.</cell>
    <cell id="C" type="String">This is a longer string that stretches over a larger number of characters.</cell>
    <cell id="D" type="Float">-12.345</cell>
    <cell id="E" type="Date">2011-09-18 23:07:26 +0900</cell>
    <cell id="F" type="Image"><![CDATA[xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00xx00]]></cell>
  </row>
}
  cnt = (size * 1000 - head.size - tail.size) / row.size
  File.open(filename, 'w') do |f|
    f.write(head)
    cnt.times do |i|
      f.write(format(row, i, i))
    end
    f.write(tail)
  end
end

class OxSax < Ox::Sax
  def error(message, line, column);
    puts message;
  end
end

class OxAllSax < OxSax
  def start_element(name); end
  def attr(name, str); end
  def attr_value(name, value); end
  def end_element(name); end
  def text(str); end
  def value(value); end
  def instruct(target); end
  def doctype(value); end
  def comment(value); end
  def cdata(value); end
end

class OxPosAllSax < OxAllSax
  def initialize()
    @line = nil
    @column = nil
  end
end

unless defined?(Nokogiri).nil?
  class NoSax < Nokogiri::XML::SAX::Document
    def error(message);
      puts message;
    end

    def warning(message);
      puts message;
    end
  end

  class NoAllSax < NoSax
    def start_element(name, attrs = []); end
    def characters(text); end
    def cdata_block(string); end
    def comment(string); end
    def end_document(); end
    def end_element(name); end
    def start_document(); end
    def xmldecl(version, encoding, standalone); end
  end
end

unless defined?(LibXML).nil?
  class LxSax
    include LibXML::XML::SaxParser::Callbacks
  end

  class LxAllSax < LxSax
    def on_start_element(element, attributes); end
    def on_cdata_block(cdata); end
    def on_characters(chars); end
    def on_comment(msg); end
    def on_end_document(); end
    def on_end_element(element); end
    def on_end_element_ns(name, prefix, uri); end
    def on_error(msg); end
    def on_external_subset(name, external_id, system_id); end
    def on_has_external_subset(); end
    def on_has_internal_subset(); end
    def on_internal_subset(name, external_id, system_id); end
    def on_is_standalone(); end
    def on_processing_instruction(target, data); end
    def on_reference(name); end
    def on_start_document(); end
    def on_start_element_ns(name, attributes, prefix, uri, namespaces); end
  end
end

if $filename.nil?
  create_file('perf.xml', $filesize)
  $filename = 'perf.xml'
end
$xml_str = File.read($filename)

puts "A #{$filesize} KByte XML file was parsed #{$iter} times for this test."

$handler = nil
perf = Perf.new

perf.add('Ox::Sax', 'sax_parse') {
  input = $strio ? StringIO.new($xml_str) : IO.open(IO.sysopen($filename))
  Ox.sax_parse($handler, input, :smart => $smart)
  input.close
}
perf.before('Ox::Sax') {
  $handler = if $all_cbs
               $pos ? OxPosAllSax.new : OxAllSax.new
             else
               OxSax.new
             end
}

unless $ox_only
  unless defined?(Nokogiri).nil?
    perf.add('Nokogiri::XML::Sax', 'parse') {
      input = $strio ? StringIO.new($xml_str) : IO.open(IO.sysopen($filename))
      $handler.parse(input)
      input.close
    }
    perf.before('Nokogiri::XML::Sax') { $handler = Nokogiri::XML::SAX::Parser.new($all_cbs ? NoAllSax.new : NoSax.new) }
  end

  unless defined?(LibXML).nil?
    perf.add('LibXML::XML::Sax', 'parse') {
      input = $strio ? StringIO.new($xml_str) : IO.open(IO.sysopen($filename))
      parser = LibXML::XML::SaxParser.io(input)
      parser.callbacks = $handler
      parser.parse
      input.close
    }
    perf.before('LibXML::XML::Sax') { $handler = $all_cbs ? LxAllSax.new : LxSax.new }
  end
end

perf.run($iter)
